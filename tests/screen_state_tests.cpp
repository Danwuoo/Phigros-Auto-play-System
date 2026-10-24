#include "rhythm/screen_state.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace rhythm;
namespace {
int checks=0;
void check(bool value) {++checks;if(!value)throw std::runtime_error("check failed");}
template<class F>void rejects(F&& f) {bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}check(rejected);}
}
int main() {
    try {
        std::vector<std::uint8_t> pixels(64*48*3,24);
        for(int y=4;y<14;++y)for(int x=4;x<14;++x)for(int c=0;c<3;++c)
            pixels[(y*64+x)*3+c]=static_cast<std::uint8_t>((x+y)%2?240:16);
        for(int y=28;y<40;++y)for(int x=40;x<56;++x)for(int c=0;c<3;++c)
            pixels[(y*64+x)*3+c]=static_cast<std::uint8_t>(x%3?220:32);
        const RgbImageView view{64,48,64*3,pixels};
        const std::vector<PixelRegion> regions{{4,4,10,10},{40,28,16,12}};
        const auto reference=make_screen_state_template(view,ObservedGameState::playing,regions,std::string(64,'a'),"synthetic-only");
        std::vector<ScreenStateTemplate> templates{reference};
        check(classify_screen_state(view,templates).state==ObservedGameState::playing);
        auto changed=pixels;
        // An unrelated changing playfield outside the calibrated anchors is allowed.
        std::fill(changed.begin()+20*64*3,changed.begin()+21*64*3,123);
        check(classify_screen_state({64,48,64*3,changed},templates).state==ObservedGameState::playing);
        // A menu overlay or another state changing either anchor must revoke.
        for(int y=4;y<14;++y)for(int x=4;x<14;++x)for(int c=0;c<3;++c)changed[(y*64+x)*3+c]/=2;
        check(classify_screen_state({64,48,64*3,changed},templates).state==ObservedGameState::unknown);
        std::fill(changed.begin(),changed.end(),24);
        check(classify_screen_state({64,48,64*3,changed},templates).state==ObservedGameState::unknown);
        auto ambiguous=reference;ambiguous.state=ObservedGameState::paused;templates.push_back(ambiguous);
        check(classify_screen_state(view,templates).reason=="ambiguous_calibrated_states");
        templates.pop_back();
        check(classify_screen_state({64,48,64*3,std::span<const std::uint8_t>(pixels).first(5)},templates).state==ObservedGameState::unknown);
        check(classify_screen_state({48,64,48*3,pixels},templates).state==ObservedGameState::unknown);
        rejects([&]{make_screen_state_template(view,ObservedGameState::unknown,regions,std::string(64,'a'),"bad");});
        rejects([&]{make_screen_state_template(view,ObservedGameState::playing,{{4,4,10,10},{8,8,10,10}},std::string(64,'a'),"overlap");});
        rejects([&]{make_screen_state_template(view,ObservedGameState::playing,{{-1,0,10,10},regions[1]},std::string(64,'a'),"bounds");});
        rejects([&]{make_screen_state_template(view,ObservedGameState::playing,{{0,20,5,5},regions[1]},std::string(64,'a'),"flat");});
        rejects([&]{make_screen_state_template(view,ObservedGameState::playing,regions,"unknown","hash");});
        auto malformed=reference;malformed.anchors[0].grayscale.pop_back();
        rejects([&]{classify_screen_state(view,std::span<const ScreenStateTemplate>(&malformed,1));});
        auto config=ScreenStateConfig{};config.max_anchor_mean_error=std::numeric_limits<double>::quiet_NaN();
        rejects([&]{classify_screen_state(view,templates,config);});
        config={};config.max_template_pixels=100;
        rejects([&]{make_screen_state_template(view,ObservedGameState::playing,regions,std::string(64,'a'),"budget",config);});
        std::cout<<checks<<" offline screen-state checks passed; synthetic pixels only\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
