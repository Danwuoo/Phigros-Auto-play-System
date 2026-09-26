#include "pas/game.hpp"
#include "pas/runtime.hpp"
#include <gtest/gtest.h>
#include <algorithm>

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
        if(i==0) EXPECT_FALSE(t.note.tail);else {ASSERT_TRUE(t.note.tail);EXPECT_NEAR(t.note.tail->y,top,3);}
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
            EXPECT_NEAR(held->note.width,144,2);
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
