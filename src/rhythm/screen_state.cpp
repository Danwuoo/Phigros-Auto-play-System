#include "rhythm/screen_state.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm {
namespace {
void validate_config(const ScreenStateConfig& c) {
    if(!std::isfinite(c.max_anchor_mean_error)||c.max_anchor_mean_error<0||c.max_anchor_mean_error>32||
       c.minimum_anchor_contrast<1||c.minimum_anchor_contrast>255||
       c.max_template_pixels<1||c.max_template_pixels>65536||c.max_templates<1||c.max_templates>8)
        throw std::invalid_argument("screen-state comparison/resource configuration invalid");
}
bool valid_image(const RgbImageView& image) {
    return image.width>=2&&image.width<=4096&&image.height>=2&&image.height<=4096&&
        image.stride>=image.width*3&&image.stride<=image.width*3+4096&&
        image.pixels.size()==static_cast<std::size_t>(image.stride)*image.height;
}
bool valid_region(const PixelRegion& r,int width,int height) {
    return r.x>=0&&r.y>=0&&r.width>0&&r.height>0&&
        r.width<=width&&r.height<=height&&r.x<=width-r.width&&r.y<=height-r.height;
}
std::uint8_t gray(const RgbImageView& image,int x,int y) {
    const auto i=static_cast<std::size_t>(y)*image.stride+static_cast<std::size_t>(x)*3;
    return static_cast<std::uint8_t>((77u*image.pixels[i]+150u*image.pixels[i+1]+29u*image.pixels[i+2]+128u)>>8);
}
bool valid_state(ObservedGameState state) {
    return state==ObservedGameState::playing||state==ObservedGameState::paused||state==ObservedGameState::result;
}
void validate_template(const ScreenStateTemplate& t,const ScreenStateConfig& c) {
    if(!valid_state(t.state)||t.width<2||t.width>4096||t.height<2||t.height>4096||
        !sha256_text(t.source_image_sha256)||t.calibration_id.empty()||t.calibration_id.size()>256||
        t.anchors.size()<2||t.anchors.size()>8)
        throw std::invalid_argument("screen-state template identity/geometry invalid");
    std::size_t total=0;
    for(std::size_t i=0;i<t.anchors.size();++i) {
        const auto& a=t.anchors[i];const auto& r=a.region;
        if(!valid_region(r,t.width,t.height))throw std::invalid_argument("screen-state region outside image");
        const auto count=static_cast<std::size_t>(r.width)*r.height;
        if(a.grayscale.size()!=count||count>c.max_template_pixels-total)
            throw std::invalid_argument("screen-state template pixel budget/size mismatch");
        total+=count;
        const auto [lo,hi]=std::minmax_element(a.grayscale.begin(),a.grayscale.end());
        if(static_cast<int>(*hi)-*lo<c.minimum_anchor_contrast)
            throw std::invalid_argument("screen-state anchor lacks contrast");
        for(std::size_t j=0;j<i;++j) {
            const auto& b=t.anchors[j].region;
            if(r.x<b.x+b.width&&b.x<r.x+r.width&&r.y<b.y+b.height&&b.y<r.y+r.height)
                throw std::invalid_argument("screen-state anchors must not overlap");
        }
    }
}
} // namespace
ScreenStateTemplate make_screen_state_template(const RgbImageView& image,ObservedGameState state,
    std::vector<PixelRegion> regions,std::string hash,std::string id,ScreenStateConfig config) {
    validate_config(config);
    if(!valid_image(image))throw std::invalid_argument("invalid RGB image");
    ScreenStateTemplate result{state,image.width,image.height,std::move(hash),std::move(id),{}};
    if(regions.size()<2||regions.size()>8)throw std::invalid_argument("two to eight distinct state anchors required");
    std::size_t total=0;
    for(const auto& r:regions) {
        if(!valid_region(r,image.width,image.height))throw std::invalid_argument("invalid anchor rectangle");
        const auto count=static_cast<std::size_t>(r.width)*r.height;
        if(count>config.max_template_pixels-total)throw std::invalid_argument("template pixel budget exceeded");
        total+=count;StateAnchor anchor;anchor.region=r;anchor.grayscale.reserve(count);
        for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)anchor.grayscale.push_back(gray(image,x,y));
        result.anchors.push_back(std::move(anchor));
    }
    validate_template(result,config);return result;
}
ScreenStateResult classify_screen_state(const RgbImageView& image,
    std::span<const ScreenStateTemplate> templates,ScreenStateConfig config) {
    validate_config(config);
    if(templates.empty()||templates.size()>config.max_templates)throw std::invalid_argument("template count outside budget");
    for(const auto& t:templates)validate_template(t,config);
    ScreenStateResult result;
    if(!valid_image(image)){result.reason="invalid_image";return result;}
    std::vector<ObservedGameState> matches;
    for(const auto& t:templates) {
        ScreenTemplateScore score;score.state=t.state;score.calibration_id=t.calibration_id;
        if(image.width!=t.width||image.height!=t.height) {result.scores.push_back(score);continue;}
        score.worst_anchor_mean_error=0;bool contrast=true;
        for(const auto& anchor:t.anchors) {
            const auto& r=anchor.region;std::uint64_t error=0;std::size_t index=0;
            int lo=255,hi=0;
            for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x) {
                const int value=gray(image,x,y);lo=std::min(lo,value);hi=std::max(hi,value);
                error+=static_cast<std::uint64_t>(std::abs(value-static_cast<int>(anchor.grayscale[index++])));
            }
            score.worst_anchor_mean_error=std::max(score.worst_anchor_mean_error,static_cast<double>(error)/anchor.grayscale.size());
            contrast=contrast&&hi-lo>=config.minimum_anchor_contrast;
        }
        score.all_anchors_match=contrast&&score.worst_anchor_mean_error<=config.max_anchor_mean_error;
        if(score.all_anchors_match&&std::find(matches.begin(),matches.end(),t.state)==matches.end())matches.push_back(t.state);
        result.scores.push_back(score);
    }
    if(matches.empty())result.reason="no_calibrated_state_match";
    else if(matches.size()!=1)result.reason="ambiguous_calibrated_states";
    else {result.state=matches.front();result.reason="calibrated_pixel_anchors_match";}
    return result;
}
} // namespace rhythm
