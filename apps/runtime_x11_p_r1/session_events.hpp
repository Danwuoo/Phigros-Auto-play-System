#pragma once
#include "pas/game.hpp"
namespace pas {
inline nlohmann::json release_json(const ReleaseReport& r,const std::string& reason) {
    return {{"event","game_release_report"},{"reason",reason},{"requested_ids",r.requested_ids},
        {"failed_ids",r.failed_ids},{"unknown_ids",r.unknown_ids},{"start_ns",r.start_ns},{"return_ns",r.return_ns},
        {"effect_verified",false}};
}
inline nlohmann::json receipt_json(const TouchReceipt& r,bool real) {
    return {{"event","game_touch_receipt"},{"intent_id",r.command.intent_id},{"contact_id",r.command.contact_id},
        {"phase",static_cast<int>(r.command.phase)},{"scheduled_ns",r.command.scheduled_ns},
        {"source_frame",r.command.source_frame_sequence},{"injection_start_ns",r.injection_start_ns},
        {"injection_return_ns",r.injection_return_ns},{"success",r.success},{"reason",r.reason},{"real_input",real}};
}
inline nlohmann::json plan_json(const ContactPlan& plan,Nanoseconds accepted,bool real) {
    using nlohmann::json;
    json steps=json::array();for(const auto& st:plan.steps)steps.push_back({{"phase",static_cast<int>(st.phase)},{"x",st.x},{"y",st.y},{"due_ns",st.due_ns}});
    return {{"event","game_plan_accepted"},{"epoch",plan.epoch},{"intent_id",plan.intent_id},{"note_id",plan.note_id},
        {"revision",plan.revision},{"prefix_offset",plan.prefix_offset},{"evidence_ns",plan.evidence_ns},
        {"valid_until_ns",plan.valid_until_ns},{"source_frame",plan.source_frame_sequence},{"accepted_ns",accepted},
        {"predicted_down_ns",plan.predicted_down_ns?json(*plan.predicted_down_ns):json(nullptr)},
        {"steps",steps},{"basis",plan.basis},{"real_input",real}};
}
}
