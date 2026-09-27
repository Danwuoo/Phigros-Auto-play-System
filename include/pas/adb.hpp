#pragma once

#include "pas/core.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace pas {

std::filesystem::path find_adb();
std::vector<std::uint8_t> adb_call(const std::filesystem::path& adb,
                                   const std::vector<std::string>& arguments,
                                   int timeout_ms = 15'000);
nlohmann::json probe_adb(const std::filesystem::path& adb, const std::string& serial = "");
std::string installed_apk_sha256(const std::filesystem::path& adb,
                                 const std::string& serial, const std::string& package);
Frame capture_adb_png(const Clock& clock, const std::filesystem::path& adb,
                      const std::string& serial, std::uint64_t sequence);
// Offline diagnostics only. Never selected as a live runtime capture source.
Frame load_diagnostic_png(const std::filesystem::path& path);
void write_diagnostic_png(const std::filesystem::path& path,const Frame& frame);
void launch_android_package(const std::filesystem::path& adb, const std::string& serial,
                            const std::string& package);

} // namespace pas
