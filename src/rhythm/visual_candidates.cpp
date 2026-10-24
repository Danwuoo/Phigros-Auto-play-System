#include "rhythm/visual_candidates.hpp"
#include "rhythm/chart_memory.hpp"
#include "rhythm/phigros_pose.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace rhythm {
namespace {
using T=long double;
constexpr SongNs second=1'000'000'000;
constexpr SongNs max_exact_clock=(SongNs{1}<<53)-1; // Exact integer conversion even when long double==double.
void require(bool ok,const char* reason) {if(!ok)throw std::invalid_argument(reason);}
bool text(const std::string& s,std::size_t max=512) {return !s.empty()&&s.size()<=max;}
T down(T x) {return std::nextafter(x,-std::numeric_limits<T>::infinity());}
T up(T x) {return std::nextafter(x,std::numeric_limits<T>::infinity());}
struct I {T lo=0,hi=0;};
I point(T x) {require(std::isfinite(x),"nonfinite arithmetic");return {x,x};}
I add(I a,I b) {
    if(a.lo==0&&a.hi==0)return b;
    if(b.lo==0&&b.hi==0)return a;
    require(std::isfinite(a.lo+b.lo)&&std::isfinite(a.hi+b.hi),"arithmetic overflow");
    return {down(a.lo+b.lo),up(a.hi+b.hi)};
}
I neg(I a) {return {-a.hi,-a.lo};}
I sub(I a,I b) {return add(a,neg(b));}
I mul(I a,I b) {
    if((a.lo==0&&a.hi==0)||(b.lo==0&&b.hi==0))return point(0);
    const T p[]{a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};
    const auto [lo,hi]=std::minmax_element(std::begin(p),std::end(p));
    require(std::isfinite(*lo)&&std::isfinite(*hi),"arithmetic overflow");
    return {down(*lo),up(*hi)};
}
I div(I a,I b) {
    require(b.lo>0||b.hi<0,"division interval contains zero");
    if(a.lo==0&&a.hi==0)return point(0);
    return mul(a,{down(1/b.hi),up(1/b.lo)});
}
bool valid(VisualTimeInterval a) {return a.lower<=a.upper;}
VisualTimeInterval intersect(VisualTimeInterval a,VisualTimeInterval b) {
    return {std::max(a.lower,b.lower),std::min(a.upper,b.upper)};
}
VisualTimeInterval empty() {return {1,0};}
// Restrict integer time by one exact affine bound. Interval arithmetic around
// the root rounds POSSIBLE bounds outward, GUARANTEED exclusions inward.
VisualTimeInterval affine(VisualTimeInterval domain,SongNs origin,T base,T slope,T target,
                          bool ge,bool strict,bool guaranteed) {
    if(!valid(domain))return domain;
    if(slope==0) {
        const bool ok=ge ? (strict?base>target:base>=target) : (strict?base<target:base<=target);
        return ok?domain:empty();
    }
    const auto root=add(point(static_cast<T>(origin)),div(sub(point(target),point(base)),point(slope)));
    const bool lower=ge==(slope>0);
    const T bound=lower ? (guaranteed?root.hi:root.lo) : (guaranteed?root.lo:root.hi);
    if(lower) {
        if(bound<static_cast<T>(domain.lower))return domain;
        if(bound>static_cast<T>(domain.upper))return empty();
        const T rounded=strict?std::floor(bound)+1:std::ceil(bound);
        if(rounded>static_cast<T>(domain.upper))return empty();
        domain.lower=std::max(domain.lower,static_cast<SongNs>(rounded));
    }else {
        if(bound>static_cast<T>(domain.upper))return domain;
        if(bound<static_cast<T>(domain.lower))return empty();
        const T rounded=strict?std::ceil(bound)-1:std::floor(bound);
        if(rounded<static_cast<T>(domain.lower))return empty();
        domain.upper=std::min(domain.upper,static_cast<SongNs>(rounded));
    }
    return domain;
}
struct LineEquation {I base,slope,x;};
VisualTimeInterval possible(const LineEquation& y,VisualTimeInterval d,SongNs origin,T low,T high) {
    d=affine(d,origin,y.base.hi,y.slope.hi,low,true,false,false);
    return affine(d,origin,y.base.lo,y.slope.lo,high,false,false,false);
}
VisualTimeInterval visible(const LineEquation& y,VisualTimeInterval d,SongNs origin,T low,T high) {
    d=affine(d,origin,y.base.lo,y.slope.lo,low,true,false,true);
    return affine(d,origin,y.base.hi,y.slope.hi,high,false,true,true);
}
std::vector<VisualTimeInterval> merge(std::vector<VisualTimeInterval> values) {
    std::sort(values.begin(),values.end(),[](auto a,auto b){return std::pair(a.lower,a.upper)<std::pair(b.lower,b.upper);});
    std::vector<VisualTimeInterval> out;
    for(auto value:values)if(valid(value)) {
        if(!out.empty()&&value.lower<=out.back().upper)out.back().upper=std::max(value.upper,out.back().upper);
        else out.push_back(value);
    }
    return out;
}
SongNs checked(T value) {
    require(std::isfinite(value)&&value>=static_cast<T>(std::numeric_limits<SongNs>::min())+4096&&
            value<=static_cast<T>(std::numeric_limits<SongNs>::max())-4096,"time arithmetic overflow");
    return static_cast<SongNs>(value);
}
VisualTimeInterval range(I a) {return {checked(std::floor(a.lo)),checked(std::ceil(a.hi))};}
struct BudgetExceeded {};
} // namespace

struct VisualTimingCandidateProducer::Impl {
    struct Piece {VisualTimeInterval time;I floor;double speed=0;std::string pointer;};
    OpeningVisualModel model;
    VisualCandidateLimits limits;
    std::vector<SemanticNote> notes;
    std::vector<Piece> pieces;
    std::uint64_t epoch=0;
    bool poisoned=false,have_frame=false,have_offsets=false;
    VisualCandidateFrame previous;
    std::set<std::string> evidence_ids,frame_ids;
    std::vector<std::string> support;
    std::vector<VisualTimeInterval> offsets;
    SongNs last_now=-1;
    std::size_t work=0;

    void bump(std::size_t n=1) {if(n>limits.max_work-work)throw BudgetExceeded{};work+=n;}
    void cap(std::size_t count) {if(count>limits.max_candidates)throw BudgetExceeded{};}
    std::vector<VisualTimeInterval> intersections(const std::vector<VisualTimeInterval>& a,
                                                 const std::vector<VisualTimeInterval>& b) {
        std::vector<VisualTimeInterval> out;
        for(auto x:a)for(auto y:b){bump();auto z=intersect(x,y);if(valid(z)){out.push_back(z);cap(out.size());}}
        return merge(std::move(out));
    }
    std::vector<VisualTimeInterval> subtract(std::vector<VisualTimeInterval> values,VisualTimeInterval cut) {
        if(!valid(cut))return values;
        std::vector<VisualTimeInterval> out;
        for(auto a:values) {
            bump();const auto overlap=intersect(a,cut);
            if(!valid(overlap))out.push_back(a);
            else {
                if(a.lower<overlap.lower)out.push_back({a.lower,overlap.lower-1});
                if(overlap.upper<a.upper)out.push_back({overlap.upper+1,a.upper});
            }
            cap(out.size());
        }
        return out;
    }
    LineEquation equation(const Piece& p,const SemanticNote& note) const {
        const I scale=mul({model.approach_scale_lower,model.approach_scale_upper},point(note.speed));
        const auto padding=I{-static_cast<T>(model.pose_tolerance_pixels),static_cast<T>(model.pose_tolerance_pixels)};
        return {sub(add(point(model.origin_y),padding),
                    mul(scale,sub(point(note.floor_position),p.floor))),
                div(mul(scale,point(p.speed)),point(second)),
                add(add(point(model.origin_x),padding),
                    mul(point(model.x_pixels_per_unit),point(note.position_x)))};
    }
    std::vector<VisualTimeCandidate> solve(const VisualCandidateFrame& frame) {
        std::vector<VisualTimeCandidate> out;
        for(const auto& piece:pieces) {
            std::vector<LineEquation> equations;
            std::vector<VisualTimeInterval> exclusions;
            for(const auto& note:notes) {
                bump();const auto eq=equation(piece,note);equations.push_back(eq);
                auto domain=piece.time;domain.upper=std::min(domain.upper,note.start_ns-1);
                const auto right=sub(point(1280),point(model.horizontal_visibility_margin_pixels));
                const bool full_x=eq.x.lo>=model.horizontal_visibility_margin_pixels&&eq.x.hi<right.lo;
                exclusions.push_back(full_x?visible(eq,domain,piece.time.lower,model.visible_top,model.visible_bottom):empty());
            }
            struct Choice {std::size_t note;VisualTimeInterval time;};
            std::vector<std::vector<Choice>> choices(frame.centers.size());
            bool missing=false;
            for(std::size_t i=0;i<frame.centers.size();++i) {
                const auto& center=frame.centers[i];
                const I error{-static_cast<T>(model.pixel_error),static_cast<T>(model.pixel_error)};
                const auto measured_x=add(point(center.x),error),measured_y=add(point(center.y),error);
                for(std::size_t j=0;j<notes.size();++j) {
                    bump();const auto& eq=equations[j];
                    if(eq.x.hi<measured_x.lo||eq.x.lo>measured_x.hi)continue;
                    auto domain=piece.time;domain.upper=std::min(domain.upper,notes[j].start_ns-1);
                    domain=possible(eq,domain,piece.time.lower,measured_y.lo,measured_y.hi);
                    // Potential visibility is sufficient; uncertain boundary pixels
                    // remain candidates instead of silently choosing an inside branch.
                    domain=possible(eq,domain,piece.time.lower,model.visible_top,model.visible_bottom);
                    if(valid(domain))choices[i].push_back({j,domain});
                }
                if(choices[i].empty())missing=true;
            }
            if(missing)continue;
            std::vector<bool> used(notes.size(),false);
            std::vector<VisualNoteMatch> matches;
            const auto visit=[&](auto&& self,std::size_t index,VisualTimeInterval domain)->void {
                bump();
                if(index==choices.size()) {
                    std::vector<VisualTimeInterval> parts{domain};
                    for(std::size_t j=0;j<notes.size()&&!parts.empty();++j)if(!used[j])parts=subtract(std::move(parts),exclusions[j]);
                    for(auto part:parts) {out.push_back({part,piece.pointer,piece.speed,matches});cap(out.size());}
                    return;
                }
                for(const auto& choice:choices[index]) {
                    bump();if(used[choice.note])continue;
                    auto next=intersect(domain,choice.time);if(!valid(next))continue;
                    used[choice.note]=true;
                    matches.push_back({frame.centers[index].id,notes[choice.note].source.id,notes[choice.note].source.json_pointer});
                    self(self,index+1,next);matches.pop_back();used[choice.note]=false;
                }
            };
            visit(visit,0,piece.time);
        }
        return out;
    }
};

VisualTimingCandidateProducer::VisualTimingCandidateProducer(const SemanticChart& chart,OpeningVisualModel model,
                                                             VisualCandidateLimits limits):impl_(std::make_unique<Impl>()) {
    require(chart.format_version==3&&!chart.lines.empty()&&sha256_text(chart.source.chart_sha256)&&
            model.chart_sha256==chart.source.chart_sha256,"chart identity or format mismatch");
    require(text(model.model_id)&&text(model.calibration_id)&&sha256_text(model.model_sha256)&&sha256_text(model.calibration_sha256),
            "model/calibration identity required");
    require(model.begin_ns>=0&&model.begin_ns<model.end_ns&&model.end_ns<=60*second&&
            model.note_horizon_ns>=model.end_ns&&model.note_horizon_ns<=600*second,"opening scope unsupported");
    require(model.origin_x==640&&model.origin_y==576&&model.x_pixels_per_unit==72&&
            std::isfinite(model.pose_tolerance_pixels)&&model.pose_tolerance_pixels>=0&&model.pose_tolerance_pixels<=.001&&
            std::isfinite(model.horizontal_visibility_margin_pixels)&&model.horizontal_visibility_margin_pixels>=0&&
            model.horizontal_visibility_margin_pixels<640&&
            model.visible_top>=0&&model.visible_top<model.visible_bottom&&model.visible_bottom<=720&&
            std::isfinite(model.visible_top)&&std::isfinite(model.visible_bottom),"only fixed horizontal opening model supported");
    require(std::isfinite(model.approach_scale_lower)&&std::isfinite(model.approach_scale_upper)&&
            model.approach_scale_lower>0&&model.approach_scale_lower<=model.approach_scale_upper&&model.approach_scale_upper<=100000&&
            std::isfinite(model.pixel_error)&&model.pixel_error>=0&&model.pixel_error<=32&&
            model.per_frame_time_allowance_ns>=0&&model.per_frame_time_allowance_ns<=second,"invalid model allowance");
    require(limits.max_notes>0&&limits.max_notes<=1024&&limits.max_speed_pieces>0&&limits.max_speed_pieces<=1024&&
            limits.max_centers>0&&limits.max_centers<=8&&limits.max_candidates>0&&limits.max_candidates<=4096&&
            limits.max_work>0&&limits.max_work<=2000000&&limits.max_observations>0&&limits.max_observations<=1024&&
            limits.max_observation_age_ns>0&&limits.max_observation_age_ns<=second&&
            limits.max_gap_ns>0&&limits.max_gap_ns<=10*second,"invalid resource limits");
    const auto& line=chart.lines[0];require(line.source_index==0&&line.speed_events.size()<=limits.max_speed_pieces,
                                          "unsupported line or speed budget");
    require(line.move_events.size()+line.rotate_events.size()+line.opacity_events.size()<=16384,"pose event budget");
    SemanticChart pose_chart;pose_chart.format_version=3;pose_chart.lines.resize(1);
    pose_chart.lines[0].bpm=line.bpm;pose_chart.lines[0].move_events=line.move_events;
    pose_chart.lines[0].rotate_events=line.rotate_events;pose_chart.lines[0].opacity_events=line.opacity_events;
    PhigrosPoseEvaluator pose(pose_chart);
    std::vector<SongNs> checkpoints{model.begin_ns,model.end_ns-1};
    const auto boundary=[&](SongNs t) {
        if(t>=model.begin_ns&&t<model.end_ns)checkpoints.push_back(t);
        if(t>model.begin_ns&&t<=model.end_ns)checkpoints.push_back(t-1); // Before a later jump wins the shared key.
    };
    for(const auto& event:line.move_events)for(auto t:{event.time.start_ns,event.time.end_ns})boundary(t);
    for(const auto& event:line.rotate_events)for(auto t:{event.time.start_ns,event.time.end_ns})boundary(t);
    for(auto t:checkpoints) {const auto p=pose.line_at(0,t);require(std::abs(p.origin.x*1280-640)<=model.pose_tolerance_pixels&&
        std::abs(p.origin.y*720-576)<=model.pose_tolerance_pixels&&
        p.rotation_degrees==0&&std::abs(p.tangent.x-1)<1e-12&&std::abs(p.tangent.y)<1e-12,
        "moving/rotating line outside supported model");}
    std::set<std::string> ids;
    for(const auto& note:line.notes) {
        require(note.start_ns>=model.end_ns,"opening scope reaches an already judged note");
        if(note.side!=NoteSide::above||(note.kind!=NoteKind::tap&&note.kind!=NoteKind::drag)||note.start_ns>model.note_horizon_ns)continue;
        require(std::isfinite(note.position_x)&&std::abs(note.position_x)<=100&&std::isfinite(note.floor_position)&&
                std::abs(note.floor_position)<=1e9&&std::isfinite(note.speed)&&note.speed>0&&note.speed<=10000,
                "unsupported note values");
        require(text(note.source.json_pointer,1024)&&note.source.id==chart.source.chart_sha256+'#'+note.source.json_pointer&&
                ids.insert(note.source.id).second,"invalid source note provenance");
        impl_->notes.push_back(note);require(impl_->notes.size()<=limits.max_notes,"note budget");
    }
    require(!impl_->notes.empty(),"no supported above Tap/Drag anchors");
    require(std::isfinite(chart.offset_seconds)&&std::abs(chart.offset_seconds)<=3600,"invalid chart offset");
    const auto origin=static_cast<SongNs>(std::llround(chart.offset_seconds*second));
    SongNs cursor=origin;I floor=point(0);
    for(const auto& event:line.speed_events) {
        require(event.time.start_ns==cursor&&event.time.end_ns>event.time.start_ns&&
                std::isfinite(event.value)&&std::abs(event.value)<=1e6,"speed coverage must be contiguous from native zero");
        require(!event.floor_position,"provided speed floor anchors are unsupported by the zero-integral model");
        require(text(event.source.json_pointer,1024),"speed source provenance missing");
        const SongNs start=std::max(model.begin_ns,event.time.start_ns),end=std::min(model.end_ns,event.time.end_ns);
        if(start<end) {
            const auto at=add(floor,mul(point(event.value),div(point(static_cast<T>(start)-event.time.start_ns),point(second))));
            impl_->pieces.push_back({{start,end-1},at,event.value,event.source.json_pointer});
        }
        cursor=event.time.end_ns;
        if(cursor>=model.end_ns)break;
        floor=add(floor,mul(point(event.value),div(point(static_cast<T>(event.time.end_ns)-event.time.start_ns),point(second))));
    }
    require(cursor>=model.end_ns&&!impl_->pieces.empty(),"speed coverage does not reach opening end");
    impl_->model=std::move(model);impl_->limits=limits;
}
VisualTimingCandidateProducer::~VisualTimingCandidateProducer()=default;
VisualTimingCandidateProducer::VisualTimingCandidateProducer(VisualTimingCandidateProducer&&) noexcept=default;
VisualTimingCandidateProducer& VisualTimingCandidateProducer::operator=(VisualTimingCandidateProducer&&) noexcept=default;
void VisualTimingCandidateProducer::begin_epoch(std::uint64_t epoch) {
    auto& p=*impl_;require(epoch>p.epoch,"new epoch must strictly increase");p.epoch=epoch;
    p.poisoned=false;p.have_frame=false;p.have_offsets=false;p.evidence_ids.clear();p.frame_ids.clear();
    p.support.clear();p.offsets.clear();p.previous={};p.last_now=-1;
}

VisualCandidateResult VisualTimingCandidateProducer::observe(const VisualCandidateFrame& frame,SongNs now) {
    auto& p=*impl_;VisualCandidateResult result;
    result.model_id=p.model.model_id;result.model_sha256=p.model.model_sha256;
    result.calibration_id=p.model.calibration_id;result.calibration_sha256=p.model.calibration_sha256;
    // Copy only bounded observations; malformed huge caller payloads are not echoed.
    if(frame.centers.size()<=p.limits.max_centers&&frame.frame_id.size()<=512&&frame.evidence_id.size()<=512&&
       frame.source_id.size()<=512&&frame.source_sha256.size()==64&&frame.frame_sha256.size()==64&&
       frame.clock_id.size()<=512&&frame.opening_context_id.size()<=512&&
       std::all_of(frame.centers.begin(),frame.centers.end(),[](const auto& c){return c.id.size()<=128;}))result.observation=frame;
    p.work=0;
    const auto fail=[&](const char* reason,bool poison,VisualCandidateState state=VisualCandidateState::unknown) {
        p.offsets.clear();p.have_offsets=false;if(poison)p.poisoned=true;
        result.reason=reason;result.state=state;result.requires_new_epoch=p.poisoned;result.work=p.work;return result;
    };
    if(p.poisoned)return fail("new_epoch_required",true);
    try {
        require(p.epoch&&frame.epoch==p.epoch&&frame.sequence&&frame.generation&&frame.geometry_version&&frame.source_valid,
                "invalid source/epoch");
        require(frame.width==1280&&frame.height==720&&text(frame.source_id)&&sha256_text(frame.source_sha256)&&
                text(frame.frame_id)&&text(frame.evidence_id)&&sha256_text(frame.frame_sha256)&&text(frame.clock_id)&&
                text(frame.opening_context_id),"missing observation provenance/geometry");
        require(now>=0&&now<=max_exact_clock&&frame.received_host_ns>=0&&frame.received_host_ns<=now&&
                static_cast<T>(now)-frame.received_host_ns<p.limits.max_observation_age_ns&&
                (p.last_now<0||now>=p.last_now),"stale/future/rollback observation");
        require(!p.evidence_ids.contains(frame.evidence_id)&&!p.frame_ids.contains(frame.frame_id),"duplicate/replayed observation");
        if(p.have_frame) {
            const auto& old=p.previous;
            require(frame.sequence>old.sequence&&frame.received_host_ns>old.received_host_ns,"observation order did not increase");
            require(frame.source_id==old.source_id&&frame.source_sha256==old.source_sha256&&frame.clock_id==old.clock_id&&
                    frame.clock_kind==old.clock_kind&&frame.generation==old.generation&&frame.geometry_version==old.geometry_version&&
                    frame.opening_context_id==old.opening_context_id,"source/model context changed");
            require(static_cast<T>(frame.received_host_ns)-old.received_host_ns<=p.limits.max_gap_ns&&
                    static_cast<T>(now)-old.received_host_ns<=p.limits.max_gap_ns,"observation gap");
            result.unchanged_pixels=frame.frame_sha256==old.frame_sha256;
        }
        require(p.evidence_ids.size()<p.limits.max_observations,"observation ledger budget");
        require(frame.centers.size()<=p.limits.max_centers,"center budget");
        std::optional<VisualTimeInterval> source;
        if(frame.clock_kind==VisualSourceClock::video_pts) {
            require(frame.source_time_ns&&*frame.source_time_ns>=0&&*frame.source_time_ns<=max_exact_clock&&
                    !frame.capture_host_interval,"invalid video PTS provenance");
            if(p.have_frame)require(*frame.source_time_ns>*p.previous.source_time_ns,"PTS did not increase");
            source=VisualTimeInterval{*frame.source_time_ns,*frame.source_time_ns};
        }else if(frame.clock_kind==VisualSourceClock::mapped_capture) {
            require(frame.capture_host_interval&&!frame.source_time_ns&&valid(*frame.capture_host_interval)&&
                    frame.capture_host_interval->lower>=0&&frame.capture_host_interval->upper<=frame.received_host_ns&&
                    static_cast<T>(now)-frame.capture_host_interval->lower<p.limits.max_observation_age_ns,"invalid caller capture interval");
            if(p.have_frame)require(frame.capture_host_interval->lower>p.previous.capture_host_interval->lower&&
                                    frame.capture_host_interval->upper>p.previous.capture_host_interval->upper,"capture interval did not advance");
            source=frame.capture_host_interval;
        }else require(frame.clock_kind==VisualSourceClock::arrival_only&&!frame.source_time_ns&&!frame.capture_host_interval,
                     "unknown/contradictory source clock");
        std::set<std::string> center_ids;
        for(const auto& center:frame.centers)require(text(center.id,128)&&center_ids.insert(center.id).second&&
            std::isfinite(center.x)&&std::isfinite(center.y)&&center.x>=0&&center.x<1280&&
            center.y>=p.model.visible_top&&center.y<p.model.visible_bottom,"invalid center/ROI provenance");
        p.evidence_ids.insert(frame.evidence_id);p.frame_ids.insert(frame.frame_id);p.last_now=now;
        p.previous=frame;p.have_frame=true;
        if(frame.centers.empty())return fail("no_centers",p.have_offsets);
        require(frame.centers_complete_unoccluded,"complete unoccluded center assumption missing");
        auto candidates=p.solve(frame);
        if(candidates.empty())return fail("no_pixel_model_match",p.have_offsets,VisualCandidateState::rejected);
        if(source) {
            std::vector<VisualTimeInterval> proposed;
            for(const auto& c:candidates) {
                p.bump();const auto interval=sub(point(static_cast<T>(source->lower)),point(static_cast<T>(c.song_time.upper)));
                const auto upper=sub(point(static_cast<T>(source->upper)),point(static_cast<T>(c.song_time.lower)));
                proposed.push_back(range(add({interval.lo,upper.hi},{-static_cast<T>(p.model.per_frame_time_allowance_ns),
                                                                       static_cast<T>(p.model.per_frame_time_allowance_ns)})));
                p.cap(proposed.size());
            }
            proposed=merge(std::move(proposed));
            auto next=p.have_offsets?p.intersections(p.offsets,proposed):proposed;
            if(next.empty())return fail("no_temporally_consistent_branch",true,VisualCandidateState::rejected);
            std::vector<VisualTimeCandidate> filtered;
            for(const auto& c:candidates)for(auto offset:next) {
                p.bump();const auto lo=sub(point(static_cast<T>(source->lower)),point(static_cast<T>(offset.upper)));
                const auto hi=sub(point(static_cast<T>(source->upper)),point(static_cast<T>(offset.lower)));
                auto compatible=range(add({lo.lo,hi.hi},{-static_cast<T>(p.model.per_frame_time_allowance_ns),
                                                        static_cast<T>(p.model.per_frame_time_allowance_ns)}));
                compatible=intersect(c.song_time,compatible);
                if(valid(compatible)){auto copy=c;copy.song_time=compatible;filtered.push_back(std::move(copy));p.cap(filtered.size());}
            }
            require(!filtered.empty(),"temporal projection invalid");candidates=std::move(filtered);
            p.offsets=std::move(next);p.have_offsets=true;p.support.push_back(frame.evidence_id);
            result.conditional_source_minus_song_intervals=p.offsets;result.supporting_evidence_ids=p.support;
        }else result.supporting_evidence_ids={frame.evidence_id};
        result.candidates=std::move(candidates);
        for(const auto& c:result.candidates)result.song_intervals.push_back(c.song_time);
        result.song_intervals=merge(std::move(result.song_intervals));
        result.state=result.song_intervals.size()==1?VisualCandidateState::conditional_candidate:VisualCandidateState::ambiguous;
        result.reason=source?"conditional_rate_one_model_only":"arrival_only_independent_pixel_candidates";
        result.work=p.work;return result;
    }catch(const BudgetExceeded&) {return fail("resource_budget_exhausted",true);}
    catch(const std::invalid_argument& error) {return fail(error.what(),true);}
}
} // namespace rhythm
