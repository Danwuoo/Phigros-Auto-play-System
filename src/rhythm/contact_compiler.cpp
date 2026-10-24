#include "rhythm/contact_compiler.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace rhythm {
namespace {
constexpr double pi = 3.1415926535897932384626433832795;
constexpr double raster_error = 1.4142135623730950488;
struct Failure : std::runtime_error {
    std::string code;
    SongNs time;
    std::optional<PosePoint> point;
    Failure(std::string code_, std::string message, SongNs time_ = 0,
            std::optional<PosePoint> point_ = {})
        : std::runtime_error(std::move(message)), code(std::move(code_)), time(time_), point(point_) {}
};
SongNs add(SongNs a, SongNs b) {
    if (b < 0 || a < 0 || a > std::numeric_limits<SongNs>::max()-b)
        throw Failure("invalid_time", "Contact time is negative or overflows", a);
    return a+b;
}
double norm(PosePoint p) { return std::hypot(p.x,p.y); }
struct Range { double lo, hi; };
Range plus(Range a, Range b) { return {a.lo+b.lo,a.hi+b.hi}; }
Range product(Range a, Range b) {
    const double values[]={a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};
    return {*std::min_element(values,values+4),*std::max_element(values,values+4)};
}
Range range(double a,double b) { return {std::min(a,b),std::max(a,b)}; }
Range trig(double a, double b, bool cosine) {
    auto bounds=range(a,b);
    if (bounds.hi-bounds.lo >= 2*pi) return {-1,1};
    // Reduce first, so huge native angles never overflow an integer conversion.
    const double first=std::remainder(bounds.lo,2*pi);
    const double last=first+(bounds.hi-bounds.lo);
    auto f=[cosine](double x){return cosine ? std::cos(x) : std::sin(x);};
    Range result=range(f(first),f(last));
    const double offset=cosine ? 0 : pi/2;
    const int k0=static_cast<int>(std::ceil((first-offset)/pi));
    const int k1=static_cast<int>(std::floor((last-offset)/pi));
    for(int k=k0;k<=k1;++k) {
        const double value=k%2==0 ? 1.0 : -1.0;
        result.lo=std::min(result.lo,value); result.hi=std::max(result.hi,value);
    }
    return result;
}
template<class Event> const Event* selected(const std::vector<Event>& events, SongNs time) {
    const auto it=std::upper_bound(events.begin(),events.end(),time,
        [](SongNs t,const Event& e){return t<e.time.start_ns;});
    if(it==events.begin()) return nullptr;
    const auto& event=*std::prev(it);
    return time<event.time.end_ns && event.time.start_ns<event.time.end_ns ? &event : nullptr;
}
struct Slope { PosePoint origin; double radians=0; };
Slope slope_at(const ChartLine& line, SongNs time) {
    Slope result;
    if(const auto* move=selected(line.move_events,time)) {
        const auto duration=static_cast<long double>(move->time.end_ns)-move->time.start_ns;
        result.origin={static_cast<double>((move->end_x-move->start_x)*phigros_reference_width/duration),
            static_cast<double>(-(move->end_y-move->start_y)*phigros_reference_height/duration)};
    }
    if(const auto* rotation=selected(line.rotate_events,time)) {
        const auto duration=static_cast<long double>(rotation->time.end_ns)-rotation->time.start_ns;
        result.radians=static_cast<double>((rotation->end-rotation->start)*pi/180/duration);
    }
    if(!std::isfinite(result.origin.x)||!std::isfinite(result.origin.y)||!std::isfinite(result.radians))
        throw Failure("invalid_motion", "Pose derivative is not finite", time);
    return result;
}
constexpr double projection_eta_floor=1e-6;
struct ProjectedPoint {
    PosePoint point;
    double correction=0;
    unsigned edge=16;
};
ProjectedPoint normal_intersection(PosePoint point,PosePoint normal,double bound,SongNs time) {
    double lower=-bound,upper=bound;
    unsigned lower_edge=0,upper_edge=0;
    const auto constrain=[&](double p,double n,double extent,unsigned low,unsigned high) {
        if(n==0) {
            if(p<0||p>extent)throw Failure("projection_unavailable","Normal line cannot intersect the canvas within the declared bound",time,point);
            return;
        }
        const double first=-p/n,last=(extent-p)/n;
        const double a=n>0 ? first : last,b=n>0 ? last : first;
        const auto a_edge=n>0 ? low : high,b_edge=n>0 ? high : low;
        if(a>lower) {lower=a;lower_edge=a_edge;}
        if(b<upper) {upper=b;upper_edge=b_edge;}
    };
    constrain(point.x,normal.x,phigros_reference_width,1,2);
    constrain(point.y,normal.y,phigros_reference_height,4,8);
    if(!std::isfinite(lower)||!std::isfinite(upper)||lower>upper)
        throw Failure("projection_unavailable","Bounded normal feasible set is empty",time,point);
    ProjectedPoint result;
    result.correction=lower>0 ? lower : upper<0 ? upper : 0;
    result.edge=lower>0 ? lower_edge : upper<0 ? upper_edge : 16;
    if(((result.edge==1||result.edge==2)&&std::abs(normal.x)<projection_eta_floor)||
       ((result.edge==4||result.edge==8)&&std::abs(normal.y)<projection_eta_floor))
        throw Failure("uncertified_normal_projection","Selected normal intersection is too near tangent for the declared conditioning proof",time,point);
    result.point={point.x+result.correction*normal.x,point.y+result.correction*normal.y};
    // This coordinate is the selected analytic slab intersection. Assign the
    // exact edge instead of allowing cancellation to put that edge a few ulps
    // outside. The other coordinate is still checked; no free x/y clamp exists.
    if(result.edge==1)result.point.x=0;
    if(result.edge==2)result.point.x=phigros_reference_width;
    if(result.edge==4)result.point.y=0;
    if(result.edge==8)result.point.y=phigros_reference_height;
    if(!std::isfinite(result.point.x)||!std::isfinite(result.point.y)||
       result.point.x<0||result.point.x>phigros_reference_width||result.point.y<0||result.point.y>phigros_reference_height)
        throw Failure("uncertified_normal_projection","Analytic normal intersection could not be represented inside both canvas slabs",time,point);
    return result;
}
Range quotient(Range numerator,Range denominator) {
    return product(numerator,range(1/denominator.lo,1/denominator.hi));
}
struct ProjectionCertificate {
    Range correction;
    unsigned edge_mask=16;
    double speed=0;
};
PhigrosLinePose advanced(PhigrosLinePose pose,Slope slope,SongNs elapsed) {
    pose.origin.x+=slope.origin.x*static_cast<double>(elapsed)/phigros_reference_width;
    pose.origin.y+=slope.origin.y*static_cast<double>(elapsed)/phigros_reference_height;
    pose.rotation_degrees+=slope.radians*static_cast<double>(elapsed)*180/pi;
    const auto angle=std::remainder(pose.rotation_degrees,360.0);
    if(angle==0) {pose.tangent={1,0};pose.above_normal={0,-1};}
    else if(angle==90) {pose.tangent={0,-1};pose.above_normal={-1,0};}
    else if(angle==-90) {pose.tangent={0,1};pose.above_normal={1,0};}
    else if(angle==180||angle==-180) {pose.tangent={-1,0};pose.above_normal={0,1};}
    else {const double rad=angle*pi/180;pose.tangent={std::cos(rad),-std::sin(rad)};pose.above_normal={-std::sin(rad),-std::cos(rad)};}
    return pose;
}
std::optional<ProjectionCertificate> certify_projection(const SemanticNote& note,const PhigrosLinePose& pose,Slope slope,
        double offset,double offset_slope,SongNs duration,double bound,double input_scale) {
    const double elapsed=static_cast<double>(duration);
    const double theta=std::remainder(pose.rotation_degrees,360.0)*pi/180;
    auto cs=trig(theta,theta+slope.radians*elapsed,true),sn=trig(theta,theta+slope.radians*elapsed,false);
    if(slope.radians==0) {cs={pose.tangent.x,pose.tangent.x};sn={-pose.tangent.y,-pose.tangent.y};}
    const double side=note.side==NoteSide::above ? 1 : -1;
    const auto nx=product({-side,-side},sn),ny=product({-side,-side},cs);
    const auto offsets=range(offset,offset+offset_slope*elapsed);
    const double along=note.position_x*phigros_position_x_pixels;
    const auto x=plus(range(pose.origin.x*phigros_reference_width,pose.origin.x*phigros_reference_width+slope.origin.x*elapsed),
        plus(product({along,along},cs),product(offsets,nx)));
    const auto y=plus(range(pose.origin.y*phigros_reference_height,pose.origin.y*phigros_reference_height+slope.origin.y*elapsed),
        plus(product({-along,-along},sn),product(offsets,ny)));
    const double max_offset=std::max(std::abs(offsets.lo),std::abs(offsets.hi));
    const double native_speed=norm(slope.origin)+(std::abs(along)+max_offset)*std::abs(slope.radians)+std::abs(offset_slope);
    if(x.lo>=0&&x.hi<=phigros_reference_width&&y.lo>=0&&y.hi<=phigros_reference_height)
        return ProjectionCertificate{{0,0},16,input_scale*native_speed*(1+1e-12)};
    struct Axis {Range lower{},upper{};double eta=0;unsigned low_edge=0,high_edge=0;bool active=false;};
    std::array<Axis,2> axes{};
    Range lower{-bound,-bound},upper{bound,bound};
    const auto axis=[&](std::size_t index,Range p,Range n,double extent,unsigned low_edge,unsigned high_edge) {
        // If the entire bounded normal tube is inside this slab, the slab can
        // neither block feasibility nor be an active closest-to-zero edge.
        const auto tube=plus(p,product({-bound,bound},n));
        if(tube.lo>=0&&tube.hi<=extent)return true;
        if(n.lo<=0&&n.hi>=0)return false;
        auto& out=axes[index];out.eta=std::min(std::abs(n.lo),std::abs(n.hi));
        if(out.eta<projection_eta_floor)return false;
        const auto zero_minus=Range{-p.hi,-p.lo},extent_minus=Range{extent-p.hi,extent-p.lo};
        out.lower=quotient(n.lo>0 ? zero_minus : extent_minus,n);
        out.upper=quotient(n.lo>0 ? extent_minus : zero_minus,n);
        out.low_edge=n.lo>0 ? low_edge : high_edge;out.high_edge=n.lo>0 ? high_edge : low_edge;out.active=true;
        lower={std::max(lower.lo,out.lower.lo),std::max(lower.hi,out.lower.hi)};
        upper={std::min(upper.lo,out.upper.lo),std::min(upper.hi,out.upper.hi)};
        return std::isfinite(lower.lo)&&std::isfinite(lower.hi)&&std::isfinite(upper.lo)&&std::isfinite(upper.hi);
    };
    if(!axis(0,x,nx,phigros_reference_width,1,2)||!axis(1,y,ny,phigros_reference_height,4,8)||lower.hi>upper.lo)return {};
    ProjectionCertificate certificate;
    certificate.correction={std::max(lower.lo,std::min(0.0,upper.lo)),std::max(lower.hi,std::min(0.0,upper.hi))};
    double speed=native_speed;
    certificate.edge_mask=certificate.correction.lo<=0&&certificate.correction.hi>=0 ? 16u : 0u;
    for(const auto& a:axes) if(a.active) {
        bool possible=false;
        if(lower.hi>0&&a.lower.hi>=std::max(0.0,lower.lo)) {certificate.edge_mask|=a.low_edge;possible=true;}
        if(upper.lo<0&&a.upper.lo<=std::min(0.0,upper.hi)) {certificate.edge_mask|=a.high_edge;possible=true;}
        if(possible) {
            const double correction=std::max(std::abs(certificate.correction.lo),std::abs(certificate.correction.hi));
            const double variation=native_speed+correction*std::abs(slope.radians);
            speed=std::max(speed,variation+variation/a.eta);
        }
    }
    certificate.speed=input_scale*speed*(1+1e-12);
    if(!std::isfinite(certificate.speed)||!certificate.edge_mask)return {};
    return certificate;
}
struct Build {
    const SemanticChart& chart;
    PhigrosPoseEvaluator poses;
    PhigrosGeometryMap mapping;
    ContactCompileOptions options;
    double input_scale;
    std::size_t visibility_checks=0;
    std::size_t projection_span_count=0;
    Build(const SemanticChart& chart_,PhigrosGeometry geometry,ContactCompileOptions options_)
        :chart(chart_),poses(chart_),mapping(geometry),options(std::move(options_)),input_scale(0) {
        const auto viewport=mapping.content_viewport();
        input_scale=std::max(viewport.width/phigros_reference_width,viewport.height/phigros_reference_height);
    }
    Point point(const PhigrosLinePose& pose,const SemanticNote& note,double offset,SongNs time) const {
        const auto target=phigros_line_point(pose,note.position_x,note.side,offset);
        const auto mapped=mapping.map_reference_pixels(target.reference_pixels);
        if(mapped.status!=GeometryStatus::mapped || !mapped.pixel)
            throw Failure("offscreen_target", "Required contact position is outside declared reference/input geometry",time,target.reference_pixels);
        // chart_memory and PAS use normalized pixel centers, p*(extent-1).
        return {static_cast<double>(mapped.pixel->x)/(mapping.geometry().input_width-1),
                static_cast<double>(mapped.pixel->y)/(mapping.geometry().input_height-1)};
    }
    double projection(const SemanticNote& note,std::size_t line) const {
        const auto pose=poses.line_at(line,note.start_ns);
        const auto target=phigros_line_point(pose,note.position_x,note.side);
        if(target.inside_reference) return 0;
        if(options.offscreen_policy==OffscreenPolicy::reject)
            throw Failure("offscreen_target", "Native judgment point is offscreen; no projection authorized",note.start_ns,target.reference_pixels);
        double lower=-options.max_normal_projection_reference_pixels;
        double upper=options.max_normal_projection_reference_pixels;
        const auto constrain=[&](double position,double normal,double extent) {
            if(std::abs(normal)<1e-14) { if(position<0||position>extent) lower=upper+1; return; }
            auto interval=range(-position/normal,(extent-position)/normal);
            lower=std::max(lower,interval.lo); upper=std::min(upper,interval.hi);
        };
        constrain(target.reference_pixels.x,target.side_normal.x,phigros_reference_width);
        constrain(target.reference_pixels.y,target.side_normal.y,phigros_reference_height);
        if(lower>upper) throw Failure("projection_unavailable", "Bounded normal candidate cannot reach reference canvas",note.start_ns,target.reference_pixels);
        return lower>0 ? lower : (upper<0 ? upper : 0);
    }
    void append(Contact& contact,SongNs time,Phase phase,Point point_,bool required=false) const {
        if(!contact.steps.empty()) {
            auto& previous=contact.steps.back();
            if(time<=previous.song_ns) throw Failure("collapsed_step_time", "Contact steps must have strictly increasing times",time);
            if(!required && phase==Phase::move && previous.point.x==point_.x && previous.point.y==point_.y) return;
        }
        if(contact.steps.size()>=options.max_steps_per_contact)
            throw Failure("trajectory_budget", "Contact trajectory exceeds configured step budget",time);
        contact.steps.push_back({time,phase,point_});
    }
    // Conservative continuous-canvas enclosure; subdivide a loose enclosure.
    // Unknown visibility is rejected instead of silently assuming the path stays in bounds.
    void visible(const SemanticNote& note,const PhigrosLinePose& pose,Slope slope,
                 double offset,double offset_slope,SongNs time,SongNs duration,unsigned depth=0) {
        if(++visibility_checks>options.max_steps_in_plan*16)
            throw Failure("visibility_budget", "Continuous path visibility proof exceeds budget",time);
        const double elapsed=static_cast<double>(duration);
        const double theta=std::remainder(pose.rotation_degrees,360.0)*pi/180;
        const auto cs=trig(theta,theta+slope.radians*elapsed,true);
        const auto sn=trig(theta,theta+slope.radians*elapsed,false);
        const double side=note.side==NoteSide::above ? 1 : -1;
        const auto normal=range(-side*offset,-side*(offset+offset_slope*elapsed));
        const double along=note.position_x*phigros_position_x_pixels;
        const auto x=plus(range(pose.origin.x*phigros_reference_width,
                               pose.origin.x*phigros_reference_width+slope.origin.x*elapsed),
                          plus(product({along,along},cs),product(normal,sn)));
        const auto y=plus(range(pose.origin.y*phigros_reference_height,
                               pose.origin.y*phigros_reference_height+slope.origin.y*elapsed),
                          plus(product({-along,-along},sn),product(normal,cs)));
        constexpr double epsilon=1e-10;
        if(x.lo>=-epsilon&&x.hi<=phigros_reference_width+epsilon&&y.lo>=-epsilon&&y.hi<=phigros_reference_height+epsilon) return;
        // Actual endpoint/midpoint checks distinguish a proven offscreen target
        // from an interval enclosure that simply could not be certified.
        const auto shifted=[&](SongNs amount) {
            auto p=pose;
            p.origin.x+=slope.origin.x*static_cast<double>(amount)/phigros_reference_width;
            p.origin.y+=slope.origin.y*static_cast<double>(amount)/phigros_reference_height;
            p.rotation_degrees+=slope.radians*static_cast<double>(amount)*180/pi;
            const double rad=std::remainder(p.rotation_degrees,360.0)*pi/180;
            p.tangent={std::cos(rad),-std::sin(rad)};
            p.above_normal={-std::sin(rad),-std::cos(rad)};
            return p;
        };
        for(auto amount:{SongNs{0},duration/2,duration})
            (void)point(shifted(amount),note,offset+offset_slope*static_cast<double>(amount),add(time,amount));
        if(depth>=24||duration<=1)
            throw Failure("uncertified_visibility", "Continuous path enclosure crosses canvas; calibration/geometry review required",time);
        const SongNs half=duration/2;
        visible(note,pose,slope,offset,offset_slope,time,half,depth+1);
        visible(note,shifted(half),slope,offset+offset_slope*static_cast<double>(half),offset_slope,
                add(time,half),duration-half,depth+1);
    }
    void trajectory(Contact& contact,const SemanticNote& note,std::size_t line_index,
                    SongNs first,SongNs last,double base_projection,bool flick) {
        const auto& line=chart.lines[line_index];
        std::vector<SongNs> boundaries={first,last};
        const auto collect=[&](const auto& events) {
            auto it=std::upper_bound(events.begin(),events.end(),first,
                [](SongNs t,const auto& e){return t<e.time.start_ns;});
            if(it!=events.begin()) {
                const auto end=std::prev(it)->time.end_ns;
                if(end>first&&end<last) boundaries.push_back(end);
            }
            for(;it!=events.end()&&it->time.start_ns<last;++it)
                for(auto t:{it->time.start_ns,it->time.end_ns})
                    if(t>first&&t<last) boundaries.push_back(t);
        };
        collect(line.move_events); collect(line.rotate_events);
        if(flick) boundaries.push_back(note.start_ns);
        std::sort(boundaries.begin(),boundaries.end());
        boundaries.erase(std::unique(boundaries.begin(),boundaries.end()),boundaries.end());
        const double offset_slope=flick ? options.flick_distance_reference_pixels/static_cast<double>(options.flick_travel_ns) : 0;
        const auto offset_at=[&](SongNs time) {
            return base_projection+offset_slope*static_cast<double>(static_cast<long double>(time)-note.start_ns);
        };
        const auto at=[&](SongNs time){return point(poses.line_at(line_index,time),note,offset_at(time),time);};
        for(std::size_t i=0;i+1<boundaries.size();++i) {
            const auto a=boundaries[i],b=boundaries[i+1];
            const auto pose=poses.line_at(line_index,a);
            const auto slope=slope_at(line,a);
            const double duration=static_cast<double>(b-a);
            const double max_offset=std::max(std::abs(offset_at(a)),std::abs(offset_at(b)));
            const double speed=input_scale*(norm(slope.origin)+
                (std::abs(note.position_x*phigros_position_x_pixels)+max_offset)*std::abs(slope.radians)+std::abs(offset_slope));
            if(!std::isfinite(speed)) throw Failure("invalid_motion", "Trajectory speed bound is not finite",a);
            visible(note,pose,slope,offset_at(a),offset_slope,a,b-a);
            if(a==first) append(contact,a,Phase::down,at(a),true);
            else append(contact,a,Phase::move,at(a),flick&&a==note.start_ns);
            if(speed==0) continue;
            const long double allowance=(options.max_path_error_pixels-raster_error)/speed;
            if(allowance<1) throw Failure("trajectory_resolution", "Requested error bound requires sub-nanosecond MOVE events",a);
            const SongNs delta=allowance>=duration ? b-a : static_cast<SongNs>(std::floor(allowance));
            for(SongNs t=a; b-t>delta;) {
                t+=delta;
                append(contact,t,Phase::move,at(t));
            }
        }
        append(contact,last,flick ? Phase::move : Phase::up,at(last),true);
        if(flick) append(contact,add(last,options.flick_release_ns),Phase::up,contact.steps.back().point,true);
    }
    ProjectedPoint projected(const PhigrosLinePose& pose,const SemanticNote& note,double offset,SongNs time) const {
        const auto source=phigros_line_point(pose,note.position_x,note.side,offset);
        return normal_intersection(source.reference_pixels,source.side_normal,options.max_normal_projection_reference_pixels,time);
    }
    Point projected_pixel(const PhigrosLinePose& pose,const SemanticNote& note,double offset,SongNs time) const {
        const auto target=projected(pose,note,offset,time);
        const auto mapped=mapping.map_reference_pixels(target.point);
        if(mapped.status!=GeometryStatus::mapped||!mapped.pixel)
            throw Failure("uncertified_normal_projection","Projected normal intersection does not map inside declared input geometry",time,target.point);
        return {static_cast<double>(mapped.pixel->x)/(mapping.geometry().input_width-1),
                static_cast<double>(mapped.pixel->y)/(mapping.geometry().input_height-1)};
    }
    double projected_reserve() const {
        // Rasterization plus a conservative floating-point conditioning reserve
        // at the certified minimum denominator. This is not judgment tolerance.
        return raster_error+input_scale*(1e-6+64*std::numeric_limits<double>::epsilon()*
            (phigros_reference_width+phigros_reference_height+options.max_normal_projection_reference_pixels)/projection_eta_floor);
    }
    void projected_segment(Contact& contact,const SemanticNote& note,std::size_t line_index,SongNs a,SongNs b,
            const std::function<double(SongNs)>& offset_at,double offset_slope,NoteContactOwnership& ownership,
            bool native_key,unsigned depth=0) {
        if(++visibility_checks>options.max_steps_in_plan*16)
            throw Failure("projection_work_budget","Normal-intersection proof exceeds finite work budget",a);
        const auto pose=poses.line_at(line_index,a);
        const auto slope=slope_at(chart.lines[line_index],a);
        const auto certificate=certify_projection(note,pose,slope,offset_at(a),offset_slope,b-a,
            options.max_normal_projection_reference_pixels,input_scale);
        if(!certificate) {
            for(auto elapsed:{SongNs{0},(b-a)/2,b-a})
                (void)projected(advanced(pose,slope,elapsed),note,offset_at(a)+offset_slope*static_cast<double>(elapsed),add(a,elapsed));
            if(depth>=24||b-a<=1)
                throw Failure("uncertified_normal_projection","Feasibility, active branches, or near-tangent speed bound cannot be certified within subdivision limits",a,
                    phigros_line_point(pose,note.position_x,note.side,offset_at(a)).reference_pixels);
            const auto middle=a+(b-a)/2;
            projected_segment(contact,note,line_index,a,middle,offset_at,offset_slope,ownership,native_key,depth+1);
            projected_segment(contact,note,line_index,middle,b,offset_at,offset_slope,ownership,false,depth+1);
            return;
        }
        if(ownership.normal_projection_spans.size()>=options.max_steps_per_contact||++projection_span_count>options.max_steps_in_plan)
            throw Failure("projection_span_budget","Normal-intersection provenance spans exceed configured finite budget",a);
        ownership.normal_projection_spans.push_back({a,b,certificate->correction.lo,certificate->correction.hi,
            certificate->edge_mask,certificate->speed});
        ownership.projection_min_reference_pixels=std::min(ownership.projection_min_reference_pixels,certificate->correction.lo);
        ownership.projection_max_reference_pixels=std::max(ownership.projection_max_reference_pixels,certificate->correction.hi);
        const auto at=[&](SongNs t){return projected_pixel(poses.line_at(line_index,t),note,offset_at(t),t);};
        append(contact,a,contact.steps.empty() ? Phase::down : Phase::move,at(a),native_key||a==note.start_ns);
        const auto reserve=projected_reserve();
        if(!std::isfinite(reserve)||reserve>=options.max_path_error_pixels)
            throw Failure("projection_precision_budget","Conditioning/raster reserve exhausts requested path error bound",a);
        if(certificate->speed==0)return;
        const long double allowance=(options.max_path_error_pixels-reserve)/certificate->speed;
        if(allowance<1)throw Failure("projection_resolution","Normal-intersection bound requires sub-nanosecond MOVE events",a);
        const auto duration=b-a;
        const SongNs delta=allowance>=static_cast<long double>(duration) ? duration : static_cast<SongNs>(std::floor(allowance));
        for(SongNs t=a;b-t>delta;) {t+=delta;append(contact,t,Phase::move,at(t));}
    }
    void projected_trajectory(Contact& contact,const SemanticNote& note,std::size_t line_index,
            SongNs first,SongNs last,bool flick,NoteContactOwnership& ownership) {
        std::vector<SongNs> boundaries={first,last};
        const auto collect=[&](const auto& events) {
            auto it=std::upper_bound(events.begin(),events.end(),first,[](SongNs t,const auto& e){return t<e.time.start_ns;});
            if(it!=events.begin()) {const auto end=std::prev(it)->time.end_ns;if(end>first&&end<last)boundaries.push_back(end);}
            for(;it!=events.end()&&it->time.start_ns<last;++it)for(auto t:{it->time.start_ns,it->time.end_ns})
                if(t>first&&t<last)boundaries.push_back(t);
        };
        collect(chart.lines[line_index].move_events);collect(chart.lines[line_index].rotate_events);
        if(flick)boundaries.push_back(note.start_ns);
        std::sort(boundaries.begin(),boundaries.end());boundaries.erase(std::unique(boundaries.begin(),boundaries.end()),boundaries.end());
        const double offset_slope=flick ? options.flick_distance_reference_pixels/static_cast<double>(options.flick_travel_ns) : 0;
        const std::function<double(SongNs)> offset_at=[&](SongNs t){return offset_slope*static_cast<double>(static_cast<long double>(t)-note.start_ns);};
        for(std::size_t i=0;i+1<boundaries.size();++i)
            projected_segment(contact,note,line_index,boundaries[i],boundaries[i+1],offset_at,offset_slope,ownership,true);
        const auto end=projected(poses.line_at(line_index,last),note,offset_at(last),last);
        ownership.projection_min_reference_pixels=std::min(ownership.projection_min_reference_pixels,end.correction);
        ownership.projection_max_reference_pixels=std::max(ownership.projection_max_reference_pixels,end.correction);
        append(contact,last,flick ? Phase::move : Phase::up,projected_pixel(poses.line_at(line_index,last),note,offset_at(last),last),true);
        if(flick)append(contact,add(last,options.flick_release_ns),Phase::up,contact.steps.back().point,true);
    }
};

void check_options(const ContactCompileOptions& options) {
    if(options.max_contacts<1||options.max_contacts>10 || options.tap_duration_ns<=0 || options.drag_duration_ns<=0 ||
       options.flick_lead_ns<=0 || options.flick_travel_ns<=options.flick_lead_ns || options.flick_release_ns<=0 ||
       !std::isfinite(options.flick_distance_reference_pixels)||options.flick_distance_reference_pixels<=0 ||
       !std::isfinite(options.max_path_error_pixels)||options.max_path_error_pixels<=raster_error ||
       options.max_contacts_in_plan==0||options.max_contacts_in_plan>200000||options.max_steps_per_contact<2||options.max_steps_per_contact>2000000||
       options.max_steps_in_plan<2||options.max_steps_in_plan>2000000||options.max_diagnostics==0||options.max_diagnostics>2000000||
       options.interference_window_ns<0||!std::isfinite(options.interference_radius_pixels)||options.interference_radius_pixels<0)
        throw Failure("invalid_options", "Compiler capacity, timing, geometry error, or resource limit is invalid");
    if(!std::isfinite(options.max_normal_projection_reference_pixels)||options.max_normal_projection_reference_pixels<0)
        throw Failure("invalid_options", "Normal projection bound must be finite and nonnegative");
    if(options.offscreen_policy!=OffscreenPolicy::reject&&options.offscreen_policy!=OffscreenPolicy::bounded_normal_candidate&&
       options.offscreen_policy!=OffscreenPolicy::bounded_normal_intersection_candidate_v1)
        throw Failure("invalid_options", "Unknown offscreen policy");
    if(options.offscreen_policy!=OffscreenPolicy::reject&&
       (!std::isfinite(options.max_normal_projection_reference_pixels)||options.max_normal_projection_reference_pixels<=0||options.calibration_id.empty()))
        throw Failure("invalid_options", "Projection candidates require a finite positive bound and pending calibration identity");
}
std::string cache_key(const CompiledContactPlan& plan) {
    const auto& i=plan.memory.identity; const auto& g=plan.metadata.geometry; const auto& o=plan.metadata.options;
    std::ostringstream stream; stream.imbue(std::locale::classic());
    stream<<std::hexfloat;
    const auto text=[&](const std::string& value){stream<<value.size()<<':'<<value<<';';};
    text("pas.compiled-contact-fingerprint.v2");
    text(plan.metadata.compiler_version);text(plan.metadata.strategy);text(plan.metadata.projection_policy_version);
    stream<<plan.metadata.requires_calibration<<','<<static_cast<int>(i.game)<<';';
    text(i.song_id);text(i.difficulty);text(i.revision);text(i.chart_sha256);text(i.audio_sha256);text(i.layout_id);
    stream<<g.input_width<<','<<g.input_height<<','<<static_cast<int>(g.rotation)<<','<<static_cast<int>(g.fit)<<','<<g.viewport.has_value()<<';';
    if(g.viewport) stream<<g.viewport->left<<','<<g.viewport->top<<','<<g.viewport->width<<','<<g.viewport->height<<';';
    stream<<o.max_contacts<<','<<o.tap_duration_ns<<','<<o.drag_duration_ns<<','<<o.flick_lead_ns<<','<<o.flick_travel_ns<<','<<o.flick_release_ns<<','
          <<o.flick_distance_reference_pixels<<','<<o.max_path_error_pixels<<','<<o.max_contacts_in_plan<<','<<o.max_steps_per_contact<<','<<o.max_steps_in_plan<<','
          <<o.max_diagnostics<<','<<o.interference_window_ns<<','<<o.interference_radius_pixels<<','<<static_cast<int>(o.offscreen_policy)<<','
          <<o.max_normal_projection_reference_pixels<<';';
    text(o.calibration_id);
    stream<<plan.ownership.size()<<':';
    for(const auto& owned:plan.ownership) {
        text(owned.source_note_id);text(owned.source_json_pointer);
        stream<<owned.line_index<<','<<owned.note_index<<','<<static_cast<int>(owned.kind)<<','<<static_cast<int>(owned.side)<<','
            <<owned.contact_id<<','<<owned.judgment_ns<<','<<owned.normal_projection_reference_pixels<<','
            <<owned.time_varying_normal_projection<<','<<owned.projection_min_reference_pixels<<','<<owned.projection_max_reference_pixels<<','
            <<owned.normal_projection_spans.size()<<';';
        for(const auto& span:owned.normal_projection_spans)stream<<span.begin_ns<<','<<span.end_ns<<','<<span.correction_lower_reference_pixels<<','
            <<span.correction_upper_reference_pixels<<','<<span.possible_edge_mask<<','<<span.speed_bound_input_pixels_per_ns<<';';
    }
    // Incorporate the complete deterministic output so manually constructed IR
    // cannot reuse a cache entry merely by claiming an existing source hash.
    stream<<plan.memory.contacts.size()<<':';
    for(const auto& c:plan.memory.contacts) {stream<<c.id<<','<<c.steps.size()<<':';for(const auto& s:c.steps) stream<<s.song_ns<<','<<static_cast<int>(s.phase)<<','<<s.point.x<<','<<s.point.y<<';';}
    stream<<plan.events.size()<<':';
    for(const auto& e:plan.events)stream<<e.song_ns<<','<<e.contact_id<<','<<static_cast<int>(e.phase)<<','<<e.point.x<<','<<e.point.y<<';';
    stream<<plan.peak_contacts<<','<<plan.total_steps<<';';
    return chart_text_sha256(stream.str());
}
} // namespace

std::string compiled_contact_fingerprint(const CompiledContactPlan& plan) {return cache_key(plan);}

ContactCompileResult compile_phigros_contacts(const SemanticChart& chart,Identity identity,
        PhigrosGeometry geometry,ContactCompileOptions options) {
    ContactCompileResult result;
    bool errors=false;
    const auto diagnostic_limit=std::clamp(options.max_diagnostics,std::size_t{1},std::size_t{2000000});
    const auto diagnostic=[&](CompileDiagnostic d) {
        if(d.severity==CompileSeverity::error) errors=true;
        if(result.diagnostics.size()<diagnostic_limit) result.diagnostics.push_back(std::move(d));
        else {errors=true; if(!result.diagnostics.empty()) result.diagnostics.back()={CompileSeverity::error,"diagnostic_budget","Further diagnostics truncated; plan rejected",{}, {},0,0,0,{}};}
    };
    try {
        check_options(options);
        if(identity.game!=Game::phigros||identity.song_id.empty()||identity.difficulty.empty()||identity.revision.empty()||identity.layout_id.empty()||
           !sha256_text(identity.chart_sha256)||!sha256_text(identity.audio_sha256)||
           identity.chart_sha256!=chart.source.chart_sha256||identity.song_id!=chart.source.song_id||
           identity.difficulty!=chart.source.difficulty||identity.revision!=chart.source.game_revision)
            throw Failure("identity_mismatch", "Chart source and complete chart/audio/layout identity must match exactly");
        Build build(chart,geometry,options);
        CompiledContactPlan plan; plan.memory.identity=std::move(identity);plan.metadata.geometry=geometry;plan.metadata.options=options;
        const bool dynamic_projection=options.offscreen_policy==OffscreenPolicy::bounded_normal_intersection_candidate_v1;
        plan.metadata.requires_calibration=options.offscreen_policy!=OffscreenPolicy::reject;
        if(dynamic_projection) {
            plan.metadata.compiler_version="phigros-contact-v2";
            plan.metadata.projection_policy_version="bounded_normal_intersection_candidate_v1";
            plan.metadata.strategy+="; bounded-normal-intersection-candidate";
        } else if(options.offscreen_policy==OffscreenPolicy::bounded_normal_candidate)
            plan.metadata.projection_policy_version="bounded_fixed_normal_candidate_v1";
        std::set<std::string> source_ids;
        std::uint64_t next_id=1;
        std::size_t notes=0;
        for(std::size_t li=0;li<chart.lines.size();++li) for(std::size_t ni=0;ni<chart.lines[li].notes.size();++ni) {
            const auto& note=chart.lines[li].notes[ni]; ++notes;
            if(notes>options.max_contacts_in_plan) throw Failure("contact_budget", "Chart exceeds configured contact budget");
            try {
                if(note.source.id.empty()||note.source.json_pointer.empty()||note.source.id!=chart.source.chart_sha256+'#'+note.source.json_pointer||!source_ids.insert(note.source.id).second)
                    throw Failure("invalid_note_source", "Each note needs a unique actual-chart-hash#JSON-pointer source identity",note.start_ns);
                if(note.start_ns<0||note.end_ns<note.start_ns||!std::isfinite(note.position_x)||!std::isfinite(note.speed)||!std::isfinite(note.floor_position)||
                   !std::isfinite(note.time_tick)||!std::isfinite(note.hold_ticks)||
                   (note.side!=NoteSide::above&&note.side!=NoteSide::below))
                    throw Failure("invalid_note", "Invalid note time, finite field, or side",note.start_ns);
                const double projection=dynamic_projection ? build.projected(build.poses.line_at(li,note.start_ns),note,0,note.start_ns).correction : build.projection(note,li);
                Contact contact;contact.id=next_id++;
                NoteContactOwnership owned;
                owned.source_note_id=note.source.id;owned.source_json_pointer=note.source.json_pointer;owned.line_index=li;owned.note_index=ni;
                owned.kind=note.kind;owned.side=note.side;owned.contact_id=contact.id;owned.judgment_ns=note.start_ns;
                owned.normal_projection_reference_pixels=projection;owned.time_varying_normal_projection=dynamic_projection;
                owned.projection_min_reference_pixels=owned.projection_max_reference_pixels=projection;
                if(note.kind==NoteKind::hold) {
                    if(note.end_ns==note.start_ns) throw Failure("zero_length_hold", "A Hold must have positive contact duration",note.start_ns);
                    if(dynamic_projection)build.projected_trajectory(contact,note,li,note.start_ns,note.end_ns,false,owned);
                    else build.trajectory(contact,note,li,note.start_ns,note.end_ns,projection,false);
                } else if(note.kind==NoteKind::flick) {
                    if(note.start_ns<options.flick_lead_ns) throw Failure("negative_flick_lead", "Flick stroke would begin before song zero",note.start_ns);
                    const auto first=note.start_ns-options.flick_lead_ns;
                    if(dynamic_projection)build.projected_trajectory(contact,note,li,first,add(first,options.flick_travel_ns),true,owned);
                    else build.trajectory(contact,note,li,first,add(first,options.flick_travel_ns),projection,true);
                    const auto& a=contact.steps.front().point;const auto& b=contact.steps[contact.steps.size()-2].point;
                    const double dx=(b.x-a.x)*(geometry.input_width-1),dy=(b.y-a.y)*(geometry.input_height-1);
                    if(std::hypot(dx,dy)<1) throw Failure("invisible_flick", "Flick has no observable raster displacement",note.start_ns);
                } else if(note.kind==NoteKind::tap||note.kind==NoteKind::drag) {
                    const auto pose=build.poses.line_at(li,note.start_ns);
                    const auto point=dynamic_projection ? build.projected_pixel(pose,note,0,note.start_ns) : build.point(pose,note,projection,note.start_ns);
                    if(dynamic_projection) {
                        if(++build.projection_span_count>options.max_steps_in_plan)throw Failure("projection_span_budget","Static projection provenance exceeds plan budget",note.start_ns);
                        const auto projected=build.projected(pose,note,0,note.start_ns);
                        owned.normal_projection_spans.push_back({note.start_ns,note.start_ns,projection,projection,projected.edge,0});
                    }
                    build.append(contact,note.start_ns,Phase::down,point,true);
                    build.append(contact,add(note.start_ns,note.kind==NoteKind::tap ? options.tap_duration_ns : options.drag_duration_ns),Phase::up,point,true);
                } else throw Failure("unsupported_note", "Unknown note kind",note.start_ns);
                if(contact.steps.size()>options.max_steps_in_plan||plan.total_steps>options.max_steps_in_plan-contact.steps.size()) throw Failure("plan_step_budget", "Whole-song step budget exceeded",note.start_ns);
                plan.total_steps+=contact.steps.size();
                const bool projected_candidate=owned.projection_min_reference_pixels!=0||owned.projection_max_reference_pixels!=0;
                plan.ownership.push_back(std::move(owned));
                plan.memory.contacts.push_back(std::move(contact));
                if(projected_candidate) diagnostic({CompileSeverity::warning,"unverified_normal_projection","Bounded normal projection is a calibration candidate; actual judgment acceptance unknown",note.source.id,{},li,ni,note.start_ns,{}});
                if(note.kind==NoteKind::drag||note.kind==NoteKind::flick)
                    diagnostic({CompileSeverity::warning,"fresh_down_candidate","Exclusive Drag/Flick starts a new DOWN; game-specific interference remains unverified",note.source.id,{},li,ni,note.start_ns,{}});
            } catch(const Failure& e) {
                diagnostic({CompileSeverity::error,e.code,e.what(),note.source.id,{},li,ni,e.time,e.point});
            }
        }
        if(notes==0) throw Failure("empty_chart", "No note contacts to compile");
        if(errors) return result;
        // Sweep potential Tap/Hold interference intervals; no unbounded all-pairs scan.
        std::vector<std::size_t> taps,fresh;
        for(std::size_t n=0;n<plan.ownership.size();++n) {
            const auto kind=plan.ownership[n].kind;
            (kind==NoteKind::tap||kind==NoteKind::hold ? taps : fresh).push_back(n);
        }
        const auto by_down=[&](std::size_t a,std::size_t b){return plan.memory.contacts[a].steps.front().song_ns<plan.memory.contacts[b].steps.front().song_ns;};
        std::sort(taps.begin(),taps.end(),by_down);std::sort(fresh.begin(),fresh.end(),by_down);
        std::set<std::pair<SongNs,std::size_t>> active;
        std::size_t cursor=0,checks=0;
        for(auto fi:fresh) {
            const auto& down=plan.memory.contacts[fi].steps.front();
            const SongNs until=down.song_ns>std::numeric_limits<SongNs>::max()-options.interference_window_ns ? std::numeric_limits<SongNs>::max() : down.song_ns+options.interference_window_ns;
            while(cursor<taps.size()&&plan.memory.contacts[taps[cursor]].steps.front().song_ns<=until) {
                const auto n=taps[cursor++];active.emplace(plan.memory.contacts[n].steps.back().song_ns,n);
            }
            const auto since=down.song_ns<options.interference_window_ns ? 0 : down.song_ns-options.interference_window_ns;
            while(!active.empty()&&active.begin()->first<since) active.erase(active.begin());
            for(const auto& [unused,ti]:active) {
                (void)unused;
                if(++checks>options.max_steps_in_plan) throw Failure("interference_budget", "Interference analysis exceeds configured finite work budget",down.song_ns);
                const auto& steps=plan.memory.contacts[ti].steps;
                const auto it=std::upper_bound(steps.begin(),steps.end(),down.song_ns,[](SongNs t,const Step& s){return t<s.song_ns;});
                const auto& other=(it==steps.begin() ? steps.front() : *std::prev(it)).point;
                if(std::hypot((down.point.x-other.x)*(geometry.input_width-1),(down.point.y-other.y)*(geometry.input_height-1))<=options.interference_radius_pixels) {
                    const auto& source=plan.ownership[fi];
                    diagnostic({CompileSeverity::warning,"nearby_tap_hold_interference","Fresh Drag/Flick DOWN is near a Tap/Hold contact in the configured time window; game behavior unverified",source.source_note_id,plan.ownership[ti].source_note_id,source.line_index,source.note_index,down.song_ns,{}});
                }
            }
        }
        if(errors) return result;
        std::vector<std::pair<SongNs,int>> edges;
        edges.reserve(plan.memory.contacts.size()*2);
        for(const auto& contact:plan.memory.contacts) {
            edges.emplace_back(contact.steps.front().song_ns,1);
            edges.emplace_back(contact.steps.back().song_ns,-1);
        }
        std::sort(edges.begin(),edges.end());
        int active_count=0,peak=0;
        for(const auto& edge:edges) {active_count+=edge.second;peak=std::max(peak,active_count);}
        result.required_peak_contacts=peak;
        if(peak>options.max_contacts)
            throw Failure("capacity_exceeded","Plan requires "+std::to_string(peak)+" contacts; configured capacity is "+std::to_string(options.max_contacts));
        try { plan.peak_contacts=validate(plan.memory,options.max_contacts); }
        catch(const std::invalid_argument& e) {throw Failure("capacity_or_lifecycle",e.what());}
        for(const auto& contact:plan.memory.contacts) for(const auto& step:contact.steps)
            plan.events.push_back({step.song_ns,contact.id,step.phase,step.point});
        std::sort(plan.events.begin(),plan.events.end(),[](const auto& a,const auto& b){
            return std::tuple(a.song_ns,phase_order(a.phase),a.contact_id)<std::tuple(b.song_ns,phase_order(b.phase),b.contact_id);
        });
        plan.metadata.cache_sha256=compiled_contact_fingerprint(plan);
        result.plan=std::move(plan);
    } catch(const Failure& e) {diagnostic({CompileSeverity::error,e.code,e.what(),{},{},0,0,e.time,e.point});}
      catch(const std::exception& e) {diagnostic({CompileSeverity::error,"invalid_chart_or_geometry",e.what(),{},{},0,0,0,{}});}
    return result;
}
} // namespace rhythm
