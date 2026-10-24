#pragma once
#include "rhythm/supervisor.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rhythm {
struct RgbImageView {
    int width=0,height=0,stride=0;
    std::span<const std::uint8_t> pixels;
};
struct PixelRegion { int x=0,y=0,width=0,height=0; };
struct StateAnchor {
    PixelRegion region;
    std::vector<std::uint8_t> grayscale;
};
struct ScreenStateTemplate {
    ObservedGameState state=ObservedGameState::unknown;
    int width=0,height=0;
    std::string source_image_sha256,calibration_id;
    std::vector<StateAnchor> anchors;
};
struct ScreenStateConfig {
    double max_anchor_mean_error=8;
    int minimum_anchor_contrast=40;
    std::size_t max_template_pixels=65536,max_templates=8;
};
struct ScreenTemplateScore {
    ObservedGameState state=ObservedGameState::unknown;
    std::string calibration_id;
    double worst_anchor_mean_error=255;
    bool all_anchors_match=false;
};
struct ScreenStateResult {
    ObservedGameState state=ObservedGameState::unknown;
    std::string reason;
    std::vector<ScreenTemplateScore> scores;
};

// Only compares calibrated pixel anchors. No OCR, clock, lease, device, automatic
// arm, or chart identity inference. An actual frame/timestamp producer is separate.
ScreenStateTemplate make_screen_state_template(const RgbImageView& image,
    ObservedGameState state,std::vector<PixelRegion> regions,
    std::string source_image_sha256,std::string calibration_id,ScreenStateConfig config={});
ScreenStateResult classify_screen_state(const RgbImageView& image,
    std::span<const ScreenStateTemplate> templates,ScreenStateConfig config={});
} // namespace rhythm
