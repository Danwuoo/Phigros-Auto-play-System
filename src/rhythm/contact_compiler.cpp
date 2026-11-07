#include "rhythm/contact_compiler.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iomanip>
#include <limits>
#include <locale>
#include <numeric>
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
    double tangent_correction=0;
};
Range projection_slab(double extent,double inset) {
    // Positive insets reserve the far raster cell too. The zero-inset branch
    // deliberately retains the archived continuous canvas and fingerprints.
    return {inset,inset>0 ? extent-1-inset : extent};
}
ProjectedPoint normal_intersection(PosePoint point,PosePoint normal,double bound,double inset,SongNs time) {
    double lower=-bound,upper=bound;
    unsigned lower_edge=0,upper_edge=0;
    const auto constrain=[&](double p,double n,double extent,unsigned low,unsigned high) {
        const auto slab=projection_slab(extent,inset);
        if(n==0) {
            if(p<slab.lo||p>slab.hi)throw Failure("projection_unavailable","Normal line cannot intersect the target rectangle within the declared bound",time,point);
            return;
        }
        const double first=inset>0 ? (slab.lo-p)/n : -p/n,last=(slab.hi-p)/n;
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
    const auto xs=projection_slab(phigros_reference_width,inset),ys=projection_slab(phigros_reference_height,inset);
    if(result.edge==1)result.point.x=xs.lo;
    if(result.edge==2)result.point.x=xs.hi;
    if(result.edge==4)result.point.y=ys.lo;
    if(result.edge==8)result.point.y=ys.hi;
    if(!std::isfinite(result.point.x)||!std::isfinite(result.point.y)||
       result.point.x<xs.lo||result.point.x>xs.hi||result.point.y<ys.lo||result.point.y>ys.hi)
        throw Failure("uncertified_normal_projection","Analytic normal intersection could not be represented inside both target slabs",time,point);
    return result;
}
ProjectedPoint judgment_rectangle(PosePoint point,PosePoint normal,PosePoint tangent,
        double normal_bound,double tangent_bound,double inset,SongNs time) {
    const auto xs=projection_slab(phigros_reference_width,inset),ys=projection_slab(phigros_reference_height,inset);
    ProjectedPoint result;
    result.point={std::clamp(point.x,xs.lo,xs.hi),std::clamp(point.y,ys.lo,ys.hi)};
    const PosePoint delta{result.point.x-point.x,result.point.y-point.y};
    result.correction=delta.x*normal.x+delta.y*normal.y;
    result.tangent_correction=delta.x*tangent.x+delta.y*tangent.y;
    if(!std::isfinite(result.correction)||!std::isfinite(result.tangent_correction)||
       std::abs(result.correction)>normal_bound||std::abs(result.tangent_correction)>tangent_bound) {
        std::ostringstream reason;reason<<"Nearest rectangle point exceeds declared judgment domain: normal="<<result.correction
            <<" / "<<normal_bound<<", tangent="<<result.tangent_correction<<" / "<<tangent_bound<<" reference pixels";
        throw Failure("judgment_domain_unreachable",reason.str(),time,point);
    }
    result.edge=0;
    if(point.x<xs.lo)result.edge|=1;if(point.x>xs.hi)result.edge|=2;
    if(point.y<ys.lo)result.edge|=4;if(point.y>ys.hi)result.edge|=8;
    if(!result.edge)result.edge=16;
    return result;
}
Range quotient(Range numerator,Range denominator) {
    return product(numerator,range(1/denominator.lo,1/denominator.hi));
}
struct ProjectionCertificate {
    Range correction;
    unsigned edge_mask=16;
    double speed=0;
    Range tangent_correction{0,0};
    bool judgment_domain_gap=false;
};
std::optional<ProjectionCertificate> certify_judgment_rectangle(const SemanticNote& note,const PhigrosLinePose& pose,
        Slope slope,double offset,double offset_slope,SongNs duration,double normal_bound,double tangent_bound,
        double input_scale,double inset,bool allow_tangent_gap=false) {
    const double elapsed=static_cast<double>(duration),theta=std::remainder(pose.rotation_degrees,360.0)*pi/180;
    auto cs=trig(theta,theta+slope.radians*elapsed,true),sn=trig(theta,theta+slope.radians*elapsed,false);
    if(slope.radians==0) {cs={pose.tangent.x,pose.tangent.x};sn={-pose.tangent.y,-pose.tangent.y};}
    const double side=note.side==NoteSide::above?1:-1,along=note.position_x*phigros_position_x_pixels;
    const auto nx=product({-side,-side},sn),ny=product({-side,-side},cs);
    const auto offsets=range(offset,offset+offset_slope*elapsed);
    const auto x=plus(range(pose.origin.x*phigros_reference_width,pose.origin.x*phigros_reference_width+slope.origin.x*elapsed),
        plus(product({along,along},cs),product(offsets,nx)));
    const auto y=plus(range(pose.origin.y*phigros_reference_height,pose.origin.y*phigros_reference_height+slope.origin.y*elapsed),
        plus(product({-along,-along},sn),product(offsets,ny)));
    const auto xs=projection_slab(phigros_reference_width,inset),ys=projection_slab(phigros_reference_height,inset);
    // C(P)=nearest rectangle point. Each component is clamp(P); its correction
    // clamp(p)-p is monotone nonincreasing. These interval dot products enclose
    // EVERY continuous normal/tangent correction, including corner branches.
    const auto correction=[](Range p,Range slab) {
        return Range{std::clamp(p.hi,slab.lo,slab.hi)-p.hi,std::clamp(p.lo,slab.lo,slab.hi)-p.lo};
    };
    const auto dx=correction(x,xs),dy=correction(y,ys);
    auto normal=plus(product(dx,nx),product(dy,ny));
    auto tangent=plus(product(dx,cs),product(dy,product({-1,-1},sn)));
    // Outward floating-point reserve for endpoint arithmetic and interval dot
    // products. An exactly zero correction stays exactly zero inside the box.
    if(dx.lo!=0||dx.hi!=0||dy.lo!=0||dy.hi!=0) {
        const double reserve=64*std::numeric_limits<double>::epsilon()*(1+std::max(std::abs(x.lo),std::abs(x.hi))+
            std::max(std::abs(y.lo),std::abs(y.hi))+std::abs(along)+std::max(std::abs(offsets.lo),std::abs(offsets.hi)));
        normal={normal.lo-reserve,normal.hi+reserve};tangent={tangent.lo-reserve,tangent.hi+reserve};
    }
    if(!std::isfinite(normal.lo)||!std::isfinite(normal.hi)||!std::isfinite(tangent.lo)||!std::isfinite(tangent.hi)||
       normal.lo < -normal_bound||normal.hi>normal_bound)return std::nullopt;
    const bool gap=tangent.lo < -tangent_bound||tangent.hi>tangent_bound;
    if(gap&&!allow_tangent_gap)return std::nullopt;
    // Rectangle projection is 1-Lipschitz. On a wholly clipped coordinate its
    // speed is zero; otherwise bound the complete native derivative. The
    // resulting speed certifies held discrete coordinates, not interpolation.
    const double normal_coefficient=along*slope.radians+side*offset_slope;
    const auto tangent_coefficient=product({-side*slope.radians,-side*slope.radians},offsets);
    const auto vx=plus({slope.origin.x,slope.origin.x},plus(product({-normal_coefficient,-normal_coefficient},sn),product(tangent_coefficient,cs)));
    const auto vy=plus({slope.origin.y,slope.origin.y},plus(product({-normal_coefficient,-normal_coefficient},cs),product(tangent_coefficient,product({-1,-1},sn))));
    const auto magnitude=[](Range r){return std::max(std::abs(r.lo),std::abs(r.hi));};
    const double sx=x.hi<=xs.lo||x.lo>=xs.hi?0:magnitude(vx),sy=y.hi<=ys.lo||y.lo>=ys.hi?0:magnitude(vy);
    const double speed=input_scale*std::hypot(sx,sy)*(1+1e-12);
    if(!std::isfinite(speed))return std::nullopt;
    unsigned edge=0;
    if(x.lo<xs.lo)edge|=1;if(x.hi>xs.hi)edge|=2;if(y.lo<ys.lo)edge|=4;if(y.hi>ys.hi)edge|=8;
    if(x.hi>=xs.lo&&x.lo<=xs.hi&&y.hi>=ys.lo&&y.lo<=ys.hi)edge|=16;
    return ProjectionCertificate{normal,edge,speed,tangent,gap};
}
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
        double offset,double offset_slope,SongNs duration,double bound,double input_scale,double inset,bool tight_speed) {
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
    const auto magnitude=[](Range r){return std::max(std::abs(r.lo),std::abs(r.hi));};
    double analytic_native_speed=native_speed;
    if(tight_speed) {
        // P'=O'+(along*w+side*offset')*N_above-side*offset*w*T.
        // Component intervals retain affine cancellation; the archived
        // triangle inequality remains the default.
        const double normal_coefficient=along*slope.radians+side*offset_slope;
        const auto tangent_coefficient=product({-side*slope.radians,-side*slope.radians},offsets);
        const auto vx=plus({slope.origin.x,slope.origin.x},plus(product({-normal_coefficient,-normal_coefficient},sn),product(tangent_coefficient,cs)));
        const auto vy=plus({slope.origin.y,slope.origin.y},plus(product({-normal_coefficient,-normal_coefficient},cs),product(tangent_coefficient,product({-1,-1},sn))));
        analytic_native_speed=std::hypot(magnitude(vx),magnitude(vy));
    }
    const auto xs=projection_slab(phigros_reference_width,inset),ys=projection_slab(phigros_reference_height,inset);
    if(x.lo>=xs.lo&&x.hi<=xs.hi&&y.lo>=ys.lo&&y.hi<=ys.hi)
        return ProjectionCertificate{{0,0},16,input_scale*(tight_speed?analytic_native_speed:native_speed)*(1+1e-12)};
    struct Axis {Range lower{},upper{};double eta=0;unsigned low_edge=0,high_edge=0;bool active=false;};
    std::array<Axis,2> axes{};
    Range lower{-bound,-bound},upper{bound,bound};
    const auto axis=[&](std::size_t index,Range p,Range n,double extent,unsigned low_edge,unsigned high_edge) {
        const auto slab=projection_slab(extent,inset);
        // If the entire bounded normal tube is inside this slab, the slab can
        // neither block feasibility nor be an active closest-to-zero edge.
        const auto tube=plus(p,product({-bound,bound},n));
        if(tube.lo>=slab.lo&&tube.hi<=slab.hi)return true;
        if(n.lo<=0&&n.hi>=0)return false;
        auto& out=axes[index];out.eta=std::min(std::abs(n.lo),std::abs(n.hi));
        if(out.eta<projection_eta_floor)return false;
        const auto zero_minus=inset>0 ? Range{slab.lo-p.hi,slab.lo-p.lo} : Range{-p.hi,-p.lo};
        const auto extent_minus=Range{slab.hi-p.hi,slab.hi-p.lo};
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
    certificate.edge_mask=certificate.correction.lo<=0&&certificate.correction.hi>=0 ? 16u : 0u;
    double speed=tight_speed ? (certificate.edge_mask&16 ? analytic_native_speed : 0) : native_speed;
    const auto analytic_edge_speed=[&](unsigned edge) {
        // Active horizontal edge Y:
        // Qx=Ox+along/cos(theta)+(Y-Oy)*tan(theta), Qy=Y.
        // Active vertical edge X:
        // Qy=Oy-along/sin(theta)+(X-Ox)*cot(theta), Qx=X.
        // Offset/stroke velocity cancel under projection. Branch switches
        // retain the maximum derivative bound of all possible active edges.
        const bool vertical=edge==1||edge==2;
        const auto denominator=vertical ? sn : cs;
        const auto eta=std::min(std::abs(denominator.lo),std::abs(denominator.hi));
        if(denominator.lo<=0&&denominator.hi>=0)return std::numeric_limits<double>::infinity();
        if(eta<projection_eta_floor)return std::numeric_limits<double>::infinity();
        const auto other=vertical ? magnitude(cs) : magnitude(sn);
        const double origin=vertical ? pose.origin.x*phigros_reference_width : pose.origin.y*phigros_reference_height;
        const double velocity=vertical ? slope.origin.x : slope.origin.y;
        const double edge_value=edge==1 ? xs.lo : edge==2 ? xs.hi : edge==4 ? ys.lo : ys.hi;
        const auto distance=magnitude(range(edge_value-origin,edge_value-origin-velocity*elapsed));
        const double tangent_motion=vertical ? std::abs(slope.origin.y) : std::abs(slope.origin.x);
        return tangent_motion+std::abs(velocity)*other/eta+
            std::abs(slope.radians)*(std::abs(along)*other+distance)/(eta*eta);
    };
    for(const auto& a:axes) if(a.active) {
        bool possible=false;
        if(lower.hi>0&&a.lower.hi>=std::max(0.0,lower.lo)) {
            certificate.edge_mask|=a.low_edge;possible=true;
            if(tight_speed)speed=std::max(speed,analytic_edge_speed(a.low_edge));
        }
        if(upper.lo<0&&a.upper.lo<=std::min(0.0,upper.hi)) {
            certificate.edge_mask|=a.high_edge;possible=true;
            if(tight_speed)speed=std::max(speed,analytic_edge_speed(a.high_edge));
        }
        if(possible&&!tight_speed) {
            const double correction=std::max(std::abs(certificate.correction.lo),std::abs(certificate.correction.hi));
            const double variation=native_speed+correction*std::abs(slope.radians);
            speed=std::max(speed,variation+variation/a.eta);
        }
    }
    certificate.speed=input_scale*speed*(1+1e-12);
    if(!std::isfinite(certificate.speed)||!certificate.edge_mask)return {};
    return certificate;
}
// Geometry follows the chart-space model in TeamFlos/phira#901 (0afa0fa):
// bottom-left percentage coordinates, scale then rotate then translate, and
// normal-union XOR subtract-parity in both raw and adjusted rectangles.
// Work in upright reference pixels, where the Y axis points down.
struct BlockAreaField {
    struct Box { Range x,y; };
    struct Bounds { Range x,y,half_x,half_y,angle; };
    struct Piece {
        const ChartBlockArea* area;
        SongNs begin,end; // Inclusive nanoseconds, already clipped to active time.
        Bounds bounds;
        Box enclosing;
    };
    enum class Certainty { no,yes,unknown };
    struct Coverage {
        Certainty ordinary=Certainty::no, subtract=Certainty::no;
        void add(Certainty value,bool subtracting) {
            if(!subtracting) {
                if(value==Certainty::yes||ordinary==Certainty::no)ordinary=value;
            } else if(subtract!=Certainty::unknown) {
                if(value==Certainty::unknown)subtract=value;
                else if(value==Certainty::yes)
                    subtract=subtract==Certainty::yes?Certainty::no:Certainty::yes;
            }
        }
        Certainty blocked() const {
            if(ordinary==Certainty::unknown||subtract==Certainty::unknown)return Certainty::unknown;
            return ordinary==subtract?Certainty::no:Certainty::yes;
        }
    };
    std::vector<Piece> pieces;
    struct IndexEntry { SongNs maximum_end; Box enclosing; };
    std::vector<IndexEntry> nodes;
    std::size_t area_count=0,work=0,work_limit=0;
    SongNs time_before,time_after;
    static constexpr double edge_tolerance_pixels=21.6;
    static constexpr double minimum_half_size=0.0036;

    static PosePoint reference(BlockAreaPoint p) {
        return {p.x*phigros_reference_width,(1-p.y)*phigros_reference_height};
    }
    static Range magnitude(Range value) {
        return {value.lo<=0&&value.hi>=0?0:std::min(std::abs(value.lo),std::abs(value.hi)),
            std::max(std::abs(value.lo),std::abs(value.hi))};
    }
    static Range shift(Range value,double amount) {return {value.lo+amount,value.hi+amount};}
    static Range fixed(double value) {return {value,value};}
    static double easing_raw(int kind,double t) {
        if(kind==13)return 0;
        if(kind==14)return 1;
        if(kind==0)return t;
        const int power=(kind-1)/3+2;
        switch((kind-1)%3) {
            case 0:return std::pow(t,power);
            case 1:return 1-std::pow(1-t,power);
            default:return t<.5?std::pow(2*t,power)/2:1-std::pow(2-2*t,power)/2;
        }
    }
    static double easing(int kind,SongNs t,SongNs start,SongNs end) {
        if(t>=end)return 1;
        const double p=end<=start?100:100*std::clamp(
            static_cast<double>((static_cast<long double>(t)-start)/(static_cast<long double>(end)-start)),0.,1.);
        const auto i=std::floor(p);
        return std::lerp(easing_raw(kind,i/100),easing_raw(kind,std::min(i+1,100.)/100),p-i);
    }
    static Range blend(double from,double to,int kind,SongNs start,SongNs end,SongNs lo,SongNs hi) {
        return range(std::lerp(from,to,easing(kind,lo,start,end)),std::lerp(from,to,easing(kind,hi,start,end)));
    }
    static Bounds bounds(const ChartBlockArea& area,SongNs lo,SongNs hi) {
        const auto a=reference(area.bottom_left),b=reference(area.top_right);
        const PosePoint base{(a.x+b.x)/2,(a.y+b.y)/2};
        Bounds out{fixed(base.x),fixed(base.y),fixed(std::abs(b.x-a.x)/2),fixed(std::abs(b.y-a.y)/2),fixed(0)};
        const auto& scales=area.scales;
        if(!scales.empty()) {
            Range sx=fixed(scales.front().scale.x),sy=fixed(scales.front().scale.y);
            for(std::size_t i=0;i+1<scales.size();++i) {
                const auto& key=scales[i];const auto& next=scales[i+1];
                sx=blend(key.scale.x,next.scale.x,key.ease_x,key.time_ns,next.time_ns,lo,hi);
                sy=blend(key.scale.y,next.scale.y,key.ease_y,key.time_ns,next.time_ns,lo,hi);
                const auto anchor=reference(key.anchor);
                const auto ratio=[](Range value,double old) {
                    return std::abs(old)<8*std::numeric_limits<float>::denorm_min()?fixed(1):
                        range(value.lo/old,value.hi/old);
                };
                out.x=shift(product(shift(out.x,-anchor.x),ratio(sx,key.scale.x)),anchor.x);
                out.y=shift(product(shift(out.y,-anchor.y),ratio(sy,key.scale.y)),anchor.y);
                if(hi<next.time_ns)break;
            }
            out.half_x=product(out.half_x,magnitude(sx));
            out.half_y=product(out.half_y,magnitude(sy));
        }
        const auto& rotations=area.rotations;
        if(!rotations.empty()) {
            out.angle=fixed(-rotations.front().degrees*pi/180);
            for(std::size_t i=0;i+1<rotations.size();++i) {
                const auto& key=rotations[i];const auto& next=rotations[i+1];
                out.angle=blend(-key.degrees*pi/180,-next.degrees*pi/180,key.ease,key.time_ns,next.time_ns,lo,hi);
                const auto delta=shift(out.angle,key.degrees*pi/180);
                const auto cs=trig(delta.lo,delta.hi,true),sn=trig(delta.lo,delta.hi,false);
                const auto anchor=reference(key.anchor);
                const auto dx=shift(out.x,-anchor.x),dy=shift(out.y,-anchor.y);
                out.x=shift(plus(product(dx,cs),product(dy,{-sn.hi,-sn.lo})),anchor.x);
                out.y=shift(plus(product(dx,sn),product(dy,cs)),anchor.y);
                if(hi<next.time_ns)break;
            }
        }
        const auto& moves=area.moves;
        if(!moves.empty()) {
            const auto first=reference(moves.front().end_position);
            Range mx=fixed(first.x),my=fixed(first.y);
            for(std::size_t i=0;i+1<moves.size();++i) {
                const auto& key=moves[i];const auto& next=moves[i+1];
                const auto from=reference(key.end_position),to=reference(next.end_position);
                mx=blend(from.x,to.x,key.ease_x,key.time_ns,next.time_ns,lo,hi);
                my=blend(from.y,to.y,key.ease_y,key.time_ns,next.time_ns,lo,hi);
                if(hi<next.time_ns)break;
            }
            out.x=plus(out.x,shift(mx,-base.x));out.y=plus(out.y,shift(my,-base.y));
        }
        return out;
    }
    static Box enclosure(const Bounds& shape) {
        // The widest possible shape is the adjusted subtract rectangle.
        const double c=magnitude(trig(shape.angle.lo,shape.angle.hi,true)).hi;
        const double s=magnitude(trig(shape.angle.lo,shape.angle.hi,false)).hi;
        const double x=shape.half_x.hi+edge_tolerance_pixels,y=shape.half_y.hi+edge_tolerance_pixels;
        const double dx=x*c+y*s,dy=x*s+y*c;
        return {{shape.x.lo-dx,shape.x.hi+dx},{shape.y.lo-dy,shape.y.hi+dy}};
    }
    static void validate(const ChartBlockArea& area,const ChartSource& source) {
        const auto finite=[](BlockAreaPoint p){return std::isfinite(p.x)&&std::isfinite(p.y);};
        const auto ease=[](int kind){return kind>=0&&kind<=14;};
        const auto valid_track=[](const auto& keys,auto valid_key) {
            return std::is_sorted(keys.begin(),keys.end(),[](const auto& a,const auto& b){return a.time_ns<b.time_ns;})&&
                std::all_of(keys.begin(),keys.end(),valid_key);
        };
        if(area.source.json_pointer.empty()||area.source.id!=source.chart_sha256+'#'+area.source.json_pointer||
           !finite(area.top_right)||!finite(area.bottom_left)||area.appear_ns>area.enable_ns||
           area.enable_ns>area.disable_ns||area.disable_ns>area.disappear_ns||
           !valid_track(area.moves,[&](const auto& key){return finite(key.end_position)&&ease(key.ease_x)&&ease(key.ease_y);})||
           !valid_track(area.rotations,[&](const auto& key){return finite(key.anchor)&&std::isfinite(key.degrees*pi)&&ease(key.ease);})||
           !valid_track(area.scales,[&](const auto& key){return finite(key.anchor)&&finite(key.scale)&&ease(key.ease_x)&&ease(key.ease_y);}))
            throw Failure("block_area_geometry","BlockArea source, time phases, or keyframe geometry is invalid: "+area.source.json_pointer,area.enable_ns);
    }
    explicit BlockAreaField(const SemanticChart& chart,const ContactCompileOptions& options)
        :work_limit(std::clamp(options.max_steps_in_plan*128,std::size_t{200'000},std::size_t{32'000'000})),
         time_before(options.block_area_phase_ns.value_or(-100'000'000)-20'000'000),
         time_after(options.block_area_phase_ns.value_or(100'000'000)+40'000'000) {
        if(chart.block_areas.size()>20'000)
            throw Failure("block_area_work_budget","BlockArea region count exceeds the finite work budget");
        for(const auto& area:chart.block_areas) {
            consume(area.enable_ns,1+area.moves.size()+area.rotations.size()+area.scales.size());
            validate(area,chart.source);
            if(area.enable_ns>=area.disable_ns)continue;
            ++area_count;
            std::vector<SongNs> cuts{area.enable_ns,area.disable_ns};
            const auto insert=[&](const auto& keys){
                for(const auto& key:keys)if(key.time_ns>area.enable_ns&&key.time_ns<area.disable_ns)
                    cuts.push_back(key.time_ns);
            };
            insert(area.moves);insert(area.rotations);insert(area.scales);
            std::sort(cuts.begin(),cuts.end());
            cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
            for(std::size_t i=1;i<cuts.size();++i) {
                const auto begin=cuts[i-1],end=cuts[i]-1;
                consume(begin,1+area.moves.size()+area.rotations.size()+area.scales.size());
                const auto shape=bounds(area,begin,end);const auto box=enclosure(shape);
                if(!std::isfinite(box.x.lo)||!std::isfinite(box.x.hi)||!std::isfinite(box.y.lo)||!std::isfinite(box.y.hi))
                    throw Failure("block_area_geometry","BlockArea pose exceeds finite geometry",begin);
                pieces.push_back({&area,begin,end,shape,box});
            }
        }
        std::stable_sort(pieces.begin(),pieces.end(),[](const auto& a,const auto& b){return a.begin<b.begin;});
        if(!pieces.empty()) {
            nodes.resize(pieces.size()*4);
            index(1,0,pieces.size());
        }
    }
    void consume(SongNs time,std::size_t amount=1) {
        if(amount>work_limit-work)
            throw Failure("block_area_work_budget","BlockArea geometry proof exceeds the finite work budget",time);
        work+=amount;
    }
    IndexEntry index(std::size_t node,std::size_t begin,std::size_t end) {
        if(end-begin==1)return nodes[node]={pieces[begin].end,pieces[begin].enclosing};
        const auto middle=begin+(end-begin)/2;
        const auto a=index(node*2,begin,middle),b=index(node*2+1,middle,end);
        const auto join=[](Range a,Range b){return Range{std::min(a.lo,b.lo),std::max(a.hi,b.hi)};};
        return nodes[node]={std::max(a.maximum_end,b.maximum_end),
            {join(a.enclosing.x,b.enclosing.x),join(a.enclosing.y,b.enclosing.y)}};
    }
    static bool intersects(Box a,Box b) {
        return a.x.lo<=b.x.hi&&a.x.hi>=b.x.lo&&a.y.lo<=b.y.hi&&a.y.hi>=b.y.lo;
    }
    static Certainty contains(const Bounds& shape,Box path,bool adjusted,bool subtract) {
        if(shape.half_x.hi<minimum_half_size||shape.half_y.hi<minimum_half_size)return Certainty::no;
        const auto cs=trig(shape.angle.lo,shape.angle.hi,true),sn=trig(shape.angle.lo,shape.angle.hi,false);
        const auto x=plus(path.x,{-shape.x.hi,-shape.x.lo}),y=plus(path.y,{-shape.y.hi,-shape.y.lo});
        const auto dx=magnitude(plus(product(x,cs),product(y,sn)));
        const auto dy=magnitude(plus(product(x,{-sn.hi,-sn.lo}),product(y,cs)));
        const auto adjust=[&](double half) {
            return adjusted?half+(subtract?1:-1)*std::min(edge_tolerance_pixels,half/2):half;
        };
        if(dx.lo>adjust(shape.half_x.hi)||dy.lo>adjust(shape.half_y.hi))return Certainty::no;
        if(shape.half_x.lo>=minimum_half_size&&shape.half_y.lo>=minimum_half_size&&
           dx.hi<=adjust(shape.half_x.lo)&&dy.hi<=adjust(shape.half_y.lo))return Certainty::yes;
        return Certainty::unknown;
    }
    void assess(std::size_t node,std::size_t begin,std::size_t end,SongNs lo,SongNs hi,Box path,
                Coverage& raw,Coverage& adjusted,std::optional<SongNs>& split) {
        const auto& group=nodes[node];
        if(pieces[begin].begin>hi||group.maximum_end<lo||!intersects(group.enclosing,path))return;
        if(end-begin>1) {
            const auto middle=begin+(end-begin)/2;
            assess(node*2,begin,middle,lo,hi,path,raw,adjusted,split);
            assess(node*2+1,middle,end,lo,hi,path,raw,adjusted,split);
            return;
        }
        consume(lo);
        const auto& piece=pieces[begin];
        const auto first=std::max(lo,piece.begin),last=std::min(hi,piece.end);
        const bool partial=first!=lo||last!=hi;
        auto shape=piece.bounds;
        const auto constant=[](Range value){return value.lo==value.hi;};
        const bool stationary=constant(shape.x)&&constant(shape.y)&&constant(shape.half_x)&&constant(shape.half_y)&&constant(shape.angle);
        if(!stationary&&(first!=piece.begin||last!=piece.end)) {
            if(contains(shape,path,false,piece.area->subtract)==Certainty::no&&
               contains(shape,path,true,piece.area->subtract)==Certainty::no)return;
            consume(lo,piece.area->moves.size()+piece.area->rotations.size()+piece.area->scales.size());
            shape=bounds(*piece.area,first,last);
        }
        const auto observe=[&](Coverage& coverage,bool narrow) {
            auto state=contains(shape,path,narrow,piece.area->subtract);
            if(partial&&state!=Certainty::no) {
                state=Certainty::unknown;
                const auto middle=std::midpoint(lo,hi);
                const auto consider=[&](SongNs candidate){
                    const auto distance=[&](SongNs time){return std::abs(static_cast<long double>(time)-middle);};
                    if(!split||distance(candidate)<distance(*split))split=candidate;
                };
                if(first>lo)consider(first);
                if(last<hi)consider(last+1);
            }
            coverage.add(state,piece.area->subtract);
        };
        observe(raw,false);observe(adjusted,true);
    }
    Certainty blocked(Box path,SongNs lo,SongNs hi,std::optional<SongNs>& split) {
        if(pieces.empty())return Certainty::no;
        Coverage raw,adjusted;
        assess(1,0,pieces.size(),lo,hi,path,raw,adjusted,split);
        if(raw.blocked()==Certainty::no||adjusted.blocked()==Certainty::no)return Certainty::no;
        if(raw.blocked()==Certainty::yes&&adjusted.blocked()==Certainty::yes)return Certainty::yes;
        return Certainty::unknown;
    }
    bool clear(Box path,SongNs lo,SongNs hi,unsigned depth=0) {
        std::optional<SongNs> split;
        const auto state=blocked(path,lo,hi,split);
        if(state==Certainty::no)return true;
        if(state==Certainty::yes||depth>=12||hi==lo)return false;
        const auto middle=std::midpoint(lo,hi);
        std::optional<SongNs> unused;
        if(blocked(path,middle,middle,unused)!=Certainty::no)return false;
        const auto cut=split.value_or(middle+1);
        return clear(path,lo,cut-1,depth+1)&&clear(path,cut,hi,depth+1);
    }
    bool active_during(SongNs lo,SongNs hi,std::size_t node,std::size_t begin,std::size_t end) const {
        if(pieces[begin].begin>hi||nodes[node].maximum_end<lo)return false;
        if(end-begin==1)return true;
        const auto middle=begin+(end-begin)/2;
        return active_during(lo,hi,node*2,begin,middle)||active_during(lo,hi,node*2+1,middle,end);
    }
    bool active_during(SongNs lo,SongNs hi) const {
        return !pieces.empty()&&active_during(lo,hi,1,0,pieces.size());
    }
};

struct Build {
    const SemanticChart& chart;
    PhigrosPoseEvaluator poses;
    PhigrosGeometryMap mapping;
    ContactCompileOptions options;
    BlockAreaField block_areas;
    double input_scale,input_min_scale;
    std::size_t visibility_checks=0;
    std::size_t projection_span_count=0;
    std::optional<std::array<std::uint32_t,4>> inset_raster_bounds; // x-low/high, y-low/high, inclusive.
    Build(const SemanticChart& chart_,PhigrosGeometry geometry,ContactCompileOptions options_)
        :chart(chart_),poses(chart_),mapping(geometry),options(std::move(options_)),block_areas(chart,options),input_scale(0),input_min_scale(0) {
        const auto viewport=mapping.content_viewport();
        input_scale=std::max(viewport.width/phigros_reference_width,viewport.height/phigros_reference_height);
        input_min_scale=std::min(viewport.width/phigros_reference_width,viewport.height/phigros_reference_height);
        if(options.normal_projection_inset_reference_pixels>0) {
            const auto xs=projection_slab(phigros_reference_width,options.normal_projection_inset_reference_pixels);
            const auto ys=projection_slab(phigros_reference_height,options.normal_projection_inset_reference_pixels);
            Range input_x{1,0},input_y{1,0};
            for(const double x:{xs.lo,xs.hi})for(const double y:{ys.lo,ys.hi}) {
                const auto mapped=mapping.map_reference_pixels({x,y});
                if(mapped.status!=GeometryStatus::mapped)
                    throw Failure("projection_inset_raster_unavailable","Inset target rectangle cannot map into input geometry");
                input_x.lo=std::min(input_x.lo,mapped.input_normalized.x);input_x.hi=std::max(input_x.hi,mapped.input_normalized.x);
                input_y.lo=std::min(input_y.lo,mapped.input_normalized.y);input_y.hi=std::max(input_y.hi,mapped.input_normalized.y);
            }
            const double xlo=std::ceil(input_x.lo*geometry.input_width),xhi=std::floor(input_x.hi*geometry.input_width);
            const double ylo=std::ceil(input_y.lo*geometry.input_height),yhi=std::floor(input_y.hi*geometry.input_height);
            if(xlo>xhi||ylo>yhi||xlo<0||ylo<0||xhi>=geometry.input_width||yhi>=geometry.input_height)
                throw Failure("projection_inset_raster_unavailable","Inset target rectangle contains no complete input pixel rectangle");
            inset_raster_bounds=std::array<std::uint32_t,4>{static_cast<std::uint32_t>(xlo),static_cast<std::uint32_t>(xhi),
                static_cast<std::uint32_t>(ylo),static_cast<std::uint32_t>(yhi)};
        }
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
    ProjectedPoint projected(const PhigrosLinePose& pose,const SemanticNote& note,double offset,SongNs time,bool allow_tangent_gap=false) const {
        const auto source=phigros_line_point(pose,note.position_x,note.side,offset);
        if(options.offscreen_policy==OffscreenPolicy::bounded_judgment_rectangle_candidate_v1) {
            const auto reserve=judgment_domain_reserve();
            return judgment_rectangle(source.reference_pixels,source.side_normal,pose.tangent,options.max_normal_projection_reference_pixels-reserve,
                allow_tangent_gap?std::numeric_limits<double>::max():options.max_tangent_error_reference_pixels-reserve,
                options.normal_projection_inset_reference_pixels,time);
        }
        return normal_intersection(source.reference_pixels,source.side_normal,options.max_normal_projection_reference_pixels,
            options.normal_projection_inset_reference_pixels,time);
    }
    Point projected_pixel(const PhigrosLinePose& pose,const SemanticNote& note,double offset,SongNs time,bool allow_tangent_gap=false) const {
        const auto target=projected(pose,note,offset,time,allow_tangent_gap);
        return projected_reference_pixel(target.point,time);
    }
    Point projected_reference_pixel(PosePoint target,SongNs time) const {
        const auto mapped=mapping.map_reference_pixels(target);
        if(mapped.status!=GeometryStatus::mapped||!mapped.pixel)
            throw Failure("uncertified_normal_projection","Projected normal intersection does not map inside declared input geometry",time,target);
        auto pixel=*mapped.pixel;
        if(inset_raster_bounds) {
            // Round a boundary inward at the raster stage only. This cannot
            // repair an invalid analytic point: normal_intersection already
            // proves the continuous reference rectangle. The changed rounding
            // remains within the same <sqrt(2) input-pixel error allowance.
            const auto& bounds=*inset_raster_bounds;
            pixel.x=std::clamp(pixel.x,bounds[0],bounds[1]);pixel.y=std::clamp(pixel.y,bounds[2],bounds[3]);
            const auto dx=static_cast<double>(pixel.x)-mapped.input_normalized.x*mapping.geometry().input_width;
            const auto dy=static_cast<double>(pixel.y)-mapped.input_normalized.y*mapping.geometry().input_height;
            if(std::hypot(dx,dy)>raster_error+input_scale*1e-6)
                throw Failure("projection_inset_raster_error","Inward raster rounding exceeds declared projection error reserve",time,target);
        }
        return {static_cast<double>(pixel.x)/(mapping.geometry().input_width-1),
                static_cast<double>(pixel.y)/(mapping.geometry().input_height-1)};
    }
    double projected_reserve() const {
        // Rasterization plus a conservative floating-point conditioning reserve
        // at the certified minimum denominator. This is not judgment tolerance.
        auto conditioning_extent=phigros_reference_width+phigros_reference_height+options.max_normal_projection_reference_pixels;
        if(options.normal_projection_inset_reference_pixels>0)conditioning_extent+=2*options.normal_projection_inset_reference_pixels;
        return raster_error+input_scale*(1e-6+64*std::numeric_limits<double>::epsilon()*conditioning_extent/projection_eta_floor);
    }
    double judgment_domain_reserve() const {
        // Bound the inverse geometry operator norm. The complete forward held
        // error (including rasterization) must fit INSIDE the judgment domain,
        // rather than being silently added to the declared 151.2px tolerance.
        const double reserve=options.max_path_error_pixels/input_min_scale+1e-6;
        if(!std::isfinite(reserve)||reserve>=options.max_tangent_error_reference_pixels||
           reserve>=options.max_normal_projection_reference_pixels)
            throw Failure("judgment_domain_precision_budget","Held/raster error exhausts the finite normal/tangent judgment domain");
        return reserve;
    }
    // Input Steps are integer pixels encoded with extent-1. Invert the
    // viewport's extent-based transform after decoding those integers.
    PosePoint block_area_reference(double x,double y) const {
        const auto& g=mapping.geometry();
        const auto view=mapping.content_viewport();
        const bool quarter=g.rotation==InputRotation::clockwise_90||g.rotation==InputRotation::clockwise_270;
        const double u=x/g.input_width,v=y/g.input_height;
        PosePoint display{u,v};
        switch(g.rotation) {
            case InputRotation::none:break;
            case InputRotation::clockwise_90:display={v,1-u};break;
            case InputRotation::clockwise_180:display={1-u,1-v};break;
            case InputRotation::clockwise_270:display={1-v,u};break;
        }
        return {(display.x*(quarter?g.input_height:g.input_width)-view.left)*phigros_reference_width/view.width,
            (display.y*(quarter?g.input_width:g.input_height)-view.top)*phigros_reference_height/view.height};
    }
    static bool block_area_outside_corners(BlockAreaField::Box box) {
        constexpr double inset=64;
        const bool horizontal=box.x.lo<inset||box.x.hi>phigros_reference_width-inset;
        const bool vertical=box.y.lo<inset||box.y.hi>phigros_reference_height-inset;
        return !horizontal||!vertical;
    }
    bool block_area_domain(const SemanticNote& note,std::size_t line,PosePoint shift,
                           NoteContactOwnership& owner,bool commit) {
        const auto reserve=judgment_domain_reserve();
        const auto fits=[](Range values,double limit){return values.lo>=-limit&&values.hi<=limit;};
        const double normal_limit=options.max_normal_projection_reference_pixels-reserve;
        const double tangent_limit=options.max_tangent_error_reference_pixels-reserve;
        const double side=note.side==NoteSide::above?1:-1;
        const auto dot=[&](double angle,double last,double x,double y) {
            return plus(product({x,x},trig(angle,last,true)),product({y,y},trig(angle,last,false)));
        };
        const auto normal=[&](double first,double last){return dot(first,last,-side*shift.y,-side*shift.x);};
        const auto tangent=[&](double first,double last){return dot(first,last,shift.x,-shift.y);};
        const auto radians=[&](SongNs t){return std::remainder(poses.line_at(line,t).rotation_degrees,360.)*pi/180;};
        const double anchor=radians(note.start_ns);
        const auto normal_at=BlockAreaField::shift(normal(anchor,anchor),owner.normal_projection_reference_pixels);
        if(!fits(normal_at,normal_limit))return false;
        double nlo=normal_at.lo,nhi=normal_at.hi;
        double tlo=std::numeric_limits<double>::infinity(),thi=-tlo;
        for(auto& span:owner.normal_projection_spans) {
            block_areas.consume(note.start_ns);
            if(span.judgment_domain_gap)return false;
            const auto angle=radians(span.begin_ns);
            const auto last=angle+slope_at(chart.lines[line],span.begin_ns).radians*static_cast<double>(span.end_ns-span.begin_ns);
            const auto n=plus({span.correction_lower_reference_pixels,span.correction_upper_reference_pixels},normal(angle,last));
            const auto t=plus({span.tangent_correction_lower_reference_pixels,span.tangent_correction_upper_reference_pixels},tangent(angle,last));
            if(!fits(n,normal_limit)||!fits(t,tangent_limit))return false;
            nlo=std::min(nlo,n.lo);nhi=std::max(nhi,n.hi);
            tlo=std::min(tlo,t.lo);thi=std::max(thi,t.hi);
            if(commit) {
                span.correction_lower_reference_pixels=n.lo;span.correction_upper_reference_pixels=n.hi;
                span.tangent_correction_lower_reference_pixels=t.lo;span.tangent_correction_upper_reference_pixels=t.hi;
                span.possible_edge_mask=16;
            }
        }
        // The last dynamic sample can be on a different native pose at a
        // discontinuous key. Its exact pose is separate from the left span.
        const bool dynamic_flick=note.kind==NoteKind::flick&&options.flick_path_policy==FlickPathPolicy::dynamic_line_v1;
        if(note.kind==NoteKind::hold||dynamic_flick) {
            const auto last=dynamic_flick?add(note.start_ns-options.flick_lead_ns,options.flick_travel_ns):
                (owner.hold_tracking_end_ns>=0?owner.hold_tracking_end_ns:note.end_ns);
            const double offset=dynamic_flick?options.flick_distance_reference_pixels*
                static_cast<double>(last-note.start_ns)/static_cast<double>(options.flick_travel_ns):0;
            const auto projected_last=projected(poses.line_at(line,last),note,offset,last);
            const auto angle=radians(last);
            const auto n=BlockAreaField::shift(normal(angle,angle),projected_last.correction);
            const auto t=BlockAreaField::shift(tangent(angle,angle),projected_last.tangent_correction);
            if(!fits(n,normal_limit)||!fits(t,tangent_limit))return false;
            nlo=std::min(nlo,n.lo);nhi=std::max(nhi,n.hi);
            tlo=std::min(tlo,t.lo);thi=std::max(thi,t.hi);
        }
        if(commit) {
            owner.normal_projection_reference_pixels=normal_at.lo;
            owner.projection_min_reference_pixels=nlo;owner.projection_max_reference_pixels=nhi;
            owner.tangent_projection_min_reference_pixels=tlo;owner.tangent_projection_max_reference_pixels=thi;
        }
        return !owner.normal_projection_spans.empty();
    }
    bool avoid_block_areas(Contact& contact,NoteContactOwnership& owner,const SemanticNote& note,std::size_t line) {
        if(!block_areas.area_count)return false;
        const auto& g=mapping.geometry();
        const int width=static_cast<int>(g.input_width),height=static_cast<int>(g.input_height);
        const auto raster_bounds=inset_raster_bounds.value_or(std::array<std::uint32_t,4>{0,g.input_width-1,0,g.input_height-1});
        struct Segment { BlockAreaField::Box box; SongNs first,last; };
        std::vector<Segment> segments;
        std::vector<std::pair<int,int>> pixels;
        pixels.reserve(contact.steps.size());segments.reserve(contact.steps.size()-1);
        BlockAreaField::Box path{{phigros_reference_width,0},{phigros_reference_height,0}};
        // Include the rendered integer pixel cell and guard against rounding
        // at a BlockArea edge. Consecutive steps enclose both a held position
        // and every point on a possible interpolated motion between them.
        const double margin=raster_error/input_min_scale+1e-6;
        if(!std::isfinite(margin))throw Failure("block_area_geometry","BlockArea requires a finite input geometry scale",note.start_ns);
        for(std::size_t i=0;i<contact.steps.size();++i) {
            pixels.push_back(pixel_point(contact.steps[i].point,width,height));
            const auto p=block_area_reference(pixels.back().first,pixels.back().second);
            path.x={std::min(path.x.lo,p.x),std::max(path.x.hi,p.x)};
            path.y={std::min(path.y.lo,p.y),std::max(path.y.hi,p.y)};
            if(i) {
                const auto q=block_area_reference(pixels[i-1].first,pixels[i-1].second);
                auto x=range(p.x,q.x),y=range(p.y,q.y);
                x.lo-=margin;x.hi+=margin;y.lo-=margin;y.hi+=margin;
                const auto end=contact.steps[i].song_ns;
                if(end>std::numeric_limits<SongNs>::max()-std::max(SongNs{0},block_areas.time_after))
                    throw Failure("block_area_time_range","BlockArea timing margin overflows the song clock",end);
                segments.push_back({{x,y},contact.steps[i-1].song_ns+block_areas.time_before,end+block_areas.time_after});
            }
        }
        const auto clear=[&](PosePoint shift,bool protect_corners) {
            return std::all_of(segments.begin(),segments.end(),[&](const auto& s){
                block_areas.consume(s.first);
                const BlockAreaField::Box box{BlockAreaField::shift(s.box.x,shift.x),BlockAreaField::shift(s.box.y,shift.y)};
                return (!protect_corners||block_area_outside_corners(box))&&block_areas.clear(box,s.first,s.last);
            });
        };
        if(clear({},false))return false;
        if(options.offscreen_policy!=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1)
            throw Failure("block_area_detour_requires_judgment_rectangle","Blocked contact requires a bounded normal/tangent judgment domain",note.start_ns);
        const auto pose=poses.line_at(line,note.start_ns);
        const auto target=phigros_line_point(pose,note.position_x,note.side);
        const auto view=mapping.content_viewport();
        const double ref_x=phigros_reference_width/view.width,ref_y=phigros_reference_height/view.height;
        PosePoint basis_x{ref_x,0},basis_y{0,ref_y};
        switch(g.rotation) {
            case InputRotation::none:break;
            case InputRotation::clockwise_90:basis_x={0,-ref_y};basis_y={ref_x,0};break;
            case InputRotation::clockwise_180:basis_x={-ref_x,0};basis_y={0,-ref_y};break;
            case InputRotation::clockwise_270:basis_x={0,ref_y};basis_y={-ref_x,0};break;
        }
        const auto translated=[&](PosePoint offset,int dx,int dy) {
            if(path.x.lo+offset.x< -1e-6||path.y.lo+offset.y< -1e-6||
               path.x.hi+offset.x>phigros_reference_width+1e-6||path.y.hi+offset.y>phigros_reference_height+1e-6)return false;
            if(!block_area_domain(note,line,offset,owner,false))return false;
            for(const auto& [x,y]:pixels) {
                block_areas.consume(note.start_ns);
                if(x+dx<static_cast<int>(raster_bounds[0])||x+dx>static_cast<int>(raster_bounds[1])||
                   y+dy<static_cast<int>(raster_bounds[2])||y+dy>static_cast<int>(raster_bounds[3]))return false;
            }
            if(!clear(offset,true))return false;
            const auto update=[&](PosePoint p){return PosePoint{p.x+offset.x,p.y+offset.y};};
            const auto vertex_matches=[&](PosePoint point,std::size_t index){
                const auto p=update(point);
                const auto xs=projection_slab(phigros_reference_width,options.normal_projection_inset_reference_pixels);
                const auto ys=projection_slab(phigros_reference_height,options.normal_projection_inset_reference_pixels);
                if(p.x<xs.lo||p.x>xs.hi||p.y<ys.lo||p.y>ys.hi)return false;
                const auto raster=pixel_point(projected_reference_pixel(p,contact.steps[index].song_ns),width,height);
                return raster.first==pixels[index].first+dx&&raster.second==pixels[index].second+dy;
            };
            if(owner.flick_discrete_path) {
                const auto& flick=*owner.flick_discrete_path;
                if(!owner.flick_anchor_reference||!vertex_matches(*owner.flick_anchor_reference,0)||
                   !vertex_matches(flick.bend_reference,1)||!vertex_matches(flick.end_reference,pixels.size()-2)||
                   (flick.middle&&!vertex_matches(flick.middle->reference,2)))return false;
            }
            for(std::size_t i=0;i<pixels.size();++i)
                contact.steps[i].point={static_cast<double>(pixels[i].first+dx)/(width-1),static_cast<double>(pixels[i].second+dy)/(height-1)};
            (void)block_area_domain(note,line,offset,owner,true);
            if(owner.flick_anchor_reference)*owner.flick_anchor_reference=update(*owner.flick_anchor_reference);
            if(owner.flick_discrete_path) {
                auto& flick=*owner.flick_discrete_path;
                flick.bend_reference=update(flick.bend_reference);flick.end_reference=update(flick.end_reference);
                if(flick.middle)flick.middle->reference=update(flick.middle->reference);
            }
            owner.block_area_translation_reference=offset;
            return true;
        };
        // Deterministic outward grid in note-local normal/tangent coordinates.
        // The cap is the screen diagonal, independent of the declared normal bound.
        const int spacing=options.allow_block_area_temporal_fallback?24:16;
        const int normal_steps=1472/spacing,tangent_steps=144/spacing;
        const auto search_start=block_areas.work;
        const auto search_budget=options.allow_block_area_temporal_fallback?160'000u:1'000'000u;
        std::set<std::pair<int,int>> visited{{0,0}};
        const auto search=[&](int n,int t) {
            if(block_areas.work-search_start>=search_budget)return false;
            block_areas.consume(note.start_ns);
            const PosePoint requested{spacing*(n*target.side_normal.x+t*pose.tangent.x),
                spacing*(n*target.side_normal.y+t*pose.tangent.y)};
            const auto dx=static_cast<int>(std::lround(basis_x.x==0?requested.y/basis_x.y:requested.x/basis_x.x));
            const auto dy=static_cast<int>(std::lround(basis_y.x==0?requested.y/basis_y.y:requested.x/basis_y.x));
            if(!visited.emplace(dx,dy).second)return false;
            const PosePoint offset{dx*basis_x.x+dy*basis_y.x,dx*basis_x.y+dy*basis_y.y};
            return translated(offset,dx,dy);
        };
        for(int radius=1;radius<=normal_steps&&block_areas.work-search_start<search_budget;++radius) {
            if(search(-radius,0)||search(radius,0))return true;
            for(int t=1;t<=std::min(radius,tangent_steps);++t)
                if(search(-radius,-t)||search(radius,-t)||search(-radius,t)||search(radius,t))return true;
            if(radius<=tangent_steps)
                for(int n=1-radius;n<radius;++n)
                    if(search(n,-radius)||search(n,radius))return true;
        }
        throw Failure("block_area_no_safe_path","No continuously clear bounded BlockArea detour was certified for this contact",note.start_ns,target.reference_pixels);
    }
    bool block_area_temporal_route(Contact& contact,NoteContactOwnership& owner,const SemanticNote& note,std::size_t line) {
        if(!options.allow_block_area_temporal_fallback||!block_areas.area_count)return false;
        const bool flick=note.kind==NoteKind::flick;
        const bool sampled=options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_sampled_v2;
        const bool snap=options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_snap_v1;
        if(flick&&((!sampled&&!snap)||note.start_ns<options.flick_lead_ns))return false;
        const auto first=flick?note.start_ns-options.flick_lead_ns:note.start_ns;
        const auto stroke_end=flick?add(first,options.flick_travel_ns):first;
        const auto last=flick?add(stroke_end,options.flick_release_ns):note.kind==NoteKind::hold?note.end_ns:
            add(first,note.kind==NoteKind::tap?options.tap_duration_ns:options.drag_duration_ns);
        if(last<=first||last>std::numeric_limits<SongNs>::max()-std::max(SongNs{0},block_areas.time_after)||
           !block_areas.active_during(first+block_areas.time_before,last+block_areas.time_after))return false;
        const auto xs=projection_slab(phigros_reference_width,options.normal_projection_inset_reference_pixels);
        const auto ys=projection_slab(phigros_reference_height,options.normal_projection_inset_reference_pixels);
        const auto target=phigros_line_point(poses.line_at(line,note.start_ns),note.position_x,note.side).reference_pixels;
        const auto& g=mapping.geometry();
        const double margin=raster_error/input_min_scale+1e-6;
        const auto contains=[&](PosePoint p){return p.x>=xs.lo&&p.x<=xs.hi&&p.y>=ys.lo&&p.y<=ys.hi;};
        const auto actual=[&](PosePoint p,SongNs time){
            const auto normalized=projected_reference_pixel(p,time);
            const auto [x,y]=pixel_point(normalized,g.input_width,g.input_height);
            return std::pair{normalized,block_area_reference(x,y)};
        };
        const auto clear=[&](PosePoint a,PosePoint b,SongNs start,SongNs end){
            block_areas.consume(start);
            const BlockAreaField::Box box{{std::min(a.x,b.x)-margin,std::max(a.x,b.x)+margin},
                {std::min(a.y,b.y)-margin,std::max(a.y,b.y)+margin}};
            return block_area_outside_corners(box)&&block_areas.clear(box,start+block_areas.time_before,end+block_areas.time_after);
        };
        std::vector<PosePoint> candidates{{std::clamp(target.x,xs.lo,xs.hi),std::clamp(target.y,ys.lo,ys.hi)}};
        constexpr double spacing=24;
        for(double y=ys.lo;;y=std::min(y+spacing,ys.hi)) {
            for(double x=xs.lo;;x=std::min(x+spacing,xs.hi)) {
                candidates.push_back({x,y});if(x==xs.hi)break;
            }
            if(y==ys.hi)break;
        }
        std::stable_sort(candidates.begin(),candidates.end(),[&](PosePoint a,PosePoint b){
            return norm({a.x-target.x,a.y-target.y})<norm({b.x-target.x,b.y-target.y});
        });
        const auto accept=[&](bool track_full_hold=false){
            owner.block_area_temporal_fallback=true;
            if(note.kind==NoteKind::hold&&options.hold_tail_policy==HoldTailPolicy::phira_tail_grace_candidate_v1)
                owner.hold_tracking_end_ns=track_full_hold?note.end_ns:std::max(note.start_ns,note.end_ns-220'000'000);
            return true;
        };
        for(const auto anchor:candidates) {
            const auto [input,reference]=actual(anchor,first);
            if(!flick) {
                if(!clear(reference,reference,first,last))continue;
                append(contact,first,Phase::down,input,true);append(contact,last,Phase::up,input,true);
                return accept();
            }
            const SongNs post=stroke_end-note.start_ns;
            if(post<(sampled?7:3)||(sampled&&post>std::numeric_limits<SongNs>::max()/4))return false;
            const auto bend_time=add(note.start_ns,post/(sampled?7:3));
            const auto middle_time=sampled?add(note.start_ns,post*4/7):stroke_end;
            constexpr double diagonal=0.70710678118654752440,minor=.075;
            const double major=std::sqrt(1-minor*minor);
            const double distance=options.flick_distance_reference_pixels;
            if(!clear(reference,reference,first,bend_time))continue;
            const std::array<PosePoint,12> directions{{{diagonal,diagonal},{-diagonal,diagonal},{diagonal,-diagonal},{-diagonal,-diagonal},
                {major,minor},{-major,minor},{major,-minor},{-major,-minor},
                {minor,major},{-minor,major},{minor,-major},{-minor,-major}}};
            for(const auto direction:directions)
                for(const double sign:{1.,-1.}) {
                    const bool narrow=std::abs(direction.x)!=std::abs(direction.y);
                    const PosePoint perpendicular{-direction.y,direction.x};
                    const auto vertex=[&](double travel,double curve){return PosePoint{
                        anchor.x+distance*(travel*direction.x+sign*curve*perpendicular.x),
                        anchor.y+distance*(travel*direction.y+sign*curve*perpendicular.y)};};
                    const auto bend=sampled?vertex(.20,narrow?.018:.10):vertex(.28,narrow?.022:.10);
                    const auto middle=sampled?vertex(.55,narrow?.030:.12):vertex(1,0);
                    const auto end=vertex(1,0);
                    if(!contains(bend)||!contains(middle)||!contains(end))continue;
                    const auto [bend_input,bend_ref]=actual(bend,bend_time);
                    const auto [middle_input,middle_ref]=actual(middle,middle_time);
                    const auto [end_input,end_ref]=actual(end,stroke_end);
                    const auto length=[](PosePoint a,PosePoint b){return norm({a.x-b.x,a.y-b.y});};
                    const auto jump=[&](Point a,Point b){return std::hypot((a.x-b.x)*(g.input_width-1),(a.y-b.y)*(g.input_height-1));};
                    const auto turn=[](PosePoint a,PosePoint b,PosePoint c){return (b.x-a.x)*(c.y-b.y)-(b.y-a.y)*(c.x-b.x);};
                    const auto jump1=jump(input,bend_input),jump2=jump(bend_input,middle_input),jump3=jump(middle_input,end_input);
                    if(end_ref.x==reference.x||end_ref.y==reference.y||jump1<1||jump2<1||
                       jump(input,end_input)<std::max(1.,distance*input_min_scale-2*raster_error)||turn(reference,bend_ref,middle_ref)==0||
                       (sampled&&(jump3<1||turn(bend_ref,middle_ref,end_ref)==0)))continue;
                    if(!clear(reference,bend_ref,first,bend_time)||!clear(bend_ref,middle_ref,bend_time,middle_time)||
                       (sampled&&!clear(middle_ref,end_ref,middle_time,stroke_end))||!clear(end_ref,end_ref,stroke_end,last))continue;
                    append(contact,first,Phase::down,input,true);append(contact,bend_time,Phase::move,bend_input,true);
                    if(sampled)append(contact,middle_time,Phase::move,middle_input,true);
                    append(contact,stroke_end,Phase::move,end_input,true);append(contact,last,Phase::up,end_input,true);
                    owner.flick_anchor_reference=anchor;owner.flick_direction_reference=direction;
                    owner.flick_discrete_path=FlickDiscretePathCertificate{
                        flick_path_policy_name(options.flick_path_policy),bend,end,bend_time,stroke_end,length(anchor,end),
                        length(anchor,bend)+length(bend,middle)+length(middle,end),std::max({jump1,jump2,jump3}),sampled?3u:2u};
                    if(sampled)owner.flick_discrete_path->middle=FlickDiscreteMiddleVertex{middle,middle_time};
                    return accept();
                }
        }
        if(note.kind!=NoteKind::hold)return false;
        // Stationary alternatives are preferable. When a moving area sweeps
        // every stationary point, try a bounded search with the same pointer
        // held from original DOWN to original UP. Only MOVE nodes are added.
        constexpr SongNs tick=48'000'000;
        constexpr std::size_t beam=24;
        if((last-first)/tick>=static_cast<SongNs>(std::min(options.max_steps_per_contact,std::size_t{512})-1))return false;
        struct Waypoint { PosePoint anchor,reference; Point input; std::size_t previous; double cost; };
        std::vector<std::vector<Waypoint>> route;
        const auto next_tick=[&](SongNs time){return time+std::min(tick,last-time);};
        const auto apart=[](PosePoint a,PosePoint b){return norm({a.x-b.x,a.y-b.y});};
        const auto insert_spaced=[&](std::vector<Waypoint>& layer,const Waypoint& point){
            if(std::none_of(layer.begin(),layer.end(),[&](const auto& prior){return apart(point.reference,prior.reference)<spacing*1.5;}))
                layer.push_back(point);
        };
        route.emplace_back();
        for(const auto anchor:candidates) {
            const auto [input,reference]=actual(anchor,first);
            if(clear(reference,reference,first,next_tick(first)))
                insert_spaced(route.back(),{anchor,reference,input,0,apart(reference,target)*.05});
            if(route.back().size()==beam)break;
        }
        if(route.back().empty())return false;
        for(auto time=next_tick(first);time<last;time=next_tick(time)) {
            const auto end=next_tick(time);
            std::vector<Waypoint> next;
            const auto& previous=route.back();
            for(std::size_t i=0;i<previous.size();++i) {
                const auto& prior=previous[i];
                for(int x=-2;x<=2;++x)for(int y=-2;y<=2;++y) {
                    const PosePoint anchor{std::clamp(prior.anchor.x+spacing*x,xs.lo,xs.hi),
                        std::clamp(prior.anchor.y+spacing*y,ys.lo,ys.hi)};
                    const auto [input,reference]=actual(anchor,time);
                    if(!clear(reference,reference,time,end))continue;
                    const double move=apart(prior.reference,reference);
                    if(move>0&&!clear(prior.reference,reference,time,time))continue;
                    next.push_back({anchor,reference,input,i,prior.cost+move+apart(reference,target)*.001});
                }
            }
            if(next.empty())return false;
            std::stable_sort(next.begin(),next.end(),[](const auto& a,const auto& b){return a.cost<b.cost;});
            route.emplace_back();
            for(const auto& candidate:next) {
                insert_spaced(route.back(),candidate);
                if(route.back().size()==beam)break;
            }
        }
        std::vector<Point> path(route.size());
        std::size_t index=0;
        for(std::size_t i=route.size();i-->0;) {
            const auto& waypoint=route[i][index];path[i]=waypoint.input;index=waypoint.previous;
        }
        append(contact,first,Phase::down,path.front(),true);
        for(std::size_t i=1;i<path.size();++i)
            append(contact,first+static_cast<SongNs>(i)*tick,Phase::move,path[i]);
        append(contact,last,Phase::up,path.back(),true);
        return accept(true);
    }
    void projected_segment(Contact& contact,const SemanticNote& note,std::size_t line_index,SongNs a,SongNs b,
            const std::function<double(SongNs)>& offset_at,double offset_slope,NoteContactOwnership& ownership,
            bool native_key,unsigned depth=0) {
        if(++visibility_checks>options.max_steps_in_plan*16)
            throw Failure("projection_work_budget","Normal-intersection proof exceeds finite work budget",a);
        const auto pose=poses.line_at(line_index,a);
        const auto slope=slope_at(chart.lines[line_index],a);
        const bool allow_gap=note.kind==NoteKind::hold&&options.max_hold_reacquire_gap_ns>0;
        const auto domain_reserve=options.offscreen_policy==OffscreenPolicy::bounded_judgment_rectangle_candidate_v1?judgment_domain_reserve():0;
        auto certificate=options.offscreen_policy==OffscreenPolicy::bounded_judgment_rectangle_candidate_v1?
            certify_judgment_rectangle(note,pose,slope,offset_at(a),offset_slope,b-a,
                options.max_normal_projection_reference_pixels-domain_reserve,options.max_tangent_error_reference_pixels-domain_reserve,input_scale,options.normal_projection_inset_reference_pixels,allow_gap):
            certify_projection(note,pose,slope,offset_at(a),offset_slope,b-a,
                options.max_normal_projection_reference_pixels,input_scale,options.normal_projection_inset_reference_pixels,options.tight_projection_speed_bounds);
        // Treat each uncertain/outside tangent interval conservatively as bad.
        // Localize it to <=1ms before counting the complete merged duration.
        if(certificate&&certificate->judgment_domain_gap&&b-a>1'000'000)certificate.reset();
        if(!certificate) {
            for(auto elapsed:{SongNs{0},(b-a)/2,b-a})
                (void)projected(advanced(pose,slope,elapsed),note,offset_at(a)+offset_slope*static_cast<double>(elapsed),add(a,elapsed),allow_gap);
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
            certificate->edge_mask,certificate->speed,certificate->tangent_correction.lo,certificate->tangent_correction.hi,certificate->judgment_domain_gap});
        ownership.projection_min_reference_pixels=std::min(ownership.projection_min_reference_pixels,certificate->correction.lo);
        ownership.projection_max_reference_pixels=std::max(ownership.projection_max_reference_pixels,certificate->correction.hi);
        if(certificate->judgment_domain_gap) {
            ownership.gap_tangent_projection_min_reference_pixels=std::min(ownership.gap_tangent_projection_min_reference_pixels,certificate->tangent_correction.lo);
            ownership.gap_tangent_projection_max_reference_pixels=std::max(ownership.gap_tangent_projection_max_reference_pixels,certificate->tangent_correction.hi);
        } else {
            ownership.tangent_projection_min_reference_pixels=std::min(ownership.tangent_projection_min_reference_pixels,certificate->tangent_correction.lo);
            ownership.tangent_projection_max_reference_pixels=std::max(ownership.tangent_projection_max_reference_pixels,certificate->tangent_correction.hi);
        }
        const auto at=[&](SongNs t){return projected_pixel(poses.line_at(line_index,t),note,offset_at(t),t,allow_gap);};
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
        ownership.tangent_projection_min_reference_pixels=std::min(ownership.tangent_projection_min_reference_pixels,end.tangent_correction);
        ownership.tangent_projection_max_reference_pixels=std::max(ownership.tangent_projection_max_reference_pixels,end.tangent_correction);
        append(contact,last,flick ? Phase::move : Phase::up,projected_pixel(poses.line_at(line_index,last),note,offset_at(last),last),true);
        if(flick)append(contact,add(last,options.flick_release_ns),Phase::up,contact.steps.back().point,true);
    }
    void validate_hold_gaps(const NoteContactOwnership& owner,SongNs first,SongNs last) const {
        if(!options.max_hold_reacquire_gap_ns)return;
        std::optional<SongNs> begin;SongNs end=first;bool had_gap=false;
        for(const auto& span:owner.normal_projection_spans) {
            if(span.judgment_domain_gap) {
                had_gap=true;
                if(!begin)begin=span.begin_ns;
                else if(span.begin_ns!=end)throw Failure("hold_reacquire_without_recovery","Hold gap certificate coverage is not contiguous",end);
                if(*begin<=first)throw Failure("hold_reacquire_without_recovery","Hold must establish valid judgment before any reacquire gap",*begin);
                end=span.end_ns;
                if(end-*begin>options.max_hold_reacquire_gap_ns)
                    throw Failure("hold_reacquire_gap_exceeded","Conservative merged Hold tangent-domain gap exceeds explicit <=20ms budget",*begin);
            } else if(begin) {
                if(span.begin_ns!=end||span.end_ns<=span.begin_ns)
                    throw Failure("hold_reacquire_without_recovery","Hold gap must be followed immediately by a positive valid recovery interval",end);
                begin.reset();
            }
        }
        if(begin||(had_gap&&end>=last))throw Failure("hold_reacquire_without_recovery","Hold must recover a valid domain before tracking end or tail grace",end);
    }
    void straight_flick(Contact& contact,const SemanticNote& note,std::size_t line_index,
            SongNs first,SongNs last,double projection,NoteContactOwnership& ownership) {
        const auto pose=poses.line_at(line_index,note.start_ns);
        const auto native=phigros_line_point(pose,note.position_x,note.side);
        const bool rectangle=options.offscreen_policy==OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;
        const bool dynamic=options.offscreen_policy==OffscreenPolicy::bounded_normal_intersection_candidate_v1||rectangle;
        auto anchor=dynamic ? projected(pose,note,0,note.start_ns).point :
            phigros_line_point(pose,note.position_x,note.side,projection).reference_pixels;
        auto direction=native.side_normal;
        const double velocity=options.flick_distance_reference_pixels/static_cast<double>(options.flick_travel_ns);
        unsigned anchor_edge=dynamic ? projected(pose,note,0,note.start_ns).edge : 16;
        if(options.flick_path_policy==FlickPathPolicy::judgment_anchored_screen_fit_v1) {
            // Solve a finite family of full straight strokes. Erode the target
            // rectangle by BOTH endpoint offsets, then intersect that anchor
            // rectangle with the native judgment normal. Thus tangent error at
            // judgment is zero and no stroke is clipped, shortened or clamped.
            const auto xs=projection_slab(phigros_reference_width,options.normal_projection_inset_reference_pixels);
            const auto ys=projection_slab(phigros_reference_height,options.normal_projection_inset_reference_pixels);
            const double before=velocity*static_cast<double>(first-note.start_ns),after=velocity*static_cast<double>(last-note.start_ns);
            const auto domain_reserve=rectangle?judgment_domain_reserve():0;
            const auto normal_bound=options.max_normal_projection_reference_pixels-domain_reserve;
            const auto tangent_bound=options.max_tangent_error_reference_pixels-domain_reserve;
            const PosePoint directions[]={native.side_normal,{-native.side_normal.x,-native.side_normal.y},
                pose.tangent,{-pose.tangent.x,-pose.tangent.y},{1,0},{-1,0},{0,1},{0,-1}};
            bool found=false;double best=std::numeric_limits<double>::infinity(),best_tangent=std::numeric_limits<double>::infinity();
            double tangent_correction=0;
            const auto accept=[&](PosePoint p,PosePoint d,double correction,double along,unsigned edge) {
                if(std::abs(along)<best_tangent||(std::abs(along)==best_tangent&&std::abs(correction)<best)) {
                    found=true;best=std::abs(correction);best_tangent=std::abs(along);anchor=p;direction=d;
                    projection=correction;tangent_correction=along;anchor_edge=edge;
                }
            };
            for(const auto d:directions) {
                const Range ax{xs.lo-std::min(before*d.x,after*d.x),xs.hi-std::max(before*d.x,after*d.x)};
                const Range ay{ys.lo-std::min(before*d.y,after*d.y),ys.hi-std::max(before*d.y,after*d.y)};
                if(ax.lo>ax.hi||ay.lo>ay.hi)continue;
                if(rectangle) {
                    const PosePoint p{std::clamp(native.reference_pixels.x,ax.lo,ax.hi),std::clamp(native.reference_pixels.y,ay.lo,ay.hi)};
                    const PosePoint delta{p.x-native.reference_pixels.x,p.y-native.reference_pixels.y};
                    const double normal=delta.x*native.side_normal.x+delta.y*native.side_normal.y;
                    const double along=delta.x*pose.tangent.x+delta.y*pose.tangent.y;
                    if(std::isfinite(normal)&&std::isfinite(along)&&std::abs(normal)<=normal_bound&&std::abs(along)<=tangent_bound) {
                        unsigned edge=0;if(native.reference_pixels.x<ax.lo)edge|=1;if(native.reference_pixels.x>ax.hi)edge|=2;
                        if(native.reference_pixels.y<ay.lo)edge|=4;if(native.reference_pixels.y>ay.hi)edge|=8;
                        accept(p,d,normal,along,edge?edge:16);
                    }
                }
                double lower=-normal_bound,upper=normal_bound;
                if(options.offscreen_policy==OffscreenPolicy::reject)lower=upper=0;
                unsigned low_edge=0,high_edge=0;bool possible=true;
                const auto axis=[&](double p,double n,Range slab,unsigned low,unsigned high) {
                    if(n==0) {if(p<slab.lo||p>slab.hi)possible=false;return;}
                    const double a=(slab.lo-p)/n,b=(slab.hi-p)/n;
                    const double lo=std::min(a,b),hi=std::max(a,b);
                    if(lo>lower) {lower=lo;low_edge=n>0?low:high;}
                    if(hi<upper) {upper=hi;high_edge=n>0?high:low;}
                };
                axis(native.reference_pixels.x,native.side_normal.x,ax,1,2);
                axis(native.reference_pixels.y,native.side_normal.y,ay,4,8);
                if(!possible||!std::isfinite(lower)||!std::isfinite(upper)||lower>upper)continue;
                const double correction=lower>0?lower:upper<0?upper:0;
                const unsigned edge=lower>0?low_edge:upper<0?high_edge:16;
                if(((edge==1||edge==2)&&std::abs(native.side_normal.x)<projection_eta_floor)||
                   ((edge==4||edge==8)&&std::abs(native.side_normal.y)<projection_eta_floor))continue;
                PosePoint p{native.reference_pixels.x+correction*native.side_normal.x,
                    native.reference_pixels.y+correction*native.side_normal.y};
                if(edge==1)p.x=ax.lo;if(edge==2)p.x=ax.hi;if(edge==4)p.y=ay.lo;if(edge==8)p.y=ay.hi;
                if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<ax.lo||p.x>ax.hi||p.y<ay.lo||p.y>ay.hi)continue;
                accept(p,d,correction,0,edge);
            }
            if(!found)throw Failure("screen_fit_flick_unavailable","No certified full stroke intersects the bounded native judgment normal",note.start_ns,native.reference_pixels);
            ownership.normal_projection_reference_pixels=projection;
            ownership.projection_min_reference_pixels=ownership.projection_max_reference_pixels=projection;
            ownership.tangent_projection_min_reference_pixels=ownership.tangent_projection_max_reference_pixels=tangent_correction;
            ownership.flick_anchor_reference=anchor;ownership.flick_direction_reference=direction;
        }
        const auto at=[&](SongNs time) {
            const double offset=velocity*static_cast<double>(static_cast<long double>(time)-note.start_ns);
            return PosePoint{anchor.x+direction.x*offset,anchor.y+direction.y*offset};
        };
        const auto xs=projection_slab(phigros_reference_width,options.normal_projection_inset_reference_pixels);
        const auto ys=projection_slab(phigros_reference_height,options.normal_projection_inset_reference_pixels);
        // A rectangle is convex. Endpoint membership certifies the full straight
        // stroke, independently of native line discontinuities during the stroke.
        for(const auto time:{first,last}) {
            const auto p=at(time);
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<xs.lo||p.x>xs.hi||p.y<ys.lo||p.y>ys.hi)
                throw Failure("straight_flick_outside_rectangle","Full judgment-anchored stroke must fit without clipping or shortening",time,p);
        }
        const double speed=input_scale*velocity;
        const long double allowance=(options.max_path_error_pixels-raster_error-input_scale*1e-6)/speed;
        if(!std::isfinite(speed)||speed<=0||allowance<1)
            throw Failure("straight_flick_resolution","Straight stroke cannot satisfy the discrete-held error bound",first);
        const SongNs boundaries[]={first,note.start_ns,last};
        std::size_t sampled=0;
        for(std::size_t i=0;i<2;++i) {
            const auto a=boundaries[i],b=boundaries[i+1];
            append(contact,a,i==0 ? Phase::down : Phase::move,projected_reference_pixel(at(a),a),true);
            const auto delta=allowance>=static_cast<long double>(b-a) ? b-a : static_cast<SongNs>(std::floor(allowance));
            for(SongNs t=a;b-t>delta;) {
                if(++sampled>options.max_steps_per_contact)
                    throw Failure("straight_flick_work_budget","Straight stroke sampling exceeds finite work budget, including duplicate pixels",t);
                t+=delta;
                append(contact,t,Phase::move,projected_reference_pixel(at(t),t));
            }
        }
        append(contact,last,Phase::move,projected_reference_pixel(at(last),last),true);
        append(contact,add(last,options.flick_release_ns),Phase::up,contact.steps.back().point,true);
        // Projection provenance applies at the judgment anchor only. This path
        // does not claim to follow the moving line or its projected trajectory.
        ownership.time_varying_normal_projection=false;
        if(dynamic) {
            if(++projection_span_count>options.max_steps_in_plan)
                throw Failure("projection_span_budget","Straight Flick anchor provenance exceeds plan budget",note.start_ns);
            ownership.normal_projection_spans.push_back({note.start_ns,note.start_ns,projection,projection,anchor_edge,0,
                ownership.tangent_projection_min_reference_pixels,ownership.tangent_projection_max_reference_pixels});
        }
    }
    void diagonal_snap_flick(Contact& contact,const SemanticNote& note,std::size_t line_index,
            SongNs first,SongNs last,NoteContactOwnership& ownership) {
        const auto pose=poses.line_at(line_index,note.start_ns);
        const auto native=phigros_line_point(pose,note.position_x,note.side);
        const bool rectangle=options.offscreen_policy==OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;
        const double reserve=rectangle?judgment_domain_reserve():0;
        const double normal_bound=options.offscreen_policy==OffscreenPolicy::reject?0:options.max_normal_projection_reference_pixels-reserve;
        const double tangent_bound=rectangle?options.max_tangent_error_reference_pixels-reserve:0;
        const auto xs=projection_slab(phigros_reference_width,options.normal_projection_inset_reference_pixels);
        const auto ys=projection_slab(phigros_reference_height,options.normal_projection_inset_reference_pixels);
        const double distance=options.flick_distance_reference_pixels;
        constexpr double diagonal=0.70710678118654752440;
        const PosePoint directions[]={{diagonal,diagonal},{-diagonal,diagonal},{diagonal,-diagonal},{-diagonal,-diagonal}};
        PosePoint anchor{},direction{},bend{},end{};double correction=0,tangent_correction=0;
        double best_tangent=std::numeric_limits<double>::infinity(),best_normal=std::numeric_limits<double>::infinity();
        double best_clearance=-std::numeric_limits<double>::infinity();unsigned anchor_edge=16;bool found=false;
        // The path starts AT the anchor and slips out after judgment. All three
        // vertices are inside one rectangle, so their entire conceptual polyline
        // is inside it. We claim only these discrete commanded vertices; neither
        // touch transport nor Android is assumed to interpolate this polyline.
        const auto accept=[&](PosePoint p,PosePoint d,PosePoint turn,PosePoint endpoint,double n,double t,unsigned edge) {
            const PosePoint finish{p.x+endpoint.x,p.y+endpoint.y};
            const double clearance=std::min({finish.x-xs.lo,xs.hi-finish.x,finish.y-ys.lo,ys.hi-finish.y});
            const auto score=std::tuple(std::abs(t),std::abs(n),-clearance);
            if(!found||score < std::tuple(best_tangent,best_normal,-best_clearance)) {
                found=true;best_tangent=std::abs(t);best_normal=std::abs(n);best_clearance=clearance;
                anchor=p;direction=d;bend={p.x+turn.x,p.y+turn.y};end=finish;
                correction=n;tangent_correction=t;anchor_edge=edge;
            }
        };
        for(const auto d:directions)for(const double sign:{1.,-1.}) {
            const PosePoint perpendicular{-d.y,d.x};
            const PosePoint turn{distance*(.28*d.x+sign*.10*perpendicular.x),distance*(.28*d.y+sign*.10*perpendicular.y)};
            const PosePoint endpoint{distance*d.x,distance*d.y};
            const Range ax{xs.lo-std::min({0.,turn.x,endpoint.x}),xs.hi-std::max({0.,turn.x,endpoint.x})};
            const Range ay{ys.lo-std::min({0.,turn.y,endpoint.y}),ys.hi-std::max({0.,turn.y,endpoint.y})};
            if(ax.lo>ax.hi||ay.lo>ay.hi)continue;
            if(rectangle) {
                const PosePoint p{std::clamp(native.reference_pixels.x,ax.lo,ax.hi),std::clamp(native.reference_pixels.y,ay.lo,ay.hi)};
                const PosePoint delta{p.x-native.reference_pixels.x,p.y-native.reference_pixels.y};
                const double n=delta.x*native.side_normal.x+delta.y*native.side_normal.y,t=delta.x*pose.tangent.x+delta.y*pose.tangent.y;
                if(std::isfinite(n)&&std::isfinite(t)&&std::abs(n)<=normal_bound&&std::abs(t)<=tangent_bound) {
                    unsigned edge=0;if(native.reference_pixels.x<ax.lo)edge|=1;if(native.reference_pixels.x>ax.hi)edge|=2;
                    if(native.reference_pixels.y<ay.lo)edge|=4;if(native.reference_pixels.y>ay.hi)edge|=8;
                    accept(p,d,turn,endpoint,n,t,edge?edge:16);
                }
            }
            double lower=-normal_bound,upper=normal_bound;unsigned low_edge=0,high_edge=0;bool possible=true;
            const auto axis=[&](double p,double n,Range slab,unsigned low,unsigned high) {
                if(n==0){if(p<slab.lo||p>slab.hi)possible=false;return;}
                const double a=(slab.lo-p)/n,b=(slab.hi-p)/n;
                const double lo=std::min(a,b),hi=std::max(a,b);
                if(lo>lower){lower=lo;low_edge=n>0?low:high;}
                if(hi<upper){upper=hi;high_edge=n>0?high:low;}
            };
            axis(native.reference_pixels.x,native.side_normal.x,ax,1,2);axis(native.reference_pixels.y,native.side_normal.y,ay,4,8);
            if(!possible||!std::isfinite(lower)||!std::isfinite(upper)||lower>upper)continue;
            const double n=lower>0?lower:upper<0?upper:0;const unsigned edge=lower>0?low_edge:upper<0?high_edge:16;
            if(((edge==1||edge==2)&&std::abs(native.side_normal.x)<projection_eta_floor)||
               ((edge==4||edge==8)&&std::abs(native.side_normal.y)<projection_eta_floor))continue;
            PosePoint p{native.reference_pixels.x+n*native.side_normal.x,native.reference_pixels.y+n*native.side_normal.y};
            if(edge==1)p.x=ax.lo;if(edge==2)p.x=ax.hi;if(edge==4)p.y=ay.lo;if(edge==8)p.y=ay.hi;
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<ax.lo||p.x>ax.hi||p.y<ay.lo||p.y>ay.hi)continue;
            accept(p,d,turn,endpoint,n,0,edge);
        }
        if(!found)throw Failure("diagonal_snap_unavailable","No complete diagonal snap fits the declared judgment domain and rectangle",note.start_ns,native.reference_pixels);
        for(const auto p:{anchor,bend,end})if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<xs.lo||p.x>xs.hi||p.y<ys.lo||p.y>ys.hi)
            throw Failure("diagonal_snap_unavailable","Discrete snap vertex cannot be represented in the certified rectangle",note.start_ns,p);
        if(last-note.start_ns<3)throw Failure("diagonal_snap_resolution","Snap needs distinct anchor, bend and endpoint times",note.start_ns);
        const auto bend_time=note.start_ns+(last-note.start_ns)/3;
        append(contact,first,Phase::down,projected_reference_pixel(anchor,first),true);
        append(contact,bend_time,Phase::move,projected_reference_pixel(bend,bend_time),true);
        append(contact,last,Phase::move,projected_reference_pixel(end,last),true);
        append(contact,add(last,options.flick_release_ns),Phase::up,contact.steps.back().point,true);
        const auto width=mapping.geometry().input_width,height=mapping.geometry().input_height;
        const auto a=pixel_point(contact.steps[0].point,width,height),b=pixel_point(contact.steps[1].point,width,height),c=pixel_point(contact.steps[2].point,width,height);
        const double bx=static_cast<double>(b.first)-a.first,by=static_cast<double>(b.second)-a.second;
        const double dx=static_cast<double>(c.first)-a.first,dy=static_cast<double>(c.second)-a.second;
        const double cross=bx*dy-by*dx,actual_distance=std::hypot(dx,dy);
        if(dx==0||dy==0||cross==0||std::hypot(bx,by)<1||
           actual_distance<std::max(1.,distance*input_min_scale-2*raster_error))
            throw Failure("diagonal_snap_raster_resolution","Raster geometry cannot retain a curved diagonal snap and its complete displacement",note.start_ns);
        ownership.normal_projection_reference_pixels=correction;ownership.time_varying_normal_projection=false;
        ownership.projection_min_reference_pixels=ownership.projection_max_reference_pixels=correction;
        ownership.tangent_projection_min_reference_pixels=ownership.tangent_projection_max_reference_pixels=tangent_correction;
        ownership.flick_anchor_reference=anchor;ownership.flick_direction_reference=direction;
        ownership.flick_discrete_path=FlickDiscretePathCertificate{"judgment-anchored-diagonal-snap-v1",bend,end,bend_time,last,
            norm({end.x-anchor.x,end.y-anchor.y}),norm({bend.x-anchor.x,bend.y-anchor.y})+norm({end.x-bend.x,end.y-bend.y}),
            std::max(std::hypot(bx,by),std::hypot(dx-bx,dy-by)),2};
        if(++projection_span_count>options.max_steps_in_plan)throw Failure("projection_span_budget","Snap anchor certificate exceeds finite plan budget",note.start_ns);
        ownership.normal_projection_spans.push_back({note.start_ns,note.start_ns,correction,correction,anchor_edge,0,tangent_correction,tangent_correction});
    }
    void diagonal_sampled_flick(Contact& contact,const SemanticNote& note,std::size_t line_index,
            SongNs first,SongNs last,NoteContactOwnership& ownership) {
        const auto pose=poses.line_at(line_index,note.start_ns);
        const auto native=phigros_line_point(pose,note.position_x,note.side);
        const bool rectangle=options.offscreen_policy==OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;
        const double reserve=rectangle?judgment_domain_reserve():0;
        const double normal_bound=options.offscreen_policy==OffscreenPolicy::reject?0:options.max_normal_projection_reference_pixels-reserve;
        const double tangent_bound=rectangle?options.max_tangent_error_reference_pixels-reserve:0;
        const auto xs=projection_slab(phigros_reference_width,options.normal_projection_inset_reference_pixels);
        const auto ys=projection_slab(phigros_reference_height,options.normal_projection_inset_reference_pixels);
        const double distance=options.flick_distance_reference_pixels;
        constexpr double diagonal=0.70710678118654752440;
        const PosePoint directions[]={{diagonal,diagonal},{-diagonal,diagonal},{diagonal,-diagonal},{-diagonal,-diagonal}};
        PosePoint anchor{},direction{},bend{},middle{},end{};double correction=0,tangent_correction=0;
        double best_tangent=std::numeric_limits<double>::infinity(),best_normal=std::numeric_limits<double>::infinity();
        double best_clearance=-std::numeric_limits<double>::infinity();unsigned anchor_edge=16;bool found=false;
        // V2 starts AT the original native anchor. All four
        // vertices are inside one rectangle, so their entire conceptual polyline
        // is inside it. We claim only these discrete commanded vertices; neither
        // touch transport nor Android is assumed to interpolate this polyline.
        const auto accept=[&](PosePoint p,PosePoint d,PosePoint turn,PosePoint middle_offset,PosePoint endpoint,double n,double t,unsigned edge) {
            const PosePoint finish{p.x+endpoint.x,p.y+endpoint.y};
            const double clearance=std::min({finish.x-xs.lo,xs.hi-finish.x,finish.y-ys.lo,ys.hi-finish.y});
            const auto score=std::tuple(std::abs(t),std::abs(n),-clearance);
            if(!found||score < std::tuple(best_tangent,best_normal,-best_clearance)) {
                found=true;best_tangent=std::abs(t);best_normal=std::abs(n);best_clearance=clearance;
                anchor=p;direction=d;bend={p.x+turn.x,p.y+turn.y};
                middle={p.x+middle_offset.x,p.y+middle_offset.y};end=finish;
                correction=n;tangent_correction=t;anchor_edge=edge;
            }
        };
        for(const auto d:directions)for(const double sign:{1.,-1.}) {
            const PosePoint perpendicular{-d.y,d.x};
            const PosePoint turn{distance*(.20*d.x+sign*.10*perpendicular.x),distance*(.20*d.y+sign*.10*perpendicular.y)};
            const PosePoint middle_offset{distance*(.55*d.x+sign*.12*perpendicular.x),distance*(.55*d.y+sign*.12*perpendicular.y)};
            const PosePoint endpoint{distance*d.x,distance*d.y};
            const Range ax{xs.lo-std::min({0.,turn.x,middle_offset.x,endpoint.x}),xs.hi-std::max({0.,turn.x,middle_offset.x,endpoint.x})};
            const Range ay{ys.lo-std::min({0.,turn.y,middle_offset.y,endpoint.y}),ys.hi-std::max({0.,turn.y,middle_offset.y,endpoint.y})};
            if(ax.lo>ax.hi||ay.lo>ay.hi)continue;
            if(rectangle) {
                const PosePoint p{std::clamp(native.reference_pixels.x,ax.lo,ax.hi),std::clamp(native.reference_pixels.y,ay.lo,ay.hi)};
                const PosePoint delta{p.x-native.reference_pixels.x,p.y-native.reference_pixels.y};
                const double n=delta.x*native.side_normal.x+delta.y*native.side_normal.y,t=delta.x*pose.tangent.x+delta.y*pose.tangent.y;
                if(std::isfinite(n)&&std::isfinite(t)&&std::abs(n)<=normal_bound&&std::abs(t)<=tangent_bound) {
                    unsigned edge=0;if(native.reference_pixels.x<ax.lo)edge|=1;if(native.reference_pixels.x>ax.hi)edge|=2;
                    if(native.reference_pixels.y<ay.lo)edge|=4;if(native.reference_pixels.y>ay.hi)edge|=8;
                    accept(p,d,turn,middle_offset,endpoint,n,t,edge?edge:16);
                }
            }
            double lower=-normal_bound,upper=normal_bound;unsigned low_edge=0,high_edge=0;bool possible=true;
            const auto axis=[&](double p,double n,Range slab,unsigned low,unsigned high) {
                if(n==0){if(p<slab.lo||p>slab.hi)possible=false;return;}
                const double a=(slab.lo-p)/n,b=(slab.hi-p)/n;
                const double lo=std::min(a,b),hi=std::max(a,b);
                if(lo>lower){lower=lo;low_edge=n>0?low:high;}
                if(hi<upper){upper=hi;high_edge=n>0?high:low;}
            };
            axis(native.reference_pixels.x,native.side_normal.x,ax,1,2);axis(native.reference_pixels.y,native.side_normal.y,ay,4,8);
            if(!possible||!std::isfinite(lower)||!std::isfinite(upper)||lower>upper)continue;
            const double n=lower>0?lower:upper<0?upper:0;const unsigned edge=lower>0?low_edge:upper<0?high_edge:16;
            if(((edge==1||edge==2)&&std::abs(native.side_normal.x)<projection_eta_floor)||
               ((edge==4||edge==8)&&std::abs(native.side_normal.y)<projection_eta_floor))continue;
            PosePoint p{native.reference_pixels.x+n*native.side_normal.x,native.reference_pixels.y+n*native.side_normal.y};
            if(edge==1)p.x=ax.lo;if(edge==2)p.x=ax.hi;if(edge==4)p.y=ay.lo;if(edge==8)p.y=ay.hi;
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<ax.lo||p.x>ax.hi||p.y<ay.lo||p.y>ay.hi)continue;
            accept(p,d,turn,middle_offset,endpoint,n,0,edge);
        }
        if(!found)throw Failure("diagonal_sampled_unavailable","No complete sampled diagonal fits the declared judgment domain and rectangle",note.start_ns,native.reference_pixels);
        for(const auto p:{anchor,bend,middle,end})if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<xs.lo||p.x>xs.hi||p.y<ys.lo||p.y>ys.hi)
            throw Failure("diagonal_sampled_unavailable","Discrete sampled vertex cannot be represented in the certified rectangle",note.start_ns,p);
        const auto post=last-note.start_ns;
        if(post<7)throw Failure("diagonal_sampled_resolution","Sampled Flick needs three distinct post-judgment MOVE times",note.start_ns);
        if(post>std::numeric_limits<SongNs>::max()/4)throw Failure("invalid_time","Sampled middle vertex time multiplication overflows",note.start_ns);
        const auto bend_time=add(note.start_ns,post/7),middle_time=add(note.start_ns,(post*4)/7);
        append(contact,first,Phase::down,projected_reference_pixel(anchor,first),true);
        append(contact,bend_time,Phase::move,projected_reference_pixel(bend,bend_time),true);
        append(contact,middle_time,Phase::move,projected_reference_pixel(middle,middle_time),true);
        append(contact,last,Phase::move,projected_reference_pixel(end,last),true);
        append(contact,add(last,options.flick_release_ns),Phase::up,contact.steps.back().point,true);
        const auto width=mapping.geometry().input_width,height=mapping.geometry().input_height;
        const auto a=pixel_point(contact.steps[0].point,width,height),b=pixel_point(contact.steps[1].point,width,height),c=pixel_point(contact.steps[2].point,width,height),d=pixel_point(contact.steps[3].point,width,height);
        const double bx=static_cast<double>(b.first)-a.first,by=static_cast<double>(b.second)-a.second;
        const double cx=static_cast<double>(c.first)-a.first,cy=static_cast<double>(c.second)-a.second;
        const double dx=static_cast<double>(d.first)-a.first,dy=static_cast<double>(d.second)-a.second;
        const double cross_first=bx*(cy-by)-by*(cx-bx),cross_last=(cx-bx)*(dy-cy)-(cy-by)*(dx-cx);
        const double actual_distance=std::hypot(dx,dy);
        if(dx==0||dy==0||cross_first==0||cross_last==0||std::hypot(bx,by)<1||
           std::hypot(cx-bx,cy-by)<1||std::hypot(dx-cx,dy-cy)<1||
           actual_distance<std::max(1.,distance*input_min_scale-2*raster_error))
            throw Failure("diagonal_sampled_raster_resolution","Raster geometry cannot retain all three sampled curved MOVE vertices and complete displacement",note.start_ns);
        ownership.normal_projection_reference_pixels=correction;ownership.time_varying_normal_projection=false;
        ownership.projection_min_reference_pixels=ownership.projection_max_reference_pixels=correction;
        ownership.tangent_projection_min_reference_pixels=ownership.tangent_projection_max_reference_pixels=tangent_correction;
        ownership.flick_anchor_reference=anchor;ownership.flick_direction_reference=direction;
        ownership.flick_discrete_path=FlickDiscretePathCertificate{"judgment-anchored-diagonal-sampled-v2",bend,end,bend_time,last,
            norm({end.x-anchor.x,end.y-anchor.y}),norm({bend.x-anchor.x,bend.y-anchor.y})+
                norm({middle.x-bend.x,middle.y-bend.y})+norm({end.x-middle.x,end.y-middle.y}),
            std::max({std::hypot(bx,by),std::hypot(cx-bx,cy-by),std::hypot(dx-cx,dy-cy)}),3,
            FlickDiscreteMiddleVertex{middle,middle_time}};
        if(++projection_span_count>options.max_steps_in_plan)throw Failure("projection_span_budget","Sampled anchor certificate exceeds finite plan budget",note.start_ns);
        ownership.normal_projection_spans.push_back({note.start_ns,note.start_ns,correction,correction,anchor_edge,0,tangent_correction,tangent_correction});
    }
};

void check_options(const ContactCompileOptions& options) {
    if(options.block_area_phase_ns&&(*options.block_area_phase_ns < -100'000'000||*options.block_area_phase_ns > 100'000'000))
        throw Failure("invalid_options","BlockArea phase must be within -100..100ms");
    if(!options.above_drag_source_ids.empty()&&!options.advance_above_drags)
        throw Failure("invalid_options","An above-Drag source filter requires the explicit above-Drag advance option");
    if(options.above_drag_source_ids.size()>2048)
        throw Failure("invalid_above_drag_source_filter","Above-Drag source filter exceeds2048 original identities");
    std::set<std::string> filtered_above_sources;
    for(const auto& id:options.above_drag_source_ids) {
        if(id.empty()||id.size()>512||!filtered_above_sources.insert(id).second)
            throw Failure("invalid_above_drag_source_filter","Above-Drag source identities must be unique nonempty strings of at most512 bytes");
    }
    if(!options.below_drag_source_ids.empty()&&!options.advance_below_drags)
        throw Failure("invalid_options","A below-Drag source filter requires the explicit below-Drag advance option");
    if(options.below_drag_source_ids.size()>2048)
        throw Failure("invalid_below_drag_source_filter","Below-Drag source filter exceeds2048 original identities");
    std::set<std::string> filtered_sources;
    for(const auto& id:options.below_drag_source_ids) {
        if(id.empty()||id.size()>512||!filtered_sources.insert(id).second)
            throw Failure("invalid_below_drag_source_filter","Below-Drag source identities must be unique nonempty strings of at most512 bytes");
    }
    if(options.coincident_flick_delay_ns!=0&&options.coincident_flick_delay_ns!=32'000'000)
        throw Failure("invalid_options","Coincident Flick delay must be explicitly0 or32ms");
    if(options.coincident_flick_delay_ns!=0&&options.flick_path_policy!=FlickPathPolicy::judgment_anchored_diagonal_snap_v1)
        throw Failure("coincident_flick_requires_discrete_path","Coincident Flick delay requires the explicit four-step discrete diagonal path");
    if(options.max_prejudge_ns<0||options.max_prejudge_ns>30'000'000||
       ((options.advance_above_drags||options.advance_below_drags)&&options.max_prejudge_ns==0))
        throw Failure("invalid_options","Pre-judge candidate must declare 0..30ms, with above/below Drag advance separately enabled");
    if(options.max_hold_reacquire_gap_ns<0||options.max_hold_reacquire_gap_ns>20'000'000||
       (options.max_hold_reacquire_gap_ns>0&&(options.offscreen_policy!=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1||
        options.hold_tail_policy!=HoldTailPolicy::phira_tail_grace_candidate_v1)))
        throw Failure("invalid_options","Hold reacquire candidate requires explicit rectangle geometry, recorded tail policy, and 0..20ms gap budget");
    if(options.flick_path_policy!=FlickPathPolicy::dynamic_line_v1&&
       options.flick_path_policy!=FlickPathPolicy::judgment_anchored_straight_v1&&
       options.flick_path_policy!=FlickPathPolicy::judgment_anchored_screen_fit_v1&&
       options.flick_path_policy!=FlickPathPolicy::judgment_anchored_diagonal_snap_v1&&
       options.flick_path_policy!=FlickPathPolicy::judgment_anchored_diagonal_sampled_v2)
        throw Failure("invalid_options","Unknown Flick path policy");
    if(options.hold_tail_policy!=HoldTailPolicy::full_pose_v1&&options.hold_tail_policy!=HoldTailPolicy::phira_tail_grace_candidate_v1)
        throw Failure("invalid_options","Unknown Hold tail policy");
    if(options.hold_tail_policy==HoldTailPolicy::phira_tail_grace_candidate_v1&&
       options.offscreen_policy!=OffscreenPolicy::bounded_normal_intersection_candidate_v1&&
       options.offscreen_policy!=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1)
        throw Failure("invalid_options","Hold tail candidate requires explicit bounded normal intersection geometry");
    if(options.max_contacts<1||options.max_contacts>10 || options.tap_duration_ns<=0 || options.drag_duration_ns<=0 ||
       options.flick_lead_ns<=0 || options.flick_travel_ns<=options.flick_lead_ns || options.flick_release_ns<=0 ||
       !std::isfinite(options.flick_distance_reference_pixels)||options.flick_distance_reference_pixels<=0 ||
       !std::isfinite(options.max_path_error_pixels)||options.max_path_error_pixels<=raster_error ||
       options.max_contacts_in_plan==0||options.max_contacts_in_plan>200000||options.max_steps_per_contact<2||options.max_steps_per_contact>2000000||
       options.max_steps_in_plan<2||options.max_steps_in_plan>2000000||options.max_diagnostics==0||options.max_diagnostics>2000000||
       options.interference_window_ns<0||!std::isfinite(options.interference_radius_pixels)||options.interference_radius_pixels<0)
        throw Failure("invalid_options", "Compiler capacity, timing, geometry error, or resource limit is invalid");
    if(!std::isfinite(options.max_normal_projection_reference_pixels)||options.max_normal_projection_reference_pixels<0||
       options.max_normal_projection_reference_pixels>1'000'000)
        throw Failure("invalid_options", "Normal projection bound must be finite and nonnegative");
    if(!std::isfinite(options.normal_projection_inset_reference_pixels)||options.normal_projection_inset_reference_pixels<0||
       options.normal_projection_inset_reference_pixels>100||
       (options.normal_projection_inset_reference_pixels>0&&options.offscreen_policy!=OffscreenPolicy::bounded_normal_intersection_candidate_v1&&
        options.offscreen_policy!=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1))
        throw Failure("invalid_options", "Normal projection inset must be finite, within 0..100, and positive only for dynamic normal intersections");
    if(options.tight_projection_speed_bounds&&options.offscreen_policy!=OffscreenPolicy::bounded_normal_intersection_candidate_v1&&
       options.offscreen_policy!=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1)
        throw Failure("invalid_options", "Analytic projected speed bounds require dynamic normal intersections");
    if(options.offscreen_policy!=OffscreenPolicy::reject&&options.offscreen_policy!=OffscreenPolicy::bounded_normal_candidate&&
       options.offscreen_policy!=OffscreenPolicy::bounded_normal_intersection_candidate_v1&&
       options.offscreen_policy!=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1)
        throw Failure("invalid_options", "Unknown offscreen policy");
    if(!std::isfinite(options.max_tangent_error_reference_pixels)||options.max_tangent_error_reference_pixels<0||
       options.max_tangent_error_reference_pixels>151.2||
       (options.offscreen_policy==OffscreenPolicy::bounded_judgment_rectangle_candidate_v1&&options.max_tangent_error_reference_pixels==0)||
       (options.offscreen_policy!=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1&&options.max_tangent_error_reference_pixels!=0))
        throw Failure("invalid_options","Only the explicit judgment rectangle may declare finite tangent tolerance in (0,151.2] reference pixels");
    if(options.offscreen_policy!=OffscreenPolicy::reject&&
       (!std::isfinite(options.max_normal_projection_reference_pixels)||options.max_normal_projection_reference_pixels<=0||options.calibration_id.empty()))
        throw Failure("invalid_options", "Projection candidates require a finite positive bound and pending calibration identity");
}
void validate_below_drag_sources(const SemanticChart& chart,const ContactCompileOptions& options) {
    if(options.below_drag_source_ids.empty())return;
    if(!sha256_text(chart.source.chart_sha256))
        throw Failure("invalid_below_drag_source_filter","Below-Drag filter requires the original chart SHA256");
    const auto prefix=chart.source.chart_sha256+'#';
    for(const auto& id:options.below_drag_source_ids)
        if(!id.starts_with(prefix))
            throw Failure("invalid_below_drag_source_filter","Below-Drag source identity belongs to a different chart");
    if(chart.lines.size()>200000)
        throw Failure("contact_budget","Below-Drag source lookup exceeds the finite chart-line budget");
    std::set<std::string> matched;
    std::size_t notes=0;
    for(const auto& line:chart.lines) {
        if(line.notes.size()>options.max_contacts_in_plan-notes)
            throw Failure("contact_budget","Below-Drag source lookup exceeds the configured complete note budget");
        notes+=line.notes.size();
        for(const auto& note:line.notes) {
            if(!std::binary_search(options.below_drag_source_ids.begin(),options.below_drag_source_ids.end(),note.source.id))continue;
            if(note.kind!=NoteKind::drag||note.side!=NoteSide::below||note.source.json_pointer.empty()||
               note.source.id!=prefix+note.source.json_pointer||!matched.insert(note.source.id).second)
                throw Failure("invalid_below_drag_source_filter","Every filtered identity must name exactly one original below Drag",note.start_ns);
        }
    }
    if(matched.size()!=options.below_drag_source_ids.size())
        throw Failure("invalid_below_drag_source_filter","Below-Drag source filter contains an unknown original identity");
}
void validate_above_drag_sources(const SemanticChart& chart,const ContactCompileOptions& options) {
    if(options.above_drag_source_ids.empty())return;
    if(!sha256_text(chart.source.chart_sha256))
        throw Failure("invalid_above_drag_source_filter","Above-Drag filter requires the original chart SHA256");
    const auto prefix=chart.source.chart_sha256+'#';
    for(const auto& id:options.above_drag_source_ids)
        if(!id.starts_with(prefix))
            throw Failure("invalid_above_drag_source_filter","Above-Drag source identity belongs to a different chart");
    if(chart.lines.size()>200000)
        throw Failure("contact_budget","Above-Drag source lookup exceeds the finite chart-line budget");
    std::set<std::string> matched;
    std::size_t notes=0;
    for(const auto& line:chart.lines) {
        if(line.notes.size()>options.max_contacts_in_plan-notes)
            throw Failure("contact_budget","Above-Drag source lookup exceeds the configured complete note budget");
        notes+=line.notes.size();
        for(const auto& note:line.notes) {
            if(!std::binary_search(options.above_drag_source_ids.begin(),options.above_drag_source_ids.end(),note.source.id))continue;
            if(note.kind!=NoteKind::drag||note.side!=NoteSide::above||note.source.json_pointer.empty()||
               note.source.id!=prefix+note.source.json_pointer||!matched.insert(note.source.id).second)
                throw Failure("invalid_above_drag_source_filter","Every filtered identity must name exactly one original above Drag",note.start_ns);
        }
    }
    if(matched.size()!=options.above_drag_source_ids.size())
        throw Failure("invalid_above_drag_source_filter","Above-Drag source filter contains an unknown original identity");
}
std::string cache_key(const CompiledContactPlan& plan) {
    const auto& i=plan.memory.identity; const auto& g=plan.metadata.geometry; const auto& o=plan.metadata.options;
    std::ostringstream stream; stream.imbue(std::locale::classic());
    stream<<std::hexfloat;
    const auto text=[&](const std::string& value){stream<<value.size()<<':'<<value<<';';};
    text("pas.compiled-contact-fingerprint.v2");
    text(plan.metadata.compiler_version);text(plan.metadata.strategy);text(plan.metadata.projection_policy_version);
    if(!plan.metadata.judgment_policy_provenance.empty()) {text("judgment-policy-provenance.v1");text(plan.metadata.judgment_policy_provenance);}
    if(plan.metadata.source_format_version!=3) {text("native-source-format.v1");stream<<plan.metadata.source_format_version<<';';}
    if(plan.metadata.import_policy_id!="phigros-v3-strict-v1"||plan.metadata.import_interpretation_unqualified||!plan.metadata.import_anomaly_source_ids.empty()) {
        text("native-import-interpretation.v1");text(plan.metadata.import_policy_id);
        stream<<plan.metadata.import_interpretation_unqualified<<','<<plan.metadata.import_anomaly_source_ids.size()<<';';
        for(const auto& source:plan.metadata.import_anomaly_source_ids)text(source);
    }
    stream<<plan.metadata.requires_calibration<<','<<static_cast<int>(i.game)<<';';
    text(i.song_id);text(i.difficulty);text(i.revision);text(i.chart_sha256);text(i.audio_sha256);text(i.layout_id);
    stream<<g.input_width<<','<<g.input_height<<','<<static_cast<int>(g.rotation)<<','<<static_cast<int>(g.fit)<<','<<g.viewport.has_value()<<';';
    if(g.viewport) stream<<g.viewport->left<<','<<g.viewport->top<<','<<g.viewport->width<<','<<g.viewport->height<<';';
    stream<<o.max_contacts<<','<<o.tap_duration_ns<<','<<o.drag_duration_ns<<','<<o.flick_lead_ns<<','<<o.flick_travel_ns<<','<<o.flick_release_ns<<','
          <<o.flick_distance_reference_pixels<<','<<o.max_path_error_pixels<<','<<o.max_contacts_in_plan<<','<<o.max_steps_per_contact<<','<<o.max_steps_in_plan<<','
          <<o.max_diagnostics<<','<<o.interference_window_ns<<','<<o.interference_radius_pixels<<','<<static_cast<int>(o.offscreen_policy)<<','
          <<o.max_normal_projection_reference_pixels<<';';
    text(o.calibration_id);
    if(o.normal_projection_inset_reference_pixels!=0) {
        text("normal_projection_inset_reference_pixels.v1");stream<<o.normal_projection_inset_reference_pixels<<';';
    }
    if(o.tight_projection_speed_bounds)text("analytic-projected-derivative-v1");
    if(o.flick_path_policy!=FlickPathPolicy::dynamic_line_v1)text(flick_path_policy_name(o.flick_path_policy));
    if(o.hold_tail_policy!=HoldTailPolicy::full_pose_v1)text(hold_tail_policy_name(o.hold_tail_policy));
    if(o.max_tangent_error_reference_pixels!=0) {text("max-tangent-error-reference-pixels.v1");stream<<o.max_tangent_error_reference_pixels<<';';}
    if(o.max_prejudge_ns!=0) {text("phira-prejudge-candidate.v1");stream<<o.max_prejudge_ns<<','<<o.advance_above_drags<<';';}
    if(o.advance_below_drags)text("advance-below-drags-candidate-v1");
    if(!o.below_drag_source_ids.empty()) {
        auto ids=o.below_drag_source_ids;std::sort(ids.begin(),ids.end());
        text("below-drag-source-filter-candidate-v1");stream<<ids.size()<<':';
        for(const auto& id:ids)text(id);
    }
    if(!o.above_drag_source_ids.empty()) {
        auto ids=o.above_drag_source_ids;std::sort(ids.begin(),ids.end());
        text("above-drag-source-filter-candidate-v1");stream<<ids.size()<<':';
        for(const auto& id:ids)text(id);
    }
    if(o.max_hold_reacquire_gap_ns!=0) {text("phira-hold-reacquire-candidate.v1");stream<<o.max_hold_reacquire_gap_ns<<';';}
    if(o.coincident_flick_delay_ns!=0) {text("coincident-flick-delay-candidate.v1");stream<<o.coincident_flick_delay_ns<<';';}
    if(!plan.metadata.block_area_policy_id.empty()) {
        text(plan.metadata.block_area_policy_id);
        stream<<plan.metadata.block_area_count<<','<<plan.metadata.block_area_detoured_contacts<<','
            <<o.block_area_phase_ns.has_value()<<','<<o.block_area_phase_ns.value_or(0)<<';';
        if(o.allow_block_area_temporal_fallback) {
            text("block-area-temporal-fallback.v1");stream<<plan.metadata.block_area_temporal_contacts<<';';
        }
    }
    stream<<plan.ownership.size()<<':';
    for(const auto& owned:plan.ownership) {
        text(owned.source_note_id);text(owned.source_json_pointer);
        stream<<owned.line_index<<','<<owned.note_index<<','<<static_cast<int>(owned.kind)<<','<<static_cast<int>(owned.side)<<','
            <<owned.contact_id<<','<<owned.judgment_ns<<','<<owned.normal_projection_reference_pixels<<','
            <<owned.time_varying_normal_projection<<','<<owned.projection_min_reference_pixels<<','<<owned.projection_max_reference_pixels<<','
            <<owned.normal_projection_spans.size()<<';';
        for(const auto& span:owned.normal_projection_spans)stream<<span.begin_ns<<','<<span.end_ns<<','<<span.correction_lower_reference_pixels<<','
            <<span.correction_upper_reference_pixels<<','<<span.possible_edge_mask<<','<<span.speed_bound_input_pixels_per_ns<<';';
        if(owned.tangent_projection_min_reference_pixels!=0||owned.tangent_projection_max_reference_pixels!=0) {
            text("tangent-projection-extrema.v1");stream<<owned.tangent_projection_min_reference_pixels<<','<<owned.tangent_projection_max_reference_pixels<<';';
        }
        if(o.offscreen_policy==OffscreenPolicy::bounded_judgment_rectangle_candidate_v1) {
            text("judgment-rectangle-span-tangent-correction.v1");
            for(const auto& span:owned.normal_projection_spans)stream<<span.tangent_correction_lower_reference_pixels<<','<<span.tangent_correction_upper_reference_pixels<<';';
        }
        if(owned.hold_tracking_end_ns!=-1) {text("hold-tracking-end.v1");stream<<owned.hold_tracking_end_ns<<';';}
        if(owned.dispatch_judgment_ns!=-1) {text("dispatch-judgment.v1");stream<<owned.dispatch_judgment_ns<<';';}
        if(owned.block_area_translation_reference) {
            text("block-area-translation-reference.v1");
            stream<<owned.block_area_translation_reference->x<<','<<owned.block_area_translation_reference->y<<';';
        }
        if(owned.block_area_temporal_fallback)text("block-area-temporal-owner.v1");
        if(o.max_hold_reacquire_gap_ns!=0) {
            text("judgment-domain-gap-spans.v1");stream<<owned.gap_tangent_projection_min_reference_pixels<<','<<owned.gap_tangent_projection_max_reference_pixels<<';';
            for(const auto& span:owned.normal_projection_spans)stream<<span.judgment_domain_gap<<';';
        }
        if(owned.flick_anchor_reference||owned.flick_direction_reference) {
            text("screen-fit-flick-anchor-direction.v1");stream<<owned.flick_anchor_reference.has_value()<<','<<owned.flick_direction_reference.has_value()<<';';
            if(owned.flick_anchor_reference)stream<<owned.flick_anchor_reference->x<<','<<owned.flick_anchor_reference->y<<';';
            if(owned.flick_direction_reference)stream<<owned.flick_direction_reference->x<<','<<owned.flick_direction_reference->y<<';';
        }
        if(owned.flick_discrete_path) {
            const auto& p=*owned.flick_discrete_path;text("flick-discrete-path-certificate.v1");text(p.model_id);
            stream<<p.bend_reference.x<<','<<p.bend_reference.y<<','<<p.end_reference.x<<','<<p.end_reference.y<<','
                <<p.bend_ns<<','<<p.end_ns<<','<<p.endpoint_distance_reference_pixels<<','<<p.polyline_length_reference_pixels<<','
                <<p.max_jump_input_pixels<<','<<p.move_count<<';';
            if(p.middle) {
                text("flick-discrete-middle-vertex.v1");stream<<p.middle->reference.x<<','<<p.middle->reference.y<<','<<p.middle->song_ns<<';';
            }
        }
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
bool prejudge_geometry_failure(const std::string& code) {
    return code=="offscreen_target"||code=="projection_unavailable"||code=="judgment_domain_unreachable"||
        code=="straight_flick_outside_rectangle"||code=="screen_fit_flick_unavailable"||
        code=="diagonal_snap_unavailable"||code=="diagonal_snap_raster_resolution"||
        code=="diagonal_sampled_unavailable"||code=="diagonal_sampled_raster_resolution"||
        code=="uncertified_normal_projection"||code=="invisible_flick";
}
ContactCompileResult compile_prejudge(const SemanticChart& chart,const Identity& identity,
        PhigrosGeometry geometry,ContactCompileOptions options) {
    auto base=options;base.max_prejudge_ns=0;base.advance_above_drags=false;base.advance_below_drags=false;
    base.below_drag_source_ids.clear();
    base.above_drag_source_ids.clear();
    auto native=compile_phigros_contacts(chart,identity,geometry,base);
    for(const auto& d:native.diagnostics)if(d.severity==CompileSeverity::error&&
        !prejudge_geometry_failure(d.code)&&d.code!="capacity_exceeded")return native;
    std::set<std::string> native_geometry_failures;
    for(const auto& d:native.diagnostics)if(d.severity==CompileSeverity::error&&prejudge_geometry_failure(d.code))
        native_geometry_failures.insert(d.source_note_id);
    // At most three complete, independently bounded offline compilations. Each
    // source note still owns exactly one contact. Only an INTERNAL dispatch copy
    // changes time; the returned native judgment/source declaration is restored.
    auto dispatch=chart;bool changed=false;std::set<std::string> forced;
    for(auto& line:dispatch.lines)for(auto& note:line.notes) {
        const bool above=options.advance_above_drags&&note.kind==NoteKind::drag&&note.side==NoteSide::above&&
            (options.above_drag_source_ids.empty()||std::binary_search(options.above_drag_source_ids.begin(),options.above_drag_source_ids.end(),note.source.id));
        const bool below=options.advance_below_drags&&note.kind==NoteKind::drag&&note.side==NoteSide::below&&
            (options.below_drag_source_ids.empty()||std::binary_search(options.below_drag_source_ids.begin(),options.below_drag_source_ids.end(),note.source.id));
        if((note.kind==NoteKind::drag||note.kind==NoteKind::flick)&&note.start_ns>=options.max_prejudge_ns&&
           (above||below||native_geometry_failures.contains(note.source.id))) {
            note.start_ns-=options.max_prejudge_ns;changed=true;if(above||below)forced.insert(note.source.id);
        }
    }
    auto result=changed?compile_phigros_contacts(dispatch,identity,geometry,base):std::move(native);
    if(changed&&!result.plan&&!forced.empty()) {
        std::set<std::string> restore;
        for(const auto& d:result.diagnostics)if(d.severity==CompileSeverity::error&&prejudge_geometry_failure(d.code)&&
            forced.contains(d.source_note_id)&&!native_geometry_failures.contains(d.source_note_id))restore.insert(d.source_note_id);
        if(!restore.empty()) {
            for(auto& line:dispatch.lines)for(auto& note:line.notes)if(restore.contains(note.source.id))note.start_ns+=options.max_prejudge_ns;
            result=compile_phigros_contacts(dispatch,identity,geometry,base);
        }
    }
    if(!result.plan)return result;
    auto& plan=*result.plan;
    plan.metadata.options=options;plan.metadata.requires_calibration=true;
    plan.metadata.strategy+="; phira-prejudge-candidate-v1; native-judgment-preserved; exclusive-source-contact";
    if(options.advance_below_drags)plan.metadata.strategy+="; advance-below-drags-candidate-v1";
    if(!options.below_drag_source_ids.empty())plan.metadata.strategy+="; below-drag-source-filter-candidate-v1";
    if(!options.above_drag_source_ids.empty())plan.metadata.strategy+="; above-drag-source-filter-candidate-v1";
    if(plan.metadata.judgment_policy_provenance.empty())
        plan.metadata.judgment_policy_provenance="https://github.com/TeamFlos/phira/blob/750579396cbc5dc61e6379fbfb2aa1adb70751a6/prpr/src/judge.rs; community simulator candidate; proprietary-engine acceptance unverified";
    plan.metadata.judgment_policy_provenance+="; Drag/Flick pre-judge at native time or declared early endpoint <=30ms; native judgment_ns and source note retained";
    if(options.advance_below_drags)
        plan.metadata.judgment_policy_provenance+="; advance-below-drags-candidate-v1: separately enabled below-Drag early endpoint; above-Drag option unchanged, native judgment/source retained";
    if(!options.below_drag_source_ids.empty())
        plan.metadata.judgment_policy_provenance+="; below-drag-source-filter-candidate-v1: closed original below-Drag source set only; unspecified sources retain prior policy and native-geometry fallback; native judgment/source unchanged";
    if(!options.above_drag_source_ids.empty())
        plan.metadata.judgment_policy_provenance+="; above-drag-source-filter-candidate-v1: closed original above-Drag source set only; unspecified sources retain prior policy and native-geometry fallback; native judgment/source unchanged";
    for(auto& owner:plan.ownership) {
        if(owner.kind!=NoteKind::drag&&owner.kind!=NoteKind::flick)continue;
        const auto& source=chart.lines.at(owner.line_index).notes.at(owner.note_index);
        owner.dispatch_judgment_ns=owner.judgment_ns;owner.judgment_ns=source.start_ns;
        if(owner.dispatch_judgment_ns<source.start_ns) {
            if(result.diagnostics.size()>=options.max_diagnostics) {
                result.plan.reset();result.diagnostics.back()={CompileSeverity::error,"diagnostic_budget","Pre-judge provenance exceeds finite diagnostic budget",{}, {},0,0,0,{}};return result;
            }
            result.diagnostics.push_back({CompileSeverity::warning,"unverified_early_judgment",
                "Explicit Drag/Flick pre-judge candidate; original native judgment and source retained, actual game acceptance unverified",
                owner.source_note_id,{},owner.line_index,owner.note_index,owner.dispatch_judgment_ns,{}});
        }
    }
    plan.metadata.cache_sha256=cache_key(plan);
    return result;
}
ContactCompileResult compile_coincident_flick_delay(const SemanticChart& chart,const Identity& identity,
        PhigrosGeometry geometry,ContactCompileOptions options) {
    // Finish the complete original compile, including pre-judge restoration of
    // native ownership, BEFORE inspecting exact same-native-time groups. The
    // internal pre-judge dispatch chart must never supply this grouping key.
    auto base=options;base.coincident_flick_delay_ns=0;
    auto result=compile_phigros_contacts(chart,identity,geometry,base);
    if(!result.plan)return result;
    const auto limit=options.max_diagnostics;
    const auto reject=[&](CompileDiagnostic diagnostic){
        result.plan.reset();
        if(result.diagnostics.size()<limit)result.diagnostics.push_back(std::move(diagnostic));
        else if(!result.diagnostics.empty())result.diagnostics.back()={CompileSeverity::error,"diagnostic_budget",
            "Coincident Flick candidate exceeds the finite diagnostic budget",{},{},0,0,0,{}};
    };
    CompileDiagnostic context;context.severity=CompileSeverity::error;
    try {
        auto& plan=*result.plan;
        if(plan.ownership.size()!=plan.memory.contacts.size())
            throw Failure("coincident_flick_owner_mismatch","Complete contact and ownership arrays differ");
        // One bounded O(N log N) native-head index; no pairwise source scan.
        std::vector<SongNs> peer_heads;peer_heads.reserve(plan.ownership.size());
        for(const auto& owner:plan.ownership)
            if(owner.kind==NoteKind::tap||owner.kind==NoteKind::hold||owner.kind==NoteKind::drag)
                peer_heads.push_back(owner.judgment_ns);
        std::sort(peer_heads.begin(),peer_heads.end());
        peer_heads.erase(std::unique(peer_heads.begin(),peer_heads.end()),peer_heads.end());
        std::size_t shifted=0;
        for(std::size_t index=0;index<plan.ownership.size();++index) {
            auto& owner=plan.ownership[index];auto& contact=plan.memory.contacts[index];
            if(contact.id!=owner.contact_id)
                throw Failure("coincident_flick_owner_mismatch","Compiler source-order contact/ownership IDs differ");
            if(owner.kind!=NoteKind::flick||!std::binary_search(peer_heads.begin(),peer_heads.end(),owner.judgment_ns))continue;
            context.source_note_id=owner.source_note_id;context.line_index=owner.line_index;
            context.note_index=owner.note_index;context.song_ns=owner.judgment_ns;
            if(owner.dispatch_judgment_ns>=0&&owner.dispatch_judgment_ns<owner.judgment_ns)
                throw Failure("coincident_flick_prejudge_conflict","Cannot delay an already pre-judged Flick without a new geometry proof",owner.judgment_ns);
            if(!owner.flick_discrete_path||contact.steps.size()!=4||contact.steps[0].phase!=Phase::down||
                contact.steps[1].phase!=Phase::move||contact.steps[2].phase!=Phase::move||contact.steps[3].phase!=Phase::up)
                throw Failure("coincident_flick_requires_discrete_path","Matched Flick lacks its complete four-step discrete certificate",owner.judgment_ns);
            // All positions, projection spans and native/source identities stay
            // verbatim. Only commanded times and the discrete vertex times move.
            for(auto& step:contact.steps)step.song_ns=add(step.song_ns,options.coincident_flick_delay_ns);
            auto& path=*owner.flick_discrete_path;
            path.bend_ns=add(path.bend_ns,options.coincident_flick_delay_ns);
            path.end_ns=add(path.end_ns,options.coincident_flick_delay_ns);
            owner.dispatch_judgment_ns=add(owner.judgment_ns,options.coincident_flick_delay_ns);
            ++shifted;
        }
        plan.metadata.options=options;
        plan.metadata.strategy+="; coincident-flick-delay32-candidate-v1";
        if(!plan.metadata.judgment_policy_provenance.empty())plan.metadata.judgment_policy_provenance+="; ";
        plan.metadata.judgment_policy_provenance+="coincident-flick-delay32-candidate-v1: exact native-time Tap/Hold/Drag peer; complete discrete Flick contact delayed32ms with geometry/source/native judgment retained; experimental input handoff, proprietary-engine acceptance unverified";
        // Rebuild every global event and recompute real post-shift overlap.
        // Capacity is never inherited from the undelayed source plan.
        std::vector<std::pair<SongNs,int>> edges;edges.reserve(plan.memory.contacts.size()*2);
        plan.events.clear();plan.events.reserve(plan.total_steps);
        std::size_t steps=0;
        for(const auto& contact:plan.memory.contacts) {
            if(contact.steps.size()>options.max_steps_in_plan||steps>options.max_steps_in_plan-contact.steps.size())
                throw Failure("plan_step_budget","Coincident Flick event rebuild exceeds the finite step budget");
            steps+=contact.steps.size();
            edges.emplace_back(contact.steps.front().song_ns,1);edges.emplace_back(contact.steps.back().song_ns,-1);
            for(const auto& step:contact.steps)plan.events.push_back({step.song_ns,contact.id,step.phase,step.point});
        }
        if(steps!=plan.total_steps)throw Failure("coincident_flick_step_count","Timing-only handoff changed the complete step count");
        std::sort(edges.begin(),edges.end());int active=0,peak=0;
        for(const auto& edge:edges){active+=edge.second;peak=std::max(peak,active);}
        result.required_peak_contacts=peak;
        if(peak>options.max_contacts)
            throw Failure("capacity_exceeded","Coincident Flick candidate requires "+std::to_string(peak)+
                " contacts after the complete timing shift; configured capacity is "+std::to_string(options.max_contacts));
        try{plan.peak_contacts=validate(plan.memory,options.max_contacts);}
        catch(const std::invalid_argument& error){throw Failure("capacity_or_lifecycle",error.what());}
        std::sort(plan.events.begin(),plan.events.end(),[](const auto& a,const auto& b){
            return std::tuple(a.song_ns,phase_order(a.phase),a.contact_id)<std::tuple(b.song_ns,phase_order(b.phase),b.contact_id);
        });
        if(result.diagnostics.size()>=limit)
            throw Failure("diagnostic_budget","Coincident Flick provenance exceeds the finite diagnostic budget");
        result.diagnostics.push_back({CompileSeverity::warning,"unverified_coincident_flick_delay",
            "Explicit timing-only handoff delayed "+std::to_string(shifted)+
            " coincident discrete Flick contacts; no actual game judgment improvement is established",{},{},0,0,0,{}});
        plan.metadata.cache_sha256=cache_key(plan);
    }catch(const Failure& error){context.code=error.code;context.message=error.what();context.song_ns=error.time;context.reference_point=error.point;reject(std::move(context));}
     catch(const std::exception& error){context.code="invalid_chart_or_geometry";context.message=error.what();reject(std::move(context));}
    return result;
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
        std::sort(options.below_drag_source_ids.begin(),options.below_drag_source_ids.end());
        std::sort(options.above_drag_source_ids.begin(),options.above_drag_source_ids.end());
        validate_below_drag_sources(chart,options);
        validate_above_drag_sources(chart,options);
        if(options.coincident_flick_delay_ns>0) {
            if(std::any_of(chart.block_areas.begin(),chart.block_areas.end(),[](const auto& area){return area.enable_ns<area.disable_ns;}))
                throw Failure("block_area_time_shift_unavailable","Coincident Flick time shifts would invalidate the BlockArea clearance proof");
            return compile_coincident_flick_delay(chart,identity,geometry,options);
        }
        if(options.max_prejudge_ns>0)return compile_prejudge(chart,identity,geometry,options);
        if(identity.game!=Game::phigros||identity.song_id.empty()||identity.difficulty.empty()||identity.revision.empty()||identity.layout_id.empty()||
           !sha256_text(identity.chart_sha256)||!sha256_text(identity.audio_sha256)||
           identity.chart_sha256!=chart.source.chart_sha256||identity.song_id!=chart.source.song_id||
           identity.difficulty!=chart.source.difficulty||identity.revision!=chart.source.game_revision)
            throw Failure("identity_mismatch", "Chart source and complete chart/audio/layout identity must match exactly");
        Build build(chart,geometry,options);
        CompiledContactPlan plan; plan.memory.identity=std::move(identity);plan.metadata.geometry=geometry;plan.metadata.options=options;
        if(options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_snap_v1||
           options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_sampled_v2)
            plan.metadata.strategy="exclusive-note-ownership; raster-pixel-normalized; other-paths-discrete-held-error-bound; flick-explicit-discrete-vertices-no-continuous-target-claim";
        if(chart.import_policy_id.empty()||chart.import_policy_id.size()>256)
            throw Failure("invalid_import_policy", "Parsed chart import policy identity is required and bounded");
        if((chart.source_format_version!=1&&chart.source_format_version!=3)||
           (chart.source_format_version==1&&chart.import_policy_id!="phigros-v1-source-priority-constant-empty-candidate-v1"&&
            chart.import_policy_id!="phigros-v1-source-priority-empty-interval-candidate-v1"))
            throw Failure("invalid_import_policy","Native source format must match an explicit supported import interpretation");
        plan.metadata.import_policy_id=chart.import_policy_id;
        plan.metadata.source_format_version=chart.source_format_version;
        plan.metadata.import_interpretation_unqualified=chart.import_interpretation_unqualified;
        for(const auto& anomaly:chart.import_anomalies) {
            if(anomaly.source.id!=chart.source.chart_sha256+'#'+anomaly.source.json_pointer)
                throw Failure("invalid_import_policy", "Native import anomaly must retain exact chart source identity");
            plan.metadata.import_anomaly_source_ids.push_back(anomaly.source.id);
        }
        const bool rectangle_projection=options.offscreen_policy==OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;
        const bool dynamic_projection=options.offscreen_policy==OffscreenPolicy::bounded_normal_intersection_candidate_v1||rectangle_projection;
        plan.metadata.requires_calibration=options.offscreen_policy!=OffscreenPolicy::reject;
        if(dynamic_projection) {
            plan.metadata.compiler_version="phigros-contact-v2";
            plan.metadata.projection_policy_version="bounded_normal_intersection_candidate_v1";
            plan.metadata.strategy+=rectangle_projection?"; bounded-judgment-rectangle-candidate":"; bounded-normal-intersection-candidate";
            if(options.normal_projection_inset_reference_pixels>0)plan.metadata.strategy+="; reference-safe-inset-v1";
            if(options.tight_projection_speed_bounds)plan.metadata.strategy+="; analytic-projected-derivative-v1";
            if(rectangle_projection) {
                plan.metadata.projection_policy_version="bounded_judgment_rectangle_candidate_v1";
                plan.metadata.strategy+="; bounded-judgment-rectangle-candidate-v1; continuous-tangent-domain-certificate; nearest-rectangle-1-Lipschitz";
            }
        } else if(options.offscreen_policy==OffscreenPolicy::bounded_normal_candidate)
            plan.metadata.projection_policy_version="bounded_fixed_normal_candidate_v1";
        if(options.flick_path_policy!=FlickPathPolicy::dynamic_line_v1) {
            plan.metadata.strategy+="; "+std::string(flick_path_policy_name(options.flick_path_policy));
            plan.metadata.requires_calibration=true;
        }
        if(options.hold_tail_policy==HoldTailPolicy::phira_tail_grace_candidate_v1) {
            plan.metadata.strategy+="; phira-tail-grace-candidate-v1; original-hold-pointer-lifetime";
            plan.metadata.requires_calibration=true;
        }
        if(options.max_hold_reacquire_gap_ns>0) {
            plan.metadata.strategy+="; phira-hold-reacquire-candidate-v1; conservative-merged-tangent-gaps";
            plan.metadata.requires_calibration=true;
        }
        if(options.hold_tail_policy==HoldTailPolicy::phira_tail_grace_candidate_v1||
           options.flick_path_policy==FlickPathPolicy::judgment_anchored_screen_fit_v1||
           options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_snap_v1||
           options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_sampled_v2||rectangle_projection||options.max_hold_reacquire_gap_ns>0)
            plan.metadata.judgment_policy_provenance="https://github.com/TeamFlos/phira/blob/750579396cbc5dc61e6379fbfb2aa1adb70751a6/prpr/src/judge.rs; community simulator: local tangent-x judgment, direction-independent Flick tracker, Hold final 220ms pre-judge; offline candidate, proprietary-engine acceptance unverified";
        if(rectangle_projection)plan.metadata.judgment_policy_provenance+="; rectangle domain reserves complete held/raster error using inverse minimum geometry scale";
        if(options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_snap_v1)
            plan.metadata.judgment_policy_provenance+="; explicit discrete diagonal Flick: held judgment anchor then two curved MOVE vertices; no continuous target interpolation or held-error qualification for Flick";
        if(options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_sampled_v2)
            plan.metadata.judgment_policy_provenance+="; explicit sampled diagonal Flick v2: held native judgment anchor then three curved MOVE vertices; no continuous target interpolation, device sampling or actual game acceptance qualification";
        if(options.max_hold_reacquire_gap_ns>0)plan.metadata.judgment_policy_provenance+="; Hold tangent-only reacquire gaps conservatively merged <=20ms with valid recovery, within community 50ms grace; physical path and finite normal bound remain continuous";
        if(build.block_areas.area_count) {
            plan.metadata.block_area_policy_id="phira-block-area-continuous-detour-v1";
            plan.metadata.block_area_count=build.block_areas.area_count;
            plan.metadata.requires_calibration=true;
            plan.metadata.strategy+="; phira-block-area-continuous-detour-v1";
            if(!plan.metadata.judgment_policy_provenance.empty())plan.metadata.judgment_policy_provenance+="; ";
            plan.metadata.judgment_policy_provenance+="https://github.com/TeamFlos/phira/pull/901; community BlockArea geometry and sticky contact cancellation; conservative raster-path and phase-window proof; actual game acceptance unverified";
            if(options.allow_block_area_temporal_fallback) {
                plan.metadata.strategy+="; active-block-area-temporal-fallback-v1";
                plan.metadata.judgment_policy_provenance+="; explicitly opted-in temporal alternatives have no tangent/normal judgment assertion";
            }
        }
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
                Contact contact;contact.id=next_id++;
                NoteContactOwnership owned;
                owned.source_note_id=note.source.id;owned.source_json_pointer=note.source.json_pointer;owned.line_index=li;owned.note_index=ni;
                owned.kind=note.kind;owned.side=note.side;owned.contact_id=contact.id;owned.judgment_ns=note.start_ns;
                const auto span_count=build.projection_span_count;
                const auto regular_contact=[&]{
                const auto judgment_target=dynamic_projection ? build.projected(build.poses.line_at(li,note.start_ns),note,0,note.start_ns):ProjectedPoint{};
                const double projection=dynamic_projection ? judgment_target.correction : build.projection(note,li);
                owned.normal_projection_reference_pixels=projection;owned.time_varying_normal_projection=dynamic_projection;
                owned.projection_min_reference_pixels=owned.projection_max_reference_pixels=projection;
                owned.tangent_projection_min_reference_pixels=owned.tangent_projection_max_reference_pixels=judgment_target.tangent_correction;
                if(note.kind==NoteKind::hold) {
                    if(note.end_ns==note.start_ns) throw Failure("zero_length_hold", "A Hold must have positive contact duration",note.start_ns);
                    const bool tail=options.hold_tail_policy==HoldTailPolicy::phira_tail_grace_candidate_v1;
                    const auto tracking_end=tail?std::max(note.start_ns,note.end_ns-220'000'000):note.end_ns;
                    if(tail)owned.hold_tracking_end_ns=tracking_end;
                    if(tracking_end==note.start_ns) {
                        const auto pose=build.poses.line_at(li,note.start_ns);
                        const auto point=dynamic_projection?build.projected_pixel(pose,note,0,note.start_ns):build.point(pose,note,projection,note.start_ns);
                        build.append(contact,note.start_ns,Phase::down,point,true);
                        build.append(contact,note.end_ns,Phase::up,point,true);
                        if(dynamic_projection) {
                            if(++build.projection_span_count>options.max_steps_in_plan)
                                throw Failure("projection_span_budget","Short Hold judgment certificate exceeds finite plan budget",note.start_ns);
                            owned.normal_projection_spans.push_back({note.start_ns,note.start_ns,projection,projection,judgment_target.edge,0,
                                judgment_target.tangent_correction,judgment_target.tangent_correction});
                        }
                    } else {
                        if(dynamic_projection)build.projected_trajectory(contact,note,li,note.start_ns,tracking_end,false,owned);
                        else build.trajectory(contact,note,li,note.start_ns,tracking_end,projection,false);
                        if(tail) {
                            contact.steps.back().phase=Phase::move;
                            build.append(contact,note.end_ns,Phase::up,contact.steps.back().point,true);
                        }
                    }
                    build.validate_hold_gaps(owned,note.start_ns,tracking_end);
                } else if(note.kind==NoteKind::flick) {
                    if(note.start_ns<options.flick_lead_ns) throw Failure("negative_flick_lead", "Flick stroke would begin before song zero",note.start_ns);
                    const auto first=note.start_ns-options.flick_lead_ns;
                    if(options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_snap_v1)
                        build.diagonal_snap_flick(contact,note,li,first,add(first,options.flick_travel_ns),owned);
                    else if(options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_sampled_v2)
                        build.diagonal_sampled_flick(contact,note,li,first,add(first,options.flick_travel_ns),owned);
                    else if(options.flick_path_policy!=FlickPathPolicy::dynamic_line_v1)
                        build.straight_flick(contact,note,li,first,add(first,options.flick_travel_ns),projection,owned);
                    else if(dynamic_projection)build.projected_trajectory(contact,note,li,first,add(first,options.flick_travel_ns),true,owned);
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
                        owned.normal_projection_spans.push_back({note.start_ns,note.start_ns,projection,projection,projected.edge,0,projected.tangent_correction,projected.tangent_correction});
                    }
                    build.append(contact,note.start_ns,Phase::down,point,true);
                    build.append(contact,add(note.start_ns,note.kind==NoteKind::tap ? options.tap_duration_ns : options.drag_duration_ns),Phase::up,point,true);
                } else throw Failure("unsupported_note", "Unknown note kind",note.start_ns);
                return build.avoid_block_areas(contact,owned,note,li);
                };
                try {
                    if(regular_contact())++plan.metadata.block_area_detoured_contacts;
                } catch(const Failure& error) {
                    if(!options.allow_block_area_temporal_fallback||
                       (error.code!="block_area_no_safe_path"&&error.code!="block_area_detour_requires_judgment_rectangle"&&
                        error.code!="judgment_domain_unreachable"&&error.code!="offscreen_target"&&
                        error.code!="uncertified_normal_projection"&&error.code!="diagonal_sampled_unavailable"&&
                        error.code!="diagonal_snap_unavailable"))throw;
                    contact.steps.clear();build.projection_span_count=span_count;
                    owned={};owned.source_note_id=note.source.id;owned.source_json_pointer=note.source.json_pointer;
                    owned.line_index=li;owned.note_index=ni;owned.kind=note.kind;owned.side=note.side;
                    owned.contact_id=contact.id;owned.judgment_ns=note.start_ns;
                    if(!build.block_area_temporal_route(contact,owned,note,li))throw;
                    ++plan.metadata.block_area_detoured_contacts;++plan.metadata.block_area_temporal_contacts;
                    diagnostic({CompileSeverity::warning,"unverified_block_area_temporal_fallback",
                        "Active BlockArea contact uses a clear temporal alternative; actual judgment acceptance unverified",note.source.id,{},li,ni,note.start_ns,{}});
                }
                if(contact.steps.size()>options.max_steps_in_plan||plan.total_steps>options.max_steps_in_plan-contact.steps.size()) throw Failure("plan_step_budget", "Whole-song step budget exceeded",note.start_ns);
                plan.total_steps+=contact.steps.size();
                const bool projected_candidate=owned.projection_min_reference_pixels!=0||owned.projection_max_reference_pixels!=0;
                const bool gap_candidate=std::any_of(owned.normal_projection_spans.begin(),owned.normal_projection_spans.end(),[](const auto& span){return span.judgment_domain_gap;});
                plan.ownership.push_back(std::move(owned));
                plan.memory.contacts.push_back(std::move(contact));
                if(projected_candidate) diagnostic({CompileSeverity::warning,"unverified_normal_projection","Bounded normal projection is a calibration candidate; actual judgment acceptance unknown",note.source.id,{},li,ni,note.start_ns,{}});
                if(note.kind==NoteKind::hold&&options.hold_tail_policy==HoldTailPolicy::phira_tail_grace_candidate_v1)
                    diagnostic({CompileSeverity::warning,"unverified_hold_tail_grace","Community-simulator final 220ms tracking grace; original DOWN/UP and source note retained, actual game acceptance unverified",note.source.id,{},li,ni,note.end_ns,{}});
                if(note.kind==NoteKind::flick&&options.flick_path_policy==FlickPathPolicy::judgment_anchored_screen_fit_v1)
                    diagnostic({CompileSeverity::warning,"unverified_screen_fit_flick","Complete straight stroke with bounded native judgment-domain anchor; actual game acceptance unverified",note.source.id,{},li,ni,note.start_ns,{}});
                if(note.kind==NoteKind::flick&&options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_snap_v1)
                    diagnostic({CompileSeverity::warning,"unverified_discrete_diagonal_snap","Two-MOVE curved diagonal path is an explicit discrete input candidate; no continuous Flick target or actual game acceptance is claimed",note.source.id,{},li,ni,note.start_ns,{}});
                if(note.kind==NoteKind::flick&&options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_sampled_v2)
                    diagnostic({CompileSeverity::warning,"unverified_discrete_diagonal_sampled","Three-MOVE sampled diagonal is an explicit discrete input candidate; device sampling and actual game acceptance remain unverified",note.source.id,{},li,ni,note.start_ns,{}});
                if(rectangle_projection)
                    diagnostic({CompileSeverity::warning,"unverified_judgment_rectangle","Continuous normal/tangent correction bounds are certified against an explicit community simulator domain; actual Phigros acceptance unverified",note.source.id,{},li,ni,note.start_ns,{}});
                if(gap_candidate)
                    diagnostic({CompileSeverity::warning,"unverified_hold_reacquire_gap","Explicit <=20ms Hold tangent-domain gaps with positive valid recovery; continuous physical contact retained, actual game acceptance unverified",note.source.id,{},li,ni,note.start_ns,{}});
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
