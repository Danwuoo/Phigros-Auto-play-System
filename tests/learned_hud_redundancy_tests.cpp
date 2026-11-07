#include "../apps/chart_experiment/learned_hud_redundancy.hpp"
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace rhythm;
using namespace rhythm::experimental;
using Image=std::vector<std::uint8_t>;
unsigned groups=0,checks=0;
constexpr std::int64_t ms=1'000'000;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
RgbImageView view(const Image& a){return {1280,720,3840,a};}
void rectangle(Image& a,int x,int y,int w,int h,std::uint8_t c=245){
    for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx){const auto i=static_cast<std::size_t>(yy*1280+xx)*3;a[i]=a[i+1]=a[i+2]=c;}
}
void title(Image& a){for(int x:{30,50,70,90}){rectangle(a,x,677,3,18);rectangle(a,x,677,14,3);rectangle(a,x,692,14,3);}}
Image reference(){
    Image a(1280u*720u*3u,32);rectangle(a,24,27,5,25);rectangle(a,38,27,5,25);title(a);
    rectangle(a,1156,677,2,17);rectangle(a,1163,677,2,17);rectangle(a,1175,677,2,17);
    for(int y=0;y<17;++y)rectangle(a,1163+y*11/16,677+y,2,1);
    rectangle(a,1194,677,2,17);rectangle(a,1194,692,10,2);
    for(int y=0;y<13;++y){const int d=y*5/12;rectangle(a,1206+d,681+y,2,1);rectangle(a,1215-d,681+y,2,1);}
    rectangle(a,1220,691,3,3);
    rectangle(a,1228,677,3,18);rectangle(a,1228,677,14,3);rectangle(a,1228,692,14,3);
    for(int i=0;i<7;++i){const int x=1084+i*24;rectangle(a,x,28,17,25);rectangle(a,x+3,31,11,19,32);}
    return a;
}
Image occluded(const Image& reference){auto a=reference;rectangle(a,1090,0,3,62);return a;}
Image merged_score_line(const Image& reference,int thickness=2,int width=180){
    auto a=reference;rectangle(a,1076,40,width,thickness);return a;
}
Image upper_boundary_score_line(const Image& reference,int end_y){
    auto a=reference;
    // Two raster rows keep successive staircase columns four-connected.
    // A one-pixel diagonal would only touch at corners and is not a merged CC.
    for(int x=0;x<180;++x)rectangle(a,1076+x,20+(end_y-20)*x/179,1,2);
    return a;
}
Image partial_boundary_score(const Image& reference){
    auto a=reference;
    // Three connected digits at the left ROI edge, one truly missing digit,
    // then three intact digits. No single CC spans four learned windows.
    rectangle(a,1156,28,17,25,32);rectangle(a,1076,40,76,2);return a;
}
Image two_boundary_score_fragments(const Image& reference){
    auto a=reference;rectangle(a,1076,40,49,2);
    rectangle(a,1156,20,2,22);rectangle(a,1156,40,41,2);return a;
}
void score_digit(Image& a,int index){
    const int x=1084+24*index;rectangle(a,x,28,17,25);rectangle(a,x+3,31,11,19,32);
}
LearnedHudFrame frame(std::uint64_t seq,std::int64_t time){
    LearnedHudFrame m;m.sequence=seq;m.epoch=m.generation=m.geometry_version=1;m.source_id="offline-source";
    m.received_ns=time*ms;m.processed_ns=m.received_ns+ms;m.source_valid=true;m.rotation=1;return m;
}
ScreenStateResult raw(){ScreenStateResult r;r.reason="no_calibrated_state_match";return r;}
void train(LearnedHudRedundancy& h,const Image& a,const HudGlyphEvidence& p){
    check(!h.observe(view(a),p,raw(),frame(1,1000),false).ready,"one frame learned a template");
    const auto& r=h.observe(view(a),p,raw(),frame(2,1060),false);
    if(!r.ready)std::cerr<<"training reason: "<<r.reason<<" primary="<<p.reason<<"\n";
    check(r.ready&&r.support_frames==2&&!r.current_candidate,"two distinct clean frames not locked");
}
std::string digest(std::span<const std::uint8_t> a){return chart_text_sha256({reinterpret_cast<const char*>(a.data()),a.size()});}
}
int main(){try{
    const auto image=reference(),blocked=occluded(image);
    HudGlyphEvidenceProducer producer(view(image),std::string(64,'a'),"synthetic-IN-v3",{},HudGlyphPolicy::shared_in_subpixel_prefix_score_v3);
    const auto clean=producer.observe_features(view(image)),occlusion=producer.observe_features(view(blocked));
    ++groups;{
        auto pixels=image;
        // The third digit's interior has an actual contrasting but lighter
        // background. The calibrated cell path fails after two accepted cells.
        rectangle(pixels,1135,31,11,19,150);
        const auto primary=producer.observe_features(view(pixels));
        check(primary.valid&&!primary.hud_candidate&&primary.reason=="score_digit_structure_missing"&&
            primary.score.components==7&&primary.score.supported_cells==2&&primary.score.boundary_components==0,
            "seven-component score cell mismatch control invalid");
        LearnedHudRedundancy h;train(h,image,clean);
        const auto& result=h.observe(view(pixels),primary,raw(),frame(3,1200),true);
        check(result.current_candidate&&result.auxiliary.hud_candidate&&result.auxiliary.score.glyphs.size()==7&&
            result.reason=="current_learned_full_HUD_after_score_cell_mismatch",
            "seven actual independent score glyphs did not recover the calibrated score cell failure");
        auto no_title=pixels;rectangle(no_title,25,672,512,29,32);
        check(!h.observe(view(no_title),primary,raw(),frame(4,1300),true).current_candidate,
            "score-cell fallback reused the previous title");
        auto no_digit=pixels;rectangle(no_digit,1132,28,17,25,32);
        check(!h.observe(view(no_digit),primary,raw(),frame(5,1400),true).current_candidate,
            "score-cell fallback accepted six actual glyphs");
        auto shifted=pixels;rectangle(shifted,1076,20,180,40,32);
        for(int i=0;i<7;++i){const int x=1081+i*24;rectangle(shifted,x,28,17,25);rectangle(shifted,x+3,31,11,19,32);}
        check(h.observe(view(shifted),primary,raw(),frame(6,1500),true).auxiliary.score.glyph_structure_candidate&&
            h.status().current_candidate,"same calibrated right-anchor tolerance was not preserved");
        rectangle(shifted,1076,20,180,40,32);
        for(int i=0;i<7;++i){const int x=1080+i*24;rectangle(shifted,x,28,17,25);rectangle(shifted,x+3,31,11,19,32);}
        check(h.observe(view(shifted),primary,raw(),frame(7,1600),true).auxiliary.score.glyph_structure_candidate&&
            !h.status().current_candidate,"score-cell fallback accepted seven actual glyphs at the wrong right anchor");
        auto pause=raw();pause.state=ObservedGameState::paused;
        check(!h.observe(view(pixels),primary,pause,frame(8,1700),true).current_candidate,
            "score-cell fallback overrode a current pause");
    }
    ++groups;{
        auto long_title=image;
        for(int x=110;x<=530;x+=20){rectangle(long_title,x,677,3,18);rectangle(long_title,x,677,14,3);rectangle(long_title,x,692,14,3);}
        const auto teacher=producer.observe_features(view(long_title));
        LearnedHudRedundancy h;train(h,long_title,teacher);
        check(h.status().title_region.width>=96&&h.status().title_region.width<512,
            "long title did not learn a bounded left prefix");
        const auto covered=occluded(long_title);
        const auto primary=producer.observe_features(view(covered));
        check(h.observe(view(covered),primary,raw(),frame(3,1200),true).current_candidate,
            "current long title prefix and score occlusion did not support the existing fallback");
        auto changed_title=covered;rectangle(changed_title,25,672,h.status().title_region.width,29,32);
        check(!h.observe(view(changed_title),primary,raw(),frame(4,1300),true).current_candidate,
            "long title fallback accepted the old prefix instead of current pixels");
        auto continuous=long_title;rectangle(continuous,26,685,511,2);
        LearnedHudRedundancy no_gap;
        const auto& rejected=no_gap.observe(view(continuous),teacher,raw(),frame(1,1000),false);
        check(!rejected.ready&&rejected.support_frames==0&&rejected.reason=="title_geometry_or_negative_space_unsupported",
            "long title learning cropped a connected line without a complete blank boundary");
    }
    ++groups;{
        for(const auto policy:{HudGlyphPolicy::shared_in_subpixel_prefix_score_v3,HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4}){
            const bool at=policy==HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4;
            auto primary_reference=image;for(int y=673;y<699;++y)for(int x=at?1144:1152;x<1225;++x){
                const auto i=static_cast<std::size_t>(y*1280+x)*3;if(primary_reference[i]>195)primary_reference[i]=primary_reference[i+1]=primary_reference[i+2]=255;}
            HudGlyphEvidenceProducer typed(view(primary_reference),std::string(64,'c'),"joint-ordinary-control",{},policy);
            auto teacher_image=primary_reference;rectangle(teacher_image,1157,677,1,17,32);const auto teacher=typed.observe_features(view(teacher_image));
            auto prefix_image=teacher_image;if(at)rectangle(prefix_image,1146,681,3,3);else rectangle(prefix_image,1157,684,6,2);
            const auto prefix_only=typed.observe_features(view(prefix_image));const auto joint_image=occluded(prefix_image);
            const auto primary=typed.observe_features(view(joint_image));
            check(teacher.hud_candidate&&!primary.hud_candidate&&primary.reason=="fixed_glyph_or_negative_space_missing"&&
                !primary.fixed_glyphs[1].glyph_present&&primary.score.bounded&&primary.score.calibrated_cells&&
                !primary.score.glyph_structure_candidate&&primary.score.boundary_components==1,"joint prefix/ordinary score control invalid");
            LearnedHudRedundancy h;train(h,teacher_image,teacher);const auto& admitted=h.observe(view(joint_image),primary,raw(),frame(3,1200),true);
            check(admitted.current_candidate&&admitted.auxiliary.valid&&admitted.auxiliary.hud_candidate&&
                admitted.auxiliary.fixed_glyphs[0].glyph_present&&admitted.auxiliary.fixed_glyphs[1].glyph_present&&admitted.auxiliary.title_glyph.glyph_present&&
                admitted.auxiliary.score.boundary_components>0&&admitted.plausible_score_components>=4&&
                admitted.reason=="current_learned_full_HUD_with_joint_prefix_and_ordinary_score_occlusion"&&!admitted.merged_score.candidate,
                "joint ordinary path did not remeasure all three full anchors and independent ordinary score evidence");
            check(!admitted.auxiliary.live_permitted&&!admitted.auxiliary.timing_qualified&&!admitted.auxiliary.source_pixel_age_qualified,
                "joint ordinary candidate manufactured input/timing qualification");
            const auto& old_route=h.observe(view(prefix_image),prefix_only,raw(),frame(4,1300),true);
            check(old_route.current_candidate&&old_route.reason=="current_learned_full_HUD_after_primary_prefix_mismatch"&&old_route.auxiliary.score.glyph_structure_candidate,
                "joint ordinary path replaced the original clean-score prefix route");
            const auto merged=merged_score_line(prefix_image);const auto merged_primary=typed.observe_features(view(merged));
            const auto& joint_merged=h.observe(view(merged),merged_primary,raw(),frame(5,1400),true);
            check(joint_merged.current_candidate&&joint_merged.auxiliary.valid&&joint_merged.merged_score.candidate&&
                joint_merged.merged_score.textured_windows>=4&&
                joint_merged.reason=="current_learned_three_anchors_with_joint_prefix_and_merged_score_boundary",
                "joint prefix/merged-score route did not independently remeasure complete current anchors and score texture");
        }
    }
    ++groups;{
        auto primary_reference=image;for(int y=673;y<699;++y)for(int x=1152;x<1225;++x){
            const auto i=static_cast<std::size_t>(y*1280+x)*3;if(primary_reference[i]>195)primary_reference[i]=primary_reference[i+1]=primary_reference[i+2]=255;}
        HudGlyphEvidenceProducer typed(view(primary_reference),std::string(64,'d'),"joint-ordinary-negative-control",{},HudGlyphPolicy::shared_in_subpixel_prefix_score_v3);
        auto teacher_image=primary_reference;rectangle(teacher_image,1157,677,1,17,32);const auto teacher=typed.observe_features(view(teacher_image));
        auto joint_image=teacher_image;rectangle(joint_image,1157,684,6,2);joint_image=occluded(joint_image);const auto primary=typed.observe_features(view(joint_image));
        const std::vector<std::function<void(Image&)>> missing={
            [](auto& a){rectangle(a,25,672,512,29,32);},[](auto& a){rectangle(a,1227,673,26,26,32);},
            [](auto& a){rectangle(a,21,23,28,33,32);},
            [](auto& a){for(int i=3;i<6;++i)rectangle(a,1084+i*24,24,23,33,32);}};
        for(const auto& mutate:missing){LearnedHudRedundancy h;train(h,teacher_image,teacher);
            check(h.observe(view(joint_image),primary,raw(),frame(3,1200),true).current_candidate,"joint positive before removal invalid");
            auto pixels=joint_image;mutate(pixels);const auto& denied=h.observe(view(pixels),primary,raw(),frame(4,1300),true);
            check(!denied.current_candidate&&denied.auxiliary.valid&&denied.reason=="current_auxiliary_anchors_or_ordinary_score_missing_after_joint_occlusion",
                "joint path borrowed previous full anchors/score instead of measuring the current removed pixels");}
        auto low=primary;low.score.glyphs.resize(3);LearnedHudRedundancy primary_low;train(primary_low,teacher_image,teacher);
        const auto& independently_measured=primary_low.observe(view(joint_image),low,raw(),frame(3,1200),true);
        check(independently_measured.current_candidate&&independently_measured.auxiliary.valid&&
            independently_measured.merged_score.boundary_fragments>=1&&independently_measured.merged_score.textured_windows>=4,
            "primary component count replaced independent CURRENT partial-boundary score pixels");
        auto invalid_core=primary;invalid_core.fixed_glyphs[1].foreground_white_fraction=.97;
        LearnedHudRedundancy core;train(core,teacher_image,teacher);
        check(!core.observe(view(joint_image),invalid_core,raw(),frame(3,1200),true).current_candidate,"joint route weakened prefix core thresholds");
        LearnedHudRedundancy stale;train(stale,teacher_image,teacher);auto expired=frame(3,1200);expired.processed_ns+=499*ms;
        check(!stale.observe(view(joint_image),primary,raw(),expired,true).current_candidate&&!stale.status().auxiliary.valid,"joint route renewed500ms-old pixels");
        LearnedHudRedundancy changed;train(changed,teacher_image,teacher);auto other=frame(3,1200);++other.epoch;
        check(!changed.observe(view(joint_image),primary,raw(),other,true).current_candidate&&!changed.status().ready,"joint route reused another source epoch");
        auto denied_raw=raw();denied_raw.state=ObservedGameState::paused;LearnedHudRedundancy denied;train(denied,teacher_image,teacher);
        check(!denied.observe(view(joint_image),primary,denied_raw,frame(3,1200),true).current_candidate,"joint route overrode current denied screen");
        LearnedHudRedundancy postarm;
        check(!postarm.observe(view(joint_image),primary,raw(),frame(1,1200),true).current_candidate&&
            !postarm.observe(view(teacher_image),teacher,raw(),frame(2,1300),false).ready,"joint failure trained a template after arming");
    }
    ++groups;{
        for(const bool two_fragments:{false,true}){
            const auto pixels=two_fragments?two_boundary_score_fragments(image):partial_boundary_score(image);
            const auto primary=producer.observe_features(view(pixels));
            check(primary.valid&&!primary.hud_candidate&&primary.reason=="score_digit_structure_missing"&&
                primary.score.bounded&&primary.score.calibrated_cells&&
                primary.score.boundary_components==static_cast<std::size_t>(two_fragments?2:1),
                "partial-boundary synthetic control did not contain the intended actual CCs");
            LearnedHudRedundancy h;train(h,image,clean);
            const auto& result=h.observe(view(pixels),primary,raw(),frame(3,1200),true);
            check(result.current_candidate&&result.auxiliary.valid&&result.auxiliary.hud_candidate&&
                result.auxiliary.fixed_glyphs[0].glyph_present&&result.auxiliary.fixed_glyphs[1].glyph_present&&
                result.auxiliary.title_glyph.glyph_present&&result.merged_score.candidate&&
                result.merged_score.boundary_fragments==static_cast<std::size_t>(two_fragments?2:1)&&
                result.merged_score.clean_components==3&&result.merged_score.spanned_windows>=4&&
                result.merged_score.textured_windows>=4&&result.merged_score.boundary_textured_windows>=1&&
                result.reason=="current_learned_three_anchors_with_partial_boundary_score",
                "actual partial merged components and clean digits did not supply four independent current texture windows");
            check(!result.auxiliary.score.glyph_structure_candidate&&!result.auxiliary.live_permitted&&
                !result.auxiliary.timing_qualified&&!result.auxiliary.source_pixel_age_qualified,
                "partial score evidence invented digits or live/timing qualification");
        }
        const auto partial=partial_boundary_score(image);const auto primary=producer.observe_features(view(partial));
        const std::vector<std::function<void(Image&)>> invalid={
            [](auto& a){rectangle(a,1076,20,180,40,32);},
            [](auto& a){rectangle(a,1076,20,180,40);},
            [](auto& a){rectangle(a,1076,20,180,40,32);rectangle(a,1076,40,180,2);},
            [](auto& a){rectangle(a,1076,20,180,40,32);for(int i:{4,5,6})score_digit(a,i);rectangle(a,1076,20,76,2);},
            [](auto& a){rectangle(a,1076,20,180,40,32);for(int i:{0,4,6})score_digit(a,i);rectangle(a,1076,40,25,2);},
            [](auto& a){rectangle(a,25,672,512,29,32);},
            [](auto& a){rectangle(a,1227,673,26,26,32);},
            [](auto& a){rectangle(a,21,23,28,33,32);}
        };
        for(const auto& mutate:invalid){
            auto pixels=partial;mutate(pixels);LearnedHudRedundancy h;train(h,image,clean);
            const auto& denied=h.observe(view(pixels),primary,raw(),frame(3,1200),true);
            check(!denied.current_candidate&&denied.auxiliary.valid&&!denied.merged_score.candidate,
                "partial-boundary branch borrowed prior/forged evidence for blank/solid/bareline/three-window/missing-anchor pixels");
        }
        auto over_budget=primary;over_budget.score.boundary_components=9;
        LearnedHudRedundancy bounded;train(bounded,image,clean);
        check(!bounded.observe(view(partial),over_budget,raw(),frame(3,1200),true).current_candidate,
            "partial-boundary branch admitted an out-of-budget primary boundary count");
        LearnedHudRedundancy stale;train(stale,image,clean);auto expired=frame(3,1200);expired.processed_ns+=499*ms;
        check(!stale.observe(view(partial),primary,raw(),expired,true).current_candidate,
            "partial-boundary branch renewed a 500ms-old observation");
        auto denied_raw=raw();denied_raw.state=ObservedGameState::paused;
        LearnedHudRedundancy paused;train(paused,image,clean);
        check(!paused.observe(view(partial),primary,denied_raw,frame(3,1200),true).current_candidate,
            "partial-boundary branch overrode a current pause");
    }
    ++groups;{
        HudGlyphEvidenceProducer wide(view(image),std::string(64,'a'),"synthetic-v4",{},HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4);
        const auto primary=wide.observe_features(view(image));
        check(primary.hud_candidate&&primary.reason=="experimental_shared_difficulty_subpixel_HUD_candidate_only","v4 reference policy reason missing");
        LearnedHudRedundancy h;train(h,image,primary);
        auto forged=primary;forged.reason="experimental_shared_IN_subpixel_HUD_candidate_only";
        LearnedHudRedundancy wrong;
        check(wrong.observe(view(image),forged,raw(),frame(1,1000),false).support_frames==0,"v4 inherited IN teacher reason");
    }
    ++groups;{
        auto soft=image;rectangle(soft,21,23,28,33,32);
        for(const int x:{24,39}){rectangle(soft,x,26,7,25,255);rectangle(soft,x,51,7,1,202);}
        auto phase=soft;for(const int x:{24,39}){rectangle(phase,x,26,7,1,151);rectangle(phase,x,51,7,1,255);}
        HudGlyphEvidenceProducer v4(view(soft),std::string(64,'a'),"AT-pause-core-v4",{},HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4);
        const auto first=v4.observe_features(view(soft)),second=v4.observe_features(view(phase));
        check(first.hud_candidate&&second.hud_candidate,"v4 pause core teacher controls invalid");
        LearnedHudRedundancy h;
        check(!h.observe(view(soft),first,raw(),frame(1,1000),false).ready,"v4 core learned from a single frame");
        check(!h.observe(view(phase),second,raw(),frame(2,1059),false).ready,"v4 core shortened the 60ms support gap");
        const auto& support=h.observe(view(phase),second,raw(),frame(3,1060),false);
        check(support.ready&&support.support_frames==2&&support.auxiliary.hud_candidate&&
            support.auxiliary.fixed_glyphs[0].foreground_pixels==230,"v4 auxiliary teacher retained unstable pause edge");
        auto crossing=occluded(phase);const auto p=v4.observe_features(view(crossing));
        check(!p.hud_candidate&&p.reason=="score_digit_structure_missing","v4 pause core occlusion control invalid");
        check(h.observe(view(crossing),p,raw(),frame(4,1200),true).current_candidate,"v4 auxiliary current pause core rejected edge phase");
        auto missing=crossing;rectangle(missing,25,27,5,1,32);
        check(!h.observe(view(missing),p,raw(),frame(5,1300),true).current_candidate,"v4 auxiliary admitted real core loss with forged primary metrics");
        auto no_pause=phase;rectangle(no_pause,21,23,28,33,32);
        LearnedHudRedundancy invalid;
        check(invalid.observe(view(no_pause),v4.observe_features(view(no_pause)),raw(),frame(1,1000),false).support_frames==0,
            "v4 learned reference without actual pause core");
    }
    ++groups;{
        auto at_reference=image;
        for(int y=673;y<699;++y)for(int x=1144;x<1225;++x) {
            const auto at=static_cast<std::size_t>(y*1280+x)*3;
            if(at_reference[at]>195)at_reference[at]=at_reference[at+1]=at_reference[at+2]=255;
        }
        HudGlyphEvidenceProducer v4(view(at_reference),std::string(64,'a'),"AT-prefix-mismatch-v4",{},HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4);
        auto at_image=at_reference;rectangle(at_image,1157,677,1,17,32);
        const auto at_clean=v4.observe_features(view(at_image));
        check(at_clean.hud_candidate&&at_clean.fixed_glyphs[1].subpixel_tolerant,"AT raster-phase teacher control invalid");
        auto mismatch=at_image;rectangle(mismatch,1146,681,3,3);
        const auto primary=v4.observe_features(view(mismatch));
        check(!primary.hud_candidate&&primary.reason=="fixed_glyph_or_negative_space_missing"&&
            !primary.fixed_glyphs[1].glyph_present&&primary.fixed_glyphs[1].foreground_white_fraction==1&&
            primary.fixed_glyphs[1].negative_space_present&&primary.fixed_glyphs[1].literal_components==6&&
            primary.score.glyph_structure_candidate,"AT primary prefix mismatch control invalid");
        LearnedHudRedundancy h;train(h,at_image,at_clean);
        const auto& admitted=h.observe(view(mismatch),primary,raw(),frame(3,1200),true);
        check(admitted.current_candidate&&admitted.auxiliary.valid&&admitted.auxiliary.hud_candidate&&
            admitted.auxiliary.fixed_glyphs[0].glyph_present&&admitted.auxiliary.fixed_glyphs[1].glyph_present&&
            admitted.auxiliary.title_glyph.glyph_present&&admitted.auxiliary.score.glyph_structure_candidate&&
            admitted.reason=="current_learned_full_HUD_after_primary_prefix_mismatch",
            "AT prefix mismatch did not independently verify current full learned HUD");
        const std::vector<std::function<void(Image&)>> missing_current={
            [](auto& a){rectangle(a,25,672,512,29,32);},
            [](auto& a){rectangle(a,1227,673,26,26,32);},
            [](auto& a){rectangle(a,21,23,28,33,32);},
            [](auto& a){rectangle(a,1076,20,180,40,32);},
            [](auto& a){rectangle(a,1084+3*24,24,23,33,32);},
            [](auto& a){rectangle(a,1144,673,109,26,32);},
            [](auto& a){rectangle(a,1144,673,109,26);},
            [](auto& a){for(auto& value:a)value=static_cast<std::uint8_t>(value/2);}
        };
        for(const auto& mutate:missing_current) {
            auto pixels=mismatch;mutate(pixels);LearnedHudRedundancy missing;train(missing,at_image,at_clean);
            const auto& denied=missing.observe(view(pixels),primary,raw(),frame(3,1200),true);
            check(!denied.current_candidate&&denied.auxiliary.valid&&
                denied.reason=="current_auxiliary_full_HUD_missing_after_primary_prefix_mismatch",
                "forged primary mismatch overrode missing CURRENT title/difficulty/pause/score pixels or hid auxiliary diagnostics");
        }
        const std::vector<std::function<void(HudGlyphEvidence&)>> invalid_primary={
            [](auto& p){p.valid=false;},[](auto& p){p.denied_state_match=true;},[](auto& p){p.title_required=true;},
            [](auto& p){p.fixed_glyphs[0].glyph_present=false;},[](auto& p){p.fixed_glyphs[1].subpixel_tolerant=false;},
            [](auto& p){p.fixed_glyphs[1].foreground_white_fraction=.97;},[](auto& p){p.fixed_glyphs[1].foreground_mean_error=8.1;},
            [](auto& p){p.fixed_glyphs[1].negative_space_bright_fraction=.36;},[](auto& p){p.fixed_glyphs[1].local_contrast=39;},
            [](auto& p){p.fixed_glyphs[1].negative_space_present=false;},[](auto& p){p.fixed_glyphs[1].foreground_pixels=0;},
            [](auto& p){p.fixed_glyphs[1].unexpected_white_fraction=std::numeric_limits<double>::quiet_NaN();},
            [](auto& p){p.score.supported_cells=6;},[](auto& p){p.score.boundary_components=1;},
            [](auto& p){p.score.glyph_structure_candidate=false;}
        };
        for(const auto& mutate:invalid_primary) {
            auto invalid=primary;mutate(invalid);LearnedHudRedundancy guarded;train(guarded,at_image,at_clean);
            check(!guarded.observe(view(mismatch),invalid,raw(),frame(3,1200),true).current_candidate,
                "AT prefix mismatch ignored strict primary core/negative/score guards");
        }
        LearnedHudRedundancy unready;
        check(!unready.observe(view(mismatch),primary,raw(),frame(1,1000),true).current_candidate,"AT prefix mismatch admitted unready model");
        LearnedHudRedundancy unarmed;train(unarmed,at_image,at_clean);
        check(!unarmed.observe(view(mismatch),primary,raw(),frame(3,1200),false).current_candidate,"AT prefix mismatch admitted unarmed model");
        for(const auto state:{ObservedGameState::paused,ObservedGameState::result}) {
            LearnedHudRedundancy denied;train(denied,at_image,at_clean);auto screen=raw();screen.state=state;
            check(!denied.observe(view(mismatch),primary,screen,frame(3,1200),true).current_candidate,"AT prefix mismatch overrode denied current state");
            screen=raw();screen.scores.push_back({state,"same-frame-denied",0,true});
            check(!denied.observe(view(mismatch),primary,screen,frame(4,1300),true).current_candidate,"AT prefix mismatch overrode denied anchor match");
        }
        LearnedHudRedundancy changed;train(changed,at_image,at_clean);auto other_context=frame(3,1200);++other_context.epoch;
        check(!changed.observe(view(mismatch),primary,raw(),other_context,true).current_candidate&&!changed.status().ready,
            "AT prefix mismatch reused a model from another context");
        LearnedHudRedundancy in_model;train(in_model,image,clean);
        check(!in_model.observe(view(mismatch),primary,raw(),frame(3,1200),true).current_candidate,
            "AT prefix mismatch borrowed an IN learned difficulty template");
        auto in_primary=primary;in_primary.policy=HudGlyphPolicy::shared_in_subpixel_prefix_score_v3;
        LearnedHudRedundancy in_only;train(in_only,image,clean);
        check(!in_only.observe(view(mismatch),in_primary,raw(),frame(3,1200),true).current_candidate,
            "IN prefix mismatch fallback accepted pixels missing its own full learned difficulty");
    }
    ++groups;{
        // First establish a real one-pixel raster phase: the exact gray mask
        // cannot retain its original edge column, while the bounded literal
        // policy can prove all five strokes. A small bridge on an otherwise
        // exact gray reference would still pass the preserved v2 path and
        // would not exercise the prefix-mismatch route at all.
        auto in_reference=image;
        for(int y=673;y<699;++y)for(int x=1152;x<1225;++x){
            const auto at=static_cast<std::size_t>(y*1280+x)*3;
            if(in_reference[at]>195)in_reference[at]=in_reference[at+1]=in_reference[at+2]=255;
        }
        HudGlyphEvidenceProducer in_producer(view(in_reference),std::string(64,'b'),"IN-prefix-mismatch-v3",{},
            HudGlyphPolicy::shared_in_subpixel_prefix_score_v3);
        auto in_image=in_reference;rectangle(in_image,1157,677,1,17,32);
        const auto in_clean=in_producer.observe_features(view(in_image));
        check(in_clean.hud_candidate&&in_clean.fixed_glyphs[1].subpixel_tolerant&&
            in_clean.fixed_glyphs[1].literal_components==5,"IN raster-phase teacher control invalid");
        // Merge two CURRENT prefix strokes but retain every teacher stroke.
        // Full title/level/pause/score must be independently measured again.
        // These synthetic pixels do not replay the Frozen003 end screen.
        auto mismatch=in_image;rectangle(mismatch,1157,684,6,2);
        const auto primary=in_producer.observe_features(view(mismatch));
        check(primary.valid&&!primary.hud_candidate&&primary.reason=="fixed_glyph_or_negative_space_missing"&&
            !primary.fixed_glyphs[1].glyph_present&&primary.fixed_glyphs[1].literal_components==4&&
            primary.fixed_glyphs[1].foreground_white_fraction==1&&primary.fixed_glyphs[1].negative_space_present&&
            primary.score.glyph_structure_candidate,"IN merged-prefix mismatch control invalid");
        LearnedHudRedundancy h;train(h,in_image,in_clean);
        const auto& admitted=h.observe(view(mismatch),primary,raw(),frame(3,1200),true);
        check(admitted.current_candidate&&admitted.auxiliary.valid&&admitted.auxiliary.hud_candidate&&
            admitted.auxiliary.fixed_glyphs[0].glyph_present&&admitted.auxiliary.fixed_glyphs[1].glyph_present&&
            admitted.auxiliary.title_glyph.glyph_present&&admitted.auxiliary.score.glyph_structure_candidate&&
            admitted.reason=="current_learned_full_HUD_after_primary_prefix_mismatch",
            "IN prefix mismatch did not independently verify current full learned HUD");
        check(!admitted.auxiliary.live_permitted&&!admitted.auxiliary.timing_qualified&&
            !admitted.auxiliary.source_pixel_age_qualified,"IN alternate route manufactured permission or timing proof");
        const std::vector<std::function<void(Image&)>> missing_current={
            [](auto& a){rectangle(a,25,672,512,29,32);},
            [](auto& a){rectangle(a,1227,673,26,26,32);},
            [](auto& a){rectangle(a,21,23,28,33,32);},
            [](auto& a){rectangle(a,1076,20,180,40,32);},
            [](auto& a){rectangle(a,1084+3*24,24,23,33,32);},
            [](auto& a){rectangle(a,1152,673,101,26,32);},
            [](auto& a){rectangle(a,1152,673,101,26);}
        };
        for(const auto& mutate:missing_current){
            auto pixels=mismatch;mutate(pixels);LearnedHudRedundancy missing;train(missing,in_image,in_clean);
            const auto& denied=missing.observe(view(pixels),primary,raw(),frame(3,1200),true);
            check(!denied.current_candidate&&denied.auxiliary.valid&&
                denied.reason=="current_auxiliary_full_HUD_missing_after_primary_prefix_mismatch",
                "forged IN primary metrics replaced missing CURRENT title/difficulty/pause/seven-digit evidence");
        }
        for(const auto state:{ObservedGameState::paused,ObservedGameState::result}){
            LearnedHudRedundancy denied;train(denied,in_image,in_clean);auto screen=raw();screen.state=state;
            check(!denied.observe(view(mismatch),primary,screen,frame(3,1200),true).current_candidate,
                "IN prefix mismatch overrode a current denied screen");
            screen=raw();screen.scores.push_back({state,"same-frame-denied",0,true});
            check(!denied.observe(view(mismatch),primary,screen,frame(4,1300),true).current_candidate,
                "IN prefix mismatch overrode a same-frame denied anchor");
        }
        LearnedHudRedundancy stale;train(stale,in_image,in_clean);auto expired=frame(3,1200);expired.processed_ns+=499*ms;
        check(!stale.observe(view(mismatch),primary,raw(),expired,true).current_candidate,
            "IN alternate route renewed an expired actual frame");
        LearnedHudRedundancy unready;
        check(!unready.observe(view(mismatch),primary,raw(),frame(1,1200),true).current_candidate&&
            !unready.status().ready,"IN prefix mismatch trained after arm");
        LearnedHudRedundancy wrong_policy;train(wrong_policy,in_image,in_clean);auto mixed=primary;
        mixed.policy=HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4;
        check(!wrong_policy.observe(view(mismatch),mixed,raw(),frame(3,1200),true).current_candidate,
            "IN alternate route reused its template for another primary policy");
    }
    ++groups;{
        for(const auto thickness:{1,2,3})for(const auto width:{145,180}){
            const auto pixels=merged_score_line(image,thickness,width);
            const auto primary=producer.observe_features(view(pixels));
            check(primary.valid&&!primary.hud_candidate&&primary.reason=="score_digit_structure_missing"&&
                primary.score.boundary_components==1&&!primary.score.glyph_structure_candidate&&
                primary.score.components<=2,"merged-score primary control invalid");
            LearnedHudRedundancy h;train(h,image,clean);
            const auto& result=h.observe(view(pixels),primary,raw(),frame(3,1200),true);
            check(result.current_candidate&&result.auxiliary.valid&&result.auxiliary.hud_candidate&&
                result.auxiliary.fixed_glyphs[0].glyph_present&&result.auxiliary.fixed_glyphs[1].glyph_present&&
                result.auxiliary.title_glyph.glyph_present&&result.merged_score.candidate&&
                result.merged_score.spanned_windows>=4&&result.merged_score.textured_windows>=4&&
                result.reason=="current_learned_three_anchors_with_merged_score_boundary",
                "CURRENT three anchors and observed merged-score vertical texture rejected");
            check(!result.auxiliary.score.glyph_structure_candidate&&result.auxiliary.score.glyphs.size()<=2&&
                !result.auxiliary.live_permitted&&!result.auxiliary.timing_qualified&&
                !result.auxiliary.source_pixel_age_qualified,
                "merged-score witness invented separate digits or timing/input qualification");
            const auto& clean_again=h.observe(view(image),clean,raw(),frame(4,1300),true);
            check(!clean_again.current_candidate&&!clean_again.merged_score.candidate&&
                clean_again.merged_score.reason=="not_measured"&&!clean_again.auxiliary.valid,
                "clean primary retained stale merged-score/current auxiliary evidence");
        }
    }
    ++groups;{
        // The line enters at the top/left ROI boundary. Its connected text
        // footprint is taller than a single32px font glyph, but all observed
        // score pixels and the complete merged bbox stay inside the40px ROI.
        for(const auto end_y:{44,52,55,58}){
            const auto pixels=upper_boundary_score_line(image,end_y);
            const auto primary=producer.observe_features(view(pixels));
            if(!(primary.valid&&!primary.hud_candidate&&primary.reason=="score_digit_structure_missing"&&
                 primary.score.boundary_components==1))std::cerr<<"diagonal control end="<<end_y<<" valid="<<primary.valid<<
                " HUD="<<primary.hud_candidate<<" reason="<<primary.reason<<" boundary="<<primary.score.boundary_components<<
                " CC="<<primary.score.components<<" cells="<<primary.score.supported_cells<<'\n';
            check(primary.valid&&!primary.hud_candidate&&primary.reason=="score_digit_structure_missing"&&
                primary.score.boundary_components==1,"upper-boundary diagonal score control invalid");
            LearnedHudRedundancy h;train(h,image,clean);
            const auto& result=h.observe(view(pixels),primary,raw(),frame(3,1200),true);
            const auto expected_height=std::max(52,end_y+1)-20+1;
            if(!result.current_candidate)std::cerr<<"diagonal merge end="<<end_y<<" reason="<<result.reason<<
                " evidence="<<result.merged_score.reason<<" height="<<result.merged_score.component.height<<
                " span="<<result.merged_score.spanned_windows<<" texture="<<result.merged_score.textured_windows<<'\n';
            check(result.current_candidate&&result.merged_score.candidate&&
                result.merged_score.component.y==20&&result.merged_score.component.height==expected_height&&
                result.merged_score.component.height>=33&&result.merged_score.component.height<=40&&
                result.merged_score.spanned_windows>=4&&result.merged_score.textured_windows>=4,
                "bounded33..40px line-plus-text footprint was mistaken for one oversized font glyph");
            check(!result.auxiliary.score.glyph_structure_candidate&&result.auxiliary.fixed_glyphs[0].glyph_present&&
                result.auxiliary.fixed_glyphs[1].glyph_present&&result.auxiliary.title_glyph.glyph_present,
                "merged ROI expansion invented clean digits or skipped a CURRENT fixed anchor");
        }
        // A raw component cannot report >40px after fixed-ROI clipping.
        // Oversized source blocks/frames must still fail texture/occupancy;
        // the rule never expands the ROI or interprets them as font glyphs.
        const auto primary=producer.observe_features(view(upper_boundary_score_line(image,44)));
        for(const bool hollow:{false,true}){
            auto pixels=image;rectangle(pixels,1076,20,180,40,32);rectangle(pixels,1076,18,180,44);
            if(hollow)rectangle(pixels,1079,21,174,38,32);
            LearnedHudRedundancy h;train(h,image,clean);
            const auto& result=h.observe(view(pixels),primary,raw(),frame(3,1200),true);
            check(!result.current_candidate&&!result.merged_score.candidate,
                "oversized source block/frame borrowed the bounded merged-score exception");
        }
        auto tall=image;rectangle(tall,1076,20,180,40,32);
        for(int i=0;i<7;++i){const auto x=1084+i*24;rectangle(tall,x,24,17,33);rectangle(tall,x+3,27,11,27,32);}
        LearnedHudRedundancy font;
        const auto& rejected=font.observe(view(tall),clean,raw(),frame(1,1000),false);
        check(!rejected.ready&&rejected.reason=="reference_full_anchors_or_clean_score_unsupported"&&font.reference_rgb().empty(),
            "merged component ROI bound changed the original32px normal font requirement");
    }
    ++groups;{
        const auto pixels=merged_score_line(image);
        const auto primary=producer.observe_features(view(pixels));
        const std::vector<std::function<void(Image&)>> missing_anchors={
            [](auto& a){rectangle(a,25,672,512,29,32);},
            [](auto& a){rectangle(a,1227,673,26,26,32);},
            [](auto& a){rectangle(a,21,23,28,33,32);},
            [](auto& a){rectangle(a,1152,673,101,26,32);},
            [](auto& a){for(auto& value:a)value=static_cast<std::uint8_t>(value/2);}
        };
        for(const auto& mutate:missing_anchors){
            auto missing=pixels;mutate(missing);LearnedHudRedundancy h;train(h,image,clean);
            const auto& denied=h.observe(view(missing),primary,raw(),frame(3,1200),true);
            check(!denied.current_candidate&&denied.auxiliary.valid&&!denied.merged_score.candidate&&
                denied.merged_score.reason=="current_full_learned_anchors_missing",
                "merged-score line borrowed a missing CURRENT title/difficulty/level/pause anchor");
        }
        const std::vector<std::function<void(Image&)>> invalid_score={
            [](auto& a){rectangle(a,1076,20,180,40,32);},
            [](auto& a){rectangle(a,1076,20,180,40);},
            [](auto& a){rectangle(a,1076,20,180,40,32);rectangle(a,1076,40,180,2);},
            [](auto& a){rectangle(a,1076,20,180,40,32);rectangle(a,1076,28,180,25);},
            [](auto& a){rectangle(a,1076,20,180,40,32);rectangle(a,1076,28,180,25);rectangle(a,1079,31,174,19,32);},
            [](auto& a){rectangle(a,1076,20,180,40,32);rectangle(a,1076,28,180,25);rectangle(a,1079,31,174,19,32);rectangle(a,1076,40,180,2);},
            [](auto& a){rectangle(a,1076,20,180,40,32);for(int i=0;i<7;++i)rectangle(a,1084+i*24,28,17,25);rectangle(a,1076,40,180,2);}
        };
        for(const auto& mutate:invalid_score){
            auto invalid=pixels;mutate(invalid);LearnedHudRedundancy h;train(h,image,clean);
            const auto& denied=h.observe(view(invalid),primary,raw(),frame(3,1200),true);
            check(!denied.current_candidate&&denied.auxiliary.valid&&!denied.merged_score.candidate&&
                denied.reason=="current_auxiliary_anchors_or_merged_score_texture_missing",
                "forged primary admitted blank/solid/bareline/hollow-frame/filled score pixels");
        }
        for(const auto state:{ObservedGameState::paused,ObservedGameState::result}){
            LearnedHudRedundancy denied;train(denied,image,clean);auto screen=raw();screen.state=state;
            check(!denied.observe(view(pixels),primary,screen,frame(3,1200),true).current_candidate,
                "merged score overrode a current denied screen");
            screen=raw();screen.scores.push_back({state,"same-frame-denied",0,true});
            check(!denied.observe(view(pixels),primary,screen,frame(4,1300),true).current_candidate,
                "merged score overrode a same-frame denied anchor");
        }
        LearnedHudRedundancy stale;train(stale,image,clean);auto expired=frame(3,1200);expired.processed_ns+=499*ms;
        check(!stale.observe(view(pixels),primary,raw(),expired,true).current_candidate&&
            !stale.status().merged_score.candidate,"merged score renewed an expired actual frame");
        LearnedHudRedundancy source;train(source,image,clean);auto invalid=frame(3,1200);invalid.source_valid=false;
        check(!source.observe(view(pixels),primary,raw(),invalid,true).current_candidate,"merged score ignored source invalidation");
        LearnedHudRedundancy changed;train(changed,image,clean);auto other=frame(3,1200);++other.epoch;
        check(!changed.observe(view(pixels),primary,raw(),other,true).current_candidate&&!changed.status().ready,
            "merged score reused a reference from a previous context");
        LearnedHudRedundancy unready;
        check(!unready.observe(view(pixels),primary,raw(),frame(1,1000),true).current_candidate&&
            !unready.observe(view(image),clean,raw(),frame(2,1100),false).ready,
            "merged score learned or retrained after arm");
        LearnedHudRedundancy wrong_policy;train(wrong_policy,image,clean);auto mismatched=primary;
        mismatched.policy=HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4;
        check(!wrong_policy.observe(view(pixels),mismatched,raw(),frame(3,1200),true).current_candidate,
            "merged score borrowed a different primary policy reference");
    }
    ++groups;{
        check(clean.valid&&clean.hud_candidate&&clean.score.supported_cells==7,"synthetic clean v3 control invalid");
        check(occlusion.valid&&!occlusion.hud_candidate&&occlusion.reason=="score_digit_structure_missing"&&
            occlusion.score.boundary_components>0&&occlusion.score.glyphs.size()>=4,"score-only occlusion control invalid");
        LearnedHudRedundancy h;train(h,image,clean);
        check(h.status().reference_sha256==digest(h.reference_rgb())&&h.status().support_sha256==digest(h.support_rgb()),"learned hashes do not bind actual bytes");
        check(h.reference_rgb().size()==image.size()&&h.support_rgb().size()==image.size()&&h.reference_stride()==3840&&h.support_stride()==3840,"actual RGB proof missing or unbounded");
        check(std::equal(h.reference_rgb().begin(),h.reference_rgb().end(),image.begin()),"reference pixels altered");
        check(h.status().reference_frame.sequence==1&&h.status().support_frame.sequence==2&&h.status().title_region.x==25&&
            h.status().title_region.width<512&&h.status().title_region.height==29,"template provenance or right blank margin wrong");
        const auto& r=h.observe(view(blocked),occlusion,raw(),frame(3,200000),true);
        if(!r.current_candidate)std::cerr<<"fallback reason="<<r.reason<<" aux="<<r.auxiliary.reason<<" rawglyph="<<r.auxiliary.score.glyphs.size()<<"\n";
        check(r.current_candidate&&r.auxiliary.valid&&r.auxiliary.hud_candidate&&r.plausible_score_components>=4&&r.score_boundary_components>0,"fresh current three-anchor score occlusion rejected");
        check(!r.auxiliary.live_permitted&&!r.auxiliary.timing_qualified&&!r.auxiliary.source_pixel_age_qualified,"helper manufactured input or timing permission");
        const auto& admitted=h.observe(view(image),clean,raw(),frame(4,200100),true);
        check(!admitted.current_candidate&&!admitted.auxiliary.valid&&admitted.reason=="primary_admitted"&&
            admitted.plausible_score_components==0&&admitted.score_boundary_components==0,"clean primary remeasured auxiliary or leaked old current metrics");
    }
    ++groups;{
        LearnedHudRedundancy h;h.observe(view(image),clean,raw(),frame(1,1000),false);
        check(!h.observe(view(image),clean,raw(),frame(2,1059),false).ready,"support earlier than 60ms accepted");
        check(h.observe(view(image),clean,raw(),frame(3,1060),false).ready,"60ms support boundary refused");
        LearnedHudRedundancy late;late.observe(view(image),clean,raw(),frame(1,1000),false);
        check(!late.observe(view(image),clean,raw(),frame(2,1500),false).ready&&late.status().support_frames<=1,"expired old frame used as support");
        LearnedHudRedundancy duplicate;duplicate.observe(view(image),clean,raw(),frame(1,1000),false);
        check(!duplicate.observe(view(image),clean,raw(),frame(1,1060),false).ready,"duplicate sequence trained support");
        check(!duplicate.observe(view(image),clean,raw(),frame(2,1000),false).ready,"duplicate receipt trained support");
        auto bad_support=image;rectangle(bad_support,1227,673,26,26,32);
        LearnedHudRedundancy full;full.observe(view(image),clean,raw(),frame(1,1000),false);
        check(!full.observe(view(bad_support),clean,raw(),frame(2,1060),false).ready,"support missing full IN level digits learned");
    }
    ++groups;{
        const std::vector<std::function<void(Image&)>> bad_pixels={
            [](auto& a){rectangle(a,1076,20,180,40,32);},
            [](auto& a){rectangle(a,1076,20,180,40);},
            [](auto& a){rectangle(a,21,23,28,33,32);},
            [](auto& a){rectangle(a,25,672,512,29,32);},
            [](auto& a){rectangle(a,1227,673,26,26,32);},
            [](auto& a){rectangle(a,1152,673,73,26,32);},
            [](auto& a){for(auto& x:a)x=static_cast<std::uint8_t>(x/2);}
        };
        for(const auto& mutate:bad_pixels){auto a=blocked;mutate(a);LearnedHudRedundancy h;train(h,image,clean);
            check(!h.observe(view(a),occlusion,raw(),frame(3,1200),true).current_candidate,"forged primary fields overrode missing current raw pixels");}
        auto six=occlusion;six.score.boundary_components=0;LearnedHudRedundancy h;train(h,image,clean);
        check(!h.observe(view(blocked),six,raw(),frame(3,1200),true).current_candidate,"ordinary missing score digits received boundary fallback");
        auto few=occlusion;few.score.glyphs.resize(3);
        const auto& current=h.observe(view(blocked),few,raw(),frame(4,1300),true);
        check(current.current_candidate&&current.auxiliary.valid&&current.merged_score.textured_windows>=4,
            "raw score evidence was replaced by a forged primary component count");
        auto spoof=occlusion;spoof.fixed_glyphs[0].glyph_present=false;
        check(!h.observe(view(blocked),spoof,raw(),frame(5,1400),true).current_candidate,"reason-only pause spoof passed");
        spoof=occlusion;spoof.difficulty_alignment_unique_family=false;
        check(!h.observe(view(blocked),spoof,raw(),frame(6,1500),true).current_candidate,"ambiguous prefix alignment admitted");
        spoof=occlusion;spoof.reason="prefix_missing";
        check(!h.observe(view(blocked),spoof,raw(),frame(7,1600),true).current_candidate,"non-score failure used score fallback");
    }
    ++groups;{
        for(const auto state:{ObservedGameState::paused,ObservedGameState::result}){
            LearnedHudRedundancy h;train(h,image,clean);auto denied=raw();denied.state=state;
            check(!h.observe(view(blocked),occlusion,denied,frame(3,1200),true).current_candidate,"denied raw state overridden");
            denied=raw();denied.scores.push_back({state,"denied",0,true});denied.scores.push_back({ObservedGameState::playing,"playing",0,true});
            check(!h.observe(view(blocked),occlusion,denied,frame(4,1300),true).current_candidate,"ambiguous denied anchor overridden");
            LearnedHudRedundancy training;training.observe(view(image),clean,denied,frame(1,1000),false);
            check(training.reference_rgb().empty()&&training.status().support_frames==0,"denied state used to construct reference");
        }
        LearnedHudRedundancy h;train(h,image,clean);auto many=raw();many.scores.resize(9);
        check(!h.observe(view(blocked),occlusion,many,frame(3,1200),true).current_candidate,"unbounded raw match vector admitted");
    }
    ++groups;{
        const std::vector<std::function<void(LearnedHudFrame&)>> mutations={
            [](auto& f){f.sequence=0;},[](auto& f){f.epoch=0;},[](auto& f){f.generation=0;},[](auto& f){f.geometry_version=0;},
            [](auto& f){f.source_valid=false;},[](auto& f){f.source_id.clear();},[](auto& f){f.source_id="bad\nsource";},
            [](auto& f){f.source_id=std::string(513,'a');},[](auto& f){f.rotation=-1;},[](auto& f){f.rotation=4;},
            [](auto& f){f.received_ns=-1;},[](auto& f){f.processed_ns=f.received_ns-1;},
            [](auto& f){f.processed_ns=f.received_ns+500*ms;}
        };
        for(const auto& mutate:mutations){LearnedHudRedundancy h;train(h,image,clean);auto m=frame(3,1200);mutate(m);
            check(!h.observe(view(blocked),occlusion,raw(),m,true).current_candidate&&!h.status().auxiliary.valid,"invalid frame metadata admitted or retained stale auxiliary");}
        for(const auto& mutate:mutations){LearnedHudRedundancy h;auto m=frame(1,1000);mutate(m);
            check(!h.observe(view(image),clean,raw(),m,false).ready&&h.reference_rgb().empty(),"invalid metadata trained reference");}
        LearnedHudRedundancy h;train(h,image,clean);auto f=frame(3,1200);f.processed_ns=f.received_ns+499*ms;
        check(h.observe(view(blocked),occlusion,raw(),f,true).current_candidate,"freshness below 500ms refused");
        check(!h.observe(view(blocked),occlusion,raw(),frame(4,1200),true).current_candidate,"same receipt reused with new sequence");
        check(!h.observe(view(blocked),occlusion,raw(),frame(2,1800),true).current_candidate,"sequence rollback admitted");
    }
    ++groups;{
        const std::vector<std::function<void(LearnedHudFrame&)>> contexts={
            [](auto& m){m.epoch=2;},[](auto& m){m.generation=2;},[](auto& m){m.geometry_version=2;},
            [](auto& m){m.source_id="new-source";},[](auto& m){m.rotation=2;}
        };
        for(const auto& mutate:contexts){LearnedHudRedundancy h;train(h,image,clean);auto m=frame(3,1200);mutate(m);
            check(!h.observe(view(blocked),occlusion,raw(),m,true).ready&&h.reference_rgb().empty()&&h.support_rgb().empty(),"context change retained learned reference");
            m.sequence=4;m.received_ns+=100*ms;m.processed_ns+=100*ms;
            check(!h.observe(view(image),clean,raw(),m,false).ready&&h.reference_rgb().empty(),"post-arm context change allowed retraining");}
        LearnedHudRedundancy h;train(h,image,clean);h.invalidate(2);
        check(!h.observe(view(image),clean,raw(),frame(3,1200),false).ready&&h.reference_rgb().empty(),"explicit invalidation accepted stale epoch");
        auto m=frame(4,1300);m.epoch=2;h.observe(view(image),clean,raw(),m,true);h.invalidate(3);m=frame(5,1400);m.epoch=3;
        check(!h.observe(view(image),clean,raw(),m,false).ready&&h.reference_rgb().empty(),"invalidate reset no-post-arm-training latch");
    }
    ++groups;{
        const std::vector<std::function<void(Image&)>> bad_title={
            [](auto& a){rectangle(a,25,672,512,29,32);},
            [](auto& a){rectangle(a,25,672,512,29,32);rectangle(a,30,677,14,18);},
            [](auto& a){rectangle(a,25,672,512,29,32);rectangle(a,30,685,80,2);},
            [](auto& a){rectangle(a,25,672,512,29,32);rectangle(a,536,677,1,18);},
            [](auto& a){rectangle(a,25,672,512,29,32);for(int y=674;y<699;y+=2)for(int x=27;x<535;x+=2)rectangle(a,x,y,1,1);}
        };
        for(const auto& mutate:bad_title){auto a=image;mutate(a);LearnedHudRedundancy h;
            check(!h.observe(view(a),clean,raw(),frame(1,1000),false).ready&&h.reference_rgb().empty(),"unsupported/solid/clipped/noisy title learned");}
        auto single=image;rectangle(single,25,672,512,29,32);
        rectangle(single,35,677,3,18);rectangle(single,35,677,14,3);rectangle(single,35,692,14,3);
        LearnedHudRedundancy one;train(one,single,clean);
        check(one.status().title_foreground_pixels>=32&&one.status().title_region.width<40,"single connected title unnecessarily requires three glyphs");
        LearnedHudRedundancy budget;auto no_title=image;rectangle(no_title,25,672,512,29,32);
        for(unsigned i=0;i<10;++i)budget.observe(view(no_title),clean,raw(),frame(i+1,1000+static_cast<std::int64_t>(i)*100),false);
        check(budget.status().construction_attempts==8&&budget.reference_rgb().empty(),"candidate construction budget not bounded");
        check(!budget.observe(view(image),clean,raw(),frame(11,2100),false).ready&&budget.status().construction_attempts==8,"exhausted candidate budget retried");
        LearnedHudRedundancy cadence;cadence.observe(view(no_title),clean,raw(),frame(1,1000),false);
        cadence.observe(view(no_title),clean,raw(),frame(2,1099),false);
        check(cadence.status().construction_attempts==1,"construction cadence under 100ms admitted");
        cadence.observe(view(no_title),clean,raw(),frame(3,1100),false);
        check(cadence.status().construction_attempts==2,"construction cadence exactly 100ms refused");
    }
    ++groups;{
        auto forged=clean;forged.valid=false;LearnedHudRedundancy h;
        check(h.observe(view(image),forged,raw(),frame(1,1000),false).support_frames==0,"invalid primary reason spoof trained");
        auto blank=image;rectangle(blank,1076,20,180,40,32);
        check(h.observe(view(blank),clean,raw(),frame(2,1100),false).support_frames==0&&h.reference_rgb().empty(),"forged primary clean score trained blank source pixels");
        LearnedHudRedundancy late;late.observe(view(image),clean,raw(),frame(1,1000),true);
        check(late.reference_rgb().empty()&&!late.observe(view(image),clean,raw(),frame(2,1100),false).ready,"first armed call trained or permitted later training");
        LearnedHudRedundancy geometry;train(geometry,image,clean);auto bad=view(blocked);bad.stride=3841;
        check(!geometry.observe(bad,occlusion,raw(),frame(3,1200),true).ready&&geometry.reference_rgb().empty(),"invalid image geometry retained template");
    }
    std::cout<<groups<<" groups / "<<checks<<" learned HUD redundancy checks passed; offline synthetic pixels only.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}}
