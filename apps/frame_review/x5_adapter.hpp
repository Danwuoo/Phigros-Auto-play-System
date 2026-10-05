#pragma once
#include "x5_rule.hpp"
#include "x3_features.hpp"
#include <deque>
#include <vector>

namespace pas::x5 {
using pas::review::json;
inline Point point(const json& j){const auto p=pas::x3::point(j);return {p.x,p.y};}
inline json vec(Point p){return json::array({p.x,p.y});}
inline std::uint64_t integer(const json& j){if(!j.is_number_unsigned()&&!j.is_number_integer())throw std::runtime_error("x5_integer_schema");if(j.is_number_integer()&&j.get<std::int64_t>()<0)throw std::runtime_error("x5_integer_schema");return j.get<std::uint64_t>();}
inline double number(const json& j){if(!j.is_number()||!std::isfinite(j.get<double>()))throw std::runtime_error("x5_float_schema");return j.get<double>();}
inline Pose pose(const json& j,std::int64_t now){const auto l=pas::x3::line(j);Pose p{{l.c.x,l.c.y},{l.u.x,l.u.y},l.length,number(j.at("confidence")),l.valid,l.observed==now};check_pose(p);return p;}
inline const json* diag(const json& r,const char* event,std::uint64_t id){const json* found=nullptr;for(const auto& d:r.at("diagnostics"))if(d.value("event","")==event&&d.contains("note_id")&&integer(d.at("note_id"))==id){if(found)throw std::runtime_error("x5_duplicate_diagnostic");found=&d;}return found;}
struct StoredNote {std::uint64_t id=0;Point p;bool unique=false,linked=false;std::int64_t prior_ns=0;Point prior;};
struct StoredFrame {std::int64_t ns=0;std::array<StoredNote,128> notes;std::size_t note_count=0;std::array<json,16> raw_lines;std::size_t line_count=0;};
struct Adapter {
    std::deque<StoredFrame> history;
    void expire(std::int64_t now){while(!history.empty()&&now-history.front().ns>90'000'000)history.pop_front();}
    StoredFrame frame(const json& row) {
        const auto& bank=row.at("candidate_bank");StoredFrame f;f.ns=static_cast<std::int64_t>(integer(bank.at("context").at("capture_ns")));
        if(bank.at("lines").size()>16||bank.at("candidates").size()>128||row.at("scene").at("targets").size()>128||row.at("diagnostics").size()>8192)throw std::runtime_error("x5_object_capacity");
        for(const auto& l:bank.at("lines")){integer(l.at("line_id"));pose(l,f.ns);for(std::size_t j=0;j<f.line_count;++j)if(l.at("line_id")==f.raw_lines[j].at("line_id"))throw std::runtime_error("x5_duplicate_line");f.raw_lines[f.line_count++]=l;}
        for(const auto& t:row.at("scene").at("targets")){StoredNote n;n.id=integer(t.at("note_id"));n.p={number(t.at("x")),number(t.at("y"))};const auto* ct=diag(row,"candidate_track",n.id);
            if(ct){n.unique=!ct->at("identity_ambiguous").get<bool>();n.linked=n.unique&&ct->at("assigned_prior_index").is_number_integer()&&ct->at("assigned_prior_index").get<int>()>=0;n.prior_ns=static_cast<std::int64_t>(integer(ct->at("prior_observed_ns")));n.prior={number(ct->at("prior_x")),number(ct->at("prior_y"))};}
            for(std::size_t j=0;j<f.note_count;++j)if(n.id==f.notes[j].id)throw std::runtime_error("x5_duplicate_note");f.notes[f.note_count++]=n;
        }return f;
    }
    Input input(const json& row,const json& target,const StoredFrame& current,std::string& history_reason) const {
        Input in;in.now=current.ns;const auto& bank=row.at("candidate_bank");in.frame_extent=number(bank.at("context").at("width"));
        const auto id=integer(target.at("note_id"));const auto* ct=diag(row,"candidate_track",id);const auto* selection=diag(row,"relation_selection",id);const json* candidate=nullptr;
        if(ct)for(const auto& c:bank.at("candidates"))if(c.at("candidate_id")==ct->at("candidate_id")){if(candidate)throw std::runtime_error("x5_duplicate_candidate");candidate=&c;}
        in.center={number(target.at("x")),number(target.at("y"))};in.width=number(target.at("width"));in.height=number(target.at("height"));
        in.hold_or_body=target.at("kind")=="hold"||target.value("held_body_evidence",false)||target.value("held_body_patch",false);
        in.strong_current=candidate&&candidate->at("quality")=="strong_current"&&bank.at("source_valid")==true&&bank.at("capacity_valid")==true;
        if(candidate){const auto& n=candidate->at("note");if(norm(sub(point(n.at("center")),in.center))<=1e-6){in.tangent=point(n.at("tangent"));in.appearance_known=true;}else in.strong_current=false;}
        if(ct)in.identity=ct->at("identity_ambiguous").get<bool>()?Identity::ambiguous:Identity::unique;
        if(selection&&selection->contains("confirmed_line_id"))in.confirmation=integer(selection->at("confirmed_line_id"))?Confirmation::confirmed:Confirmation::unconfirmed;
        in.line_count=current.line_count;for(std::size_t j=0;j<in.line_count;++j)in.lines[j]=pose(current.raw_lines[j],current.ns);
        const StoredNote* current_note=nullptr;for(std::size_t i=0;i<current.note_count;++i)if(current.notes[i].id==id)current_note=&current.notes[i];
        std::array<const StoredFrame*,6> frames{};std::array<const StoredNote*,6> notes{};std::size_t count=1;frames[0]=&current;notes[0]=current_note;history_reason="first_observation_or_no_stored_prior";
        while(count<6&&notes[count-1]&&notes[count-1]->linked){const auto& n=*notes[count-1];const StoredFrame* old=nullptr;const StoredNote* previous=nullptr;
            for(const auto& f:history)if(f.ns==n.prior_ns){old=&f;for(std::size_t i=0;i<f.note_count;++i)if(f.notes[i].id==id)previous=&f.notes[i];}
            if(!old||!previous){history_reason="exported_prior_outside_stored_6frame_90ms";break;}
            if(!previous->unique){history_reason="prior_identity_ambiguous";break;}
            if(norm(sub(previous->p,n.prior))>1e-6)throw std::runtime_error("x5_identity_geometry_binding");
            frames[count]=old;notes[count]=previous;++count;history_reason="explicit_unique_runtime_correspondence_physical_identity_unverified";
        }
        in.sample_count=count;
        for(std::size_t k=0;k<count;++k){const auto rev=count-1-k;auto& s=in.samples[k];s.ns=frames[rev]->ns;s.note=notes[rev]->p;
            for(std::size_t j=0;j<in.line_count;++j)for(std::size_t old=0;old<frames[rev]->line_count;++old)if(frames[rev]->raw_lines[old].at("line_id")==current.raw_lines[j].at("line_id"))s.lines[j]=pose(frames[rev]->raw_lines[old],s.ns);
        }return in;
    }
    void commit(StoredFrame f){if(!history.empty()&&f.ns<=history.back().ns)throw std::runtime_error("x5_frame_time_order");history.push_back(std::move(f));while(history.size()>5)history.pop_front();}
};
inline json feature_json(const Feature& f,const json& raw,const Input& in,std::size_t j){
    json latest={{"valid",false},{"reason","prior_geometry_or_correspondence_unknown"}};
    if(in.identity==Identity::ambiguous)latest["reason"]="identity_ambiguous";
    else if(in.sample_count>=2&&in.samples[in.sample_count-2].lines[j]){
        const auto& a=in.samples[in.sample_count-2];const auto& old=*a.lines[j];const auto l=pas::x3::line(raw);
        // pose() sets current only when raw observed_ns == this sample's ns.
        // Never relabel a stale prior pose with the note's measurement time.
        if(!old.current)latest["reason"]="line_not_current_valid_at_both_note_times";
        else {
            const pas::x3::Line prior{{old.center.x,old.center.y},{old.tangent.x,old.tangent.y},old.valid,false,old.length,a.ns,l.id};
            latest=pas::x3::measured_pair({in.center.x,in.center.y},l,{a.note.x,a.note.y},prior,a.ns,in.now);
        }
    }
    return {{"line_id",raw.at("line_id")},{"center",raw.at("center")},{"tangent",raw.at("tangent")},{"normal",vec(normal(in.lines[j].tangent))},{"length",raw.at("length")},{"confidence",raw.at("confidence")},{"observed_ns",raw.at("observed_ns")},{"current_valid",f.current},{"support_span_extent",f.support},{"distance_px",f.distance},{"along_px",f.along},{"note_appearance_dot",f.appearance_dot?json(*f.appearance_dot):json(nullptr)},{"measured_poses",f.measured_poses},{"aggregate_pair_valid",f.pair_valid},{"latest_measured_secant",latest},{"note_velocity_px_s",f.note_velocity?vec(*f.note_velocity):json(nullptr)},{"line_center_velocity_px_s",f.line_velocity?vec(*f.line_velocity):json(nullptr)},{"note_normal_rate",f.note_normal_rate?json(*f.note_normal_rate):json(nullptr)},{"note_tangent_rate",f.note_tangent_rate?json(*f.note_tangent_rate):json(nullptr)},{"line_center_normal_rate",f.line_normal_rate?json(*f.line_normal_rate):json(nullptr)},{"relative_signed_rate",f.relative_signed_rate?json(*f.relative_signed_rate):json(nullptr)},{"absolute_distance_rate",f.closing_rate?json(*f.closing_rate):json(nullptr)},{"minimum_note_normal_fraction",f.normal_fraction?json(*f.normal_fraction):json(nullptr)},{"maximum_line_contribution",f.line_contribution?json(*f.line_contribution):json(nullptr)},{"maximum_pair_angle_rad",f.max_angle?json(*f.max_angle):json(nullptr)},{"motion_model_valid",raw.at("motion_valid")},{"motion_model_velocity",raw.at("motion_valid")==true?raw.at("velocity"):json(nullptr)},{"motion_model_angular_velocity",raw.at("motion_valid")==true?raw.at("angular_velocity"):json(nullptr)},{"eligibility",f.eligible},{"unresolved",f.unresolved},{"reason",f.reason},{"judgment_role","unknown"}};
}
}
