#include "../apps/chart_experiment/scene_line_detector.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using namespace rhythm;
using namespace rhythm::experimental;

struct Frame {
    static constexpr int width = 1280, height = 720, stride = width * 3;
    std::vector<std::uint8_t> pixels = std::vector<std::uint8_t>(stride * height, 20);

    void rectangle(int left, int top, int right, int bottom, std::array<std::uint8_t, 3> color) {
        for (int y = top; y < bottom; ++y) for (int x = left; x < right; ++x)
            std::copy(color.begin(), color.end(), pixels.begin() + y * stride + x * 3);
    }

    RgbImageView image() const { return {width, height, stride, pixels}; }
};

constexpr std::array<std::uint8_t, 3> white{232, 232, 232}, gold{240, 202, 125}, background{20, 20, 20};

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

bool contains(const SceneLineDetection& detection, SceneOpeningAxis axis, double coordinate) {
    return std::any_of(detection.lines.begin(), detection.lines.end(), [&](const auto& line) {
        return line.axis == axis && std::abs(line.coordinate - coordinate) < 0.75;
    });
}
}

int main() {
    try {
        Frame frame;
        check(detect_scene_lines(frame.image()).lines.empty(), "blank frame must provide no strokes");
        frame.rectangle(100, 359, 1170, 362, white);
        frame.rectangle(238, 130, 243, 590, gold);
        frame.rectangle(550, 359, 562, 362, background);
        frame.rectangle(500, 260, 680, 265, white);
        frame.rectangle(470, 475, 1000, 493, white);
        frame.rectangle(48, 40, 1232, 43, white);
        frame.rectangle(1260, 80, 1264, 650, white);
        const auto observed = detect_scene_lines(frame.image());
        check(observed.lines.size() == 2 && contains(observed, SceneOpeningAxis::horizontal, 360) &&
              contains(observed, SceneOpeningAxis::vertical, 240), "mixed thin axes must reject heads, thick bars and HUD chrome");

        Frame clipped;
        clipped.rectangle(0, 199, 305, 202, white);
        clipped.rectangle(899, 0, 902, 355, gold);
        const auto partial = detect_scene_lines(clipped.image());
        check(partial.lines.size() == 2 && contains(partial, SceneOpeningAxis::horizontal, 200) &&
              contains(partial, SceneOpeningAxis::vertical, 900), "a clipped long line still has a measurable axis coordinate");

        Frame fade;
        fade.rectangle(100, 359, 1180, 362, white);
        const auto bright = sample_horizontal_scene_line_contrast(fade.image(), 360.4);
        fade.rectangle(100, 359, 1180, 362, {120, 120, 120});
        const auto dim = sample_horizontal_scene_line_contrast(fade.image(), 360.4);
        check(bright && dim && bright->median == 212 && dim->median == 100 &&
              dim->lower_quartile == dim->upper_quartile,
              "a predicted static row exposes robust relative contrast without inferring opacity");
        check(!sample_horizontal_scene_line_contrast(fade.image(), 40), "predicted HUD row is outside the sample domain");

        Frame diagonal;
        for (int x = 128; x < 1152; ++x) {
            const int y = 220 + (x - 128) * 17 / 1000;
            diagonal.rectangle(x, y - 2, x + 1, y + 3, white);
        }
        check(detect_scene_lines(diagonal.image()).lines.empty(), "diagonal strokes must not masquerade as axis lines");

        Frame overloaded;
        for (int i = 0; i < 25; ++i) overloaded.rectangle(100, 100 + i * 16, 1180, 102 + i * 16, white);
        const auto overflow = detect_scene_lines(overloaded.image());
        check(overflow.lines.empty() && std::string_view(overflow.reason) == "scene_line_count_exceeded",
              "overloaded frames must not retain a favorable subset");
        auto invalid = frame.image();
        invalid.pixels = invalid.pixels.first(10);
        check(std::string_view(detect_scene_lines(invalid).reason) == "scene_line_geometry_invalid",
              "invalid buffer length must fail before sampling");
        std::cout << "Scene line detector contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
