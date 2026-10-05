#pragma once
#include "review_io.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

// Read-only geometry measurements. No winner, intent, owner or scheduler API.
namespace pas::x3 {
using pas::review::json;
struct Point { double x=0,y=0; };
inline Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y};}
inline double dot(Point a,Point b){return a.x*b.x+a.y*b.y;}
inline double norm(Point a){return std::hypot(a.x,a.y);}
inline Point point(const json& j) {
    if(!j.is_array()||j.size()!=2||!j[0].is_number()||!j[1].is_number())throw std::runtime_error("x3_point_shape");
    Point p{j[0].get<double>(),j[1].get<double>()};
    if(!std::isfinite(p.x)||!std::isfinite(p.y))throw std::runtime_error("x3_nonfinite");return p;
}
inline json vec(Point p){return json::array({p.x,p.y});}
struct Line { Point c,u; bool valid=false,motion=false; double length=0; std::int64_t observed=0; std::uint64_t id=0; };
inline Line line(const json& j) {
    Line l{point(j.at("center")),point(j.at("tangent")),j.at("association_valid").get<bool>(),j.at("motion_valid").get<bool>(),j.at("length").get<double>(),j.at("observed_ns").get<std::int64_t>(),j.at("line_id").get<std::uint64_t>()};
    if(std::abs(norm(l.u)-1)>.01||!std::isfinite(l.length)||l.length<=0||l.observed<0||!l.id)throw std::runtime_error("x3_line_geometry");
    return l;
}
inline Point canonical(Point u){if(u.x< -1e-10||(std::abs(u.x)<=1e-10&&u.y<0))return {-u.x,-u.y};return u;}
inline double signed_distance(Point p,const Line& l){return dot(sub(p,l.c),{-l.u.y,l.u.x});}
inline Point hit(Point p,const Line& l){const auto d=signed_distance(p,l);return {p.x+d*l.u.y,p.y-d*l.u.x};}
inline json measured_pair(Point current,const Line& l,Point prior,const std::optional<Line>& previous,std::int64_t prior_ns,std::int64_t now) {
    json r={{"valid",false},{"reason","previous_line_measurement_unknown"},{"dt_ns",nullptr},{"signed_distance_delta_px",nullptr},{"distance_rate_px_s",nullptr},{"absolute_distance_rate_px_s",nullptr},{"note_velocity_px_s",nullptr},{"note_normal_velocity_px_s",nullptr},{"note_tangent_velocity_px_s",nullptr}};
    const auto dt=now-prior_ns;
    if(dt<=0||dt>90'000'000){r["reason"]="note_time_outside_90ms";return r;}
    if(!previous)return r;
    if(!l.valid||!previous->valid||l.observed!=now||previous->observed!=prior_ns){r["reason"]="line_not_current_valid_at_both_note_times";return r;}
    if(l.id!=previous->id){r["reason"]="line_ID_continuity_unknown";return r;}
    if(std::abs(dot(l.u,previous->u))<.5){r["reason"]="orientation_discontinuous";return r;}
    auto local=l;local.u=canonical(l.u);auto old=*previous;if(dot(local.u,old.u)<0)old.u={-old.u.x,-old.u.y};
    const auto d=signed_distance(current,local),pd=signed_distance(prior,old);const double seconds=dt/1e9;
    Point v{(current.x-prior.x)/seconds,(current.y-prior.y)/seconds};
    r["valid"]=true;r["reason"]="two_measured_poses_runtime_ID_proposed_continuity";r["dt_ns"]=dt;
    r["signed_distance_delta_px"]=d-pd;r["distance_rate_px_s"]=(d-pd)/seconds;r["absolute_distance_rate_px_s"]=(std::abs(d)-std::abs(pd))/seconds;
    r["note_velocity_px_s"]=vec(v);r["note_normal_velocity_px_s"]=dot(v,{-local.u.y,local.u.x});r["note_tangent_velocity_px_s"]=dot(v,local.u);
    return r; // Includes measured rotation; it is a secant, not an instantaneous velocity or root.
}
inline json continuation(const std::optional<Line>& previous,Point prior,std::int64_t prior_ns,const Line& selected,bool old_visible,std::int64_t now) {
    json r={{"old_visible",old_visible},{"prior_measured_point_known",previous.has_value()},{"evidence_age_ns",nullptr},{"within_90ms",nullptr},{"abs_tangent_dot",nullptr},{"tangent_pass_097",nullptr},{"prior_hit_normal_gap_px",nullptr},{"gap_pass_36px",nullptr},{"local_geometry_continuation",nullptr},{"first_geometry_guard",nullptr}};
    if(old_visible){r["first_geometry_guard"]="old_line_currently_visible";}
    if(!previous){if(!old_visible)r["first_geometry_guard"]="prior_point_unknown";return r;}
    const auto age=now-prior_ns;r["evidence_age_ns"]=age;r["within_90ms"]=age>=0&&age<=90'000'000;
    const auto alignment=std::abs(dot(previous->u,selected.u));const auto gap=std::abs(signed_distance(hit(prior,*previous),selected));
    r["abs_tangent_dot"]=alignment;r["tangent_pass_097"]=alignment>=.97;r["prior_hit_normal_gap_px"]=gap;r["gap_pass_36px"]=gap<=36;
    r["local_geometry_continuation"]=!old_visible&&age>=0&&age<=90'000'000&&alignment>=.97&&gap<=36;
    if(!old_visible)r["first_geometry_guard"]=age<0||age>90'000'000?"prior_point_outside_90ms":alignment<.97?"tangent_not_local_continuation":gap>36?"prior_hit_normal_gap":"local_geometry_pass_replacement_count_and_12ms_unknown";
    return r; // Existing geometry predicates only; never recommends releasing a guard.
}
inline json owner_status(const json& owner,const json& receipts) {
    json r={{"identity",owner},{"down_receipt",nullptr},{"last_receipt",nullptr},{"contact_id",nullptr},{"state","unknown"},{"game_adoption","unknown"}};
    if(!receipts.is_null()) {r["down_receipt"]=receipts.value("successful_down",json(nullptr));r["last_receipt"]=receipts.value("last_receipt",json(nullptr));}
    if(!r["down_receipt"].is_null())r["contact_id"]=r["down_receipt"].at("command").at("contact_id");
    if(!r["last_receipt"].is_null()&&!r["last_receipt"].at("success").get<bool>())r["state"]="unknown_receipt_no_retry";
    else if(!r["last_receipt"].is_null()&&r["last_receipt"].at("command").at("phase")==2)r["state"]="up_receipt_completed_no_resurrection";
    else if(!owner.is_null()) {
        if(!owner.at("submitted").get<bool>())r["state"]="not_submitted";
        else if(owner.at("cursor").is_null())r["state"]="retired_or_completed_or_unknown_no_retry";
        else r["state"]=owner.at("cursor").get<std::size_t>()<=owner.at("prefix_offset").get<std::size_t>()?"pending":"started";
    }
    return r;
}
}
