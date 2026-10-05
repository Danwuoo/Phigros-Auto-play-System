#pragma once
#include "pas/game.hpp"
#include "x10b_claim.hpp"
#include <nlohmann/json.hpp>
#include <span>
#include <stdexcept>
#include <set>
namespace pas::x10c {
using json=nlohmann::json;
inline thread_local bool enabled=false;
inline thread_local json records=json::array();
inline thread_local std::size_t bytes=0,peak=0;
inline void validate_windows(const json& m) {
    if(m.at("schema")!=1||!m.at("schema").is_number_integer())throw std::runtime_error("x10c_schema");
    const auto& windows=m.at("windows");
    if(!windows.is_array()||windows.empty()||windows.size()>8)throw std::runtime_error("x10c_window_capacity");
    std::set<std::string> ids;
    for(const auto& w:windows){
        if(!w.at("id").is_string()||w.at("id").get<std::string>().empty()||!ids.insert(w.at("id").get<std::string>()).second)throw std::runtime_error("x10c_window_id");
        for(const auto* k:{"first","last","anchor"})if(!w.at(k).is_number_integer()||w.at(k).get<long long>()<0||w.at(k).get<long long>()>=36000)throw std::runtime_error("x10c_window_ordinal");
        const auto a=w.at("first").get<int>(),b=w.at("last").get<int>(),c=w.at("anchor").get<int>();
        if(b<a||c<a||c>b||b-a>=120)throw std::runtime_error("x10c_window_range");
    }
}
inline json note_json(const NoteCandidate& n) {
    return {{"kind",static_cast<int>(n.kind)},{"center",{n.center.x,n.center.y}},
        {"width",n.width},{"height",n.height},{"tangent",{n.tangent.x,n.tangent.y}},
        {"outline",n.outline_evidence},{"recent_identity",n.recent_identity},{"rails",n.rails_geometry},
        {"direct",n.direct_rails_evidence},{"head_on_line",n.head_on_line},{"held_body",n.held_body_evidence},
        {"held_patch",n.held_body_patch},{"tail",n.tail?json{n.tail->x,n.tail->y}:json(nullptr)}};
}
inline json history_json(const GameTrackHistory& t) {
    if(t.points.size()>6)throw std::runtime_error("x10c_history_capacity");
    json points=json::array();
    for(const auto& p:t.points)points.push_back({{"time_ns",p.t},{"position",{p.p.x,p.p.y}},
        {"rails",p.rails},{"front_is_touch",p.front_is_touch},{"line_id",p.line.track_id},
        {"line_center",{p.line.center.x,p.line.center.y}},{"line_tangent",{p.line.tangent.x,p.line.tangent.y}}});
    return {{"id",t.id},{"revision",t.revision},{"observed_ns",t.observed},{"last",{t.last.x,t.last.y}},
        {"points",points},{"appearance",note_json(t.appearance)},
        {"rail_anchor",t.rail_anchor?note_json(*t.rail_anchor):json(nullptr)},
        {"rail_observed_ns",t.rail_observed},{"rail_frame",t.rail_frame},{"point_bucket_ns",t.point_bucket_ns}};
}
inline json histories(std::span<const GameTrackHistory> tracks) {
    if(tracks.size()>128)throw std::runtime_error("x10c_track_capacity");
    json result=json::array();for(const auto& t:tracks)result.push_back(history_json(t));return result;
}
inline void emit(json j) {
    if(!enabled)return;
    const auto size=j.dump().size()+1;
    if(records.size()>=256||bytes+size>2*1024*1024)throw std::runtime_error("x10c_diagnostic_capacity");
    bytes+=size;peak=std::max(peak,bytes);records.push_back(std::move(j));
}
inline json drain(){auto out=std::move(records);records=json::array();bytes=0;return out;}
inline void fallback(const Frame& f,const NoteCandidate& note,const LineCandidate& line,
                     std::span<const NoteCandidate> claims,const GameTrackHistory& track,
                     std::span<const NoteCandidate> raw,bool approaching,bool would_skip,bool applied,bool detailed) {
    if(!enabled||(!would_skip&&!detailed))return;
    if(claims.size()>128||raw.size()>128)throw std::runtime_error("x10c_candidate_capacity");
    json cs=json::array(),rs=json::array();for(const auto& c:claims)cs.push_back(note_json(c));
    if(detailed||would_skip)for(const auto& r:raw)rs.push_back(note_json(r));
    emit({{"event","x10c_fallback_claim"},{"source_frame",f.sequence},{"capture_complete_ns",f.capture_complete_ns},
        {"pixels_ready_ns",f.pixels_ready_ns},{"candidate",note_json(note)},{"line_id",line.track_id},
        {"approaching",approaching},{"would_suppress",would_skip},{"applied",applied},
        {"history",history_json(track)},{"current_claims",cs},{"raw_candidates_before_fallback_erase",rs}});
}
}
