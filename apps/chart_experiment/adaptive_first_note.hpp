#pragma once
#include "first_note_trigger.hpp"
#include "../chart_calibrate/opening_detector.hpp"
#include "rhythm/phigros_pose.hpp"
#include <array>
#include <sstream>
#include <iomanip>
#include <numbers>

namespace rhythm::experimental {
// Native opening geometry is a conditional rendering hypothesis, never a
// capture-clock qualification or a calibrated game hit region. This separate
// path leaves the archived Dlyrotz first-note model unchanged.
struct AdaptiveOpeningDescription {
    Ns begin_ns=0,end_ns=0,note_time_ns=0,first_down_ns=0;
    std::size_t line_index=0,native_pieces=0,modeled_notes=0,earliest_note_count=0;
    NoteKind note_kind=NoteKind::tap;
    std::string note_id,note_json_pointer,model_id="native-piecewise-2d-opening-v3",model_sha256;
    std::string detector_policy_id="typed-rle-oriented-head-v1";
    std::vector<std::string> earliest_note_ids;
    std::size_t retained_equations=0,provably_invisible_equations=0;
    double approach_pixels_per_floor_unit=720/(2*.83175),pixel_error=3;
};

// Same bounded connected-run algorithm as the archived cyan detector. Yellow
// Drag runs are selected explicitly from the immutable first-note semantics;
// this detector does not infer note identity, chart time or complete coverage.
inline calibration::OpeningDetection detect_adaptive_horizontal_opening_centers(
        int width,int height,int stride,std::span<const std::uint8_t> rgb,NoteKind kind) {
    if(kind==NoteKind::tap||kind==NoteKind::hold)return calibration::detect_opening_centers(width,height,stride,rgb);
    calibration::OpeningDetection result;
    if((kind!=NoteKind::drag&&kind!=NoteKind::flick)||width!=1280||height!=720||stride<3840||stride>16384||
       rgb.size()!=static_cast<std::size_t>(stride)*720)return result;
    struct Group {int last_y=0,last_x2=0;std::int64_t sum_x2=0,sum_y=0;std::vector<int> widths;};
    std::vector<Group> groups;
    const auto fail=[&](const char* why) {
        result.valid=false;result.reason=why;result.centers.clear();result.metrics.clear();result.groups=groups.size();return result;
    };
    const auto yellow=[&](int x,int y) {
        const auto i=static_cast<std::size_t>(y)*static_cast<std::size_t>(stride)+static_cast<std::size_t>(x)*3;
        if(kind==NoteKind::flick)return rgb[i]>180&&rgb[i+2]>150&&rgb[i+1]<180&&
            static_cast<int>(rgb[i])-rgb[i+1]>60&&static_cast<int>(rgb[i+2])-rgb[i+1]>40;
        return rgb[i]>160&&rgb[i+1]>140&&rgb[i+2]<150&&
            static_cast<int>(rgb[i])-rgb[i+2]>60&&static_cast<int>(rgb[i+1])-rgb[i+2]>50;
    };
    for(int y=30;y<570;++y) {
        int x=0;
        while(x<width) {
            while(x<width&&!yellow(x,y))++x;
            if(x==width)break;
            const int begin=x;while(x<width&&yellow(x,y))++x;
            const int length=x-begin;
            if(length<40){++result.ignored_short_runs;continue;}
            if(length>180){++result.ignored_long_runs;continue;}
            if(++result.qualifying_runs>4096)return fail("yellow_run_budget_exhausted");
            const int center_x2=begin+x-1;std::size_t selected=groups.size(),compatible=0;
            for(std::size_t i=0;i<groups.size();++i)if(groups[i].last_y>=y-1&&std::abs(groups[i].last_x2-center_x2)<=4) {
                selected=i;++compatible;
            }
            if(compatible>1)return fail("ambiguous_yellow_run_grouping");
            if(!compatible) {if(groups.size()>=128)return fail("yellow_group_budget_exhausted");groups.emplace_back();selected=groups.size()-1;}
            auto& group=groups[selected];if(group.widths.size()>=540)return fail("yellow_group_row_budget_exhausted");
            group.last_y=y;group.last_x2=center_x2;group.sum_x2+=center_x2;group.sum_y+=y;group.widths.push_back(length);
        }
    }
    result.groups=groups.size();
    for(auto& group:groups)if(group.widths.size()>=2) {
        if(result.centers.size()>=8)return fail("too_many_yellow_centers");
        const auto rows=group.widths.size();const auto count=static_cast<double>(rows);
        const double x=static_cast<double>(group.sum_x2)/(2*count),y=static_cast<double>(group.sum_y)/count;
        std::sort(group.widths.begin(),group.widths.end());const auto mid=rows/2;
        const auto median=rows%2?static_cast<double>(group.widths[mid]):(static_cast<double>(group.widths[mid-1])+group.widths[mid])/2;
        result.centers.push_back({std::string(kind==NoteKind::drag?"yellow-":"pink-")+std::to_string(result.centers.size()),x,y});result.metrics.push_back({x,y,median,rows});
    }
    if(result.centers.empty())return fail("no_supported_yellow_centers");
    result.valid=true;result.reason="bounded_yellow_run_centers;completeness_unverified";return result;
}

// Explicit head bars supplement the preserved horizontal detector. Components
// are built from bounded row runs, then checked in their own principal axes.
// A connected Hold body never replaces an already extracted horizontal head
// with the whole component's centroid. Complex rotated bodies remain unknown.
// The result still cannot establish coverage, occlusion, or native identity.
inline calibration::OpeningDetection detect_adaptive_opening_centers(
        int width,int height,int stride,std::span<const std::uint8_t> rgb,NoteKind kind) {
    auto result=detect_adaptive_horizontal_opening_centers(width,height,stride,rgb,kind);
    if(width!=1280||height!=720||stride<3840||stride>16384||rgb.size()!=static_cast<std::size_t>(stride)*720||
       (kind!=NoteKind::tap&&kind!=NoteKind::hold&&kind!=NoteKind::drag&&kind!=NoteKind::flick))return result;
    if(!result.valid&&result.reason!="no_supported_cyan_centers"&&result.reason!="no_supported_yellow_centers")return result;
    const auto fail=[&](const char* reason){result.valid=false;result.reason=reason;result.centers.clear();result.metrics.clear();return result;};
    const auto colored=[&](int x,int y){
        const auto at=static_cast<std::size_t>(y)*static_cast<std::size_t>(stride)+static_cast<std::size_t>(x)*3;
        if(kind==NoteKind::tap||kind==NoteKind::hold)return rgb[at]<80&&rgb[at+1]>140&&rgb[at+2]>185;
        if(kind==NoteKind::flick)return rgb[at]>180&&rgb[at+2]>150&&rgb[at+1]<180&&
            static_cast<int>(rgb[at])-rgb[at+1]>60&&static_cast<int>(rgb[at+2])-rgb[at+1]>40;
        return rgb[at]>160&&rgb[at+1]>140&&rgb[at+2]<150&&
            static_cast<int>(rgb[at])-rgb[at+2]>60&&static_cast<int>(rgb[at+1])-rgb[at+2]>50;
    };
    struct Run {int y=0,a=0,b=0;std::size_t parent=0;};
    std::vector<Run> runs;runs.reserve(2048);
    const auto root=[&](std::size_t i){
        auto r=i;while(runs[r].parent!=r)r=runs[r].parent;
        while(runs[i].parent!=i){const auto next=runs[i].parent;runs[i].parent=r;i=next;}return r;
    };
    std::size_t previous_begin=0,previous_end=0;
    for(int y=30;y<570;++y){
        const auto current_begin=runs.size();std::size_t previous=previous_begin;int x=0;
        while(x<1280){
            while(x<1280&&!colored(x,y))++x;if(x==1280)break;
            const auto begin=x;while(x<1280&&colored(x,y))++x;
            if(runs.size()>=16384)return fail("oriented_head_run_budget_exhausted");
            const auto index=runs.size();runs.push_back({y,begin,x-1,index});
            while(previous<previous_end&&runs[previous].b+1<begin)++previous;
            for(auto p=previous;p<previous_end&&runs[p].a<=x;++p){
                const auto a=root(index),b=root(p);if(a!=b)runs[std::max(a,b)].parent=std::min(a,b);
            }
        }
        previous_begin=current_begin;previous_end=runs.size();
    }
    struct Component {
        double count=0,sx=0,sy=0,sxx=0,syy=0,sxy=0,ux=1,uy=0;
        double umin=1e20,umax=-1e20,vmin=1e20,vmax=-1e20;
        int xmin=1280,xmax=-1,ymin=720,ymax=-1;
        std::vector<std::size_t> horizontal_heads;
    };
    std::vector<Component> components(runs.size());std::size_t component_count=0;
    const auto squares=[](double n){return n*(n+1)*(2*n+1)/6;};
    for(std::size_t i=0;i<runs.size();++i){
        const auto& r=runs[i];auto& c=components[root(i)];if(c.count==0&&++component_count>4096)return fail("oriented_head_component_budget_exhausted");
        const double count=r.b-r.a+1,sum=(r.a+r.b)*count/2;
        c.count+=count;c.sx+=sum;c.sy+=count*r.y;c.sxx+=squares(r.b)-squares(r.a-1);c.syy+=count*r.y*r.y;c.sxy+=sum*r.y;
        c.xmin=std::min(c.xmin,r.a);c.xmax=std::max(c.xmax,r.b);c.ymin=std::min(c.ymin,r.y);c.ymax=std::max(c.ymax,r.y);
    }
    for(auto& c:components)if(c.count>=80){
        const auto x=c.sx/c.count,y=c.sy/c.count;
        const auto angle=.5*std::atan2(2*(c.sxy/c.count-x*y),(c.sxx-c.syy)/c.count-x*x+y*y);
        c.ux=std::cos(angle);c.uy=std::sin(angle);
    }
    for(std::size_t i=0;i<runs.size();++i){
        const auto& r=runs[i];auto& c=components[root(i)];if(c.count<80)continue;
        for(const auto x:{r.a,r.b}){
            const auto u=c.ux*x+c.uy*r.y,v=-c.uy*x+c.ux*r.y;
            c.umin=std::min(c.umin,u);c.umax=std::max(c.umax,u);c.vmin=std::min(c.vmin,v);c.vmax=std::max(c.vmax,v);
        }
    }
    for(std::size_t h=0;h<result.centers.size();++h){
        const auto& center=result.centers[h];std::set<std::size_t> owners;
        for(std::size_t i=0;i<runs.size();++i){const auto& r=runs[i];if(std::abs(r.y-center.y)<=2&&center.x>=r.a-2&&center.x<=r.b+2)owners.insert(root(i));}
        if(owners.size()!=1)return fail("horizontal_head_component_ownership_ambiguous");
        components[*owners.begin()].horizontal_heads.push_back(h);
        const auto& metric=result.metrics[h];
        if(metric.rows>18||metric.width<4*static_cast<double>(metric.rows))return fail("horizontal_head_body_extent_ambiguous");
        if(center.x-metric.width/2<=0||center.x+metric.width/2>=1279||
           center.y-static_cast<double>(metric.rows)/2<=30||center.y+static_cast<double>(metric.rows)/2>=569)return fail("clipped_horizontal_head");
        for(std::size_t i=0;i<runs.size();++i){
            const auto& r=runs[i];
            if(root(i)==*owners.begin()&&std::abs(r.y-center.y)<=static_cast<double>(metric.rows)/2+2&&
               r.b>=center.x-metric.width/2-2&&r.a<=center.x+metric.width/2+2&&
               (r.a==0||r.b==1279||r.y==30||r.y==569))return fail("clipped_horizontal_head_band");
        }
    }
    const auto minimum=(kind==NoteKind::tap||kind==NoteKind::hold)?70.0:40.0;
    for(const auto& c:components)if(c.count>=80){
        if(c.horizontal_heads.size()>1)return fail("merged_horizontal_head_component");
        if(!c.horizontal_heads.empty())continue; // Preserve the explicit head, including connected long Hold bodies.
        const auto pixel_extent=std::abs(c.ux)+std::abs(c.uy);
        const auto major=c.umax-c.umin+pixel_extent,minor=c.vmax-c.vmin+pixel_extent;
        if(major<minimum||major>180||minor<2||minor>18||major<4*minor)continue;
        if(c.xmin==0||c.xmax==1279||c.ymin==30||c.ymax==569)return fail("clipped_oriented_head");
        if(c.count/(major*minor)<.72)return fail("nonrectangular_or_merged_oriented_head");
        const auto u=(c.umin+c.umax)/2,v=(c.vmin+c.vmax)/2;
        const auto x=c.ux*u-c.uy*v,y=c.uy*u+c.ux*v;
        if(std::any_of(result.centers.begin(),result.centers.end(),[&](const auto& p){return std::hypot(p.x-x,p.y-y)<=3;}))continue;
        if(result.centers.size()>=8)return fail("too_many_oriented_head_centers");
        result.centers.push_back({std::string(kind==NoteKind::drag?"yellow-":kind==NoteKind::flick?"pink-":"cyan-")+std::to_string(result.centers.size()),x,y});
        result.metrics.push_back({x,y,major,static_cast<std::size_t>(c.ymax-c.ymin+1)});
    }
    result.groups=component_count;
    if(result.centers.empty())return fail("no_supported_typed_head_centers");
    result.valid=true;result.reason="typed_rle_oriented_head_centers;completeness_unverified";return result;
}

// All time branches and one-to-one assignments inside the native pre-judgment
// domain are retained. Rotating native pieces use an affine chord with an
// explicit second-derivative enclosure, not a time-grid/nearest-time choice.
// A broad enclosure can keep ambiguity and prevent arming; it never authorizes
// silently dropping a visible alias. Rendering is still an unqualified model.
class AdaptiveFirstNoteTrigger {
    static constexpr Ns ms=1'000'000,second=1'000'000'000,safe=(Ns{1}<<53)-1;
    // A closed contact needs at least two of the runtime's 20000 steps.
    // Keep independent equation/work budgets; never discard aliases to fit.
    static constexpr std::size_t max_notes=20000/2,max_pieces=2048,max_equations=300000,max_candidates=1024,max_work=2'000'000;
    struct Range {long double lo=0,hi=0;};
    struct Axis {Range base,slope;};
    struct Equation {Axis x,y;bool positive=false;std::string speed_pointer;double line_speed=0;};
    struct NativeNote {SemanticNote note;std::size_t line=0;};
    struct Piece {VisualTimeInterval time;std::vector<Equation> equations;std::vector<std::size_t> note_indices;};
    struct Support {std::vector<VisualTimeCandidate> candidates;VisualCandidateFrame frame;};
    AdaptiveOpeningDescription description_;
    Ns phase_=0,last_now_=-1,last_received_=-1;
    std::uint64_t last_sequence_=0;
    std::size_t first_note_=0,work_=0;
    Ns native_zero_=0;
    std::string context_;
    std::vector<NativeNote> notes_;
    std::vector<Piece> pieces_;
    std::set<std::string> evidence_ids_,frame_ids_;
    std::optional<Support> previous_;
    FirstNoteTriggerStatus value_;
    bool consumed_=false;
    static long double down(long double v){return std::nextafter(v,-std::numeric_limits<long double>::infinity());}
    static long double up(long double v){return std::nextafter(v,std::numeric_limits<long double>::infinity());}
    static Range add(Range a,Range b){return {down(a.lo+b.lo),up(a.hi+b.hi)};}
    static Range sub(Range a,Range b){return add(a,{-b.hi,-b.lo});}
    static Range mul(Range a,Range b) {
        const long double v[]{a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};const auto [lo,hi]=std::minmax_element(std::begin(v),std::end(v));
        return {down(*lo),up(*hi)};
    }
    static Range point(long double v){return {v,v};}
    static bool text(const std::string& s,std::size_t max=512){return !s.empty()&&s.size()<=max;}
    static bool valid(VisualTimeInterval i){return i.lower<=i.upper;}
    static VisualTimeInterval intersect(VisualTimeInterval a,VisualTimeInterval b){return {std::max(a.lower,b.lower),std::min(a.upper,b.upper)};}
    static void require(bool ok,const char* why){if(!ok)throw std::invalid_argument(why);}
    void bump(){if(++work_>max_work)throw std::runtime_error("adaptive_opening_work_budget");}
    static Ns midpoint(VisualTimeInterval i){return i.lower+(i.upper-i.lower)/2;}
    static std::vector<VisualTimeInterval> merge(std::vector<VisualTimeInterval> in) {
        std::sort(in.begin(),in.end(),[](auto a,auto b){return std::pair(a.lower,a.upper)<std::pair(b.lower,b.upper);});
        std::vector<VisualTimeInterval> out;
        for(auto i:in)if(valid(i)) {if(!out.empty()&&i.lower<=out.back().upper+1)out.back().upper=std::max(out.back().upper,i.upper);else out.push_back(i);}
        return out;
    }
    static std::string context(const VisualCandidateFrame& f) {
        std::string s;for(const auto* v:{&f.source_id,&f.source_sha256,&f.clock_id,&f.opening_context_id})s+=std::to_string(v->size())+":"+*v;
        return s+":"+std::to_string(f.epoch)+":"+std::to_string(f.generation)+":"+std::to_string(f.geometry_version);
    }
    // Outward roots for possible matches, inward roots for definite visibility.
    // Each inequality uses its proper interval endpoint; no branch is removed
    // solely because a floating-point nominal root falls across a boundary.
    static VisualTimeInterval bound(VisualTimeInterval domain,Ns origin,long double base,long double slope,
                                   long double target,bool ge,bool guaranteed) {
        if(!valid(domain))return domain;
        if(slope==0)return (ge?base>=target:base<=target)?domain:VisualTimeInterval{1,0};
        const Range numerator=sub(point(target),point(base));
        const auto divided=mul(numerator,{down(1/slope),up(1/slope)});
        const auto root=add(point(static_cast<long double>(origin)),divided);const bool lower=ge==(slope>0);
        const auto edge=lower?(guaranteed?root.hi:root.lo):(guaranteed?root.lo:root.hi);
        if(lower) {
            if(edge>domain.upper)return {1,0};
            if(edge>domain.lower)domain.lower=std::max(domain.lower,static_cast<Ns>(std::ceil(edge)));
        } else {
            if(edge<domain.lower)return {1,0};
            if(edge<domain.upper)domain.upper=std::min(domain.upper,static_cast<Ns>(std::floor(edge)));
        }
        return domain;
    }
    static VisualTimeInterval axis_match(const Axis& axis,VisualTimeInterval domain,Ns origin,long double lo,long double hi,bool guaranteed=false) {
        domain=bound(domain,origin,guaranteed?axis.base.lo:axis.base.hi,guaranteed?axis.slope.lo:axis.slope.hi,lo,true,guaranteed);
        return bound(domain,origin,guaranteed?axis.base.hi:axis.base.lo,guaranteed?axis.slope.hi:axis.slope.lo,hi,false,guaranteed);
    }
    static Range increment(double speed,Ns dt){return mul(point(speed),{down(static_cast<long double>(dt)/second),up(static_cast<long double>(dt)/second)});}
    Range floor_at(const ChartLine& line,Ns at) const {
        Range floor=point(0);Ns cursor=native_zero_;
        for(const auto& e:line.speed_events) {
            require(e.time.start_ns==cursor&&e.time.end_ns>=cursor&&std::isfinite(e.value)&&std::abs(e.value)<=1e6,
                    "adaptive opening speed coverage/values unsupported");
            if(e.floor_position)floor=point(*e.floor_position);
            // The declared native-zero boundary holds its initial floor before
            // the first speed interval. No host/planner timestamp enters it.
            if(at<cursor)return floor;
            if(e.time.end_ns==cursor)continue;
            if(at<e.time.end_ns)return add(floor,increment(e.value,at-cursor));
            floor=add(floor,increment(e.value,e.time.end_ns-cursor));cursor=e.time.end_ns;
        }
        throw std::invalid_argument("adaptive opening speed coverage missing");
    }
    static const SpeedLineEvent& speed_at(const ChartLine& line,Ns at) {
        if(!line.speed_events.empty()&&at<line.speed_events.front().time.start_ns)return line.speed_events.front();
        for(const auto& e:line.speed_events)if(e.time.start_ns<=at&&at<e.time.end_ns)return e;
        throw std::invalid_argument("adaptive opening native speed piece missing");
    }
    void reset(const char* why){previous_.reset();value_.anchor.reset();value_.support_count=0;value_.state=FirstNoteTriggerState::acquiring;value_.reason=why;}
    bool health(Ns now,bool healthy) {
        if(value_.state==FirstNoteTriggerState::cancelled||value_.state==FirstNoteTriggerState::fired)return false;
        if(now<0||now>safe||(last_now_>=0&&now<last_now_)){cancel("host_clock_invalid_or_rollback");return false;}last_now_=now;
        if(!healthy){reset("waiting_for_healthy_actual_UI");return false;}
        if(value_.state==FirstNoteTriggerState::ready&&value_.anchor&&
           (now-value_.anchor->received_host_ns>=100*ms||value_.anchor->target_host_ns-now<200*ms))reset("first_note_readiness_expired");
        return true;
    }
    std::vector<VisualTimeCandidate> solve(const VisualCandidateFrame& f) {
        std::vector<VisualTimeCandidate> out;
        for(const auto& piece:pieces_) {
            struct Choice {std::size_t note;VisualTimeInterval time;};
            std::vector<std::vector<Choice>> choices(f.centers.size());
            std::vector<VisualTimeInterval> exclusions;
            for(const auto& e:piece.equations) {
                // 180x18px head half diagonal <91px, plus 3px detector/model
                // error and one strict pixel at the detector's ROI boundary.
                auto inside=axis_match(e.x,piece.time,piece.time.lower,95,1184,true);
                inside=axis_match(e.y,inside,piece.time.lower,125,474,true);exclusions.push_back(inside);
            }
            bool missing=false;
            for(std::size_t i=0;i<f.centers.size();++i) {
                const auto& center=f.centers[i];
                for(std::size_t n=0;n<piece.equations.size();++n) {
                    bump();const auto& e=piece.equations[n];
                    auto domain=axis_match(e.x,piece.time,piece.time.lower,center.x-3,center.x+3);
                    domain=axis_match(e.y,domain,piece.time.lower,center.y-3,center.y+3);
                    if(valid(domain))choices[i].push_back({piece.note_indices[n],domain});
                }
                if(choices[i].empty())missing=true;
            }
            if(missing)continue;
            std::vector<bool> used(notes_.size(),false);std::vector<VisualNoteMatch> matches;
            const auto visit=[&](auto&& self,std::size_t index,VisualTimeInterval domain)->void {
                bump();
                if(index==choices.size()) {
                    std::vector<VisualTimeInterval> parts{domain};
                    for(std::size_t n=0;n<piece.equations.size()&&!parts.empty();++n)if(!used[piece.note_indices[n]]&&valid(exclusions[n])) {
                        std::vector<VisualTimeInterval> rest;
                        for(auto p:parts) {
                            bump();const auto cut=intersect(p,exclusions[n]);
                            if(!valid(cut))rest.push_back(p);else {
                                if(p.lower<cut.lower)rest.push_back({p.lower,cut.lower-1});
                                if(cut.upper<p.upper)rest.push_back({cut.upper+1,p.upper});
                            }
                        }
                        parts=std::move(rest);if(parts.size()>max_candidates)throw std::runtime_error("adaptive_opening_candidate_budget");
                    }
                    for(auto p:parts) {
                        const auto first=std::find(piece.note_indices.begin(),piece.note_indices.end(),first_note_);
                        const auto equation_index=first==piece.note_indices.end()?std::size_t{0}:static_cast<std::size_t>(first-piece.note_indices.begin());
                        const auto& e=piece.equations[equation_index];out.push_back({p,e.speed_pointer,e.line_speed,matches});
                        if(out.size()>max_candidates)throw std::runtime_error("adaptive_opening_candidate_budget");
                    }
                    return;
                }
                for(const auto& choice:choices[index])if(!used[choice.note]) {
                    bump();const auto next=intersect(domain,choice.time);if(!valid(next))continue;
                    used[choice.note]=true;const auto& note=notes_[choice.note].note;
                    matches.push_back({f.centers[index].id,note.source.id,note.source.json_pointer});
                    self(self,index+1,next);matches.pop_back();used[choice.note]=false;
                }
            };
            visit(visit,0,piece.time);
        }
        return out;
    }
public:
    AdaptiveFirstNoteTrigger(const SemanticChart& chart,Ns phase_ns,Ns first_down_ns=-1):phase_(phase_ns) {
        require(phase_ns>=-100*ms&&phase_ns<=100*ms,"first-note phase must be +/-100ms");
        require(chart.format_version==3&&std::isfinite(chart.offset_seconds)&&sha256_text(chart.source.chart_sha256)&&
                !chart.lines.empty()&&chart.lines.size()<=64&&chart.note_count>0&&chart.note_count<=max_notes,"adaptive opening chart scope unsupported");
        const SemanticNote* first=nullptr;std::size_t first_line=0,actual_notes=0;
        for(std::size_t l=0;l<chart.lines.size();++l)for(const auto& n:chart.lines[l].notes) {
            require(++actual_notes<=max_notes,"adaptive opening actual note budget derived from closed-contact steps");
            if(!first||n.start_ns<first->start_ns){first=&n;first_line=l;}
        }
        require(actual_notes==chart.note_count,"adaptive opening native note count mismatch");
        require(first&&first->start_ns>0&&first->start_ns<=60*second,"adaptive opening needs a future native earliest judgment");
        description_.end_ns=description_.note_time_ns=first->start_ns;description_.line_index=first_line;
        description_.note_kind=first->kind;description_.note_id=first->source.id;description_.note_json_pointer=first->source.json_pointer;
        bool first_flick=false;
        for(const auto& line:chart.lines)for(const auto& n:line.notes)if(n.start_ns==first->start_ns) {
            description_.earliest_note_ids.push_back(n.source.id);first_flick|=n.kind==NoteKind::flick;
        }
        description_.earliest_note_count=description_.earliest_note_ids.size();
        description_.first_down_ns=first_down_ns<0?first->start_ns-(first_flick?30*ms:0):first_down_ns;
        require(description_.first_down_ns>=0&&description_.first_down_ns<=first->start_ns,"adaptive first DOWN outside native earliest judgment");
        native_zero_=phigros_time_ns(0,chart.lines.front().bpm,chart.offset_seconds);
        std::set<Ns> cuts{0,first->start_ns};std::set<std::size_t> lines;std::set<std::string> ids;
        const auto color=[](NoteKind k){return k==NoteKind::drag?2:k==NoteKind::flick?4:1;};
        for(std::size_t l=0;l<chart.lines.size();++l)for(const auto& n:chart.lines[l].notes) {
            if(color(n.kind)!=color(first->kind))continue;
            require(n.start_ns>=first->start_ns&&std::isfinite(n.position_x)&&std::abs(n.position_x)<=100&&
                    std::isfinite(n.floor_position)&&std::abs(n.floor_position)<=1e9&&std::isfinite(n.speed)&&std::abs(n.speed)<=10000,
                    "adaptive opening native note values unsupported");
            const auto pointer="/judgeLineList/"+std::to_string(l)+(n.side==NoteSide::above?"/notesAbove/":"/notesBelow/")+std::to_string(n.source_index);
            require(n.source.json_pointer==pointer&&n.source.id==chart.source.chart_sha256+'#'+pointer&&ids.insert(n.source.id).second,
                    "adaptive opening native note provenance invalid");
            if(&n==first)first_note_=notes_.size();notes_.push_back({n,l});lines.insert(l);
            require(notes_.size()<=max_notes,"adaptive opening same-color note budget");
        }
        const auto add_cut=[&](Ns t){if(t>0&&t<first->start_ns)cuts.insert(t);};add_cut(native_zero_);
        for(auto l:lines) {
            const auto& line=chart.lines[l];require(line.source_index==l&&line.speed_events.size()<=4096,"adaptive opening line/speed budget");
            require(line.move_events.size()+line.rotate_events.size()+line.opacity_events.size()<=65536,"adaptive opening pose budget");
            for(const auto& e:line.move_events){add_cut(e.time.start_ns);add_cut(e.time.end_ns);}
            for(const auto& e:line.rotate_events){add_cut(e.time.start_ns);add_cut(e.time.end_ns);}
            for(const auto& e:line.speed_events){add_cut(e.time.start_ns);add_cut(e.time.end_ns);}
        }
        require(cuts.size()<=max_pieces+1,"adaptive opening piece budget");
        PhigrosPoseEvaluator pose(chart);const std::vector<Ns> boundaries(cuts.begin(),cuts.end());
        std::size_t evaluated=0;
        for(std::size_t i=1;i<boundaries.size();++i) {
            Piece piece;piece.time={boundaries[i-1],boundaries[i]-1};const auto dt=piece.time.upper-piece.time.lower;
            for(std::size_t ni=0;ni<notes_.size();++ni) {
                require(++evaluated<=max_work,"adaptive opening constructor work budget");
                const auto& native=notes_[ni];const auto& line=chart.lines[native.line];const auto& n=native.note;
                const auto a=pose.line_at(native.line,piece.time.lower),b=pose.line_at(native.line,piece.time.upper);
                const auto& speed=speed_at(line,piece.time.lower);
                const auto fa=floor_at(line,piece.time.lower),fb=floor_at(line,piece.time.upper);
                const auto sign=n.side==NoteSide::above?1.0L:-1.0L;
                const auto scale=description_.approach_pixels_per_floor_unit*n.speed*sign;
                const auto za=mul(sub(fa,point(n.floor_position)),point(scale));
                const auto zb=mul(sub(fb,point(n.floor_position)),point(scale));
                const auto along=n.position_x*72;
                const auto position=[&](const PhigrosLinePose& p,Range z) {
                    return std::array<Range,2>{add(point(p.origin.x*1280+along*p.tangent.x),mul(z,point(-p.above_normal.x))),
                                              add(point(p.origin.y*720+along*p.tangent.y),mul(z,point(-p.above_normal.y)))};
                };
                const auto pa=position(a,za),pb=position(b,zb);
                const auto angle=std::abs(static_cast<long double>(b.rotation_degrees)-a.rotation_degrees)*std::numbers::pi_v<long double>/180;
                const auto zmax=std::max({std::abs(za.lo),std::abs(za.hi),std::abs(zb.lo),std::abs(zb.hi)});
                const auto zdiff=std::max(std::abs(zb.hi-za.lo),std::abs(zb.lo-za.hi));
                // A linear origin plus (along,z(t)) rotated by linear theta(t)
                // has chord error <= max|p''|*dt^2/8 on each native piece.
                // This enclosure covers every interior time, including aliases;
                // it is deliberately wider than a sampled trajectory.
                const auto curve=up(angle*angle*(std::abs(along)+zmax)/8+angle*zdiff/4);
                const auto magnitude=std::max({1.0L,zmax,std::abs(pa[0].lo),std::abs(pa[1].lo),std::abs(pb[0].hi),std::abs(pb[1].hi)});
                const auto padding=up(0.000001L+64*std::numeric_limits<double>::epsilon()*magnitude+curve);
                require(std::isfinite(padding),"adaptive opening curvature enclosure overflow");
                const auto axis=[&](Range start,Range finish) {
                    const auto middle=(start.lo+start.hi)/2;
                    const auto slope=dt?(finish.lo+finish.hi-start.lo-start.hi)/(2*dt):0;
                    const auto slope_pad=dt?(start.hi-start.lo+finish.hi-finish.lo)/dt:0;
                    return Axis{{down(middle-padding),up(middle+padding)},{down(slope-slope_pad),up(slope+slope_pad)}};
                };
                Equation equation{axis(pa[0],pb[0]),axis(pa[1],pb[1]),true,speed.source.json_pointer,speed.value};
                auto possible=axis_match(equation.x,piece.time,piece.time.lower,-3,1283);
                possible=axis_match(equation.y,possible,piece.time.lower,27,573);
                if(!valid(possible)){++description_.provably_invisible_equations;continue;}
                require(++description_.retained_equations<=max_equations,"adaptive opening retained equation storage budget");
                piece.note_indices.push_back(ni);piece.equations.push_back(std::move(equation));
            }
            pieces_.push_back(std::move(piece));
        }
        description_.native_pieces=pieces_.size();description_.modeled_notes=notes_.size();
        std::ostringstream canonical;canonical<<description_.model_id<<'\n'<<chart.source.chart_sha256<<'\n'
            <<chart.import_policy_id<<'\n'<<chart.import_interpretation_unqualified<<'\n';
        for(const auto& anomaly:chart.import_anomalies)canonical<<anomaly.source.id<<'\n'<<anomaly.code<<'\n'<<anomaly.original_declaration_json<<'\n';
        for(const auto& id:description_.earliest_note_ids)canonical<<id<<'\n';
        canonical<<description_.note_time_ns<<'\n'<<description_.first_down_ns<<'\n'<<description_.detector_policy_id<<'\n'<<std::setprecision(17)
            <<description_.approach_pixels_per_floor_unit<<"\npixel_error=3;chord_curvature_enclosure;native_zero_floor_hold;full_head_visibility=95,1184,125,474;lead=200ms";
        description_.model_sha256=chart_text_sha256(canonical.str());value_.reason="waiting_for_native_earliest_group_motion";
    }
    const AdaptiveOpeningDescription& description()const{return description_;}
    FirstNoteTriggerStatus observe(const VisualCandidateFrame& f,bool ui_healthy,Ns now) {
        if(!health(now,ui_healthy))return value_;
        if(!(f.sequence&&f.epoch&&f.generation&&f.geometry_version&&f.source_valid&&f.centers_complete_unoccluded&&
             f.width==1280&&f.height==720&&f.clock_kind==VisualSourceClock::arrival_only&&!f.source_time_ns&&!f.capture_host_interval&&
             text(f.source_id)&&sha256_text(f.source_sha256)&&text(f.clock_id)&&text(f.opening_context_id)&&text(f.frame_id)&&
             text(f.evidence_id)&&sha256_text(f.frame_sha256)&&f.received_host_ns>=0&&f.received_host_ns<=now&&
             now-f.received_host_ns<100*ms&&f.centers.size()<=8)) {cancel("invalid_or_stale_first_note_provenance");return value_;}
        const auto current_context=context(f);
        if(!context_.empty()&&current_context!=context_){cancel("first_note_source_epoch_or_geometry_changed");return value_;}
        if(f.sequence<=last_sequence_||f.received_host_ns<=last_received_||evidence_ids_.contains(f.evidence_id)||frame_ids_.contains(f.frame_id)) {
            cancel("replayed_or_nonmonotonic_first_note_frame");return value_;
        }
        if(evidence_ids_.size()>=512){cancel("first_note_observation_budget");return value_;}
        context_=current_context;last_sequence_=f.sequence;last_received_=f.received_host_ns;evidence_ids_.insert(f.evidence_id);frame_ids_.insert(f.frame_id);
        std::set<std::string> center_ids;
        for(const auto& c:f.centers)if(!text(c.id,128)||!std::isfinite(c.x)||!std::isfinite(c.y)||c.x<0||c.x>=1280||c.y<30||c.y>=570||!center_ids.insert(c.id).second) {
            cancel("invalid_first_note_centers");return value_;
        }
        value_.raw_candidates.clear();value_.raw_candidate_count=value_.first_note_positive_candidates=0;value_.selected_intervals.clear();value_.anchor.reset();
        if(f.centers.empty()){reset("waiting_for_native_first_note_centers");return value_;}
        try {work_=0;value_.raw_candidates=solve(f);}catch(const std::runtime_error& e){cancel(e.what());return value_;}
        value_.raw_candidate_count=value_.raw_candidates.size();
        const auto group_present=[&](const VisualTimeCandidate& candidate) {
            return std::all_of(description_.earliest_note_ids.begin(),description_.earliest_note_ids.end(),[&](const auto& id) {
                return std::any_of(candidate.matches.begin(),candidate.matches.end(),[&](const auto& m){return m.note_id==id;});
            });
        };
        for(const auto& c:value_.raw_candidates)if(group_present(c))++value_.first_note_positive_candidates;
        if(value_.raw_candidates.empty()){reset("native_earliest_group_missing_or_ambiguous");return value_;}
        Support current{value_.raw_candidates,f};const auto previous=std::move(previous_);previous_=current;
        value_.support_count=1;value_.state=FirstNoteTriggerState::acquiring;value_.reason="waiting_for_second_distinct_native_group_frame";
        if(!previous)return value_;
        const auto dt=f.received_host_ns-previous->frame.received_host_ns;
        if(dt<20*ms||dt>200*ms||f.frame_sha256==previous->frame.frame_sha256) {
            value_.reason="native_group_frames_not_distinct_or_too_far_apart";return value_;
        }
        const auto center_for=[](const VisualTimeCandidate& candidate,const VisualCandidateFrame& frame,const std::string& id)->const VisualCenter* {
            const auto m=std::find_if(candidate.matches.begin(),candidate.matches.end(),[&](const auto& v){return v.note_id==id;});
            if(m==candidate.matches.end())return nullptr;
            const auto c=std::find_if(frame.centers.begin(),frame.centers.end(),[&](const auto& v){return v.id==m->center_id;});
            return c==frame.centers.end()?nullptr:&*c;
        };
        std::vector<VisualTimeInterval> selected;bool unresolved=false;std::size_t pairs=0;
        for(const auto& c:current.candidates)for(const auto& p:previous->candidates) {
            if(++pairs>65536){cancel("adaptive_opening_pair_budget");return value_;}
            // Keep every rate-one-compatible native branch. Non-first aliases
            // are not discarded merely because a preferred source ID matches.
            if(c.song_time.upper<=p.song_time.lower)continue;
            const auto delta_lower=c.song_time.lower-p.song_time.upper,delta_upper=c.song_time.upper-p.song_time.lower;
            if(dt<delta_lower-60*ms||dt>delta_upper+60*ms)continue;
            auto interval=intersect(c.song_time,{p.song_time.lower+std::max<Ns>(1,dt-60*ms),p.song_time.upper+dt+60*ms});
            if(!valid(interval))continue;selected.push_back(interval);
            bool motion=false;
            for(const auto& id:description_.earliest_note_ids) {
                const auto* a=center_for(p,previous->frame,id);const auto* b=center_for(c,f,id);
                if(a&&b&&std::hypot(b->x-a->x,b->y-a->y)>6)motion=true;
            }
            if(!group_present(p)||!group_present(c)||!motion)unresolved=true;
        }
        value_.selected_intervals=merge(std::move(selected));
        if(unresolved||value_.selected_intervals.size()!=1||value_.selected_intervals[0].upper-value_.selected_intervals[0].lower>40*ms) {
            value_.reason="native_group_temporal_or_source_ambiguity_retained";return value_;
        }
        value_.support_count=2;FirstNoteTriggerAnchor anchor;anchor.received_host_ns=f.received_host_ns;
        anchor.chosen_song_interval=value_.selected_intervals[0];anchor.origin_host_ns=f.received_host_ns-midpoint(anchor.chosen_song_interval);
        anchor.target_host_ns=anchor.origin_host_ns+description_.first_down_ns+phase_;anchor.evidence_id=f.evidence_id;
        anchor.supporting_evidence_ids={previous->frame.evidence_id,f.evidence_id};
        if(anchor.origin_host_ns<0||anchor.target_host_ns>safe||anchor.target_host_ns-now<200*ms){cancel("first_note_dispatch_lead_below_200ms");return value_;}
        value_.anchor=std::move(anchor);value_.state=FirstNoteTriggerState::ready;
        value_.reason="two_distinct_native_group_motion_frames_on_unique_temporal_branch";return value_;
    }
    FirstNoteTriggerStatus status(Ns now,bool healthy=true){health(now,healthy);return value_;}
    std::optional<FirstNoteTriggerAnchor> take_anchor(Ns now) {
        if(consumed_||!health(now,true)||value_.state!=FirstNoteTriggerState::ready||!value_.anchor)return std::nullopt;
        consumed_=true;value_.state=FirstNoteTriggerState::fired;value_.reason="first_note_trigger_fired_once";return value_.anchor;
    }
    void cancel(std::string reason){if(value_.state==FirstNoteTriggerState::cancelled)return;value_.state=FirstNoteTriggerState::cancelled;value_.reason=reason.substr(0,512);previous_.reset();value_.anchor.reset();}
};
} // namespace rhythm::experimental
