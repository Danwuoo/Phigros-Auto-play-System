#include "../apps/chart_experiment/result_label_evidence.hpp"
#include <iostream>
#include <stdexcept>
using namespace rhythm;
using namespace rhythm::experimental;
namespace {
using Image=std::vector<std::uint8_t>;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
RgbImageView view(const Image& image){return {1280,720,3840,image};}
void put(Image& image,int x,int y,std::uint8_t gray){const auto p=static_cast<std::size_t>(y*1280+x)*3;image[p]=image[p+1]=image[p+2]=gray;}
Image captions(int dx=0,int dy=0,std::uint8_t background=24) {
    Image image(1280u*720u*3u,background);
    for(const auto& r:ResultLabelEvidenceProducer::regions)for(int k=0;k<4;++k)
        for(int y=3;y<10;++y)for(int x=0;x<4;++x)
            if(x==0||y==3||y==9)put(image,r.x+2+k*6+x+dx,r.y+y+dy,170);
    return image;
}
}
int main(){try {
    const auto original=captions();const auto regions=ResultLabelEvidenceProducer::regions;
    auto reference=make_screen_state_template(view(original),ObservedGameState::result,
        std::vector<PixelRegion>(regions.begin(),regions.end()),std::string(64,'a'),"shared-result-labels-v1-synthetic");
    ResultLabelEvidenceProducer producer(std::span<const ScreenStateTemplate>(&reference,1));
    check(producer.observe(view(original)).candidate,"exact captions rejected");
    check(producer.observe(view(captions(2,1,70))).candidate,"caption alignment/background rejected");
    for(const auto& r:regions) {
        auto missing=original;
        for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)put(missing,x,y,24);
        check(!producer.observe(view(missing)).candidate,"missing caption accepted");
    }
    for(const std::uint8_t color:{std::uint8_t{0},std::uint8_t{170},std::uint8_t{255}})
        check(!producer.observe(view(Image(original.size(),color))).candidate,"flat screen accepted");
    auto checker=original;
    for(const auto& r:regions)for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)put(checker,x,y,(x+y)%2?170:24);
    check(!producer.observe(view(checker)).candidate,"checker accepted");
    check(!producer.observe(view(captions(10,0))).candidate,"large shift accepted");
    auto faint=captions(0,0,127);
    for(auto& gray:faint)if(gray==170)gray=128;
    check(!producer.observe(view(faint)).candidate,"one-level caption contrast accepted");
    auto bad=view(original);bad.width=1279;check(!producer.observe(bad).candidate,"invalid geometry accepted");
    ScreenStateResult raw;
    check(result_label_fallback_eligible(true,true,raw),"completed valid unknown ineligible");
    check(!result_label_fallback_eligible(false,true,raw)&&!result_label_fallback_eligible(true,false,raw),"input/source gate omitted");
    for(const auto state:{ObservedGameState::playing,ObservedGameState::paused,ObservedGameState::result}) {
        raw.state=state;check(!result_label_fallback_eligible(true,true,raw),"classified evidence overwritten");
        raw.state=ObservedGameState::unknown;raw.scores={{state,"ambiguous",0,true}};
        check(!result_label_fallback_eligible(true,true,raw),"ambiguous evidence overwritten");raw.scores.clear();
    }
    auto wrong=reference;wrong.anchors.pop_back();
    bool rejected=false;try{ResultLabelEvidenceProducer invalid(std::span<const ScreenStateTemplate>(&wrong,1));}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"wrong reference policy accepted");
    std::cout<<"result caption and terminal-only evidence contracts passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
