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
    double start_x = 0, end_x = 0, start_y = 0, end_y = 0; // Normalized native line coordinates.
    // v1 encodes x*1000+y on an 880 by 520 canvas. Keep the declaration's
    // original packed values even when the evaluation track is subdivided.
    std::optional<double> packed_start, packed_end;
};
struct SpeedLineEvent {
    SourceLocation source;
    ChartEventTime time;
    double value = 0;
    std::optional<double> floor_position; // Absent in some native v3 charts; never invented.
};
enum class PhigrosImportPolicy { strict_v3, source_priority_constant_empty_candidate_v1,
    native_v1_source_priority_constant_empty_candidate_v1,
    native_v1_source_priority_empty_interval_candidate_v1 };
struct ImportOptions {
    PhigrosImportPolicy policy = PhigrosImportPolicy::strict_v3;
    std::size_t max_anomalies = 4096, max_evaluated_pose_events = 500000;
};
// Original source-order event declarations, including empty/reversed records.
// Populated only by the explicit candidate importer. Existing line event arrays
// then describe its evaluated, disjoint half-open segments; these declarations
// remain unchanged and keep their original chart SHA + JSON pointers.
struct NativeLineEventDeclarations {
    std::vector<MoveLineEvent> move_events;
    std::vector<NumericLineEvent> rotate_events, opacity_events;
    std::vector<SpeedLineEvent> speed_events;
};
struct ChartImportAnomaly {
    SourceLocation source;
    std::string code, reason, event_track;
    std::size_t line_index = 0, declaration_index = 0;
    ChartEventTime declared_time;
    // All declared JSON fields/values, canonical serialization for diagnostics.
    // Original byte spelling/whitespace is identified by chart.source hash;
    // typed native_event_declarations retain the original parsed values.
    std::string original_declaration_json;
    bool declaration_evaluated = false;
    std::vector<ChartEventTime> evaluated_segments;
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
    std::optional<NativeLineEventDeclarations> native_event_declarations;
};
struct BlockAreaPoint { double x = 0, y = 0; };
struct BlockAreaMove {
    SongNs time_ns = 0;
    BlockAreaPoint end_position;
    int ease_x = 0, ease_y = 0;
};
struct BlockAreaRotation {
    SongNs time_ns = 0;
    BlockAreaPoint anchor;
    double degrees = 0;
    int ease = 0;
};
struct BlockAreaScale {
    SongNs time_ns = 0;
    BlockAreaPoint anchor, scale;
    int ease_x = 0, ease_y = 0;
};
struct ChartBlockArea {
    SourceLocation source;
    BlockAreaPoint top_right, bottom_left;
    // Song time, including chart offset. Touch blocking uses [enable_ns, disable_ns).
    SongNs appear_ns = 0, enable_ns = 0, disable_ns = 0, disappear_ns = 0;
    bool subtract = false;
    std::vector<BlockAreaMove> moves;
    std::vector<BlockAreaRotation> rotations;
    std::vector<BlockAreaScale> scales;
};
struct SemanticChart {
    ChartSource source;
    int format_version = 3;
    // format_version describes the normalized pose IR. Source bytes, source
    // format and native packed declarations remain independently inspectable.
    int source_format_version = 3;
    double offset_seconds = 0;
    std::vector<ChartLine> lines; // Empty charts/lines are valid semantic input.
    std::size_t note_count = 0, event_count = 0;
    // event_count always counts original declarations, including ignored empty
    // records. Candidate interpretation is not a game-engine qualification.
    std::string import_policy_id = "phigros-v3-strict-v1";
    bool import_interpretation_unqualified = false;
    std::vector<ChartImportAnomaly> import_anomalies;
    std::size_t evaluated_pose_event_count = 0;
    std::vector<ChartBlockArea> block_areas;
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
std::string_view phigros_import_policy_id(PhigrosImportPolicy policy);
SemanticChart parse_phigros(std::string_view json_text, ChartSource source,
                           const ChartImportLimits& limits = {}, const ImportOptions& options = {});
} // namespace rhythm
