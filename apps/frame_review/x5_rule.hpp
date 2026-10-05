#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

// Pure, fixed-capacity shadow geometry. No files, JSON, IDs, ordinal, role
// proposal, model/root, owner, receipt, scheduler or action API.
namespace pas::x5 {
inline constexpr const char* rule_version="X5-NDA-v1";
struct Point {double x=0,y=0;};
inline Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y};}
inline double dot(Point a,Point b){return a.x*b.x+a.y*b.y;}
inline double norm(Point p){return std::hypot(p.x,p.y);}
inline Point normal(Point u){return {-u.y,u.x};}
enum class Identity {unknown,ambiguous,unique};
enum class Confirmation {unknown,unconfirmed,confirmed};
struct Pose {Point center,tangent;double length=0,confidence=0;bool valid=false,current=false;};
struct Sample {std::int64_t ns=0;Point note;std::array<std::optional<Pose>,16> lines;};
struct Input {
    std::int64_t now=0;double frame_extent=0,width=0,height=0;
    Point center,tangent;bool appearance_known=false,strong_current=false,hold_or_body=false;
    Identity identity=Identity::unknown;Confirmation confirmation=Confirmation::unknown;
    std::array<Pose,16> lines;std::size_t line_count=0;
    std::array<Sample,6> samples;std::size_t sample_count=0;
};
struct Feature {
    bool current=false,support=false,pair_valid=false,eligible=false,unresolved=false;
    std::string reason="current_support_unknown";
    double distance=0,along=0;std::optional<double> appearance_dot;
    std::optional<double> closing_rate,note_normal_rate,note_tangent_rate,line_normal_rate;
    std::optional<double> relative_signed_rate,normal_fraction,line_contribution,max_angle;
    std::optional<Point> note_velocity,line_velocity;
    std::size_t measured_poses=0;
};
struct Decision {
    std::array<Feature,16> features;std::size_t line_count=0,eligible_count=0;
    std::optional<std::size_t> selected;std::optional<double> margin;
    bool scope_eligible=false;std::string reason="no_candidates";
};
inline void check_point(Point p){if(!std::isfinite(p.x)||!std::isfinite(p.y))throw std::runtime_error("x5_nonfinite");}
inline void check_pose(const Pose& p){check_point(p.center);check_point(p.tangent);if(std::abs(norm(p.tangent)-1)>.01||!std::isfinite(p.length)||p.length<=0||!std::isfinite(p.confidence)||p.confidence<0||p.confidence>1)throw std::runtime_error("x5_pose_geometry");}
inline double distance(Point p,const Pose& l){return dot(sub(p,l.center),normal(l.tangent));}
inline Decision evaluate(const Input& in) {
    if(in.line_count>16||in.sample_count>6)throw std::runtime_error("x5_object_capacity");
    check_point(in.center);check_point(in.tangent);
    if(in.now<0||!std::isfinite(in.frame_extent)||in.frame_extent<=0||!std::isfinite(in.width)||in.width<=0||!std::isfinite(in.height)||in.height<=0||(in.appearance_known&&std::abs(norm(in.tangent)-1)>.01))throw std::runtime_error("x5_note_geometry");
    for(std::size_t k=0;k<in.sample_count;++k){const auto& s=in.samples[k];check_point(s.note);if(s.ns<0||s.ns>in.now||in.now-s.ns>90'000'000||(k&&s.ns<=in.samples[k-1].ns))throw std::runtime_error("x5_history_time");for(std::size_t j=0;j<in.line_count;++j)if(s.lines[j])check_pose(*s.lines[j]);}
    if(in.sample_count&&(in.samples[in.sample_count-1].ns!=in.now||norm(sub(in.samples[in.sample_count-1].note,in.center))>1e-6))throw std::runtime_error("x5_current_sample_binding");
    Decision out;out.line_count=in.line_count;
    for(std::size_t j=0;j<in.line_count;++j){const auto& l=in.lines[j];check_pose(l);auto& f=out.features[j];
        f.current=l.current&&l.valid;f.distance=distance(in.center,l);f.along=dot(sub(in.center,l.center),l.tangent);if(in.appearance_known)f.appearance_dot=std::abs(dot(in.tangent,l.tangent));
        f.support=f.current&&l.length>=in.frame_extent*.32&&l.confidence>=.5&&std::abs(f.along)<=l.length*.5;
        if(!f.current){f.reason="current_pose_invalid_or_stale";f.unresolved=true;continue;}
        if(!f.support){f.reason="span_extent_or_confidence_insufficient";continue;}
        bool complete=in.sample_count>=3;
        for(std::size_t k=0;k<in.sample_count;++k){if(in.samples[k].lines[j]&&in.samples[k].lines[j]->current&&in.samples[k].lines[j]->valid)++f.measured_poses;else complete=false;}
        if(!complete){f.reason="paired_history_missing";f.unresolved=true;continue;}
        const auto& first=in.samples[0];const auto& last=in.samples[in.sample_count-1];const auto span=last.ns-first.ns;
        if(span<12'000'000){f.reason="temporal_span_below_12ms";f.unresolved=true;continue;}
        const auto delta=sub(last.note,first.note);const double seconds=span/1e9,travel=norm(delta);
        f.pair_valid=true;f.note_velocity=Point{delta.x/seconds,delta.y/seconds};
        const auto ld=sub(last.lines[j]->center,first.lines[j]->center);f.line_velocity=Point{ld.x/seconds,ld.y/seconds};
        f.note_normal_rate=dot(delta,normal(l.tangent))/seconds;f.note_tangent_rate=dot(delta,l.tangent)/seconds;f.line_normal_rate=dot(ld,normal(l.tangent))/seconds;
        auto old=*first.lines[j];if(dot(old.tangent,l.tangent)<0)old.tangent={-old.tangent.x,-old.tangent.y};
        const auto d0=distance(first.note,old);f.closing_rate=(std::abs(f.distance)-std::abs(d0))/seconds;f.relative_signed_rate=(f.distance-d0)/seconds;
        double fraction=1,contribution=0,angle=0;bool normal_approach=true,closes=true;
        for(std::size_t k=1;k<in.sample_count;++k){auto a=*in.samples[k-1].lines[j],b=*in.samples[k].lines[j];if(dot(a.tangent,b.tangent)<0)a.tangent={-a.tangent.x,-a.tangent.y};
            const auto nd=sub(in.samples[k].note,in.samples[k-1].note);const double ndn=dot(nd,normal(b.tangent)),step=norm(nd);
            const double da=distance(in.samples[k-1].note,a),db=distance(in.samples[k].note,b);
            fraction=std::min(fraction,step>1e-9?std::abs(ndn)/step:0.0);
            // Observed local-distance change minus note-only change includes
            // line translation AND rotation, not the fitted motion model.
            contribution=std::max(contribution,std::abs(ndn)>1e-9?std::abs((db-da)-ndn)/std::abs(ndn):1e9);
            angle=std::max(angle,std::acos(std::clamp(std::abs(dot(a.tangent,b.tangent)),0.0,1.0)));
            normal_approach&=db*ndn<0;closes&=da*db>0&&std::abs(db)<std::abs(da);
        }
        f.normal_fraction=fraction;f.line_contribution=contribution;f.max_angle=angle;
        if(std::abs(f.distance)<=in.height*.5+2){f.reason="overlap_or_crossed_not_role_proof";f.unresolved=true;continue;}
        if(travel<3||travel/seconds>4000){f.reason="note_travel_or_speed_outside_scope";f.unresolved=true;continue;}
        if(!closes||std::abs(d0)-std::abs(f.distance)<3){f.reason="not_sustained_closing";continue;}
        if(angle>.10||contribution>.5){f.reason="line_driven_or_rotating_closing_unresolved";f.unresolved=true;continue;}
        if(!normal_approach||fraction<.85){f.reason="note_motion_tangential_or_receding";continue;}
        f.eligible=true;f.reason="measured_note_driven_approach";++out.eligible_count;
    }
    // These preconditions are separate from per-candidate measurements.
    if(in.hold_or_body){out.reason="hold_head_body_tail_outside_rule";return out;}
    if(!in.strong_current){out.reason="note_current_support_unknown";return out;}
    if(in.identity!=Identity::unique){out.reason=in.identity==Identity::ambiguous?"identity_ambiguous":"identity_correspondence_unknown";return out;}
    if(in.confirmation!=Confirmation::unconfirmed){out.reason=in.confirmation==Confirmation::confirmed?"confirmed_relation_outside_early_rule":"confirmation_not_exported";return out;}
    out.scope_eligible=true;
    if(!out.eligible_count){out.reason="no_geometrically_eligible_candidate";return out;}
    for(std::size_t j=0;j<in.line_count;++j)if(out.features[j].unresolved){out.reason="unresolved_current_competitor";return out;}
    double best=-1,second=-1;std::size_t index=0;
    for(std::size_t j=0;j<in.line_count;++j)if(out.features[j].eligible){const double score=*out.features[j].normal_fraction;if(score>best){second=best;best=score;index=j;}else second=std::max(second,score);}
    if(second>=0){out.margin=best-second;if(*out.margin<.15){out.reason="competition_margin_below_015";return out;}}
    out.selected=index;out.reason="shadow_geometry_recommendation_role_unverified";return out;
}
}
