#pragma once
#include "rhythm/chart_memory.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace rhythm {
enum class CapturePcmEncoding { signed16, float32 };
// Pure conversion, also exercised without opening any endpoint.
std::vector<float> decode_capture_mono(std::span<const std::byte> bytes,
    std::uint32_t frames, unsigned channels, CapturePcmEncoding encoding, bool silent);
Ns audio_host_qpc_ns();
struct CapturedAudioPacket {
    std::uint64_t sequence = 0, generation = 0, device_position_frames = 0;
    std::uint64_t qpc_position_100ns = 0;
    Ns first_host_ns = 0, received_host_ns = 0;
    unsigned sample_rate = 0;
    std::uint32_t flags = 0;
    bool silent = false, discontinuity = false, timestamp_error = false;
    std::vector<float> mono;
};
struct AudioCaptureConfig {
    Ns max_duration_ns = 180'000'000'000;
    std::size_t max_queued_packets = 32;
};
struct AudioCaptureStatus {
    bool started = false, running = false, finished = false, failed = false;
    std::string error, endpoint_id;
    unsigned sample_rate = 0, channels = 0, bits_per_sample = 0;
    std::uint64_t packets = 0, frames = 0, silent_packets = 0;
    std::uint64_t discontinuities = 0, timestamp_errors = 0, dropped_packets = 0, generation = 1;
};
// Explicit start only. Dedicated capture worker does no correlation and no input.
// Captures the default eRender/eMultimedia mix, not process-isolated or microphone.
// QPC timestamps describe the packet's first sample, never callback arrival.
class WasapiLoopbackCapture {
public:
    explicit WasapiLoopbackCapture(AudioCaptureConfig config = {});
    ~WasapiLoopbackCapture();
    WasapiLoopbackCapture(const WasapiLoopbackCapture&) = delete;
    WasapiLoopbackCapture& operator=(const WasapiLoopbackCapture&) = delete;
    void start();
    void stop();
    std::vector<CapturedAudioPacket> drain();
    AudioCaptureStatus status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace rhythm
