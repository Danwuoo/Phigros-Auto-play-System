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
TEST(GameObserver, UiLossCancelsAndCapacityIsBounded) {
    FakeClock clock; GameObserver observer(clock);
    for(int i=0;i<3;++i) {auto f=image(i+1,i*20'000'000); hud(f); clock.set(f.capture_complete_ns);
        const auto s=observer.process(f); EXPECT_EQ(s.playing_gate,i==2);}
    auto blank=image(4,60'000'000); clock.set(blank.capture_complete_ns);
    EXPECT_FALSE(observer.process(blank).playing_gate);
    auto f=image(5,80'000'000); hud(f);
    for(int y=100;y<660;y+=24) for(int x=10;x<1220;x+=36) rect(f,x,y,24,6,{20,190,255});
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
TEST(GameObserver, InterruptedThinBandIsOneLineButSeparatedBandsRemainDistinct) {
    FakeClock clock; GameObserver observer(clock); auto f=image(1,1);
    rect(f,0,574,424,2,{255,255,255});
    rect(f,568,574,712,2,{255,255,255});
    rect(f,0,576,1280,2,{255,255,255});
    clock.set(1); const auto first=observer.process(f);
    ASSERT_EQ(first.lines.size(),1); EXPECT_NEAR(first.lines[0].center.y,575.5,0.1);
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
    s=snapshot(2,10'000'000); owner.accept(s); EXPECT_TRUE(touch.contacts().empty());
    s=snapshot(3,10'000'000); s.targets={target(80,10'000'000,20'000'000)};
    owner.accept(s); clock.set(20'000'000); EXPECT_TRUE(owner.poll().empty());
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
