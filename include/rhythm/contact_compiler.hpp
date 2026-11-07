#pragma once

#include "rhythm/chart_memory.hpp"
#include "rhythm/phigros_pose.hpp"
#include <optional>
#include <string>
#include <vector>

namespace rhythm {
enum class CompileSeverity { warning, error };
enum class OffscreenPolicy { reject, bounded_normal_candidate, bounded_normal_intersection_candidate_v1,
    bounded_judgment_rectangle_candidate_v1 };
inline const char* offscreen_policy_name(OffscreenPolicy policy) {
    switch(policy) {
    case OffscreenPolicy::reject:return "reject";
    case OffscreenPolicy::bounded_normal_candidate:return "bounded-normal-candidate";
    case OffscreenPolicy::bounded_normal_intersection_candidate_v1:return "bounded-normal-intersection-candidate-v1";
    case OffscreenPolicy::bounded_judgment_rectangle_candidate_v1:return "bounded-judgment-rectangle-candidate-v1";
    }
    return "invalid";
}
enum class FlickPathPolicy { dynamic_line_v1, judgment_anchored_straight_v1, judgment_anchored_screen_fit_v1,
    judgment_anchored_diagonal_snap_v1, judgment_anchored_diagonal_sampled_v2 };
enum class HoldTailPolicy { full_pose_v1, phira_tail_grace_candidate_v1 };
inline const char* hold_tail_policy_name(HoldTailPolicy policy) {
    switch(policy) {
    case HoldTailPolicy::full_pose_v1:return "full-pose-v1";
    case HoldTailPolicy::phira_tail_grace_candidate_v1:return "phira-tail-grace-candidate-v1";
    }
    return "invalid";
}
inline const char* flick_path_policy_name(FlickPathPolicy policy) {
    switch(policy) {
    case FlickPathPolicy::dynamic_line_v1:return "dynamic-line-v1";
    case FlickPathPolicy::judgment_anchored_straight_v1:return "judgment-anchored-straight-v1";
    case FlickPathPolicy::judgment_anchored_screen_fit_v1:return "judgment-anchored-screen-fit-v1";
    case FlickPathPolicy::judgment_anchored_diagonal_snap_v1:return "judgment-anchored-diagonal-snap-v1";
    case FlickPathPolicy::judgment_anchored_diagonal_sampled_v2:return "judgment-anchored-diagonal-sampled-v2";
    }
    return "invalid";
}
struct ContactCompileOptions {
    int max_contacts = 2;
    SongNs tap_duration_ns = 16'000'000, drag_duration_ns = 16'000'000;
    SongNs flick_lead_ns = 16'000'000, flick_travel_ns = 32'000'000,
           flick_release_ns = 4'000'000;
    double flick_distance_reference_pixels = 48;
    // Straight candidates freeze a judgment anchor and certify a complete stroke.
    // Diagonal snap holds that anchor through judgment, then commands exactly
    // two noncollinear MOVE vertices; its separate discrete certificate makes
    // no claim about interpolation between those commands. No stroke is clipped.
    // The default preserves archived paths, sampling and fingerprints.
    FlickPathPolicy flick_path_policy = FlickPathPolicy::dynamic_line_v1;
    // Explicit community-simulator candidate: after a successful Hold start,
    // stop tracking the moving line in its final 220ms. The pointer still stays
    // down until the original native end time. No game qualification is implied.
    HoldTailPolicy hold_tail_policy = HoldTailPolicy::full_pose_v1;
    SongNs max_prejudge_ns = 0; // Explicit Drag/Flick candidate, 0..30ms.
    bool advance_above_drags = false; // Prefer the early endpoint; separate opt-in.
    SongNs max_hold_reacquire_gap_ns = 0; // Rectangular Hold candidate, 0..20ms.
    // Includes <sqrt(2) pixel rasterization error. Applies to zero-order held
    // coordinates, since PAS dispatches discrete MOVE events, not interpolation.
    // Diagonal-snap Flicks instead use their explicit discrete path certificate.
    double max_path_error_pixels = 4;
    std::size_t max_contacts_in_plan = 200000, max_steps_per_contact = 20000,
                max_steps_in_plan = 2000000, max_diagnostics = 2048;
    SongNs interference_window_ns = 30'000'000;
    double interference_radius_pixels = 48;
    OffscreenPolicy offscreen_policy = OffscreenPolicy::reject;
    double max_normal_projection_reference_pixels = 0;
    // Only the explicit judgment-rectangle policy may use a nonzero tolerance.
    // Maximum151.2 reference px is a Phira community judgment-model candidate,
    // not an official Phigros hit-region measurement.
    double max_tangent_error_reference_pixels = 0;
    // Optional candidate geometry for avoiding a device's system-gesture edges.
    // Dynamic normal intersections and judgment rectangles support an inset.
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
    // Explicit timing-only input-handoff experiment, 0 (archived default) or
    // 32ms. A discrete Flick sharing its ORIGINAL native judgment with a
    // Tap/Hold/Drag moves as one complete contact; source/geometry stay fixed.
    // This is not a measured game judgment rule or qualification.
    SongNs coincident_flick_delay_ns = 0;
    // Independent opt-in for the existing bounded pre-judge endpoint. False
    // preserves archived above-only Drag dispatch and fingerprints.
    bool advance_below_drags = false;
    // Optional closed subset for the below-Drag early preference. Empty keeps
    // the archived all-below option; nonempty requires advance_below_drags.
    // The compiler validates exact original source identities and stores a
    // sorted set (duplicates are errors), never a best-effort name filter.
    std::vector<std::string> below_drag_source_ids;
    // Optional closed subset for the existing above-Drag early preference.
    // Empty preserves all-above behavior and archived fingerprints. Nonempty
    // requires advance_above_drags and exact original above-Drag identities.
    std::vector<std::string> above_drag_source_ids;
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
    double tangent_correction_lower_reference_pixels=0,tangent_correction_upper_reference_pixels=0;
    bool judgment_domain_gap=false;
};
struct FlickDiscreteMiddleVertex {
    PosePoint reference;
    SongNs song_ns=0;
};
// An explicitly discrete Flick path: DOWN at the held judgment anchor, two
// MOVE vertices for v1 or three for v2, then UP. No continuous target or zero-order-held
// approximation error is claimed for the unobserved movement between vertices.
// All vertices and the conceptual polyline lie in the declared rectangle.
struct FlickDiscretePathCertificate {
    std::string model_id;
    PosePoint bend_reference,end_reference;
    SongNs bend_ns=0,end_ns=0;
    double endpoint_distance_reference_pixels=0,polyline_length_reference_pixels=0;
    double max_jump_input_pixels=0;
    std::size_t move_count=0;
    // Appended: absence preserves every archived v1 field and fingerprint.
    // V2 bend/end remain the first/last MOVE; this is its second MOVE.
    std::optional<FlickDiscreteMiddleVertex> middle=std::nullopt;
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
    double tangent_projection_min_reference_pixels=0,tangent_projection_max_reference_pixels=0;
    double gap_tangent_projection_min_reference_pixels=0,gap_tangent_projection_max_reference_pixels=0;
    std::vector<NormalProjectionSpan> normal_projection_spans;
    SongNs hold_tracking_end_ns = -1; // Candidate only; native end/UP is unchanged.
    SongNs dispatch_judgment_ns = -1; // Actual Drag/Flick anchor; judgment_ns remains native.
    std::optional<PosePoint> flick_anchor_reference, flick_direction_reference;
    std::optional<FlickDiscretePathCertificate> flick_discrete_path;
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
    int source_format_version = 3;
    bool import_interpretation_unqualified = false;
    std::vector<std::string> import_anomaly_source_ids;
    std::string judgment_policy_provenance; // Empty for archived/default policies.
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
