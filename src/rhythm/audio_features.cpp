#include "rhythm/audio_features.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace rhythm {
namespace {
bool valid_rate(unsigned rate, AudioFeatureKind kind) {
    return rate >= (kind == AudioFeatureKind::log_rms_v1 ? 8000u : 32000u) &&
           rate <= 192000 && rate % 100 == 0;
}
// Iterative power-of-two FFT; Bluestein below preserves the original, unpadded DFT bins.
void fft(std::vector<std::complex<double>>& data, bool inverse) {
    const auto size = data.size();
    for (std::size_t i = 1, j = 0; i < size; ++i) {
        auto bit = size >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }
    for (std::size_t length = 2; length <= size; length <<= 1) {
        const auto root = std::polar(1., (inverse ? 2 : -2) * std::numbers::pi / length);
        for (std::size_t start = 0; start < size; start += length) {
            std::complex<double> phase{1, 0};
            for (std::size_t j = 0; j < length / 2; ++j) {
                const auto a = data[start+j], b = data[start+j+length/2] * phase;
                data[start+j] = a+b; data[start+j+length/2] = a-b; phase *= root;
            }
        }
    }
    if (inverse) for (auto& value : data) value /= static_cast<double>(size);
}
Ns sample_time(Ns first, std::size_t offset, unsigned rate) {
    const auto result = std::round(static_cast<long double>(first) + offset * 1000000000.L / rate);
    if (!std::isfinite(result) || result < 0 || result >= static_cast<long double>(std::numeric_limits<Ns>::max()))
        throw std::overflow_error("sample clock overflow");
    return static_cast<Ns>(result);
}
} // namespace
unsigned audio_feature_bands(AudioFeatureKind kind) {
    switch (kind) {
    case AudioFeatureKind::log_rms_v1: return 1;
    case AudioFeatureKind::iir8_log_energy_v1: return 8;
    case AudioFeatureKind::fft8_log_energy_v1: return 8;
    }
    throw std::invalid_argument("unknown audio feature kind");
}
const char* audio_feature_id(AudioFeatureKind kind) {
    switch (kind) {
    case AudioFeatureKind::log_rms_v1: return "log-rms-100hz-mono-v1;floor=1e-5";
    case AudioFeatureKind::iir8_log_energy_v1:
        return "iir2x1pole8-logrms-100hz-v1;cutoffs=100,250,500,1000,2000,4000,8000;alpha=1-exp(-2pi*f/fs);floor=1e-5";
    case AudioFeatureKind::fft8_log_energy_v1:
        return "fft8-logrms-100hz-v1;window=20ms-hann-symmetric;hop=10ms;dft=unpadded-bluestein-f64;cutoffs=100,250,500,1000,2000,4000,8000;floor=1e-5";
    }
    throw std::invalid_argument("unknown audio feature kind");
}
Ns audio_feature_time_origin_ns(AudioFeatureKind kind) {
    (void)audio_feature_bands(kind); // Reject unknown enum values.
    return kind == AudioFeatureKind::fft8_log_energy_v1 ? 10000000 : 0;
}
const char* audio_feature_reason_name(AudioFeatureReason reason) {
    switch (reason) {
#define REASON(name) case AudioFeatureReason::name: return #name
    REASON(none); REASON(discontinuity); REASON(device_gap); REASON(time_gap); REASON(timestamp_error);
    REASON(unknown_uncertainty); REASON(uncertainty); REASON(unsupported_sample_rate);
    REASON(invalid_packet); REASON(nonfinite);
#undef REASON
    }
    return "invalid";
}
MonoFeatureExtractor::MonoFeatureExtractor(unsigned rate, AudioFeatureKind kind)
    : sample_rate_(rate), bands_(audio_feature_bands(kind)), kind_(kind), samples_per_frame_(rate / 100) {
    if (!valid_rate(rate, kind)) throw std::invalid_argument("sample rate must support exact 100Hz bins");
    constexpr std::array<double, 7> cutoffs{100,250,500,1000,2000,4000,8000};
    for (std::size_t b = 0; b < alpha_.size(); ++b)
        alpha_[b] = 1 - std::exp(-2 * std::numbers::pi * cutoffs[b] / rate);
    if (kind == AudioFeatureKind::fft8_log_energy_v1) {
        const auto size = 2 * samples_per_frame_;
        std::size_t padded = 1; while (padded < 2 * size - 1) padded <<= 1;
        fft_samples_.resize(size); hann_.resize(size); chirp_.resize(size);
        kernel_.resize(padded); scratch_.resize(padded);
        for (std::size_t i = 0; i < size; ++i) {
            hann_[i] = .5 - .5 * std::cos(2 * std::numbers::pi * i / (size - 1));
            fft_normalizer_ += size * hann_[i] * hann_[i];
            const auto phase = std::numbers::pi * static_cast<double>((i*i) % (2*size)) / size;
            chirp_[i] = std::polar(1., -phase);
            kernel_[i] = std::conj(chirp_[i]);
            if (i) kernel_[padded-i] = kernel_[i];
        }
        fft(kernel_, false);
    }
}
void MonoFeatureExtractor::reset() {
    partial_count_ = 0; raw_energy_ = 0;
    stage1_.fill(0); stage2_.fill(0); band_energy_.fill(0);
    fft_count_ = 0; std::fill(fft_samples_.begin(), fft_samples_.end(), 0);
}
void MonoFeatureExtractor::append_fft(AudioFeatureFrames& result, std::size_t end_offset) {
    std::fill(scratch_.begin(), scratch_.end(), std::complex<double>{});
    double raw = 0;
    for (std::size_t i = 0; i < fft_samples_.size(); ++i) {
        scratch_[i] = fft_samples_[i] * hann_[i] * chirp_[i];
        raw += fft_samples_[i] * fft_samples_[i];
    }
    fft(scratch_, false);
    for (std::size_t i = 0; i < scratch_.size(); ++i) scratch_[i] *= kernel_[i];
    fft(scratch_, true);
    std::array<double, 8> energy{};
    constexpr std::array<unsigned, 7> cutoffs{100,250,500,1000,2000,4000,8000};
    for (std::size_t k = 0; k <= fft_samples_.size() / 2; ++k) {
        // A 20ms unpadded DFT has exact 50Hz bins at all supported rates.
        const auto band = static_cast<std::size_t>(std::upper_bound(cutoffs.begin(), cutoffs.end(), k*50) - cutoffs.begin());
        const double scale = k == 0 || k == fft_samples_.size()/2 ? 1 : 2;
        energy[band] += std::norm(scratch_[k] * chirp_[k]) * scale / fft_normalizer_;
    }
    for (double value : energy)
        result.values.push_back(static_cast<float>(std::log(std::max(std::sqrt(value), 1e-5))));
    result.rms.push_back(std::sqrt(raw / fft_samples_.size()));
    result.end_sample_offsets.push_back(end_offset);
}
AudioFeatureFrames MonoFeatureExtractor::push(std::span<const float> samples) {
    if (samples.size() > sample_rate_ * 2u) throw std::invalid_argument("audio packet exceeds two seconds");
    for (float value : samples)
        if (!std::isfinite(value) || std::abs(value) > 8) throw std::invalid_argument("invalid PCM value");
    AudioFeatureFrames result;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        if (kind_ == AudioFeatureKind::fft8_log_energy_v1) {
            fft_samples_[fft_count_++] = samples[i];
            partial_count_ = fft_count_ % samples_per_frame_;
            if (fft_count_ == fft_samples_.size()) {
                append_fft(result, i+1);
                std::copy(fft_samples_.begin()+samples_per_frame_, fft_samples_.end(), fft_samples_.begin());
                fft_count_ = samples_per_frame_;
            }
            continue;
        }
        const double value = samples[i]; raw_energy_ += value * value;
        if (kind_ == AudioFeatureKind::log_rms_v1) band_energy_[0] += value * value;
        else {
            double previous = 0;
            for (std::size_t b = 0; b < 7; ++b) {
                stage1_[b] += alpha_[b] * (value - stage1_[b]);
                stage2_[b] += alpha_[b] * (stage1_[b] - stage2_[b]);
                const auto band = stage2_[b] - previous;
                band_energy_[b] += band * band; previous = stage2_[b];
            }
            const auto high = value - previous; band_energy_[7] += high * high;
        }
        if (++partial_count_ == samples_per_frame_) {
            for (unsigned b = 0; b < bands_; ++b)
                result.values.push_back(static_cast<float>(std::log(std::max(std::sqrt(band_energy_[b] / samples_per_frame_), 1e-5))));
            result.rms.push_back(std::sqrt(raw_energy_ / samples_per_frame_));
            result.end_sample_offsets.push_back(i + 1);
            partial_count_ = 0; raw_energy_ = 0; band_energy_.fill(0);
        }
    }
    return result;
}
AudioFeatureSeries extract_audio_features(std::span<const float> samples, unsigned rate,
                                         AudioFeatureKind kind, std::size_t max_frames) {
    if (!max_frames || max_frames > 120000 || !valid_rate(rate, kind))
        throw std::invalid_argument("offline feature resource/sample-rate limit");
    const auto bins = samples.size() / (rate / 100);
    const auto warmup = kind == AudioFeatureKind::fft8_log_energy_v1 ? 1u : 0u;
    const auto frames_count = bins > warmup ? bins - warmup : 0;
    if (frames_count > max_frames) throw std::invalid_argument("offline feature frame budget");
    MonoFeatureExtractor extractor(rate, kind);
    AudioFeatureSeries result; result.bands = audio_feature_bands(kind); result.feature_id = audio_feature_id(kind);
    result.feature_origin_ns = audio_feature_time_origin_ns(kind);
    result.source_sample_frames = samples.size();
    result.dropped_tail_samples = frames_count ? samples.size() % (rate/100) : samples.size();
    for (std::size_t offset = 0; offset < samples.size();) {
        const auto count = std::min<std::size_t>(samples.size() - offset, rate * 2u);
        auto frames = extractor.push(samples.subspan(offset, count));
        result.values.insert(result.values.end(), frames.values.begin(), frames.values.end());
        result.rms.insert(result.rms.end(), frames.rms.begin(), frames.rms.end()); offset += count;
    }
    return result;
}
AudioFeatureAssembler::AudioFeatureAssembler(AudioFeatureConfig config) : config_(config) {
    const auto band_count = audio_feature_bands(config_.kind);
    if (config_.window_frames < 16 || config_.window_frames > 500 || !config_.hop_frames ||
        config_.hop_frames > config_.window_frames || config_.continuity_tolerance_ns < 0 ||
        config_.continuity_tolerance_ns > 100000000 || config_.max_timestamp_uncertainty_ns < config_.continuity_tolerance_ns ||
        config_.max_timestamp_uncertainty_ns > 100000000)
        throw std::invalid_argument("invalid feature assembler configuration");
    ring_.resize(500 * band_count); next_emit_ = config_.window_frames;
}
void AudioFeatureAssembler::reset() {
    if (segment_id_ == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("audio segment exhausted");
    ++segment_id_; extractor_.reset(); sample_rate_ = 0;
    ring_next_ = feature_count_ = 0; next_emit_ = config_.window_frames;
    expected_position_ = 0; expected_first_ns_ = last_received_ns_ = -1;
    bin_uncertainty_ns_ = previous_bin_uncertainty_ns_ = 0;
    uncertainty_partial_samples_ = 0; uncertainty_first_bin_ = true;
}
AudioFeatureBatch AudioFeatureAssembler::push(const MonoAudioPacket& packet) {
    AudioFeatureBatch result; result.segment_id = segment_id_;
    const auto reject = [&](AudioFeatureReason reason) {
        reset(); result.segment_reset = true; result.segment_id = segment_id_; result.reason = reason;
        return result;
    };
    if (packet.timestamp_error) return reject(AudioFeatureReason::timestamp_error);
    if (packet.timestamp_uncertainty_ns < 0) return reject(AudioFeatureReason::unknown_uncertainty);
    if (packet.timestamp_uncertainty_ns > config_.max_timestamp_uncertainty_ns - config_.continuity_tolerance_ns)
        return reject(AudioFeatureReason::uncertainty);
    if (!valid_rate(packet.sample_rate, config_.kind)) return reject(AudioFeatureReason::unsupported_sample_rate);
    if (packet.samples.empty() || packet.samples.size() > packet.sample_rate * 2u || packet.first_host_ns < 0 ||
        packet.received_host_ns < packet.first_host_ns ||
        packet.device_position > std::numeric_limits<std::uint64_t>::max() - packet.samples.size())
        return reject(AudioFeatureReason::invalid_packet);
    for (float value : packet.samples) {
        if (!std::isfinite(value)) return reject(AudioFeatureReason::nonfinite);
        if (std::abs(value) > 8) return reject(AudioFeatureReason::invalid_packet);
    }
    Ns end = 0;
    try { end = sample_time(packet.first_host_ns, packet.samples.size(), packet.sample_rate); }
    catch (const std::exception&) { return reject(AudioFeatureReason::invalid_packet); }
    if (end > packet.received_host_ns) return reject(AudioFeatureReason::invalid_packet);
    AudioFeatureReason boundary = AudioFeatureReason::none;
    if (packet.discontinuity || (sample_rate_ && sample_rate_ != packet.sample_rate)) boundary = AudioFeatureReason::discontinuity;
    else if (extractor_ && packet.device_position != expected_position_) boundary = AudioFeatureReason::device_gap;
    else if (extractor_ && (packet.received_host_ns < last_received_ns_ ||
             std::abs(static_cast<long double>(packet.first_host_ns) - expected_first_ns_) > config_.continuity_tolerance_ns))
        boundary = AudioFeatureReason::time_gap;
    if (boundary != AudioFeatureReason::none) {
        reset(); result.segment_reset = true; result.reason = boundary; result.segment_id = segment_id_;
    }
    if (!extractor_) { extractor_.emplace(packet.sample_rate, config_.kind); sample_rate_ = packet.sample_rate; }
    auto frames = extractor_->push(packet.samples);
    std::vector<Ns> frame_uncertainties;
    for (std::size_t offset = 0; offset < packet.samples.size();) {
        const auto count = std::min(packet.samples.size()-offset, packet.sample_rate/100-uncertainty_partial_samples_);
        bin_uncertainty_ns_ = std::max(bin_uncertainty_ns_, packet.timestamp_uncertainty_ns);
        uncertainty_partial_samples_ += count; offset += count;
        if (uncertainty_partial_samples_ == packet.sample_rate/100) {
            if (config_.kind != AudioFeatureKind::fft8_log_energy_v1 || !uncertainty_first_bin_)
                frame_uncertainties.push_back(config_.kind == AudioFeatureKind::fft8_log_energy_v1 ?
                    std::max(previous_bin_uncertainty_ns_, bin_uncertainty_ns_) : bin_uncertainty_ns_);
            previous_bin_uncertainty_ns_ = bin_uncertainty_ns_; bin_uncertainty_ns_ = 0;
            uncertainty_partial_samples_ = 0; uncertainty_first_bin_ = false;
        }
    }
    if (frames.rms.size() != frame_uncertainties.size()) throw std::logic_error("feature uncertainty alignment");
    const unsigned band_count = bands();
    for (std::size_t f = 0; f < frames.rms.size(); ++f) {
        std::copy_n(frames.values.begin()+f*band_count, band_count, ring_.begin()+ring_next_*band_count);
        ring_energy_[ring_next_] = frames.rms[f]*frames.rms[f];
        ring_uncertainty_[ring_next_] = frame_uncertainties[f];
        ring_next_ = (ring_next_ + 1) % 500; ++feature_count_;
        if (feature_count_ == next_emit_) {
            if (observation_id_ == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("observation ID exhausted");
            AudioObservation observation; observation.observation_id = ++observation_id_;
            observation.window_end_host_ns = sample_time(packet.first_host_ns, frames.end_sample_offsets[f], packet.sample_rate);
            observation.received_host_ns = packet.received_host_ns;
            double energy = 0; Ns uncertainty = 0;
            observation.values.reserve(config_.window_frames * band_count);
            for (std::size_t i = 0; i < config_.window_frames; ++i) {
                const auto index = (ring_next_ + 500 - config_.window_frames + i) % 500;
                observation.values.insert(observation.values.end(), ring_.begin()+index*band_count, ring_.begin()+(index+1)*band_count);
                energy += ring_energy_[index]; uncertainty = std::max(uncertainty, ring_uncertainty_[index]);
            }
            observation.rms = std::sqrt(energy / config_.window_frames);
            observation.timestamp_uncertainty_ns = uncertainty + config_.continuity_tolerance_ns;
            result.observations.push_back(std::move(observation)); next_emit_ += config_.hop_frames;
        }
    }
    expected_position_ = packet.device_position + packet.samples.size(); expected_first_ns_ = end;
    last_received_ns_ = packet.received_host_ns; result.packet_accepted = true;
    return result;
}
} // namespace rhythm
