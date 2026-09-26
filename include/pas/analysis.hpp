#pragma once

#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace pas {

nlohmann::json distribution(std::vector<double> values);
nlohmann::json analyze_capture_jsonl(const std::filesystem::path& path);
nlohmann::json analyze_pause_jsonl(const std::filesystem::path& path);
nlohmann::json analyze_capture_campaign(const std::filesystem::path& directory);
std::string sha256_file(const std::filesystem::path& path);
bool diagnostic_image_matches(const std::filesystem::path& path,
                              const std::string& retention,
                              const nlohmann::json& expected_hash);

} // namespace pas
