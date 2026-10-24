#include "rhythm/audio_sync.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rhythm {
namespace {
Ns rounded_ns(long double value) {
    value = std::round(value);
    if (!std::isfinite(value) || value < static_cast<long double>(std::numeric_limits<Ns>::min()) ||
        value >= static_cast<long double>(std::numeric_limits<Ns>::max()))
        throw std::overflow_error("audio clock overflow");
    return static_cast<Ns>(value);
}
long double song_at(const ClockMap& clock, Ns host) {
    return static_cast<long double>(clock.song_anchor_ns) +
           (static_cast<long double>(host) - clock.host_anchor_ns) / clock.host_per_song;
}
bool finite_positive(double value) { return std::isfinite(value) && value > 0; }
} // namespace

const char* audio_sync_state_name(AudioSyncState state) {
    switch (state) {
    case AudioSyncState::acquiring: return "acquiring";
    case AudioSyncState::locked: return "locked";
    case AudioSyncState::lost: return "lost";
    case AudioSyncState::paused: return "paused";
    }
    return "invalid";
}
const char* audio_sync_reason_name(AudioSyncReason reason) {
    switch (reason) {
#define REASON(name) case AudioSyncReason::name: return #name
    REASON(none); REASON(need_more_anchors); REASON(low_energy); REASON(weak_match);
    REASON(ambiguous); REASON(inconsistent); REASON(stale); REASON(invalid_observation);
    REASON(nonfinite); REASON(clock_jump); REASON(work_limit); REASON(uncertainty);
    REASON(paused); REASON(reset); REASON(seek); REASON(restart); REASON(geometry_change);
    REASON(end_of_reference);
#undef REASON
    }
    return "invalid";
}

AudioSync::AudioSync(AudioFeatureReference reference, AudioSyncConfig config, std::uint64_t geometry)
    : reference_(std::move(reference)), config_(config), geometry_(geometry) {
    const auto& c = config_;
    if ((reference_.feature_rate_hz != 100 && reference_.feature_rate_hz != 200) ||
        !reference_.bands || reference_.bands > 8 || !geometry_ ||
        !sha256_text(reference_.audio_sha256) || reference_.feature_id.empty() ||
        reference_.feature_id.size() > 1024 || reference_.song_start_ns < 0)
        throw std::invalid_argument("invalid audio feature reference identity/format");
    if (c.window_frames < 16 || c.window_frames > 2000 || !c.max_reference_frames ||
        c.max_reference_frames > 120000 || !c.max_correlation_terms || c.max_correlation_terms > 1000000000 ||
        c.acquire_anchor_count < 3 || c.max_anchor_count < c.acquire_anchor_count || c.max_anchor_count > 64 ||
        !c.global_peak_exclusion_frames || c.global_peak_exclusion_frames > c.window_frames / 2 ||
        !c.local_peak_exclusion_frames)
        throw std::invalid_argument("invalid bounded correlation/anchor configuration");
    for (const double value : {c.min_rms, c.min_feature_variance, c.max_abs_feature,
                              c.min_correlation, c.global_peak_margin, c.local_peak_margin,
                              c.max_rate_deviation_ppm})
        if (!finite_positive(value)) throw std::invalid_argument("invalid correlation numeric configuration");
    if (c.min_correlation > 1 || c.global_peak_margin > 1 || c.local_peak_margin > 1 ||
        c.max_rate_deviation_ppm > 20000) throw std::invalid_argument("invalid correlation/rate threshold");
    for (const Ns value : {c.local_search_radius_ns, c.max_phase_residual_ns, c.min_anchor_spacing_ns,
                           c.min_acquire_span_ns, c.max_acquire_gap_ns, c.max_observation_gap_ns,
                           c.max_observation_age_ns, c.lease_duration_ns, c.max_uncertainty_ns})
        if (value <= 0 || value > 60000000000LL) throw std::invalid_argument("invalid audio time budget");
    if (c.max_timestamp_uncertainty_ns < 0 || c.max_timestamp_uncertainty_ns > c.max_uncertainty_ns ||
        c.local_peak_exclusion_frames * 1000000000.L / reference_.feature_rate_hz >= c.local_search_radius_ns)
        throw std::invalid_argument("invalid mapping uncertainty/local exclusion");
    if (reference_.values.size() % reference_.bands != 0) throw std::invalid_argument("incomplete reference frame");
    const auto frames = reference_.values.size() / reference_.bands;
    if (frames < c.window_frames || frames > c.max_reference_frames) throw std::invalid_argument("reference frame budget exceeded");
    prefix_sum_.assign((frames + 1) * reference_.bands, 0);
    prefix_square_.assign(prefix_sum_.size(), 0);
    for (std::size_t f = 0; f < frames; ++f) for (unsigned b = 0; b < reference_.bands; ++b) {
        const double value = reference_.values[f * reference_.bands + b];
        if (!std::isfinite(value) || std::abs(value) > c.max_abs_feature) throw std::invalid_argument("nonfinite/out-of-range reference feature");
        prefix_sum_[(f+1)*reference_.bands+b] = prefix_sum_[f*reference_.bands+b] + value;
        prefix_square_[(f+1)*reference_.bands+b] = prefix_square_[f*reference_.bands+b] + value*value;
    }
    (void)rounded_ns(static_cast<long double>(reference_.song_start_ns) + frames * 1000000000.L / reference_.feature_rate_hz);
    result_.reference_audio_sha256 = reference_.audio_sha256;
    result_.feature_id = reference_.feature_id;
    result_.lease.epoch = epoch_; result_.lease.geometry_version = geometry_;
}

void AudioSync::bump_epoch() {
    if (epoch_ == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("audio epoch exhausted");
    ++epoch_;
    result_.lease.epoch = epoch_;
}
void AudioSync::lose(AudioSyncReason reason) {
    const bool was_paused = result_.state == AudioSyncState::paused;
    if (result_.state != AudioSyncState::lost) bump_epoch();
    result_.state = was_paused ? AudioSyncState::paused : AudioSyncState::lost; result_.reason = reason;
    result_.lease.locked = false; result_.global_unique = result_.continuity_assumed = false;
    anchors_.clear(); result_.anchor_count = 0;
}
void AudioSync::reset(std::uint64_t geometry, AudioSyncReason reason) {
    if (!geometry) throw std::invalid_argument("zero geometry version");
    bump_epoch(); geometry_ = geometry;
    result_ = {}; result_.reference_audio_sha256 = reference_.audio_sha256;
    result_.feature_id = reference_.feature_id; result_.reason = reason;
    result_.lease.epoch = epoch_; result_.lease.geometry_version = geometry_;
    anchors_.clear(); last_id_ = 0; last_end_ = last_now_ = -1; base_uncertainty_ = 0;
}
void AudioSync::set_paused(bool paused, Ns now) {
    (void)status(now);
    if (paused && result_.state != AudioSyncState::paused) {
        reset(geometry_, AudioSyncReason::paused);
        result_.state = AudioSyncState::paused; last_now_ = now;
    } else if (!paused && result_.state == AudioSyncState::paused) {
        reset(geometry_, AudioSyncReason::restart); last_now_ = now;
    }
}
AudioSyncEstimate AudioSync::status(Ns now) {
    if (now < 0 || (last_now_ >= 0 && now < last_now_)) {
        lose(AudioSyncReason::clock_jump); last_now_ = now; return result_;
    }
    last_now_ = now;
    if (result_.state == AudioSyncState::locked) {
        const auto age = static_cast<long double>(now) - result_.lease.observed_host_ns;
        if (now >= result_.lease.expires_host_ns || age > config_.max_observation_age_ns) lose(AudioSyncReason::stale);
        else {
            result_.lease.uncertainty_ns = rounded_ns(base_uncertainty_ + age * config_.max_rate_deviation_ppm / 1000000.L);
            if (result_.lease.uncertainty_ns > config_.max_uncertainty_ns) lose(AudioSyncReason::uncertainty);
        }
    } else if (result_.state == AudioSyncState::acquiring && !anchors_.empty() &&
               static_cast<long double>(now) - anchors_.back().host > config_.max_acquire_gap_ns)
        lose(AudioSyncReason::stale);
    return result_;
}

AudioSync::Match AudioSync::correlate(const AudioObservation& observation, bool local) const {
    const auto n = config_.window_frames;
    const auto bands = reference_.bands;
    const auto frames = reference_.values.size() / bands;
    std::size_t low = 0, high = frames - n;
    if (local) {
        const long double center = (song_at(result_.clock, observation.window_end_host_ns) - reference_.song_start_ns) *
                                   reference_.feature_rate_hz / 1000000000.L - n;
        const long double radius = config_.local_search_radius_ns * reference_.feature_rate_hz / 1000000000.L;
        if (center + radius < 0 || center - radius > high) return {0,-1,-1,0,AudioSyncReason::end_of_reference};
        low = static_cast<std::size_t>(std::max(0.L, std::floor(center - radius)));
        high = static_cast<std::size_t>(std::min(static_cast<long double>(high), std::ceil(center + radius)));
    }
    const auto width = high - low + 1;
    if (width > config_.max_correlation_terms / (n * bands)) return {0,-1,-1,0,AudioSyncReason::work_limit};
    std::vector<double> centered(observation.values.begin(), observation.values.end()), means(bands, 0);
    for (std::size_t f = 0; f < n; ++f) for (unsigned b = 0; b < bands; ++b) means[b] += centered[f*bands+b] / n;
    double query_energy = 0;
    for (std::size_t f = 0; f < n; ++f) for (unsigned b = 0; b < bands; ++b) {
        auto& value = centered[f*bands+b]; value -= means[b]; query_energy += value*value;
    }
    if (query_energy / (n * bands) < config_.min_feature_variance) return {0,-1,-1,0,AudioSyncReason::low_energy};
    std::vector<double> scores(width, -1);
    std::size_t best = 0;
    double peak = -1;
    for (std::size_t start = low; start <= high; ++start) {
        double energy = 0, dot = 0;
        for (unsigned b = 0; b < bands; ++b) {
            const double sum = prefix_sum_[(start+n)*bands+b] - prefix_sum_[start*bands+b];
            const double mean = sum / n;
            energy += std::max(0., prefix_square_[(start+n)*bands+b] - prefix_square_[start*bands+b] - sum*mean);
            for (std::size_t f = 0; f < n; ++f) dot += centered[f*bands+b] * (reference_.values[(start+f)*bands+b] - mean);
        }
        if (energy / (n * bands) < config_.min_feature_variance) continue;
        const double score = std::clamp(dot / std::sqrt(query_energy * energy), -1., 1.);
        scores[start-low] = score;
        if (score > peak) { peak = score; best = start-low; }
    }
    double second = -1;
    const auto exclusion = local ? config_.local_peak_exclusion_frames : config_.global_peak_exclusion_frames;
    for (std::size_t i = 0; i < width; ++i)
        if ((i > best ? i-best : best-i) > exclusion) second = std::max(second, scores[i]);
    double fraction = 0;
    if (best && best + 1 < width) {
        const double curve = scores[best-1] - 2*scores[best] + scores[best+1];
        if (curve < -1e-12) fraction = std::clamp(.5*(scores[best-1]-scores[best+1])/curve, -.5, .5);
    }
    Match match;
    match.song = rounded_ns(reference_.song_start_ns + (static_cast<long double>(low+best+n) + fraction) * 1000000000.L / reference_.feature_rate_hz);
    match.peak = peak; match.second = second; match.margin = peak - second;
    if (peak < config_.min_correlation) match.reason = AudioSyncReason::weak_match;
    else if (match.margin < (local ? config_.local_peak_margin : config_.global_peak_margin)) match.reason = AudioSyncReason::ambiguous;
    return match;
}

bool AudioSync::fit(const std::vector<Anchor>& anchors, ClockMap& clock, Ns& residual) const {
    if (anchors.size() < 2) return false;
    long double xbar = 0, ybar = 0;
    for (const auto& a : anchors) { xbar += static_cast<long double>(a.song) - anchors[0].song; ybar += static_cast<long double>(a.host) - anchors[0].host; }
    xbar /= anchors.size(); ybar /= anchors.size();
    long double xx = 0, xy = 0;
    for (const auto& a : anchors) {
        const auto x = static_cast<long double>(a.song) - anchors[0].song - xbar;
        const auto y = static_cast<long double>(a.host) - anchors[0].host - ybar;
        xx += x*x; xy += x*y;
    }
    if (xx <= 0) return false;
    const auto rate = xy / xx;
    if (!std::isfinite(rate) || rate <= 0 || std::abs(rate-1) * 1000000.L > config_.max_rate_deviation_ppm) return false;
    clock.song_anchor_ns = anchors.back().song;
    clock.host_anchor_ns = rounded_ns(anchors[0].host + ybar + rate * (static_cast<long double>(clock.song_anchor_ns) - anchors[0].song - xbar));
    clock.host_per_song = rate;
    long double maximum = 0;
    for (const auto& a : anchors) maximum = std::max(maximum, std::abs(static_cast<long double>(a.host) - clock.to_host(a.song)));
    residual = rounded_ns(std::ceil(maximum));
    return residual <= config_.max_phase_residual_ns;
}

AudioSyncEstimate AudioSync::observe(const AudioObservation& observation, Ns now) {
    if (now < 0 || (last_now_ >= 0 && now < last_now_)) { lose(AudioSyncReason::clock_jump); last_now_ = now; return result_; }
    (void)status(now);
    if (result_.state == AudioSyncState::paused) return result_;
    const auto& o = observation;
    if (!o.observation_id || o.observation_id <= last_id_ || o.window_end_host_ns < 0 ||
        o.received_host_ns < o.window_end_host_ns || o.received_host_ns > now ||
        o.values.size() != config_.window_frames * reference_.bands || o.timestamp_uncertainty_ns < 0) {
        lose(AudioSyncReason::invalid_observation); return result_;
    }
    if (last_end_ >= 0 && o.window_end_host_ns <= last_end_) { lose(AudioSyncReason::clock_jump); return result_; }
    const auto gap = last_end_ < 0 ? 0.L : static_cast<long double>(o.window_end_host_ns) - last_end_;
    last_id_ = o.observation_id; last_end_ = o.window_end_host_ns; result_.observation_id = o.observation_id;
    if (static_cast<long double>(now) - o.window_end_host_ns > config_.max_observation_age_ns) { lose(AudioSyncReason::stale); return result_; }
    if (o.timestamp_uncertainty_ns > config_.max_timestamp_uncertainty_ns) { lose(AudioSyncReason::uncertainty); return result_; }
    if (!std::isfinite(o.rms) || std::any_of(o.values.begin(), o.values.end(), [&](float value) { return !std::isfinite(value) || std::abs(value) > config_.max_abs_feature; })) {
        lose(AudioSyncReason::nonfinite); return result_;
    }
    if (o.rms < config_.min_rms) { lose(AudioSyncReason::low_energy); return result_; }
    if (result_.state == AudioSyncState::locked && gap > config_.max_observation_gap_ns) lose(AudioSyncReason::stale);
    const bool local = result_.state == AudioSyncState::locked;
    const Match match = correlate(o, local);
    result_.peak = match.peak; result_.runner_up = match.second; result_.margin = match.margin; result_.matched_song_ns = match.song;
    result_.global_unique = !local && match.reason == AudioSyncReason::none;
    result_.continuity_assumed = local;
    if (match.reason != AudioSyncReason::none) {
        if (!local && (match.reason == AudioSyncReason::ambiguous || match.reason == AudioSyncReason::weak_match) && !anchors_.empty()) {
            result_.state = AudioSyncState::acquiring; result_.reason = match.reason; result_.lease.locked = false;
        } else lose(match.reason);
        return result_;
    }
    if (!anchors_.empty()) {
        const auto& first = anchors_[0];
        const long double expected = local ? result_.clock.to_host(match.song) :
            static_cast<long double>(first.host) + match.song - first.song;
        const long double allowed = config_.max_phase_residual_ns + o.timestamp_uncertainty_ns + first.uncertainty +
            (local ? 0.L : std::abs(static_cast<long double>(match.song)-first.song) * config_.max_rate_deviation_ppm / 1000000.L);
        result_.phase_residual_ns = rounded_ns(static_cast<long double>(o.window_end_host_ns) - expected);
        if (std::abs(static_cast<long double>(result_.phase_residual_ns)) > allowed) { lose(AudioSyncReason::inconsistent); return result_; }
    }
    const long double anchor_spacing = local ? config_.min_anchor_spacing_ns :
        std::max(static_cast<long double>(config_.min_anchor_spacing_ns),
                 config_.window_frames * 500000000.L / reference_.feature_rate_hz);
    if (anchors_.empty() || static_cast<long double>(o.window_end_host_ns) - anchors_.back().host >= anchor_spacing) {
        anchors_.push_back({match.song, o.window_end_host_ns, o.timestamp_uncertainty_ns});
        if (anchors_.size() > config_.max_anchor_count) anchors_.erase(anchors_.begin());
    }
    result_.anchor_count = anchors_.size();
    if (anchors_.size() < config_.acquire_anchor_count ||
        static_cast<long double>(anchors_.back().host) - anchors_.front().host < config_.min_acquire_span_ns) {
        result_.state = AudioSyncState::acquiring; result_.reason = AudioSyncReason::need_more_anchors;
        result_.lease.locked = false; return result_;
    }
    ClockMap clock; Ns fit_residual = 0;
    if (!fit(anchors_, clock, fit_residual)) { lose(AudioSyncReason::inconsistent); return result_; }
    Ns timestamp_budget = o.timestamp_uncertainty_ns;
    for (const auto& a : anchors_) timestamp_budget = std::max(timestamp_budget, a.uncertainty);
    const auto half_frame = 500000000.L / reference_.feature_rate_hz;
    const auto nominal_window_warp = config_.window_frames * 1000000000.L / reference_.feature_rate_hz *
                                     config_.max_rate_deviation_ppm / 2000000.L;
    base_uncertainty_ = rounded_ns(std::ceil(timestamp_budget + fit_residual + half_frame + nominal_window_warp));
    if (base_uncertainty_ > config_.max_uncertainty_ns) { lose(AudioSyncReason::uncertainty); return result_; }
    result_.clock = clock; result_.fit_residual_ns = fit_residual;
    result_.state = AudioSyncState::locked; result_.reason = AudioSyncReason::none;
    result_.lease = {true, epoch_, geometry_, o.window_end_host_ns,
                     rounded_ns(static_cast<long double>(o.window_end_host_ns) + config_.lease_duration_ns), base_uncertainty_};
    return status(now);
}
} // namespace rhythm
