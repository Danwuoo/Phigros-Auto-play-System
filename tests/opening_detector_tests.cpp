#include "../apps/chart_calibrate/opening_detector.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

namespace calibration=rhythm::calibration;
namespace {
int checks=0;
void check(bool ok,const char* reason) {++checks;if(!ok)throw std::runtime_error(reason);}
struct Image {
    int stride=1280*3;std::vector<std::uint8_t> bytes;
    explicit Image(int padding=0):stride(1280*3+padding),bytes(static_cast<std::size_t>(stride)*720,0) {}
    void run(int y,int x,int width,std::array<std::uint8_t,3> color={40,180,220}) {
        for(int p=x;p<x+width;++p)for(int c=0;c<3;++c)bytes[static_cast<std::size_t>(y)*stride+static_cast<std::size_t>(p)*3+c]=color[static_cast<std::size_t>(c)];
    }
    calibration::OpeningDetection detect() const {return calibration::detect_opening_centers(1280,720,stride,bytes);}
};
}
int main() {
try {
    Image image;image.run(30,100,100);image.run(31,102,100);
    auto a=image.detect();check(a.valid&&a.centers.size()==1,"two adjacent qualifying rows make one center");
    check(a.centers[0].x==150.5&&a.centers[0].y==30.5,"means use pixel centers and all rows");
    check(a.metrics[0].width==100&&a.metrics[0].rows==2,"width and row provenance retained");
    check(!a.centers_complete_unoccluded,"detector cannot assert completeness");
    for(const auto color:std::array<std::array<std::uint8_t,3>,3>{{{80,180,220},{40,140,220},{40,180,185}}}) {
        Image boundary;boundary.run(30,100,100,color);boundary.run(31,100,100,color);
        check(!boundary.detect().valid,"threshold comparisons are strict");
    }
    Image exact;exact.run(30,100,70,{79,141,186});exact.run(31,100,70,{79,141,186});
    check(exact.detect().valid,"all lower/upper strict thresholds accept nearest valid values");
    for(const int width:{69,181}) {
        Image bad;bad.run(30,100,width);bad.run(31,100,width);check(!bad.detect().valid,"out-of-range runs ignored without invented center");
    }
    Image right;right.run(568,1100,180);right.run(569,1100,180);
    check(right.detect().valid&&right.detect().centers[0].x==1189.5,"ROI last row and right-edge run use correct bounds");
    Image outside;outside.run(29,100,100);outside.run(570,100,100);
    check(!outside.detect().valid,"vertical ROI is half-open");
    Image gap;gap.run(30,100,100);gap.run(32,100,100);
    check(!gap.detect().valid,"missing middle row cannot join groups");
    Image drift;drift.run(30,100,100);drift.run(31,103,100);
    check(!drift.detect().valid,"more than two-pixel adjacent center drift does not join");
    Image padded(7);padded.run(40,200,71);padded.run(41,200,72);
    auto p=padded.detect();check(p.valid&&p.metrics[0].width==71.5,"padded RGB and even median are supported");
    Image eight;
    for(int i=0;i<8;++i){eight.run(40+i*4,100,100);eight.run(41+i*4,100,100);}
    check(eight.detect().valid&&eight.detect().centers.size()==8,"exact eight-center capacity");
    eight.run(72,100,100);eight.run(73,100,100);
    auto ninth=eight.detect();check(!ninth.valid&&ninth.centers.empty()&&ninth.reason=="too_many_centers","ninth center rejects whole observation");
    Image budget;
    for(int y=30;y<570;++y)for(int x=0;x+70<=1280;x+=71)budget.run(y,x,70);
    auto overflow=budget.detect();check(!overflow.valid&&overflow.centers.empty()&&overflow.reason=="qualifying_run_budget_exhausted","run resource overflow never returns a partial set");
    check(!calibration::detect_opening_centers(640,720,image.stride,image.bytes).valid,"wrong normalized width rejected");
    check(!calibration::detect_opening_centers(1280,720,-1,image.bytes).valid,"negative stride rejected");
    check(!calibration::detect_opening_centers(1280,720,16385,image.bytes).valid,"oversized stride rejected");
    check(!calibration::detect_opening_centers(1280,720,image.stride,std::span<const std::uint8_t>(image.bytes).first(100)).valid,"truncated payload rejected");
    std::cout<<checks<<" opening detector checks passed; completeness remains unknown.\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
