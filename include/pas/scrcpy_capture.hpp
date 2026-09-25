#pragma once

#include "pas/core.hpp"

#include <atomic>
#include <array>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>

namespace pas {

struct ScrcpyCaptureOptions {
    std::string serial;
    std::filesystem::path server_file;
    int width = 1280;
    int height = 720;
    int max_fps = 60;
    int video_bit_rate = 8'000'000;
    std::string video_encoder = "c2.android.avc.encoder";
};

struct ScrcpyWireHeader {
    bool session = false;
    bool config = false;
    bool key = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t packet_size = 0;
    std::int64_t pts_us = 0;
};

ScrcpyWireHeader parse_scrcpy_v41_header(const std::array<std::uint8_t, 12>& bytes);
std::string ffmpeg_runtime_version();
std::uint32_t ffmpeg_avcodec_version();
std::uint32_t ffmpeg_swscale_version();

class ScrcpyCapture final {
public:
    ScrcpyCapture(const Clock& clock, ScrcpyCaptureOptions options);
    ~ScrcpyCapture();
    void stream(std::stop_token stop, const std::function<void(Frame&&)>& on_frame,
                const std::function<void(std::string_view, Nanoseconds)>& on_signal = {});
    void cancel();
private:
    void stream_once(std::stop_token stop, const std::function<void(Frame&&)>& on_frame,
                     const std::function<void(std::string_view, Nanoseconds)>& on_signal);
    const Clock& clock_;
    ScrcpyCaptureOptions options_;
    std::atomic<bool> cancelled_{false};
    std::atomic<std::uintptr_t> active_socket_{~std::uintptr_t{0}};
    std::uint64_t sequence_ = 0;
    std::uint64_t generation_ = 0;
};

} // namespace pas
