#pragma once
#include "pas/config.hpp"
#include "pas/emulator.hpp"

namespace pas {
CaptureOptions runtime_capture_options(const RuntimeConfig& config);
nlohmann::json match_touch_capability(const RuntimeConfig& config, const nlohmann::json& report,
                                     const nlohmann::json& device, const std::string& installed_hash);
nlohmann::json game_preflight(const std::string& config_path, const std::string& capability_path);
void run_observe(const std::string& config_path, double duration_s, bool no_preview,
                 const std::string& launch_package = "", double stale_ms = 100);
void run_auto_start(const std::string& config_path, const std::string& capability_path,
                    double duration_s, bool no_preview);
void run_assist(const std::string& config_path, const std::string& capability_path,
                double duration_s, bool no_preview,bool keep_diagnostic_anomalies=false);
}
