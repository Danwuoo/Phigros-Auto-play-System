#include "rhythm/phigros_pose.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>

// Independently implemented coordinate math, cross-checked against:
// phisap 8ac9a67d584ed2c34faccebd9ad35194331ec9d5 judge_line.py:130-160;
// Phira 191b56e8404e26ab4bdf6c7d38dc577a265510a7 prpr/src/parse/pgr.rs
// (v3 move and position_x scaling) and core/line.rs:396 (below reflects local y).
// These are community render/planning references, not official judgment rules.
namespace rhythm {
namespace {
void finite(double value) {
    if (!std::isfinite(value)) throw std::invalid_argument("Nonfinite pose/geometry value");
}
void finite(PosePoint p) { finite(p.x); finite(p.y); }
bool inside(PosePoint p) { return p.x >= 0 && p.x <= 1 && p.y >= 0 && p.y <= 1; }
template<class Events> void validate_times(const Events& events) {
    if (events.empty()) throw std::invalid_argument("Pose track is missing events");
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto& t = events[i].time;
        finite(t.start_tick); finite(t.end_tick);
        if (t.start_ns > t.end_ns || t.start_tick > t.end_tick)
            throw std::invalid_argument("Reversed pose event interval");
        if (i && (t.start_ns < events[i-1].time.start_ns || t.start_ns < events[i-1].time.end_ns))
            throw std::invalid_argument("Unsorted or overlapping pose events");
    }
}
template<class Events> std::size_t find_event(const Events& events, SongNs song_ns) {
    const auto it = std::upper_bound(events.begin(), events.end(), song_ns,
        [](SongNs t, const auto& event) { return t < event.time.start_ns; });
    return it == events.begin() ? 0 : static_cast<std::size_t>(it - events.begin() - 1);
}
double fraction(const ChartEventTime& time, SongNs song_ns) {
    if (song_ns < time.start_ns) return 0;
    if (song_ns >= time.end_ns) return 1; // Includes zero-duration step.
    return static_cast<double>((static_cast<long double>(song_ns) - time.start_ns) /
        (static_cast<long double>(time.end_ns) - time.start_ns));
}
double interpolate(double start, double end, double fraction_value) {
    const double value = std::lerp(start, end, fraction_value);
    finite(value);
    return value;
}
std::pair<double, double> trig(double degrees) {
    const auto angle = std::remainder(degrees, 360.);
    // Exact cardinal values avoid inventing offscreen epsilon at canvas edges.
    if (angle == 0) return {1, 0};
    if (angle == 90) return {0, 1};
    if (angle == -90) return {0, -1};
    if (angle == 180 || angle == -180) return {-1, 0};
    const double radians = angle * std::numbers::pi / 180.;
    return {std::cos(radians), std::sin(radians)};
}
}

PhigrosNoteTarget phigros_line_point(const PhigrosLinePose& pose, double position_x,
                                    NoteSide side, double normal_offset_pixels) {
    finite(pose.origin); finite(pose.tangent); finite(pose.above_normal);
    finite(pose.rotation_degrees); finite(pose.opacity); finite(position_x); finite(normal_offset_pixels);
    if (side != NoteSide::above && side != NoteSide::below)
        throw std::invalid_argument("Unknown note side");
    const auto sign = side == NoteSide::above ? 1. : -1.;
    const PosePoint normal{pose.above_normal.x * sign, pose.above_normal.y * sign};
    const double along = position_x * phigros_position_x_pixels;
    const PosePoint pixels{
        pose.origin.x * phigros_reference_width + along * pose.tangent.x + normal_offset_pixels * normal.x,
        pose.origin.y * phigros_reference_height + along * pose.tangent.y + normal_offset_pixels * normal.y};
    finite(pixels);
    const PosePoint normalized{pixels.x / phigros_reference_width, pixels.y / phigros_reference_height};
    return {pixels, normalized, normal, inside(normalized)};
}

PhigrosPoseEvaluator::PhigrosPoseEvaluator(const SemanticChart& chart) {
    if (chart.format_version != 3) throw std::invalid_argument("Pose evaluator requires native v3 IR");
    finite(chart.offset_seconds);
    lines_.reserve(chart.lines.size());
    for (const auto& line : chart.lines) {
        finite(line.bpm);
        if (line.bpm <= 0) throw std::invalid_argument("Invalid pose line BPM");
        validate_times(line.move_events); validate_times(line.rotate_events); validate_times(line.opacity_events);
        for (const auto& event : line.move_events) {
            finite(event.start_x); finite(event.end_x); finite(event.start_y); finite(event.end_y);
        }
        for (const auto* events : {&line.rotate_events, &line.opacity_events})
            for (const auto& event : *events) { finite(event.start); finite(event.end); }
        lines_.push_back({line.move_events, line.rotate_events, line.opacity_events});
    }
}
std::size_t PhigrosPoseEvaluator::line_count() const noexcept { return lines_.size(); }
PhigrosLinePose PhigrosPoseEvaluator::line_at(std::size_t line_index, SongNs song_ns) const {
    const auto& line = lines_.at(line_index);
    const auto mi = find_event(line.move, song_ns), ri = find_event(line.rotate, song_ns), oi = find_event(line.opacity, song_ns);
    const auto& move = line.move[mi];
    const auto& rotate = line.rotate[ri];
    const auto& opacity = line.opacity[oi];
    const auto mf = fraction(move.time, song_ns);
    const auto x = interpolate(move.start_x, move.end_x, mf);
    const auto y = interpolate(move.start_y, move.end_y, mf);
    const auto degrees = interpolate(rotate.start, rotate.end, fraction(rotate.time, song_ns));
    const auto [c, s] = trig(degrees);
    return {{x, 1 - y}, {c, -s}, {-s, -c}, degrees,
            interpolate(opacity.start, opacity.end, fraction(opacity.time, song_ns)), mi, ri, oi};
}
PhigrosNoteTarget PhigrosPoseEvaluator::note_target(std::size_t line_index, const SemanticNote& note,
                                                   SongNs song_ns) const {
    finite(note.position_x); finite(note.speed); finite(note.floor_position);
    if (note.kind != NoteKind::tap && note.kind != NoteKind::drag && note.kind != NoteKind::hold && note.kind != NoteKind::flick)
        throw std::invalid_argument("Unknown note kind");
    return phigros_line_point(line_at(line_index, song_ns), note.position_x, note.side);
}

PhigrosGeometryMap::PhigrosGeometryMap(PhigrosGeometry geometry) : geometry_(std::move(geometry)) {
    if (!geometry_.input_width || !geometry_.input_height || geometry_.input_width > 65535 || geometry_.input_height > 65535)
        throw std::invalid_argument("Invalid input surface dimensions");
    switch (geometry_.rotation) {
    case InputRotation::none: case InputRotation::clockwise_180:
        display_width_ = geometry_.input_width; display_height_ = geometry_.input_height; break;
    case InputRotation::clockwise_90: case InputRotation::clockwise_270:
        display_width_ = geometry_.input_height; display_height_ = geometry_.input_width; break;
    default: throw std::invalid_argument("Unknown input rotation");
    }
    content_ = geometry_.viewport.value_or(PixelViewport{0, 0, display_width_, display_height_});
    finite(content_.left); finite(content_.top); finite(content_.width); finite(content_.height);
    if (content_.left < 0 || content_.top < 0 || content_.width <= 0 || content_.height <= 0 ||
        content_.left + content_.width > display_width_ || content_.top + content_.height > display_height_)
        throw std::invalid_argument("Viewport is outside the display surface");
    if (geometry_.fit == ViewportFit::contain) {
        const auto scale = std::min(content_.width / phigros_reference_width, content_.height / phigros_reference_height);
        const auto width = phigros_reference_width * scale, height = phigros_reference_height * scale;
        content_.left += (content_.width - width) / 2.; content_.top += (content_.height - height) / 2.;
        content_.width = width; content_.height = height;
    } else if (geometry_.fit != ViewportFit::stretch) throw std::invalid_argument("Unknown viewport fit mode");
}
PosePoint PhigrosGeometryMap::rotate_to_input(PosePoint point) const {
    switch (geometry_.rotation) {
    case InputRotation::none: return point;
    case InputRotation::clockwise_90: return {1-point.y, point.x};
    case InputRotation::clockwise_180: return {1-point.x, 1-point.y};
    case InputRotation::clockwise_270: return {point.y, 1-point.x};
    }
    throw std::invalid_argument("Unknown input rotation");
}
MappedPhigrosPoint PhigrosGeometryMap::map_normalized(PosePoint point) const {
    finite(point);
    const auto display = PosePoint{
        std::lerp(content_.left, content_.left + content_.width, point.x) / display_width_,
        std::lerp(content_.top, content_.top + content_.height, point.y) / display_height_};
    const auto input = rotate_to_input(display);
    finite(input);
    if (!inside(point)) return {input, GeometryStatus::outside_reference, std::nullopt};
    if (!inside(input)) return {input, GeometryStatus::outside_input, std::nullopt};
    const auto center = rotate_to_input({(content_.left + content_.width/2) / display_width_,
                                        (content_.top + content_.height/2) / display_height_});
    bool x_edge = point.x == 0 || point.x == 1, y_edge = point.y == 0 || point.y == 1;
    if (geometry_.rotation == InputRotation::clockwise_90 || geometry_.rotation == InputRotation::clockwise_270)
        std::swap(x_edge, y_edge);
    const auto raster = [](double normalized, double toward_center, std::uint32_t extent, bool edge) {
        double value = normalized * extent;
        if (edge || value == 0 || value == extent)
            value = std::nextafter(value, toward_center * extent);
        const auto integer = std::floor(value);
        if (integer < 0 || integer >= extent) throw std::invalid_argument("Accepted geometry could not rasterize inside input");
        return static_cast<std::uint32_t>(integer);
    };
    return {input, GeometryStatus::mapped,
            InputPixel{raster(input.x, center.x, geometry_.input_width, x_edge),
                       raster(input.y, center.y, geometry_.input_height, y_edge)}};
}
MappedPhigrosPoint PhigrosGeometryMap::map_reference_pixels(PosePoint point) const {
    finite(point);
    return map_normalized({point.x/phigros_reference_width, point.y/phigros_reference_height});
}
PixelViewport PhigrosGeometryMap::content_viewport() const noexcept { return content_; }
const PhigrosGeometry& PhigrosGeometryMap::geometry() const noexcept { return geometry_; }
} // namespace rhythm
