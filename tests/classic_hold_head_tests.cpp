#include "../apps/chart_experiment/classic_hold_head.hpp"
#include "../apps/chart_experiment/classic_oriented_hold_head.hpp"
#include "../apps/chart_experiment/adaptive_opening_detector.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace {
using Pixels=std::vector<std::uint8_t>;
using Color=std::array<std::uint8_t,3>;
constexpr Color cyan{156,233,255},white{255,255,255},gold{255,235,0},dark{0,0,0};
unsigned checks=0;
void check(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
Pixels image(){return Pixels(1280*720*3);}
void rectangle(Pixels& p,int x,int y,int w,int h,Color c){for(int v=y;v<y+h;++v)for(int u=x;u<x+w;++u)if(u>=0&&u<1280&&v>=0&&v<720)for(int k=0;k<3;++k)p[(v*1280+u)*3+k]=c[static_cast<std::size_t>(k)];}
void head(Pixels& p,int x,int junction,int width=90,int rows=5,bool highlighted=false,bool below=false,bool body=true) {
    const auto top=below?junction-rows:junction;
    if(body)rectangle(p,x-width/2-6,below?junction:junction-100,width+12,100,cyan);
    if(highlighted)rectangle(p,x-width/2-10,below?top-4:top,width+20,rows+4,gold);
    for(int row=0;row<rows;++row) {
        const auto inset=(below?rows-1-row:row)/2;
        rectangle(p,x-width/2+inset,top+row,width-2*inset,1,cyan);
        rectangle(p,x-width/2+inset-5,top+row,3,1,white);
        rectangle(p,x+width/2-inset+2,top+row,3,1,white);
    }
}
rhythm::calibration::OpeningDetection detect(const Pixels& p){return rhythm::experimental::detect_classic_hold_head_centers(1280,720,3840,p);}
rhythm::calibration::OpeningDetection oriented(const Pixels& p){return rhythm::experimental::detect_classic_oriented_hold_head_centers(1280,720,3840,p);}
Pixels rotated(const Pixels& source,double degrees,Color background=dark) {
    auto p=image();const auto rad=degrees*3.14159265358979323846/180.,c=std::cos(rad),s=std::sin(rad);
    for(int y=0;y<720;++y)for(int x=0;x<1280;++x){
        const auto dx=x-640.,dy=y-300.;const auto u=static_cast<int>(std::lround(640+c*dx-s*dy));
        const auto v=static_cast<int>(std::lround(300+s*dx+c*dy));
        auto color=background;
        if(u>=0&&u<1280&&v>=0&&v<720){const auto at=(v*1280+u)*3;
            const Color raw{source[at],source[at+1],source[at+2]};if(raw!=dark)color=raw;}
        for(std::size_t k=0;k<3;++k)p[(y*1280+x)*3+static_cast<int>(k)]=color[k];
    }return p;
}
void overlay(Pixels& destination,const Pixels& source,int shift_x,int shift_y){
    for(int y=0;y<720;++y)for(int x=0;x<1280;++x){const auto tx=x+shift_x,ty=y+shift_y;
        if(tx<0||tx>=1280||ty<0||ty>=720)continue;const auto at=(y*1280+x)*3;
        if(source[at]==0&&source[at+1]==0&&source[at+2]==0)continue;
        for(int k=0;k<3;++k)destination[(ty*1280+tx)*3+k]=source[at+k];
    }
}
void source_asset(const std::string& path,double x,double y){
    std::ifstream input(path,std::ios::binary);check(static_cast<bool>(input),"asset fixture unavailable");
    input.seekg(0,std::ios::end);check(input.tellg()==1280*720*3,"asset fixture has wrong bounded RGB byte count");input.seekg(0);
    Pixels pixels((std::istreambuf_iterator<char>(input)),{});
    const auto d=oriented(pixels);check(d.valid&&d.centers.size()==1,"asset-derived complete oriented head rejected");
    const auto error=std::hypot(d.centers[0].x-x,d.centers[0].y-y);
    check(std::isfinite(error)&&error<3,"asset-derived sprite body-junction pivot error exceeds3px");
    std::cout<<"asset fixture PASS pivot_error="<<error<<"; no gameplay qualification\n";
}
void unsupported_paint(const rhythm::calibration::OpeningDetection& d,double x,double y){
    check(d.valid?(d.centers.size()==1&&std::hypot(d.centers[0].x-x,d.centers[0].y-y)<3):d.centers.empty(),
        "unsupported raster paint was admitted with a false body-centroid pivot or partial invalid output");
}
}
int main(int argc,char** argv){try{
    // Optional authorized source-asset fixture, never a device/game capture.
    // Keep the portable CTest independent of private copyrighted asset files.
    if(argc==5&&std::string(argv[1])=="--asset"){
        source_asset(argv[2],std::stod(argv[3]),std::stod(argv[4]));return 0;
    }
    if(argc==3&&std::string(argv[1])=="--asset-list"){
        std::ifstream input(argv[2]);check(static_cast<bool>(input),"asset list unavailable");
        input.seekg(0,std::ios::end);check(input.tellg()>0&&input.tellg()<=65536,"asset list exceeds64KiB");input.seekg(0);
        std::string line;unsigned count=0;
        while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;
            check(++count<=160,"asset list exceeds160fixtures");std::istringstream row(line);std::string path,x,y,extra;
            check(static_cast<bool>(std::getline(row,path,'\t'))&&static_cast<bool>(std::getline(row,x,'\t'))&&
                  static_cast<bool>(std::getline(row,y,'\t'))&&!std::getline(row,extra,'\t'),"asset list requires RGBpath TAB pivotX TAB pivotY");
            source_asset(path,std::stod(x),std::stod(y));}
        check(count>0,"asset list is empty");std::cout<<count<<" source-derived fixture contracts passed\n";return 0;
    }
    check(argc==1,"unsupported test arguments");
    for(const auto highlighted:{false,true})for(const auto below:{false,true})for(const auto width:{76,100,160}) {
        auto p=image();head(p,640,300,width,static_cast<int>(std::lround(width/17.)),highlighted,below);const auto d=detect(p);
        check(d.valid&&d.centers.size()==1,"normal/HL/side/scale head not found");
        check(std::hypot(d.centers[0].x-639.5,d.centers[0].y-300)<1,"body-junction pivot error exceeds1px synthetic fixture");
        check(!d.centers_complete_unoccluded,"helper inferred image completeness");
        check(d.metrics[0].rows>=2&&d.metrics[0].rows<=18,"connected body extended head band");
    }
    {auto p=image();rectangle(p,600,100,90,204,cyan);check(!detect(p).valid,"plain wide body became head centroid");
        p=image();rectangle(p,600,100,90,5,white);check(!detect(p).valid,"white tail became Hold head");
        p=image();head(p,640,300,90,5,false,false,false);check(!detect(p).valid,"isolated light-cyan Tap/noise claimed Hold body");
        p=image();head(p,640,300);rectangle(p,589,305,102,100,cyan);check(!detect(p).valid,"two-sided merged body selected a head");}
    {auto p=image();head(p,640,300);rectangle(p,589,300,8,5,dark);check(!detect(p).valid,"missing white chevron accepted");
        p=image();head(p,640,300);rectangle(p,590,300,100,5,cyan);check(!detect(p).valid,"missing negative gaps accepted");
        p=image();head(p,640,300,90,30);check(!detect(p).valid,"thick striped body became head");
        p=image();head(p,640,29);check(!detect(p).valid,"ROI-clipped head accepted");}
    {auto p=image();head(p,250,280);head(p,900,350,100,6,true,true);const auto d=detect(p);
        check(d.valid&&d.centers.size()==2,"multiple independent heads omitted");
        check(d.centers[0].id!=d.centers[1].id,"Hold head IDs collide");}
    {auto p=image();for(int i=0;i<9;++i)head(p,100+(i%3)*400,130+(i/3)*150,76,5);
        const auto d=detect(p);check(!d.valid&&d.centers.empty(),"head overflow silently truncated");}
    {auto p=image();rectangle(p,600,300,80,4,{0,230,255});check(!detect(p).valid,"legacy dark-cyan Tap predicate changed");
        const auto old=rhythm::calibration::detect_opening_centers(1280,720,3840,p);
        check(old.valid&&old.centers.size()==1,"legacy Tap detection altered");}
    {auto p=image();head(p,900,300);rectangle(p,200,300,80,4,{0,230,255});
        rhythm::experimental::AdaptiveOpeningDescription description;description.note_kind=rhythm::NoteKind::hold;description.includes_hold_heads=true;
        const auto both=rhythm::experimental::detect_model_opening_centers(1280,720,3840,p,description);
        check(both.valid&&both.centers.size()==2,"adapter dropped deep-cyan Tap or light classic Hold");
        check(both.centers[0].id!=both.centers[1].id&&!both.centers_complete_unoccluded,"adapter ownership/completeness changed");
        description.includes_hold_heads=false;
        const auto prior=rhythm::experimental::detect_model_opening_centers(1280,720,3840,p,description);
        check(prior.valid&&prior.centers.size()==1&&prior.centers[0].id=="cyan-0","hasHoldfalse changed legacy candidate set");
        p=image();head(p,900,300);rectangle(p,0,100,80,4,{0,230,255});description.includes_hold_heads=true;
        const auto clipped=rhythm::experimental::detect_model_opening_centers(1280,720,3840,p,description);
        check(!clipped.valid&&clipped.reason=="clipped_horizontal_head","classic helper repaired old clipped-head failure");
        p=image();head(p,900,300);rectangle(p,200,90,80,4,{0,230,255});rectangle(p,200,130,80,4,{0,230,255});rectangle(p,238,94,4,36,{0,230,255});
        const auto merged=rhythm::experimental::detect_model_opening_centers(1280,720,3840,p,description);
        check(!merged.valid&&merged.reason=="merged_horizontal_head_component","classic helper repaired old merged-head ambiguity");}
    {auto p=image();head(p,640,300);rectangle(p,589,0,102,300,cyan);
        const auto d=detect(p);check(d.valid&&d.centers.size()==1&&std::abs(d.centers[0].y-300)<=3,"ROI-clipped long body hid a complete head");}
    {auto p=image();head(p,640,33);check(detect(p).valid,"body support outside search ROI was treated as head clipping");}
    for(const auto highlighted:{false,true})for(const auto below:{false,true})for(const auto angle:{0.,15.,-15.,32.,60.,-60.,90.,180.}){
        auto p=image();head(p,640,300,100,6,highlighted,below);p=rotated(p,angle);
        const auto d=oriented(p);
        std::cout<<"authored_material width=100 rows=6 angle="<<angle<<" highlighted="<<highlighted
            <<" below="<<below<<" valid="<<d.valid<<" reason="<<d.reason<<" centers="<<d.centers.size()
            <<" groups="<<d.groups<<" qualified_rows="<<d.qualifying_runs<<'\n';
        // The old authored horizontal paint has an opaque rectangular body
        // without the real material's rails/alpha. Nearest rotation does not
        // establish a normal-Level asset contract. Retain the actual failing
        //15deg paint as fail-closed/no-false-pivot coverage; source-derived
        //rotated positives are required separately through --asset-list.
        if(angle==0||angle==90||angle==180)check(d.valid&&d.centers.size()==1,"cardinal authored head unexpectedly rejected");
        unsupported_paint(d,640,300);
        check(!d.centers_complete_unoccluded,"oriented Hold inferred coverage");
    }
    for(const auto color:{Color{32,32,32},Color{64,64,64},Color{0,40,20},Color{35,45,8}}){
        auto p=image();head(p,640,300);p=rotated(p,32,color);const auto d=oriented(p);
        unsupported_paint(d,640,300);
        p=image();head(p,640,300);p=rotated(p,0,color);const auto cardinal=oriented(p);
        check(cardinal.valid&&cardinal.centers.size()==1&&std::hypot(cardinal.centers[0].x-640,cardinal.centers[0].y-300)<3,
            "unchanged dark/gold gap rule rejected a supported cardinal head on dark/green/olive background");}
    for(const double angle:{0.,32.}){auto p=image();head(p,640,300,100,6);p=rotated(p,angle);
        rectangle(p,0,230,1280,2,white);const auto d=oriented(p);
        if(angle==0)check(d.valid&&d.centers.size()==1,"full-width judge line hid a supported cardinal head");
        unsupported_paint(d,640,300);}
    for(const auto angle:{15.,60.,90.}){
        auto p=image();rectangle(p,590,200,100,110,cyan);check(!oriented(rotated(p,angle)).valid,"rotated plain body became pivot");
        p=image();rectangle(p,590,300,100,6,white);check(!oriented(rotated(p,angle)).valid,"rotated white tail became head");
        p=image();head(p,640,300,100,6,false,false,false);check(!oriented(rotated(p,angle)).valid,"rotated head without body accepted");
        p=image();head(p,640,300);rectangle(p,589,300,8,5,dark);check(!oriented(rotated(p,angle)).valid,"rotated missing-chevron head accepted");
        p=image();head(p,640,300);rectangle(p,589,305,102,100,cyan);check(!oriented(rotated(p,angle)).valid,"rotated two-sided body ambiguity accepted");
    }
    {auto a=image();head(a,640,350,100,6);auto raw=image();head(raw,640,300,100,6);const auto b=rotated(raw,90);
        check(oriented(a).valid&&oriented(b).valid,"merged-cap negative did not start from two supported complete heads");
        auto joined=a;overlay(joined,b,0,-90);rectangle(joined,638,210,5,45,cyan);
        const auto merged=oriented(joined);check(!merged.valid&&merged.centers.empty(),"first cap silently hid a different oriented head in one connected body");
        auto separate=image();head(separate,250,350,100,6);overlay(separate,b,260,0);const auto independent=oriented(separate);
        check(independent.valid&&independent.centers.size()==2,"independent different-component rotated heads were merged or omitted");}
    {auto p=image();head(p,640,29);check(!oriented(p).valid,"oriented head clipped at ROI boundary accepted");
        check(!rhythm::experimental::detect_classic_oriented_hold_head_centers(640,720,3840,p).valid,
            "oriented Hold accepted changed source geometry");
        p=image();for(int i=0;i<9;++i)head(p,100+(i%3)*400,130+(i/3)*150,76,5);
        const auto d=oriented(p);check(!d.valid&&d.centers.empty(),"oriented head overflow silently truncated");}
    {auto p=image();for(int y=30;y<570;++y)for(int x=0;x<1280;x+=6)rectangle(p,x,y,1,1,cyan);
        const auto d=oriented(p);check(!d.valid&&d.centers.empty()&&d.reason=="classic_oriented_hold_head_run_budget_exhausted",
            "oriented raw-run budget silently discarded excess input");}
    std::cout<<checks<<" classic Hold head checks passed; synthetic structure only, no gameplay or capture qualification.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}}
