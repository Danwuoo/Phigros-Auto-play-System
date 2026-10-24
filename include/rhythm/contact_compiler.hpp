#pragma once

#include "rhythm/chart_memory.hpp"
#include "rhythm/phigros_pose.hpp"
#include <optional>
#include <string>
#include <vector>

namespace rhythm {
enum class CompileSeverity { warning, error };
enum class OffscreenPolicy { reject, bounded_normal_candidate, bounded_normal_intersection_candidate_v1 };
struct ContactCompileOptions {
    int max_contacts = 2;
    SongNs tap_duration_ns = 16'000'000, drag_duration_ns = 16'000'000;
    SongNs flick_lead_ns = 16'000'000, flick_travel_ns = 32'000'000,
           flick_release_ns = 4'000'000;
    double flick_distance_reference_pixels = 48;
    // Includes <sqrt(2) pixel rasterization error. Applies to zero-order held
    // coordinates, since PAS dispatches discrete MOVE events, not interpolation.
    double max_path_error_pixels = 4;
    std::size_t max_contacts_in_plan = 200000, max_steps_per_contact = 20000,
                max_steps_in_plan = 2000000, max_diagnostics = 2048;
    SongNs interference_window_ns = 30'000'000;
    double interference_radius_pixels = 48;
    OffscreenPolicy offscreen_policy = OffscreenPolicy::reject;
    double max_normal_projection_reference_pixels = 0;
    // Optional candidate geometry for avoiding a device's system-gesture edges.
    // Only bounded_normal_intersection_candidate_v1 supports a positive inset.
    // Positive I uses reference slabs [I,1280-1-I] and [I,720-1-I], including
    // otherwise onscreen targets, the full continuous path and inward raster
    // rounding. I=0 preserves the original [0,1280] / [0,720] canvas exactly.
    // This does not qualify game judgment acceptance or a device safe area.
    double normal_projection_inset_reference_pixels = 0; // Finite, 0..100.
    // Opt-in analytic-projected-derivative-v1: interval bounds on the actual
    // normal-intersection path, including active canvas/inset edges. The
    // zero-order held error budget and native critical keys remain unchanged.
    // False preserves archived triangle bounds, sampling and fingerprints.
    bool tight_projection_speed_bounds = false;
    // Required for candidate policy; a pending calibration identity, NOT proof
    // that an off-line target is accepted by the actual game.
    std::string calibration_id;
};
struct CompileDiagnostic {
    CompileSeverity severity = CompileSeverity::error;
    std::string code, message, source_note_id, related_note_id;
    std::size_t line_index = 0, note_index = 0;
    SongNs song_ns = 0;
    std::optional<PosePoint> reference_point;
};
// Conservative continuous-segment certificate, not an official hit region.
// Edge mask of the selected canvas/inset rectangle: left=1, right=2,
// top=4, bottom=8, unprojected=16. Positive insets do not certify the
// corresponding physical device system-gesture region or game acceptance.
struct NormalProjectionSpan {
    SongNs begin_ns=0,end_ns=0;
    double correction_lower_reference_pixels=0,correction_upper_reference_pixels=0;
    unsigned possible_edge_mask=16;
    double speed_bound_input_pixels_per_ns=0;
};
struct NoteContactOwnership {
    std::string source_note_id, source_json_pointer;
    std::size_t line_index = 0, note_index = 0;
    NoteKind kind = NoteKind::tap;
    NoteSide side = NoteSide::above;
    std::uint64_t contact_id = 0;
    SongNs judgment_ns = 0;
    double normal_projection_reference_pixels = 0; // Judgment-time value; fixed only for old policy.
    bool time_varying_normal_projection=false;
    double projection_min_reference_pixels=0,projection_max_reference_pixels=0;
    std::vector<NormalProjectionSpan> normal_projection_spans;
};
struct CompiledSongEvent {
    SongNs song_ns = 0;
    std::uint64_t contact_id = 0;
    Phase phase = Phase::down;
    Point point;
};
struct ContactCompileMetadata {
    std::string compiler_version = "phigros-contact-v2";
    // Derived automatically from the parsed chart. Non-strict interpretation
    // and original anomalous source IDs are fingerprinted independently of
    // immutable source bytes and the full generated contact path.
    std::string import_policy_id = "phigros-v3-strict-v1";
    bool import_interpretation_unqualified = false;
    std::vector<std::string> import_anomaly_source_ids;
    std::string cache_sha256;
    PhigrosGeometry geometry;
    ContactCompileOptions options;
    bool requires_calibration = false;
    std::string projection_policy_version = "none";
    // All strategies are offline candidates, not verified game judgment rules.
    std::string strategy = "exclusive-note-ownership; discrete-held-error-bound; raster-pixel-normalized";
};
struct CompiledContactPlan {
    Memory memory;
    std::vector<CompiledSongEvent> events; // song_ns, UP then MOVE then DOWN, id.
    std::vector<NoteContactOwnership> ownership;
    int peak_contacts = 0;
    std::size_t total_steps = 0;
    ContactCompileMetadata metadata;
};
struct ContactCompileResult {
    std::optional<CompiledContactPlan> plan; // absent if ANY note/config is rejected.
    std::vector<CompileDiagnostic> diagnostics;
    std::optional<int> required_peak_contacts; // Set only after every note compiled.
};

// Pure offline compilation; no devices, clocks, live evidence, or auto-arming.
// Each note owns one closed contact; no multi-note sharing or pointer reassignment.
// Source identity must exactly match the imported chart. Empty charts are rejected.
ContactCompileResult compile_phigros_contacts(const SemanticChart& chart,
    Identity identity, PhigrosGeometry geometry, ContactCompileOptions options = {});
// Canonical v2 digest of every semantic plan field, including generated events,
// ownership, metadata, geometry/options, and projection certificates. Excludes
// only metadata.cache_sha256 itself. A consistency fingerprint, not a signature.
std::string compiled_contact_fingerprint(const CompiledContactPlan& plan);
} // namespace rhythm
