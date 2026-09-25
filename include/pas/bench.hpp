#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace pas {

struct CaptureBenchOptions {
    std::string serial;
    std::string backend = "emulator-grpc";
    std::string transport = "payload";
    std::string grpc_endpoint;
    std::filesystem::path grpc_token_file;
    std::string image_format = "rgb888";
    std::string row_order = "top-down";
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
    std::optional<double> max_relative_lag_ms;
    bool diagnostic_mmap = false;
    bool fixture = false;
    std::string fixture_schema = "native-v2";
    std::filesystem::path fixture_apk;
    std::filesystem::path output_dir;
};

void run_capture_bench(const CaptureBenchOptions& options);
void run_capture_campaign(CaptureBenchOptions options, int normal_runs,
                          bool include_stress, double stability_s);

} // namespace pas
