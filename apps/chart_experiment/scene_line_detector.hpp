#pragma once

#include "scene_opening_sync.hpp"
#include "rhythm/screen_state.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace rhythm::experimental {
struct SceneLineDetection {
    std::vector<SceneOpeningLine> lines;
    const char* reason = "no_supported_scene_lines";
};

struct HorizontalSceneLineContrast {
    int lower_quartile = 0, median = 0, upper_quartile = 0;
};

namespace scene_line_detail {
inline constexpr int sample_step = 4;
inline constexpr int minimum_samples = 56;
inline constexpr int maximum_gap = 3;
inline constexpr int side_offset = 4;

inline bool valid_image(RgbImageView image) {
    constexpr int row_bytes = 1280 * 3;
    return image.width == 1280 && image.height == 720 && image.stride >= row_bytes &&
           image.pixels.size() >= row_bytes &&
           static_cast<std::size_t>(image.stride) <= (image.pixels.size() - row_bytes) / 719;
}

struct Support {
    int first = 0, last = -1, hits = 0;
    int span() const { return last - first + 1; }
};

inline int luminance(RgbImageView image, int x, int y) {
    const auto pixel = image.pixels.data() + static_cast<std::size_t>(y) * image.stride + x * 3;
    return (77 * pixel[0] + 150 * pixel[1] + 29 * pixel[2]) >> 8;
}

inline int ridge(RgbImageView image, bool vertical, int normal, int along) {
    const auto sample = [&](int offset) {
        return vertical ? luminance(image, normal + offset, along) : luminance(image, along, normal + offset);
    };
    const auto center = sample(0);
    if (center < 112) return 0;
    const auto contrast = center - std::max(sample(-side_offset), sample(side_offset));
    return contrast >= 24 ? contrast : 0;
}

inline Support support(RgbImageView image, bool vertical, int normal, int along_begin, int along_end) {
    Support best, run;
    for (int along = along_begin, index = 0; along < along_end; along += sample_step, ++index) {
        if (!ridge(image, vertical, normal, along)) continue;
        if (index - run.last > maximum_gap + 1 || run.hits == 0) run = {index, index, 0};
        run.last = index;
        ++run.hits;
        if (run.span() >= minimum_samples && run.hits * 5 >= run.span() * 4 && run.span() > best.span())
            best = run;
    }
    return best;
}

inline bool detect_axis(RgbImageView image, SceneOpeningAxis axis, std::vector<SceneOpeningLine>& lines) {
    const bool vertical = axis == SceneOpeningAxis::vertical;
    // The outer playfield strips can contain score, pause, title, and menu chrome.
    const int normal_begin = vertical ? 40 : 72, normal_end = vertical ? 1240 : 648;
    const int along_begin = vertical ? 72 : 48, along_end = vertical ? 648 : 1232;
    std::array<Support, 1280> supports{};
    for (int normal = normal_begin; normal < normal_end; ++normal)
        supports[normal] = support(image, vertical, normal, along_begin, along_end);

    for (int normal = normal_begin; normal < normal_end;) {
        if (supports[normal].hits == 0) { ++normal; continue; }
        const int first = normal;
        auto strongest = supports[normal];
        do {
            if (supports[normal].span() > strongest.span()) strongest = supports[normal];
            ++normal;
        } while (normal < normal_end && supports[normal].hits != 0);
        if (normal - first > 6) continue;

        // Check the spine in three distant parts of the same long stroke. A
        // diagonal edge or a run of neighboring note heads must not supply an
        // apparently stable axis-normal coordinate from one small patch.
        const int radius = strongest.span() / 8;
        const std::array anchors{strongest.first + radius, (strongest.first + strongest.last) / 2,
                                 strongest.last - radius};
        std::array<double, 3> centers{};
        bool supported = true;
        for (std::size_t i = 0; i < anchors.size(); ++i) {
            int weight = 0, position = 0, hits = 0;
            for (int along = anchors[i] - radius; along <= anchors[i] + radius; ++along) {
                int total = 0;
                for (int p = std::max(normal_begin, first - 2); p < std::min(normal_end, normal + 2); ++p) {
                    const auto value = ridge(image, vertical, p, along_begin + along * sample_step);
                    total += value;
                    position += p * value;
                }
                weight += total;
                hits += total != 0;
            }
            if (hits <= radius) { supported = false; break; }
            centers[i] = static_cast<double>(position) / weight;
        }
        if (!supported || *std::max_element(centers.begin(), centers.end()) -
                          *std::min_element(centers.begin(), centers.end()) > 2.5) continue;
        if (lines.size() == scene_opening_max_lines) return false;
        lines.push_back({axis, (centers[0] + centers[1] + centers[2]) / 3});
    }
    return true;
}
}

// Bounded shape observations only. Rendering identity and timing come from a
// separate chart matcher. No image data leaves the call.
inline SceneLineDetection detect_scene_lines(RgbImageView image) {
    if (!scene_line_detail::valid_image(image)) return {{}, "scene_line_geometry_invalid"};
    SceneLineDetection out;
    out.lines.reserve(scene_opening_max_lines);
    if (!scene_line_detail::detect_axis(image, SceneOpeningAxis::horizontal, out.lines) ||
        !scene_line_detail::detect_axis(image, SceneOpeningAxis::vertical, out.lines))
        return {{}, "scene_line_count_exceeded"};
    if (!out.lines.empty()) out.reason = "axis_scene_lines_observed";
    return out;
}

// Observe contrast at one chart-provided row. Contrast is relative to this
// frame's nearby background and is not a calibrated opacity or timing witness.
inline std::optional<HorizontalSceneLineContrast> sample_horizontal_scene_line_contrast(RgbImageView image, double y) {
    if (!scene_line_detail::valid_image(image) || !std::isfinite(y) || y < 72 || y >= 648) return std::nullopt;
    const auto row = static_cast<int>(std::lround(y));
    std::array<int, 192> samples{};
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto x = 256 + static_cast<int>(i) * 4;
        const auto gray = [&](int delta) { return scene_line_detail::luminance(image, x, row + delta); };
        samples[i] = std::max({gray(-1), gray(0), gray(1)}) - std::max(gray(-4), gray(4));
    }
    std::sort(samples.begin(), samples.end());
    return HorizontalSceneLineContrast{samples[47], (samples[95] + samples[96]) / 2, samples[143]};
}
}
