#pragma once
#include "cost_workload.hpp"
#include <stdexcept>
namespace pas::current_rails::cost_family_v2 {
enum class Control { positive, absence, effect, no_tail_cap };
inline std::string_view name(Control c){switch(c){case Control::absence:return "absence";case Control::effect:return "effect";case Control::no_tail_cap:return "no_tail_cap";default:return "positive";}}
constexpr int frames_per_cycle=81;
inline int head(int phase){if(phase<8||phase>=73)return -1;if(phase<16)return 460+(phase-8)*4;if(phase<35)return 489+(phase-16);if(phase<52)return 515+(phase-35)*8;if(phase<71)return 651+(phase-52);return phase==71?675:683;}
inline std::vector<std::uint8_t> image(int phase,int offset,Control c){
 if(phase<0||phase>=frames_per_cycle||(offset!=-2&&offset!=0&&offset!=2))throw std::invalid_argument("declared family bounds");
 using cost_workload::box;std::vector<std::uint8_t> p(1280*720*3,0);
 box(p,20,20,26,42,{255,255,255});box(p,34,20,40,42,{255,255,255});for(int d=0;d<6;++d)box(p,1020+d*24,20,1032+d*24,40,{255,255,255});box(p,115,499,1165,502,{255,255,255});
 int y=head(phase);if(y<0||(phase>=35&&c==Control::absence))return p;y+=offset;
 const auto color=phase>=35&&c==Control::effect?std::array<std::uint8_t,3>{230,190,70}:std::array<std::uint8_t,3>{40,190,255};
 box(p,350,y-160,451,y+1,color);box(p,350,y-160,352,y+1,{255,255,255});box(p,449,y-160,451,y+1,{255,255,255});
 if(c!=Control::no_tail_cap)box(p,350,y-160,451,y-158,{255,255,255});
 return p;
}
}
