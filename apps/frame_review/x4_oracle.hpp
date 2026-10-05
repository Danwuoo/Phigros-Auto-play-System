#pragma once
// Included ONLY by the X4 exported main50 core. No runtime target sees this hook.
#include "pas/game.hpp"
#include <algorithm>
#include <cmath>
#include <map>

namespace pas::x4 {
using json=nlohmann::json;
inline thread_local bool enabled=false;
inline thread_local std::map<std::size_t,json> packets;
inline thread_local const json* packet=nullptr;
inline thread_local std::size_t ordinal=0;
inline thread_local json choices=json::array();
inline thread_local std::size_t applied_count=0,changed_count=0;
inline json note_geometry(const NoteCandidate& n) {
    return {{"x",n.center.x},{"y",n.center.y},{"width",n.width},{"height",n.height},
        {"ux",n.tangent.x},{"uy",n.tangent.y},{"kind",name(n.kind)}};
}
inline json line_geometry(const LineCandidate& l) {
    return {{"x",l.center.x},{"y",l.center.y},{"length",l.length},{"ux",l.tangent.x},{"uy",l.tangent.y}};
}
inline bool near(double a,double b,double tolerance=1e-6) {return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tolerance;}
inline bool geometry_matches(const json& actual,const json& binding,bool note) {
    for(const auto key:{"x","y","ux","uy"})if(!binding.contains(key)||!binding.at(key).is_number()||!near(actual.at(key).get<double>(),binding.at(key).get<double>()))return false;
    for(const auto key:note?std::vector<const char*>{"width","height"}:std::vector<const char*>{"length"})
        if(!binding.contains(key)||!binding.at(key).is_number()||!near(actual.at(key).get<double>(),binding.at(key).get<double>()))return false;
    return !note||binding.value("kind","")==actual.at("kind").get<std::string>();
}
inline void reset() {enabled=false;packet=nullptr;packets.clear();choices=json::array();applied_count=changed_count=0;}
inline void begin_frame(std::size_t n,std::uint64_t source,const std::string& png_sha) {
    ordinal=n;choices=json::array();packet=nullptr;
    if(!enabled||n<6158||n>6220)return;
    const auto it=packets.find(n);if(it==packets.end())return;
    if(it->second.at("source_frame")!=source||it->second.at("png_sha256")!=png_sha)throw std::runtime_error("x4_packet_frame_SHA_binding");
    packet=&it->second;
}
inline json choose(const NoteCandidate& n,const std::vector<NoteCandidate>& notes,
    const DecisionSnapshot& s,std::uint64_t confirmed,const LineCandidate*& selected,
    double best,double second,std::uint64_t note_id) {
    if(!enabled||!packet||!packet->contains("core")||packet->at("core").is_null()||
       n.kind!=NoteKind::drag||!geometry_matches(note_geometry(n),packet->at("core"),true))return nullptr;
    json row={{"ordinal",ordinal},{"source_frame",s.context.frame},{"png_sha256",packet->at("png_sha256")},
        {"evidence_ns",s.context.capture_ns},{"note_id",note_id},{"core",note_geometry(n)},
        {"proposal_source",packet->at("proposal_source")},{"role_grade","proposed"},
        {"confirmed_before",confirmed},{"original_winner",selected?selected->track_id:0},
        {"best_score",best},{"second_score",second},{"applied",false},{"changed",false},
        {"override_winner",selected?selected->track_id:0},{"reason","already_confirmed"}};
    if(confirmed)return row;
    if(std::count_if(notes.begin(),notes.end(),[&](const auto& other){return geometry_matches(note_geometry(other),packet->at("core"),true);})!=1){row["reason"]="core_correspondence_nonunique";return row;}
    if(packet->at("line").is_null()){row["reason"]="proposal_unknown_no_line";return row;}
    const LineCandidate* oracle=nullptr;std::size_t matches=0,qualified=0;
    for(const auto& l:s.lines) {
        if(!geometry_matches(line_geometry(l),packet->at("line"),false))continue;
        ++matches;
        const double along=std::abs((n.center.x-l.center.x)*l.tangent.x+(n.center.y-l.center.y)*l.tangent.y);
        if(l.observed_ns!=s.context.capture_ns||!l.association_valid||l.length<s.context.width*.24||
           along>l.length*.5+n.width+24)continue;
        ++qualified;oracle=&l;
    }
    row["correspondence_count"]=matches;row["qualified_count"]=qualified;
    if(matches!=1||qualified!=1){row["reason"]=matches>1?"line_correspondence_nonunique":matches==0?"line_geometry_missing":"line_not_current_or_basic_qualified";return row;}
    row["line"]=line_geometry(*oracle);row["override_winner"]=oracle->track_id;
    row["applied"]=true;row["changed"]=selected!=oracle;row["reason"]="provisional_winner_only";
    // The only strategy mutation. Scores and ALL downstream gates are untouched.
    selected=oracle;++applied_count;if(row.at("changed").get<bool>())++changed_count;
    return row;
}
inline void finish(json row,const GameTarget& t,bool identity_ambiguous,bool relation_ambiguous,
    bool preserve,bool conflict,std::uint64_t confirmed_after) {
    if(row.is_null())return;
    row["identity_ambiguous"]=identity_ambiguous;row["relation_ambiguous"]=relation_ambiguous;
    row["preserve_flag"]=preserve;row["relation_conflict"]=conflict;row["confirmed_after"]=confirmed_after;
    row["final_line_id"]=t.line_id;row["samples"]=t.samples;row["final_reason"]=t.reason;
    row["root_ns"]=t.crossing_ns?json(*t.crossing_ns):json(nullptr);
    row["projection_only"]=t.line_projection_only;
    if(choices.size()>=128)throw std::runtime_error("x4_choices_capacity");choices.push_back(std::move(row));
}
inline json drain() {
    if(packet&&choices.empty())choices.push_back({{"ordinal",ordinal},{"source_frame",packet->at("source_frame")},
        {"png_sha256",packet->at("png_sha256")},{"applied",false},{"changed",false},
        {"role_grade","unknown"},{"reason",packet->at("core").is_null()?"proposal_unknown_no_unique_core":"current_core_geometry_absent"}});
    auto result=std::move(choices);choices=json::array();return result;
}
}
