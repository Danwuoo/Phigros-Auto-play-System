#pragma once
#include "first_note_trigger.hpp"
#include "../chart_calibrate/opening_detector.hpp"
#include "rhythm/phigros_pose.hpp"
#include <array>
#include <sstream>
#include <iomanip>
#include <numbers>

namespace rhythm::experimental {
inline constexpr Ns default_first_note_dispatch_lead_ns=200'000'000;
inline constexpr Ns desktop_first_note_dispatch_lead_ns=20'000'000;

// Native opening geometry is a conditional rendering hypothesis, never a
// capture-clock qualification or a calibrated game hit region. This separate
// path leaves the archived Dlyrotz first-note model unchanged.
struct AdaptiveOpeningDescription {
    Ns begin_ns=0,end_ns=0,note_time_ns=0,first_down_ns=0;
    std::size_t line_index=0,native_pieces=0,modeled_notes=0,earliest_note_count=0;
    NoteKind note_kind=NoteKind::tap;
    std::string note_id,note_json_pointer,model_id="native-piecewise-2d-opening-v3",model_sha256;
    std::string detector_policy_id="typed-rle-oriented-head-v1";
    bool includes_hold_heads=false;
    bool includes_classic_flick_heads=false;
    bool includes_extended_drag_heads=false;
    bool includes_mixed_earliest_heads=false;
    std::vector<int> required_center_families;
    std::vector<std::string> earliest_note_ids;
    std::size_t retained_equations=0,provably_invisible_equations=0;
    double approach_pixels_per_floor_unit=720/(2*.83175),pixel_error=3;
    bool includes_outlined_drag_heads=false;
    bool native_head_appearance_witness=false;
    bool certified_first_group_coverage=false;
};

// Same bounded connected-run algorithm as the archived cyan detector. Yellow
// Drag runs are selected explicitly from the immutable first-note semantics;
// this detector does not infer note identity, chart time or complete coverage.
inline calibration::OpeningDetection detect_adaptive_horizontal_opening_centers_roi(
        int width,int height,int stride,std::span<const std::uint8_t> rgb,NoteKind kind,int roi_bottom) {
    if(roi_bottom!=570&&(roi_bottom!=650||kind!=NoteKind::drag))return {};
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
    for(int y=30;y<roi_bottom;++y) {
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
            auto& group=groups[selected];if(group.widths.size()>=static_cast<std::size_t>(roi_bottom-30))return fail("yellow_group_row_budget_exhausted");
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
inline calibration::OpeningDetection detect_adaptive_opening_centers_roi(
        int width,int height,int stride,std::span<const std::uint8_t> rgb,NoteKind kind,int roi_bottom) {
    auto result=detect_adaptive_horizontal_opening_centers_roi(width,height,stride,rgb,kind,roi_bottom);
    if(roi_bottom!=570&&(roi_bottom!=650||kind!=NoteKind::drag))return result;
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
    for(int y=30;y<roi_bottom;++y){
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
           center.y-static_cast<double>(metric.rows)/2<=30||center.y+static_cast<double>(metric.rows)/2>=roi_bottom-1)return fail("clipped_horizontal_head");
        for(std::size_t i=0;i<runs.size();++i){
            const auto& r=runs[i];
            if(root(i)==*owners.begin()&&std::abs(r.y-center.y)<=static_cast<double>(metric.rows)/2+2&&
               r.b>=center.x-metric.width/2-2&&r.a<=center.x+metric.width/2+2&&
               (r.a==0||r.b==1279||r.y==30||r.y==roi_bottom-1))return fail("clipped_horizontal_head_band");
        }
    }
    const auto minimum=(kind==NoteKind::tap||kind==NoteKind::hold)?70.0:40.0;
    for(const auto& c:components)if(c.count>=80){
        if(c.horizontal_heads.size()>1)return fail("merged_horizontal_head_component");
        if(!c.horizontal_heads.empty())continue; // Preserve the explicit head, including connected long Hold bodies.
        const auto pixel_extent=std::abs(c.ux)+std::abs(c.uy);
        const auto major=c.umax-c.umin+pixel_extent,minor=c.vmax-c.vmin+pixel_extent;
        if(major<minimum||major>180||minor<2||minor>18||major<4*minor)continue;
        if(c.xmin==0||c.xmax==1279||c.ymin==30||c.ymax==roi_bottom-1)return fail("clipped_oriented_head");
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

// Explicit opt-in for closed horizontal Drag contours. Every accepted center
// belongs to one original connected component, never to merged RLE centers.
inline calibration::OpeningDetection detect_outlined_horizontal_drag_opening_centers(
        int width,int height,int stride,std::span<const std::uint8_t> rgb) {
    calibration::OpeningDetection result;
    if(width!=1280||height!=720||stride<3840||stride>16384||rgb.size()!=static_cast<std::size_t>(stride)*720)return result;
    // Failure metrics are bounded numeric diagnostics only; no center survives
    // and callers require valid=true before submitting any solver evidence.
    const auto fail=[&](const char* why){result.valid=false;result.reason=why;result.centers.clear();return result;};
    const auto colored=[&](int x,int y){const auto i=static_cast<std::size_t>(y)*static_cast<std::size_t>(stride)+static_cast<std::size_t>(x)*3;
        return rgb[i]>160&&rgb[i+1]>140&&rgb[i+2]<150&&static_cast<int>(rgb[i])-rgb[i+2]>60&&static_cast<int>(rgb[i+1])-rgb[i+2]>50;};
    struct Run{int y,a,b;std::size_t parent;};std::vector<Run> runs;runs.reserve(2048);
    const auto root=[&](std::size_t i){auto r=i;while(runs[r].parent!=r)r=runs[r].parent;
        while(runs[i].parent!=i){const auto next=runs[i].parent;runs[i].parent=r;i=next;}return r;};
    std::size_t previous_begin=0,previous_end=0,work=0;
    for(int y=30;y<650;++y){const auto current_begin=runs.size();std::size_t previous=previous_begin;int x=0;
        while(x<1280){while(x<1280&&!colored(x,y))++x;if(x==1280)break;const auto a=x;while(x<1280&&colored(x,y))++x;
            if(runs.size()>=16384)return fail("outlined_drag_run_budget");const auto index=runs.size();runs.push_back({y,a,x-1,index});
            if(x-a>=40&&x-a<=180)++result.qualifying_runs;
            while(previous<previous_end&&runs[previous].b+1<a)++previous;
            for(auto p=previous;p<previous_end&&runs[p].a<=x;++p){const auto first=root(index),second=root(p);
                if(first!=second)runs[std::max(first,second)].parent=std::min(first,second);}
        }previous_begin=current_begin;previous_end=runs.size();
    }
    struct Component{int xmin=1280,xmax=-1,ymin=720,ymax=-1,count=0;};std::vector<Component> components(runs.size());
    for(std::size_t i=0;i<runs.size();++i){const auto& r=runs[i];auto& c=components[root(i)];if(c.count==0&&++result.groups>4096)return fail("outlined_drag_component_budget");
        c.count+=r.b-r.a+1;c.xmin=std::min(c.xmin,r.a);c.xmax=std::max(c.xmax,r.b);c.ymin=std::min(c.ymin,r.y);c.ymax=std::max(c.ymax,r.y);}
    const auto reject_component=[&](std::size_t ci,const char* why){const auto& c=components[ci];if(result.metrics.size()<8)result.metrics.push_back({
        (c.xmin+c.xmax)/2.,(c.ymin+c.ymax)/2.,static_cast<double>(c.xmax-c.xmin+1),static_cast<std::size_t>(c.ymax-c.ymin+1)});return fail(why);};
    std::vector<std::size_t> qualified;qualified.reserve(16);
    for(std::size_t ci=0;ci<components.size();++ci){const auto& c=components[ci];if(c.count<80)continue;
        const int w=c.xmax-c.xmin+1,h=c.ymax-c.ymin+1;
        if(c.xmin==0||c.xmax==1279||c.ymin==30||c.ymax==649)return reject_component(ci,"outlined_drag_clipped_component");
        if(w<40||w>180||h<2||h>18||w<4*h)return reject_component(ci,"outlined_drag_unsupported_component");
        if(qualified.size()>=16)return reject_component(ci,"outlined_drag_qualified_component_budget");qualified.push_back(ci);
    }
    const auto solid=[&](std::size_t ci){const auto& c=components[ci];return static_cast<double>(c.count)/((c.xmax-c.xmin+1)*(c.ymax-c.ymin+1))>=.72;};
    std::array<int,16> core_for{},halo_for{};core_for.fill(-1);halo_for.fill(-1);std::optional<std::size_t> invalid_nested;
    for(std::size_t a=0;a<qualified.size();++a)for(std::size_t b=0;b<qualified.size();++b){if(a==b)continue;
        const auto& outer=components[qualified[a]];const auto& inner=components[qualified[b]];
        if(!(outer.xmin<inner.xmin&&inner.xmax<outer.xmax&&outer.ymin<inner.ymin&&inner.ymax<outer.ymax))continue;
        const int left=inner.xmin-outer.xmin,right=outer.xmax-inner.xmax,top=inner.ymin-outer.ymin,bottom=outer.ymax-inner.ymax;
        const double dx=(inner.xmin+inner.xmax-outer.xmin-outer.xmax)/2.,dy=(inner.ymin+inner.ymax-outer.ymin-outer.ymax)/2.;
        if(!solid(qualified[b])||left<4||left>24||right<4||right>24||top<2||top>6||bottom<2||bottom>6||
            left+right<8||left+right>48||top+bottom<4||top+bottom>12||std::abs(left-right)>2||std::abs(top-bottom)>2||std::hypot(dx,dy)>1){invalid_nested=a;continue;}
        if(core_for[a]>=0||halo_for[b]>=0)return reject_component(qualified[a],"outlined_drag_ambiguous_nested_ownership");
        core_for[a]=static_cast<int>(b);halo_for[b]=static_cast<int>(a);
    }
    if(invalid_nested)return reject_component(qualified[*invalid_nested],"outlined_drag_unqualified_nested_component");
    for(std::size_t a=0;a<qualified.size();++a)if(core_for[a]>=0&&halo_for[a]>=0)
        return reject_component(qualified[a],"outlined_drag_nested_role_conflict");
    const auto prove_outline=[&](std::size_t ci,int cap)->std::string{const auto& c=components[ci];const int w=c.xmax-c.xmin+1,h=c.ymax-c.ymin+1;
            if(h<4)return std::string("outlined_drag_unclosed_contour");
            std::array<std::uint8_t,180*18> mask{};
            for(std::size_t i=0;i<runs.size();++i)if(root(i)==ci){const auto& r=runs[i];for(int x=r.a;x<=r.b;++x){
                if(++work>4000000)return std::string("outlined_drag_pixel_work_budget");
                mask[static_cast<std::size_t>(r.y-c.ymin)*180+static_cast<std::size_t>(x-c.xmin)]=1;}}
            int upper=-1,lower=-1;
            std::array<std::pair<int,int>,9> bands{};std::size_t band_count=0;bool in_band=false;
            // Infer exactly two original central row bands. A third band or
            // a bridge filling their gap is not repaired as a thicker outline.
            for(int y=0;y<h;++y){bool ink=false;for(int x=cap+1;x<w-1-cap;++x)
                    ink=ink||mask[static_cast<std::size_t>(y)*180+static_cast<std::size_t>(x)];
                if(ink){if(!in_band){if(band_count>=bands.size())return std::string("outlined_drag_central_band_budget");
                        bands[band_count++]={y,y};in_band=true;}else bands[band_count-1].second=y;}
                else in_band=false;
            }
            const auto band_reason=[&](const char* prefix){std::string why=prefix;why+='=';why+=std::to_string(band_count);why+=";bands=";
                for(std::size_t i=0;i<band_count;++i){if(i)why+=',';why+=std::to_string(bands[i].first);why+="..";why+=std::to_string(bands[i].second);}return why;};
            if(band_count!=2){const auto why=band_reason("outlined_drag_central_band_count");return std::string(why.c_str());}
            if(bands[1].first-bands[0].second<2||bands[0].second-bands[0].first+1>(h-1)/2||
                bands[1].second-bands[1].first+1>(h-1)/2){const auto why=band_reason("outlined_drag_unbounded_rail_bands");return std::string(why.c_str());}
            for(std::size_t band=0;band<band_count;++band)for(int y=bands[band].first;y<=bands[band].second;++y){
                bool rail=true;for(int x=cap;x<w-cap;++x)rail=rail&&mask[static_cast<std::size_t>(y)*180+static_cast<std::size_t>(x)];
                if(rail){if(band==0&&upper<0)upper=y;if(band==1)lower=y;}
            }
            if(upper<0||lower<0)return std::string("outlined_drag_missing_long_rails");
            const auto cap_connected=[&](bool right){
                std::array<std::uint8_t,180*18> seen{};std::array<std::size_t,180*18> queue{};std::size_t begin=0,end=0;
                const int lo=right?w-1-cap:0,hi=right?w-1:cap;
                for(int x=lo;x<=hi;++x){const auto at=static_cast<std::size_t>(upper)*180+static_cast<std::size_t>(x);
                    if(mask[at]){seen[at]=1;queue[end++]=at;}}
                while(begin<end){const auto at=queue[begin++];const int x=static_cast<int>(at%180),y=static_cast<int>(at/180);
                    if(y==lower)return true;
                    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){if(++work>4000000)return false;const int nx=x+dx,ny=y+dy;
                        if(nx<lo||nx>hi||ny<upper||ny>lower)continue;const auto next=static_cast<std::size_t>(ny)*180+static_cast<std::size_t>(nx);
                        if(mask[next]&&!seen[next]){seen[next]=1;queue[end++]=next;}}
                }return false;
            };
            if(!cap_connected(false)||!cap_connected(true))return std::string(work>4000000?"outlined_drag_pixel_work_budget":"outlined_drag_missing_closed_cap");
        return {};
    };
    for(std::size_t a=0;a<qualified.size();++a){if(halo_for[a]>=0)continue;
        const auto ci=qualified[a];const auto& outer=components[ci];std::size_t observed=ci;
        if(core_for[a]>=0){observed=qualified[static_cast<std::size_t>(core_for[a])];const auto& inner=components[observed];
            const int cap=std::max(inner.xmin-outer.xmin,outer.xmax-inner.xmax)+2;
            if(cap>26)return reject_component(ci,"outlined_drag_nested_cap_budget");
            const auto why=prove_outline(ci,cap);if(!why.empty())return reject_component(ci,why.c_str());
        }else if(!solid(ci)){const auto why=prove_outline(ci,(outer.ymax-outer.ymin+1)/2+2);
            if(!why.empty())return reject_component(ci,why.c_str());}
        if(result.centers.size()>=8)return reject_component(ci,"outlined_drag_center_budget");
        const auto& c=components[observed];const double x=(c.xmin+c.xmax)/2.,y=(c.ymin+c.ymax)/2.;
        result.centers.push_back({"yellow-outline-"+std::to_string(result.centers.size()),x,y});
        result.metrics.push_back({x,y,static_cast<double>(c.xmax-c.xmin+1),static_cast<std::size_t>(c.ymax-c.ymin+1)});
    }
    if(result.centers.empty())return fail("outlined_drag_no_supported_heads");
    result.valid=true;result.reason="original_connected_closed_horizontal_drag_heads;completeness_unverified";return result;
}

inline calibration::OpeningDetection detect_adaptive_horizontal_opening_centers(
        int width,int height,int stride,std::span<const std::uint8_t> rgb,NoteKind kind) {
    return detect_adaptive_horizontal_opening_centers_roi(width,height,stride,rgb,kind,570);
}
inline calibration::OpeningDetection detect_adaptive_opening_centers(
        int width,int height,int stride,std::span<const std::uint8_t> rgb,NoteKind kind) {
    return detect_adaptive_opening_centers_roi(width,height,stride,rgb,kind,570);
}
// First-Drag-only versioned adapter. The remaining 70 rows stay outside this
// ROI, including bottom song-title UI. All old typed-head shape rules remain.
inline calibration::OpeningDetection detect_extended_drag_opening_centers(
        int width,int height,int stride,std::span<const std::uint8_t> rgb) {
    return detect_adaptive_opening_centers_roi(width,height,stride,rgb,NoteKind::drag,650);
}

// All time branches and one-to-one assignments inside the native pre-judgment
// domain are retained. Rotating native pieces use an affine chord with an
// explicit second-derivative enclosure, not a time-grid/nearest-time choice.
// A broad enclosure can keep ambiguity and prevent arming; it never authorizes
// silently dropping a visible alias. Rendering is still an unqualified model.
struct AdaptiveFirstDispatchGate {
    Ns origin_host_ns, target_host_ns, received_host_ns, evaluated_host_ns, minimum_lead_ns;
    VisualTimeInterval song_interval;
    const char* reason;
    bool allowed;
};

class AdaptiveFirstNoteTrigger {
    static constexpr Ns ms=1'000'000,second=1'000'000'000,safe=(Ns{1}<<53)-1;
    // A closed contact needs at least two of the runtime's 20000 steps.
    // Keep independent equation/work budgets; never discard aliases to fit.
    static constexpr std::size_t max_notes=20000/2,max_pieces=2048,max_equations=300000,max_candidates=1024,max_work=2'000'000;
    struct Range {long double lo=0,hi=0;};
    struct Axis {Range base,slope;};
    struct Equation {Axis x,y;bool positive=false;std::string speed_pointer;double line_speed=0;};
    struct NativeNote {SemanticNote note;std::size_t line=0;};
    struct RelativePose {
        std::array<Range,2> origin_begin,origin_end;
        long double theta_begin=0,theta_end=0,along=0,numeric_padding=0;
        Range z_begin,z_end;
    };
    struct Piece {VisualTimeInterval time;std::vector<Equation> equations;std::vector<std::size_t> note_indices;
        std::vector<RelativePose> relative_poses;
        std::vector<std::pair<std::size_t,Equation>> appearance_equations,first_group_equations;};
    struct AppearanceJump {std::size_t note=0;VisualTimeInterval time;std::string pointer;};
    struct Support {
        std::vector<VisualTimeCandidate> candidates;
        VisualCandidateFrame frame;
        std::vector<VisualTimeInterval> time,without_complete_group,without_first;
    };
    AdaptiveOpeningDescription description_;
    Ns phase_=0,minimum_dispatch_lead_ns_=default_first_note_dispatch_lead_ns,last_now_=-1,last_received_=-1;
    std::uint64_t last_sequence_=0;
    std::size_t first_note_=0,work_=0;
    Ns native_zero_=0;
    bool source_v1_=false;
    std::string context_;
    std::vector<NativeNote> notes_;
    std::vector<Piece> pieces_;
    std::vector<AppearanceJump> appearance_jumps_;
    std::set<std::string> evidence_ids_,frame_ids_;
    // Keep actual supports for at most200ms, with the existing pair spacing. A noisy
    // intermediate receipt must not erase an older, still eligible support.
    std::vector<Support> support_history_;
    FirstNoteTriggerStatus value_;
    std::optional<AdaptiveFirstDispatchGate> dispatch_gate_;
    bool consumed_=false;
    bool allows_first_dispatch(const FirstNoteTriggerAnchor& anchor,Ns now) {
        const char* reason=anchor.origin_host_ns<0?"negative_origin":
            anchor.target_host_ns>safe?"target_out_of_range":
            anchor.target_host_ns-now<minimum_dispatch_lead_ns_?"insufficient_lead":nullptr;
        dispatch_gate_=AdaptiveFirstDispatchGate{anchor.origin_host_ns,anchor.target_host_ns,anchor.received_host_ns,
            now,minimum_dispatch_lead_ns_,anchor.chosen_song_interval,reason?reason:"accepted",reason==nullptr};
        if(reason)cancel(minimum_dispatch_lead_ns_==default_first_note_dispatch_lead_ns?
            "first_note_dispatch_lead_below_200ms":"first_note_dispatch_lead_below_minimum");
        return reason==nullptr;
    }
    static long double down(long double v){return std::nextafter(v,-std::numeric_limits<long double>::infinity());}
    static long double up(long double v){return std::nextafter(v,std::numeric_limits<long double>::infinity());}
    static Range add(Range a,Range b){return {down(a.lo+b.lo),up(a.hi+b.hi)};}
    static Range sub(Range a,Range b){return add(a,{-b.hi,-b.lo});}
    static Range mul(Range a,Range b) {
        const long double v[]{a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};const auto [lo,hi]=std::minmax_element(std::begin(v),std::end(v));
        return {down(*lo),up(*hi)};
    }
    static Range point(long double v){return {v,v};}
    static bool outside_viewport(Range x,Range y) {
        return x.hi< -95||x.lo>1375||y.hi< -95||y.lo>815;
    }
    static bool outside_viewport(const Equation& equation,const Piece& piece,VisualTimeInterval time) {
        const Range dt{static_cast<long double>(time.lower-piece.time.lower),static_cast<long double>(time.upper-piece.time.lower)};
        return outside_viewport(add(equation.x.base,mul(equation.x.slope,dt)),
            add(equation.y.base,mul(equation.y.slope,dt)));
    }
    static bool fully_visible(Range x,Range y) {
        return x.lo>=95&&x.hi<=1184&&y.lo>=125&&y.hi<=554;
    }
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
    static std::vector<VisualTimeInterval> intersect_receipts(
            const std::vector<VisualTimeInterval>& current,const std::vector<VisualTimeInterval>& previous,Ns dt) {
        std::vector<VisualTimeInterval> out;
        std::size_t a=0,b=0;
        while(a<current.size()&&b<previous.size()){
            const VisualTimeInterval shifted{previous[b].lower+dt,previous[b].upper+dt};
            const auto overlap=intersect(current[a],shifted);
            if(valid(overlap))out.push_back(overlap);
            if(current[a].upper<shifted.upper)++a;else ++b;
        }
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
    static bool coincident_for_whole_piece(const Equation& a,const Equation& b,VisualTimeInterval piece) {
        if(!valid(piece))return false;
        const auto duration=point(static_cast<long double>(piece.upper-piece.lower));
        const auto axis_distance=[&](const Axis& x,const Axis& y){
            const auto base=sub(x.base,y.base),slope=sub(x.slope,y.slope);
            const auto finish=add(base,mul(slope,duration));
            // The difference of the affine enclosures is affine for every
            // enclosed coefficient choice. Its maximum absolute value over
            // the entire closed piece is bounded by these interval endpoints.
            // Each base already includes the complete curvature/error padding.
            return std::max({std::abs(base.lo),std::abs(base.hi),std::abs(finish.lo),std::abs(finish.hi)});
        };
        const auto l1=up(axis_distance(a.x,b.x)+axis_distance(a.y,b.y));
        return std::isfinite(l1)&&l1<=1.0L/1024;
    }
    static bool relatively_coincident_for_whole_piece(const RelativePose& a,const RelativePose& b) {
        const auto finite_range=[](Range r){return std::isfinite(r.lo)&&std::isfinite(r.hi)&&r.lo<=r.hi;};
        for(const auto* p:{&a,&b}){
            if(!std::isfinite(p->theta_begin)||!std::isfinite(p->theta_end)||!std::isfinite(p->along)||
               !std::isfinite(p->numeric_padding)||p->numeric_padding<0||!finite_range(p->z_begin)||!finite_range(p->z_end))return false;
            for(const auto& origin:{p->origin_begin,p->origin_end})for(const auto axis:origin)if(!finite_range(axis))return false;
        }
        const auto absolute=[](Range r){return std::max(std::abs(r.lo),std::abs(r.hi));};
        // Choose one exactly representable whole-turn offset at the beginning.
        // Never wrap the end independently: a whole extra turn is not equal.
        const auto turns=std::round((a.theta_begin-b.theta_begin)/360.0L);
        if(!std::isfinite(turns)||std::abs(turns)>1'000'000'000.0L)return false;
        const auto offset=point(turns*360.0L);
        const auto angle=std::max(absolute(sub(sub(point(a.theta_begin),point(b.theta_begin)),offset)),
                                  absolute(sub(sub(point(a.theta_end),point(b.theta_end)),offset)));
        const auto origin_distance=[&](const auto& x,const auto& y){return up(absolute(sub(x[0],y[0]))+absolute(sub(x[1],y[1])));};
        const auto origin=std::max(origin_distance(a.origin_begin,b.origin_begin),origin_distance(a.origin_end,b.origin_end));
        const auto local=up(absolute(sub(point(a.along),point(b.along)))+
            std::max(absolute(sub(a.z_begin,b.z_begin)),absolute(sub(a.z_end,b.z_end))));
        const auto radius=up(std::abs(a.along)+std::max(absolute(a.z_begin),absolute(a.z_end)));
        const auto radians=mul(point(angle),{down(std::numbers::pi_v<long double>/180),up(std::numbers::pi_v<long double>/180)});
        // Origin, local vector and unwrapped theta are affine on this complete
        // native piece. Convex endpoint bounds plus rotation's Lipschitz bound
        // cancel common curvature analytically, without sampling trajectories.
        auto bound=add(point(origin),mul(add(point(local),mul(point(radius),radians)),
            {down(std::numbers::sqrt2_v<long double>),up(std::numbers::sqrt2_v<long double>)}));
        // Keep both notes' independent per-axis roundoff/floor enclosures;
        // only the common geometric curvature is removed by the proof above.
        bound=add(bound,point(up(2*up(a.numeric_padding+b.numeric_padding))));
        return finite_range(bound)&&bound.hi<=1.0L/1024;
    }
    static Range increment(double speed,Ns dt){return mul(point(speed),{down(static_cast<long double>(dt)/second),up(static_cast<long double>(dt)/second)});}
    Range floor_at(const ChartLine& line,Ns at) const {
        Range floor=point(0);Ns cursor=native_zero_;
        // Native v1 may declare only speeds, with a negative-time sentinel.
        // Its missing floors are an opening-model integral anchored at tick 0,
        // not an integral from the sentinel and not invented source fields.
        const bool zero_integral=source_v1_&&std::none_of(line.speed_events.begin(),line.speed_events.end(),
            [](const auto& e){return e.floor_position.has_value();});
        std::optional<Ns> previous_end;
        for(const auto& e:line.speed_events) {
            require(std::isfinite(e.value)&&std::abs(e.value)<=1e6&&e.time.end_ns>=e.time.start_ns&&
                    (zero_integral?(previous_end?e.time.start_ns==*previous_end:e.time.start_ns<=native_zero_):e.time.start_ns==cursor),
                    "adaptive opening speed coverage/values unsupported");
            previous_end=e.time.end_ns;
            if(zero_integral&&e.time.end_ns<=native_zero_)continue;
            require((zero_integral?std::max(e.time.start_ns,native_zero_):e.time.start_ns)==cursor&&e.time.end_ns>=cursor,
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
    void reset(const char* why){support_history_.clear();value_.anchor.reset();value_.support_count=0;value_.state=FirstNoteTriggerState::acquiring;value_.reason=why;}
    bool health(Ns now,bool healthy) {
        if(value_.state==FirstNoteTriggerState::cancelled||value_.state==FirstNoteTriggerState::fired)return false;
        if(now<0||now>safe||(last_now_>=0&&now<last_now_)){cancel("host_clock_invalid_or_rollback");return false;}last_now_=now;
        if(!healthy){reset("waiting_for_healthy_actual_UI");return false;}
        if(value_.state==FirstNoteTriggerState::ready&&value_.anchor&&
           (now-value_.anchor->received_host_ns>=100*ms||
            value_.anchor->target_host_ns-now<minimum_dispatch_lead_ns_))reset("first_note_readiness_expired");
        return true;
    }
    std::vector<VisualTimeCandidate> solve(const VisualCandidateFrame& f,const std::vector<int>& families,
                                          const std::vector<NoteKind>& unavailable_kinds) {
        std::vector<VisualTimeCandidate> out;
        for(const auto& piece:pieces_) {
            struct Choice {std::size_t note,equation;VisualTimeInterval time;};
            std::vector<std::vector<Choice>> choices(f.centers.size());
            std::vector<VisualTimeInterval> exclusions;
            for(const auto& e:piece.equations) {
                // 180x18px head half diagonal <91px, plus 3px detector/model
                // error and one strict pixel at the detector's ROI boundary.
                auto inside=axis_match(e.x,piece.time,piece.time.lower,95,1184,true);
                inside=axis_match(e.y,inside,piece.time.lower,125,description_.native_head_appearance_witness?554:474,true);exclusions.push_back(inside);
            }
            bool missing=false;
            for(std::size_t i=0;i<f.centers.size();++i) {
                const auto& center=f.centers[i];
                for(std::size_t n=0;n<piece.equations.size();++n) {
                    bump();const auto& e=piece.equations[n];
                    const auto kind=notes_[piece.note_indices[n]].note.kind;
                    const int family=kind==NoteKind::drag?2:kind==NoteKind::flick?4:1;
                    if(description_.includes_mixed_earliest_heads&&families[i]!=family)continue;
                    auto domain=axis_match(e.x,piece.time,piece.time.lower,center.x-3,center.x+3);
                    domain=axis_match(e.y,domain,piece.time.lower,center.y-3,center.y+3);
                    if(valid(domain))choices[i].push_back({piece.note_indices[n],n,domain});
                }
                if(choices[i].empty())missing=true;
            }
            if(missing)continue;
            std::vector<bool> used(notes_.size(),false);std::vector<VisualNoteMatch> matches;
            std::vector<std::size_t> observed_equations;
            const auto visit=[&](auto&& self,std::size_t index,VisualTimeInterval domain)->void {
                bump();
                if(index==choices.size()) {
                    std::vector<VisualTimeInterval> parts{domain};
                    for(std::size_t n=0;n<piece.equations.size()&&!parts.empty();++n)if(!used[piece.note_indices[n]]&&valid(exclusions[n])) {
                        // A genuinely observed head can hide a same-kind/side
                        // source whose complete projected piece is certified
                        // subpixel-coincident. This exempts absence only: never
                        // add a match, earliest-group support or motion for it.
                        // Compare directly to observed sources, never through a
                        // transitive chain of merely covered source aliases.
                        bool occluded=false;
                        const auto& absent=notes_[piece.note_indices[n]].note;
                        // A rejected shape detector provides no evidence of absence.
                        if(std::find(unavailable_kinds.begin(),unavailable_kinds.end(),absent.kind)!=unavailable_kinds.end())continue;
                        if(description_.includes_hold_heads||description_.includes_classic_flick_heads||
                           (description_.certified_first_group_coverage&&absent.start_ns==description_.note_time_ns))for(const auto observed:observed_equations){
                            bump();
                            const auto& present=notes_[piece.note_indices[observed]].note;
                            if(absent.kind==present.kind&&absent.side==present.side&&
                               (coincident_for_whole_piece(piece.equations[n],piece.equations[observed],piece.time)||
                                (description_.includes_classic_flick_heads&&absent.kind==NoteKind::flick&&
                                 piece.relative_poses.size()==piece.equations.size()&&
                                 relatively_coincident_for_whole_piece(piece.relative_poses[n],piece.relative_poses[observed])))){
                                occluded=true;break;
                            }
                        }
                        if(occluded)continue;
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
                    observed_equations.push_back(choice.equation);
                    self(self,index+1,next);matches.pop_back();used[choice.note]=false;
                    observed_equations.pop_back();
                }
            };
            visit(visit,0,piece.time);
        }
        return out;
    }
    bool appearance_geometry(std::size_t note,VisualTimeInterval time,bool visible) {
        if(!valid(time)||time.lower<0||time.upper>=description_.end_ns)return false;
        auto cursor=time.lower;
        for(const auto& piece:pieces_) {
            const auto part=intersect(time,piece.time);if(!valid(part))continue;bump();
            if(part.lower!=cursor)return false;
            const auto found=std::find_if(piece.appearance_equations.begin(),piece.appearance_equations.end(),
                [&](const auto& e){return e.first==note;});
            if(found==piece.appearance_equations.end())return false;
            const Range dt{static_cast<long double>(part.lower-piece.time.lower),static_cast<long double>(part.upper-piece.time.lower)};
            const auto x=add(found->second.x.base,mul(found->second.x.slope,dt));
            const auto y=add(found->second.y.base,mul(found->second.y.slope,dt));
            if(!(visible?fully_visible(x,y):outside_viewport(x,y)))return false;
            cursor=part.upper+1;
        }
        return cursor==time.upper+1;
    }
    bool appearance_group_present(const VisualTimeCandidate& candidate) const {
        return std::all_of(description_.earliest_note_ids.begin(),description_.earliest_note_ids.end(),[&](const auto& id){
            return std::any_of(candidate.matches.begin(),candidate.matches.end(),[&](const auto& match){return match.note_id==id;});});
    }
    bool certified_group_present(const VisualTimeCandidate& candidate,VisualTimeInterval time) {
        const auto piece=std::lower_bound(pieces_.begin(),pieces_.end(),time.lower,
            [](const Piece& value,Ns at){return value.time.upper<at;});
        if(piece==pieces_.end()||time.lower<piece->time.lower||time.upper>piece->time.upper)return false;
        for(const auto& [index,equation]:piece->first_group_equations){
            const auto& note=notes_[index].note;
            const auto observed=std::any_of(candidate.matches.begin(),candidate.matches.end(),
                [&](const auto& match){return match.note_id==note.source.id;});
            if(observed||outside_viewport(equation,*piece,time))continue;
            bool covered=false;
            for(std::size_t n=0;n<piece->note_indices.size();++n){
                bump();const auto& other=notes_[piece->note_indices[n]].note;
                if(note.kind!=other.kind||note.side!=other.side)continue;
                if(std::none_of(candidate.matches.begin(),candidate.matches.end(),
                    [&](const auto& match){return match.note_id==other.source.id;}))continue;
                if(coincident_for_whole_piece(equation,piece->equations[n],piece->time)){covered=true;break;}
            }
            if(!covered)return false;
        }
        return piece->first_group_equations.size()==description_.earliest_note_count;
    }
    FirstNoteTriggerStatus observe_appearance(Ns now) {
        const auto& current=support_history_.back();const auto& f=current.frame;
        std::size_t pairs=0;std::vector<std::size_t> eligible;
        for(std::size_t i=0;i+1<support_history_.size();++i) {
            const auto& frame=support_history_[i].frame;const auto dt=f.received_host_ns-frame.received_host_ns;
            if(dt>=20*ms&&dt<=200*ms&&f.frame_sha256!=frame.frame_sha256)eligible.push_back(i);
        }
        if(eligible.empty()){value_.reason="native_group_frames_not_distinct_or_too_far_apart";return value_;}
        const auto compatible=[&](const VisualTimeCandidate& c,const VisualTimeCandidate& p,Ns dt) {
            if(++pairs>65536)throw std::runtime_error("adaptive_opening_pair_budget");
            if(p.song_time.lower>safe-dt||p.song_time.upper>safe-dt)return VisualTimeInterval{1,0};
            return intersect(c.song_time,{p.song_time.lower+dt,p.song_time.upper+dt});
        };
        // Every actual support contributes its union of ALL native aliases.
        // Intersect constraints, never choose a favorable frame or source alias.
        std::vector<VisualTimeInterval> retained;
        std::vector<std::vector<VisualTimeInterval>> pair_domains(support_history_.size()-1);
        for(const auto& candidate:current.candidates) {
            std::vector<VisualTimeInterval> domains{candidate.song_time};
            for(const auto index:eligible) {
                const auto& previous=support_history_[index];const auto dt=f.received_host_ns-previous.frame.received_host_ns;
                std::vector<VisualTimeInterval> allowed,next;
                for(const auto& p:previous.candidates){auto v=compatible(candidate,p,dt);if(valid(v))allowed.push_back(v);}
                allowed=merge(std::move(allowed));
                pair_domains[index].insert(pair_domains[index].end(),allowed.begin(),allowed.end());
                for(const auto a:domains)for(const auto b:allowed){bump();const auto v=intersect(a,b);if(valid(v))next.push_back(v);}
                domains=merge(std::move(next));
            }
            retained.insert(retained.end(),domains.begin(),domains.end());
        }
        value_.selected_intervals=merge(std::move(retained));
        if(value_.selected_intervals.size()!=1||value_.selected_intervals[0].upper-value_.selected_intervals[0].lower>40*ms) {
            value_.reason="native_appearance_all_support_temporal_ambiguity_retained";return value_;
        }
        for(auto& domains:pair_domains)domains=merge(std::move(domains));
        const auto chosen=value_.selected_intervals[0];std::optional<std::size_t> supporting,constraining;
        const auto exact_chosen=[&](const auto& domains) {
            return domains.size()==1&&domains[0].lower==chosen.lower&&domains[0].upper==chosen.upper;
        };
        const auto center=[](const VisualTimeCandidate& c,const VisualCandidateFrame& frame,const std::string& id)->const VisualCenter* {
            const auto match=std::find_if(c.matches.begin(),c.matches.end(),[&](const auto& m){return m.note_id==id;});
            if(match==c.matches.end())return nullptr;
            const auto point=std::find_if(frame.centers.begin(),frame.centers.end(),[&](const auto& p){return p.id==match->center_id;});
            return point==frame.centers.end()?nullptr:&*point;
        };
        for(const auto index:eligible) {
            const auto& previous=support_history_[index];const auto dt=f.received_host_ns-previous.frame.received_host_ns;
            bool certified=true;std::optional<std::string> unique_jump;std::vector<VisualTimeInterval> coverage;
            for(const auto& c:current.candidates)for(const auto& p:previous.candidates) {
                const auto part=intersect(compatible(c,p,dt),chosen);if(!valid(part))continue;
                if(!appearance_group_present(p)||!appearance_group_present(c)) {
                    value_.reason="native_appearance_nonfirst_source_alias_retained";return value_;
                }
                bool motion=false;
                for(const auto& id:description_.earliest_note_ids) {
                    const auto* a=center(p,previous.frame,id);const auto* b=center(c,f,id);
                    if(a&&b&&std::hypot(b->x-a->x,b->y-a->y)>6)motion=true;
                }
                std::vector<std::string> witnesses;
                if(!motion)for(const auto& jump:appearance_jumps_) {
                    bump();const auto& id=notes_[jump.note].note.source.id;
                    if(!center(c,f,id)||center(p,previous.frame,id))continue;
                    const VisualTimeInterval prior{part.lower-dt,part.upper-dt};
                    if(prior.upper>jump.time.lower||part.lower<jump.time.lower)continue;
                    if(!appearance_geometry(jump.note,prior,false)||!appearance_geometry(jump.note,part,true)||
                       !appearance_geometry(jump.note,{jump.time.lower,jump.time.lower},false)||
                       !appearance_geometry(jump.note,{jump.time.upper,jump.time.upper},true))continue;
                    witnesses.push_back(id+"|"+jump.pointer);
                }
                if(!motion) {
                    if(witnesses.size()!=1||(unique_jump&&*unique_jump!=witnesses[0]))certified=false;
                    else unique_jump=witnesses[0];
                }
                coverage.push_back(part);
            }
            coverage=merge(std::move(coverage));
            if(certified&&std::any_of(coverage.begin(),coverage.end(),[&](auto v){return v.lower<=chosen.lower&&chosen.upper<=v.upper;})) {
                if(exact_chosen(pair_domains[index])){supporting=index;constraining.reset();}
                else for(const auto other:eligible)if(other!=index&&
                    support_history_[other].frame.frame_sha256!=previous.frame.frame_sha256) {
                    std::vector<VisualTimeInterval> combined;
                    for(const auto a:pair_domains[index])for(const auto b:pair_domains[other]) {
                        bump();const auto part=intersect(a,b);if(valid(part))combined.push_back(part);
                    }
                    combined=merge(std::move(combined));
                    if(exact_chosen(combined)){supporting=index;constraining=other;break;}
                }
            }
        }
        if(!supporting){value_.reason="native_appearance_no_one_actual_pair_certifies_all_retained_aliases";return value_;}
        value_.support_count=constraining?3:2;FirstNoteTriggerAnchor anchor;anchor.received_host_ns=f.received_host_ns;
        anchor.chosen_song_interval=chosen;anchor.origin_host_ns=f.received_host_ns-midpoint(chosen);
        anchor.target_host_ns=anchor.origin_host_ns+description_.first_down_ns+phase_;anchor.evidence_id=f.evidence_id;
        anchor.supporting_evidence_ids={support_history_[*supporting].frame.evidence_id};
        if(constraining)anchor.supporting_evidence_ids.push_back(support_history_[*constraining].frame.evidence_id);
        anchor.supporting_evidence_ids.push_back(f.evidence_id);
        if(!allows_first_dispatch(anchor,now))return value_;
        value_.anchor=std::move(anchor);value_.state=FirstNoteTriggerState::ready;
        value_.reason="two_actual_native_group_frames_motion_or_certified_pop_in_on_all_support_origin";return value_;
    }
public:
    AdaptiveFirstNoteTrigger(const SemanticChart& chart,Ns phase_ns,Ns first_down_ns=-1,bool outlined_drag_heads=false,
                             bool native_head_appearance_witness=false,bool certified_first_group_coverage=false,
                             Ns minimum_dispatch_lead_ns=default_first_note_dispatch_lead_ns)
      :phase_(phase_ns),minimum_dispatch_lead_ns_(minimum_dispatch_lead_ns) {
        require(phase_ns>=-100*ms&&phase_ns<=100*ms,"first-note phase must be +/-100ms");
        require(minimum_dispatch_lead_ns_>0&&minimum_dispatch_lead_ns_<=default_first_note_dispatch_lead_ns,
                "adaptive first-note dispatch lead must be positive and at most 200ms");
        require(chart.format_version==3&&std::isfinite(chart.offset_seconds)&&sha256_text(chart.source.chart_sha256)&&
                !chart.lines.empty()&&chart.lines.size()<=128&&chart.note_count>0&&chart.note_count<=max_notes,"adaptive opening chart scope unsupported");
        const SemanticNote* first=nullptr;std::size_t first_line=0,actual_notes=0;
        for(std::size_t l=0;l<chart.lines.size();++l)for(const auto& n:chart.lines[l].notes) {
            require(++actual_notes<=max_notes,"adaptive opening actual note budget derived from closed-contact steps");
            if(!first||n.start_ns<first->start_ns){first=&n;first_line=l;}
        }
        require(actual_notes==chart.note_count,"adaptive opening native note count mismatch");
        require(first&&first->start_ns>0&&first->start_ns<=60*second,"adaptive opening needs a future native earliest judgment");
        description_.end_ns=description_.note_time_ns=first->start_ns;description_.line_index=first_line;
        description_.note_kind=first->kind;description_.note_id=first->source.id;description_.note_json_pointer=first->source.json_pointer;
        const auto color=[](NoteKind k){return k==NoteKind::drag?2:k==NoteKind::flick?4:1;};
        std::set<int> required_families;
        bool first_flick=false;
        for(const auto& line:chart.lines)for(const auto& n:line.notes)if(n.start_ns==first->start_ns) {
            description_.earliest_note_ids.push_back(n.source.id);first_flick|=n.kind==NoteKind::flick;required_families.insert(color(n.kind));
        }
        description_.earliest_note_count=description_.earliest_note_ids.size();
        description_.certified_first_group_coverage=certified_first_group_coverage&&!native_head_appearance_witness&&
            description_.earliest_note_count<=64;
        description_.required_center_families.assign(required_families.begin(),required_families.end());
        description_.includes_mixed_earliest_heads=required_families.size()>1;
        description_.includes_classic_flick_heads=required_families.contains(4);
        description_.includes_extended_drag_heads=required_families.contains(2);
        require(!outlined_drag_heads||required_families==std::set<int>{2},"outlined Drag heads require a pure earliest-Drag group");
        description_.includes_outlined_drag_heads=outlined_drag_heads;
        require(!native_head_appearance_witness||(required_families==std::set<int>{2}&&!outlined_drag_heads),
                "native appearance witness requires pure solid earliest Drag heads");
        description_.native_head_appearance_witness=native_head_appearance_witness;
        description_.first_down_ns=first_down_ns<0?first->start_ns-(first_flick?30*ms:0):first_down_ns;
        require(description_.first_down_ns>=0&&description_.first_down_ns<=first->start_ns,"adaptive first DOWN outside native earliest judgment");
        native_zero_=phigros_time_ns(0,chart.lines.front().bpm,chart.offset_seconds);
        source_v1_=chart.source_format_version==1;
        std::set<Ns> cuts{0,first->start_ns};std::set<std::size_t> lines;std::set<std::string> ids;
        for(std::size_t l=0;l<chart.lines.size();++l)for(const auto& n:chart.lines[l].notes) {
            if(!required_families.contains(color(n.kind)))continue;
            require(n.start_ns>=first->start_ns&&std::isfinite(n.position_x)&&std::abs(n.position_x)<=100&&
                    std::isfinite(n.floor_position)&&std::abs(n.floor_position)<=1e9&&std::isfinite(n.speed)&&std::abs(n.speed)<=10000,
                    "adaptive opening native note values unsupported");
            const auto pointer="/judgeLineList/"+std::to_string(l)+(n.side==NoteSide::above?"/notesAbove/":"/notesBelow/")+std::to_string(n.source_index);
            require(n.source.json_pointer==pointer&&n.source.id==chart.source.chart_sha256+'#'+pointer&&ids.insert(n.source.id).second,
                    "adaptive opening native note provenance invalid");
            description_.includes_hold_heads|=n.kind==NoteKind::hold;
            if(&n==first)first_note_=notes_.size();notes_.push_back({n,l});lines.insert(l);
            require(notes_.size()<=max_notes,"adaptive opening same-color note budget");
        }
        if(description_.includes_hold_heads){
            description_.model_id="native-piecewise-2d-opening-v10";
            description_.detector_policy_id=description_.includes_mixed_earliest_heads?
                "typed-family-mixed-earliest-oriented-hold-heads-v2":"typed-rle-oriented-classic-hold-head-v3";
        }else if(description_.includes_mixed_earliest_heads){
            description_.model_id="native-piecewise-2d-opening-v9";
            description_.detector_policy_id="typed-family-mixed-earliest-heads-v1";
        }else if(description_.includes_classic_flick_heads){
            description_.model_id="native-piecewise-2d-opening-v7";
            description_.detector_policy_id="typed-rle-oriented-classic-red-flick-head-v3";
        }else if(description_.includes_extended_drag_heads){
            description_.model_id="native-piecewise-2d-opening-v8";
            description_.detector_policy_id="typed-rle-oriented-drag-head-roi650-v2";
        }
        if(outlined_drag_heads){description_.model_id="native-piecewise-2d-opening-v13";
            description_.detector_policy_id="typed-rle-closed-outline-horizontal-drag-roi650-v3";}
        if(native_head_appearance_witness)description_.model_id="native-piecewise-2d-opening-v14";
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
        std::set<std::size_t> appearance_notes;
        if(native_head_appearance_witness)for(std::size_t ni=0;ni<notes_.size();++ni) {
            const auto& native=notes_[ni];const auto& line=chart.lines[native.line];const auto& n=native.note;
            const auto center=[&](Ns at) {
                const auto p=pose.line_at(native.line,at);const auto floor=floor_at(line,at);
                const auto z=mul(sub(floor,point(n.floor_position)),point(description_.approach_pixels_per_floor_unit*n.speed*(n.side==NoteSide::above?1:-1)));
                const auto along=n.position_x*72;
                return std::array<Range,2>{add(point(p.origin.x*1280+along*p.tangent.x),mul(z,point(-p.above_normal.x))),
                    add(point(p.origin.y*720+along*p.tangent.y),mul(z,point(-p.above_normal.y)))};
            };
            for(const auto& event:line.speed_events) {
                bump();const auto begin=event.time.start_ns,end=event.time.end_ns;
                if(begin<0||end<=begin||end-begin>20*ms||end>=description_.end_ns||event.value<=0)continue;
                const auto a=center(begin),b=center(end);
                if(!outside_viewport(a[0],a[1])||!fully_visible(b[0],b[1]))continue;
                require(appearance_jumps_.size()<128,"native appearance transition budget");
                appearance_jumps_.push_back({ni,{begin,end},event.source.json_pointer});appearance_notes.insert(ni);
            }
        }
        std::size_t appearance_equations=0;
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
                // Phira's native PGR parser gives Hold heads effective speed 1;
                // their line floor already contains the native scroll speed.
                // Keep the immutable raw note speed in notes_ and apply this
                // rendering rule before endpoints, derivatives and curvature.
                // https://github.com/TeamFlos/phira/blob/750579396cbc5dc61e6379fbfb2aa1adb70751a6/prpr/src/parse/pgr.rs
                const auto head_speed=n.kind==NoteKind::hold?1.0:n.speed;
                const auto scale=description_.approach_pixels_per_floor_unit*head_speed*sign;
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
                const auto numeric_padding=up(0.000001L+64*std::numeric_limits<double>::epsilon()*magnitude);
                const auto padding=up(0.000001L+64*std::numeric_limits<double>::epsilon()*magnitude+curve);
                require(std::isfinite(padding),"adaptive opening curvature enclosure overflow");
                const auto axis=[&](Range start,Range finish) {
                    const auto middle=(start.lo+start.hi)/2;
                    const auto slope=dt?(finish.lo+finish.hi-start.lo-start.hi)/(2*dt):0;
                    const auto slope_pad=dt?(start.hi-start.lo+finish.hi-finish.lo)/dt:0;
                    return Axis{{down(middle-padding),up(middle+padding)},{down(slope-slope_pad),up(slope+slope_pad)}};
                };
                Equation equation{axis(pa[0],pb[0]),axis(pa[1],pb[1]),true,speed.source.json_pointer,speed.value};
                if(description_.certified_first_group_coverage&&n.start_ns==description_.note_time_ns)
                    piece.first_group_equations.emplace_back(ni,equation);
                if(appearance_notes.contains(ni)) {
                    require(++appearance_equations<=max_equations,"native appearance geometry budget");
                    piece.appearance_equations.emplace_back(ni,equation);
                }
                auto possible=axis_match(equation.x,piece.time,piece.time.lower,-3,1283);
                possible=axis_match(equation.y,possible,piece.time.lower,27,n.kind==NoteKind::drag&&description_.includes_extended_drag_heads?653:573);
                if(!valid(possible)){++description_.provably_invisible_equations;continue;}
                require(++description_.retained_equations<=max_equations,"adaptive opening retained equation storage budget");
                if(description_.includes_classic_flick_heads){
                    const auto origin=[](const PhigrosLinePose& p){return std::array<Range,2>{
                        mul(point(p.origin.x),point(1280)),mul(point(p.origin.y),point(720))};};
                    piece.relative_poses.push_back({origin(a),origin(b),a.rotation_degrees,b.rotation_degrees,along,numeric_padding,za,zb});
                }
                piece.note_indices.push_back(ni);piece.equations.push_back(std::move(equation));
            }
            pieces_.push_back(std::move(piece));
        }
        description_.native_pieces=pieces_.size();description_.modeled_notes=notes_.size();
        std::ostringstream canonical;canonical<<description_.model_id<<'\n'<<chart.source.chart_sha256<<'\n'
            <<chart.import_policy_id<<'\n'<<chart.import_interpretation_unqualified<<'\n';
        for(const auto& anomaly:chart.import_anomalies)canonical<<anomaly.source.id<<'\n'<<anomaly.code<<'\n'<<anomaly.original_declaration_json<<'\n';
        for(const auto& id:description_.earliest_note_ids)canonical<<id<<'\n';
        // Keep the archived 200ms token in the pinned geometry model digest.
        // The actual dispatch policy is independently recorded in the run gate.
        canonical<<description_.note_time_ns<<'\n'<<description_.first_down_ns<<'\n'<<description_.detector_policy_id<<'\n'<<std::setprecision(17)
            <<description_.approach_pixels_per_floor_unit<<"\npixel_error=3;chord_curvature_enclosure;native_zero_floor_hold;full_head_visibility=95,1184,125,474;lead=200ms";
        if(description_.includes_extended_drag_heads)canonical<<"\nfirst_drag_detector_roi=0,1280,30,650;clipped_head_rejected;shape_thresholds_legacy_v1;negative_full_head_visibility_legacy=95,1184,125,474";
        if(native_head_appearance_witness) {
            canonical<<"\nnative-certified-head-pop-in-v1;pure-solid-Drag-only;all-actual-support-origin-unions-intersected;no-alias-preference;earliest-positive-both;native-continuous-positive-speed-segment<=20ms;true-viewport-offscreen-margin95;full-head-ROI=95,1184,125,554;unique-new-source-and-speed-event;all-pair-aliases-covered;origin-certificate-reproduced-by2or3-actual-distinct-SHA-supports;all-contributing-evidence-IDs-retained;dt20..200ms;history16;pair65536;origin-width<=40ms;fresh100ms;lead200ms;complete-unoccluded-rendering-hypothesis-unqualified";
            for(const auto& jump:appearance_jumps_)canonical<<'\n'<<notes_[jump.note].note.source.id<<':'<<jump.time.lower<<':'<<jump.time.upper<<':'<<jump.pointer;
        }
        if(outlined_drag_heads)canonical<<"\noutlined-horizontal-drag-heads-v3;original-yellow-RLE;connected-components;roi=0,1280,30,650;runs<=16384;components<=4096;centers<=8;pixel-work<=4000000;solid-occupancy>=.72;outline-width=40..180;height=4..18;aspect>=4;exactly-two-central-row-bands;each-band<=floor((height-1)/2);empty-central-gap>=1row;standalone-cap-tube<=height/2+2;nested-qualified-components<=16;unique-solid-core-occupancy>=.72;strict-nesting;center-distance<=1;gap-asymmetry<=2;horizontal-gaps=4..24;vertical-gaps=2..6;width-difference=8..48;height-difference=4..12;outer-only-mask;derived-cap-tube=max-horizontal-gap+2<=26;every-qualified-component-owned-once;unqualified-nesting-rejected;inner-bbox-center;full-long-rail-in-each-band;original-cap-connectivity;clipping-rejected;whole-qualified-frame-closure;bbox-center-only";
        if(description_.includes_mixed_earliest_heads){
            canonical<<"\nmixed_earliest_families=";for(std::size_t i=0;i<description_.required_center_families.size();++i){if(i)canonical<<',';canonical<<description_.required_center_families[i];}
            canonical<<"\nmixed_center_families=explicit-per-center-no-cross-family-matches-v1\nmixed_cross_detector_ownership=distance>3px;total_centers<=8";
        }
        if(source_v1_)canonical<<"\nsource_format_version=1;missing_speed_floors=tick_zero_integral_v1";
        if(description_.includes_hold_heads)canonical<<"\nhold_head_effective_speed=1\ncoincident_head_occlusion=direct-same-kind-side-piece-l1<=1/1024px-v1";
        if(description_.includes_hold_heads)canonical<<"\nclassic_hold_head_detector=oriented-raw-pixel-cap-pca-dual-axis-v1;anchor=body-side-cap-boundary;ROI=0,1280,30,570;thresholds=classic-hold-v2";
        if(description_.includes_hold_heads)canonical<<"\noriented_hold_sampling=nearest-original-raster;grid_phases=0,1/2,1/4,3/4;axis_refinement_degrees=0,+1,-1,+2,-2,+3,-3;cap_radius<91;pixel_work_budget=4000000;interpretation=first-complete-cap-per-component;no-centroid-or-phase-average;coverage-unverified";
        if(description_.includes_hold_heads)canonical<<"\noriented_hold_storage=live-bands<=128-per-proposal;qualified-rows<=4096-per-proposal;all-proposal-raw-reads<=4000000;output<=8;proposal_order=phase-u,phase-v,angle,axis";
        if(description_.includes_hold_heads)canonical<<"\noriented_hold_component=whole-original-RLE-endpoints-in-observed-cap-outer-white-chevron-tube;raster-bound=1.415px;shared-bent-component-rejects-whole-frame";
        if(description_.includes_hold_heads)canonical<<"\noriented_hold_rail_proposals=original-white-RLE-PCA;run<=16384;components<=4096;thin-rails<=128;major>=20;minor<=12;aspect>=4;cyan-adjacency<=10;fit-original-runs-clipped-to-cyan-bbox+20;one-original-major-endpoint-within-cyan+20;unique-directions<=8;dedup-abs-cos>0.99996;positive-cap-shape-unchanged;incompatible-same-cyan-rail-directions>3deg-reject-whole-frame";
        if(description_.includes_classic_flick_heads)canonical<<"\nclassic_red_flick_head_detector=typed-rle-oriented-classic-red-flick-head-v3";
        if(description_.includes_classic_flick_heads)canonical<<"\nflick_head_occlusion=direct-same-kind-side-piece-relative-pose-l1<=1/1024px-v1";
        description_.model_sha256=chart_text_sha256(canonical.str());value_.reason="waiting_for_native_earliest_group_motion";
    }
    const AdaptiveOpeningDescription& description()const{return description_;}
    FirstNoteTriggerStatus observe(const VisualCandidateFrame& f,bool ui_healthy,Ns now) {return observe(f,{},ui_healthy,now);}
    FirstNoteTriggerStatus observe(const VisualCandidateFrame& f,const std::vector<int>& families,bool ui_healthy,Ns now,
                                   const std::vector<NoteKind>& unavailable_kinds={}) {
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
        if((description_.includes_mixed_earliest_heads||!families.empty())&&(families.size()!=f.centers.size()||
            std::any_of(families.begin(),families.end(),[&](int family){return std::find(description_.required_center_families.begin(),description_.required_center_families.end(),family)==description_.required_center_families.end();}))){
            cancel("invalid_or_missing_first_note_center_families");return value_;
        }
        if(!unavailable_kinds.empty()){
            std::set<NoteKind> unique;
            const auto valid_kind=[&](NoteKind kind){
                if(kind!=NoteKind::tap&&kind!=NoteKind::hold&&kind!=NoteKind::drag&&kind!=NoteKind::flick)return false;
                const auto family=kind==NoteKind::drag?2:kind==NoteKind::flick?4:1;
                return std::find(description_.required_center_families.begin(),description_.required_center_families.end(),family)
                    !=description_.required_center_families.end()&&unique.insert(kind).second;
            };
            if(!description_.certified_first_group_coverage||unavailable_kinds.size()>4||
               !std::all_of(unavailable_kinds.begin(),unavailable_kinds.end(),valid_kind)){
                cancel("invalid_first_note_unavailable_detector_kinds");return value_;
            }
        }
        std::set<std::string> center_ids;
        for(std::size_t i=0;i<f.centers.size();++i){const auto& c=f.centers[i];
            const bool extended=description_.includes_extended_drag_heads&&(!description_.includes_mixed_earliest_heads||families[i]==2);
            if(!text(c.id,128)||!std::isfinite(c.x)||!std::isfinite(c.y)||c.x<0||c.x>=1280||c.y<30||c.y>=(extended?650:570)||!center_ids.insert(c.id).second) {
            cancel("invalid_first_note_centers");return value_;
        }}
        if(description_.includes_mixed_earliest_heads)for(std::size_t i=0;i<f.centers.size();++i)for(std::size_t j=i+1;j<f.centers.size();++j)
            if(families[i]!=families[j]&&std::hypot(f.centers[i].x-f.centers[j].x,f.centers[i].y-f.centers[j].y)<=3){
                cancel("mixed_cross_family_head_ownership_ambiguous");return value_;
            }
        value_.raw_candidates.clear();value_.raw_candidate_count=value_.first_note_positive_candidates=0;value_.selected_intervals.clear();value_.anchor.reset();
        if(f.centers.empty()){reset("waiting_for_native_first_note_centers");return value_;}
        try {work_=0;value_.raw_candidates=solve(f,families,unavailable_kinds);}catch(const std::runtime_error& e){cancel(e.what());return value_;}
        value_.raw_candidate_count=value_.raw_candidates.size();
        if(value_.raw_candidates.empty()){reset("native_earliest_group_missing_or_ambiguous");return value_;}
        const auto is_first=[&](const auto& match){
            return std::find(description_.earliest_note_ids.begin(),description_.earliest_note_ids.end(),match.note_id)
                !=description_.earliest_note_ids.end();
        };
        Support observed{value_.raw_candidates,f,{},{},{}};
        for(const auto& candidate:observed.candidates){
            const auto count=static_cast<std::size_t>(std::count_if(candidate.matches.begin(),candidate.matches.end(),is_first));
            observed.time.push_back(candidate.song_time);
            if(count==description_.earliest_note_count)++value_.first_note_positive_candidates;
            else observed.without_complete_group.push_back(candidate.song_time);
            if(count==0)observed.without_first.push_back(candidate.song_time);
        }
        observed.time=merge(std::move(observed.time));
        observed.without_complete_group=merge(std::move(observed.without_complete_group));
        observed.without_first=merge(std::move(observed.without_first));
        std::erase_if(support_history_,[&](const auto& support){return now-support.frame.received_host_ns>200*ms;});
        if(support_history_.size()>=16){cancel("adaptive_opening_support_history_budget");return value_;}
        support_history_.push_back(std::move(observed));const auto& current=support_history_.back();
        value_.support_count=1;value_.state=FirstNoteTriggerState::acquiring;value_.reason="waiting_for_second_distinct_native_group_frame";
        if(support_history_.size()==1)return value_;
        if(description_.native_head_appearance_witness) {
            try{return observe_appearance(now);}catch(const std::runtime_error& e){cancel(e.what());return value_;}
        }
        const auto center_for=[](const VisualTimeCandidate& candidate,const VisualCandidateFrame& frame,const std::string& id)->const VisualCenter* {
            const auto m=std::find_if(candidate.matches.begin(),candidate.matches.end(),[&](const auto& v){return v.note_id==id;});
            if(m==candidate.matches.end())return nullptr;
            const auto c=std::find_if(frame.centers.begin(),frame.centers.end(),[&](const auto& v){return v.id==m->center_id;});
            return c==frame.centers.end()?nullptr:&*c;
        };
        std::vector<VisualTimeInterval> selected;bool unresolved=false,eligible=false;
        std::vector<std::vector<VisualTimeInterval>> by_support(support_history_.size()-1);
        const bool certified=description_.certified_first_group_coverage;
        for(std::size_t support=0;support+1<support_history_.size();++support) {
            const auto& previous=support_history_[support];const auto dt=f.received_host_ns-previous.frame.received_host_ns;
            if(dt<20*ms||dt>200*ms||f.frame_sha256==previous.frame.frame_sha256)continue;
            eligible=true;
            // Intersections distribute over union. Compute the exact temporal
            // union and negative witness vetoes before comparing source pairs.
            // A large number of indistinguishable ornaments still vetoes an
            // anchor, without spending the pair budget just to learn that.
            auto& overlap=by_support[support];
            overlap=intersect_receipts(current.time,previous.time,dt);
            selected.insert(selected.end(),overlap.begin(),overlap.end());
            if(certified) {
                unresolved|=!intersect_receipts(current.without_first,previous.without_first,dt).empty();
            }else{
                unresolved|=!intersect_receipts(current.without_complete_group,previous.time,dt).empty()||
                    !intersect_receipts(current.time,previous.without_complete_group,dt).empty();
            }
        }
        if(!eligible){value_.reason="native_group_frames_not_distinct_or_too_far_apart";return value_;}
        value_.selected_intervals=merge(std::move(selected));
        if(unresolved||value_.selected_intervals.size()!=1||value_.selected_intervals[0].upper-value_.selected_intervals[0].lower>40*ms) {
            value_.reason="native_group_temporal_or_source_ambiguity_retained";return value_;
        }
        // Retain every compatible temporal branch, rather than selecting a
        // favorable historical pair. One real pair must support the entire
        // retained interval; an overlap chain alone cannot authorize input.
        std::optional<std::size_t> supporting;
        const auto chosen=value_.selected_intervals[0];
        std::size_t pairs=0;bool used_certified_witness=false;
        for(std::size_t support=by_support.size();support-->0;) {
            const auto& intervals=by_support[support];
            if(intervals.empty())continue;
            const bool covers=std::any_of(intervals.begin(),intervals.end(),[&](const auto& interval){
                return interval.lower<=chosen.lower&&chosen.upper<=interval.upper;});
            if(!certified&&(!covers||supporting))continue;
            const auto& previous=support_history_[support];const auto dt=f.received_host_ns-previous.frame.received_host_ns;
            const bool needs_coverage=certified&&(!previous.without_complete_group.empty()||!current.without_complete_group.empty());
            if(!needs_coverage&&(!covers||supporting))continue;
            bool motion_resolved=true,additional_evidence=false;
            for(const auto& c:current.candidates)for(const auto& p:previous.candidates) {
                if(++pairs>65536){cancel("adaptive_opening_pair_budget");return value_;}
                const auto interval=intersect(c.song_time,{p.song_time.lower+dt,p.song_time.upper+dt});
                if(!valid(interval))continue;
                if(needs_coverage)try {
                    if(!certified_group_present(c,interval)||
                       !certified_group_present(p,{interval.lower-dt,interval.upper-dt})){
                        value_.reason="native_group_temporal_or_source_ambiguity_retained";return value_;
                    }
                    additional_evidence=true;
                }catch(const std::runtime_error& e){cancel(e.what());return value_;}
                if(!motion_resolved)continue;
                bool first_motion=false,other_motion=false;
                for(const auto& match:c.matches) {
                    const bool earliest=is_first(match);
                    if(!earliest&&!certified)continue;
                    const auto* a=center_for(p,previous.frame,match.note_id);
                    const auto* b=center_for(c,f,match.note_id);
                    if(a&&b&&std::hypot(b->x-a->x,b->y-a->y)>6){
                        first_motion|=earliest;other_motion|=!earliest;
                    }
                }
                motion_resolved=first_motion||other_motion;
                additional_evidence|=!first_motion&&other_motion;
            }
            if(covers&&motion_resolved&&!supporting){
                supporting=support;used_certified_witness=additional_evidence;
            }
        }
        if(!supporting){value_.reason="native_group_interval_union_not_supported_by_one_actual_pair";return value_;}
        value_.support_count=2;FirstNoteTriggerAnchor anchor;anchor.received_host_ns=f.received_host_ns;
        anchor.chosen_song_interval=value_.selected_intervals[0];anchor.origin_host_ns=f.received_host_ns-midpoint(anchor.chosen_song_interval);
        anchor.target_host_ns=anchor.origin_host_ns+description_.first_down_ns+phase_;anchor.evidence_id=f.evidence_id;
        anchor.supporting_evidence_ids={support_history_[*supporting].frame.evidence_id,f.evidence_id};
        if(!allows_first_dispatch(anchor,now))return value_;
        value_.anchor=std::move(anchor);value_.state=FirstNoteTriggerState::ready;
        value_.reason=used_certified_witness?"two_distinct_native_frames_certify_first_group_and_motion":
            "two_distinct_native_group_motion_frames_on_unique_temporal_branch";return value_;
    }
    const std::optional<AdaptiveFirstDispatchGate>& dispatch_gate() const noexcept{return dispatch_gate_;}
    FirstNoteTriggerStatus status(Ns now,bool healthy=true){health(now,healthy);return value_;}
    std::optional<FirstNoteTriggerAnchor> take_anchor(Ns now) {
        if(consumed_||!health(now,true)||value_.state!=FirstNoteTriggerState::ready||!value_.anchor)return std::nullopt;
        consumed_=true;value_.state=FirstNoteTriggerState::fired;value_.reason="first_note_trigger_fired_once";return value_.anchor;
    }
    void cancel(std::string reason){if(value_.state==FirstNoteTriggerState::cancelled)return;value_.state=FirstNoteTriggerState::cancelled;value_.reason=reason.substr(0,512);support_history_.clear();value_.anchor.reset();}
};
} // namespace rhythm::experimental
