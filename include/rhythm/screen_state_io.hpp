#pragma once
#include "rhythm/screen_state.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rhythm {
struct ScreenStateCalibration {
    int width=0,height=0;
    std::string dataset_sha256;
    std::vector<ScreenStateTemplate> templates;
};
// Offline calibration only: <=1 MiB v1 JSON, bounded exact RGB bytes/hashes,
// strict integer geometry, unique calibration IDs and validated anchor regions.
// Samples are intentionally not loaded or required. Source-image hashes remain
// producer-supplied provenance, not independently decoded PNG verification.
ScreenStateCalibration load_screen_state_calibration(const std::filesystem::path& dataset_path);
// Allows a caller such as screen_state_probe to read its dataset once and use
// exactly the same bytes for calibration and sample metadata. Relative RGB paths
// are resolved against base_directory. Does not infer clocks/chart identity.
ScreenStateCalibration parse_screen_state_calibration(std::string_view dataset_json,
    const std::filesystem::path& base_directory);
} // namespace rhythm
