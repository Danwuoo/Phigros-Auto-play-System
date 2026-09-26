#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace pas {

struct Frame;
// Pixels stay in memory unless the caller explicitly requests one diagnostic image.
std::optional<std::string> save_capture_diagnostic(
    const std::filesystem::path& path, const Frame& frame, bool keep);
struct FixtureCounter {
    int value = 0;
    const char* schema = "";
    int max_color_error = 0;
    std::optional<int> binary_value;
    std::optional<int> exact_colour_value;
};
std::optional<FixtureCounter> decode_capture_fixture_counter(const Frame& frame);

struct CaptureBenchOptions {
    std::string serial;
    std::string backend = "emulator-grpc";
    std::string transport = "payload";
    std::string grpc_endpoint;
    std::filesystem::path grpc_token_file;
    std::string image_format = "rgb888";
    std::string row_order = "top-down";
    std::string grpc_copy_mode = "fast-memcpy";
    int grpc_read_chunk_kib = 256;
    bool grpc_rotate_ccw = false;
    std::string run_class = "development_smoke";
    std::string window_hwnd;
    int monitor_index = 0;
    int crop_x = 0;
    int crop_y = 0;
    int wgc_crop_x = -1;
    int wgc_crop_y = -1;
    int dxgi_crop_x = -1;
    int dxgi_crop_y = -1;
    std::filesystem::path scrcpy_server;
    int scrcpy_max_fps = 60;
    int scrcpy_video_bit_rate = 8'000'000;
    std::string scrcpy_video_encoder = "c2.android.avc.encoder";
    std::size_t max_rgb_bytes = 16 * 1024 * 1024;
    int width = 1280;
    int height = 720;
    int source_rotation = 1;
    double ready_timeout_s = 15;
    double warmup_s = 10;
    double duration_s = 60;
    double consumer_delay_ms = 0;
    std::optional<double> consumer_recover_after_s;
    double receiver_pause_ms = 0;
    bool load = false;
    bool gpu_load = false;
    bool preview = false;
    bool keep_diagnostic_image = false;
    double source_static_s = 0;
    int memory_load_mib = 0;
    std::optional<double> max_relative_lag_ms;
    bool diagnostic_mmap = false;
    bool fixture = false;
    bool fixture_position_truth = false;
    std::string fixture_schema = "native-v2";
    std::filesystem::path fixture_apk;
    std::filesystem::path output_dir;
};

void run_capture_bench(const CaptureBenchOptions& options);
void run_capture_campaign(CaptureBenchOptions options, int normal_runs,
                          bool include_stress, double stability_s);
void run_five_capture_campaign(CaptureBenchOptions options, int normal_runs,
                               bool include_stress, double stability_s,
                               const std::string& source_revision,
                               const std::string& dirty_diff_sha256,
                               bool plan_only);

} // namespace pas
