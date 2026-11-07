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
// Normal Level Flick2/Flick2HL candidate, not a generic red-bar detector.
// The two red slabs, white outline/logo/outer chevrons and dark-or-gold gaps
// are independently required. Every accepted center is the pair's source
// midpoint, not either slab centroid. Coverage/native identity are unproven.
inline calibration::OpeningDetection detect_classic_flick_head_centers(
        int width,int height,int stride,std::span<const std::uint8_t> rgb) {
    calibration::OpeningDetection out;
    if(width!=1280||height!=720||stride<3840||stride>16384||rgb.size()!=static_cast<std::size_t>(stride)*720)return out;
    out.reason="no_supported_classic_flick_head";
    const auto pixel=[&](int x,int y){const auto p=static_cast<std::size_t>(y)*static_cast<std::size_t>(stride)+static_cast<std::size_t>(x)*3;
        return std::array<int,3>{rgb[p],rgb[p+1],rgb[p+2]};};
    const auto red=[&](int x,int y){const auto p=pixel(x,y);
        return p[0]>=190&&p[1]>=20&&p[1]<=115&&p[2]>=45&&p[2]<=145&&p[0]-p[1]>=100&&p[0]-p[2]>=80;};
    const auto white=[&](int x,int y){const auto p=pixel(x,y);return std::min({p[0],p[1],p[2]})>=205&&std::max({p[0],p[1],p[2]})-std::min({p[0],p[1],p[2]})<=35;};
    const auto gap=[&](int x,int y){const auto p=pixel(x,y);
        return std::max({p[0],p[1],p[2]})<=145||(p[0]>=160&&p[1]>=140&&p[2]<=150&&std::min(p[0],p[1])-p[2]>=50);};
    const auto fail=[&](const char* reason){out.valid=false;out.reason=reason;out.centers.clear();out.metrics.clear();return out;};
    struct Run{int y=0,a=0,b=0;std::size_t parent=0;};
    std::vector<Run> runs;runs.reserve(2048);
    const auto root=[&](std::size_t i){auto r=i;while(runs[r].parent!=r)r=runs[r].parent;
        while(runs[i].parent!=i){const auto next=runs[i].parent;runs[i].parent=r;i=next;}return r;};
    std::size_t previous_begin=0,previous_end=0;
    for(int y=30;y<570;++y){const auto current_begin=runs.size();std::size_t previous=previous_begin;int x=0;
        while(x<1280){while(x<1280&&!red(x,y))++x;if(x==1280)break;
            const auto begin=x;while(x<1280&&red(x,y))++x;
            if(runs.size()>=16384)return fail("classic_flick_head_run_budget_exhausted");
            const auto index=runs.size();runs.push_back({y,begin,x-1,index});
            while(previous<previous_end&&runs[previous].b+1<begin)++previous;
            for(auto p=previous;p<previous_end&&runs[p].a<=x;++p){const auto a=root(index),b=root(p);if(a!=b)runs[std::max(a,b)].parent=std::min(a,b);}}
        previous_begin=current_begin;previous_end=runs.size();}
    struct Component{double count=0,sx=0,sy=0,sxx=0,syy=0,sxy=0,ux=1,uy=0,umin=1e20,umax=-1e20,vmin=1e20,vmax=-1e20;
        int xmin=1280,xmax=-1,ymin=720,ymax=-1;};
    std::vector<Component> components(runs.size());std::size_t count=0;
    const auto squares=[](double n){return n*(n+1)*(2*n+1)/6;};
    for(std::size_t i=0;i<runs.size();++i){const auto& r=runs[i];auto& c=components[root(i)];
        if(c.count==0&&++count>4096)return fail("classic_flick_head_component_budget_exhausted");
        const double n=r.b-r.a+1,sum=(r.a+r.b)*n/2;c.count+=n;c.sx+=sum;c.sy+=n*r.y;
        c.sxx+=squares(r.b)-squares(r.a-1);c.syy+=n*r.y*r.y;c.sxy+=sum*r.y;
        c.xmin=std::min(c.xmin,r.a);c.xmax=std::max(c.xmax,r.b);c.ymin=std::min(c.ymin,r.y);c.ymax=std::max(c.ymax,r.y);}
    out.groups=count;
    for(auto& c:components)if(c.count>=60){const auto x=c.sx/c.count,y=c.sy/c.count;
        const auto a=.5*std::atan2(2*(c.sxy/c.count-x*y),(c.sxx-c.syy)/c.count-x*x+y*y);c.ux=std::cos(a);c.uy=std::sin(a);}
    for(std::size_t i=0;i<runs.size();++i){const auto& r=runs[i];auto& c=components[root(i)];if(c.count<60)continue;
        for(const auto x:{r.a,r.b}){const auto u=c.ux*x+c.uy*r.y,v=-c.uy*x+c.ux*r.y;
            c.umin=std::min(c.umin,u);c.umax=std::max(c.umax,u);c.vmin=std::min(c.vmin,v);c.vmax=std::max(c.vmax,v);}}
    struct Slab{double x,y,ux,uy,major,minor;};std::vector<Slab> slabs;slabs.reserve(32);
    for(const auto& c:components)if(c.count>=60){const auto extent=std::abs(c.ux)+std::abs(c.uy);
        const auto major=c.umax-c.umin+extent,minor=c.vmax-c.vmin+extent;
        if(minor<2||minor>12||major<24||major<4*minor)continue;
        if(major>75){if(major<=180)return fail("merged_classic_flick_head_core");continue;}
        if(c.count/(major*minor)<.68)return fail("nonrectangular_classic_flick_head_core");
        if(c.xmin==0||c.xmax==1279||c.ymin==30||c.ymax==569)return fail("clipped_classic_flick_head");
        if(slabs.size()>=128)return fail("classic_flick_head_slab_budget_exhausted");
        const auto u=(c.umin+c.umax)/2,v=(c.vmin+c.vmax)/2;
        slabs.push_back({c.ux*u-c.uy*v,c.uy*u+c.ux*v,c.ux,c.uy,major,minor});}
    struct Pair{std::size_t a,b;double x,y,span,minor;};std::vector<Pair> pairs;
    std::vector<unsigned> degree(slabs.size(),0);std::size_t work=0;
    for(std::size_t i=0;i<slabs.size();++i)for(std::size_t j=i+1;j<slabs.size();++j){
        if(++work>8192)return fail("classic_flick_head_pair_budget_exhausted");
        const auto& a=slabs[i];const auto& b=slabs[j];const auto dot=a.ux*b.ux+a.uy*b.uy;
        if(std::abs(dot)<.996)continue;
        auto ux=a.ux+(dot<0?-b.ux:b.ux),uy=a.uy+(dot<0?-b.uy:b.uy);const auto norm=std::hypot(ux,uy);ux/=norm;uy/=norm;
        const auto dx=b.x-a.x,dy=b.y-a.y,along=std::abs(dx*ux+dy*uy),across=std::abs(-dx*uy+dy*ux);
        const auto length=(a.major+b.major)/2,minor=(a.minor+b.minor)/2;
        const auto middle_gap=along-length,span=along+length;
        if(across>1.5||std::abs(a.major-b.major)>std::max(3.,.12*length)||std::abs(a.minor-b.minor)>2||
           middle_gap<.45*length||middle_gap>.85*length||span<70||span>180)continue;
        const auto cx=(a.x+b.x)/2,cy=(a.y+b.y)/2;
        // Check the entire asset-derived card envelope, including the tall
        // central logo and outer chevrons, before sampling its white evidence.
        const auto half_u=span/2+.25*length,half_v=2.35*minor;
        // The solver's guaranteed-full-head exclusion uses a <91px half
        // diagonal plus its separate 3px error and one-pixel boundary guard.
        if(std::hypot(half_u,half_v)>90)return fail("oversized_classic_flick_head_envelope");
        const auto ex=std::abs(ux)*half_u+std::abs(uy)*half_v,ey=std::abs(uy)*half_u+std::abs(ux)*half_v;
        if(cx-ex<=0||cx+ex>=1279||cy-ey<=30||cy+ey>=569)return fail("clipped_classic_flick_head");
        const auto sample=[&](double u,double v,const auto& predicate){
            const auto x=static_cast<int>(std::lround(cx+u*ux-v*uy)),y=static_cast<int>(std::lround(cy+u*uy+v*ux));
            // At most one raster pixel accounts for rotation/antialiasing.
            for(int yy=y-1;yy<=y+1;++yy)for(int xx=x-1;xx<=x+1;++xx)if(predicate(xx,yy))return true;
            return false;};
        bool structure=true;
        for(const auto* slab:{&a,&b}){const auto u=(slab->x-cx)*ux+(slab->y-cy)*uy;
            for(const double du:{-.18*slab->major,.18*slab->major})for(const double sign:{-1.,1.})
                structure=structure&&sample(u+du,sign*(slab->minor/2+1),white);}
        for(const double v:{-1.25*minor,0.,1.25*minor})structure=structure&&sample(0,v,white);
        for(const double sign:{-1.,1.}){
            structure=structure&&sample(sign*(span/2+.21*length),0,white);
            structure=structure&&sample(sign*(span/2+.13*length),0,gap);}
        if(!structure)continue;
        if(++degree[i]>1||++degree[j]>1)return fail("classic_flick_head_pair_ownership_ambiguous");
        pairs.push_back({i,j,cx,cy,span,minor});}
    for(const auto& p:pairs){
        if(out.centers.size()>=8)return fail("too_many_classic_flick_head_centers");
        out.centers.push_back({"classic-red-flick-head-"+std::to_string(out.centers.size()),p.x,p.y});
        out.metrics.push_back({p.x,p.y,p.span,static_cast<std::size_t>(std::ceil(p.minor))});}
    out.qualifying_runs=slabs.size();
    if(!out.centers.empty()){out.valid=true;out.reason="oriented_classic_red_flick_two_slabs_white_logo_chevrons;completeness_unverified";}
    return out;
}
} // namespace rhythm::experimental
