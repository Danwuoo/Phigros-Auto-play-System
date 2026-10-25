#include "../apps/chart_experiment/result_label_evidence.hpp"
#include "rhythm/screen_state_io.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
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
int main(int argc,char** argv){try {
    if(argc==3) {
        const auto calibration=load_screen_state_calibration(std::filesystem::path(argv[1]));
        std::ifstream input(std::filesystem::path(argv[2]),std::ios::binary);
        Image pixels(1280u*720u*3u);input.read(reinterpret_cast<char*>(pixels.data()),static_cast<std::streamsize>(pixels.size()));
        if(!input||input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Expected exact RGB888 frame");
        ResultLabelEvidenceProducer legacy(calibration.templates),normalized(calibration.templates,ResultLabelPolicy::local_contrast_v2);
        const auto raw=classify_screen_state(view(pixels),calibration.templates);
        const auto old=legacy.observe(view(pixels)),current=normalized.observe(view(pixels));
        nlohmann::json labels=nlohmann::json::array();
        for(const auto& metric:current.labels)labels.push_back({{"supported",metric.supported_fraction},{"unexpected",metric.unexpected_fraction},
            {"ink_ratio",metric.ink_ratio},{"contrast",metric.local_contrast}});
        std::cout<<nlohmann::json({{"legacy_candidate",old.candidate},{"normalized_candidate",current.candidate},
            {"raw_class",static_cast<int>(raw.state)},{"normalized_terminal_result",raw.state==ObservedGameState::result||
                (result_label_fallback_eligible(true,true,raw)&&current.candidate)},
            {"labels",labels},{"alignment",{current.alignment_x,current.alignment_y}}}).dump()<<'\n';return 0;
    }
    if(argc!=1)throw std::invalid_argument("Use no arguments for synthetic contracts, or DATASET.json FRAME.rgb for offline pixel inspection");
    const auto original=captions();const auto regions=ResultLabelEvidenceProducer::regions;
    auto reference=make_screen_state_template(view(original),ObservedGameState::result,
        std::vector<PixelRegion>(regions.begin(),regions.end()),std::string(64,'a'),"shared-result-labels-v1-synthetic");
    ResultLabelEvidenceProducer producer(std::span<const ScreenStateTemplate>(&reference,1));
    check(producer.observe(view(original)).candidate,"exact captions rejected");
    check(producer.observe(view(captions(2,1,70))).candidate,"caption alignment/background rejected");
    ResultLabelEvidenceProducer normalized(std::span<const ScreenStateTemplate>(&reference,1),ResultLabelPolicy::local_contrast_v2);
    check(normalized.observe(view(original)).candidate,"normalized exact captions rejected");
    for(const int background:{0,24,70,110,150}) {
        auto relit=captions(2,1);
        for(auto& gray:relit)gray=static_cast<std::uint8_t>(background+(gray==170?90:0));
        check(normalized.observe(view(relit)).candidate,"local contrast failed under affine brightness changes");
    }
    for(const auto& r:regions) {
        auto missing=original;
        for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)put(missing,x,y,24);
        check(!producer.observe(view(missing)).candidate,"missing caption accepted");
        check(!normalized.observe(view(missing)).candidate,"normalized missing caption accepted");
    }
    for(const std::uint8_t color:{std::uint8_t{0},std::uint8_t{170},std::uint8_t{255}}) {
        check(!producer.observe(view(Image(original.size(),color))).candidate,"flat screen accepted");
        check(!normalized.observe(view(Image(original.size(),color))).candidate,"normalized flat screen accepted");
    }
    auto checker=original;
    for(const auto& r:regions)for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)put(checker,x,y,(x+y)%2?170:24);
    check(!producer.observe(view(checker)).candidate,"checker accepted");
    check(!normalized.observe(view(checker)).candidate,"normalized checker accepted");
    check(!producer.observe(view(captions(10,0))).candidate,"large shift accepted");
    check(!normalized.observe(view(captions(10,0))).candidate,"normalized large shift accepted");
    auto faint=captions(0,0,127);
    for(auto& gray:faint)if(gray==170)gray=128;
    check(!producer.observe(view(faint)).candidate,"one-level caption contrast accepted");
    check(!normalized.observe(view(faint)).candidate,"normalized one-level contrast accepted");
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
