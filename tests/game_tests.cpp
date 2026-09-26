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

TEST(GameObserver, OnePixelLineJoinedToHoldBorderStaysObservable) {
    FakeClock clock; GameObserver observer(clock); auto f=image(1,1);
    hud(f); rect(f,0,575,1280,1,{255,255,255});
    rect(f,400,200,2,376,{255,255,255}); rect(f,550,200,2,376,{255,255,255});
    rect(f,403,200,144,350,{40,190,255}); clock.set(1);
    const auto s=observer.process(f); ASSERT_EQ(s.lines.size(),1);
    EXPECT_NEAR(s.lines[0].center.y,575,1); EXPECT_GT(s.lines[0].length,1200);
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
