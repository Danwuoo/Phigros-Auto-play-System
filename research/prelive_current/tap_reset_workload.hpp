#pragma once
#include "cost_workload.hpp"
namespace pas::current_rails::tap_reset_workload {
inline constexpr std::string_view scene="tap-closed-reset-v1";
inline constexpr std::string_view generator="tap_closed_reset_v1_streamed_all_offsets";
inline constexpr std::size_t bytes=1280u*720u*3u;
inline constexpr int frames_per_cycle=32;
inline constexpr std::array<int,3> offsets{-2,0,2};
// Explicit pixel absence closes identity before a new approach. At 8ms even
// the 16-frame prefix alone exceeds the existing 90ms track lifetime. Nothing
// from Runtime/owner/receipts reaches this deterministic offline generator.
inline void render_phase(std::vector<std::uint8_t>& p,int phase,int offset,bool absent=false){
 if(phase<0||phase>=frames_per_cycle||(offset!=-2&&offset!=0&&offset!=2))throw std::invalid_argument("tap reset declared phase/offset");
 p.assign(bytes,30);using cost_workload::box;
 box(p,20,20,26,42,{255,255,255});box(p,34,20,40,42,{255,255,255});
 for(int d=0;d<6;++d)box(p,1020+d*24,20,1032+d*24,40,{255,255,255});
 box(p,115,499,1165,502,{255,255,255});
 if(absent||phase<16||phase>=28)return;
 const int y=440+(phase-16)*8+offset;
 box(p,491,y-4,569,y+5,{40,190,255});
}
inline void render(std::vector<std::uint8_t>& p,int attempt){
 if(attempt<1||attempt>10000)throw std::invalid_argument("tap reset attempt bound");
 render_phase(p,(attempt-1)%frames_per_cycle,offsets[std::size_t((attempt-1)/frames_per_cycle)%offsets.size()]);
}
}
