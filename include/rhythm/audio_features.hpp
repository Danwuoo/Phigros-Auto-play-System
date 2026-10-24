#pragma once
#include "rhythm/audio_sync.hpp"
#include <array>
#include <complex>
#include <optional>
#include <span>

namespace rhythm {
enum class AudioFeatureKind { log_rms_v1, iir8_log_energy_v1, fft8_log_energy_v1 };
unsigned audio_feature_bands(AudioFeatureKind kind);
const char* audio_feature_id(AudioFeatureKind kind);
// Feature k ends at origin + (k+1)*10ms. FFT uses a 20ms window/10ms hop.
Ns audio_feature_time_origin_ns(AudioFeatureKind kind);
struct AudioFeatureFrames {
    std::vector<float> values;
    std::vector<double> rms; // Physical mono PCM RMS before feature log/floor.
    std::vector<std::size_t> end_sample_offsets; // Exclusive offsets within the most recent push.
};
class MonoFeatureExtractor {
public:
    MonoFeatureExtractor(unsigned sample_rate, AudioFeatureKind kind);
    AudioFeatureFrames push(std::span<const float> mono_samples); // At most two seconds per call.
    void reset();
    std::size_t partial_sample_count() const { return partial_count_; }
private:
    unsigned sample_rate_, bands_;
    AudioFeatureKind kind_;
    std::size_t samples_per_frame_, partial_count_ = 0;
    double raw_energy_ = 0;
    std::array<double, 7> alpha_{}, stage1_{}, stage2_{};
    std::array<double, 8> band_energy_{};
    std::size_t fft_count_ = 0;
    std::vector<double> fft_samples_, hann_;
    std::vector<std::complex<double>> chirp_, kernel_, scratch_;
    double fft_normalizer_ = 0;
    void append_fft(AudioFeatureFrames& result, std::size_t end_offset);
};
struct AudioFeatureSeries {
    unsigned feature_rate_hz = 100, bands = 1;
    std::string feature_id;
    Ns feature_origin_ns = 0; // Feed this into AudioFeatureReference.song_start_ns.
    std::vector<float> values;
    std::vector<double> rms;
    std::size_t source_sample_frames = 0, dropped_tail_samples = 0;
};
AudioFeatureSeries extract_audio_features(std::span<const float> mono_samples, unsigned sample_rate,
                                         AudioFeatureKind kind, std::size_t max_frames = 120000);

struct MonoAudioPacket {
    std::span<const float> samples;
    unsigned sample_rate = 0;
    std::uint64_t device_position = 0; // First mono sample-frame position, not an audio byte count.
    Ns first_host_ns = -1; // Sample/device/QPC mapping for the first sample, not callback arrival.
    Ns received_host_ns = -1;
    Ns timestamp_uncertainty_ns = -1; // Unknown by default: such packets are refused.
    bool discontinuity = false, timestamp_error = false;
};
enum class AudioFeatureReason {
    none, discontinuity, device_gap, time_gap, timestamp_error,
    unknown_uncertainty, uncertainty, unsupported_sample_rate, invalid_packet, nonfinite
};
const char* audio_feature_reason_name(AudioFeatureReason reason);
struct AudioFeatureConfig {
    AudioFeatureKind kind = AudioFeatureKind::iir8_log_energy_v1;
    std::size_t window_frames = 500, hop_frames = 5; // 5s ring window, 50ms emission by default.
    // Output uncertainty includes the continuity tolerance; the remaining budget bounds packet mapping uncertainty.
    Ns continuity_tolerance_ns = 2000000, max_timestamp_uncertainty_ns = 20000000;
};
struct AudioFeatureBatch {
    std::vector<AudioObservation> observations;
    bool packet_accepted = false, segment_reset = false; // segment_reset requires immediate downstream sync invalidation.
    AudioFeatureReason reason = AudioFeatureReason::none;
    std::uint64_t segment_id = 1;
};
class AudioFeatureAssembler {
public:
    explicit AudioFeatureAssembler(AudioFeatureConfig config = {});
    AudioFeatureBatch push(const MonoAudioPacket& packet);
    void reset(); // Clears partial/filter/window state. Observation IDs remain strictly increasing.
    std::uint64_t segment_id() const { return segment_id_; }
    std::uint64_t last_observation_id() const { return observation_id_; }
    const char* feature_id() const { return audio_feature_id(config_.kind); }
    unsigned bands() const { return audio_feature_bands(config_.kind); }
private:
    AudioFeatureConfig config_;
    std::optional<MonoFeatureExtractor> extractor_;
    std::vector<float> ring_;
    std::array<double, 500> ring_energy_{};
    std::array<Ns, 500> ring_uncertainty_{};
    std::size_t ring_next_ = 0, feature_count_ = 0, next_emit_ = 0;
    unsigned sample_rate_ = 0;
    std::uint64_t segment_id_ = 1, observation_id_ = 0, expected_position_ = 0;
    Ns expected_first_ns_ = -1, last_received_ns_ = -1;
    Ns bin_uncertainty_ns_ = 0, previous_bin_uncertainty_ns_ = 0;
    std::size_t uncertainty_partial_samples_ = 0;
    bool uncertainty_first_bin_ = true;
};
} // namespace rhythm
