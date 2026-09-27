#pragma once
#include "pas/config.hpp"
#include "pas/emulator.hpp"

namespace pas {
// Session lifetime only; never supplied to note decisions or touch plans.
class GameRunBudget final {
public:
    GameRunBudget(Nanoseconds start,Nanoseconds duration,Nanoseconds wait_play=0);
    bool arm_from_playing(Nanoseconds observed_ns);
    bool expired(Nanoseconds now) const { return now>=deadline_; }
    bool waiting() const { return wait_play_>0&&!origin_; }
    Nanoseconds deadline() const { return deadline_; }
    std::optional<Nanoseconds> origin() const { return origin_; }
    const char* expiry_reason() const { return waiting()?"waiting_for_play_timeout":"duration"; }
private:
    Nanoseconds start_,duration_,wait_play_,deadline_;
    std::optional<Nanoseconds> origin_;
};
CaptureOptions runtime_capture_options(const RuntimeConfig& config);
nlohmann::json match_touch_capability(const RuntimeConfig& config, const nlohmann::json& report,
                                     const nlohmann::json& device, const std::string& installed_hash);
nlohmann::json game_preflight(const std::string& config_path, const std::string& capability_path);
void run_observe(const std::string& config_path, double duration_s, bool no_preview,
                 const std::string& launch_package = "", double stale_ms = 100);
void run_auto_start(const std::string& config_path, const std::string& capability_path,
                    double duration_s, bool no_preview);
void run_assist(const std::string& config_path, const std::string& capability_path,
                double duration_s, bool no_preview,bool keep_diagnostic_anomalies=false,
                bool keep_vision_dataset=false,const std::string& tracking_shadow="",bool manual_play=false,
                double wait_play_s=60);
}
