#pragma once
#include "cost_workload_family_v2.hpp"
#include <limits>
namespace pas::current_rails::streaming_workload {
inline constexpr std::string_view scene="hold-closed-reset-v2";
inline constexpr std::size_t bytes=1280u*720u*3u;
// New experiment only: fixed v2 RGB, not a policy input. No target/plan/receipt
// reaches the generator, and scene/cycle metadata never reaches Runtime.
inline void render(std::vector<std::uint8_t>& p,int attempt){
 if(attempt<1||attempt>10000)throw std::invalid_argument("streaming workload attempt bound");
 constexpr std::array<int,3> offsets{-2,0,2};
 const int phase=(attempt-1)%cost_family_v2::frames_per_cycle;
 const int offset=offsets[std::size_t((attempt-1)/cost_family_v2::frames_per_cycle)%offsets.size()];
 p.assign(bytes,0);using cost_workload::box;
 box(p,20,20,26,42,{255,255,255});box(p,34,20,40,42,{255,255,255});
 for(int d=0;d<6;++d)box(p,1020+d*24,20,1032+d*24,40,{255,255,255});
 box(p,115,499,1165,502,{255,255,255});
 int y=cost_family_v2::head(phase);if(y<0)return;y+=offset;
 box(p,350,y-160,451,y+1,{40,190,255});
 box(p,350,y-160,352,y+1,{255,255,255});box(p,449,y-160,451,y+1,{255,255,255});
 box(p,350,y-160,451,y-158,{255,255,255});
}
}
