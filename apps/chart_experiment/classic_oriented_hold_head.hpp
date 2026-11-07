#pragma once
#include "../chart_calibrate/opening_detector.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace rhythm::experimental {
// A raw-pixel, bounded extension of classic_hold_head's terminal-cap test.
// PCA proposes both local axes only; no component centroid is a head/pivot.
// Every accepted pivot needs the original cyan/chevrons/gaps and a body on
// exactly one side. Local coordinates sample actual nearest raster pixels;
// no rotated image, inferred head, completeness or native identity is made.
inline calibration::OpeningDetection detect_classic_oriented_hold_head_centers(
        int width,int height,int stride,std::span<const std::uint8_t> rgb) {
    calibration::OpeningDetection out;
    if(width!=1280||height!=720||stride<3840||stride>16384||rgb.size()!=static_cast<std::size_t>(stride)*720)return out;
    out.reason="no_supported_classic_oriented_hold_head";
    std::size_t samples=0;
    const auto pixel=[&](int x,int y){if(++samples>4'000'000)return std::array<int,3>{0,0,0};
        const auto p=static_cast<std::size_t>(y)*stride+static_cast<std::size_t>(x)*3;
        return std::array<int,3>{rgb[p],rgb[p+1],rgb[p+2]};};
    const auto light=[&](int x,int y){const auto c=pixel(x,y);return c[0]>=80&&c[0]<=210&&c[1]>=160&&c[2]>=185&&c[1]-c[0]>=25&&c[2]-c[0]>=35;};
    const auto white=[&](int x,int y){const auto c=pixel(x,y);return std::min({c[0],c[1],c[2]})>=210&&std::max({c[0],c[1],c[2]})-std::min({c[0],c[1],c[2]})<=30;};
    const auto gap=[&](int x,int y){const auto c=pixel(x,y);return std::max({c[0],c[1],c[2]})<=130||
        (c[0]>=140&&c[1]>=120&&c[2]<=130&&std::min(c[0],c[1])-c[2]>=60);};
    const auto fail=[&](const char* why){out.valid=false;out.reason=why;out.centers.clear();out.metrics.clear();return out;};
    struct Run{int y,a,b;std::size_t parent;};
    std::vector<Run> runs;runs.reserve(2048);
    std::array<std::pair<std::size_t,std::size_t>,720> row_ranges{};
    const auto root=[&](std::size_t i){auto r=i;while(runs[r].parent!=r)r=runs[r].parent;
        while(runs[i].parent!=i){const auto next=runs[i].parent;runs[i].parent=r;i=next;}return r;};
    for(int y=30;y<570;++y){const auto begin_row=runs.size();std::size_t previous=y>30?row_ranges[y-1].first:0;
        const auto previous_end=y>30?row_ranges[y-1].second:0;int x=0;
        while(x<1280){while(x<1280&&!light(x,y))++x;if(x==1280)break;
            const auto a=x;while(x<1280&&light(x,y))++x;
            if(runs.size()>=16384)return fail("classic_oriented_hold_head_run_budget_exhausted");
            const auto i=runs.size();runs.push_back({y,a,x-1,i});
            while(previous<previous_end&&runs[previous].b+1<a)++previous;
            for(auto p=previous;p<previous_end&&runs[p].a<=x;++p){const auto r=root(i),s=root(p);if(r!=s)runs[std::max(r,s)].parent=std::min(r,s);}}
        row_ranges[y]={begin_row,runs.size()};}
    if(samples>4'000'000)return fail("classic_oriented_hold_head_pixel_work_budget_exhausted");
    struct Component{double count=0,sx=0,sy=0,sxx=0,syy=0,sxy=0;std::vector<std::size_t> indices;
        int xmin=1280,xmax=-1,ymin=720,ymax=-1;};
    std::vector<Component> components(runs.size());std::size_t component_count=0;
    const auto squares=[](double n){return n*(n+1)*(2*n+1)/6;};
    for(std::size_t i=0;i<runs.size();++i){const auto& r=runs[i];auto& c=components[root(i)];
        if(c.count==0&&++component_count>4096)return fail("classic_oriented_hold_head_component_budget_exhausted");
        const double n=r.b-r.a+1,sum=(r.a+r.b)*n/2;c.count+=n;c.sx+=sum;c.sy+=n*r.y;
        c.sxx+=squares(r.b)-squares(r.a-1);c.syy+=n*r.y*r.y;c.sxy+=sum*r.y;c.indices.push_back(i);
        c.xmin=std::min(c.xmin,r.a);c.xmax=std::max(c.xmax,r.b);c.ymin=std::min(c.ymin,r.y);c.ymax=std::max(c.ymax,r.y);}
    // White body rails/chevrons provide independent raw-pixel directions when
    // a nearly-square cyan body makes its covariance ill-conditioned. Their
    // RLE graph is bounded separately; every read shares the same4M budget.
    std::vector<Run> white_runs;white_runs.reserve(2048);
    const auto white_root=[&](std::size_t i){auto r=i;while(white_runs[r].parent!=r)r=white_runs[r].parent;
        while(white_runs[i].parent!=i){const auto next=white_runs[i].parent;white_runs[i].parent=r;i=next;}return r;};
    std::size_t wb=0,we=0;
    for(int y=30;y<570;++y){const auto begin_row=white_runs.size();auto previous=wb;int x=0;
        while(x<1280){while(x<1280&&!white(x,y))++x;if(x==1280)break;
            const auto a=x;while(x<1280&&white(x,y))++x;
            if(white_runs.size()>=16384)return fail("classic_oriented_hold_white_run_budget_exhausted");
            const auto i=white_runs.size();white_runs.push_back({y,a,x-1,i});
            while(previous<we&&white_runs[previous].b+1<a)++previous;
            for(auto p=previous;p<we&&white_runs[p].a<=x;++p){const auto r=white_root(i),s=white_root(p);if(r!=s)white_runs[std::max(r,s)].parent=std::min(r,s);}}
        wb=begin_row;we=white_runs.size();if(samples>4'000'000)return fail("classic_oriented_hold_head_pixel_work_budget_exhausted");}
    std::vector<Component> white_components(white_runs.size());std::size_t white_count=0;
    for(std::size_t i=0;i<white_runs.size();++i){const auto& r=white_runs[i];auto& c=white_components[white_root(i)];
        if(c.count==0&&++white_count>4096)return fail("classic_oriented_hold_white_component_budget_exhausted");
        const double n=r.b-r.a+1,sum=(r.a+r.b)*n/2;c.count+=n;c.sx+=sum;c.sy+=n*r.y;
        c.sxx+=squares(r.b)-squares(r.a-1);c.syy+=n*r.y*r.y;c.sxy+=sum*r.y;c.indices.push_back(i);
        c.xmin=std::min(c.xmin,r.a);c.xmax=std::max(c.xmax,r.b);c.ymin=std::min(c.ymin,r.y);c.ymax=std::max(c.ymax,r.y);}
    struct Rail{std::size_t component;double ux,uy;};std::vector<Rail> rails;rails.reserve(32);
    for(std::size_t i=0;i<white_components.size();++i){const auto& c=white_components[i];if(c.count<20)continue;
        const auto x=c.sx/c.count,y=c.sy/c.count;
        const auto a=.5*std::atan2(2*(c.sxy/c.count-x*y),(c.sxx-c.syy)/c.count-x*x+y*y);
        const auto ux=std::cos(a),uy=std::sin(a);double lo=1e9,hi=-1e9,vl=1e9,vh=-1e9;
        for(const auto index:c.indices){const auto& r=white_runs[index];for(const int px:{r.a,r.b}){
            const auto u=ux*px+uy*r.y,v=-uy*px+ux*r.y;lo=std::min(lo,u);hi=std::max(hi,u);vl=std::min(vl,v);vh=std::max(vh,v);}}
        const auto extent=std::abs(ux)+std::abs(uy),major=hi-lo+extent,minor=vh-vl+extent;
        if(major<20||minor>12||major<4*minor)continue;
        if(rails.size()>=128)return fail("classic_oriented_hold_white_rail_budget_exhausted");
        rails.push_back({i,ux,uy});}
    out.groups=component_count;
    struct Row{int v,a,b;double left_white,right_white,left_outer,right_outer;};
    struct Band{int last,last_u2;std::vector<Row> rows;};
    for(std::size_t ci=0;ci<components.size();++ci){const auto& c=components[ci];if(c.count<140)continue;
        const auto cx=c.sx/c.count,cy=c.sy/c.count;
        const auto angle=.5*std::atan2(2*(c.sxy/c.count-cx*cy),(c.sxx-c.syy)/c.count-cx*cx+cy*cy);
        if(!std::isfinite(angle))return fail("classic_oriented_hold_head_nonfinite_axis");
        std::vector<double> directions;directions.reserve(10);std::vector<Rail> associated_rails;associated_rails.reserve(8);
        for(std::size_t ri=0;ri<rails.size();++ri){const auto& rail=rails[ri];const auto& w=white_components[rail.component];
            if(w.xmax<c.xmin-10||w.xmin>c.xmax+10||w.ymax<c.ymin-10||w.ymin>c.ymax+10)continue;
            // Alpha-faded cyan can end before a white rail. Crop original
            // white runs to the cyan's20px gap+white neighborhood, then fit
            // only those actual pixels. Do not fabricate/fill a cropped image.
            const auto clip=[&](const Run& r){return std::pair{std::max(r.a,c.xmin-20),std::min(r.b,c.xmax+20)};};
            const auto in_neighborhood=[&](int x,int y){return x>=c.xmin-20&&x<=c.xmax+20&&y>=c.ymin-20&&y<=c.ymax+20;};
            Component local;double first_u=1e9,last_u=-1e9;std::array<int,2> first_endpoint{},last_endpoint{};
            for(const auto index:w.indices){if(++samples>4'000'000)return fail("classic_oriented_hold_head_pixel_work_budget_exhausted");
                const auto& r=white_runs[index];for(const int x:{r.a,r.b}){const auto u=rail.ux*x+rail.uy*r.y;
                    if(u<first_u){first_u=u;first_endpoint={x,r.y};}if(u>last_u){last_u=u;last_endpoint={x,r.y};}}
                if(r.y<c.ymin-20||r.y>c.ymax+20)continue;const auto [a,b]=clip(r);if(a>b)continue;
                const double n=b-a+1,sum=(a+b)*n/2;local.count+=n;local.sx+=sum;local.sy+=n*r.y;
                local.sxx+=squares(b)-squares(a-1);local.syy+=n*r.y*r.y;local.sxy+=sum*r.y;}
            // A body rail terminates at a cap/tail within this neighborhood.
            // Both ends of an unrelated full-screen judge line are outside.
            if(!in_neighborhood(first_endpoint[0],first_endpoint[1])&&!in_neighborhood(last_endpoint[0],last_endpoint[1]))continue;
            if(local.count<20)continue;const auto lx=local.sx/local.count,ly=local.sy/local.count;
            const auto local_angle=.5*std::atan2(2*(local.sxy/local.count-lx*ly),(local.sxx-local.syy)/local.count-lx*lx+ly*ly);
            const auto local_ux=std::cos(local_angle),local_uy=std::sin(local_angle);
            double lu=1e9,hu=-1e9,lv=1e9,hv=-1e9;
            for(const auto index:w.indices){const auto& r=white_runs[index];if(r.y<c.ymin-20||r.y>c.ymax+20)continue;
                const auto [a,b]=clip(r);if(a>b)continue;for(const int x:{a,b}){
                    const auto u=local_ux*x+local_uy*r.y,v=-local_uy*x+local_ux*r.y;
                    lu=std::min(lu,u);hu=std::max(hu,u);lv=std::min(lv,v);hv=std::max(hv,v);}}
            const auto extent=std::abs(local_ux)+std::abs(local_uy),major=hu-lu+extent,minor=hv-lv+extent;
            if(major<20||minor>12||major<4*minor)continue;
            bool adjacent=false;
            for(const auto index:w.indices){const auto& wr=white_runs[index];if(wr.y<c.ymin-20||wr.y>c.ymax+20)continue;
                const auto [a,b]=clip(wr);if(a>b)continue;for(const int px:{a,(a+b)/2,b}){
                for(int y=std::max(30,wr.y-10);y<=std::min(569,wr.y+10)&&!adjacent;++y){
                    if(++samples>4'000'000)return fail("classic_oriented_hold_head_pixel_work_budget_exhausted");
                    const auto [begin,end]=row_ranges[y];auto lo=begin,hi=end;
                    while(lo<hi){const auto mid=lo+(hi-lo)/2;if(runs[mid].b<px-10)lo=mid+1;else hi=mid;}
                    for(auto k=lo;k<end&&runs[k].a<=px+10;++k)if(root(k)==ci){adjacent=true;break;}}
                if(adjacent)break;}if(adjacent)break;}
            if(!adjacent)continue;associated_rails.push_back({rail.component,local_ux,local_uy});
            const auto a=std::atan2(-local_ux,local_uy);
            if(std::none_of(directions.begin(),directions.end(),[&](double p){return std::abs(std::cos(a-p))>.99996;})){
                if(directions.size()>=8)return fail("classic_oriented_hold_head_direction_budget_exhausted");directions.push_back(a);}}
        directions.push_back(angle);directions.push_back(angle+1.57079632679489661923);
        // Nearly square head+body components have ill-conditioned PCA angles.
        // Propose only a finite local angular refinement and four raster-grid
        // phases on each axis. All RGB/shape tests still inspect original
        // pixels with their original thresholds. Stop after a complete cap
        // interpretation; never average phases or turn a centroid into a head.
        const auto prior_centers=out.centers.size();bool component_found=false,component_tube_rejected=false;
        for(const double phase_u:{0.,.5,.25,.75})for(const double phase_v:{0.,.5,.25,.75})
          for(const double refine:{0.,1.,-1.,2.,-2.,3.,-3.})for(const double direction:directions){
            if(component_found)continue;
            const auto a=direction+refine*.01745329251994329577;
            const auto ux=std::cos(a),uy=std::sin(a),vx=-uy,vy=ux;
            double umin=1e9,umax=-1e9,vmin=1e9,vmax=-1e9;
            for(const auto i:c.indices){const auto& r=runs[i];for(const int x:{r.a,r.b}){
                const auto u=ux*x+uy*r.y,v=vx*x+vy*r.y;umin=std::min(umin,u);umax=std::max(umax,u);vmin=std::min(vmin,v);vmax=std::max(vmax,v);}}
            if(umax-umin<68||umax-umin>260)continue;
            const auto sample=[&](double u,double v,const auto& predicate){
                if(samples>4'000'000)return false;
                const auto x=static_cast<int>(std::lround((u+phase_u)*ux+(v+phase_v)*vx)),y=static_cast<int>(std::lround((u+phase_u)*uy+(v+phase_v)*vy));
                return x>=0&&x<1280&&y>=0&&y<720&&predicate(x,y);};
            const auto owned_light=[&](int x,int y){
                if(!light(x,y))return false;
                // Support outside the search ROI is a raw body probe only.
                if(y<30||y>=570)return true;
                const auto [begin,end]=row_ranges[y];auto lo=begin,hi=end;
                while(lo<hi){const auto mid=lo+(hi-lo)/2;if(runs[mid].b<x)lo=mid+1;else hi=mid;}
                return lo<end&&runs[lo].a<=x&&root(lo)==ci;
            };
            const auto transition=[&](double u,int v){return !sample(u,v,light)&&!sample(u,v,white);};
            const auto side=[&](int u,int step,int v,double& center,double& outer){int gaps=0,cores=0,soft=0,whites=0;
                while(transition(u,v)&&gaps<10){if(sample(u,v,gap))++cores;else if(++soft>2)return false;++gaps;u+=step;}
                if(!gaps||!cores)return false;const auto first=u;
                while(sample(u,v,white)&&whites<10){++whites;u+=step;}
                center=(first+u-step)/2.;outer=u-step+.5*step;return whites>0&&transition(u,v);};
            // These vectors are destroyed at the end of this one proposal.
            // Retried views of the same pixels are charged to global samples,
            // not to storage that no longer exists.
            std::vector<Band> bands;bands.reserve(8);std::size_t qualified_rows=0;
            const auto first_u=static_cast<int>(std::floor(umin-phase_u))-1,last_u=static_cast<int>(std::ceil(umax-phase_u))+1;
            for(int v=static_cast<int>(std::floor(vmin-phase_v))-1;v<=static_cast<int>(std::ceil(vmax-phase_v))+1;++v){
                int u=first_u;
                while(u<=last_u){while(u<=last_u&&!sample(u,v,owned_light))++u;if(u>last_u)break;
                    const auto begin=u;while(u<=last_u&&sample(u,v,owned_light))++u;const auto length=u-begin;
                    if(length<70||length>180)continue;double lw=0,rw=0,lo=0,ro=0;
                    if(!side(begin-1,-1,v,lw,lo)||!side(u,1,v,rw,ro))continue;
                    ++out.qualifying_runs;
                    if(++qualified_rows>4096)return fail("classic_oriented_hold_head_qualified_row_budget_exhausted");
                    const auto u2=begin+u-1;std::size_t selected=bands.size(),compatible=0;
                    for(std::size_t i=0;i<bands.size();++i)if(bands[i].last==v-1&&std::abs(bands[i].last_u2-u2)<=4){selected=i;++compatible;}
                    if(compatible>1)return fail("classic_oriented_hold_head_band_ownership_ambiguous");
                    if(!compatible){if(bands.size()>=128)return fail("classic_oriented_hold_head_band_budget_exhausted");bands.push_back({v,u2,{}});selected=bands.size()-1;}
                    auto& band=bands[selected];band.last=v;band.last_u2=u2;band.rows.push_back({v,begin,u-1,lw,rw,lo,ro});
                }
                if(samples>4'000'000)return fail("classic_oriented_hold_head_pixel_work_budget_exhausted");
            }
            for(const auto& band:bands)for(const bool body_negative:{true,false}){
                if(band.rows.size()<2)continue;const auto& terminal=body_negative?band.rows.back():band.rows.front();
                const auto count=static_cast<std::size_t>(std::lround((terminal.b-terminal.a+1)/17.));
                if(count<2||count>18||band.rows.size()<count)continue;
                const auto first_index=body_negative?band.rows.size()-count:0;
                const auto& first=band.rows[first_index];const auto& last=band.rows[first_index+count-1];
                const auto& body_edge=body_negative?first:last;const auto& outer_edge=body_negative?last:first;
                const auto taper_left=outer_edge.left_white-body_edge.left_white,taper_right=body_edge.right_white-outer_edge.right_white;
                if(taper_left<.75||taper_right<.75||taper_left>static_cast<double>(count)*1.5||taper_right>static_cast<double>(count)*1.5)continue;
                if(band.rows.size()>=count+3){const auto& p=body_negative?band.rows[first_index-3]:band.rows[count];
                    const auto& q=body_negative?band.rows[first_index-1]:band.rows[count+2];
                    if(std::abs(p.left_white-q.left_white)>.75||std::abs(p.right_white-q.right_white)>.75)continue;}
                double u=0,left=1e9,right=-1e9;int min_width=1280,max_width=0;bool coherent=true;
                for(std::size_t i=first_index;i<first_index+count;++i){const auto& row=band.rows[i];u+=(row.a+row.b)/2.;
                    min_width=std::min(min_width,row.b-row.a+1);max_width=std::max(max_width,row.b-row.a+1);
                    left=std::min(left,row.left_outer);right=std::max(right,row.right_outer);
                    if(i>first_index){const auto& p=band.rows[i-1];const auto sign=body_negative?1.:-1.;
                        if((row.left_white-p.left_white)*sign<-.5||(p.right_white-row.right_white)*sign<-.5||
                           std::abs(row.left_white-p.left_white)>2||std::abs(row.right_white-p.right_white)>2)coherent=false;}}
                if(!coherent||max_width-min_width>std::max(6,max_width/8))continue;u/=static_cast<double>(count);
                int negative=0,positive=0;
                for(const int distance:{2,4,6})for(const double f:{-.3,0.,.3}){
                    negative+=sample(u+f*min_width,first.v-distance,light)?1:0;
                    positive+=sample(u+f*min_width,last.v+distance,light)?1:0;}
                if(negative>=8&&positive>=8)return fail("classic_oriented_hold_head_body_side_ambiguous");
                if(body_negative?!(negative>=8&&positive<=1):!(positive>=8&&negative<=1))continue;
                const auto pivot_v=body_negative?first.v-.5:last.v+.5;
                const auto px=(u+phase_u)*ux+(pivot_v+phase_v)*vx,py=(u+phase_u)*uy+(pivot_v+phase_v)*vy;
                // A single straight source body must remain inside this
                // actually observed cap's outer white-chevron tube. A bent
                // branch/second differently oriented head cannot be silently
                // hidden by the first complete interpretation of a shared CC.
                // RLE endpoints bound every pixel along each original run;
                // 1.415px is the existing outward two-axis raster bound.
                bool tube_supported=true;
                for(const auto i:c.indices){const auto& run=runs[i];for(const int x:{run.a,run.b}){
                        if(++samples>4'000'000)return fail("classic_oriented_hold_head_pixel_work_budget_exhausted");
                        const auto transverse=ux*x+uy*run.y;
                        if(transverse<left+phase_u-1.415||transverse>right+phase_u+1.415){tube_supported=false;break;}}
                    if(!tube_supported)break;}
                if(!tube_supported){component_tube_rejected=true;continue;}
                // Adjacent thin white body rails in this same cyan graph
                // cannot simultaneously belong to incompatible orientations.
                // Reject the whole frame; never keep one cap and hide a
                // crossed/embedded cap as an unobserved source. This is only
                // a conservative negative; rails never create positive heads.
                bool rail_ownership=true;
                for(const auto& rail:associated_rails){
                    if(std::abs(vx*rail.ux+vy*rail.uy)<.9986295347545738){rail_ownership=false;break;}}
                if(!rail_ownership)return fail("crossed_classic_oriented_hold_white_rail_ownership");
                for(const double cu:{left,right})for(const double cv:{first.v-.5,last.v+.5}){
                    // Same <91px cap envelope underlying the solver's95px
                    // full-head exclusion; body support is not a cap extent.
                    if(std::hypot(cu-u,cv-pivot_v)>=91)return fail("oversized_classic_oriented_hold_head_envelope");
                    const auto x=(cu+phase_u)*ux+(cv+phase_v)*vx,y=(cu+phase_u)*uy+(cv+phase_v)*vy;
                    if(x<=0||x>=1279||y<=30||y>=569)return fail("clipped_classic_oriented_hold_head");}
                if(std::any_of(out.centers.begin(),out.centers.end(),[&](const auto& p){return std::hypot(p.x-px,p.y-py)<=3;}))
                    return fail("classic_oriented_hold_head_pivot_ownership_ambiguous");
                if(out.centers.size()>=8)return fail("too_many_classic_oriented_hold_head_centers");
                out.centers.push_back({"classic-oriented-hold-head-"+std::to_string(out.centers.size()),px,py});
                out.metrics.push_back({px,py,static_cast<double>(max_width),count});
            }
            component_found=out.centers.size()!=prior_centers;
        }
        if(component_tube_rejected&&!component_found)return fail("merged_or_bent_classic_oriented_hold_head_component");
    }
    if(samples>4'000'000)return fail("classic_oriented_hold_head_pixel_work_budget_exhausted");
    if(!out.centers.empty()){out.valid=true;out.reason="oriented_classic_hold_head_light_cyan_chevrons_body_junction;completeness_unverified";}
    return out;
}
} // namespace rhythm::experimental
