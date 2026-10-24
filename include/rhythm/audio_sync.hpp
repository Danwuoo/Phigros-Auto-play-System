#pragma once

#include "rhythm/chart_memory.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rhythm {
struct AudioFeatureReference {
    unsigned feature_rate_hz = 100, bands = 1; // Only 100/200 Hz, 1..8 bands in this implementation.
    std::vector<float> values; // Frame-interleaved features, not PCM. Frame zero has song_start_ns.
    Ns song_start_ns = 0;
    std::string audio_sha256, feature_id; // Caller-verified WAV/feature identity; not hashed by this class.
};
struct AudioObservation {
    std::vector<float> values; // Exactly config.window_frames * reference.bands.
    std::uint64_t observation_id = 0; // Strictly increasing real observations; ticks must not invent IDs.
    Ns window_end_host_ns = 0; // Exclusive window end, mapped from sample/device position to host clock.
    Ns received_host_ns = 0; // Host delivery time; never substituted for sample time.
    Ns timestamp_uncertainty_ns = -1; // Unknown rejects; zero requires an explicit exact test clock.
    double rms = 0; // Actual PCM RMS before log/normalization; silence must not become energetic features.
};
struct AudioSyncConfig {
    std::size_t window_frames = 500, max_reference_frames = 120000;
    std::size_t max_correlation_terms = 100000000, max_anchor_count = 16;
    std::size_t acquire_anchor_count = 3, global_peak_exclusion_frames = 100;
    std::size_t local_peak_exclusion_frames = 3;
    double min_rms = 1e-5, min_feature_variance = 1e-8, max_abs_feature = 1e6;
    double min_correlation = .8, global_peak_margin = .08, local_peak_margin = .02;
    double max_rate_deviation_ppm = 3000;
    Ns local_search_radius_ns = 150000000, max_phase_residual_ns = 30000000;
    // Acquisition anchors additionally require >= half a feature window of new samples.
    // Local tracking can fit more frequently; correlated/overlapping windows are not independent trials.
    Ns min_anchor_spacing_ns = 500000000, min_acquire_span_ns = 1000000000;
    Ns max_acquire_gap_ns = 10000000000LL, max_observation_gap_ns = 500000000;
    Ns max_observation_age_ns = 200000000, lease_duration_ns = 250000000;
    Ns max_timestamp_uncertainty_ns = 20000000, max_uncertainty_ns = 30000000;
    // Defaults are engineering/test budgets, not measured game judgment windows or calibrated error bounds.
};
enum class AudioSyncState { acquiring, locked, lost, paused };
enum class AudioSyncReason {
    none, need_more_anchors, low_energy, weak_match, ambiguous, inconsistent,
    stale, invalid_observation, nonfinite, clock_jump, work_limit, uncertainty,
    paused, reset, seek, restart, geometry_change, end_of_reference
};
const char* audio_sync_state_name(AudioSyncState state);
const char* audio_sync_reason_name(AudioSyncReason reason);
struct AudioSyncEstimate {
    AudioSyncState state = AudioSyncState::acquiring;
    AudioSyncReason reason = AudioSyncReason::need_more_anchors;
    std::uint64_t observation_id = 0;
    std::string reference_audio_sha256, feature_id;
    ClockMap clock;
    SyncLease lease;
    Ns matched_song_ns = 0, phase_residual_ns = 0, fit_residual_ns = 0;
    double peak = -1, runner_up = -1, margin = 0;
    std::size_t anchor_count = 0;
    bool global_unique = false, continuity_assumed = false;
    // Local tracking cannot detect a seek into an acoustically identical repeated section.
    // uncertainty is a configured/model budget, not a statistically calibrated confidence interval.
};

class AudioSync {
public:
    AudioSync(AudioFeatureReference reference, AudioSyncConfig config = {}, std::uint64_t geometry_version = 1);
    AudioSyncEstimate observe(const AudioObservation& observation, Ns now_host_ns);
    AudioSyncEstimate status(Ns now_host_ns); // Age/expiry only; never creates or renews evidence.
    AudioSyncEstimate snapshot(Ns now_host_ns) { return status(now_host_ns); }
    const AudioSyncEstimate& last_result() const { return result_; }
    void reset(std::uint64_t geometry_version, AudioSyncReason reason = AudioSyncReason::reset);
    void set_paused(bool paused, Ns now_host_ns); // Explicit external state observation, no automatic resume lock.
private:
    struct Anchor { Ns song, host, uncertainty; };
    struct Match { Ns song = 0; double peak = -1, second = -1, margin = 0; AudioSyncReason reason = AudioSyncReason::none; };
    Match correlate(const AudioObservation& observation, bool local) const;
    bool fit(const std::vector<Anchor>& anchors, ClockMap& clock, Ns& residual) const;
    void lose(AudioSyncReason reason);
    void bump_epoch();
    AudioFeatureReference reference_;
    AudioSyncConfig config_;
    AudioSyncEstimate result_;
    std::vector<double> prefix_sum_, prefix_square_;
    std::vector<Anchor> anchors_;
    std::uint64_t epoch_ = 1, geometry_ = 1, last_id_ = 0;
    Ns last_now_ = -1, last_end_ = -1, base_uncertainty_ = 0;
};
} // namespace rhythm
