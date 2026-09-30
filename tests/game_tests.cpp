#include "pas/game.hpp"
#include "pas/runtime.hpp"
#include "pas/game_motion.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>

using namespace pas;
namespace {
Frame image(std::uint64_t sequence, Nanoseconds time) {
    Frame f; f.width=1280; f.height=720; f.stride=3840;
    f.epoch=1; f.generation=1; f.geometry_version=1; f.sequence=sequence;
    f.capture_complete_ns=time; f.rgb.resize(1280*720*3); return f;
}
void rect(Frame& f,int x,int y,int w,int h,std::array<std::uint8_t,3> c) {
    for(int py=y;py<y+h;++py) for(int px=x;px<x+w;++px) {
        auto* p=f.rgb.data()+static_cast<std::size_t>(py)*f.stride+px*3;
        std::copy(c.begin(),c.end(),p);
    }
}
void hud(Frame& f) {
    rect(f,20,20,6,22,{255,255,255}); rect(f,34,20,6,22,{255,255,255});
    for(int i=0;i<6;++i) rect(f,1020+i*24,20,12,20,{255,255,255});
}
void oriented_box(Frame& f,Vec2 center,Vec2 u,double width,double height,std::array<std::uint8_t,3> color) {
    const Vec2 n{-u.y,u.x};
    const int rx=static_cast<int>(std::ceil((std::abs(u.x)*width+std::abs(n.x)*height)/2))+1,
              ry=static_cast<int>(std::ceil((std::abs(u.y)*width+std::abs(n.y)*height)/2))+1;
    for(int y=std::max(0,static_cast<int>(center.y)-ry);y<std::min(f.height,static_cast<int>(center.y)+ry+1);++y)
        for(int x=std::max(0,static_cast<int>(center.x)-rx);x<std::min(f.width,static_cast<int>(center.x)+rx+1);++x) {
            const double dx=x-center.x,dy=y-center.y;
            if(std::abs(dx*u.x+dy*u.y)<=width/2&&std::abs(dx*n.x+dy*n.y)<=height/2)
                rect(f,x,y,1,1,color);
        }
}
void enclosed_hold_patch(Frame& f,int head,int shift=0) {
    rect(f,425+shift,36,142,head-36,{40,190,255});
    rect(f,418+shift,0,4,head,{245,245,245});rect(f,570+shift,0,4,head,{245,245,245});
    rect(f,428+shift,head-80,136,3,{210,197,146});
    rect(f,428+shift,head-80,3,80,{210,197,146});
    rect(f,561+shift,head-80,3,80,{210,197,146});
    rect(f,431+shift,head-38,130,24,{210,197,146});
}
ContactPlan tap(std::uint64_t id,Nanoseconds evidence,Nanoseconds due) {
    return {1,id,1,evidence,due+30'000'000,1,"pixels",
        {{Phase::down,20,30,due},{Phase::up,20,30,due+5'000'000}}};
}
DecisionSnapshot snapshot(std::uint64_t sequence,Nanoseconds time) {
    DecisionSnapshot s; s.sequence=sequence; s.context={1,1,1,sequence,time,1280,720,1};
    s.ui=GameUi::playing; s.playing_gate=true; return s;
}
GameTarget target(std::uint64_t id,Nanoseconds time,Nanoseconds due) {
    GameTarget t; t.note_id=id; t.revision=1; t.evidence_ns=time; t.expires_ns=time+100'000'000;
    t.note.kind=NoteKind::tap; t.crossing_ns=due; t.hit={400,500};
    t.reason="prediction_observe_only"; t.uncertainty_ns=2'000'000; return t;
}
}

TEST(GameObserver, MultiplePixelsTracksAndRelativeMovingLinePrediction) {
    FakeClock clock; GameObserver observer(clock); DecisionSnapshot result;
    for(int i=0;i<5;++i) {
        auto f=image(i+1,i*20'000'000); hud(f);
        rect(f,100,500+i*2,1080,4,{255,255,255});
        rect(f,270,380+i*10,60,10,{40,190,255});
        rect(f,770,280+i*10,60,10,{40,190,255});
        clock.set(f.capture_complete_ns); result=observer.process(f);
    }
    ASSERT_TRUE(result.playing_gate); ASSERT_EQ(result.lines.size(),1);
    ASSERT_EQ(result.targets.size(),2);
    EXPECT_NE(result.targets[0].note_id,result.targets[1].note_id);
    const auto approaching = std::find_if(result.targets.begin(), result.targets.end(),
        [](const GameTarget& t) { return t.note.center.x < 500; });
    ASSERT_NE(approaching, result.targets.end());
    ASSERT_TRUE(approaching->crossing_ns);
    EXPECT_NEAR(approaching->velocity,400,2);
    EXPECT_NEAR(approaching->residual,0,0.01);
    EXPECT_NEAR(*approaching->crossing_ns/1e6,292.5,3);
    EXPECT_GT(approaching->uncertainty_ns,0);
}
TEST(GameObserver, RowPrescreenPreservesCompleteDecisionsOnSyntheticMotion) {
    FakeClock clock; GameObserver full(clock,false,false),fast(clock,true,false);
    for(int i=0;i<12;++i) {
        auto f=image(i+1,i*17'000'000);hud(f);
        rect(f,80,510+i,1120,3,{255,255,255});
        for(int x=300;x<390;++x) rect(f,x,390+i*9,1,8,{40,190,255});
        if(i%3==0) rect(f,830,140,3,440,{255,255,255});
        clock.set(f.capture_complete_ns);
        EXPECT_EQ(decision_json(full.process(f)),decision_json(fast.process(f))) << "frame " << i;
    }
}
TEST(GameObserver, RowPrescreenPreservesRecordedClipDecisionsWhenProvided) {
    const char* root_env=std::getenv("PAS_RGB_CLIP_ROOT");
    if(!root_env) GTEST_SKIP() << "Set PAS_RGB_CLIP_ROOT to a verified pixel-clips directory";
    const std::filesystem::path root(root_env);
    std::ifstream index(root/"index.jsonl",std::ios::binary);
    ASSERT_TRUE(index.is_open());
    FakeClock clock; GameObserver full(clock,false,false),fast(clock,true,false),joined(clock,true,true);
    int prior_round=-1,prior_clip=-1,checked=0;
    int frames_with_new_lines=0,focus_frames_with_new_lines=0;
    std::vector<double> full_ms,fast_ms,joined_ms;
    std::string line;
    while(std::getline(index,line)) {
        if(line.empty())continue;
        const auto entry=nlohmann::json::parse(line);
        const int round=entry.at("round_id").get<int>(),clip=entry.at("clip_id").get<int>();
        if(round!=prior_round||clip!=prior_clip) {full.reset();fast.reset();joined.reset();}
        prior_round=round;prior_clip=clip;
        Frame f;f.width=entry.at("width").get<int>();f.height=entry.at("height").get<int>();
        f.stride=entry.at("stride").get<int>();f.source_rotation=1;
        f.epoch=f.generation=f.geometry_version=1;
        f.sequence=entry.at("source_frame").get<std::uint64_t>();
        f.capture_complete_ns=entry.at("capture_complete_ns").get<Nanoseconds>();
        f.rgb.resize(static_cast<std::size_t>(f.stride)*f.height);
        std::ifstream input(root/entry.at("path").get<std::string>(),std::ios::binary);
        ASSERT_TRUE(input.is_open());
        input.read(reinterpret_cast<char*>(f.rgb.data()),static_cast<std::streamsize>(f.rgb.size()));
        ASSERT_EQ(input.gcount(),static_cast<std::streamsize>(f.rgb.size()));
        clock.set(f.capture_complete_ns);
        DecisionSnapshot full_decision,fast_decision,joined_decision;
        const auto run=[&](GameObserver& observer,DecisionSnapshot& decision,std::vector<double>& times) {
            const auto begin=std::chrono::steady_clock::now();
            decision=observer.process(f);
            const auto end=std::chrono::steady_clock::now();
            times.push_back(std::chrono::duration<double,std::milli>(end-begin).count());
        };
        if(checked%2) {
            run(fast,fast_decision,fast_ms);run(full,full_decision,full_ms);
        } else {
            run(full,full_decision,full_ms);run(fast,fast_decision,fast_ms);
        }
        run(joined,joined_decision,joined_ms);
        EXPECT_EQ(decision_json(full_decision),decision_json(fast_decision))
            << "round " << round << " clip " << clip << " frame " << f.sequence;
        ASSERT_LE(joined_decision.lines.size(),16u);
        if(joined_decision.lines.size()>fast_decision.lines.size()) {
            ++frames_with_new_lines;
            if(round==5&&(clip==3||clip==4))++focus_frames_with_new_lines;
            std::cout << "joined_line_diff round=" << round << " clip=" << clip
                      << " frame=" << f.sequence << " baseline=" << fast_decision.lines.size()
                      << " split=" << joined_decision.lines.size();
            for(const auto& candidate:joined_decision.lines)
                std::cout << " (" << candidate.center.x << "," << candidate.center.y
                          << ";" << candidate.tangent.x << "," << candidate.tangent.y
                          << ";" << candidate.length << ")";
            std::cout << '\n';
        }
        ++checked;
    }
    EXPECT_GT(checked,0);
    const auto report=[](const char* label,std::vector<double> values) {
        std::sort(values.begin(),values.end());
        const auto q=[&](double p){return values[static_cast<std::size_t>(p*(values.size()-1))];};
        std::cout << "row_prescreen " << label << " n=" << values.size()
                  << " p5=" << q(.05) << " p50=" << q(.50) << " p95=" << q(.95)
                  << " p99=" << q(.99) << " max=" << values.back()
                  << " jitter_p95_minus_p5=" << q(.95)-q(.05) << " ms\n";
    };
    if(checked>0) {
        report("full",full_ms);report("prescreen",fast_ms);report("joined",joined_ms);
        std::cout << "joined_line_differences n=" << frames_with_new_lines
                  << " focus=" << focus_frames_with_new_lines << " of " << checked << " frames\n";
    }
}
TEST(GameObserver, SplitsPixelConnectedCrossingRidgesWithoutInventingHoldRails) {
    FakeClock clock;
    auto single=image(1,20'000'000);hud(single);
    rect(single,80,510,1120,4,{255,255,255});
    rect(single,540,30,4,680,{255,255,255});
    rect(single,955,30,4,680,{255,255,255});
    GameObserver single_baseline(clock,true,false),single_candidate(clock,true,true);
    clock.set(single.capture_complete_ns);
    const auto single_old=single_baseline.process(single),single_split=single_candidate.process(single);
    ASSERT_EQ(single_old.lines.size(),1u);
    EXPECT_EQ(single_split.lines.size(),single_old.lines.size());

    auto f=single;rect(f,80,200,1120,4,{255,255,255});
    GameObserver baseline(clock,true,false),candidate(clock,true,true);
    clock.set(f.capture_complete_ns);
    const auto old=baseline.process(f),split=candidate.process(f);
    const auto has_vertical=[](const DecisionSnapshot& decision,int x,int min_length) {
        return std::any_of(decision.lines.begin(),decision.lines.end(),[&](const LineCandidate& line) {
            return std::abs(line.center.x-x)<12&&std::abs(line.tangent.y)>.98&&line.length>min_length;
        });
    };
    ASSERT_GE(old.lines.size(),2u);
    EXPECT_FALSE(has_vertical(old,540,600));
    EXPECT_FALSE(has_vertical(old,955,600));
    EXPECT_TRUE(has_vertical(split,540,600));
    EXPECT_TRUE(has_vertical(split,955,600));

    auto hold=image(2,40'000'000);hud(hold);
    rect(hold,80,510,1120,4,{255,255,255});
    enclosed_hold_patch(hold,510); // White rails join the judgment line at their lower edge.
    clock.set(hold.capture_complete_ns);
    const auto hold_result=candidate.process(hold);
    EXPECT_FALSE(has_vertical(hold_result,418,350));
    EXPECT_FALSE(has_vertical(hold_result,570,350));
}
TEST(GameObserver, NotesAloneDuplicateFramesAndGeometryCannotArm) {
    FakeClock clock; GameObserver observer(clock);
    auto f=image(1,0); rect(f,100,500,1080,4,{255,255,255});
    rect(f,270,380,60,10,{40,190,255});
    for(int i=0;i<4;++i) {f.sequence=i+1; f.capture_complete_ns=i*20'000'000;
        clock.set(f.capture_complete_ns); EXPECT_FALSE(observer.process(f).playing_gate);}
    hud(f); f.sequence=10; f.capture_complete_ns=100'000'000; clock.set(f.capture_complete_ns);
    for(int i=0;i<4;++i) EXPECT_FALSE(observer.process(f).playing_gate);
    f.geometry_version=2; f.sequence=11; f.capture_complete_ns+=20'000'000;
    clock.set(f.capture_complete_ns); const auto result=observer.process(f);
    ASSERT_EQ(result.targets.size(),1); EXPECT_FALSE(result.targets[0].crossing_ns);
    EXPECT_FALSE(result.playing_gate);
}
TEST(GameObserver, ChangingIndicatorColorKeepsTiltedLinePredictionAndNoteIdentity) {
    FakeClock clock;GameObserver observer(clock);DecisionSnapshot result;std::uint64_t id=0;
    const std::array<std::array<std::uint8_t,3>,5> colors{{
        {255,255,255},{255,220,40},{40,190,255},{40,190,255},{255,255,255}}};
    for(int i=0;i<5;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);
        for(int x=0;x<1280;++x) rect(f,x,520+x/20,1,3,colors[i]);
        // Blue note is short and remains distinct from the spanning blue line.
        for(int x=280;x<420;++x) rect(f,x,400+i*10+(x-350)/20,1,6,{40,190,255});
        clock.set(f.capture_complete_ns);result=observer.process(f);
        ASSERT_EQ(result.lines.size(),1);EXPECT_GT(result.lines[0].length,1200);
        EXPECT_NEAR(result.lines[0].tangent.y,.05,.005);
        ASSERT_EQ(result.targets.size(),1);
        if(!id) id=result.targets[0].note_id;EXPECT_EQ(result.targets[0].note_id,id);
    }
    ASSERT_TRUE(result.playing_gate);ASSERT_TRUE(result.targets[0].crossing_ns);
    EXPECT_NEAR(result.targets[0].velocity,500,10);
}
TEST(GameObserver, UiLossCancelsAndCapacityIsBounded) {
    FakeClock clock; GameObserver observer(clock);
    for(int i=0;i<3;++i) {auto f=image(i+1,i*20'000'000); hud(f); clock.set(f.capture_complete_ns);
        const auto s=observer.process(f); EXPECT_EQ(s.playing_gate,i==2);}
    auto blank=image(4,60'000'000); clock.set(blank.capture_complete_ns);
    EXPECT_FALSE(observer.process(blank).playing_gate);
    auto f=image(5,80'000'000); hud(f);
    for(int y=100;y<660;y+=24) for(int x=10;x<1220;x+=80) rect(f,x,y,60,6,{20,190,255});
    clock.set(f.capture_complete_ns); const auto s=observer.process(f);
    EXPECT_LE(s.targets.size(),128); EXPECT_FALSE(s.capacity_valid); EXPECT_FALSE(s.playing_gate);
}

TEST(GameObserver, FreshHudAfterShortGapKeepsUiButClearsAllMotionAcrossEpoch) {
    FakeClock clock;GameObserver observer(clock);DecisionSnapshot s;
    const auto observe=[&](int frame,Nanoseconds time,std::uint64_t epoch) {
        auto f=image(frame,time);f.epoch=epoch;hud(f);
        rect(f,0,575,1280,2,{255,255,255});
        rect(f,400,450+frame*8,144,8,{40,190,255});
        clock.set(time);return observer.process(f);
    };
    for(int i=0;i<5;++i) s=observe(i+1,i*20'000'000,1);
    ASSERT_TRUE(s.playing_gate);ASSERT_EQ(s.targets.size(),1);
    ASSERT_TRUE(s.targets[0].crossing_ns);const auto old_id=s.targets[0].note_id;
    s=observe(6,181'700'000,1); // Current HUD visible after a 101.7 ms gap.
    EXPECT_TRUE(s.playing_gate);ASSERT_EQ(s.targets.size(),1);
    EXPECT_NE(s.targets[0].note_id,old_id);EXPECT_EQ(s.targets[0].samples,1);
    EXPECT_FALSE(s.targets[0].crossing_ns);EXPECT_EQ(s.targets[0].history_span_ns,0);
    const auto gap_id=s.targets[0].note_id;
    s=observe(7,201'700'000,2); // Input revoke changes epoch, not visual scene.
    EXPECT_TRUE(s.playing_gate);ASSERT_EQ(s.targets.size(),1);
    EXPECT_NE(s.targets[0].note_id,gap_id);EXPECT_EQ(s.targets[0].samples,1);
    EXPECT_FALSE(s.targets[0].crossing_ns);
}

TEST(GameObserver, HudContinuityNeverBridgesMissingPixelsLongGapOrGeometry) {
    FakeClock clock;GameObserver observer(clock);
    const auto observe=[&](int frame,Nanoseconds time,bool visible,std::uint64_t geometry=1,bool valid=true) {
        auto f=image(frame,time);f.geometry_version=geometry;f.source_valid=valid;
        if(visible) hud(f);clock.set(time);return observer.process(f);
    };
    EXPECT_FALSE(observe(1,1,true).playing_gate);
    EXPECT_FALSE(observe(2,20'000'001,true).playing_gate);
    ASSERT_TRUE(observe(3,40'000'001,true).playing_gate);
    EXPECT_FALSE(observe(4,160'000'001,false).playing_gate);
    EXPECT_FALSE(observe(5,180'000'001,true).playing_gate);
    EXPECT_FALSE(observe(6,200'000'001,true).playing_gate);
    ASSERT_TRUE(observe(7,220'000'001,true).playing_gate);
    EXPECT_FALSE(observe(8,471'000'001,true).playing_gate);
    EXPECT_FALSE(observe(9,491'000'001,true).playing_gate);
    ASSERT_TRUE(observe(10,511'000'001,true).playing_gate);
    EXPECT_FALSE(observe(11,531'000'001,true,2).playing_gate);
    EXPECT_FALSE(observe(12,551'000'001,true,2).playing_gate);
    ASSERT_TRUE(observe(13,571'000'001,true,2).playing_gate);
    EXPECT_FALSE(observe(14,591'000'001,true,2,false).playing_gate);
    EXPECT_FALSE(observe(15,611'000'001,true,2).playing_gate);
    EXPECT_FALSE(observe(16,631'000'001,true,2).playing_gate);
    ASSERT_TRUE(observe(17,651'000'001,true,2).playing_gate);
    EXPECT_FALSE(observe(17,651'000'001,true,2).playing_gate); // Replay resets.
}

TEST(GameObserver, OnePixelLineJoinedToHoldBorderStaysObservable) {
    FakeClock clock; GameObserver observer(clock); auto f=image(1,1);
    hud(f); rect(f,0,575,1280,1,{255,255,255});
    rect(f,400,200,2,376,{255,255,255}); rect(f,550,200,2,376,{255,255,255});
    rect(f,403,200,144,350,{40,190,255}); clock.set(1);
    const auto s=observer.process(f); ASSERT_EQ(s.lines.size(),1);
    EXPECT_NEAR(s.lines[0].center.y,575,1); EXPECT_GT(s.lines[0].length,1200);
}

TEST(GameDiagnostics, CurrentCentralDigitShapesExcludeComboLabelRailsAndProgress) {
    FakeClock clock;GameObserver observer(clock);auto f=image(1,1);hud(f);
    rect(f,0,0,640,8,{255,255,255}); // Progress bar.
    rect(f,560,0,4,550,{255,255,255}); // Hold rail through the HUD.
    for(int i=0;i<5;++i) rect(f,612+i*10,64,6,12,{255,255,255}); // COMBO label.
    clock.set(1);EXPECT_EQ(observer.process(f).combo_digit_glyphs,0);
    rect(f,620,18,10,36,{255,255,255});
    rect(f,648,18,22,36,{255,255,255});
    f.sequence=2;f.capture_complete_ns=20'000'001;clock.set(f.capture_complete_ns);
    const auto s=observer.process(f);EXPECT_EQ(s.combo_digit_glyphs,2);
    EXPECT_EQ(decision_json(s).at("combo_digit_glyphs"),2);
    EXPECT_EQ(decision_json(s).at("per_note_feedback"),"unknown");
}

TEST(GameDiagnostics, ComboDisappearanceRequiresFreshSustainedAbsenceAndRearms) {
    ComboVisibilityDiagnostic diagnostic;
    const auto observe=[&](int frame,Nanoseconds time,int glyphs,bool gate=true) {
        auto s=snapshot(frame,time);s.combo_digit_glyphs=glyphs;s.playing_gate=gate;
        return diagnostic.observe(s);
    };
    EXPECT_FALSE(observe(1,1,1));EXPECT_FALSE(observe(2,20'000'001,1));
    EXPECT_FALSE(observe(3,40'000'001,0));
    EXPECT_FALSE(observe(4,45'000'001,0)); // Burst shorter than 12 ms.
    EXPECT_TRUE(observe(5,60'000'001,0));
    EXPECT_FALSE(observe(6,80'000'001,0));EXPECT_FALSE(observe(7,100'000'001,0));
    EXPECT_FALSE(observe(8,120'000'001,1));EXPECT_FALSE(observe(9,140'000'001,0));
    EXPECT_FALSE(observe(10,160'000'001,0)); // One positive frame cannot arm.
    EXPECT_FALSE(observe(11,180'000'001,1));EXPECT_FALSE(observe(12,200'000'001,1));
    EXPECT_FALSE(observe(13,220'000'001,0));EXPECT_FALSE(observe(14,240'000'001,1)); // Flicker.
    EXPECT_FALSE(observe(15,260'000'001,0));EXPECT_TRUE(observe(16,280'000'001,0));
}

TEST(GameDiagnostics, DecorativeRailConnectedToDigitDoesNotReportDisappearance) {
    FakeClock clock;GameObserver observer(clock);ComboVisibilityDiagnostic diagnostic;
    for(int i=0;i<6;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);
        if(i>=3) rect(f,638,0,4,575,{255,255,255});
        // A digit-like three connected strokes, crossed by a full-height rail.
        for(const int y:{18,34,50}) rect(f,628,y,24,4,{255,255,255});
        rect(f,648,18,4,36,{255,255,255});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        EXPECT_EQ(s.combo_digit_glyphs,1)<<"frame "<<i;
        EXPECT_FALSE(diagnostic.observe(s));
    }
    auto blank=image(7,120'000'000);hud(blank);rect(blank,638,0,4,575,{255,255,255});
    clock.set(blank.capture_complete_ns);auto s=observer.process(blank);
    EXPECT_EQ(s.combo_digit_glyphs,0);EXPECT_FALSE(diagnostic.observe(s));
    blank.sequence=8;blank.capture_complete_ns=140'000'000;clock.set(blank.capture_complete_ns);
    s=observer.process(blank);EXPECT_EQ(s.combo_digit_glyphs,0);EXPECT_TRUE(diagnostic.observe(s));
}

TEST(GameDiagnostics, ThinHorizontalRibbonJoiningDigitsKeepsVisibleTallInk) {
    FakeClock clock;GameObserver observer(clock);auto f=image(1,1);hud(f);
    rect(f,638,0,4,575,{255,255,255});
    rect(f,564,33,152,2,{255,255,255});rect(f,564,41,152,2,{255,255,255});
    rect(f,564,35,152,6,{255,220,40});
    clock.set(1);EXPECT_EQ(observer.process(f).combo_digit_glyphs,0); // Only borders and a rail.
    rect(f,620,18,10,36,{255,255,255});rect(f,648,18,10,36,{255,255,255});
    rect(f,564,35,152,6,{255,220,40}); // Current ribbon partially occludes both digits.
    f.sequence=2;f.capture_complete_ns=20'000'001;clock.set(f.capture_complete_ns);
    EXPECT_GE(observer.process(f).combo_digit_glyphs,1); // A cluster count, not OCR "11".
}

TEST(GameDiagnostics, ComboDiagnosticRejectsReplayUiLossLongGapAndGeometryChange) {
    for(int discontinuity=0;discontinuity<4;++discontinuity) {
        ComboVisibilityDiagnostic diagnostic;
        auto a=snapshot(1,1);a.combo_digit_glyphs=1;EXPECT_FALSE(diagnostic.observe(a));
        a=snapshot(2,20'000'001);a.combo_digit_glyphs=1;EXPECT_FALSE(diagnostic.observe(a));
        auto b=snapshot(3,40'000'001);
        if(discontinuity==0) b.context.frame=2;
        if(discontinuity==1) b.playing_gate=false;
        if(discontinuity==2) b.context.capture_ns=271'000'001;
        if(discontinuity==3) b.context.geometry=2;
        EXPECT_FALSE(diagnostic.observe(b));
        b.sequence++;b.context.frame++;b.context.capture_ns+=20'000'000;b.playing_gate=true;
        EXPECT_FALSE(diagnostic.observe(b));
    }
}

namespace {
DecisionSnapshot diagnostic_hold_scene(std::uint64_t frame,Nanoseconds time) {
    auto s=snapshot(frame,time);s.lines.push_back({{640,575},{1,0},1280,2,.85});
    auto h=target(17,time,time+60'000'000);h.note.kind=NoteKind::hold;
    h.note.center={850,575};h.note.width=144;h.note.height=300;h.note.rails_geometry=true;h.note.head_on_line=true;
    s.targets.push_back(h);return s;
}
}

TEST(GameDiagnostics, LongHoldDisappearanceRetainsOnlyItsPriorCoordinateEvidence) {
    HoldVisibilityDiagnostic diagnostic;auto s=diagnostic_hold_scene(1,1);
    EXPECT_FALSE(diagnostic.observe(s));
    s.context.frame=2;s.context.capture_ns=20'000'001;s.targets.clear();
    const auto loss=diagnostic.observe(s);ASSERT_TRUE(loss);
    EXPECT_EQ(loss->source_frame,1);EXPECT_EQ(loss->note_id,17);EXPECT_EQ(loss->capture_ns,1);
    EXPECT_EQ(loss->head.x,850);EXPECT_EQ(loss->height,300);
    s.context.frame=3;s.context.capture_ns=40'000'001;EXPECT_FALSE(diagnostic.observe(s));
    EXPECT_EQ(decision_json(s).at("per_note_feedback"),"unknown");
}

TEST(GameDiagnostics, HoldDiagnosticKeepsVisibleNewIdentityButRejectsNarrowFragments) {
    HoldVisibilityDiagnostic diagnostic;auto s=diagnostic_hold_scene(1,1);
    EXPECT_FALSE(diagnostic.observe(s));
    s.context.frame=2;s.context.capture_ns=20'000'001;s.targets[0].note_id=18;
    s.targets[0].note.center.y=574;EXPECT_FALSE(diagnostic.observe(s));
    s.context.frame=3;s.context.capture_ns=40'000'001;s.targets[0].note.width=50;
    const auto loss=diagnostic.observe(s);ASSERT_TRUE(loss);EXPECT_EQ(loss->note_id,18);
}

TEST(GameDiagnostics, HoldDiagnosticSkipsEndingBodyAndFrameDiscontinuities) {
    for(int variation=0;variation<7;++variation) {
        HoldVisibilityDiagnostic diagnostic;auto s=diagnostic_hold_scene(1,1);
        if(variation==0) s.targets[0].note.height=32;
        if(variation==6) s.targets[0].note.rails_geometry=false;
        EXPECT_FALSE(diagnostic.observe(s));s.context.frame=2;s.context.capture_ns=20'000'001;
        s.targets.clear();
        if(variation==1) s.playing_gate=false;
        if(variation==2) s.context.capture_ns=101'000'001;
        if(variation==3) s.context.geometry=2;
        if(variation==4) s.context.frame=1;
        if(variation==5) s.lines[0].confidence=.6;
        EXPECT_FALSE(diagnostic.observe(s));
    }
}

TEST(GameObserver, NestedRibbonsMergeButEqualWidthOverlapsRemainDistinct) {
    FakeClock clock; GameObserver observer(clock); auto f=image(1,1);
    rect(f,280,200,150,4,{255,220,40}); rect(f,292,208,126,4,{255,220,40});
    rect(f,780,200,150,4,{255,220,40}); rect(f,780,208,150,4,{255,220,40});
    clock.set(1); const auto s=observer.process(f);
    ASSERT_EQ(s.targets.size(),3);
    EXPECT_EQ(std::count_if(s.targets.begin(),s.targets.end(),[](const GameTarget& t) {
        return t.note.center.x<500;
    }),1);
}
TEST(GameDiagnostics, ApproachingBodyCannotConsumeTheHeldDisappearanceWindow) {
    HoldVisibilityDiagnostic diagnostic;auto s=diagnostic_hold_scene(1,1);
    s.targets.front().note.center.y=530;s.targets.front().note.head_on_line=false;
    EXPECT_FALSE(diagnostic.observe(s));s.context.frame=2;s.context.capture_ns=20'000'001;s.targets.clear();
    EXPECT_FALSE(diagnostic.observe(s));s=diagnostic_hold_scene(3,40'000'001);EXPECT_FALSE(diagnostic.observe(s));
    s.context.frame=4;s.context.capture_ns=60'000'001;s.targets.clear();EXPECT_TRUE(diagnostic.observe(s));
}
TEST(GameDiagnostics, CurrentHeldBodyMovingAwayFromTheLineCanReportItsDisappearance) {
    HoldVisibilityDiagnostic diagnostic;auto s=diagnostic_hold_scene(1,1);
    auto& n=s.targets.front().note;n.center.y=480;n.head_on_line=false;n.held_body_evidence=true;
    EXPECT_FALSE(diagnostic.observe(s));s.context.frame=2;s.context.capture_ns=20'000'001;
    n.center.y=470;EXPECT_FALSE(diagnostic.observe(s));
    s.context.frame=3;s.context.capture_ns=40'000'001;s.targets.clear();
    const auto missing=diagnostic.observe(s);ASSERT_TRUE(missing);EXPECT_NEAR(missing->head.y,470,1);
}
TEST(GameObserver, YellowOutlineDoesNotDuplicateBlueCoreAndHoldHasLeadingEdgeAndTail) {
    FakeClock clock; GameObserver observer(clock); auto f=image(1,1); hud(f);
    rect(f,200,380,150,18,{255,220,40}); rect(f,212,385,126,8,{40,190,255});
    rect(f,700,200,2,321,{255,255,255}); rect(f,842,200,2,321,{255,255,255});
    rect(f,703,360,136,160,{40,190,255}); clock.set(1); const auto s=observer.process(f);
    ASSERT_EQ(s.targets.size(),2);
    EXPECT_EQ(std::count_if(s.targets.begin(),s.targets.end(),[](const auto& t){return t.note.kind==NoteKind::drag;}),0);
    const auto h=std::find_if(s.targets.begin(),s.targets.end(),[](const auto& t){return t.note.kind==NoteKind::hold;});
    ASSERT_NE(h,s.targets.end()); EXPECT_NEAR(h->note.center.y,516,2);
    ASSERT_TRUE(h->note.tail); EXPECT_NEAR(h->note.tail->y,200,2);
}
TEST(GameObserver, DesaturatedHoldRequiresFreshParallelRailsAndDoesNotInventClippedTail) {
    FakeClock clock;GameObserver observer(clock);std::uint64_t identity=0;
    for(int i=0;i<3;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,403,400+i*10,136,140,{40,190,255});clock.set(f.capture_complete_ns);
        const auto s=observer.process(f);ASSERT_EQ(s.targets.size(),1);identity=s.targets[0].note_id;
    }
    for(int i=0;i<3;++i) {
        auto f=image(i+4,(i+3)*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int top=i==0?70:i==1?200:525;
        rect(f,400,top,2,576-top,{230,230,230});rect(f,542,top,2,576-top,{230,230,230});
        rect(f,403,top,136,575-top,{160,175,185});clock.set(f.capture_complete_ns);
        const auto s=observer.process(f);ASSERT_EQ(s.targets.size(),1);const auto& t=s.targets[0];
        EXPECT_EQ(t.note_id,identity);EXPECT_EQ(t.note.kind,NoteKind::hold);EXPECT_TRUE(t.note.outline_evidence);
        EXPECT_NEAR(t.note.center.y,575,2);
        if(i==0) EXPECT_FALSE(t.note.tail);else if(t.note.tail) EXPECT_NEAR(t.note.tail->y,top,3);
    }
    auto blank=image(7,120'000'000);hud(blank);rect(blank,0,575,1280,2,{255,255,255});
    clock.set(blank.capture_complete_ns);EXPECT_TRUE(observer.process(blank).targets.empty());
    auto old=image(8,240'000'000);hud(old);rect(old,0,575,1280,2,{255,255,255});
    rect(old,400,200,2,376,{230,230,230});rect(old,542,200,2,376,{230,230,230});
    clock.set(old.capture_complete_ns);EXPECT_TRUE(observer.process(old).targets.empty());
}
TEST(GameObserver, CurrentHoldRailsOverrideFragmentedCoreWithoutRemovingIndependentNotes) {
    FakeClock clock;GameObserver observer(clock);std::uint64_t identity=0;
    for(int i=0;i<3;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,783,300+i*10,144,250,{40,190,255});clock.set(f.capture_complete_ns);
        const auto s=observer.process(f);ASSERT_EQ(s.targets.size(),1);identity=s.targets[0].note_id;
    }
    for(int i=0;i<8;++i) {
        auto f=image(i+4,(i+3)*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,778,146+i*10,2,430-i*10,{230,230,230});
        rect(f,932,146+i*10,2,430-i*10,{230,230,230});
        if(i>0) {
            rect(f,778,528,2,48,{255,220,40});rect(f,932,528,2,48,{255,220,40});
        }
        // Later frames contain the short asymmetric rail occlusion caused
        // by hit effects. Current head fill still proves an attached body.
        if(i>0) {
            rect(f,778,548,2,24,{0,0,0});rect(f,932,480,2,24,{0,0,0});
        }
        rect(f,783,552,144,20,{160,175,185});
        // Upper core and two disconnected head fragments, as seen in the
        // retained live anomaly. Their PCA axes no longer describe the Hold.
        rect(f,794,360,110,112,{40,190,255});
        rect(f,794,432,110,16,{255,220,40}); // Internal overlay cannot create a new held head.
        rect(f,800,490,58,58,{40,190,255});rect(f,876,500,52,48,{40,190,255});
        rect(f,300,150,140,180,{40,190,255}); // A separate approaching Hold.
        rect(f,800,600,140,6,{40,190,255}); // A separate thin Tap.
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        ASSERT_EQ(s.targets.size(),3);
        const auto held=std::find_if(s.targets.begin(),s.targets.end(),[&](const auto& t){return t.note_id==identity;});
        ASSERT_NE(held,s.targets.end());EXPECT_TRUE(held->note.outline_evidence);
        EXPECT_NEAR(held->note.center.x,855,2);EXPECT_NEAR(held->note.center.y,575,2);
        EXPECT_NEAR(held->note.width,144,2);EXPECT_GT(held->samples,0);
        EXPECT_EQ(std::count_if(s.targets.begin(),s.targets.end(),[](const auto& t){return t.note.kind==NoteKind::tap;}),1);
    }
    auto f=image(12,220'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
    // Even a 24 px gap within the occlusion allowance cannot refresh a
    // previous held identity when current head fill is absent.
    rect(f,778,200,2,350,{230,230,230});rect(f,932,200,2,350,{230,230,230});
    clock.set(f.capture_complete_ns);EXPECT_TRUE(observer.process(f).targets.empty());
}
TEST(GameObserver, HoldRailAnchorSurvivesOneMissingRailButRequiresCurrentPairToRecover) {
    FakeClock clock;GameObserver observer(clock);std::uint64_t identity=0;
    for(int i=0;i<3;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,783,300+i*10,144,250,{40,190,255});clock.set(f.capture_complete_ns);
        const auto s=observer.process(f);ASSERT_EQ(s.targets.size(),1);identity=s.targets[0].note_id;
    }
    for(int i=0;i<3;++i) {
        auto f=image(i+4,(i+3)*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        if(i!=1) rect(f,778,200,2,376,{230,230,230});
        rect(f,932,200,2,376,{230,230,230});rect(f,783,552,144,20,{160,175,185});
        rect(f,794,360,110,112,{40,190,255});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        const auto held=std::find_if(s.targets.begin(),s.targets.end(),[&](const auto& t){return t.note_id==identity;});
        if(i==1) {
            EXPECT_TRUE(std::none_of(s.targets.begin(),s.targets.end(),[](const auto& t){return t.note.outline_evidence;}));
        } else {
            ASSERT_NE(held,s.targets.end());EXPECT_TRUE(held->note.outline_evidence);
            EXPECT_NEAR(held->note.center.x,855,2);EXPECT_NEAR(held->note.center.y,575,2);
            EXPECT_NEAR(held->note.width,i==0?144:154,2); // recovered current outer rail spacing
        }
    }
    auto old=image(7,240'000'000);hud(old);rect(old,0,575,1280,2,{255,255,255});
    rect(old,778,200,2,376,{230,230,230});rect(old,932,200,2,376,{230,230,230});
    rect(old,783,552,144,20,{160,175,185});clock.set(old.capture_complete_ns);
    EXPECT_TRUE(observer.process(old).targets.empty());
}
TEST(GameObserver, HeldTiltedBodyPreservesAnIndependentThinTapInsideItsRails) {
    FakeClock clock;GameObserver observer(clock);DecisionSnapshot result;std::uint64_t held_id=0,tap_id=0;
    const Vec2 u{.9916004112,-.1293391841},n{-u.y,u.x};
    const Vec2 on_line{640+u.x*220,575+u.y*220};
    for(int i=0;i<8;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);oriented_box(f,{640,575},u,1300,2,{255,255,255});
        const int offset=i<3?-30+i*10:0;
        const Vec2 head{on_line.x+n.x*offset,on_line.y+n.y*offset};
        const Vec2 body{head.x-n.x*100,head.y-n.y*100};
        oriented_box(f,body,u,144,200,i<3?std::array<std::uint8_t,3>{40,190,255}:
            std::array<std::uint8_t,3>{160,175,185});
        if(i>=3) {
            for(const int side:{-1,1}) oriented_box(f,
                {body.x+side*u.x*76,body.y+side*u.y*76},u,2,200,{245,245,245});
            const int depth=80-(i-3)*10;
            oriented_box(f,{on_line.x-n.x*depth,on_line.y-n.y*depth},u,120,8,{40,190,255});
        }
        clock.set(f.capture_complete_ns);result=observer.process(f);
        if(i<3) {ASSERT_EQ(result.targets.size(),1);held_id=result.targets[0].note_id;continue;}
        ASSERT_EQ(result.targets.size(),2)<<decision_json(result).dump();
        const auto held=std::find_if(result.targets.begin(),result.targets.end(),[](const auto& t){return t.note.kind==NoteKind::hold;});
        const auto thin=std::find_if(result.targets.begin(),result.targets.end(),[](const auto& t){return t.note.kind==NoteKind::tap;});
        ASSERT_NE(held,result.targets.end());ASSERT_NE(thin,result.targets.end());
        EXPECT_EQ(held->note_id,held_id);EXPECT_TRUE(held->note.outline_evidence);
        if(!tap_id) tap_id=thin->note_id;EXPECT_EQ(thin->note_id,tap_id);EXPECT_GT(thin->note.height,12);
    }
    const auto thin=std::find_if(result.targets.begin(),result.targets.end(),[](const auto& t){return t.note.kind==NoteKind::tap;});
    ASSERT_TRUE(thin->crossing_ns);EXPECT_NEAR(thin->velocity,500,20);
}
TEST(GameObserver, RecentHeldRegionRejectsEnclosedSideFragmentsWithoutRefreshingMissingRails) {
    FakeClock clock;GameObserver observer(clock);
    for(int i=0;i<3;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,778,375,2,201,{245,245,245});rect(f,932,375,2,201,{245,245,245});
        rect(f,783,375,144,200,{40,190,255});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);ASSERT_EQ(s.targets.size(),1);
    }
    auto f=image(4,60'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
    // Current full rails have disappeared. Mixed hit-overlay pixels alone
    // cannot refresh the original contact or move it to a side fragment.
    rect(f,783,375,144,200,{200,170,135});
    for(const int left:{872}) {
        rect(f,left,474,2,36,{245,245,245});rect(f,left+64,474,2,36,{245,245,245});
        rect(f,left+3,476,58,32,{40,190,255});
    }
    clock.set(f.capture_complete_ns);const auto s=observer.process(f);
    EXPECT_TRUE(s.targets.empty())<<decision_json(s).dump();
}
TEST(GameObserver, ApproachingHoldKeepsWholeGeometryWhenDecorativeLineSplitsItsCore) {
    FakeClock clock;GameObserver observer(clock);std::uint64_t identity=0;DecisionSnapshot result;
    for(int i=0;i<7;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int head=430+i*10,top=head-180;
        rect(f,400,top,2,184,{255,255,255});rect(f,550,top,2,184,{255,255,255});
        rect(f,403,top,144,180,{40,190,255});
        if(i>=3) rect(f,472,100,4,550,{255,255,255}); // Independent decorative line.
        clock.set(f.capture_complete_ns);result=observer.process(f);
        ASSERT_EQ(result.targets.size(),1);
        if(!identity) identity=result.targets[0].note_id;
        EXPECT_EQ(result.targets[0].note_id,identity);EXPECT_GT(result.targets[0].note.width,135);
        EXPECT_NEAR(result.targets[0].note.center.y,head-4,3);
        if(i>=3) EXPECT_TRUE(result.targets[0].note.outline_evidence||result.targets[0].note.direct_rails_evidence);
    }
    ASSERT_TRUE(result.targets[0].crossing_ns);EXPECT_NEAR(result.targets[0].velocity,500,20);
    EXPECT_LT(result.targets[0].residual,2);
}

TEST(GameObserver, ApproachingConnectedHoldWithInteriorHitTintKeepsOneIdentity) {
    for(const int overlay_depth:{38,28,18}) {
    FakeClock clock;GameObserver observer(clock);FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,2,{2,35'000'000,30'000'000});std::uint64_t identity=0;
    for(int i=0;i<12;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int head=std::min(575,490+i*10),top=std::max(0,head-538);
        rect(f,403,top,144,head-top,{40,190,255});
        rect(f,400,top,2,head-top,{245,245,245});rect(f,550,top,2,head-top,{245,245,245});
        // The overlay leaves both side strips connected to the lower head.
        // Its internal color boundary is not a second physical Hold head.
        if(i>=7) rect(f,421,head-overlay_depth,108,16,{210,197,146});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        ASSERT_EQ(s.targets.size(),1)<<"frame "<<i<<" "<<decision_json(s).dump();
        const auto& t=s.targets[0];if(!identity) identity=t.note_id;
        EXPECT_EQ(t.note_id,identity);EXPECT_EQ(t.note.kind,NoteKind::hold);
        EXPECT_NEAR(t.note.center.y,head-4,6);EXPECT_GT(t.samples,0);
        owner.accept(s);owner.poll();owner.take_accepted_plans();owner.take_plan_cancellations();
        if(i>=8) EXPECT_EQ(touch.contacts().size(),1)<<"frame "<<i;
    }
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    }
}

TEST(GameObserver, ApproachingHoldWithEnclosedHitPatchKeepsOneIdentityAcrossSparseFrames) {
    FakeClock clock;GameObserver observer(clock);FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,2,{2,35'000'000,30'000'000});std::uint64_t identity=0;
    const std::array<Nanoseconds,9> times{0,20'000'000,40'000'000,60'000'000,80'000'000,
        100'000'000,118'639'000,169'611'600,179'744'200};
    const std::array<int,9> heads{448,462,476,490,504,518,532,568,578};
    for(std::size_t i=0;i<times.size();++i) {
        auto f=image(i+1,times[i]);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int head=std::min(575,heads[i]);
        // Outer saturated strips remain connected. The warm frame encloses
        // a separate blue seed: a rectangle across the center alone cannot
        // reproduce the two descriptions of the same physical rail pair.
        rect(f,425,36,142,head-36,{40,190,255});
        rect(f,418,0,4,head,{245,245,245});rect(f,570,0,4,head,{245,245,245});
        if(i>=7) enclosed_hold_patch(f,head);
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        EXPECT_EQ(s.targets.size(),1)<<"frame "<<i<<" "<<decision_json(s).dump();
        if(!identity&&!s.targets.empty()) identity=s.targets[0].note_id;
        const auto held=std::find_if(s.targets.begin(),s.targets.end(),[&](const auto& t){return t.note_id==identity;});
        EXPECT_NE(held,s.targets.end())<<"frame "<<i;
        if(held!=s.targets.end()) {
            EXPECT_GT(held->samples,0);EXPECT_NE(held->reason,"association_ambiguous");
            EXPECT_NEAR(held->note.center.y,head-4,6);
        }
        owner.accept(s);owner.poll();owner.take_accepted_plans();
        if(i==6) {clock.set(155'000'000);owner.poll();}
        if(i>=6) EXPECT_EQ(touch.contacts().size(),1)<<"frame "<<i;
    }
    EXPECT_TRUE(std::none_of(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){
        return r.command.phase==Phase::up;}));
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){
        return r.command.phase==Phase::down;}),1);
}

TEST(GameObserver, InteriorHoldDescriptionDedupPreservesThinTapAndAdjacentHold) {
    FakeClock clock;GameObserver observer(clock);FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,2,{2,35'000'000,30'000'000});std::uint64_t first=0,second=0;
    for(int i=0;i<9;++i) {
        const Nanoseconds time=i<6?i*20'000'000:i==6?118'639'000:169'611'600+(i-7)*10'132'600;
        auto f=image(i+1,time);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int head=i<7?448+i*14:std::min(575,568+(i-7)*7);
        for(const int shift:{0,170}) {
            rect(f,425+shift,36,142,head-36,{40,190,255});
            rect(f,418+shift,0,4,head,{245,245,245});rect(f,570+shift,0,4,head,{245,245,245});
            if(i>=7) enclosed_hold_patch(f,head,shift);
        }
        // A separate thin ribbon is visible within the warm overlay. Its
        // own pixels must survive even while the enclosed body is deduped.
        if(i>=7) rect(f,455,head-34,80,8,{40,190,255});
        clock.set(time);const auto s=observer.process(f);
        EXPECT_EQ(s.targets.size(),i<7?2:3)<<decision_json(s).dump();
        const auto a=std::find_if(s.targets.begin(),s.targets.end(),[](const auto& t){
            return t.note.kind==NoteKind::hold&&t.note.center.x<580;});
        const auto b=std::find_if(s.targets.begin(),s.targets.end(),[](const auto& t){
            return t.note.kind==NoteKind::hold&&t.note.center.x>580;});
        ASSERT_NE(a,s.targets.end());ASSERT_NE(b,s.targets.end());
        if(i==0) {first=a->note_id;second=b->note_id;}
        EXPECT_EQ(a->note_id,first);EXPECT_EQ(b->note_id,second);EXPECT_NE(first,second);
        EXPECT_GT(a->samples,0);EXPECT_GT(b->samples,0);
        if(i>=7) EXPECT_EQ(std::count_if(s.targets.begin(),s.targets.end(),[](const auto& t){
            return t.note.kind==NoteKind::tap&&t.note.height<12;}),1);
        owner.accept(s);owner.poll();owner.take_accepted_plans();
        if(i==6) {clock.set(155'000'000);owner.poll();}
        if(i>=6) EXPECT_EQ(touch.contacts().size(),2);
    }
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){
        return r.command.phase==Phase::down;}),2);
}

TEST(GameObserver, InteriorHoldDescriptionNeedsBothCurrentSideBandsAndRecentFront) {
    for(const int missing:{0,1,2,3,4}) {
        FakeClock clock;GameObserver observer(clock);
        if(missing!=0) for(int i=0;i<7;++i) {
            auto f=image(i+1,i==6&&missing!=4?118'639'000:i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
            const int head=448+i*14;
            rect(f,425,36,142,head-36,{40,190,255});
            rect(f,418,0,4,head,{245,245,245});rect(f,570,0,4,head,{245,245,245});
            clock.set(f.capture_complete_ns);observer.process(f);
        }
        auto f=image(8,missing==3?221'000'000:missing==4?171'000'000:169'611'600);hud(f);
        rect(f,0,575,1280,2,{255,255,255});enclosed_hold_patch(f,568);
        // Break continuation eight pixels FORWARD of the reconstructed
        // inner front, while retaining its existing 4/24 px fill probes.
        if(missing==1) rect(f,425,532,3,2,{210,197,146});
        if(missing==2) rect(f,564,532,3,2,{210,197,146});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        EXPECT_GE(std::count_if(s.targets.begin(),s.targets.end(),[](const auto& t){
            return t.note.kind==NoteKind::hold;}),2)<<"missing "<<missing<<" "<<decision_json(s).dump();
    }
}

TEST(GameObserver, ClippedTopHoldKeepsCurrentRailsAndContactAcrossTheHeightLimit) {
    FakeClock clock;GameObserver observer(clock);FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,2,{2,35'000'000,30'000'000});std::uint64_t id=0;
    for(int i=0;i<14;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int head=std::min(575,500+i*10);
        rect(f,403,0,144,head,i<8?std::array<std::uint8_t,3>{40,190,255}:
            std::array<std::uint8_t,3>{160,175,185});
        rect(f,400,0,2,head,{245,245,245});rect(f,550,0,2,head,{245,245,245});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        ASSERT_EQ(s.targets.size(),1)<<"frame "<<i<<" "<<decision_json(s).dump();
        const auto& t=s.targets[0];ASSERT_EQ(t.note.kind,NoteKind::hold);
        if(!id) id=t.note_id;EXPECT_EQ(t.note_id,id);
        EXPECT_NEAR(t.note.center.x,475,4);EXPECT_NEAR(t.note.center.y,head-4,5);
        EXPECT_TRUE(t.note.rails_geometry);EXPECT_FALSE(t.note.tail); // Cropped tail stays unknown.
        if(i>=4&&i<8) {ASSERT_TRUE(t.crossing_ns);EXPECT_NEAR(t.velocity,500,20);}
        owner.accept(s);owner.poll();
        if(i>=7) EXPECT_EQ(touch.contacts().size(),1)<<"frame "<<i;
    }
    auto blank=image(15,280'000'000);hud(blank);rect(blank,0,575,1280,2,{255,255,255});
    clock.set(blank.capture_complete_ns);owner.accept(observer.process(blank));owner.poll();
    blank.sequence=16;blank.capture_complete_ns=340'000'000;clock.set(blank.capture_complete_ns);
    owner.accept(observer.process(blank));owner.poll();EXPECT_TRUE(touch.contacts().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){
        return r.command.phase==Phase::down;}),1);
}

TEST(GameObserver, ClippedTopHoldCanValidateCurrentLeadingEdgeWithoutPriorAnchor) {
    FakeClock clock;GameObserver observer(clock);DecisionSnapshot s;std::uint64_t id=0;
    for(int i=0;i<5;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int head=550+i*4; // Stay before the on-line Hold transition.
        rect(f,403,0,144,head,{40,190,255});
        rect(f,400,0,2,head,{245,245,245});rect(f,550,0,2,head,{245,245,245});
        clock.set(f.capture_complete_ns);s=observer.process(f);
        ASSERT_EQ(s.targets.size(),1)<<"frame "<<i<<" "<<decision_json(s).dump();
        const auto& t=s.targets[0];EXPECT_EQ(t.note.kind,NoteKind::hold);
        EXPECT_TRUE(t.note.direct_rails_evidence);EXPECT_TRUE(t.note.rails_geometry);
        EXPECT_FALSE(t.note.tail);EXPECT_NEAR(t.note.center.x,475,3);
        EXPECT_NEAR(t.note.center.y,head-4,4);
        if(!id) id=t.note_id;EXPECT_EQ(t.note_id,id);
    }
    ASSERT_TRUE(s.targets[0].crossing_ns);EXPECT_NEAR(s.targets[0].velocity,200,10);
    EXPECT_TRUE(s.playing_gate);
}

TEST(GameObserver, RecentHeldRailsRemainVisibleThroughWarmHitTint) {
    FakeClock clock;GameObserver observer(clock);FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,2,{2,35'000'000,30'000'000});std::uint64_t id=0;
    for(int i=0;i<18;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int head=std::min(575,500+i*10);
        rect(f,403,0,144,head,{40,190,255});
        rect(f,400,0,2,head,{245,245,245});rect(f,550,0,2,head,{245,245,245});
        if(i>=9) {
            // Live hit tint covers ~76 px of the near-line rails. The left
            // rail is neither neutral white nor saturated yellow.
            rect(f,400,495,2,80,{210,197,146});rect(f,550,495,2,80,{246,231,169});
        }
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        const auto h=std::find_if(s.targets.begin(),s.targets.end(),[](const auto& t){return t.note.kind==NoteKind::hold;});
        ASSERT_NE(h,s.targets.end())<<"frame "<<i<<" "<<decision_json(s).dump();
        if(!id) id=h->note_id;EXPECT_EQ(h->note_id,id);
        EXPECT_NEAR(h->note.center.x,475,4);EXPECT_NEAR(h->note.center.y,head-4,5);
        if(i>=9) {EXPECT_TRUE(h->note.outline_evidence);EXPECT_GT(h->note.height,480);}
        owner.accept(s);owner.poll();if(i>=7) EXPECT_EQ(touch.contacts().size(),1);
    }
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){
        return r.command.phase==Phase::down;}),1);
}

TEST(GameObserver, ColorIndependentHeldOutlineRequiresFreshAnchorAndBothCurrentRails) {
    for(int failure=0;failure<6;++failure) {
        FakeClock clock;GameObserver observer(clock);
        if(failure!=0) for(int i=0;i<9;++i) {
            auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
            const int head=std::min(575,500+i*10);
            rect(f,403,0,144,head,{40,190,255});
            rect(f,400,0,2,head,{245,245,245});rect(f,550,0,2,head,{245,245,245});
            clock.set(f.capture_complete_ns);observer.process(f);
        }
        auto f=image(10,failure==1?261'000'000:180'000'000);hud(f);
        rect(f,0,575,1280,2,{255,255,255});
        rect(f,403,0,144,575,failure==2?std::array<std::uint8_t,3>{100,110,120}:
            std::array<std::uint8_t,3>{40,190,255});
        rect(f,400,0,2,575,{245,245,245});rect(f,550,0,2,575,{245,245,245});
        rect(f,400,failure==3?0:495,2,failure==3?575:80,{210,197,146});
        rect(f,550,495,2,80,{246,231,169});
        if(failure==4) rect(f,400,0,2,575,{0,0,0});
        if(failure==5) rect(f,400,375,2,200,{210,197,146});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        if(failure==0||failure==1||failure==4) EXPECT_TRUE(s.targets.empty())<<"failure "<<failure;
        else {
            ASSERT_EQ(s.targets.size(),1)<<"case "<<failure;
            EXPECT_TRUE(s.targets.front().note.outline_evidence);EXPECT_TRUE(s.targets.front().note.head_on_line);
        }
    }
}

TEST(GameObserver, WarmTintUsesCurrentNeutralRailPairWhenCoreWidthWasBiased) {
    for(const bool cover_head:{false,true}) {
    FakeClock clock;GameObserver observer(clock);FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,2,{2,35'000'000,30'000'000});std::uint64_t id=0;
    for(int i=0;i<18;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int head=std::min(575,500+i*10);
        // A desaturated upper body leaves only a shorter saturated core.
        // Its complete core geometry is retained before the hit overlay.
        rect(f,403,0,144,head,{160,175,185});
        rect(f,403,head-200,144,200,{40,190,255});
        rect(f,396,0,2,head,{245,245,245});rect(f,554,0,2,head,{245,245,245});
        if(i>=9) {rect(f,396,495,2,80,{210,197,146});rect(f,554,495,2,80,{246,231,169});}
        if(i>=9&&cover_head) rect(f,421,560,108,15,{210,197,146});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        const auto h=std::find_if(s.targets.begin(),s.targets.end(),[](const auto& t){return t.note.kind==NoteKind::hold;});
        ASSERT_NE(h,s.targets.end())<<"frame "<<i<<" "<<decision_json(s).dump();
        if(!id) id=h->note_id;EXPECT_EQ(h->note_id,id);
        EXPECT_NEAR(h->note.center.x,475,4);EXPECT_NEAR(h->note.center.y,head-4,5);
        if(i>=9) {EXPECT_TRUE(h->note.outline_evidence);EXPECT_NEAR(h->note.width,158,3);EXPECT_GT(h->note.height,480);}
        owner.accept(s);owner.poll();if(i>=7) EXPECT_EQ(touch.contacts().size(),1);
    }
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){
        return r.command.phase==Phase::down;}),1);
    }
}

TEST(GameObserver, WarmSectionCannotUseEmptyOrMismatchedWhitePair) {
    for(int failure=0;failure<4;++failure) {
        FakeClock clock;GameObserver observer(clock);
        for(int i=0;i<9;++i) {
            auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
            const int head=std::min(575,500+i*10);
            rect(f,403,0,144,head,{160,175,185});rect(f,403,head-200,144,200,{40,190,255});
            rect(f,396,0,2,head,{245,245,245});rect(f,554,0,2,head,{245,245,245});
            clock.set(f.capture_complete_ns);observer.process(f);
        }
        auto f=image(10,180'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,403,0,144,575,{160,175,185});rect(f,403,375,144,200,{40,190,255});
        rect(f,396,0,2,575,{245,245,245});rect(f,554,0,2,575,{245,245,245});
        rect(f,396,495,2,80,{210,197,146});rect(f,554,495,2,80,{246,231,169});
        if(failure==0) for(const int y:{384,448}) rect(f,403,y-6,144,12,{0,0,0});
        else if(failure==3) rect(f,403,560,144,15,{0,0,0});
        else {
            rect(f,396,0,2,495,{0,0,0});rect(f,554,0,2,495,{0,0,0});
            // Both are inside the bounded search and have fill. One pair is
            // too wide; the other is shifted beyond the allowed center change.
            rect(f,failure==1?386:426,0,2,495,{245,245,245});
            rect(f,564,0,2,495,{245,245,245});
        }
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        if(failure==1||failure==2)
            EXPECT_TRUE(std::none_of(s.targets.begin(),s.targets.end(),[](const auto& t){return t.note.outline_evidence;}))<<"failure "<<failure;
        else {
            ASSERT_EQ(s.targets.size(),1);EXPECT_TRUE(s.targets.front().note.outline_evidence);
            EXPECT_NEAR(s.targets.front().note.width,158,3);
        }
    }
}

TEST(GameObserver, ClippedBlueColumnWithoutBothRailsCannotBecomeANewHold) {
    for(int rails=0;rails<2;++rails) {
        FakeClock clock;GameObserver observer(clock);auto f=image(1,1);hud(f);
        rect(f,0,575,1280,2,{255,255,255});rect(f,403,0,144,560,{40,190,255});
        if(rails) rect(f,400,0,2,560,{245,245,245});
        clock.set(1);EXPECT_TRUE(observer.process(f).targets.empty());
    }
    FakeClock clock;GameObserver observer(clock);auto f=image(1,1);hud(f);
    rect(f,0,575,1280,2,{255,255,255});
    rect(f,400,0,2,560,{245,245,245});rect(f,550,0,2,560,{245,245,245});
    clock.set(1);EXPECT_TRUE(observer.process(f).targets.empty()); // No body fill.
}
TEST(GameObserver, CurrentRailsReconstructSplitGradientBodyWithoutAnEarlierWholeCore) {
    FakeClock clock;GameObserver observer(clock);DecisionSnapshot result;std::uint64_t id=0;
    const Vec2 u{.9987523389,-.04993761694},n{-u.y,u.x};
    for(int i=0;i<5;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);
        oriented_box(f,{640,600},u,1300,2,{255,255,255});
        const Vec2 head{530+n.x*i*10,510+n.y*i*10};
        const Vec2 body{head.x-n.x*150,head.y-n.y*150};
        oriented_box(f,body,u,146,300,{135,145,150});
        oriented_box(f,{head.x-n.x*35,head.y-n.y*35},u,146,70,{140,210,240});
        // A yellow hit overlay inside the body must not create a new head
        // or truncate both rails into a matching but false short tail.
        oriented_box(f,{head.x-n.x*45,head.y-n.y*45},u,146,16,{255,220,40});
        for(const int side:{-1,1}) oriented_box(f,
            {body.x+side*u.x*76,body.y+side*u.y*76},u,2,300,{245,245,245});
        if(i>=3) oriented_box(f,{head.x-n.x*55-u.x*76,head.y-n.y*55-u.y*76},u,4,24,{0,0,0});
        rect(f,594,80,4,560,{255,255,255}); // Internal decoration extends past both ends.
        oriented_box(f,{960,540},u,140,8,{40,190,255});
        clock.set(f.capture_complete_ns);result=observer.process(f);
        ASSERT_EQ(result.targets.size(),2)<<"frame "<<i<<" "<<decision_json(result).dump();
        const auto held=std::find_if(result.targets.begin(),result.targets.end(),[](const auto& t){return t.note.kind==NoteKind::hold;});
        ASSERT_NE(held,result.targets.end());EXPECT_NEAR(held->note.width,152,5);
        EXPECT_NEAR(held->note.center.x,head.x,4);EXPECT_NEAR(held->note.center.y,head.y-3,4);
        ASSERT_TRUE(held->note.tail);EXPECT_NEAR(held->note.tail->y,head.y-n.y*300,5);
        if(!id) id=held->note_id;EXPECT_EQ(held->note_id,id);
        if(i==0) EXPECT_TRUE(held->note.direct_rails_evidence);
    }
    const auto held=std::find_if(result.targets.begin(),result.targets.end(),[](const auto& t){return t.note.kind==NoteKind::hold;});
    ASSERT_TRUE(held->crossing_ns);EXPECT_NEAR(held->velocity,500,20);
}
TEST(GameObserver, ShortWideCurrentRailsNeedDepthAndCannotJoinAdjacentBodiesOrEmptyBorders) {
    FakeClock clock;GameObserver observer(clock);auto f=image(1,1);hud(f);
    rect(f,0,575,1280,2,{255,255,255});
    // A 50 px Hold is too thick for a Tap, but shorter than the old aspect gate.
    rect(f,400,450,2,50,{245,245,245});rect(f,544,450,2,50,{245,245,245});
    rect(f,403,450,138,50,{40,190,255});
    // A thin ribbon with short white edge flashes must stay a Tap.
    rect(f,780,490,2,12,{245,245,245});rect(f,924,490,2,12,{245,245,245});
    rect(f,783,492,138,8,{40,190,255});
    // Empty long borders and a decoration do not make an unseen body.
    rect(f,100,200,2,300,{245,245,245});rect(f,244,200,2,300,{245,245,245});
    rect(f,320,100,2,550,{245,245,245});
    clock.set(1);const auto s=observer.process(f);ASSERT_EQ(s.targets.size(),2);
    const auto held=std::find_if(s.targets.begin(),s.targets.end(),[](const auto& t){return t.note.kind==NoteKind::hold;});
    ASSERT_NE(held,s.targets.end());EXPECT_TRUE(held->note.direct_rails_evidence);
    EXPECT_NEAR(held->note.center.y,496,3);EXPECT_NEAR(held->note.width,144,3);
    EXPECT_EQ(std::count_if(s.targets.begin(),s.targets.end(),[](const auto& t){return t.note.kind==NoteKind::tap;}),1);
    auto adjacent=image(2,200'000'000);hud(adjacent);rect(adjacent,0,575,1280,2,{255,255,255});
    for(const int left:{600,740}) {
        rect(adjacent,left,350,2,50,{245,245,245});rect(adjacent,left+80,350,2,50,{245,245,245});
        rect(adjacent,left+3,350,74,50,{40,190,255});
    }
    clock.set(adjacent.capture_complete_ns);const auto separate=observer.process(adjacent);
    ASSERT_EQ(separate.targets.size(),2);
    EXPECT_TRUE(std::all_of(separate.targets.begin(),separate.targets.end(),[](const auto& t) {
        return t.note.kind==NoteKind::hold&&t.note.width<90&&t.note.direct_rails_evidence;
    }));
}
TEST(GameObserver, CurrentHoldReconstructionRejectsLongRailGapDespiteBodyFill) {
    FakeClock clock;GameObserver observer(clock);auto f=image(1,1);hud(f);
    rect(f,0,575,1280,2,{255,255,255});
    rect(f,400,400,2,70,{245,245,245});rect(f,544,400,2,70,{245,245,245});
    rect(f,403,400,138,70,{40,190,255});
    rect(f,400,414,2,44,{0,0,0});
    clock.set(1);const auto s=observer.process(f);EXPECT_TRUE(s.targets.empty())<<decision_json(s).dump();
}
TEST(GameObserver, HoldRailsCannotExpandToDecorationWithFillMissingOnOneSide) {
    FakeClock clock;GameObserver observer(clock);auto f=image(1,1);hud(f);
    rect(f,0,575,1280,2,{255,255,255});
    rect(f,400,250,2,250,{245,245,245});rect(f,550,250,2,250,{245,245,245});
    rect(f,403,250,144,250,{40,190,255});
    // A long decorative line can satisfy the loose paired-rail extent.
    // It cannot widen the body: its adjacent inner side has no current fill.
    rect(f,600,100,2,550,{245,245,245});
    clock.set(1);const auto s=observer.process(f);ASSERT_EQ(s.targets.size(),1)<<decision_json(s).dump();
    EXPECT_NEAR(s.targets[0].note.width,144,10)<<decision_json(s).dump();
    EXPECT_NEAR(s.targets[0].note.center.x,475,3);
}
TEST(GameObserver, ShrinkingHoldKeepsHeadIdentityButThinNewTapCannotInheritIt) {
    FakeClock clock;GameObserver observer(clock);std::uint64_t identity=0;int frame=0;
    for(const int height:{160,120,100,80,60,40,20}) {
        auto f=image(++frame,frame*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,400,576-height,2,height,{230,230,230});rect(f,542,576-height,2,height,{230,230,230});
        rect(f,403,576-height,136,height,{40,190,255});clock.set(f.capture_complete_ns);
        const auto s=observer.process(f);ASSERT_EQ(s.targets.size(),1);
        if(!identity) identity=s.targets[0].note_id;
        EXPECT_EQ(s.targets[0].note_id,identity);EXPECT_EQ(s.targets[0].note.kind,NoteKind::hold);
        if(s.targets[0].note.outline_evidence) EXPECT_NEAR(s.targets[0].note.center.y,576,1);
        else EXPECT_NEAR(s.targets[0].note.center.y,572,3);
    }
    auto f=image(++frame,frame*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
    rect(f,403,568,136,8,{40,190,255});clock.set(f.capture_complete_ns);
    const auto s=observer.process(f);ASSERT_EQ(s.targets.size(),1);
    EXPECT_NE(s.targets[0].note_id,identity);EXPECT_EQ(s.targets[0].note.kind,NoteKind::tap);
}
TEST(GameObserver, SmallHitParticlesAndYellowRingsCannotBecomeNoteTargets) {
    FakeClock clock;GameObserver observer(clock);auto f=image(1,1);
    rect(f,200,200,24,24,{40,190,255});rect(f,300,200,32,32,{255,220,40});
    rect(f,400,200,60,50,{255,220,40});rect(f,500,200,60,8,{40,190,255});
    rect(f,700,200,92,46,{40,190,255}); // Thick hit flash competing with a Hold head.
    clock.set(1);const auto s=observer.process(f);ASSERT_EQ(s.targets.size(),1);
    EXPECT_EQ(s.targets[0].note.kind,NoteKind::tap);EXPECT_GT(s.targets[0].note.center.x,490);
}
TEST(GameObserver, ShortWideHoldUsesLineNormalAndStationaryHeadStillHasHitRegion) {
    FakeClock clock;GameObserver observer(clock);DecisionSnapshot s;
    for(int i=0;i<4;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,400,200,2,320,{255,255,255});rect(f,542,200,2,320,{255,255,255});
        rect(f,403,420,136,100,{40,190,255});clock.set(f.capture_complete_ns);s=observer.process(f);
    }
    ASSERT_EQ(s.targets.size(),1);const auto& t=s.targets[0];EXPECT_EQ(t.note.kind,NoteKind::hold);
    EXPECT_NEAR(t.note.tangent.x,1,.01);EXPECT_NEAR(t.note.center.y,516,2);
    ASSERT_TRUE(t.note.tail);EXPECT_NEAR(t.note.tail->y,200,2);
    EXPECT_NEAR(t.hit.x,t.note.center.x,2);EXPECT_NEAR(t.hit.y,575,2);
    EXPECT_FALSE(t.crossing_ns);
}
TEST(GameObserver, SplitFlickRequiresCentralArrowAndHighlightCannotBecomeDrag) {
    FakeClock clock; GameObserver observer(clock); auto f=image(1,1);
    rect(f,200,380,70,20,{255,220,40}); rect(f,282,380,70,20,{255,220,40});
    rect(f,212,386,50,6,{255,65,115}); rect(f,294,386,50,6,{255,65,115});
    rect(f,274,376,8,28,{255,255,255});
    rect(f,612,386,50,6,{255,65,115}); rect(f,694,386,50,6,{255,65,115});
    clock.set(1); const auto s=observer.process(f);
    ASSERT_EQ(s.targets.size(),3);
    EXPECT_TRUE(std::all_of(s.targets.begin(),s.targets.end(),[](const auto& t){return t.note.kind==NoteKind::flick;}));
    const auto joined=std::find_if(s.targets.begin(),s.targets.end(),[](const auto& t){return t.note.center.x<500;});
    ASSERT_NE(joined,s.targets.end()); EXPECT_NEAR(joined->note.center.x,277,2);
    EXPECT_GT(joined->note.width,125);
}
TEST(GameObserver, TallSimultaneousHighlightKeepsOnlyTheCoreAndIndependentDrag) {
    FakeClock clock;GameObserver observer(clock);DecisionSnapshot result;
    for(int i=0;i<6;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int y=460+i*10;
        // Connected outer highlight with tall end caps and a thin blue core.
        // The yellow component is 148x26, matching the rejected live candidate.
        rect(f,336,y-13,148,4,{255,240,20});
        rect(f,336,y-13,4,26,{255,240,20});rect(f,480,y-13,4,26,{255,240,20});
        rect(f,346,y-4,128,8,{40,190,255});
        rect(f,759,y-4,128,8,{245,40,120});
        rect(f,1000,y-4,128,8,{255,240,20});
        clock.set(f.capture_complete_ns);result=observer.process(f);
        ASSERT_EQ(result.targets.size(),3)<<decision_json(result).dump();
        EXPECT_EQ(std::count_if(result.targets.begin(),result.targets.end(),[](const auto& t){return t.note.kind==NoteKind::drag;}),1);
    }
    for(const auto& t:result.targets) {ASSERT_TRUE(t.crossing_ns);EXPECT_NEAR(t.velocity,500,20);}
}
TEST(GameObserver, ShortCaptureBurstCannotPredictAnImminentCrossingFromFarNotes) {
    FakeClock clock;GameObserver observer(clock);DecisionSnapshot result;
    for(int i=0;i<3;++i) {
        auto f=image(i+1,i*1'800'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,359,236+i*10,128,6,{255,240,20});
        clock.set(f.capture_complete_ns);result=observer.process(f);
    }
    ASSERT_TRUE(result.playing_gate);ASSERT_EQ(result.targets.size(),1);
    // Three near-instant host deliveries do not establish a reliable speed.
    // This live failure yielded ~5,467 px/s with nearly zero fit residual.
    EXPECT_FALSE(result.targets[0].crossing_ns)<<decision_json(result).dump();
    for(int i=1;i<=7;++i) {
        auto f=image(i+3,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,359,244+i*10,128,6,{255,240,20});
        clock.set(f.capture_complete_ns);result=observer.process(f);
    }
    ASSERT_EQ(result.targets.size(),1);
    EXPECT_FALSE(result.targets[0].crossing_ns);EXPECT_EQ(result.targets[0].reason,"outside_short_horizon");
    EXPECT_GE(result.targets[0].history_span_ns,30'000'000);
    EXPECT_NEAR(result.targets[0].velocity,500,20);
}
TEST(GameObserver, ContinuousFastFramesKeepEnoughTemporalHistoryToPredict) {
    FakeClock clock;GameObserver observer(clock);DecisionSnapshot result;
    for(int i=0;i<25;++i) {
        auto f=image(i+1,i*4'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,359,400+i*2,128,6,{255,240,20});
        clock.set(f.capture_complete_ns);result=observer.process(f);
        ASSERT_EQ(result.targets.size(),1);
        EXPECT_LE(result.targets[0].samples,6);EXPECT_LE(result.targets[0].history_span_ns,90'000'000);
    }
    ASSERT_TRUE(result.targets[0].crossing_ns);EXPECT_GE(result.targets[0].history_span_ns,30'000'000);
    EXPECT_NEAR(result.targets[0].velocity,500,20);
}
TEST(GameObserver, TimingUncertaintySeparatesFastCaptureJitterFromSlowAmbiguousMotion) {
    for(const double speed:{800.0,100.0}) {
        SCOPED_TRACE(speed);FakeClock clock;GameObserver observer(clock);DecisionSnapshot scene;
        const std::array<int,6> jitter{0,12,-12,-12,12,0};
        for(int i=0;i<6;++i) {
            auto f=image(i+1,1'000'000+i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
            const double base=speed==800?455:560;
            const int y=static_cast<int>(std::lround(base+speed*i*.02+jitter[i]));
            rect(f,336,y-4,128,8,{40,190,255});clock.set(f.capture_complete_ns);scene=observer.process(f);
        }
        ASSERT_EQ(scene.targets.size(),1);const auto& t=scene.targets[0];ASSERT_TRUE(t.crossing_ns);
        EXPECT_GT(t.residual,8);EXPECT_LE(t.residual,16);EXPECT_EQ(t.fit_residual_limit_px,16);
        EXPECT_GE(t.prediction_error_px,t.residual);EXPECT_EQ(t.reason,"prediction_observe_only");
        EXPECT_NEAR(t.velocity,speed,2);EXPECT_EQ(t.hit.y,576);
        FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{1,35'000'000,30'000'000});
        owner.accept(scene);const auto plans=owner.take_accepted_plans();
        if(speed==800) {
            EXPECT_LT(t.uncertainty_ns,30'000'000);ASSERT_EQ(plans.size(),1);
            clock.set(plans[0].steps[0].due_ns);owner.poll();ASSERT_EQ(touch.contacts().size(),1);
        } else {EXPECT_GT(t.uncertainty_ns,30'000'000);EXPECT_TRUE(plans.empty());EXPECT_TRUE(touch.contacts().empty());}
        owner.stop();
    }
}
TEST(GameObserver, CurrentPointMismatchAndLargeGeometryJumpsRemainRejectedAtHighSpeed) {
    FakeClock clock;GameObserver observer(clock);DecisionSnapshot scene;
    for(int i=0;i<6;++i) {
        auto f=image(i+1,1'000'000+i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int y=330+i*16+(i==5?64:0);rect(f,336,y-4,128,8,{40,190,255});
        clock.set(f.capture_complete_ns);scene=observer.process(f);
    }
    ASSERT_EQ(scene.targets.size(),1);const auto& t=scene.targets[0];
    EXPECT_FALSE(t.crossing_ns);EXPECT_EQ(t.reason,"nonlinear_or_mismatch");
    EXPECT_GT(t.prediction_error_px,t.fit_residual_limit_px);
    FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{1,35'000'000,30'000'000});
    owner.accept(scene);EXPECT_TRUE(owner.take_accepted_plans().empty());owner.poll();EXPECT_TRUE(touch.contacts().empty());
}
TEST(GameOwner, NewTimingUncertaintyCancelsPendingDownAndFreshEvidenceCanRetryExactlyOnce) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    auto s=snapshot(1,0);auto t=target(1,0,50'000'000);t.samples=3;s.targets={t};
    owner.accept(s);ASSERT_EQ(owner.take_accepted_plans().size(),1);
    clock.set(20'000'000);s=snapshot(2,clock.now_ns());t.revision++;t.evidence_ns=clock.now_ns();
    t.expires_ns=clock.now_ns()+100'000'000;t.crossing_ns=60'000'000;t.uncertainty_ns=45'000'000;s.targets={t};
    owner.accept(s);owner.poll();const auto canceled=owner.take_plan_cancellations();ASSERT_EQ(canceled.size(),1);
    EXPECT_EQ(canceled[0].at("reason"),"timing_uncertainty_exceeds_limit");EXPECT_TRUE(touch.receipts().empty());
    clock.set(40'000'000);s=snapshot(3,clock.now_ns());t.revision++;t.evidence_ns=clock.now_ns();
    t.expires_ns=clock.now_ns()+100'000'000;t.crossing_ns=70'000'000;t.uncertainty_ns=2'000'000;s.targets={t};
    owner.accept(s);ASSERT_EQ(owner.take_accepted_plans().size(),1);
    clock.set(50'000'000);owner.poll();EXPECT_TRUE(touch.contacts().empty());
    clock.set(70'000'000);owner.poll();ASSERT_EQ(touch.contacts().size(),1);
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}
TEST(GameObserver, FitResidualDoesNotDisplaceTheHitPointFromTheObservedLine) {
    FakeClock clock;GameObserver observer(clock);DecisionSnapshot result;
    constexpr int jitter[]{0,4,-4,4,-4,4};
    for(int i=0;i<6;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,359,360+i*20+jitter[i],128,6,{255,240,20});
        clock.set(f.capture_complete_ns);result=observer.process(f);
    }
    ASSERT_EQ(result.targets.size(),1);ASSERT_EQ(result.lines.size(),1);
    const auto& t=result.targets[0];const auto& line=result.lines[0];ASSERT_TRUE(t.crossing_ns);
    EXPECT_LE(t.residual,8);
    const double hit_distance=-(t.hit.x-line.center.x)*line.tangent.y+(t.hit.y-line.center.y)*line.tangent.x;
    EXPECT_NEAR(hit_distance,0,.01)<<decision_json(result).dump();
}
TEST(GameObserver, SlantedLineSurvivesIntersectionAndMatchesNoteOrientation) {
    FakeClock clock; GameObserver observer(clock); DecisionSnapshot s;
    const Vec2 u{.9950371902,-.0995037190},n{.0995037190,.9950371902};
    for(int i=0;i<5;++i) {
        auto f=image(i+1,i*20'000'000); hud(f);
        rect(f,638,90,4,594,{255,255,255});
        for(int x=0;x<1280;++x) rect(f,x,static_cast<int>(std::lround(550-.1*(x-640))),1,3,{255,255,255});
        const Vec2 p{640-u.x*250+n.x*(-100+i*8),550-u.y*250+n.y*(-100+i*8)};
        for(int y=static_cast<int>(p.y-12);y<p.y+12;++y) for(int x=static_cast<int>(p.x-34);x<p.x+34;++x) {
            const double along=(x-p.x)*u.x+(y-p.y)*u.y,across=(x-p.x)*n.x+(y-p.y)*n.y;
            if(std::abs(along)<30&&std::abs(across)<4) rect(f,x,y,1,1,{40,190,255});
        }
        clock.set(f.capture_complete_ns); s=observer.process(f);
    }
    ASSERT_EQ(s.targets.size(),1); ASSERT_TRUE(s.targets[0].crossing_ns);
    EXPECT_NEAR(s.targets[0].velocity,400,25);
    EXPECT_LT(s.targets[0].residual,2); EXPECT_NEAR(s.targets[0].hit.y,550-.1*(s.targets[0].hit.x-640),3);
}
TEST(GameScheduler, ActivePrefixCompactionKeepsLastExecutedStepAndCannotDropFuture) {
    FakeClock clock; FakeTouchBackend backend(clock); ContactScheduler scheduler(clock,backend);
    ASSERT_TRUE(scheduler.set_gate(1,true,0)); auto p=tap(1,0,10'000'000);
    p.steps={{Phase::down,20,30,10'000'000},{Phase::move,25,30,20'000'000},
             {Phase::move,30,30,30'000'000},{Phase::up,30,30,40'000'000}};
    ASSERT_TRUE(scheduler.submit(p)); clock.set(20'000'000); ASSERT_EQ(scheduler.run_due().size(),2);
    p.revision=2; p.evidence_ns=20'000'000; p.prefix_offset=1; p.steps.erase(p.steps.begin());
    ASSERT_TRUE(scheduler.submit(p)); EXPECT_EQ(scheduler.executed_steps(1),2);
    auto bad=p; bad.revision=3; bad.prefix_offset=2; bad.steps.erase(bad.steps.begin());
    EXPECT_FALSE(scheduler.submit(bad));
    clock.set(30'000'000); ASSERT_EQ(scheduler.run_due().size(),1);
    clock.set(40'000'000); ASSERT_EQ(scheduler.run_due().size(),1); EXPECT_TRUE(backend.contacts().empty());
}
TEST(GameOwner, FreshPredictionRefinesPendingDeadlineButCannotReplayActiveDown) {
    FakeClock clock; FakeTouchBackend backend(clock); GamePlanOwner owner(clock,backend,2,{1,0,30'000'000});
    auto s=snapshot(1,0); auto t=target(1,0,50'000'000); t.samples=3;s.targets={t};
    owner.accept(s); owner.take_accepted_plans();
    clock.set(20'000'000);s=snapshot(2,clock.now_ns());t.revision=2;t.evidence_ns=clock.now_ns();
    t.expires_ns=clock.now_ns()+100'000'000;t.crossing_ns=60'000'000;t.hit.x=410;s.targets={t};
    owner.accept(s);auto revised=owner.take_accepted_plans();ASSERT_EQ(revised.size(),1);
    EXPECT_EQ(revised[0].steps.front().due_ns,60'000'000);
    clock.set(50'000'000);EXPECT_TRUE(owner.poll().empty());
    clock.set(60'000'000);ASSERT_EQ(owner.poll().size(),1);EXPECT_EQ(backend.receipts().back().command.x,410);
    s=snapshot(3,clock.now_ns());t.revision=3;t.evidence_ns=clock.now_ns();t.crossing_ns=70'000'000;
    t.hit.x=420;s.targets={t};owner.accept(s);owner.take_accepted_plans();
    clock.set(78'000'000);ASSERT_EQ(owner.poll().size(),1);
    EXPECT_EQ(backend.receipts().size(),2);EXPECT_TRUE(backend.contacts().empty());
}
TEST(GameOwner, ContradictedPendingPredictionCannotInjectButFreshPredictionCanRetry) {
    for(const auto kind:{NoteKind::tap,NoteKind::hold,NoteKind::drag,NoteKind::flick})
    for(const auto* reason:{"nonlinear_or_mismatch","outside_short_horizon","root_past"}) {
        SCOPED_TRACE(std::string(name(kind))+" "+reason);
        FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,2,{15,0,30'000'000});
        auto t=target(1,0,50'000'000);t.note.kind=kind;t.samples=4;
        auto s=snapshot(1,0);s.targets={t};owner.accept(s);owner.take_accepted_plans();
        clock.set(20'000'000);s=snapshot(2,clock.now_ns());t.evidence_ns=clock.now_ns();
        t.expires_ns=clock.now_ns()+100'000'000;t.revision++;t.crossing_ns.reset();t.reason=reason;
        t.distance=-125;t.velocity=524;t.residual=20;s.targets={t};owner.accept(s);owner.take_accepted_plans();
        EXPECT_EQ(owner.scheduler().pending_count(),0);
        const auto canceled=owner.take_plan_cancellations();ASSERT_EQ(canceled.size(),1);
        EXPECT_EQ(canceled[0].at("reason"),reason);EXPECT_EQ(canceled[0].at("source_frame"),2);
        EXPECT_EQ(canceled[0].at("prior_source_frame"),1);EXPECT_EQ(canceled[0].at("evidence_ns"),20'000'000);
        EXPECT_EQ(canceled[0].at("prior_evidence_ns"),0);EXPECT_TRUE(canceled[0].at("no_down_injected").get<bool>());
        EXPECT_TRUE(owner.take_plan_cancellations().empty());
        clock.set(60'000'000);owner.poll();EXPECT_TRUE(touch.receipts().empty());
        clock.set(80'000'000);s=snapshot(3,clock.now_ns());t.evidence_ns=clock.now_ns();
        t.expires_ns=clock.now_ns()+100'000'000;t.revision++;t.crossing_ns=100'000'000;
        t.reason="prediction_observe_only";t.distance=-10;t.velocity=500;t.residual=1;
        s.targets={t};owner.accept(s);owner.take_accepted_plans();
        clock.set(100'000'000);owner.poll();
        const auto down=std::find_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;});
        ASSERT_NE(down,touch.receipts().end());EXPECT_EQ(down->command.source_frame_sequence,3);
        EXPECT_EQ(down->injection_start_ns,100'000'000);
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    }
}
TEST(GameOwner, ContradictionAfterDownCannotReplayOrCancelTheActiveDragWindow) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,2,{4,0,30'000'000});
    auto t=target(1,0,50'000'000);t.note.kind=NoteKind::drag;t.samples=4;
    auto s=snapshot(1,0);s.targets={t};owner.accept(s);owner.take_accepted_plans();
    clock.set(35'000'000);ASSERT_EQ(owner.poll().size(),1);
    clock.set(45'000'000);s=snapshot(2,clock.now_ns());t.evidence_ns=clock.now_ns();
    t.expires_ns=clock.now_ns()+100'000'000;t.revision++;t.crossing_ns.reset();
    t.reason="nonlinear_or_mismatch";s.targets={t};owner.accept(s);owner.take_accepted_plans();
    EXPECT_TRUE(owner.take_plan_cancellations().empty());
    EXPECT_EQ(touch.contacts().size(),1);
    clock.set(124'000'000);owner.poll();EXPECT_EQ(touch.contacts().size(),1);
    clock.set(125'000'000);owner.poll();EXPECT_TRUE(touch.contacts().empty());
    clock.set(130'000'000);s=snapshot(3,clock.now_ns());t.evidence_ns=clock.now_ns();
    t.expires_ns=clock.now_ns()+100'000'000;t.crossing_ns=140'000'000;t.reason="prediction_observe_only";
    t.revision++;s.targets={t};owner.accept(s);clock.set(140'000'000);owner.poll();
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    owner.stop();
}
TEST(GameOwner, HoldRefreshStaysBoundedThenMissingEvidenceReleasesContact) {
    FakeClock clock; FakeTouchBackend backend(clock); GamePlanOwner owner(clock,backend,2,{3,0,30'000'000});
    auto s=snapshot(1,0); auto h=target(1,0,10'000'000); h.note.kind=NoteKind::hold;
    h.tail_crossing_ns=1'000'000'000; h.samples=3; s.targets={h}; owner.accept(s); owner.take_accepted_plans();
    clock.set(10'000'000); ASSERT_EQ(owner.poll().size(),1);
    for(int i=1;i<=35;++i) {
        clock.set(10'000'000+i*20'000'000); s=snapshot(i+1,clock.now_ns());
        h.evidence_ns=clock.now_ns(); h.expires_ns=clock.now_ns()+100'000'000; h.revision=i+1;
        h.hit.x=400+i*3; s.targets={h}; owner.accept(s); owner.poll();
        for(const auto& p:owner.take_accepted_plans()) EXPECT_LE(p.steps.size(),4);
        EXPECT_EQ(backend.contacts().size(),1);
    }
    clock.set(clock.now_ns()+60'000'000); s=snapshot(40,clock.now_ns()); owner.accept(s);
    EXPECT_TRUE(backend.contacts().empty()); EXPECT_TRUE(owner.scheduler().fault().empty());
}
TEST(GameOwner, FiveHoldsKeepIndependentContactsUntilCurrentTailsActuallyPass) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,5,{2,0,30'000'000});
    auto s=snapshot(1,0);s.lines={{{640,500},{1,0},1280,2,1}};
    for(int id=0;id<5;++id) {
        auto h=target(id+1,0,10'000'000);h.note.kind=NoteKind::hold;h.samples=4;
        h.hit={100.0+id*220,500};h.note.center=h.hit;h.note.width=100;h.note.height=300;
        h.note.tail=Vec2{h.hit.x,200};h.note.rails_geometry=true;h.tail_crossing_ns=30'000'000;
        s.targets.push_back(h);
    }
    owner.accept(s);owner.take_accepted_plans();clock.set(10'000'000);owner.poll();
    ASSERT_EQ(touch.contacts().size(),5);
    for(int i=1;i<=7;++i) {
        clock.set(10'000'000+i*20'000'000);s.sequence++;s.context.frame++;s.context.capture_ns=clock.now_ns();
        for(auto& h:s.targets) {
            h.evidence_ns=clock.now_ns();h.expires_ns=clock.now_ns()+100'000'000;h.revision++;
            h.tail_crossing_ns=clock.now_ns()-10'000'000; // misleading fit must not release early
        }
        owner.accept(s);owner.poll();owner.take_accepted_plans();EXPECT_EQ(touch.contacts().size(),5);
    }
    clock.set(160'000'000);s.sequence++;s.context.frame++;s.context.capture_ns=clock.now_ns();
    for(auto& h:s.targets) {h.evidence_ns=clock.now_ns();h.expires_ns=clock.now_ns()+100'000'000;h.revision++;
        h.note.tail=Vec2{h.hit.x,502};h.note.center={h.hit.x,520};}
    owner.accept(s);owner.take_accepted_plans();
    EXPECT_TRUE(owner.take_coverage_updates().empty()); // one tail sample cannot finish
    clock.set(170'000'000);s.sequence++;s.context.frame++;s.context.capture_ns=clock.now_ns();
    for(auto& h:s.targets) {h.evidence_ns=clock.now_ns();h.expires_ns=clock.now_ns()+100'000'000;h.revision++;}
    owner.accept(s);owner.poll();EXPECT_EQ(touch.contacts().size(),5);
    const auto ends=owner.take_coverage_updates();ASSERT_EQ(ends.size(),5);
    for(const auto& e:ends) EXPECT_EQ(e.at("release_ns"),190'000'000);
    clock.set(180'000'000);owner.poll();EXPECT_EQ(touch.contacts().size(),5);
    clock.set(190'000'000);owner.poll();EXPECT_TRUE(touch.contacts().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),5);
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::up;}),5);
    owner.accept(snapshot(20,190'000'000));owner.stop();EXPECT_TRUE(owner.scheduler().fault().empty());
}
TEST(GameOwner, TailPredictionWithoutCurrentVisibleTailDoesNotEndHoldOrExtendExpiry) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{2,0,30'000'000});
    auto h=target(1,0,10'000'000);h.note.kind=NoteKind::hold;h.samples=3;h.tail_crossing_ns=0;
    auto s=snapshot(1,0);s.targets={h};owner.accept(s);owner.take_accepted_plans();
    clock.set(10'000'000);owner.poll();clock.set(50'000'000);owner.poll();ASSERT_EQ(touch.contacts().size(),1);
    clock.set(100'000'000);owner.poll();EXPECT_TRUE(touch.contacts().empty());
}
TEST(GameOwner, MovingNearLineDragKeepsOneFingerWithoutRepeatedCrossingFits) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{4,0,30'000'000});
    auto t=target(1,0,30'000'000);t.note.kind=NoteKind::drag;t.note.width=120;t.note.height=8;t.samples=4;t.line_id=7;
    auto s=snapshot(1,0);s.targets={t};owner.accept(s);clock.set(15'000'000);owner.poll();
    const auto finger=touch.contacts().begin()->first;
    for(int i=1;i<=15;++i) {
        clock.set(i*20'000'000);t.evidence_ns=clock.now_ns();t.expires_ns=clock.now_ns()+100'000'000;
        t.hit.x+=12;t.hit.y+=4;t.revision++;t.crossing_ns.reset();t.reason="relative_velocity_small";
        s=snapshot(i+1,clock.now_ns());s.targets={t};owner.accept(s);owner.poll();owner.take_accepted_plans();
        ASSERT_EQ(touch.contacts().size(),1);EXPECT_EQ(touch.contacts().begin()->first,finger);
        EXPECT_LE(std::abs(touch.contacts().begin()->second[0]-t.hit.x),36);
        EXPECT_LE(std::abs(touch.contacts().begin()->second[1]-t.hit.y),2);
    }
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    clock.set(340'000'000);owner.accept(snapshot(30,clock.now_ns()));EXPECT_TRUE(touch.contacts().empty());
}
TEST(GameOwner, CurrentOuterBodyReassociatesHoldButCannotReviveCompletedFinger) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{2,0,30'000'000});
    auto h=target(1,0,10'000'000);h.note.kind=NoteKind::hold;h.note.center=h.hit;h.note.width=100;
    h.note.height=200;h.note.rails_geometry=true;h.note.head_on_line=true;h.samples=4;h.line_id=3;
    auto s=snapshot(1,0);s.targets={h};owner.accept(s);clock.set(10'000'000);owner.poll();
    clock.set(30'000'000);h.note_id=2;h.note.center.x+=10;h.hit.x+=10;h.evidence_ns=clock.now_ns();
    h.expires_ns=clock.now_ns()+100'000'000;h.crossing_ns.reset();h.reason="insufficient_history";h.samples=1;
    s=snapshot(2,clock.now_ns());s.targets={h};owner.accept(s);owner.poll();ASSERT_EQ(touch.contacts().size(),1);
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    const auto assoc=owner.take_coverage_updates();ASSERT_EQ(assoc.size(),1);EXPECT_EQ(assoc[0].at("event"),"game_hold_contact_reassociated");
    clock.set(90'000'000);owner.accept(snapshot(3,clock.now_ns()));EXPECT_TRUE(touch.contacts().empty());
    clock.set(95'000'000);h.evidence_ns=clock.now_ns();h.expires_ns=clock.now_ns()+100'000'000;
    s=snapshot(4,clock.now_ns());s.targets={h};owner.accept(s);owner.poll();EXPECT_TRUE(touch.contacts().empty());
    const auto cancelled=owner.take_plan_cancellations();ASSERT_EQ(cancelled.size(),1);EXPECT_EQ(cancelled[0].at("reason"),"current_object_missing_or_region_lost");
}
TEST(GameMotion, CurrentGrayOutlineTracksTranslationButRejectsSingleRailAndMissingClosure) {
    NoteCandidate anchor;anchor.kind=NoteKind::hold;anchor.center={500,560};anchor.tangent={1,0};
    anchor.width=120;anchor.height=300;anchor.rails_geometry=true;anchor.head_on_line=true;
    LineCandidate line{{640,570},{1,0},1200,2,.9};
    auto f=image(1,0);rect(f,454,200,4,360,{160,160,160});rect(f,574,200,4,360,{160,160,160});
    auto body=observe_held_outline(f,anchor,line);ASSERT_TRUE(body);EXPECT_NEAR(body->center.x,516,3);
    EXPECT_NEAR(body->center.y,570,1);EXPECT_FALSE(body->tail); // no visible transverse closing edge
    auto core_anchor=anchor;core_anchor.width=106; // 13% narrower birth core than outer rails
    body=observe_held_outline(f,core_anchor,line);ASSERT_TRUE(body);EXPECT_NEAR(body->width,120,4);
    rect(f,454,200,124,4,{160,160,160});body=observe_held_outline(f,anchor,line);ASSERT_TRUE(body);ASSERT_TRUE(body->tail);
    rect(f,574,200,4,360,{0,0,0});EXPECT_FALSE(observe_held_outline(f,anchor,line));
}
TEST(GameMotion, IndependentLinesKeepIdentityAcrossReorderingMotionAndTangentSign) {
    GameLineTracker tracker;auto s=snapshot(1,20'000'000);
    std::vector<LineCandidate> ls={{{640,500},{1,0},1000,2,.9},{{640,300},{1,0},1000,2,.9}};
    tracker.update(ls,s.context);const auto first=ls[0].track_id,second=ls[1].track_id;
    s.context.capture_ns+=20'000'000;s.context.frame++;
    ls={{{640,305},{-1,0},1000,2,.9},{{640,506},{1,0},1000,2,.9}};tracker.update(ls,s.context);
    EXPECT_EQ(ls[0].track_id,second);EXPECT_EQ(ls[1].track_id,first);EXPECT_GT(ls[0].tangent.x,0);
    EXPECT_FALSE(ls[1].motion_valid);EXPECT_EQ(ls[1].velocity.y,0);
    s.context.capture_ns+=20'000'000;s.context.frame++;
    ls={{{640,310},{1,0},1000,2,.9},{{640,512},{1,0},1000,2,.9}};tracker.update(ls,s.context);
    EXPECT_TRUE(ls[1].motion_valid);EXPECT_NEAR(ls[1].velocity.y,300,0.01);
    s.context.capture_ns+=20'000'000;s.context.epoch++;tracker.update(ls,s.context);EXPECT_NE(ls[1].track_id,first);
}
TEST(GameMotion, BurstFramesCannotAmplifyLineNoiseOrReplaceCurrentGeometry) {
    GameLineTracker tracker;auto s=snapshot(1,1'000'000);std::uint64_t id=0;
    for(const Nanoseconds t:{1'000'000LL,21'000'000LL,41'000'000LL,42'000'000LL,43'000'000LL,44'000'000LL}) {
        s.context.capture_ns=t;s.context.frame++;
        const double y=400+(t-1'000'000)/1e9*300+(t==42'000'000?.3:0);
        std::vector<LineCandidate> lines={{{640,y},{1,0},1000,2,.9}};tracker.update(lines,s.context);
        if(!id)id=lines[0].track_id;EXPECT_EQ(lines[0].track_id,id);
        EXPECT_EQ(lines[0].center.y,y);EXPECT_EQ(lines[0].observed_ns,t);
        if(t>=41'000'000) {ASSERT_TRUE(lines[0].motion_valid);EXPECT_NEAR(lines[0].velocity.y,300,10);EXPECT_EQ(lines[0].motion_samples,3);}
    }
}
TEST(GameMotion, RotatingLineEquationsIgnoreAlongLineCropCenterDrift) {
    GameLineTracker tracker;auto s=snapshot(1,1'000'000);std::uint64_t id=0;
    for(int i=0;i<5;++i) {
        const double angle=.4+i*.04;const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
        const double along=i%2?180:-120;
        const Vec2 center{640+u.x*along,360+i*4+u.y*along};
        s.context.capture_ns=1'000'000+i*20'000'000;s.context.frame++;
        std::vector<LineCandidate> lines={{center,i%2?Vec2{-u.x,-u.y}:u,1000,2,.9}};
        tracker.update(lines,s.context);const auto& l=lines[0];if(!id)id=l.track_id;
        EXPECT_EQ(l.track_id,id);EXPECT_NEAR(l.center.x,center.x,1e-8);EXPECT_NEAR(l.tangent.x,u.x,1e-8);
        if(i>=2) {
            ASSERT_TRUE(l.motion_valid);EXPECT_NEAR(l.angular_velocity,2,1e-6);
            // Normal velocity at this visible reference point includes the
            // true rotation around the pivot, not fictitious along-line speed.
            EXPECT_NEAR(l.velocity.x*n.x+l.velocity.y*n.y,200*n.y+2*along,20);
            EXPECT_EQ(decision_json(DecisionSnapshot{.lines=lines}).at("lines")[0].at("motion_samples"),l.motion_samples);
        }
    }
}
TEST(GameMotion, FittedRotationAssociatesAfterGapWithoutInventingAMissingLine) {
    GameLineTracker tracker;auto s=snapshot(1,1'000'000);std::uint64_t id=0;
    for(int i=0;i<4;++i) {
        const double angle=i*.16;s.context.capture_ns=1'000'000+i*20'000'000;s.context.frame++;
        std::vector<LineCandidate> lines={{{640,360},{std::cos(angle),std::sin(angle)},700,2,.9}};
        tracker.update(lines,s.context);if(!id)id=lines[0].track_id;EXPECT_EQ(lines[0].track_id,id);
        if(i>=2)ASSERT_TRUE(lines[0].motion_valid);
    }
    s.context.capture_ns=81'000'000;s.context.frame++;std::vector<LineCandidate> absent;
    tracker.update(absent,s.context);EXPECT_TRUE(absent.empty());
    s.context.capture_ns=141'000'000;s.context.frame++;const double angle=1.12;
    std::vector<LineCandidate> lines={{{640,360},{std::cos(angle),std::sin(angle)},700,2,.9}};
    tracker.update(lines,s.context);EXPECT_EQ(lines[0].track_id,id);EXPECT_EQ(lines[0].observed_ns,141'000'000);
    EXPECT_FALSE(lines[0].motion_valid); // Too few retained poses after the gap.
    s.context.capture_ns+=100'000'000;s.context.frame++;tracker.update(lines,s.context);
    EXPECT_NE(lines[0].track_id,id);EXPECT_FALSE(lines[0].motion_valid);
}
TEST(GameMotion, NonlinearLinePoseRejectsVelocityAndDuplicateLinesRemainAmbiguous) {
    GameLineTracker tracker;auto s=snapshot(1,1'000'000);std::uint64_t id=0;
    for(int i=0;i<4;++i) {
        s.context.capture_ns=1'000'000+i*20'000'000;s.context.frame++;
        std::vector<LineCandidate> lines={{{640,400.0+(i==3?22:i*4)},{1,0},1000,2,.9}};
        tracker.update(lines,s.context);if(!id)id=lines[0].track_id;
        EXPECT_EQ(lines[0].track_id,id);if(i==2)ASSERT_TRUE(lines[0].motion_valid);
        if(i==3){EXPECT_FALSE(lines[0].motion_valid);EXPECT_GT(lines[0].motion_residual,2);EXPECT_EQ(lines[0].velocity.y,0);}
    }
    s.context.capture_ns+=20'000'000;s.context.frame++;
    std::vector<LineCandidate> twins={{{640,424},{1,0},1000,2,.9},{{640,425},{1,0},1000,2,.9}};
    tracker.update(twins,s.context);EXPECT_FALSE(twins[0].association_valid);EXPECT_FALSE(twins[1].association_valid);
}
TEST(GameMotion, DistributedTiltedRidgeRejectsWideFillShortBarsAndMissingSupport) {
    auto f=image(1,0);const Vec2 u{.6,.8};const LineCandidate line{{640,360},u,750,3,.6};
    oriented_box(f,line.center,u,750,3,{245,245,245});EXPECT_TRUE(current_line_ridge_support(f,line));
    auto short_line=line;short_line.length=150;EXPECT_FALSE(current_line_ridge_support(f,short_line));
    auto broad=image(2,20'000'000);oriented_box(broad,line.center,u,750,20,{245,245,245});
    EXPECT_FALSE(current_line_ridge_support(broad,line));
    auto missing=image(3,40'000'000);oriented_box(missing,line.center,u,250,3,{245,245,245});
    EXPECT_FALSE(current_line_ridge_support(missing,line));
}
TEST(GameMotion, TiltedLineExtentUsesMeasuredFragmentsAndRejectsLargeUnsupportedGaps) {
    const double angle=.8;const Vec2 u{std::cos(angle),std::sin(angle)},c{640,360};
    const Vec2 seed_center{c.x+u.x*150,c.y+u.y*150};
    const LineCandidate seed{seed_center,u,400,3,.6};auto f=image(1,0);
    oriented_box(f,c,u,750,3,{245,245,245});
    oriented_box(f,{c.x-u.x*120,c.y-u.y*120},u,128,14,{240,220,40});
    const auto observed=observe_current_line_extent(f,seed);ASSERT_TRUE(observed);
    EXPECT_NEAR(observed->length,750,12);EXPECT_NEAR(observed->center.x,c.x,5);EXPECT_NEAR(observed->center.y,c.y,5);
    auto missing=image(2,20'000'000);EXPECT_FALSE(observe_current_line_extent(missing,seed));
    auto short_fragment=image(3,40'000'000);oriented_box(short_fragment,seed_center,u,400,3,{245,245,245});
    EXPECT_FALSE(observe_current_line_extent(short_fragment,seed));
    auto wide_gap=image(4,60'000'000);
    for(const int side:{-1,1})oriented_box(wide_gap,{c.x+u.x*side*350,c.y+u.y*side*350},u,300,3,{245,245,245});
    EXPECT_FALSE(observe_current_line_extent(wide_gap,seed));
}
TEST(GameOwner, PreviouslySeenUnsubmittedHoldDescriptionCanRetainStartedContact) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{2,0,30'000'000});
    auto held=target(1,0,10'000'000);held.note.kind=NoteKind::hold;held.note.center=held.hit;
    held.note.width=152;held.note.height=310;held.note.rails_geometry=true;held.note.head_on_line=true;
    held.samples=4;held.line_id=1;auto s=snapshot(1,0);s.targets={held};owner.accept(s);
    clock.set(10'000'000);owner.poll();ASSERT_EQ(touch.contacts().size(),1);
    auto fragment=held;fragment.note_id=42;fragment.note.center={held.note.center.x,held.note.center.y-90};
    fragment.note.head_on_line=false;fragment.crossing_ns.reset();fragment.reason="outside_short_horizon";
    clock.set(20'000'000);held.evidence_ns=fragment.evidence_ns=clock.now_ns();
    held.expires_ns=fragment.expires_ns=clock.now_ns()+100'000'000;s=snapshot(2,clock.now_ns());s.targets={held,fragment};owner.accept(s);
    clock.set(40'000'000);fragment.note.center=held.note.center;fragment.note.head_on_line=true;
    fragment.note.height=298;fragment.evidence_ns=clock.now_ns();fragment.expires_ns=clock.now_ns()+100'000'000;
    fragment.reason="root_past";s=snapshot(3,clock.now_ns());s.targets={fragment};owner.accept(s);owner.poll();
    const auto updates=owner.take_coverage_updates();ASSERT_EQ(updates.size(),1);
    EXPECT_EQ(updates.front().at("candidate_note_id"),42);EXPECT_EQ(updates.front().at("note_id"),1);
    clock.set(80'000'000);fragment.evidence_ns=clock.now_ns();fragment.expires_ns=clock.now_ns()+100'000'000;
    s=snapshot(4,clock.now_ns());s.targets={fragment};owner.accept(s);owner.poll();EXPECT_EQ(touch.contacts().size(),1);
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}
TEST(GameOwner, AliasesNeedCurrentSupportAndCannotOverrideOriginalOrReviveExpiredOwner) {
    for(const bool alias_first:{false,true}) {
        FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{2,0,30'000'000});
        auto original=target(1,0,10'000'000);original.note.kind=NoteKind::hold;original.note.center=original.hit;
        original.note.width=100;original.note.height=200;original.note.rails_geometry=true;
        original.note.head_on_line=true;original.samples=4;original.line_id=3;
        auto s=snapshot(1,0);s.targets={original};owner.accept(s);clock.set(10'000'000);owner.poll();
        auto alias=original;alias.note_id=2;alias.crossing_ns.reset();alias.reason="root_past";
        clock.set(30'000'000);alias.evidence_ns=clock.now_ns();alias.expires_ns=clock.now_ns()+100'000'000;
        s=snapshot(2,clock.now_ns());s.targets={alias};owner.accept(s);owner.poll();
        ASSERT_EQ(owner.take_coverage_updates().size(),1);
        auto invalid=alias;invalid.samples=0;invalid.reason="association_ambiguous";
        invalid.note.head_on_line=false;invalid.note.center.y-=90;
        clock.set(50'000'000);invalid.evidence_ns=original.evidence_ns=clock.now_ns();
        invalid.expires_ns=original.expires_ns=clock.now_ns()+100'000'000;
        s=snapshot(3,clock.now_ns());s.targets=alias_first?std::vector<GameTarget>{invalid,original}:
            std::vector<GameTarget>{original,invalid};owner.accept(s);owner.poll();
        ASSERT_EQ(touch.contacts().size(),1);EXPECT_TRUE(owner.take_plan_cancellations().empty());
        // Conversely, a valid alias supplies current support when the raw
        // original descriptor is invalid. Snapshot ordering cannot cancel it.
        clock.set(60'000'000);original.samples=0;original.reason="line_unobservable";
        alias.evidence_ns=original.evidence_ns=clock.now_ns();alias.expires_ns=original.expires_ns=clock.now_ns()+100'000'000;
        s=snapshot(4,clock.now_ns());s.targets=alias_first?std::vector<GameTarget>{alias,original}:
            std::vector<GameTarget>{original,alias};owner.accept(s);owner.poll();ASSERT_EQ(touch.contacts().size(),1);
        EXPECT_TRUE(owner.take_plan_cancellations().empty());
        clock.set(75'000'000);invalid.evidence_ns=clock.now_ns();invalid.expires_ns=clock.now_ns()+100'000'000;
        s=snapshot(5,clock.now_ns());s.targets={invalid};owner.accept(s);owner.poll();EXPECT_EQ(touch.contacts().size(),1);
        clock.set(130'000'000);invalid.evidence_ns=clock.now_ns();invalid.expires_ns=clock.now_ns()+100'000'000;
        s=snapshot(6,clock.now_ns());s.targets={invalid};owner.accept(s);owner.poll();EXPECT_TRUE(touch.contacts().empty());
        for(int i=0;i<2;++i) {
            clock.set(500'000'000+i*20'000'000);alias.evidence_ns=clock.now_ns();alias.expires_ns=clock.now_ns()+100'000'000;
            alias.crossing_ns=clock.now_ns()+10'000'000;alias.reason="prediction_observe_only";
            s=snapshot(7+i,clock.now_ns());s.targets={alias};owner.accept(s);owner.poll();EXPECT_TRUE(touch.contacts().empty());
        }
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
        owner.stop();
    }
}
TEST(GameObserver, OneCurrentRailPairCannotMultiplyHeldIdentitiesThroughCoreFragments) {
    FakeClock clock;GameObserver observer(clock);std::uint64_t identity=0;
    for(int i=0;i<4;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int head=510+i*22;
        rect(f,432,head-200,136,200,{40,190,255});
        rect(f,423,head-200,3,200,{245,245,245});rect(f,575,head-200,3,200,{245,245,245});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        ASSERT_EQ(s.targets.size(),1);identity=s.targets.front().note_id;
    }
    for(int i=0;i<40;++i) {
        auto f=image(i+5,(i+4)*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,426,150,149,425,{90,100,110});
        rect(f,423,150,3,425,{170,170,170});rect(f,575,150,3,425,{170,170,170});
        // A short blue core is a changing description inside the same pair.
        const int core_top=460+(i%3)*20;
        rect(f,432,core_top,136,570-core_top,{40,190,255});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        ASSERT_EQ(s.targets.size(),1)<<i;EXPECT_EQ(s.targets.front().note_id,identity)<<i;
        EXPECT_TRUE(s.targets.front().note.rails_geometry);EXPECT_TRUE(s.targets.front().note.head_on_line);
    }
}
TEST(GameMotion, ApproachingAnchorNeedsCurrentRailsAttachedToTheLine) {
    NoteCandidate anchor;anchor.kind=NoteKind::hold;anchor.center={500,548};anchor.tangent={1,0};
    anchor.width=136;anchor.height=300;anchor.rails_geometry=true;anchor.head_on_line=false;
    LineCandidate line{{640,576},{1,0},1280,2,.9};
    auto f=image(1,0);
    rect(f,423,200,3,374,{170,170,170});rect(f,575,200,3,374,{170,170,170});
    const auto supported=observe_held_outline(f,anchor,line);ASSERT_TRUE(supported);
    EXPECT_TRUE(supported->head_on_line);EXPECT_NEAR(supported->center.y,576,1);
    EXPECT_NEAR(supported->width,152,4);EXPECT_FALSE(supported->tail);
    // A real approaching body with its leading edge still 22px above the
    // line must not inherit the projected head or keep a contact alive.
    rect(f,423,554,3,20,{0,0,0});rect(f,575,554,3,20,{0,0,0});
    EXPECT_FALSE(observe_held_outline(f,anchor,line));
    rect(f,423,554,3,20,{170,170,170});rect(f,575,554,3,20,{170,170,170});
    anchor.center.y=527;EXPECT_FALSE(observe_held_outline(f,anchor,line));
}
TEST(GameObserver, CurrentAttachedGrayRailsRetainFrontClippedBeforeOnlineClassification) {
    FakeClock clock;GameObserver observer(clock);FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,2,{2,35'000'000,30'000'000});std::uint64_t identity=0;
    for(int i=0;i<4;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int head=506+i*16;
        rect(f,432,head-240,136,240,{40,190,255});
        rect(f,423,head-240,3,240,{245,245,245});rect(f,575,head-240,3,240,{245,245,245});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        ASSERT_EQ(s.targets.size(),1);identity=s.targets.front().note_id;
        EXPECT_FALSE(s.targets.front().note.head_on_line);
        owner.accept(s);owner.poll();
    }
    for(int i=0;i<8;++i) {
        auto f=image(i+5,(i+4)*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        rect(f,426,200,149,375,{90,100,110});
        rect(f,423,200,3,375,{170,170,170});rect(f,575,200,3,375,{170,170,170});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        ASSERT_EQ(s.targets.size(),1)<<i;EXPECT_EQ(s.targets.front().note_id,identity);
        EXPECT_TRUE(s.targets.front().note.head_on_line);EXPECT_GT(s.targets.front().samples,0);
        EXPECT_EQ(s.targets.front().evidence_ns,f.capture_complete_ns);
        owner.accept(s);owner.poll();EXPECT_EQ(touch.contacts().size(),1)<<i;
    }
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){
        return r.command.phase==Phase::down;}),1);
    auto missing=image(13,280'000'000);hud(missing);rect(missing,0,575,1280,2,{255,255,255});
    clock.set(missing.capture_complete_ns);owner.accept(observer.process(missing));owner.poll();
    EXPECT_TRUE(touch.contacts().empty());owner.stop();
}
TEST(GameMotion, ShortTerminalOutlineKeepsCurrentTailAndRejectsInternalClosingFlash) {
    NoteCandidate anchor;anchor.kind=NoteKind::hold;anchor.center={500,560};anchor.tangent={1,0};
    anchor.width=120;anchor.height=200;anchor.rails_geometry=true;anchor.head_on_line=true;
    LineCandidate line{{640,560},{1,0},1200,2,.9};
    for(const int tail_y:{540,550,558,562}) {
        auto f=image(1,0);rect(f,440,tail_y,3,24,{160,160,160});rect(f,560,tail_y,3,24,{160,160,160});
        rect(f,440,tail_y,123,3,{160,160,160});const auto current=observe_held_outline(f,anchor,line);
        ASSERT_TRUE(current)<<tail_y;ASSERT_TRUE(current->tail)<<tail_y;
        EXPECT_NEAR(current->tail->y,tail_y,3);EXPECT_TRUE(current->head_on_line);
    }
    auto f=image(1,0);rect(f,440,300,3,290,{160,160,160});rect(f,560,300,3,290,{160,160,160});
    rect(f,440,562,123,3,{160,160,160});const auto current=observe_held_outline(f,anchor,line);
    ASSERT_TRUE(current);EXPECT_FALSE(current->tail); // the flash cannot cut the continuing rails
}
TEST(GameMotion, MovingHeldFrontNeedsPairedCurrentTerminationAndCannotInferFromHistory) {
    NoteCandidate anchor;anchor.kind=NoteKind::hold;anchor.center={500,576};anchor.tangent={1,0};
    anchor.width=152;anchor.height=300;anchor.rails_geometry=true;anchor.head_on_line=true;
    LineCandidate line{{640,576},{1,0},1280,2,.9};
    auto f=image(1,0);rect(f,426,180,149,368,{90,100,110});
    rect(f,423,180,3,368,{170,170,170});rect(f,575,180,3,368,{170,170,170});
    auto observed=observe_moving_held_front(f,anchor,line);ASSERT_TRUE(observed);
    EXPECT_TRUE(observed->held_body_evidence);EXPECT_FALSE(observed->head_on_line);
    EXPECT_NEAR(observed->center.y,548,5);EXPECT_FALSE(observed->tail);
    auto unestablished=anchor;unestablished.head_on_line=false;
    EXPECT_FALSE(observe_moving_held_front(f,unestablished,line));
    rect(f,575,180,3,368,{0,0,0});EXPECT_FALSE(observe_moving_held_front(f,anchor,line));
    rect(f,575,180,3,368,{170,170,170});rect(f,426,180,149,368,{0,0,0});
    EXPECT_FALSE(observe_moving_held_front(f,anchor,line));
    // A dark effect stripe inside a still-continuing body is not its front.
    rect(f,426,180,149,520,{90,100,110});rect(f,426,545,149,10,{0,0,0});
    rect(f,423,180,3,520,{170,170,170});rect(f,575,180,3,520,{170,170,170});
    EXPECT_FALSE(observe_moving_held_front(f,anchor,line));
    auto blank=image(2,20'000'000);EXPECT_FALSE(observe_moving_held_front(blank,*observed,line));
}
TEST(GameObserver, GrayHeldFrontCanMoveAwayFromTheLineWithTheSameFingerAndCurrentHit) {
    FakeClock clock;GameObserver observer(clock);FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,2,{2,35'000'000,30'000'000});std::uint64_t identity=0;int finger=-1;
    for(int i=0;i<4;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int head=510+i*22;
        rect(f,432,head-260,136,260,{40,190,255});
        rect(f,423,head-260,3,260,{245,245,245});rect(f,575,head-260,3,260,{245,245,245});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        ASSERT_EQ(s.targets.size(),1);identity=s.targets.front().note_id;owner.accept(s);owner.poll();
    }
    ASSERT_EQ(touch.contacts().size(),1);finger=touch.contacts().begin()->first;
    for(int i=0;i<12;++i) {
        auto f=image(i+5,(i+4)*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});
        const int x=500+i*6,head=548-i*8;
        rect(f,x-74,128,149,head-128,{90,100,110});
        // Alternating effect rails extend below the true front toward the line.
        const int rail_end=i%2==0?590:head;
        rect(f,x-77,128,3,rail_end-128,{170,170,170});rect(f,x+75,128,3,rail_end-128,{170,170,170});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        ASSERT_EQ(s.targets.size(),1)<<i;const auto& t=s.targets.front();EXPECT_EQ(t.note_id,identity)<<i;
        EXPECT_TRUE(t.note.held_body_evidence);EXPECT_FALSE(t.note.head_on_line);EXPECT_FALSE(t.note.tail);
        EXPECT_NEAR(t.hit.x,x,5);EXPECT_NEAR(t.hit.y,head,5);
        EXPECT_EQ(t.evidence_ns,f.capture_complete_ns);owner.accept(s);owner.poll();
        ASSERT_EQ(touch.contacts().size(),1)<<i;EXPECT_EQ(touch.contacts().begin()->first,finger);
        EXPECT_NEAR(touch.contacts().begin()->second[0],t.hit.x,2);EXPECT_NEAR(touch.contacts().begin()->second[1],t.hit.y,2);
    }
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    auto missing=image(17,360'000'000);hud(missing);rect(missing,0,575,1280,2,{255,255,255});
    clock.set(missing.capture_complete_ns);owner.accept(observer.process(missing));owner.poll();
    EXPECT_TRUE(touch.contacts().empty());owner.stop();
}
TEST(GameMotion, MovingFrontCannotSnapToAnEffectBoxWithoutCurrentBodyInterior) {
    NoteCandidate anchor;anchor.kind=NoteKind::hold;anchor.center={500,535};anchor.tangent={1,0};
    anchor.width=152;anchor.height=350;anchor.rails_geometry=true;anchor.held_body_evidence=true;
    LineCandidate line{{640,576},{1,0},1280,2,.9};
    auto f=image(1,0);rect(f,426,180,149,342,{90,100,110});
    rect(f,423,180,3,410,{170,170,170});rect(f,575,180,3,410,{170,170,170});
    rect(f,423,584,155,3,{170,170,170});
    EXPECT_FALSE(observe_held_outline(f,anchor,line));
    const auto current=observe_moving_held_front(f,anchor,line);ASSERT_TRUE(current);
    EXPECT_TRUE(current->held_body_evidence);EXPECT_NEAR(current->center.y,522,5);
    // Actual body fill returning to the line permits reattachment.
    rect(f,426,180,149,396,{90,100,110});
    const auto attached=observe_held_outline(f,anchor,line);ASSERT_TRUE(attached);
    EXPECT_TRUE(attached->head_on_line);EXPECT_NEAR(attached->center.y,576,2);
}
TEST(GameMotion, OccludedFrontCanUseOnlyAVisiblePairedBodyInterior) {
    NoteCandidate anchor;anchor.kind=NoteKind::hold;anchor.center={500,498};anchor.tangent={1,0};
    anchor.width=152;anchor.height=300;anchor.rails_geometry=true;anchor.held_body_evidence=true;
    LineCandidate line{{640,576},{1,0},1280,2,.9};
    auto f=image(1,0);rect(f,426,180,149,310,{90,100,110});
    rect(f,423,180,3,310,{170,170,170});rect(f,575,180,3,310,{170,170,170});
    rect(f,415,470,172,60,{170,160,110}); // current touch effect hides the leading edge
    EXPECT_FALSE(observe_moving_held_front(f,anchor,line));
    const auto patch=observe_held_body_patch(f,anchor,line);ASSERT_TRUE(patch);
    EXPECT_TRUE(patch->held_body_patch);EXPECT_TRUE(patch->held_body_evidence);
    EXPECT_FALSE(patch->head_on_line);EXPECT_FALSE(patch->tail);
    EXPECT_LE(patch->center.y,470);EXPECT_GE(patch->center.y,450);EXPECT_NEAR(patch->center.x,500,4);
    auto unheld=anchor;unheld.held_body_evidence=false;EXPECT_FALSE(observe_held_body_patch(f,unheld,line));
    rect(f,575,180,3,290,{0,0,0});EXPECT_FALSE(observe_held_body_patch(f,anchor,line));
    auto empty=image(2,20'000'000);EXPECT_FALSE(observe_held_body_patch(empty,anchor,line));
    rect(empty,423,180,3,310,{170,170,170});rect(empty,575,180,3,310,{170,170,170});
    EXPECT_FALSE(observe_held_body_patch(empty,anchor,line)); // rails without body fill
}
TEST(GameOwner, MovingHoldRejectsColorFragmentProjectionAndKeepsTheOriginalMissingGrace) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,2,{2,0,30'000'000});
    auto h=target(1,0,10'000'000);h.note.kind=NoteKind::hold;h.samples=4;
    h.note.center=h.hit;h.note.width=152;h.note.height=300;h.note.rails_geometry=true;h.note.outline_evidence=true;
    auto s=snapshot(1,0);s.targets={h};owner.accept(s);clock.set(10'000'000);owner.poll();
    ASSERT_EQ(touch.contacts().size(),1);const int finger=touch.contacts().begin()->first;
    const auto update=[&](Nanoseconds t,bool supported,bool patch){
        clock.set(t);s=snapshot(s.sequence+1,t);h.evidence_ns=t;h.expires_ns=t+100'000'000;h.revision++;
        h.note.held_body_evidence=supported;h.note.held_body_patch=patch;h.note.rails_geometry=supported;
        h.note.center={400,supported?(patch?460.:480.):465.};h.hit={400,supported?h.note.center.y:500.};
        h.crossing_ns.reset();h.reason=patch?"held_body_touch_only":"root_past";
        s.targets={h};owner.accept(s);owner.poll();
    };
    update(20'000'000,true,false);ASSERT_EQ(touch.contacts().size(),1);
    EXPECT_EQ(touch.contacts().begin()->second[1],480);owner.take_accepted_plans();
    update(40'000'000,false,false);EXPECT_EQ(touch.contacts().begin()->second[1],480);
    EXPECT_TRUE(owner.take_accepted_plans().empty());
    update(60'000'000,true,true);EXPECT_EQ(touch.contacts().begin()->first,finger);
    EXPECT_EQ(touch.contacts().begin()->second[1],460);
    update(80'000'000,false,false);EXPECT_EQ(touch.contacts().begin()->second[1],460);
    update(120'000'000,false,false);EXPECT_TRUE(touch.contacts().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    owner.stop();
}
TEST(GameOwner, HeldBodySupportCannotCreateANewDownEvenWithAFittedCrossing) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,2,{2,0,30'000'000});
    auto h=target(1,0,10'000'000);h.note.kind=NoteKind::hold;h.samples=4;
    h.note.held_body_evidence=h.note.held_body_patch=h.note.rails_geometry=h.note.outline_evidence=true;
    auto s=snapshot(1,0);s.targets={h};owner.accept(s);clock.set(10'000'000);owner.poll();
    EXPECT_TRUE(touch.contacts().empty());EXPECT_TRUE(touch.receipts().empty());owner.stop();
}
TEST(GameOwner, PendingHoldDeadlineRevisionsRenewFromCurrentCaptureBeforeTheNextFrameGap) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,2,{2,35'000'000,30'000'000});
    auto h=target(1,0,90'000'000);h.note.kind=NoteKind::hold;h.samples=4;
    h.note.center=h.hit;h.note.width=152;h.note.height=400;h.note.rails_geometry=true;
    auto s=snapshot(1,0);s.targets={h};owner.accept(s);
    auto plans=owner.take_accepted_plans();ASSERT_EQ(plans.size(),1);EXPECT_EQ(plans.back().steps.back().due_ns,100'000'000);
    for(const auto [capture,crossing]:std::array<std::pair<Nanoseconds,Nanoseconds>,2>{{{10'000'000,80'000'000},{20'000'000,60'000'000}}}) {
        clock.set(capture);s=snapshot(s.sequence+1,capture);h.evidence_ns=capture;h.expires_ns=capture+100'000'000;
        h.crossing_ns=crossing;h.revision++;s.targets={h};owner.accept(s);
        plans=owner.take_accepted_plans();ASSERT_EQ(plans.size(),1);
        EXPECT_EQ(plans.back().steps.back().due_ns,capture+100'000'000);
        EXPECT_EQ(*plans.back().predicted_down_ns,crossing-35'000'000);
    }
    clock.set(25'000'000);owner.poll();ASSERT_EQ(touch.contacts().size(),1);
    // The next frame is delivered after the old, incorrectly shifted Up=70ms.
    clock.set(80'000'000);owner.poll();ASSERT_EQ(touch.contacts().size(),1);
    s=snapshot(4,74'000'000);h.evidence_ns=74'000'000;h.expires_ns=174'000'000;h.crossing_ns.reset();
    h.reason="relative_velocity_small";h.note.head_on_line=true;s.targets={h};owner.accept(s);owner.poll();
    ASSERT_EQ(touch.contacts().size(),1);plans=owner.take_accepted_plans();ASSERT_EQ(plans.size(),1);
    EXPECT_EQ(plans.back().steps.back().due_ns,174'000'000); // capture clock, not recognition/now+100
    clock.set(173'000'000);owner.poll();EXPECT_EQ(touch.contacts().size(),1);
    clock.set(174'000'000);owner.poll();EXPECT_TRUE(touch.contacts().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    owner.stop();
}
TEST(GameObserver, HeldBodyKeepsItsFingerThroughAFrontOcclusionAndRecoversCurrentFront) {
    FakeClock clock;GameObserver observer(clock);FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,2,{2,35'000'000,30'000'000});std::uint64_t identity=0;int finger=-1,patches=0;
    for(int i=0;i<4;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});const int head=510+i*22;
        rect(f,432,head-260,136,260,{40,190,255});
        rect(f,423,head-260,3,260,{245,245,245});rect(f,575,head-260,3,260,{245,245,245});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);ASSERT_EQ(s.targets.size(),1);
        identity=s.targets.front().note_id;owner.accept(s);owner.poll();
    }
    ASSERT_EQ(touch.contacts().size(),1);finger=touch.contacts().begin()->first;
    for(int i=0;i<8;++i) {
        auto f=image(i+5,(i+4)*20'000'000);hud(f);rect(f,0,575,1280,2,{255,255,255});const int head=548-i*8;
        rect(f,426,128,149,head-128,{90,100,110});
        rect(f,423,128,3,head-128,{170,170,170});rect(f,575,128,3,head-128,{170,170,170});
        if(i==2||i==3)rect(f,415,head-20,172,60,{170,160,110});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        const auto held=std::find_if(s.targets.begin(),s.targets.end(),[&](const auto& t){return t.note_id==identity;});
        ASSERT_NE(held,s.targets.end())<<i;EXPECT_TRUE(held->note.held_body_evidence)<<i;
        EXPECT_FALSE(held->note.head_on_line);EXPECT_FALSE(held->note.tail);
        EXPECT_LE(held->hit.y,head);EXPECT_GE(held->hit.y,head-48);EXPECT_NEAR(held->hit.x,500,5);
        if(held->note.held_body_patch) {++patches;EXPECT_FALSE(held->crossing_ns);EXPECT_EQ(held->reason,"held_body_touch_only");}
        if(i>=4)EXPECT_FALSE(held->note.held_body_patch);
        owner.accept(s);owner.poll();ASSERT_EQ(touch.contacts().size(),1)<<i;
        EXPECT_EQ(touch.contacts().begin()->first,finger);EXPECT_NEAR(touch.contacts().begin()->second[1],held->hit.y,2);
    }
    EXPECT_GE(patches,2);EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    auto missing=image(13,280'000'000);hud(missing);rect(missing,0,575,1280,2,{255,255,255});
    clock.set(missing.capture_complete_ns);owner.accept(observer.process(missing));owner.poll();EXPECT_TRUE(touch.contacts().empty());owner.stop();
}
TEST(GameObserver, RotatingCurrentLineRetainsIdentityAndFreshLocalHitGeometry) {
    FakeClock clock;GameObserver observer(clock);std::uint64_t line_id=0,note_id=0;
    for(int i=0;i<8;++i) {
        auto f=image(i+1,i*20'000'000);hud(f);const double angle=.80+i*.035;
        const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x},c{640,380.0+i*3};
        oriented_box(f,c,u,750,3,{255,255,255});
        const Vec2 p{c.x-u.x*120-n.x*(90-i*8),c.y-u.y*120-n.y*(90-i*8)};
        oriented_box(f,p,u,70,6,{40,190,255});clock.set(f.capture_complete_ns);
        const auto s=observer.process(f);ASSERT_EQ(s.lines.size(),1);ASSERT_EQ(s.targets.size(),1);
        if(!line_id){line_id=s.lines.front().track_id;note_id=s.targets.front().note_id;}
        EXPECT_EQ(s.lines.front().track_id,line_id);EXPECT_EQ(s.targets.front().note_id,note_id);
        EXPECT_EQ(s.targets.front().line_id,line_id);EXPECT_NEAR(s.targets.front().hit.x,c.x-u.x*120,4);
        EXPECT_NEAR(s.targets.front().hit.y,c.y-u.y*120,4);
    }
}
TEST(GameOwner, FreshYellowRegionsBridgeSuccessiveIdsWithoutRepeatingDown) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{4,0,30'000'000});
    auto t=target(1,0,30'000'000);t.note.kind=NoteKind::drag;t.note.width=100;t.samples=4;
    auto s=snapshot(1,0);s.targets={t};owner.accept(s);owner.take_accepted_plans();
    clock.set(15'000'000);owner.poll();
    for(int i=1;i<=8;++i) {
        clock.set(i*60'000'000);t=target(i+1,clock.now_ns(),clock.now_ns()+20'000'000);
        t.note.kind=NoteKind::drag;t.note.width=100;t.samples=4;t.hit.x=400+(i%2?20:-20);
        s=snapshot(i+1,clock.now_ns());s.targets={t};owner.accept(s);owner.poll();owner.take_accepted_plans();
        ASSERT_EQ(touch.contacts().size(),1);EXPECT_TRUE(owner.scheduler().fault().empty());
    }
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    EXPECT_EQ(owner.take_coverage_updates().size(),8);
    clock.set(520'000'000);owner.accept(snapshot(20,clock.now_ns()));
    EXPECT_TRUE(touch.contacts().empty());owner.stop();
}
TEST(GameOwner, YellowRegionSharingRejectsAcrossLineAndAmbiguousActiveCoverage) {
    for(const bool ambiguous:{false,true}) {
        FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,3,{4,0,30'000'000});
        auto a=target(1,0,30'000'000);a.note.kind=NoteKind::drag;a.note.width=100;a.samples=4;
        auto b=a;b.note_id=2;b.hit.x=440;auto s=snapshot(1,0);s.targets={a};
        if(ambiguous)s.targets.push_back(b);
        owner.accept(s);owner.take_accepted_plans();clock.set(15'000'000);owner.poll();
        clock.set(20'000'000);auto c=target(3,clock.now_ns(),50'000'000);
        c.note.kind=NoteKind::drag;c.note.width=100;c.samples=4;c.hit.x=420;
        if(!ambiguous)c.hit.y+=3;
        a.evidence_ns=clock.now_ns();a.expires_ns=clock.now_ns()+100'000'000;a.revision++;
        b.evidence_ns=clock.now_ns();b.expires_ns=clock.now_ns()+100'000'000;b.revision++;
        s=snapshot(2,clock.now_ns());s.targets={a,c};if(ambiguous)s.targets.push_back(b);
        owner.accept(s);owner.take_accepted_plans();clock.set(35'000'000);owner.poll();
        EXPECT_EQ(touch.contacts().size(),ambiguous?3:2);EXPECT_TRUE(owner.take_coverage_updates().empty());owner.stop();
    }
}
TEST(GameOwner, LongHoldBridgesFreshSparseFramesButStopsAtActualEvidenceExpiry) {
    for(const bool refresh_before_gap:{false,true}) {
        FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,2,{2,0,30'000'000});
        auto h=target(1,0,10'000'000);h.note.kind=NoteKind::hold;h.samples=3;
        auto s=snapshot(1,0);s.targets={h};owner.accept(s);owner.take_accepted_plans();
        clock.set(10'000'000);ASSERT_EQ(owner.poll().size(),1);
        const Nanoseconds last_evidence=refresh_before_gap?30'000'000:0;
        if(refresh_before_gap) {
            clock.set(last_evidence);s=snapshot(2,last_evidence);h.evidence_ns=last_evidence;
            h.expires_ns=last_evidence+100'000'000;h.revision++;s.targets={h};
            owner.accept(s);owner.poll();owner.take_accepted_plans();
        }
        // A still-fresh 85 ms capture gap must not execute an artificial
        // 70 ms rolling Up before the next current Hold can renew the contact.
        clock.set(last_evidence+85'000'000);owner.poll();EXPECT_EQ(touch.contacts().size(),1);
        const auto current_evidence=clock.now_ns();clock.set(current_evidence+5'000'000);
        s=snapshot(3,current_evidence);h.evidence_ns=current_evidence;h.expires_ns=current_evidence+100'000'000;
        h.revision++;h.samples=2;h.crossing_ns.reset();h.reason="insufficient_history";s.targets={h};
        owner.accept(s);owner.poll();ASSERT_EQ(touch.contacts().size(),1);
        const auto plans=owner.take_accepted_plans();ASSERT_EQ(plans.size(),1);
        EXPECT_EQ(plans[0].steps.back().due_ns,current_evidence+100'000'000);
        clock.set(current_evidence+99'000'000);owner.poll();EXPECT_EQ(touch.contacts().size(),1);
        clock.set(current_evidence+100'000'000);owner.poll();EXPECT_TRUE(touch.contacts().empty());
        EXPECT_FALSE(owner.scheduler().armed());EXPECT_TRUE(owner.scheduler().fault().empty());
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){
            return r.command.phase==Phase::down;}),1);
    }
}
TEST(GameOwner, TypeGateAndFlickUseIndependentDownMoveUpSequence) {
    FakeClock clock; FakeTouchBackend backend(clock); GamePlanOwner owner(clock,backend,2,{8,0,30'000'000});
    auto s=snapshot(1,0); auto f=target(1,0,10'000'000); f.note.kind=NoteKind::flick;
    s.targets={f,target(2,0,10'000'000)}; owner.accept(s);
    auto plans=owner.take_accepted_plans(); ASSERT_EQ(plans.size(),1); ASSERT_EQ(plans[0].steps.size(),6);
    for(const auto& step:plans[0].steps) {clock.set(step.due_ns); owner.poll();}
    EXPECT_EQ(backend.receipts().size(),6); EXPECT_TRUE(backend.contacts().empty());
}
TEST(GameObserver, InterruptedThinBandIsOneLineButSeparatedBandsRemainDistinct) {
    FakeClock clock; GameObserver observer(clock); auto f=image(1,1);
    rect(f,0,574,424,2,{255,255,255});
    rect(f,568,574,712,2,{255,255,255});
    rect(f,0,576,1280,2,{255,255,255});
    clock.set(1); const auto first=observer.process(f);
    ASSERT_EQ(first.lines.size(),1);
    // Supporting-point fit uses integer rows: half-pixel rounding is allowed.
    EXPECT_NEAR(first.lines[0].center.y,575.5,0.6);
    EXPECT_GT(first.lines[0].length,1200);
    rect(f,0,500,1280,2,{255,255,255}); f.sequence=2; f.capture_complete_ns=20'000'001;
    clock.set(f.capture_complete_ns); const auto second=observer.process(f);
    EXPECT_EQ(second.lines.size(),2);
}
TEST(GameScheduler, FuturePlansDoNotReserveContacts) {
    FakeClock clock; FakeTouchBackend touch(clock); ContactScheduler scheduler(clock,touch,1);
    ASSERT_TRUE(scheduler.set_gate(1,true,0));
    ASSERT_TRUE(scheduler.submit(tap(1,0,80'000'000)));
    ASSERT_TRUE(scheduler.submit(tap(2,0,10'000'000)));
    clock.set(10'000'000); ASSERT_EQ(scheduler.run_due().size(),1);
    clock.set(15'000'000); ASSERT_EQ(scheduler.run_due().size(),1);
    clock.set(80'000'000); ASSERT_EQ(scheduler.run_due().size(),1);
    EXPECT_TRUE(scheduler.fault().empty()); EXPECT_EQ(touch.receipts()[0].command.intent_id,2);
}
TEST(GameScheduler, ActiveRevisionPreservesMovesAndRejectsChangedExecutedPrefix) {
    FakeClock clock; FakeTouchBackend touch(clock); ContactScheduler scheduler(clock,touch);
    ASSERT_TRUE(scheduler.set_gate(1,true,0));
    auto p=tap(1,0,10'000'000);
    p.steps={{Phase::down,20,30,10'000'000},{Phase::move,30,30,20'000'000},
        {Phase::move,40,30,30'000'000},{Phase::up,40,30,40'000'000}};
    ASSERT_TRUE(scheduler.submit(p)); clock.set(20'000'000); ASSERT_EQ(scheduler.run_due().size(),2);
    p.revision=2; p.evidence_ns=20'000'000; p.steps[2].x=45;
    ASSERT_TRUE(scheduler.submit(p)); EXPECT_EQ(scheduler.next_due_ns(),30'000'000);
    auto invalid=p; invalid.revision=3; invalid.steps[1].x=99;
    EXPECT_FALSE(scheduler.submit(invalid)); clock.set(30'000'000);
    auto events=scheduler.run_due(); ASSERT_EQ(events.size(),1); EXPECT_EQ(events[0].command.x,45);
    EXPECT_EQ(events[0].command.contact_id,touch.receipts()[0].command.contact_id);
    clock.set(40'000'000); ASSERT_EQ(scheduler.run_due().size(),1); EXPECT_TRUE(touch.contacts().empty());
}
TEST(GameScheduler, TargetExpiryReleasesOnlyThatContactAndFreshGateKeepsOther) {
    FakeClock clock; FakeTouchBackend touch(clock); ContactScheduler scheduler(clock,touch);
    ASSERT_TRUE(scheduler.set_gate(1,true,0));
    auto a=tap(1,0,10'000'000); a.steps.back().due_ns=400'000'000;
    ASSERT_TRUE(scheduler.submit(a)); clock.set(10'000'000); scheduler.run_due();
    clock.set(100'000'000); ASSERT_TRUE(scheduler.set_gate(1,true,100'000'000));
    ASSERT_TRUE(scheduler.submit(tap(2,100'000'000,160'000'000)));
    clock.set(150'000'000); const auto releases=scheduler.run_due();
    ASSERT_EQ(releases.size(),1); EXPECT_EQ(releases[0].command.phase,Phase::up);
    EXPECT_TRUE(scheduler.armed()); EXPECT_TRUE(scheduler.fault().empty());
    EXPECT_EQ(scheduler.pending_count(),1);
    clock.set(160'000'000); EXPECT_EQ(scheduler.run_due().size(),1);
}
TEST(GameScheduler, ContextAndDispatchGuardRevokePendingAndActive) {
    FakeClock clock; FakeTouchBackend touch(clock); ContactScheduler scheduler(clock,touch);
    ASSERT_TRUE(scheduler.set_context(1,1,1,true,0));
    auto p=tap(1,0,10); p.generation=1; p.geometry_version=1;
    ASSERT_TRUE(scheduler.submit(p)); clock.set(10); scheduler.run_due();
    ASSERT_TRUE(scheduler.set_context(1,1,2,true,10)); EXPECT_TRUE(touch.contacts().empty());
    EXPECT_FALSE(scheduler.submit(p)); EXPECT_FALSE(scheduler.set_context(1,1,1,true,10));
    p=tap(2,10,20); p.generation=1; p.geometry_version=2;
    ASSERT_TRUE(scheduler.submit(p)); bool valid=true;
    scheduler.set_dispatch_guard([&] {return valid;}); valid=false; clock.set(20);
    EXPECT_TRUE(scheduler.run_due().empty()); EXPECT_FALSE(scheduler.armed());
}
TEST(GameOwner, OutOfOrderNotesGetSubmissionIdsAndFullSnapshotCancelsMissing) {
    FakeClock clock; FakeTouchBackend touch(clock); GamePlanOwner owner(clock,touch);
    auto s=snapshot(1,0); s.targets={target(80,0,10'000'000),target(4,0,10'000'000)};
    owner.accept(s); EXPECT_EQ(owner.scheduler().accepted_high_watermark(),2);
    clock.set(10'000'000); const auto events=owner.poll(); ASSERT_EQ(events.size(),2);
    EXPECT_NE(events[0].command.contact_id,events[1].command.contact_id);
    clock.set(50'000'000);s=snapshot(2,50'000'000); owner.accept(s); EXPECT_TRUE(touch.contacts().empty());
    s=snapshot(3,50'000'000); s.targets={target(80,50'000'000,60'000'000)};
    owner.accept(s); clock.set(60'000'000); EXPECT_TRUE(owner.poll().empty());
}
TEST(GameOwner, SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt) {
    for(const bool returns:{false,true}) {
        FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,2,{4,20'000'000,30'000'000});
        auto s=snapshot(1,0);auto t=target(9,0,80'000'000);t.note.kind=NoteKind::drag;t.samples=4;s.targets={t};
        owner.accept(s);ASSERT_EQ(owner.scheduler().pending_count(),1);
        clock.set(20'000'000);owner.accept(snapshot(2,20'000'000));EXPECT_EQ(owner.scheduler().pending_count(),1);
        if(returns) {clock.set(30'000'000);s=snapshot(3,30'000'000);t.evidence_ns=30'000'000;t.expires_ns=130'000'000;
            t.revision=2;s.targets={t};owner.accept(s);clock.set(45'000'000);ASSERT_EQ(owner.poll().size(),1);
            EXPECT_EQ(touch.contacts().size(),1);owner.stop();EXPECT_TRUE(touch.contacts().empty());}
        else {clock.set(40'000'000);owner.accept(snapshot(3,40'000'000));EXPECT_EQ(owner.scheduler().pending_count(),0);
            clock.set(50'000'000);EXPECT_TRUE(owner.poll().empty());EXPECT_TRUE(touch.contacts().empty());}
    }
}
TEST(GameOwner, MissingPendingTapAndDragRetryWithCurrentEvidenceExactlyOnce) {
    for(const auto kind:{NoteKind::tap,NoteKind::drag}) {
        SCOPED_TRACE(name(kind));FakeClock clock;FakeTouchBackend touch(clock);
        GamePlanOwner owner(clock,touch,2,{15,0,30'000'000});
        auto s=snapshot(1,0);auto t=target(9,0,60'000'000);t.note.kind=kind;t.samples=4;s.targets={t};
        owner.accept(s);const auto original=owner.take_accepted_plans();ASSERT_EQ(original.size(),1);
        clock.set(40'000'000);owner.accept(snapshot(2,clock.now_ns()));owner.poll();
        const auto canceled=owner.take_plan_cancellations();ASSERT_EQ(canceled.size(),1);
        EXPECT_EQ(canceled[0].at("executed_steps"),0);EXPECT_TRUE(canceled[0].at("retry_without_prior_down"));
        EXPECT_EQ(owner.scheduler().pending_count(),0);EXPECT_TRUE(touch.receipts().empty());
        clock.set(60'000'000);s=snapshot(3,clock.now_ns());t.evidence_ns=clock.now_ns();
        t.expires_ns=clock.now_ns()+100'000'000;t.revision++;t.crossing_ns=80'000'000;s.targets={t};
        owner.accept(s);const auto rebuilt=owner.take_accepted_plans();ASSERT_EQ(rebuilt.size(),1);
        EXPECT_NE(rebuilt[0].intent_id,original[0].intent_id);EXPECT_EQ(rebuilt[0].source_frame_sequence,3);
        owner.poll();EXPECT_TRUE(touch.contacts().empty());clock.set(80'000'000);owner.poll();
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
        ASSERT_EQ(touch.contacts().size(),1);owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}
TEST(GameOwner, InvalidPendingGeometryCanRetryOnlyFromFreshValidPrediction) {
    for(const auto kind:{NoteKind::tap,NoteKind::hold,NoteKind::drag,NoteKind::flick}) {
        SCOPED_TRACE(name(kind));FakeClock clock;FakeTouchBackend touch(clock);
        GamePlanOwner owner(clock,touch,2,{15,0,30'000'000});
        auto s=snapshot(1,0);auto t=target(1,0,60'000'000);t.note.kind=kind;t.samples=4;s.targets={t};
        owner.accept(s);owner.take_accepted_plans();clock.set(20'000'000);
        s=snapshot(2,clock.now_ns());t.evidence_ns=clock.now_ns();t.expires_ns=clock.now_ns()+100'000'000;
        t.revision++;t.samples=0;s.targets={t};owner.accept(s);
        const auto canceled=owner.take_plan_cancellations();ASSERT_EQ(canceled.size(),1);
        EXPECT_EQ(canceled[0].at("reason"),"current_geometry_unsupported");EXPECT_TRUE(canceled[0].at("retry_without_prior_down"));
        clock.set(30'000'000);s=snapshot(3,clock.now_ns());t.evidence_ns=clock.now_ns();
        t.revision++;t.samples=4;t.uncertainty_ns=45'000'000;s.targets={t};owner.accept(s);
        EXPECT_TRUE(owner.take_accepted_plans().empty());owner.poll();EXPECT_TRUE(touch.receipts().empty());
        clock.set(40'000'000);s=snapshot(4,clock.now_ns());t.evidence_ns=clock.now_ns();
        t.expires_ns=clock.now_ns()+100'000'000;t.revision++;t.uncertainty_ns=2'000'000;s.targets={t};
        owner.accept(s);const auto rebuilt=owner.take_accepted_plans();ASSERT_EQ(rebuilt.size(),1);
        EXPECT_EQ(rebuilt[0].source_frame_sequence,4);clock.set(60'000'000);owner.poll();
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}
TEST(GameOwner, MissingAfterStartedOrCompletedDownCannotRetryReturnedPixels) {
    for(const auto kind:{NoteKind::tap,NoteKind::hold,NoteKind::drag,NoteKind::flick}) {
        SCOPED_TRACE(name(kind));FakeClock clock;FakeTouchBackend touch(clock);
        GamePlanOwner owner(clock,touch,2,{15,0,30'000'000});
        auto s=snapshot(1,0);auto t=target(1,0,20'000'000);t.note.kind=kind;t.samples=4;s.targets={t};
        owner.accept(s);owner.take_accepted_plans();clock.set(20'000'000);owner.poll();
        ASSERT_FALSE(touch.contacts().empty());clock.set(90'000'000);owner.accept(snapshot(2,clock.now_ns()));owner.poll();
        EXPECT_TRUE(touch.contacts().empty());
        for(const auto& canceled:owner.take_plan_cancellations())EXPECT_FALSE(canceled.at("retry_without_prior_down"));
        clock.set(95'000'000);s=snapshot(3,clock.now_ns());t.evidence_ns=clock.now_ns();
        t.expires_ns=clock.now_ns()+100'000'000;t.revision++;t.crossing_ns=110'000'000;s.targets={t};owner.accept(s);
        EXPECT_TRUE(owner.take_accepted_plans().empty());clock.set(110'000'000);owner.poll();
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    }
}
TEST(GameOwner, DragCoverageSurvivesLateCrossingWithoutReplayingDown) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,2,{4,0,30'000'000});
    auto t=target(1,0,30'000'000);t.note.kind=NoteKind::drag;t.samples=4;
    auto s=snapshot(1,0);s.targets={t};owner.accept(s);
    clock.set(15'000'000);ASSERT_EQ(owner.poll().size(),1);
    for(int i=1;i<=4;++i) {
        clock.set(i*20'000'000);s=snapshot(i+1,clock.now_ns());t.evidence_ns=clock.now_ns();
        t.expires_ns=clock.now_ns()+100'000'000;t.revision=i+1;s.targets={t};owner.accept(s);owner.poll();
    }
    // At 70 ms, a delayed visible crossing is still covered; no second down.
    EXPECT_EQ(touch.contacts().size(),1);EXPECT_EQ(touch.receipts().size(),1);
    clock.set(105'000'000);ASSERT_EQ(owner.poll().size(),1);EXPECT_TRUE(touch.contacts().empty());
    EXPECT_EQ(touch.receipts().size(),2);
}
TEST(GameOwner, DenseColocatedDragsShareOneActiveContactAndKeepTapIndependent) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,2,{5,0,30'000'000});
    auto a=target(1,0,30'000'000);a.note.kind=NoteKind::drag;a.samples=4;
    auto s=snapshot(1,0);s.targets={a};owner.accept(s);owner.take_accepted_plans();
    clock.set(15'000'000);ASSERT_EQ(owner.poll().size(),1);
    clock.set(20'000'000);auto b=target(2,clock.now_ns(),50'000'000);b.note.kind=NoteKind::drag;b.samples=4;
    a.evidence_ns=clock.now_ns();a.expires_ns=clock.now_ns()+100'000'000;a.revision++;
    auto tap_note=target(3,clock.now_ns(),40'000'000);tap_note.hit.x=850;tap_note.samples=4;
    s=snapshot(2,clock.now_ns());s.targets={a,b,tap_note};owner.accept(s);owner.take_accepted_plans();
    EXPECT_EQ(owner.scheduler().pending_count(),2);
    clock.set(40'000'000);const auto down=owner.poll();ASSERT_EQ(down.size(),1);
    EXPECT_EQ(down[0].command.x,850);EXPECT_EQ(touch.contacts().size(),2);
    clock.set(45'000'000);auto c=target(4,clock.now_ns(),70'000'000);c.note.kind=NoteKind::drag;c.samples=4;
    b.evidence_ns=clock.now_ns();b.expires_ns=clock.now_ns()+100'000'000;b.revision++;
    tap_note.evidence_ns=clock.now_ns();tap_note.expires_ns=clock.now_ns()+100'000'000;tap_note.revision++;
    // The original leader disappeared, but fresh colocated Drag evidence remains.
    s=snapshot(3,clock.now_ns());s.targets={b,c,tap_note};owner.accept(s);
    clock.set(60'000'000);owner.poll();EXPECT_EQ(touch.contacts().size(),1);
    EXPECT_TRUE(owner.scheduler().fault().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),2);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}
TEST(GameOwner, ADisappearingDragMemberCannotReleaseTheStillObservedGroup) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{4,0,30'000'000});
    auto a=target(1,0,30'000'000);a.note.kind=NoteKind::drag;a.samples=4;
    auto s=snapshot(1,0);s.targets={a};owner.accept(s);owner.take_accepted_plans();
    clock.set(15'000'000);owner.poll();clock.set(20'000'000);
    auto b=target(2,clock.now_ns(),50'000'000);b.note.kind=NoteKind::drag;b.samples=4;
    s=snapshot(2,clock.now_ns());s.targets={a,b};owner.accept(s);owner.take_accepted_plans();
    EXPECT_EQ(owner.scheduler().pending_count(),1);
    clock.set(60'000'000);a.evidence_ns=clock.now_ns();a.expires_ns=clock.now_ns()+100'000'000;a.revision++;
    s=snapshot(3,clock.now_ns());s.targets={a};owner.accept(s);
    EXPECT_EQ(touch.contacts().size(),1);
    clock.set(100'000'000);s=snapshot(4,clock.now_ns());owner.accept(s);
    EXPECT_TRUE(touch.contacts().empty());EXPECT_TRUE(owner.scheduler().fault().empty());
}
TEST(GameOwner, DifferentDragPositionsKeepIndependentContactsAndGateLossReleasesTheGroup) {
    for(const bool colocated:{false,true}) {
        FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,2,{4,0,30'000'000});
        auto a=target(1,0,30'000'000);a.note.kind=NoteKind::drag;a.samples=4;
        auto s=snapshot(1,0);s.targets={a};owner.accept(s);owner.take_accepted_plans();
        clock.set(15'000'000);owner.poll();clock.set(20'000'000);
        auto b=target(2,clock.now_ns(),50'000'000);b.note.kind=NoteKind::drag;b.samples=4;if(!colocated) b.hit.x=850;
        s=snapshot(2,clock.now_ns());s.targets={a,b};owner.accept(s);owner.take_accepted_plans();
        const auto coverage=owner.take_coverage_updates();EXPECT_EQ(coverage.size(),colocated?1:0);
        if(colocated) {EXPECT_EQ(coverage[0].at("note_id"),2);EXPECT_EQ(coverage[0].at("leader_note_id"),1);}
        clock.set(35'000'000);owner.poll();EXPECT_EQ(touch.contacts().size(),colocated?1:2);
        s=snapshot(3,clock.now_ns());s.playing_gate=false;owner.accept(s);
        EXPECT_TRUE(touch.contacts().empty());EXPECT_EQ(owner.scheduler().pending_count(),0);
        s=snapshot(4,clock.now_ns());s.context.epoch=2;s.targets={a,b};owner.accept(s);
        clock.set(50'000'000);owner.poll();owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}
TEST(GameOwner, BoundedLatePredictionRecoversNowButOldEvidenceAndExecutedDownCannotReplay) {
    for(const auto kind:{NoteKind::tap,NoteKind::hold,NoteKind::drag,NoteKind::flick}) {
        FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,2,{15,20'000'000,30'000'000});
        auto t=target(1,0,-10'000'000);t.note.kind=kind;t.samples=4;auto s=snapshot(1,0);s.targets={t};
        owner.accept(s);const auto plans=owner.take_accepted_plans();ASSERT_EQ(plans.size(),1);
        EXPECT_EQ(plans.front().steps.front().due_ns,0);ASSERT_EQ(owner.poll().size(),1);
        ASSERT_TRUE(plans.front().predicted_down_ns);EXPECT_EQ(*plans.front().predicted_down_ns,
            kind==NoteKind::drag?-45'000'000:-30'000'000);
        clock.set(5'000'000);s=snapshot(2,clock.now_ns());t.evidence_ns=clock.now_ns();t.revision=2;s.targets={t};
        owner.accept(s);owner.poll();EXPECT_EQ(touch.receipts().size(),1);owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,2,{15,20'000'000,30'000'000});
    auto s=snapshot(1,0);s.targets={target(1,0,-41'000'000)};owner.accept(s);EXPECT_TRUE(owner.poll().empty());
    clock.set(100'000'000);s=snapshot(2,0);s.targets={target(2,0,100'000'000)};owner.accept(s);
    EXPECT_TRUE(owner.poll().empty());EXPECT_TRUE(touch.contacts().empty());
    FakeClock lead_clock;FakeTouchBackend lead_touch(lead_clock);GamePlanOwner lead_owner(lead_clock,lead_touch,2,{1,35'000'000,30'000'000});
    s=snapshot(1,0);s.targets={target(1,0,-30'000'000)};lead_owner.accept(s);
    const auto plans=lead_owner.take_accepted_plans();ASSERT_EQ(plans.size(),1);
    ASSERT_TRUE(plans.front().predicted_down_ns);EXPECT_EQ(*plans.front().predicted_down_ns,-65'000'000);
    ASSERT_EQ(lead_owner.poll().size(),1);lead_owner.stop();EXPECT_TRUE(lead_touch.contacts().empty());
}
TEST(GameRuntime, ActualCaptureOptionsUseEffectiveProfileGuard) {
    RuntimeConfig c; c.width=1280; c.height=720; c.source_rotation=1;
    c.max_relative_lag_ms=73; c.grpc_read_chunk_kib=256;
    const auto options=runtime_capture_options(c);
    EXPECT_EQ(options.max_relative_lag_ns,73'000'000); EXPECT_EQ(options.grpc_read_chunk_kib,256);
    EXPECT_TRUE(options.optimized_rgb_copy); EXPECT_FALSE(options.rgba); EXPECT_FALSE(options.bottom_up);
    EXPECT_EQ(options.source_rotation,1);
}
TEST(GameOwner, LateGeometrySnapshotCannotCancelCurrentPendingIntent) {
    FakeClock clock; FakeTouchBackend touch(clock); GamePlanOwner owner(clock,touch);
    auto current=snapshot(1,0); current.context.geometry=2;
    current.targets={target(9,0,10'000'000)}; owner.accept(current);
    ASSERT_EQ(owner.scheduler().pending_count(),1);
    clock.set(5'000'000); auto old=snapshot(2,5'000'000);
    owner.accept(old); EXPECT_EQ(owner.last_rejection(),"context_order");
    EXPECT_EQ(owner.scheduler().pending_count(),1);
    clock.set(10'000'000); ASSERT_EQ(owner.poll().size(),1);
    EXPECT_EQ(touch.contacts().size(),1);
    owner.stop(); EXPECT_TRUE(touch.contacts().empty());
}

TEST(GameRuntime, HistoricalFingerprintRejectsMappingDeviceAndUnverifiedReports) {
    using nlohmann::json;
    RuntimeConfig c; c.serial="emulator-5554"; c.width=1280; c.height=720;
    c.source_rotation=1; c.touch_width=720; c.touch_height=1280; c.touch_rotation=90;
    const json device={{"android_release","16"},{"android_sdk","36"},{"cpu_abi","x86_64"},
                       {"model","AVD"},{"wm_size","720x1280"},{"wm_density","320"}};
    json report={{"schema_version",2},{"fixture_schema","native_touch_v2"},{"serial",c.serial},
        {"capability_verified",true},{"fixture_apk_sha256","hash"},{"installed_apk_sha256","hash"},
        {"device_report",device},{"config",{
            {"capture",{{"kind","emulator-grpc"},{"transport","payload"},{"image_format","rgb888"},
                        {"row_order","top-down"},{"width",1280},{"height",720},{"source_rotation",1}}},
            {"touch",{{"kind","emulator-grpc"},{"width",720},{"height",1280},
                      {"rotation_deg",90},{"max_contacts",2}}}}}};
    EXPECT_TRUE(match_touch_capability(c,report,device,"hash")["fingerprint_matches"].get<bool>());
    auto invalid=report; invalid["capability_verified"]=false;
    EXPECT_FALSE(match_touch_capability(c,invalid,device,"hash")["fingerprint_matches"].get<bool>());
    invalid=report; invalid["config"]["touch"]["rotation_deg"]=0;
    EXPECT_FALSE(match_touch_capability(c,invalid,device,"hash")["fingerprint_matches"].get<bool>());
    auto changed=device; changed["wm_density"]="480";
    EXPECT_FALSE(match_touch_capability(c,report,changed,"hash")["fingerprint_matches"].get<bool>());
    EXPECT_FALSE(match_touch_capability(c,report,device,"different")["fingerprint_matches"].get<bool>());
    c.max_contacts=5;report["config"]["touch"]["max_contacts"]=5;
    EXPECT_FALSE(match_touch_capability(c,report,device,"hash")["fingerprint_matches"].get<bool>());
    report["max_contacts_verified"]=4;
    EXPECT_FALSE(match_touch_capability(c,report,device,"hash")["fingerprint_matches"].get<bool>());
    report["max_contacts_verified"]=5;
    EXPECT_TRUE(match_touch_capability(c,report,device,"hash")["fingerprint_matches"].get<bool>());
}

TEST(AutoPlay, PixelsConfirmSelectionAndOneAttemptNeverBecomesGameplayInput) {
    FakeClock clock; GameObserver observer(clock); DecisionSnapshot menu;
    for(int i=0;i<3;++i) {
        auto f=image(i+1,i*20'000'000);
        rect(f,448,245,103,87,{255,255,255}); rect(f,1152,580,128,110,{255,255,255});
        // Right-facing outline on the white Play panel.
        rect(f,1190,615,2,38,{0,0,0});
        for(int y=615;y<653;++y) {
            const int x=1218-static_cast<int>(std::abs(y-634)*1.4);
            rect(f,x,y,2,1,{0,0,0});
        }
        clock.set(f.capture_complete_ns); menu=observer.process(f);
        EXPECT_EQ(menu.ui==GameUi::menu,i==2);
    }
    ASSERT_TRUE(menu.play_button); EXPECT_FALSE(menu.playing_gate);
    FakeTouchBackend touch(clock); ContactScheduler scheduler(clock,touch,2,1,2);
    PlayButtonPlanner planner; const auto p=planner.take(menu,clock.now_ns()); ASSERT_TRUE(p);
    ASSERT_TRUE(scheduler.set_context(menu.context.epoch,menu.context.generation,menu.context.geometry,true,
                                      menu.context.capture_ns));
    ASSERT_TRUE(scheduler.submit(*p)); ASSERT_EQ(scheduler.run_due().size(),1);
    clock.set(clock.now_ns()+20'000'000); ASSERT_EQ(scheduler.run_due().size(),1);
    EXPECT_TRUE(touch.contacts().empty()); EXPECT_FALSE(planner.take(menu,clock.now_ns()));
    menu.ui=GameUi::playing; menu.playing_gate=true;
    PlayButtonPlanner another; EXPECT_FALSE(another.take(menu,clock.now_ns()));
}
namespace {
DecisionSnapshot overlapping_drag(std::uint64_t sequence,Nanoseconds time,std::uint64_t id=1) {
    auto s=snapshot(sequence,time);auto t=target(id,time,time);
    t.note={{400,577},NoteKind::drag,128,12,.55};t.hit={400,576};
    t.samples=2;t.history_span_ns=24'000'000;t.line_id=7;
    t.crossing_ns.reset();t.reason="nonlinear_or_mismatch";t.distance=-41;t.residual=32;
    s.targets={t};s.lines={{{640,576},{1,0},1280,2,.9,7,time}};return s;
}
}
TEST(GameOwner, CurrentDragOverlapStartsWithoutInventingARootAndTracksMovingLine) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{4,35'000'000,30'000'000});
    auto s=overlapping_drag(1,0);s.targets[0].samples=1;s.targets[0].history_span_ns=0;
    owner.accept(s);owner.poll();EXPECT_TRUE(touch.contacts().empty());
    clock.set(20'000'000);s=overlapping_drag(2,clock.now_ns());owner.accept(s);
    const auto plans=owner.take_accepted_plans();ASSERT_EQ(plans.size(),1);
    EXPECT_EQ(plans[0].basis,"live_pixels_current_drag_overlap");EXPECT_FALSE(plans[0].predicted_down_ns);
    EXPECT_EQ(plans[0].steps.front().due_ns,20'000'000);EXPECT_EQ(plans[0].steps.back().due_ns,120'000'000);
    owner.poll();ASSERT_EQ(touch.contacts().size(),1);const auto finger=touch.contacts().begin()->first;
    for(int i=1;i<=8;++i) {
        clock.set(20'000'000+i*20'000'000);s=overlapping_drag(i+2,clock.now_ns());
        s.lines[0].center.y+=i*3;s.targets[0].note.center.x+=i*12;
        s.targets[0].note.center.y+=i*3;s.targets[0].hit={s.targets[0].note.center.x,s.lines[0].center.y};
        owner.accept(s);owner.poll();owner.take_accepted_plans();
        ASSERT_EQ(touch.contacts().size(),1);EXPECT_EQ(touch.contacts().begin()->first,finger);
        EXPECT_NEAR(touch.contacts().begin()->second[1],s.lines[0].center.y,0.01);
    }
    clock.set(219'000'000);owner.accept(snapshot(20,clock.now_ns()));EXPECT_EQ(touch.contacts().size(),1);
    clock.set(220'000'000);owner.accept(snapshot(21,clock.now_ns()));EXPECT_TRUE(touch.contacts().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    owner.stop();
}
TEST(GameObserver, TiltedMovingDragUsesLocalCoreAndOneCurrentContact) {
    FakeClock clock;GameObserver observer(clock);FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,1,{4,35'000'000,30'000'000});std::uint64_t line_id=0,note_id=0;
    std::optional<int> finger;
    for(int i=0;i<9;++i) {
        auto f=image(i+1,1'000'000+i*20'000'000);hud(f);
        const double angle=.8+i*.025;const Vec2 u{std::cos(angle),std::sin(angle)},c{640,340.0+i*10};
        const Vec2 hit{c.x+u.x*(-120+i*6),c.y+u.y*(-120+i*6)};
        oriented_box(f,c,u,750,3,{245,245,245});oriented_box(f,hit,u,128,10,{240,220,40});
        clock.set(f.capture_complete_ns);const auto s=observer.process(f);
        ASSERT_EQ(s.lines.size(),1);ASSERT_EQ(s.targets.size(),1);const auto& t=s.targets[0];
        ASSERT_EQ(t.note.kind,NoteKind::drag);EXPECT_GE(s.lines[0].confidence,.8);
        EXPECT_NEAR(t.note.width,128,5);EXPECT_LE(t.note.height,14);
        if(!line_id){line_id=s.lines[0].track_id;note_id=t.note_id;}
        EXPECT_EQ(s.lines[0].track_id,line_id);EXPECT_EQ(t.note_id,note_id);EXPECT_EQ(t.line_id,line_id);
        EXPECT_FALSE(t.crossing_ns);owner.accept(s);owner.poll();
        const auto plans=owner.take_accepted_plans();for(const auto& p:plans)EXPECT_EQ(p.basis,"live_pixels_current_drag_overlap");
        if(i>=2) {ASSERT_EQ(touch.contacts().size(),1);
            if(!finger)finger=touch.contacts().begin()->first;EXPECT_EQ(touch.contacts().begin()->first,*finger);
            const double dx=touch.contacts().begin()->second[0]-hit.x,dy=touch.contacts().begin()->second[1]-hit.y;
            EXPECT_LE(std::abs(-dx*u.y+dy*u.x),3);EXPECT_LE(std::abs(dx*u.x+dy*u.y),48);
        }
    }
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    EXPECT_GE(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::move;}),2);
    clock.set(220'000'000);owner.accept(snapshot(20,clock.now_ns()));EXPECT_TRUE(touch.contacts().empty());owner.stop();
}
TEST(GameOwner, DragOverlapRejectsMissingStaleAmbiguousAndNonCoreGeometry) {
    for(int rejection=0;rejection<15;++rejection) {
        SCOPED_TRACE(rejection);FakeClock clock;FakeTouchBackend touch(clock);
        GamePlanOwner owner(clock,touch,1,{4,35'000'000,30'000'000});auto s=overlapping_drag(1,0);auto& t=s.targets[0];
        switch(rejection) {
        case 0:s.lines.clear();break;
        case 1:s.lines[0].observed_ns=-1;break;
        case 2:s.lines[0].association_valid=false;break;
        case 3:s.lines[0].track_id=8;break;
        case 4:t.reason="association_ambiguous";break;
        case 5:t.reason="multiple_line_association_unvalidated";break;
        case 6:t.note.center.y=590;break;
        case 7:t.hit.x+=30;break;
        case 8:t.note.height=70;break;
        case 9:t.note.rails_geometry=true;break;
        case 10:t.note.tangent={0,1};break;
        case 11:t.expires_ns=0;break;
        case 12:t.history_span_ns=1'000'000;break;
        case 13:clock.set(100'000'000);break;
        case 14:t.note.confidence=.3;break;
        }
        owner.accept(s);owner.poll();EXPECT_TRUE(touch.receipts().empty());EXPECT_TRUE(owner.take_accepted_plans().empty());
    }
}
TEST(GameOwner, PendingDragPredictionCanBecomeACurrentSpatialContactExactlyOnce) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{4,35'000'000,30'000'000});
    auto s=overlapping_drag(1,0);auto& t=s.targets[0];t.note.center.y=510;t.hit={400,576};
    t.reason="prediction_observe_only";t.crossing_ns=100'000'000;owner.accept(s);owner.take_accepted_plans();
    clock.set(20'000'000);s=overlapping_drag(2,clock.now_ns());owner.accept(s);
    const auto plans=owner.take_accepted_plans();ASSERT_EQ(plans.size(),1);
    EXPECT_EQ(plans[0].basis,"live_pixels_current_drag_overlap");EXPECT_FALSE(plans[0].predicted_down_ns);
    EXPECT_TRUE(owner.take_plan_cancellations().empty());owner.poll();ASSERT_EQ(touch.contacts().size(),1);
    clock.set(60'000'000);owner.accept(snapshot(3,clock.now_ns()));EXPECT_TRUE(touch.contacts().empty());
    clock.set(70'000'000);s=overlapping_drag(4,clock.now_ns());owner.accept(s);owner.poll();
    EXPECT_TRUE(touch.contacts().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
}
TEST(GameOwner, ConsecutiveSpatialDragCoresShareTheStartedFingerWithoutAnyCrossing) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{4,35'000'000,30'000'000});
    auto s=overlapping_drag(1,0);owner.accept(s);owner.poll();owner.take_accepted_plans();
    ASSERT_EQ(touch.contacts().size(),1);const auto finger=touch.contacts().begin()->first;
    clock.set(30'000'000);s=overlapping_drag(2,clock.now_ns(),2);s.targets[0].note.center.x+=10;s.targets[0].hit.x+=10;
    owner.accept(s);owner.poll();ASSERT_EQ(touch.contacts().size(),1);EXPECT_EQ(touch.contacts().begin()->first,finger);
    const auto updates=owner.take_coverage_updates();ASSERT_EQ(updates.size(),1);
    EXPECT_EQ(updates[0].at("event"),"game_drag_coverage");EXPECT_TRUE(updates[0].at("crossing_ns").is_null());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),1);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}
TEST(AutoPlay, UnknownStaleOutOfFrameAndContradictoryScenesCannotStart) {
    auto s=snapshot(1,0); s.ui=GameUi::menu; s.playing_gate=false; s.play_button=Vec2{1200,630};
    PlayButtonPlanner planner;
    EXPECT_FALSE(planner.take(s,60'000'000));
    s.ui=GameUi::unknown; EXPECT_FALSE(planner.take(s,0));
    s.ui=GameUi::menu; s.play_button=Vec2{1280,630}; EXPECT_FALSE(planner.take(s,0));
    s.play_button=Vec2{1200,630}; s.capacity_valid=false; EXPECT_FALSE(planner.take(s,0));
    s.capacity_valid=true; s.playing_gate=true; EXPECT_FALSE(planner.take(s,0));
    s.playing_gate=false; EXPECT_TRUE(planner.take(s,0));
}
