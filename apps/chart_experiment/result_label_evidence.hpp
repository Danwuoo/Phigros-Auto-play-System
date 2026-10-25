#pragma once
#include "rhythm/screen_state.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace rhythm::experimental {
enum class ResultLabelPolicy { absolute_v1, local_contrast_v2 };
// Receipt-only terminal UI evidence. This policy never identifies a score,
// judgment, song, clock, or permission to send input.
inline bool result_label_fallback_eligible(bool sequence_complete,bool source_valid,const ScreenStateResult& raw) {
    return sequence_complete&&source_valid&&raw.state==ObservedGameState::unknown&&
        std::none_of(raw.scores.begin(),raw.scores.end(),[](const auto& score){return score.all_anchors_match;});
}
struct ResultLabelMetric {
    double supported_fraction=0,unexpected_fraction=1,ink_ratio=0,local_contrast=0;
};
struct ResultLabelEvidence {
    bool candidate=false;
    int alignment_x=0,alignment_y=0;
    std::string calibration_id;
    std::array<ResultLabelMetric,5> labels{};
};
class ResultLabelEvidenceProducer {
public:
    // Fixed words: Max Combo, Accuracy, Good, Bad, Miss. Deliberately excludes
    // values and the old partial-Perfect patch. Actual caption positions are
    // stable in the archived references; transparent background levels differ.
    static constexpr std::array<PixelRegion,5> regions={PixelRegion{710,432,100,20},
        PixelRegion{1050,432,82,22},PixelRegion{783,538,33,14},
        PixelRegion{850,538,30,14},PixelRegion{915,538,33,14}};
    explicit ResultLabelEvidenceProducer(std::span<const ScreenStateTemplate> templates,
        ResultLabelPolicy policy=ResultLabelPolicy::absolute_v1):policy_(policy) {
        for(const auto& item:templates)if(item.state==ObservedGameState::result) {
            // Retain the earlier calibrated three-anchor result templates.
            // Only the explicitly versioned caption references join this policy.
            if(item.calibration_id.rfind("shared-result-labels-v1-",0)!=0)continue;
            if(references_.size()>=2||item.width!=1280||item.height!=720||item.anchors.size()!=regions.size())
                throw std::invalid_argument("result label reference geometry/count differs");
            Reference reference;reference.id=item.calibration_id;
            for(std::size_t n=0;n<regions.size();++n) {
                const auto& anchor=item.anchors[n];const auto& r=regions[n];
                if(anchor.region.x!=r.x||anchor.region.y!=r.y||anchor.region.width!=r.width||anchor.region.height!=r.height||
                   anchor.grayscale.size()!=static_cast<std::size_t>(r.width*r.height))
                    throw std::invalid_argument("result label regions differ from versioned policy");
                const auto threshold=caption_threshold(anchor.grayscale);
                if(threshold<0)throw std::invalid_argument("result label reference lacks local contrast");
                auto& mask=reference.masks[n];mask.reserve(anchor.grayscale.size());
                for(const auto gray:anchor.grayscale)mask.push_back(gray>=threshold?1:0);
                reference.counts[n]=std::count(mask.begin(),mask.end(),1);
                if(reference.counts[n]<20||reference.counts[n]*2>=mask.size())
                    throw std::invalid_argument("result label reference lacks bounded caption ink");
            }
            references_.push_back(std::move(reference));
        }
        if(references_.empty())throw std::invalid_argument("result label references missing");
    }
    ResultLabelEvidence observe(const RgbImageView& image) const {
        ResultLabelEvidence best;double best_quality=-1;
        if(image.width!=1280||image.height!=720||image.stride<3840||image.stride>4096||
           image.pixels.size()!=static_cast<std::size_t>(image.stride)*720)return best;
        // One global, small alignment for all five words. No per-label search.
        for(const auto& reference:references_)for(int dy=-1;dy<=1;++dy)for(int dx=-2;dx<=2;++dx) {
            ResultLabelEvidence current;current.candidate=true;current.calibration_id=reference.id;
            current.alignment_x=dx;current.alignment_y=dy;double quality=1;
            for(std::size_t n=0;n<regions.size();++n) {
                const auto& r=regions[n];const auto& expected=reference.masks[n];
                std::array<std::uint8_t,2000> observed{},grayscale{};
                std::size_t ink=0,supported=0,unexpected=0,negative_count=0;
                std::uint64_t ink_sum=0,negative_sum=0;
                for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x) {
                    const auto p=static_cast<std::size_t>(r.y+y+dy)*image.stride+static_cast<std::size_t>(r.x+x+dx)*3;
                    const auto gray=(77u*image.pixels[p]+150u*image.pixels[p+1]+29u*image.pixels[p+2]+128u)>>8;
                    const auto index=static_cast<std::size_t>(y*r.width+x);
                    grayscale[index]=static_cast<std::uint8_t>(gray);
                }
                const auto threshold=caption_threshold({grayscale.data(),expected.size()});
                if(threshold<0)current.candidate=false;
                for(std::size_t index=0;index<expected.size();++index) {
                    observed[index]=threshold>=0&&grayscale[index]>=threshold?1:0;
                    ink+=observed[index];if(observed[index])ink_sum+=grayscale[index];
                }
                for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x) {
                    const auto index=static_cast<std::size_t>(y*r.width+x);bool near_observed=false,near_expected=false;
                    for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox) {
                        const int px=x+ox,py=y+oy;
                        if(px<0||py<0||px>=r.width||py>=r.height)continue;
                        const auto neighbor=static_cast<std::size_t>(py*r.width+px);
                        near_observed=near_observed||observed[neighbor];near_expected=near_expected||expected[neighbor];
                    }
                    if(expected[index]&&near_observed)++supported;
                    if(observed[index]&&!near_expected)++unexpected;
                    if(!near_expected){++negative_count;negative_sum+=grayscale[index];}
                }
                auto& metric=current.labels[n];
                metric.supported_fraction=static_cast<double>(supported)/static_cast<double>(reference.counts[n]);
                metric.unexpected_fraction=ink?static_cast<double>(unexpected)/static_cast<double>(ink):1;
                metric.ink_ratio=static_cast<double>(ink)/static_cast<double>(reference.counts[n]);
                metric.local_contrast=ink&&negative_count?static_cast<double>(ink_sum)/static_cast<double>(ink)-
                    static_cast<double>(negative_sum)/static_cast<double>(negative_count):0;
                current.candidate=current.candidate&&metric.supported_fraction>=0.98&&metric.unexpected_fraction<=0.05&&
                    metric.ink_ratio>=0.7&&metric.ink_ratio<=1.4&&metric.local_contrast>=40;
                quality=std::min(quality,metric.supported_fraction-metric.unexpected_fraction);
            }
            if(current.candidate)return current;
            if(quality>best_quality){best_quality=quality;best=std::move(current);}
        }
        return best;
    }
private:
    int caption_threshold(std::span<const std::uint8_t> pixels) const noexcept {
        if(policy_==ResultLabelPolicy::absolute_v1)return 128;
        // Normalize each fixed caption's foreground/background mixture. A
        // global absolute cutoff made bright-background antialiasing count as
        // extra ink. Both frozen references and current pixels use this rule.
        // No learned/current-frame template and no score digits are involved.
        std::array<std::size_t,256> histogram{};
        for(const auto gray:pixels)++histogram[gray];
        const auto percentile=[&](std::size_t numerator) {
            const auto rank=(pixels.size()*numerator+99)/100;
            std::size_t count=0;
            for(int gray=0;gray<256;++gray) {
                count+=histogram[static_cast<std::size_t>(gray)];
                if(count>=rank)return gray;
            }
            return 255;
        };
        const auto background=percentile(20),foreground=percentile(99);
        if(foreground-background<40)return -1;
        return background+(foreground-background+1)/2;
    }
    ResultLabelPolicy policy_;
    struct Reference {std::string id;std::array<std::vector<std::uint8_t>,5> masks;std::array<std::size_t,5> counts{};};
    std::vector<Reference> references_; // At most two references, fixed 5x2000 pixels each.
};
} // namespace rhythm::experimental
