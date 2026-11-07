#include "../apps/chart_experiment/classic_flick_head.hpp"
#include "../apps/chart_experiment/adaptive_opening_detector.hpp"
#include <iostream>
#include <numbers>
#include <stdexcept>

namespace {
using Pixels=std::vector<std::uint8_t>;
using Color=std::array<std::uint8_t,3>;
constexpr Color red{254,67,101},white{255,255,255},gold{255,235,0},pink{245,80,220},dark{0,0,0};
unsigned checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
Pixels image(){return Pixels(1280*720*3);}
void rectangle(Pixels& p,int x,int y,int w,int h,Color c){
    for(int v=y;v<y+h;++v)for(int u=x;u<x+w;++u)if(u>=0&&u<1280&&v>=0&&v<720)
        for(std::size_t k=0;k<3;++k)p[(static_cast<std::size_t>(v)*1280+u)*3+k]=c[k];
}
// Synthetic structure derived from the normal/HL asset proportions, not a
// copied game image: two red slabs, white borders/logo/outer chevrons and gaps.
void flick(Pixels& p,double cx,double cy,double degrees=0,double scale=1,bool highlighted=false,
           bool left=true,bool right=true,bool outline=true,bool logo=true,Color core=red){
    const auto a=degrees*std::numbers::pi/180,c=std::cos(a),s=std::sin(a);
    const auto extent=75*scale;
    for(int y=std::max(0,static_cast<int>(std::floor(cy-extent)));y<=std::min(719,static_cast<int>(std::ceil(cy+extent)));++y)
        for(int x=std::max(0,static_cast<int>(std::floor(cx-extent)));x<=std::min(1279,static_cast<int>(std::ceil(cx+extent)));++x){
            const auto u=((x-cx)*c+(y-cy)*s)/scale,v=(-(x-cx)*s+(y-cy)*c)/scale;
            Color color=dark;bool painted=false;
            if(highlighted&&std::abs(u)<=65&&std::abs(v)<=14){color=gold;painted=true;}
            const auto side=[&](bool enabled,double center){
                if(!enabled)return;
                if(outline&&std::abs(u-center)<=22&&std::abs(v)<=6){color=white;painted=true;}
                if(std::abs(u-center)<=19&&std::abs(v)<=3){color=core;painted=true;}
            };
            side(left,-31);side(right,31);
            if(outline&&std::abs(std::abs(u)-59)<=2&&std::abs(v)<=2){color=white;painted=true;}
            if(logo&&(std::abs(u)<=1.5&&std::abs(v)<=12)) {color=white;painted=true;}
            if(logo&&std::abs(u)<=9&&std::abs(v-.5*u)<=1.3){color=white;painted=true;}
            if(painted)for(std::size_t k=0;k<3;++k)p[(static_cast<std::size_t>(y)*1280+x)*3+k]=color[k];
        }
}
rhythm::calibration::OpeningDetection detect(const Pixels& p){
    return rhythm::experimental::detect_classic_flick_head_centers(1280,720,3840,p);
}
rhythm::experimental::AdaptiveOpeningDescription description(bool enabled){
    rhythm::experimental::AdaptiveOpeningDescription d;d.note_kind=rhythm::NoteKind::flick;d.includes_classic_flick_heads=enabled;
    d.detector_policy_id=enabled?"typed-rle-oriented-classic-red-flick-head-v3":"typed-rle-oriented-head-v1";return d;
}
}
int main(){try{
    for(const auto highlighted:{false,true})for(const auto degrees:{0.,32.,-47.,90.,180.})for(const auto scale:{.85,1.,1.4}){
        auto p=image();flick(p,640,300,degrees,scale,highlighted);const auto d=detect(p);
        check(d.valid&&d.centers.size()==1,"normal/HL oriented classic red Flick missing");
        check(std::hypot(d.centers[0].x-640,d.centers[0].y-300)<=3,"classic Flick source midpoint shifted by more than3px");
        check(d.centers[0].id=="classic-red-flick-head-0"&&!d.centers_complete_unoccluded,
            "classic Flick helper inferred coverage or changed center ownership");
    }
    {auto p=image();flick(p,640,300,0,1,false,true,false);
        check(!detect(p).valid,"single red slab became a whole Flick head");
        p=image();flick(p,640,300,0,1,false,false,true);
        check(!detect(p).valid,"missing left slab was ignored");
        p=image();flick(p,640,300,0,1,false,true,true,false,false);
        check(!detect(p).valid,"plain paired red bars without white evidence became a Flick");
        p=image();flick(p,640,300,0,1,false,true,true,true,false);
        check(!detect(p).valid,"missing central white logo was accepted");
        p=image();flick(p,640,300);rectangle(p,576,295,12,11,dark);
        check(!detect(p).valid,"missing outer white chevron was accepted");
        p=image();flick(p,640,300);rectangle(p,584,298,4,5,white);
        check(!detect(p).valid,"missing negative gap was accepted");
    }
    {for(const auto color:{pink,Color{156,233,255},white,Color{254,135,175}}){
            auto p=image();flick(p,640,300,0,1,false,true,true,true,true,color);
            check(!detect(p).valid,"non-red core color claimed a classic red Flick");
        }
        auto p=image();rectangle(p,590,297,101,7,red);
        check(!detect(p).valid,"merged continuous red slab became a Flick pair");
        p=image();flick(p,35,300);check(!detect(p).valid,"frame-clipped classic Flick accepted");
        p=image();flick(p,640,35);check(!detect(p).valid,"ROI-clipped full Flick card accepted");
        p=image();flick(p,640,300,0,1.5);const auto oversized=detect(p);
        // Red slabs/span still fit their own limits, while the complete white
        // card half diagonal exceeds the solver's90px visibility contract.
        check(!oversized.valid&&oversized.centers.empty()&&oversized.reason=="oversized_classic_flick_head_envelope",
            "oversized full card bypassed the solver visibility-footprint bound");
    }
    {auto p=image();flick(p,250,220);flick(p,950,390,180,1,true);const auto d=detect(p);
        check(d.valid&&d.centers.size()==2,"independent normal/HL Flick pair omitted");
        check(d.centers[0].id!=d.centers[1].id,"independent Flick ownership IDs collide");
        p=image();flick(p,609,300);flick(p,671,300);const auto ambiguous=detect(p);
        check(!ambiguous.valid&&ambiguous.centers.empty(),"three slabs with competing pair ownership were greedily selected");
    }
    {auto p=image();for(int i=0;i<9;++i)flick(p,180+(i%3)*400,130+(i/3)*160);
        const auto many=detect(p);check(!many.valid&&many.centers.empty(),"Flick center budget silently truncated sources");
        p=image();for(int y=30;y<570;y+=2)for(int x=0;x<1280;x+=2)rectangle(p,x,y,1,1,red);
        const auto exhausted=detect(p);check(!exhausted.valid&&exhausted.centers.empty(),"red run budget did not fail closed");
        check(!rhythm::experimental::detect_classic_flick_head_centers(1279,720,3840,p).valid,"wrong geometry accepted");
        check(!rhythm::experimental::detect_classic_flick_head_centers(1280,720,3839,p).valid,"undersized stride accepted");
        check(!rhythm::experimental::detect_classic_flick_head_centers(1280,720,3840,std::span<const std::uint8_t>(p).first(10)).valid,
            "truncated RGB buffer accepted");
    }
    {auto p=image();flick(p,350,300);const auto legacy=rhythm::experimental::detect_adaptive_opening_centers(1280,720,3840,p,rhythm::NoteKind::flick);
        const auto disabled=rhythm::experimental::detect_model_opening_centers(1280,720,3840,p,description(false));
        check(disabled.valid==legacy.valid&&disabled.reason==legacy.reason&&disabled.centers.size()==legacy.centers.size(),
            "disabled classic Flick policy changed legacy semantics");
        const auto enabled=rhythm::experimental::detect_model_opening_centers(1280,720,3840,p,description(true));
        check(enabled.valid&&enabled.centers.size()==1&&!enabled.centers_complete_unoccluded,"adapter did not expose the enabled classic Flick head");
        rectangle(p,900,298,80,4,pink);
        const auto combined=rhythm::experimental::detect_model_opening_centers(1280,720,3840,p,description(true));
        check(combined.valid&&combined.centers.size()==2&&combined.centers[0].id!=combined.centers[1].id,
            "adapter discarded legacy pink or classic red source ownership");
        const auto only_old=rhythm::experimental::detect_model_opening_centers(1280,720,3840,p,description(false));
        check(only_old.valid&&only_old.centers.size()==1&&only_old.centers[0].id=="pink-0","disabled policy admitted new red sources");
        p=image();flick(p,900,300);rectangle(p,0,100,80,4,pink);
        const auto clipped=rhythm::experimental::detect_model_opening_centers(1280,720,3840,p,description(true));
        check(!clipped.valid&&clipped.reason=="clipped_horizontal_head","classic Flick helper repaired an invalid clipped legacy source");
        p=image();for(int i=0;i<5;++i)rectangle(p,100+i*220,100,80,4,pink);for(int i=0;i<4;++i)flick(p,180+i*300,400);
        const auto overflow=rhythm::experimental::detect_model_opening_centers(1280,720,3840,p,description(true));
        check(!overflow.valid&&overflow.centers.empty()&&overflow.reason=="combined_typed_head_center_budget",
            "combined pink/red budget silently removed sources");
    }
    std::cout<<checks<<" classic red Flick checks passed; synthetic structure only, no gameplay or capture qualification.\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
