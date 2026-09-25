#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace pas {

void run_touch_bench(const std::filesystem::path& config_path, int repetitions,
                     const std::vector<std::string>& kinds,
                     const std::filesystem::path& output_dir,
                     const std::filesystem::path& fixture_apk = {});
void run_touch_batch_bench(const std::filesystem::path& config_path, int repetitions,
                           const std::filesystem::path& output_dir,
                           const std::filesystem::path& fixture_apk = {});
void run_touch_disconnect_smoke(const std::filesystem::path& config_path,
                                const std::filesystem::path& output_dir,
                                const std::filesystem::path& fixture_apk = {});

} // namespace pas
