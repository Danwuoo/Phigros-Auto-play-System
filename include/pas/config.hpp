#pragma once

#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>

namespace pas {

struct RuntimeConfig {
    std::string name;
    std::string serial;
    std::string capture_kind;
    std::string capture_transport;
    int width = 0;
    int height = 0;
    int source_rotation = 0;
    int grpc_read_chunk_kib = 256;
    std::string touch_kind;
    int touch_width = 0;
    int touch_height = 0;
    int touch_rotation = 0;
    int touch_timeout_ms = 500;
    int max_contacts = 2;
    int max_plans = 64;
    int max_steps = 16;
    int horizon_ms = 2000;
    int evidence_max_age_ms = 150;
    double preview_hz = 0;
    std::string log_dir;
    std::string endpoint;
    std::filesystem::path token_file;
    nlohmann::json public_json;
};

RuntimeConfig load_config(const std::filesystem::path& path);
void migrate_config(const std::filesystem::path& source, const std::filesystem::path& target);

} // namespace pas
