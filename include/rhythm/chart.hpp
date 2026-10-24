#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rhythm {
using SongNs = std::int64_t;
enum class NoteKind { tap = 1, drag = 2, hold = 3, flick = 4 };
enum class NoteSide { above, below };

struct ChartSource {
    std::string label;
    std::string chart_sha256; // Optional expected hash on input; verified actual hash on output.
    std::string song_id, difficulty, game_revision;
};
struct SourceLocation {
    std::string id; // Actual chart SHA256 + '#' + JSON pointer; never a native note ID.
    std::string json_pointer;
};
struct ChartEventTime {
    double start_tick = 0, end_tick = 0;
    SongNs start_ns = 0, end_ns = 0; // Song time, offset already applied exactly once.
};
struct NumericLineEvent {
    SourceLocation source;
    ChartEventTime time;
    double start = 0, end = 0; // Degrees for rotation; raw opacity for disappearance events.
};
struct MoveLineEvent {
    SourceLocation source;
    ChartEventTime time;
    double start_x = 0, end_x = 0, start_y = 0, end_y = 0; // Raw v3 line coordinates.
};
struct SpeedLineEvent {
    SourceLocation source;
    ChartEventTime time;
    double value = 0;
    std::optional<double> floor_position; // Absent in some native v3 charts; never invented.
};
struct SemanticNote {
    SourceLocation source;
    std::size_t source_index = 0; // Index within its original above/below array.
    NoteKind kind = NoteKind::tap;
    NoteSide side = NoteSide::above;
    double time_tick = 0, hold_ticks = 0;
    SongNs start_ns = 0, end_ns = 0; // Song time, includes chart offset; may precede music zero.
    double position_x = 0, speed = 0, floor_position = 0; // Preserve native units unchanged.
};
struct ChartLine {
    SourceLocation source;
    std::size_t source_index = 0;
    double bpm = 0;
    std::vector<SemanticNote> notes; // Original above-array order, then original below-array order.
    std::vector<MoveLineEvent> move_events;
    std::vector<NumericLineEvent> rotate_events, opacity_events;
    std::vector<SpeedLineEvent> speed_events;
};
struct SemanticChart {
    ChartSource source;
    int format_version = 3;
    double offset_seconds = 0;
    std::vector<ChartLine> lines; // Empty charts/lines are valid semantic input.
    std::size_t note_count = 0, event_count = 0;
};
struct ChartImportLimits {
    std::size_t max_json_bytes = 32 * 1024 * 1024;
    std::size_t max_json_depth = 32, max_lines = 1024;
    std::size_t max_notes = 200000, max_events = 500000;
    double max_abs_tick = 1000000000.0; // Retains native -999999 / 1e9 event sentinels.
    double max_abs_value = 1e12, max_bpm = 1000000;
    double max_abs_offset_seconds = 3600, max_abs_note_seconds = 86400;
};

// Positive offset delays chart events relative to music. Round once, nearest ns,
// with halfway values away from zero. Neither speed nor floor changes judgment time.
SongNs phigros_time_ns(double tick, double bpm, double offset_seconds);
std::string chart_text_sha256(std::string_view text);
SemanticChart parse_phigros(std::string_view json_text, ChartSource source,
                           const ChartImportLimits& limits = {});
} // namespace rhythm
