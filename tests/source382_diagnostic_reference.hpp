#pragma once
#include "pas/game_tracking.hpp"
namespace pas::diagnostic_reference {
using nlohmann::json;
inline json vec(Vec2 p){return json::array({p.x,p.y});}
inline json note_json(const NoteCandidate& n){return {{"kind",name(n.kind)},{"center",vec(n.center)},{"tangent",vec(n.tangent)},
 {"width",n.width},{"height",n.height},{"confidence_diagnostic_only",n.confidence},{"tail",n.tail?vec(*n.tail):json(nullptr)},
 {"outline",n.outline_evidence},{"rails",n.rails_geometry},{"head_on_line",n.head_on_line},{"held_body_evidence",n.held_body_evidence},{"held_body_patch",n.held_body_patch},{"direct_rails",n.direct_rails_evidence},{"extractor_hint_id",n.recent_identity}};}

inline nlohmann::json decision_json(const DecisionSnapshot& s) {
    using nlohmann::json;
    json lines=json::array(),targets=json::array();
    for(const auto& l:s.lines) lines.push_back({{"x",l.center.x},{"y",l.center.y},
        {"ux",l.tangent.x},{"uy",l.tangent.y},{"length",l.length},{"confidence",l.confidence},
        {"line_id",l.track_id},{"observed_ns",l.observed_ns},{"association_valid",l.association_valid},
        {"vx",l.velocity.x},{"vy",l.velocity.y},{"angular_velocity",l.angular_velocity},
        {"motion_valid",l.motion_valid},{"motion_samples",l.motion_samples},{"motion_span_ns",l.motion_span_ns},
        {"motion_residual_px",l.motion_residual},{"angular_residual_rad",l.angular_residual}});
    for(const auto& t:s.targets) targets.push_back({{"note_id",t.note_id},{"revision",t.revision},
        {"kind",name(t.note.kind)},{"x",t.note.center.x},{"y",t.note.center.y},
        {"width",t.note.width},{"height",t.note.height},{"evidence_ns",t.evidence_ns},
        {"expires_ns",t.expires_ns},{"crossing_ns",t.crossing_ns?json(*t.crossing_ns):json(nullptr)},
        {"uncertainty_ns",t.uncertainty_ns},{"hit_x",t.hit.x},{"hit_y",t.hit.y},
        {"line_id",t.line_id},{"hit_vx",t.hit_velocity.x},{"hit_vy",t.hit_velocity.y},
        {"current_contact",t.current_contact==GameTarget::CurrentContact::hold_front_rails?"hold_front_rails":
            t.current_contact==GameTarget::CurrentContact::hold_tail_only?"hold_tail_only":"none"},
        {"tail_terminal_proof",t.tail_terminal_proof?json{{"frame",t.tail_terminal_proof->context.frame},
            {"capture_ns",t.tail_terminal_proof->context.capture_ns},{"epoch",t.tail_terminal_proof->context.epoch},
            {"generation",t.tail_terminal_proof->context.generation},{"geometry",t.tail_terminal_proof->context.geometry},
            {"note_id",t.tail_terminal_proof->note_id},{"line_id",t.tail_terminal_proof->line_id},
            {"candidate_id",t.tail_terminal_proof->candidate_id},{"x",t.tail_terminal_proof->boundary.x},
            {"y",t.tail_terminal_proof->boundary.y},{"pixels_ready_ns",t.tail_terminal_proof->pixels_ready_ns},
            {"left_rail_rows",t.tail_terminal_proof->left_rail_rows},{"right_rail_rows",t.tail_terminal_proof->right_rail_rows},
            {"terminal_clusters",t.tail_terminal_proof->terminal_clusters},{"current_line_count",t.tail_terminal_proof->current_line_count},
            {"probes",t.tail_terminal_proof->probes},{"line_aware",t.tail_terminal_proof->line_aware}}:json(nullptr)},
        {"line_projection_only",t.line_projection_only},
        {"last_line_observed_ns",t.last_line_observed_ns},
        {"projected_line_along_px",t.projected_line_along_px},
        {"projected_line_half_length_px",t.projected_line_half_length_px},
        {"tail_crossing_ns",t.tail_crossing_ns?json(*t.tail_crossing_ns):json(nullptr)},
        {"tail_x",t.note.tail?json(t.note.tail->x):json(nullptr)},{"tail_y",t.note.tail?json(t.note.tail->y):json(nullptr)},
        {"observation_basis",t.note.direct_rails_evidence?"hold_current_parallel_rails_and_fill":
            t.note.outline_evidence?"hold_parallel_rails_and_recent_identity":"color_core"},
        {"rails_geometry",t.note.rails_geometry},{"head_on_line",t.note.head_on_line},
        {"held_body_evidence",t.note.held_body_evidence},
        {"held_body_patch",t.note.held_body_patch},
        {"relative_distance_px",t.distance},{"relative_velocity_px_s",t.velocity},
        {"residual_px",t.residual},{"prediction_error_px",t.prediction_error_px},
        {"fit_residual_limit_px",t.fit_residual_limit_px},{"samples",t.samples},
        {"history_span_ns",t.history_span_ns},{"reason",t.reason}});
    json result={{"event","game_decision"},{"decision_schema",4},{"sequence",s.sequence},
        {"note_anchor_semantics","tap_flick_core_center_hold_leading_edge"},
        {"epoch",s.context.epoch},{"generation",s.context.generation},{"geometry_version",s.context.geometry},
        {"frame_sequence",s.context.frame},{"capture_complete_ns",s.context.capture_ns},
        {"recognition_start_ns",s.recognition_start_ns},{"recognition_end_ns",s.recognition_end_ns},
        {"ui",name(s.ui)},{"ui_basis",s.ui_basis},{"playing_gate",s.playing_gate},
        {"combo_digit_glyphs",s.combo_digit_glyphs},{"combo_glyph_semantics","diagnostic_shapes_only_not_ocr_or_judgment"},
        {"play_button",s.play_button?json{{"x",s.play_button->x},{"y",s.play_button->y}}:json(nullptr)},
        {"capacity_valid",s.capacity_valid},
        {"source_absolute_age",nullptr},{"per_note_feedback","unknown"}};
    // Large diagnostic arrays already own every scalar. Transfer them once;
    // initializer-list insertion otherwise copies their entire JSON trees.
    result["lines"]=std::move(lines);result["targets"]=std::move(targets);
    if(s.compute_timing_host_qpc) {
        result["compute_clock_domain"]="host_qpc_ns";
        result["components_compute_ns"]=s.components_compute_ns;
        result["base_scene_compute_ns"]=s.base_scene_compute_ns;
        result["combo_glyph_compute_ns"]=s.combo_glyph_compute_ns;
        result["line_scan_compute_ns"]=s.line_scan_compute_ns;
        result["component_line_decode_compute_ns"]=s.component_line_decode_compute_ns;
        result["note_decode_compute_ns"]=s.note_decode_compute_ns;
        result["line_tracking_compute_ns"]=s.line_tracking_compute_ns;
        result["held_recovery_compute_ns"]=s.held_recovery_compute_ns;
        result["tracking_compute_ns"]=s.tracking_compute_ns;
    }
    return result;
}


inline json candidate_batch_json(const CandidateBatch& b){json cs=json::array(),ls=json::array();
 for(const auto& c:b.candidates)cs.push_back({{"candidate_id",c.candidate_id},{"note",note_json(c.note)},
  {"shortened_hold",c.shortened_hold?note_json(*c.shortened_hold):json(nullptr)},
  {"quality",c.quality==ObservationQuality::strong_current?"strong_current":c.quality==ObservationQuality::weak_current?"weak_current":"rejected"},
  {"head_visible",c.head_visible},{"body_visible",c.body_visible},{"left_rail",c.left_rail},{"right_rail",c.right_rail},
  {"action_support",c.action_support},{"origin",c.origin},{"hint_source_frame",c.hint_source_frame},{"hint_age_ns",c.hint_age_ns}});
 for(const auto& l:b.lines)ls.push_back({{"center",vec(l.center)},{"tangent",vec(l.tangent)},{"length",l.length},{"thickness",l.thickness},{"confidence",l.confidence},
   {"line_id",l.track_id},{"observed_ns",l.observed_ns},{"velocity",vec(l.velocity)},
   {"angular_velocity",l.angular_velocity},{"association_valid",l.association_valid},
   {"motion_valid",l.motion_valid},{"motion_samples",l.motion_samples},{"motion_span_ns",l.motion_span_ns},
   {"motion_residual_px",l.motion_residual},{"angular_residual_rad",l.angular_residual}});
 json result={{"schema",1},{"extractor_version",b.extractor_version},{"quality_version",b.quality_version},{"history_source",b.history_source},
 {"context",{{"epoch",b.context.epoch},{"generation",b.context.generation},{"geometry",b.context.geometry},{"frame",b.context.frame},{"capture_ns",b.context.capture_ns},{"width",b.context.width},{"height",b.context.height},{"rotation",b.context.rotation}}},
 {"extraction_start_ns",b.extraction_start_ns},{"extraction_end_ns",b.extraction_end_ns},{"ui",name(b.ui)},
 {"playing_gate",b.playing_gate},{"capacity_valid",b.capacity_valid},{"source_valid",b.source_valid}};
 result["lines"]=std::move(ls);result["candidates"]=std::move(cs);return result;
}


}
