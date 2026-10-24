#include "../apps/chart_experiment/hud_glyph_evidence.hpp"
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace rhythm;
using namespace rhythm::experimental;
unsigned checks=0,groups=0;
void check(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
using Image=std::vector<std::uint8_t>;
Image canvas(std::uint8_t gray=32) {return Image(1280u*720u*3u,gray);}
RgbImageView view(const Image& image) {return {1280,720,3840,image};}
void rectangle(Image& image,int x,int y,int w,int h,std::uint8_t value=245) {
    for(int py=y;py<y+h;++py)for(int px=x;px<x+w;++px) {
        const auto p=(static_cast<std::size_t>(py)*1280+static_cast<std::size_t>(px))*3;
        image[p]=image[p+1]=image[p+2]=value;
    }
}
void fixed_glyphs(Image& image) {
    rectangle(image,24,27,5,25);rectangle(image,38,27,5,25);
    for(int x:{1156,1180,1204,1228}) {
        rectangle(image,x,677,3,18);rectangle(image,x,677,14,3);rectangle(image,x,692,14,3);
    }
}
void title_glyph(Image& image) {
    for(int x:{30,50,70,90}) {
        rectangle(image,x,677,3,18);rectangle(image,x,677,14,3);rectangle(image,x,692,14,3);
    }
}
void score_glyphs(Image& image,bool filled=false) {
    for(int i=0;i<7;++i) {
        const int x=1084+i*24;
        rectangle(image,x,28,17,25);
        if(!filled)rectangle(image,x+3,31,11,19,32);
    }
}
Image reference() {auto image=canvas();fixed_glyphs(image);title_glyph(image);score_glyphs(image);return image;}
HudFrameMetadata metadata() {
    HudFrameMetadata f;f.sequence=f.epoch=f.generation=f.geometry_version=1;
    f.source_id="offline-synthetic";f.source_sha256=std::string(64,'a');f.frame_id="frame-1";
    f.frame_sha256=std::string(64,'b');f.evidence_id="evidence-1";f.clock_id="synthetic-pts";
    f.timestamp_kind=HudTimestampKind::video_pts;f.timestamp_ns=123;return f;
}
}
int main() {
try {
    const auto original=reference();const auto identity=metadata();
    HudGlyphEvidenceProducer producer(view(original),std::string(64,'c'),"synthetic-glyph-policy-v1");
    ++groups;{
        ScreenStateResult unknown;unknown.reason="no_calibrated_state_match";
        check(hud_fallback_eligible(true,true,unknown),"armed valid unmatched frame not eligible");
        check(!hud_fallback_eligible(false,true,unknown)&&!hud_fallback_eligible(true,false,unknown),"fallback ignored arm/source validity");
        for(const auto state:{ObservedGameState::playing,ObservedGameState::paused,ObservedGameState::result}) {
            auto classified=unknown;classified.state=state;
            check(hud_calibration_matches(classified,state)&&!hud_fallback_eligible(true,true,classified),"classified frame incorrectly eligible");
        }
        for(const auto state:{ObservedGameState::paused,ObservedGameState::result}) {
            auto ambiguous=unknown;ambiguous.reason="ambiguous_calibrated_states";
            ambiguous.scores.push_back({state,"same-frame-denied",0,true});
            ambiguous.scores.push_back({ObservedGameState::playing,"same-frame-playing",0,true});
            check(hud_calibration_matches(ambiguous,state)&&!hud_fallback_eligible(true,true,ambiguous),"ambiguous denied match lost priority");
            ambiguous.reason="no_calibrated_state_match";
            check(!hud_fallback_eligible(true,true,ambiguous),"inconsistent no-match reason overrode denied score");
            ambiguous.scores[0].all_anchors_match=false;
            check(!hud_calibration_matches(ambiguous,state),"nonmatching denied template became match");
        }
        auto invalid=unknown;invalid.reason="invalid_image";
        check(!hud_fallback_eligible(true,true,invalid),"invalid classifier result became eligible");
        auto ambiguous=unknown;ambiguous.reason="ambiguous_calibrated_states";
        check(!hud_fallback_eligible(true,true,ambiguous),"unknown ambiguity accepted without ordinary no-match reason");
    }
    ++groups;{
        const auto r=producer.observe(view(original),identity);
        check(r.valid&&r.hud_candidate&&r.score.glyph_structure_candidate,"synthetic stroke HUD rejected");
        check(r.metadata_attached&&r.frame.frame_id==identity.frame_id&&r.frame.timestamp_kind==HudTimestampKind::video_pts&&r.frame.timestamp_ns==123,"provenance mutated");
        check(r.experiment_only&&!r.live_permitted&&!r.timing_qualified&&!r.source_pixel_age_qualified,"diagnostics manufactured timing/input qualification");
        check(r.score.glyphs.size()==7&&r.score.boundary_components==0,"score shape evidence incomplete");
        check(r.title_glyph.glyph_present&&r.title_glyph.negative_space_present,"third title evidence missing");
        for(const auto& m:r.fixed_glyphs) {
            check(m.foreground_mean_error==0&&m.foreground_white_fraction==1,"exact glyph metric mismatch");
            check(m.negative_space_bright_fraction==0&&m.local_contrast>200,"negative space metric mismatch");
        }
        auto live=identity;live.timestamp_kind=HudTimestampKind::host_arrival;
        check(producer.observe(view(original),live).hud_candidate,"arrival diagnostic input unnecessarily refused");
    }
    ++groups;{
        auto lines=original;
        rectangle(lines,0,39,90,3);rectangle(lines,1140,684,130,3);
        auto r=producer.observe(view(lines),identity);
        check(r.hud_candidate,"small crossing white lines erased preserved glyph evidence");
        for(const auto& m:r.fixed_glyphs)check(m.glyph_present&&m.negative_space_present,"crossing lines lost fixed-mask evidence");
        for(const auto value:{0,255}) {
            const auto image=canvas(static_cast<std::uint8_t>(value));r=producer.observe(view(image),identity);
            check(r.valid&&!r.hud_candidate,"uniform image became playing candidate");
        }
        auto filled=original;
        rectangle(filled,21,23,28,33);rectangle(filled,1152,673,101,26);
        r=producer.observe(view(filled),identity);
        check(!r.hud_candidate&&!r.fixed_glyphs[0].negative_space_present&&!r.fixed_glyphs[1].negative_space_present,"white ROI with valid score lacked negative-space veto");
        auto generic=canvas();rectangle(generic,24,27,5,25);rectangle(generic,38,27,5,25);score_glyphs(generic,true);
        check(!producer.observe(view(generic),identity).hud_candidate,"bars+rectangles false positive");
        auto foreground=canvas(0);fixed_glyphs(foreground);
        check(!producer.observe(view(foreground),identity).hud_candidate,"copied fixed foreground without score passed");
        auto missing_title=original;rectangle(missing_title,25,672,105,29,32);
        check(!producer.observe(view(missing_title),identity).hud_candidate,"two fixed glyphs replaced required title evidence");
    }
    ++groups;{
        auto filled=original;score_glyphs(filled,true);
        const auto r=producer.observe(view(filled),identity);
        check(r.hud_candidate&&!r.score.glyph_structure_candidate&&r.score.glyphs.size()==7,"score diagnostic incorrectly became state gate");
        for(const auto& glyph:r.score.glyphs)check(!glyph.plausible_stroke&&glyph.negative_pixels==0,"solid rectangle accepted as score stroke");
        auto cropped=original;rectangle(cropped,1076,24,180,2);
        const auto boundary=producer.observe(view(cropped),identity);
        check(boundary.hud_candidate&&!boundary.score.glyph_structure_candidate&&boundary.score.boundary_components>0,"ROI crop cut line into acceptable score evidence");
        auto missing=original;rectangle(missing,1080,24,23,33,32);
        check(!producer.observe(view(missing),identity).score.glyph_structure_candidate,"six glyph score accepted by seven-glyph diagnostic");
        auto shifted=original;rectangle(shifted,1080,24,23,33,32);rectangle(shifted,1084,36,17,25);rectangle(shifted,1087,39,11,19,32);
        check(!producer.observe(view(shifted),identity).score.glyph_structure_candidate,"different score baseline accepted");
    }
    ++groups;{
        auto dim=original;for(auto& p:dim)p=static_cast<std::uint8_t>(p/2);
        check(!producer.observe(view(dim),identity).hud_candidate,"brightness normalization resurrected dim pause overlay");
        for(const auto state:{ObservedGameState::paused,ObservedGameState::result}) {
            const auto r=producer.observe(view(original),identity,state);
            check(r.valid&&!r.hud_candidate&&r.denied_state_match&&r.reason=="calibrated_denied_state_has_priority","same-frame denied match overridden");
        }
        check(producer.observe(view(original),identity,ObservedGameState::playing).hud_candidate,"existing playing state obstructed independent evidence");
    }
    ++groups;{
        const std::vector<std::function<void(HudFrameMetadata&)>> changes={
            [](auto& f){f.sequence=0;},[](auto& f){f.epoch=0;},[](auto& f){f.generation=0;},
            [](auto& f){f.geometry_version=0;},[](auto& f){f.source_id.clear();},
            [](auto& f){f.source_sha256="bad";},[](auto& f){f.frame_id=std::string(513,'x');},
            [](auto& f){f.frame_sha256=std::string(64,'G');},[](auto& f){f.evidence_id.clear();},
            [](auto& f){f.clock_id.clear();},[](auto& f){f.timestamp_kind=HudTimestampKind::unknown;},
            [](auto& f){f.timestamp_ns=-1;}
        };
        for(const auto& mutate:changes) {
            auto f=identity;mutate(f);const auto r=producer.observe(view(original),f);
            check(!r.valid&&!r.hud_candidate&&!r.metadata_attached&&r.frame.source_id.empty(),"invalid metadata accepted/echoed");
        }
        for(const int stride:{0,3839,4097,std::numeric_limits<int>::max()}) {
            auto bad=view(original);bad.stride=stride;
            check(!producer.observe(bad,identity).valid,"unbounded/short stride accepted");
        }
        auto short_image=original;short_image.pop_back();check(!producer.observe(view(short_image),identity).valid,"truncated pixels accepted");
        auto wrong=view(original);wrong.width=1279;check(!producer.observe(wrong,identity).valid,"wrong geometry accepted");
    }
    ++groups;{
        const auto features=producer.observe_features(view(original));
        const auto attached=producer.observe(view(original),identity);
        check(features.valid&&features.hud_candidate&&!features.metadata_attached,"feature-only API requires or claims metadata");
        const auto& f=features.frame;
        check(f.sequence==0&&f.epoch==0&&f.generation==0&&f.geometry_version==0&&
              f.source_id.empty()&&f.source_sha256.empty()&&f.frame_id.empty()&&f.frame_sha256.empty()&&
              f.evidence_id.empty()&&f.clock_id.empty()&&f.timestamp_kind==HudTimestampKind::unknown&&f.timestamp_ns==-1,
              "feature-only API fabricated frame identity/hash/timing");
        check(features.reference_sha256==std::string(64,'c')&&features.calibration_id==attached.calibration_id,
              "feature-only API lost reference provenance");
        check(features.experiment_only&&!features.live_permitted&&!features.timing_qualified&&!features.source_pixel_age_qualified,
              "feature-only API manufactured permission or timing qualification");
        check(features.reason==attached.reason&&features.score.components==attached.score.components&&
              features.score.glyph_structure_candidate==attached.score.glyph_structure_candidate,
              "feature and metadata paths changed classification");
        for(std::size_t i=0;i<3;++i) {
            const auto& a=i<2?features.fixed_glyphs[i]:features.title_glyph;
            const auto& b=i<2?attached.fixed_glyphs[i]:attached.title_glyph;
            check(a.foreground_mean_error==b.foreground_mean_error&&a.foreground_white_fraction==b.foreground_white_fraction&&
                  a.negative_space_bright_fraction==b.negative_space_bright_fraction&&a.local_contrast==b.local_contrast,
                  "feature-only metric differs from metadata API");
        }
        for(const auto state:{ObservedGameState::paused,ObservedGameState::result}) {
            const auto r=producer.observe_features(view(original),state);
            check(r.valid&&!r.hud_candidate&&r.denied_state_match&&!r.metadata_attached&&r.frame.frame_sha256.empty(),
                  "feature-only API overrode same-frame paused/result priority");
        }
        auto wrong=view(original);wrong.stride=3839;
        const auto bad=producer.observe_features(wrong);
        check(!bad.valid&&!bad.hud_candidate&&!bad.metadata_attached&&bad.frame.sequence==0,
              "feature-only API accepted invalid image geometry");
        const auto missing_metadata=producer.observe(view(original),HudFrameMetadata{});
        check(!missing_metadata.valid&&!missing_metadata.hud_candidate&&!missing_metadata.metadata_attached,
              "full metadata API silently fell back to features");
    }
    ++groups;{
        bool rejected=false;try {const auto white=canvas(255);HudGlyphEvidenceProducer bad(view(white),std::string(64,'c'),"bad");}
        catch(const std::invalid_argument&) {rejected=true;}
        check(rejected,"all-white reference accepted without negative space");
        rejected=false;try {const auto black=canvas(0);HudGlyphEvidenceProducer bad(view(black),std::string(64,'c'),"bad");}
        catch(const std::invalid_argument&) {rejected=true;}
        check(rejected,"blank reference accepted without glyphs");
        rejected=false;try {
            auto contaminated=original;rectangle(contaminated,25,680,105,2);
            HudGlyphEvidenceProducer bad(view(contaminated),std::string(64,'c'),"contaminated-reference");
        }catch(const std::invalid_argument&) {rejected=true;}
        check(rejected,"boundary-crossing judge line learned as reference glyph");
        auto crowded=original;rectangle(crowded,1076,20,180,40,32);
        for(int y=22;y<57;y+=5)for(int x=1078;x<1252;x+=5)rectangle(crowded,x,y,2,2);
        const auto r=producer.observe(view(crowded),identity);
        check(!r.hud_candidate&&!r.score.bounded&&r.score.components==65,"component budget not enforced");
    }
    std::cout<<groups<<" groups / "<<checks<<" HUD glyph evidence checks passed; offline feature candidate only.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
