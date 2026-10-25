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
        check(!h.observe(view(blocked),few,raw(),frame(4,1300),true).current_candidate,"fewer than four observed score glyphs admitted");
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
