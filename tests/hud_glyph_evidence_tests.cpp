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
        HudGlyphEvidenceProducer shared(view(original),std::string(64,'c'),"shared-IN-candidate",{},
            HudGlyphPolicy::shared_in_prefix_score_v1);
        const auto common=shared.observe(view(original),identity);
        check(shared.policy()==HudGlyphPolicy::shared_in_prefix_score_v1&&common.hud_candidate&&
              !common.title_required&&common.difficulty_alignment_candidates==1&&common.difficulty_alignment_x==0,
              "explicit shared-IN policy lost candidate/prefix alignment metadata");
        auto other_title=original;rectangle(other_title,25,672,160,29,32);
        rectangle(other_title,240,675,100,20);
        check(shared.observe(view(other_title),identity).hud_candidate&&!producer.observe(view(other_title),identity).hud_candidate,
              "shared-IN policy unexpectedly required the source title or changed the default policy");
        auto missing_level=original;rectangle(missing_level,1225,673,28,26,32);
        check(shared.observe(view(missing_level),identity).hud_candidate,"shared prefix included a numeric level glyph");
        for(const int dx:{-8,8}) {
            auto shifted=original;rectangle(shifted,1140,673,116,26,32);
            for(const int x:{1156,1180,1204}) {
                rectangle(shifted,x+dx,677,3,18);rectangle(shifted,x+dx,677,14,3);rectangle(shifted,x+dx,692,14,3);
            }
            const auto aligned=shared.observe(view(shifted),identity);
            check(aligned.hud_candidate&&aligned.difficulty_alignment_x==dx&&aligned.difficulty_alignment_candidates==1,
                  "bounded shared prefix translation failed");
        }
        auto wrong_prefix=original;rectangle(wrong_prefix,1140,673,116,26,32);
        check(!shared.observe(view(wrong_prefix),identity).hud_candidate,"missing common IN prefix accepted");
        auto filled_score=original;score_glyphs(filled_score,true);
        check(!shared.observe(view(filled_score),identity).hud_candidate,"shared-IN accepted filled score rectangles");
        auto line_score=original;rectangle(line_score,1076,24,180,2);
        check(!shared.observe(view(line_score),identity).hud_candidate,"shared-IN accepted score boundary line");
        auto fewer_digits=original;rectangle(fewer_digits,1080,24,23,33,32);
        check(!shared.observe(view(fewer_digits),identity).hud_candidate,"shared-IN accepted fewer than seven digits");
        auto solid_prefix=original;rectangle(solid_prefix,1140,673,116,26);
        check(!shared.observe(view(solid_prefix),identity).hud_candidate,"shared-IN ignored prefix negative space");
        auto dim=original;for(auto& p:dim)p=static_cast<std::uint8_t>(p/2);
        check(!shared.observe(view(dim),identity).hud_candidate,"shared-IN brightness-normalized dim overlay");
        for(const auto state:{ObservedGameState::paused,ObservedGameState::result}) {
            const auto denied=shared.observe(view(original),identity,state);
            check(!denied.hud_candidate&&denied.denied_state_match&&denied.reason=="calibrated_denied_state_has_priority",
                  "shared-IN overrode same-frame paused/result state");
        }
        const auto feature=shared.observe_features(view(original));
        check(feature.hud_candidate&&!feature.metadata_attached&&!feature.live_permitted&&!feature.timing_qualified&&
              !feature.source_pixel_age_qualified,"shared-IN manufactured frame/timing/input qualification");
        check(!shared.observe(view(original),HudFrameMetadata{}).valid,"shared-IN bypassed required full-API metadata");
        auto periodic=original;rectangle(periodic,1140,673,116,26,32);
        for(int x=1140;x<1240;x+=8)rectangle(periodic,x,677,2,18);
        HudGlyphEvidenceProducer ambiguous(view(periodic),std::string(64,'d'),"periodic-prefix",{},
            HudGlyphPolicy::shared_in_prefix_score_v1);
        const auto rejected=ambiguous.observe(view(periodic),identity);
        check(!rejected.hud_candidate&&rejected.difficulty_alignment_candidates>1&&
              rejected.reason=="shared_IN_prefix_alignment_ambiguous","shared-IN accepted multiple plausible prefix translations");
        bool rejected_reference=false;
        try {HudGlyphEvidenceProducer bad(view(filled_score),std::string(64,'c'),"bad-score",{},
                 HudGlyphPolicy::shared_in_prefix_score_v1);}
        catch(const std::invalid_argument&) {rejected_reference=true;}
        check(rejected_reference,"shared-IN learned a reference without score glyph structure");
    }
    ++groups;{
        auto longer_title=original;
        rectangle(longer_title,120,677,3,18);rectangle(longer_title,120,677,18,3);rectangle(longer_title,120,692,18,3);
        bool rejected=false;
        try {HudGlyphEvidenceProducer cropped(view(longer_title),std::string(64,'c'),"cropped-title");}
        catch(const std::invalid_argument&) {rejected=true;}
        check(rejected,"default title mask silently cropped a longer title at its boundary");
        const PixelRegion region{25,672,160,29};
        HudGlyphEvidenceProducer expanded(view(longer_title),std::string(64,'d'),"longer-title",region);
        const auto configured=expanded.title_region();
        check(configured.x==25&&configured.y==672&&configured.width==160&&configured.height==29,"configured title region not retained");
        const auto accepted=expanded.observe(view(longer_title),identity);
        check(accepted.hud_candidate&&accepted.title_glyph.glyph_present&&accepted.title_glyph.negative_space_present,
              "clean longer title rejected with explicit bounded reference ROI");
        check(accepted.fixed_glyphs[0].foreground_pixels==producer.observe(view(original),identity).fixed_glyphs[0].foreground_pixels&&
              accepted.fixed_glyphs[1].foreground_pixels==producer.observe(view(original),identity).fixed_glyphs[1].foreground_pixels,
              "title configuration changed pause/difficulty anchors");
        auto missing_tail=longer_title;rectangle(missing_tail,120,677,18,18,32);
        check(!expanded.observe(view(missing_tail),identity).hud_candidate,"newly included title tail not required by mask");
        auto filled_title=longer_title;rectangle(filled_title,region.x,region.y,region.width,region.height);
        check(!expanded.observe(view(filled_title),identity).hud_candidate,"expanded white title ROI bypassed negative-space guard");
        for(const auto state:{ObservedGameState::paused,ObservedGameState::result}) {
            const auto denied=expanded.observe(view(longer_title),identity,state);
            check(!denied.hud_candidate&&denied.denied_state_match&&denied.reason=="calibrated_denied_state_has_priority",
                  "expanded title mask overrode same-frame denied state");
        }
        auto boundary=longer_title;rectangle(boundary,184,680,1,3);
        rejected=false;
        try {HudGlyphEvidenceProducer contaminated(view(boundary),std::string(64,'d'),"expanded-boundary",region);}
        catch(const std::invalid_argument&) {rejected=true;}
        check(rejected,"expanded title mask learned a white boundary as glyph evidence");
    }
    ++groups;{
        for(const auto region:{PixelRegion{-1,672,160,29},PixelRegion{25,-1,160,29},PixelRegion{25,672,0,29},
                PixelRegion{25,672,160,-1},PixelRegion{1200,672,160,29},PixelRegion{25,700,160,29},
                PixelRegion{0,0,1280,720},PixelRegion{0,0,std::numeric_limits<int>::max(),29},
                PixelRegion{std::numeric_limits<int>::max(),672,160,29},
                PixelRegion{25,std::numeric_limits<int>::max(),160,29},
                PixelRegion{25,672,160,std::numeric_limits<int>::max()}}) {
            bool rejected=false;
            try {HudGlyphEvidenceProducer bad(view(original),std::string(64,'c'),"invalid-title-region",region);}
            catch(const std::invalid_argument&) {rejected=true;}
            check(rejected,"invalid or unbounded title mask region accepted");
        }
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
    ++groups;{
        ScreenStateResult state;state.state=ObservedGameState::unknown;state.reason="no_calibrated_state_match";
        check(hud_candidate_eligible(true,false,true,state),"explicit shared opening candidate unavailable before arm");
        check(!hud_candidate_eligible(false,false,true,state),"default per-song fallback enabled before arm");
        check(!hud_candidate_eligible(true,false,false,state),"invalid source admitted to shared glyph path");
        state.state=ObservedGameState::playing;
        check(hud_candidate_eligible(true,false,true,state),"shared template playing frame skipped glyph gate");
        for(const auto denied:{ObservedGameState::paused,ObservedGameState::result}) {
            state.state=denied;check(!hud_candidate_eligible(true,true,true,state),"shared path overrode denied state");
            state.state=ObservedGameState::unknown;
            decltype(state.scores)::value_type score;score.state=denied;score.all_anchors_match=true;state.scores={score};
            check(!hud_candidate_eligible(true,false,true,state),"ambiguous denied match enabled shared path");
            state.scores.clear();
        }
    }
    ++groups;{
        HudGlyphEvidenceProducer cells(view(original),std::string(64,'c'),"shared-IN-cells-v2",{},HudGlyphPolicy::shared_in_cell_score_v2);
        const auto clean=cells.observe_features(view(original));
        check(clean.hud_candidate&&clean.score.calibrated_cells&&clean.score.supported_cells==7,"v2 clean seven-cell baseline rejected");
        auto left=original;rectangle(left,1076,40,20,3);
        const auto overlap=cells.observe_features(view(left));
        check(overlap.hud_candidate&&overlap.score.supported_cells==7&&overlap.score.trimmed_left_pixels>0&&
            overlap.score.original_left_bounds&&overlap.score.retained_left_bounds,"v2 left note connection erased actual first digit");
        HudGlyphEvidenceProducer old(view(original),std::string(64,'c'),"old-shared",{},HudGlyphPolicy::shared_in_prefix_score_v1);
        check(!old.observe_features(view(left)).hud_candidate,"old policy unexpectedly changed");
        auto external=original;rectangle(external,1076,21,180,2);
        check(cells.observe_features(view(external)).hud_candidate,"v2 off-baseline note fragments erased seven observed digits");
        for(unsigned which=0;which<8;++which) {
            auto invalid=original;
            if(which==0)rectangle(invalid,1076,20,180,40,32);
            if(which==1){rectangle(invalid,1080,24,23,33,32);rectangle(invalid,1076,40,24,2);}
            if(which==2)score_glyphs(invalid,true);
            if(which==3)rectangle(invalid,1076,20,180,40);
            if(which==4)rectangle(invalid,1084+3*24,24,23,33,32);
            if(which==5){rectangle(invalid,1076,20,180,40,32);rectangle(invalid,1076,40,180,3);}
            if(which==6)rectangle(invalid,1140,673,116,26,32);
            if(which==7)for(auto& value:invalid)value=static_cast<std::uint8_t>(value/2);
            check(!cells.observe_features(view(invalid)).hud_candidate,"v2 admitted missing/filled/line-only/dim score or absent prefix");
        }
        for(const auto state:{ObservedGameState::paused,ObservedGameState::result})
            check(!cells.observe_features(view(left),state).hud_candidate,"v2 repair overrode denied state");
        // Actual score font is proportional: right-align separated narrow ones
        // and broad zero strokes, without borrowing reference pitch.
        auto proportional=original;rectangle(proportional,1076,20,180,40,32);
        int right=1245;
        for(int i=6;i>=0;--i) {
            const int w=i%2?6:17;const int x=right-w;
            rectangle(proportional,x,28,w,25);
            if(w==17)rectangle(proportional,x+3,31,11,19,32);
            else rectangle(proportional,x,31,2,19,32);
            right=x-7;
        }
        check(cells.observe_features(view(proportional)).hud_candidate,"v2 assumed monospace score advances");
        auto colored=original;
        for(int i=0;i<7;++i)for(int y=28;y<53;++y)for(int x=1084+i*24;x<1101+i*24;++x) {
            const auto at=static_cast<std::size_t>(y*1280+x)*3;colored[at]=245;colored[at+1]=245;colored[at+2]=150;
        }
        check(!cells.observe_features(view(colored)).hud_candidate,"v2 bright colored fill supplied fake dark negative space");
    }
    ++groups;{
        auto literal=original;rectangle(literal,1152,673,73,26,32);
        rectangle(literal,1156,677,2,17);
        rectangle(literal,1163,677,2,17);rectangle(literal,1175,677,2,17);
        for(int y=0;y<17;++y)rectangle(literal,1163+y*11/16,677+y,2,1);
        rectangle(literal,1194,677,2,17);rectangle(literal,1194,692,10,2);
        for(int y=0;y<13;++y){const int d=y*5/12;rectangle(literal,1206+d,681+y,2,1);rectangle(literal,1215-d,681+y,2,1);}
        rectangle(literal,1220,691,3,3);
        HudGlyphEvidenceProducer exact(view(literal),std::string(64,'c'),"exact-v2",{},HudGlyphPolicy::shared_in_cell_score_v2);
        HudGlyphEvidenceProducer tolerant(view(literal),std::string(64,'c'),"subpixel-v3",{},HudGlyphPolicy::shared_in_subpixel_prefix_score_v3);
        check(tolerant.observe_features(view(literal)).hud_candidate,"v3 changed exact acceptance");
        auto phase=literal;rectangle(phase,1152,673,73,26,32);
        for(int y=673;y<699;++y)for(int x=1152;x<1225;++x) {
            const auto at=static_cast<std::size_t>(y*1280+x)*3;
            if(literal[at]<195)continue;
            if(x==1157)continue; // A genuine one-pixel white core for the I.
            const int shift=(x>=1194&&x<=1216)?-2:-1;
            rectangle(phase,x+shift,y,1,1,255);
        }
        const auto repaired=tolerant.observe_features(view(phase));
        check(!exact.observe_features(view(phase)).hud_candidate,"v2 unexpectedly accepted nonuniform pixel phases");
        check(repaired.hud_candidate&&repaired.fixed_glyphs[1].subpixel_tolerant&&
            repaired.fixed_glyphs[1].literal_components==5&&repaired.fixed_glyphs[1].foreground_white_fraction>=.98&&
            repaired.fixed_glyphs[1].foreground_mean_error<=8&&repaired.difficulty_alignment_unique_family&&
            repaired.difficulty_alignment_lower_x==-2&&repaired.difficulty_alignment_upper_x==-1,
            "v3 failed bounded one-pixel literal support/alignment family");
        for(const auto r:{PixelRegion{1155,677,1,17},PixelRegion{1162,677,14,17},PixelRegion{1192,677,10,17},
                         PixelRegion{1204,681,11,13},PixelRegion{1219,691,3,3}}) {
            auto missing=phase;rectangle(missing,r.x,r.y,r.width,r.height,32);
            check(!tolerant.observe_features(view(missing)).hud_candidate,"v3 ignored a missing literal component");
        }
        auto checker=phase;
        for(int y=673;y<699;++y)for(int x=1152;x<1225;++x)if((x+y)%3==0)rectangle(checker,x,y,1,1,32);
        check(!tolerant.observe_features(view(checker)).hud_candidate,"v3 admitted fragmented checker strokes");
        auto solid=phase;rectangle(solid,1152,673,73,26);
        check(!tolerant.observe_features(view(solid)).hud_candidate,"v3 admitted solid prefix");
        auto line=phase;rectangle(line,1152,673,73,26,32);rectangle(line,1152,684,73,2);
        check(!tolerant.observe_features(view(line)).hud_candidate,"v3 admitted line-only prefix");
        for(const auto state:{ObservedGameState::paused,ObservedGameState::result})
            check(!tolerant.observe_features(view(phase),state).hud_candidate,"v3 support overrode denied state");
        auto score_missing=phase;rectangle(score_missing,1076,20,180,40,32);
        check(!tolerant.observe_features(view(score_missing)).hud_candidate,"v3 dropped mandatory seven-score structure");
        auto pause_missing=phase;rectangle(pause_missing,21,23,28,33,32);
        check(!tolerant.observe_features(view(pause_missing)).hud_candidate,"v3 dropped mandatory pause glyph");
        bool rejected=false;
        try {HudGlyphEvidenceProducer bad(view(original),std::string(64,'c'),"wrong-literal-reference",{},HudGlyphPolicy::shared_in_subpixel_prefix_score_v3);}
        catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"v3 reference silently calibrated nonliteral component layout");
        ++groups;{
            // Actual AT RAM observations: the 255-gray top edge moves to
            // 148..151 while the reference's 202-gray bottom becomes white.
            // Keep the interior, negative space and other HUD anchors intact.
            auto soft=literal;rectangle(soft,21,23,28,33,32);
            for(const int x:{24,39}){rectangle(soft,x,26,7,25,255);rectangle(soft,x,51,7,1,202);}
            auto pause_phase=soft;
            for(const int x:{24,39}){rectangle(pause_phase,x,26,7,1,151);rectangle(pause_phase,x,51,7,1,255);}
            HudGlyphEvidenceProducer v4(view(soft),std::string(64,'c'),"AT-pause-core-v4",{},HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4);
            HudGlyphEvidenceProducer v3(view(soft),std::string(64,'c'),"IN-pause-reference-v3",{},HudGlyphPolicy::shared_in_subpixel_prefix_score_v3);
            const auto accepted=v4.observe_features(view(pause_phase)),legacy=v3.observe_features(view(pause_phase));
            check(v4.observe_features(view(soft)).hud_candidate&&accepted.hud_candidate,"v4 rejected pause core with changed antialiased edge");
            check(accepted.fixed_glyphs[0].foreground_pixels==230&&accepted.fixed_glyphs[0].foreground_white_fraction==1&&
                accepted.fixed_glyphs[0].foreground_mean_error==0,"v4 changed strict core checks or retained soft reference edge");
            check(v3.observe_features(view(soft)).hud_candidate&&!legacy.hud_candidate&&legacy.fixed_glyphs[0].foreground_pixels==364&&
                legacy.fixed_glyphs[0].foreground_white_fraction==350.0/364,"v3 pause reference identity changed");
            for(const auto value:{148,150,151}) {
                auto observed=pause_phase;for(const int x:{24,39})rectangle(observed,x,26,7,1,static_cast<std::uint8_t>(value));
                check(v4.observe_features(view(observed)).hud_candidate,"v4 rejected observed 14-pixel AT top-edge phases");
            }
            for(unsigned which=0;which<4;++which) {
                auto invalid=pause_phase;
                if(which==0)rectangle(invalid,25,27,5,1,32); // >2% genuinely missing interior core.
                if(which==1)rectangle(invalid,21,23,28,33,32);
                if(which==2)rectangle(invalid,21,23,28,33);
                if(which==3)for(auto& value:invalid)value=static_cast<std::uint8_t>(value/2);
                check(!v4.observe_features(view(invalid)).hud_candidate,"v4 pause core admitted missing/dim core or filled negative space");
            }
            for(const auto state:{ObservedGameState::paused,ObservedGameState::result})
                check(!v4.observe_features(view(pause_phase),state).hud_candidate,"v4 pause core overrode denied state");
            auto faint=soft;for(const int x:{24,39})rectangle(faint,x,26,7,26,202);
            rejected=false;
            try {HudGlyphEvidenceProducer bad(view(faint),std::string(64,'c'),"no-strong-pause",{},HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4);}
            catch(const std::invalid_argument&){rejected=true;}
            check(rejected,"v4 calibrated a pause without strong foreground support");
            rejected=false;
            try {HudGlyphEvidenceProducer bad(view(soft),std::string(64,'c'),"v3-forced-core",{},HudGlyphPolicy::shared_in_subpixel_prefix_score_v3,{},HudPauseMaskPolicy::eroded_strong_white_core_v2);}
            catch(const std::invalid_argument&){rejected=true;}
            check(rejected,"v3 accepted a silent pause mask override");
        }
    }
    std::cout<<groups<<" groups / "<<checks<<" HUD glyph evidence checks passed; offline feature candidate only.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
