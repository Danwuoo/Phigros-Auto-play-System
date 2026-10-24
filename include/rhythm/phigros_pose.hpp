#pragma once

#include "rhythm/chart.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace rhythm {
struct PosePoint { double x = 0, y = 0; };
inline constexpr double phigros_reference_width = 1280;
inline constexpr double phigros_reference_height = 720;
inline constexpr double phigros_position_x_pixels = 72;

struct PhigrosLinePose {
    PosePoint origin; // Normalized reference canvas, top-left origin, y down.
    PosePoint tangent; // Unit vector in isotropic reference-pixel coordinates.
    PosePoint above_normal; // Unit vector; below uses its negative.
    double rotation_degrees = 0; // Raw native CCW angle; positive turns x toward screen up.
    double opacity = 0; // Raw value, not clamped or interpreted as gameplay eligibility.
    std::size_t move_event_index = 0, rotate_event_index = 0, opacity_event_index = 0;
};
struct PhigrosNoteTarget {
    PosePoint reference_pixels;
    PosePoint normalized;
    PosePoint side_normal;
    bool inside_reference = false; // Inclusive continuous canvas edges.
};

// Mathematical v3 line-local point only; not an official hit-region model.
// At normal_offset_pixels=0 above/below share the same line-center target.
// Nonzero normal offsets are explicit diagnostic geometry, not judgment tolerance.
PhigrosNoteTarget phigros_line_point(const PhigrosLinePose& pose, double position_x,
                                    NoteSide side, double normal_offset_pixels = 0);

class PhigrosPoseEvaluator {
public:
    // Owns validated copies of just the pose tracks; O(events) construction,
    // O(log(events per track)) arbitrary-time lookups, including reverse seeks.
    // Missing tracks, nonfinite values, unsorted starts, and interval overlaps
    // are rejected. Intervals are [start,end); later source events win shared
    // start boundaries. Zero-duration events step to end at their timestamp.
    // Before first -> first start, gaps/after last -> preceding end, never
    // extrapolate. Gap endpoint hold is an explicit adapter policy, not a claim
    // about unspecified native formats. SongNs is already offset-normalized.
    explicit PhigrosPoseEvaluator(const SemanticChart& chart);
    std::size_t line_count() const noexcept;
    PhigrosLinePose line_at(std::size_t line_index, SongNs song_ns) const;
    PhigrosNoteTarget note_target(std::size_t line_index, const SemanticNote& note,
                                  SongNs song_ns) const;
private:
    struct LineIndex {
        std::vector<MoveLineEvent> move;
        std::vector<NumericLineEvent> rotate, opacity;
    };
    std::vector<LineIndex> lines_;
};

enum class InputRotation { none, clockwise_90, clockwise_180, clockwise_270 };
enum class ViewportFit { contain, stretch };
struct PixelViewport { double left = 0, top = 0, width = 0, height = 0; };
struct PhigrosGeometry {
    std::uint32_t input_width = 0, input_height = 0; // Actual backend surface dimensions.
    // Rotates upright display coordinates clockwise INTO the input surface.
    // 90: (u,v)->(1-v,u); this is a declared transform, not inferred metadata.
    InputRotation rotation = InputRotation::none;
    // Viewport in upright DISPLAY pixel-edge coordinates, before rotation.
    // Absent = full display. For quarter turns, display dimensions are swapped.
    std::optional<PixelViewport> viewport;
    ViewportFit fit = ViewportFit::contain; // Centered 16:9 letterbox, or explicit stretch.
};
enum class GeometryStatus { mapped, outside_reference, outside_input };
struct InputPixel { std::uint32_t x = 0, y = 0; };
struct MappedPhigrosPoint {
    PosePoint input_normalized; // Continuous input pixel-edge domain.
    GeometryStatus status = GeometryStatus::outside_reference;
    std::optional<InputPixel> pixel; // Absent for every offscreen target.
};

class PhigrosGeometryMap {
public:
    explicit PhigrosGeometryMap(PhigrosGeometry geometry);
    // No offscreen clamping or normal-direction projection. Accepted continuous
    // content edges rasterize inward into the content's final pixel cells.
    MappedPhigrosPoint map_normalized(PosePoint reference_normalized) const;
    MappedPhigrosPoint map_reference_pixels(PosePoint reference_pixels) const;
    PixelViewport content_viewport() const noexcept;
    const PhigrosGeometry& geometry() const noexcept;
private:
    PhigrosGeometry geometry_;
    PixelViewport content_;
    double display_width_ = 0, display_height_ = 0;
    PosePoint rotate_to_input(PosePoint display_normalized) const;
};
} // namespace rhythm
