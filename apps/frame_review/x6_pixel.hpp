#pragma once
#include "pas/core.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

// Offline current-pixel microscope. No tracking, line selection or touch API.
namespace pas::x6 {
struct Point { double x=0,y=0; };
struct Region {Point center,tangent;double length=0,thickness=0;int color=0;};
inline int classify(const std::uint8_t* p){
    const int r=p[0],g=p[1],b=p[2];
    if(b>165&&g>115&&b>r+35)return 1;
    if(r>180&&g>160&&b<g-55)return 2;
    if(r>190&&r>g+45&&b>75)return 3;
    if(r>195&&g>195&&b>195&&std::max({r,g,b})-std::min({r,g,b})<35)return 4;
    return 0;
}
struct Measurement {
    bool supported=false,clipped=false;
    int bins=0,sampled=0,target_votes=0,foreign_votes=0,segments=0;
    int supported_bins=0,gap_bins=0,max_gap=0,foreign_gap_bins=0;
    double span=0,coverage=0,midpoint_offset=0,centroid_offset=0;
    double normal_centroid=0,lateral_radius=0;
    Point tangent;
};
inline Measurement measure(const Frame& frame,Region r){
    if(frame.width<=0||frame.height<=0||frame.width>4096||frame.height>4096||frame.stride<frame.width*3||
       frame.rgb.size()!=static_cast<std::size_t>(frame.stride)*frame.height)throw std::runtime_error("x6_frame_geometry");
    if(!std::isfinite(r.center.x)||!std::isfinite(r.center.y)||!std::isfinite(r.tangent.x)||!std::isfinite(r.tangent.y)||
       std::abs(std::hypot(r.tangent.x,r.tangent.y)-1)>.01||!std::isfinite(r.length)||!std::isfinite(r.thickness)||
       r.length<=0||r.length>464||r.thickness<=0||r.thickness>96||r.color<1||r.color>3)throw std::runtime_error("x6_roi_geometry");
    if(r.tangent.x< -1e-10||(std::abs(r.tangent.x)<=1e-10&&r.tangent.y<0)){r.tangent.x=-r.tangent.x;r.tangent.y=-r.tangent.y;}
    const int radius=static_cast<int>(std::ceil(r.length*.5))+24;
    const int lateral=std::clamp(static_cast<int>(std::ceil(r.thickness*.5)),3,12);
    std::array<int,513> target{},foreign{};Measurement m;m.bins=radius*2+1;m.tangent=r.tangent;m.lateral_radius=lateral;
    double sum_along=0,sum_normal=0;
    for(int i=0;i<m.bins;++i){const int along=i-radius;
        for(int n=-lateral;n<=lateral;++n){const double px=r.center.x+r.tangent.x*along-r.tangent.y*n,py=r.center.y+r.tangent.y*along+r.tangent.x*n;
            if(px<-.5||py<-.5||px>=frame.width-.5||py>=frame.height-.5){m.clipped=true;continue;}
            const int x=static_cast<int>(std::lround(px)),y=static_cast<int>(std::lround(py));
            const auto* pixel=frame.rgb.data()+static_cast<std::size_t>(y)*frame.stride+x*3;++m.sampled;
            const auto c=classify(pixel);if(c==r.color){++target[i];++m.target_votes;sum_along+=along;sum_normal+=n;}
            else if(c>=1&&c<=3){++foreign[i];++m.foreign_votes;}
        }
    }
    int first=-1,last=-1;
    // Two target-color votes per longitudinal bin suppress isolated pixels.
    for(int i=0;i<m.bins;++i)if(target[i]>=2){if(first<0)first=i;last=i;++m.supported_bins;}
    if(first<0)return m;
    m.supported=true;m.span=last-first+1;m.coverage=m.supported_bins/m.span;
    m.midpoint_offset=(first+last)*.5-radius;m.centroid_offset=sum_along/m.target_votes;m.normal_centroid=sum_normal/m.target_votes;
    int gap=0;bool was_supported=false;
    for(int i=first;i<=last;++i){const bool present=target[i]>=2;
        if(present){if(!was_supported)++m.segments;m.max_gap=std::max(m.max_gap,gap);gap=0;}
        else {++gap;++m.gap_bins;if(foreign[i]>=2)++m.foreign_gap_bins;}
        was_supported=present;
    }
    return m;
}
enum class Reliability {unknown,current_core_supported,foreign_overlap_fragmentation};
// PCA axes are unoriented: canonical signs can flip while crossing vertical.
// Compare both offsets in the current frame's axis, never subtract signed scalars.
inline double midpoint_offset_change(const Measurement& current,const Measurement& prior){
    return current.midpoint_offset-prior.midpoint_offset*
        (current.tangent.x*prior.tangent.x+current.tangent.y*prior.tangent.y);
}
inline Reliability classify_measurement(const Measurement& m){
    if(!m.supported||m.clipped)return Reliability::unknown;
    if(m.segments>1&&m.max_gap>=3&&m.foreign_gap_bins>=2)return Reliability::foreign_overlap_fragmentation;
    return Reliability::current_core_supported; // Not a guarantee of physical center/identity or motion reliability.
}
}
