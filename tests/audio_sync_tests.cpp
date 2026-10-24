#include "rhythm/audio_sync.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace rhythm;
namespace {
void check(bool value) { if (!value) throw std::runtime_error("check failed"); }
void rejects(const std::function<void()>& run) {
    try { run(); } catch (const std::exception&) { return; }
    throw std::runtime_error("expected exception");
}
AudioFeatureReference reference(unsigned bands = 1, unsigned rate = 100, std::uint32_t seed = 17) {
    AudioFeatureReference ref;
    ref.bands = bands; ref.feature_rate_hz = rate;
    ref.audio_sha256 = std::string(64, 'a'); ref.feature_id = "synthetic deterministic random features";
    ref.values.resize(60 * rate * bands);
    for (auto& value : ref.values) {
        seed = seed * 1664525u + 1013904223u;
        value = static_cast<float>((static_cast<double>(seed >> 8) / 16777216.) * 2 - 1);
    }
    return ref;
}
AudioSyncConfig synthetic_config() {
    AudioSyncConfig config;
    config.window_frames = 100; // One-second deterministic feature windows; not the real 5s profile.
    config.global_peak_exclusion_frames = 25;
    return config;
}
AudioObservation observation(const AudioFeatureReference& ref, double song_end, std::uint64_t id,
                             double rate = 1, double host_offset = 4.22, std::size_t frames = 100) {
    AudioObservation result;
    result.observation_id = id; result.rms = .01;
    result.timestamp_uncertainty_ns = 0; // This fixture explicitly owns its exact synthetic sample clock.
    result.window_end_host_ns = static_cast<Ns>(std::llround((host_offset + rate * song_end) * 1e9));
    result.received_host_ns = result.window_end_host_ns + 10000000;
    result.values.resize(frames * ref.bands);
    for (std::size_t f = 0; f < frames; ++f) {
        const double index = song_end * ref.feature_rate_hz - static_cast<double>(frames - f) / rate;
        const auto low = static_cast<std::size_t>(std::max(0., std::floor(index)));
        const auto high = std::min(low + 1, ref.values.size() / ref.bands - 1);
        const double part = index - static_cast<double>(low);
        for (unsigned b = 0; b < ref.bands; ++b)
            result.values[f * ref.bands + b] = static_cast<float>(ref.values[low*ref.bands+b] * (1-part) + ref.values[high*ref.bands+b] * part);
    }
    return result;
}
AudioSyncEstimate acquire(AudioSync& sync, const AudioFeatureReference& ref, double rate = 1,
                          double offset = 4.22, std::uint64_t first_id = 1, double start = 5) {
    AudioSyncEstimate result;
    for (unsigned i = 0; i <= 10; ++i) {
        const auto obs = observation(ref, start + .1 * i, first_id + i, rate, offset);
        result = sync.observe(obs, obs.received_host_ns);
    }
    return result;
}
AudioFeatureReference repeated_reference() {
    auto ref = reference();
    for (std::size_t f = 1549; f < 3745; ++f) ref.values[f] = ref.values[1000 + (f-1000) % 549];
    return ref;
}
} // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"unknown timestamp uncertainty cannot silently become exact", [] {
            const auto ref = reference(); AudioSync sync(ref, synthetic_config());
            const auto locked = acquire(sync, ref); check(locked.lease.locked);
            auto obs = observation(ref, 6.1, 12);
            obs.timestamp_uncertainty_ns = AudioObservation{}.timestamp_uncertainty_ns;
            check(obs.timestamp_uncertainty_ns < 0);
            const auto result = sync.observe(obs, obs.received_host_ns);
            check(!result.lease.locked && result.reason == AudioSyncReason::invalid_observation);
            check(result.lease.observed_host_ns == locked.lease.observed_host_ns);
        }},
        {"default acquisition requires half-window of new data per anchor", [] {
            const auto ref = reference(); AudioSync sync(ref);
            for (unsigned i = 0; i < 3; ++i) {
                const auto obs = observation(ref, 5 + .5*i, i+1, 1, 4.22, 500);
                const auto result = sync.observe(obs, obs.received_host_ns);
                check(!result.lease.locked && result.anchor_count == 1);
            }
            const auto second = observation(ref, 7.5, 4, 1, 4.22, 500);
            check(sync.observe(second, second.received_host_ns).anchor_count == 2);
            const auto third = observation(ref, 10, 5, 1, 4.22, 500);
            check(sync.observe(third, third.received_host_ns).lease.locked);
        }},
        {"known lag requires three compatible unique observations", [] {
            const auto ref = reference(); AudioSync sync(ref, synthetic_config());
            const auto one = observation(ref, 5, 1);
            auto result = sync.observe(one, one.received_host_ns);
            check(!result.lease.locked && result.anchor_count == 1 && result.global_unique);
            const auto two = observation(ref, 5.5, 2);
            result = sync.observe(two, two.received_host_ns);
            check(!result.lease.locked && result.anchor_count == 2);
            const auto three = observation(ref, 6, 3);
            result = sync.observe(three, three.received_host_ns);
            check(result.state == AudioSyncState::locked && result.anchor_count == 3 && result.peak > .999);
            check(std::abs(result.clock.to_host(10000000000LL) - 14220000000LL) < 1000000);
            check(result.reference_audio_sha256 == std::string(64, 'a') && result.observation_id == 3);
            check(result.lease.observed_host_ns == three.window_end_host_ns && result.lease.observed_host_ns != three.received_host_ns);
        }},
        {"status cannot renew evidence and expires within configured age", [] {
            const auto ref = reference(); AudioSync sync(ref, synthetic_config()); const auto locked = acquire(sync, ref);
            check(locked.lease.locked);
            const auto before = sync.status(locked.lease.observed_host_ns + 150000000);
            check(before.lease.locked && before.lease.observed_host_ns == locked.lease.observed_host_ns && before.observation_id == locked.observation_id);
            const auto lost = sync.status(locked.lease.observed_host_ns + 200000001);
            check(!lost.lease.locked && lost.reason == AudioSyncReason::stale && lost.lease.epoch > locked.lease.epoch);
            check(sync.status(locked.lease.observed_host_ns + 300000000).lease.epoch == lost.lease.epoch);
        }},
        {"silence mute and flat features stop without false lock", [] {
            const auto ref = reference();
            for (unsigned mode = 0; mode < 3; ++mode) {
                AudioSync sync(ref, synthetic_config()); check(acquire(sync, ref).lease.locked);
                auto obs = observation(ref, 6.1, 12);
                if (mode == 0) obs.rms = 0;
                if (mode == 1) std::fill(obs.values.begin(), obs.values.end(), 0.f);
                if (mode == 2) std::fill(obs.values.begin(), obs.values.end(), -20.f);
                const auto result = sync.observe(obs, obs.received_host_ns);
                check(!result.lease.locked && result.reason == AudioSyncReason::low_energy);
            }
        }},
        {"wrong source does not acquire", [] {
            const auto ref = reference(), wrong = reference(1, 100, 371); AudioSync sync(ref, synthetic_config());
            for (unsigned i = 0; i < 15; ++i) {
                const auto obs = observation(wrong, 5 + .1*i, i+1);
                check(!sync.observe(obs, obs.received_host_ns).lease.locked);
            }
        }},
        {"repeated global peaks remain ambiguous even with excellent scores", [] {
            const auto ref = repeated_reference(); AudioSync sync(ref, synthetic_config());
            for (unsigned i = 0; i < 15; ++i) {
                const auto obs = observation(ref, 16 + .1*i, i+1);
                const auto result = sync.observe(obs, obs.received_host_ns);
                check(!result.lease.locked && result.reason == AudioSyncReason::ambiguous && result.peak > .999 && result.margin < .001);
            }
        }},
        {"local repeated passage retains conditional continuity without global claim", [] {
            const auto ref = repeated_reference(); AudioSync sync(ref, synthetic_config()); check(acquire(sync, ref).lease.locked);
            AudioSyncEstimate result;
            for (unsigned i = 11; i <= 150; ++i) {
                const auto obs = observation(ref, 5 + .1*i, i+1);
                result = sync.observe(obs, obs.received_host_ns);
                check(result.lease.locked);
            }
            check(result.continuity_assumed && !result.global_unique);
            // A hidden seek by exactly the 5.49-second repeat is acoustically indistinguishable.
            // This result explicitly remains conditional; an external seek/pause signal must reset it.
            auto same_audio = observation(ref, 20.1 + 5.49, 152);
            const auto expected_host = observation(ref, 20.1, 152);
            same_audio.window_end_host_ns = expected_host.window_end_host_ns;
            same_audio.received_host_ns = expected_host.received_host_ns;
            result = sync.observe(same_audio, same_audio.received_host_ns);
            check(result.lease.locked && result.continuity_assumed && !result.global_unique);
        }},
        {"drift estimates phase and rate on multiband features", [] {
            const auto ref = reference(2); AudioSync sync(ref, synthetic_config());
            constexpr double true_rate = 1.0005;
            auto result = acquire(sync, ref, true_rate);
            check(result.lease.locked);
            std::vector<double> errors;
            for (unsigned i = 11; i <= 300; ++i) {
                const double song = 5 + .1*i;
                const auto obs = observation(ref, song, i+1, true_rate);
                result = sync.observe(obs, obs.received_host_ns);
                check(result.lease.locked && result.continuity_assumed);
                errors.push_back(std::abs(static_cast<double>(result.clock.to_host(static_cast<Ns>(std::llround(song*1e9)))) - obs.window_end_host_ns) / 1e6);
            }
            std::sort(errors.begin(), errors.end());
            const double ppm_error = std::abs(static_cast<double>(result.clock.host_per_song) - true_rate) * 1e6;
            check(errors.back() < 10 && ppm_error < 1200);
            std::cout << "synthetic 100Hz/2band/1s-window +500ppm phase-ms p50=" << errors[errors.size()/2]
                      << " p95=" << errors[errors.size()*95/100] << " p99=" << errors[errors.size()*99/100]
                      << " max=" << errors.back() << " rate-error-ppm=" << ppm_error << '\n';
        }},
        {"200Hz multiband reference is supported", [] {
            const auto ref = reference(3, 200); AudioSync sync(ref, synthetic_config());
            const auto result = acquire(sync, ref);
            check(result.lease.locked && result.peak > .999);
        }},
        {"pause resume restart seek and geometry invalidate epochs", [] {
            const auto ref = reference(); AudioSync sync(ref, synthetic_config()); const auto locked = acquire(sync, ref);
            sync.set_paused(true, locked.lease.observed_host_ns + 20000000);
            auto result = sync.last_result();
            check(result.state == AudioSyncState::paused && !result.lease.locked && result.lease.epoch > locked.lease.epoch);
            const auto ignored = observation(ref, 6.1, 12);
            check(sync.observe(ignored, ignored.received_host_ns).state == AudioSyncState::paused);
            sync.set_paused(false, ignored.received_host_ns);
            const auto epoch = sync.last_result().lease.epoch;
            check(!sync.last_result().lease.locked && sync.last_result().state == AudioSyncState::acquiring);
            check(acquire(sync, ref, 1, 20).lease.locked);
            sync.reset(2, AudioSyncReason::seek);
            check(sync.last_result().lease.epoch > epoch && sync.last_result().lease.geometry_version == 2 && !sync.last_result().lease.locked);
            check(acquire(sync, ref, 1, 40).lease.locked);
            sync.reset(2, AudioSyncReason::restart);
            check(!sync.last_result().lease.locked);
        }},
        {"unannounced nonrepeated seek and stopped song fail continuity", [] {
            const auto ref = reference();
            for (unsigned mode = 0; mode < 2; ++mode) {
                AudioSync sync(ref, synthetic_config()); check(acquire(sync, ref).lease.locked);
                auto changed = observation(ref, mode == 0 ? 30 : 6, 12);
                const auto host = observation(ref, 6.1, 12);
                changed.window_end_host_ns = host.window_end_host_ns; changed.received_host_ns = host.received_host_ns;
                check(!sync.observe(changed, changed.received_host_ns).lease.locked);
            }
        }},
        {"clock fault while paused cannot implicitly resume acquisition", [] {
            const auto ref = reference(); AudioSync sync(ref, synthetic_config()); const auto locked = acquire(sync, ref);
            sync.set_paused(true, locked.lease.observed_host_ns + 20000000);
            const auto paused = sync.last_result();
            const auto fault = sync.status(locked.lease.observed_host_ns - 1);
            check(fault.state == AudioSyncState::paused && !fault.lease.locked && fault.lease.epoch > paused.lease.epoch);
            check(acquire(sync, ref, 1, 20).state == AudioSyncState::paused);
        }},
        {"delay future delivery uncertainty and backwards clocks reject", [] {
            const auto ref = reference();
            for (unsigned mode = 0; mode < 6; ++mode) {
                AudioSync sync(ref, synthetic_config()); const auto locked = acquire(sync, ref); auto obs = observation(ref, 6.1, 12);
                Ns now = obs.received_host_ns;
                if (mode == 0) now += 1000000000;
                if (mode == 1) obs.received_host_ns = obs.window_end_host_ns - 1;
                if (mode == 2) obs.received_host_ns = now + 1;
                if (mode == 3) obs.timestamp_uncertainty_ns = 20000001;
                if (mode == 4) now = locked.lease.observed_host_ns - 1;
                if (mode == 5) { obs.window_end_host_ns = locked.lease.observed_host_ns - 1; obs.received_host_ns = now; }
                const auto result = sync.observe(obs, now);
                check(!result.lease.locked && result.lease.epoch > locked.lease.epoch);
            }
        }},
        {"NaN malformed frames and duplicate observations never renew lease", [] {
            const auto ref = reference();
            for (unsigned mode = 0; mode < 4; ++mode) {
                AudioSync sync(ref, synthetic_config()); const auto locked = acquire(sync, ref); auto obs = observation(ref, 6.1, 12);
                if (mode == 0) obs.values[0] = std::numeric_limits<float>::quiet_NaN();
                if (mode == 1) obs.rms = std::numeric_limits<double>::quiet_NaN();
                if (mode == 2) obs.values.pop_back();
                if (mode == 3) obs.observation_id = locked.observation_id;
                const auto result = sync.observe(obs, obs.received_host_ns);
                check(!result.lease.locked && result.lease.observed_host_ns == locked.lease.observed_host_ns);
            }
        }},
        {"bounded work and constructor validation", [] {
            const auto ref = reference(); auto limited = synthetic_config(); limited.max_correlation_terms = 1;
            AudioSync sync(ref, limited); const auto obs = observation(ref, 5, 1);
            check(sync.observe(obs, obs.received_host_ns).reason == AudioSyncReason::work_limit);
            auto bad = ref; bad.values[0] = std::numeric_limits<float>::infinity();
            rejects([&] { AudioSync invalid(bad); });
            bad = ref; bad.feature_rate_hz = 44100;
            rejects([&] { AudioSync invalid(bad); });
            bad = ref; bad.audio_sha256 = "unknown";
            rejects([&] { AudioSync invalid(bad); });
            auto config = AudioSyncConfig{}; config.acquire_anchor_count = 1;
            rejects([&] { AudioSync invalid(ref, config); });
        }}
    };
    for (const auto& [name, run] : tests) {
        try { run(); }
        catch (const std::exception& error) { std::cerr << "FAIL " << name << ": " << error.what() << '\n'; return 1; }
    }
    std::cout << tests.size() << " offline audio synchronization checks passed\n";
    return 0;
}
