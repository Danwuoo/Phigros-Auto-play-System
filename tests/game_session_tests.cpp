#include "pas/game_session.hpp"
#include "pas/result_ui_labels.hpp"
#include "pas/session_archive.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include "hd9_strategy_trace.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <chrono>

using namespace pas;
using namespace pas::test;
namespace {
SceneContext context(std::uint64_t sequence,Nanoseconds time) {return {1,1,1,sequence,time,1280,720,1};}
SessionUiEvidence hud_evidence(){SessionUiEvidence e;e.hud=true;return e;}
SessionUiEvidence result_evidence(){SessionUiEvidence e;e.result=true;e.result_labels=6;return e;}
void paint_result(Frame& f,int omit=-1) {
    for(std::size_t i=0;i<result_ui::labels.size();++i) {
        if(static_cast<int>(i)==omit)continue;const auto& l=result_ui::labels[i];
        for(int y=0;y<l.height;++y)for(int x=0;x<l.width;++x)
            if(l.bits[static_cast<std::size_t>(y)*l.width+x]=='#')box(f,l.x+x,l.y+y,1,1,{245,245,245});
    }
}
SessionStatus playing(PlaySessionLifecycle& l,std::uint64_t& seq,Nanoseconds& t) {
    SessionStatus s;for(int i=0;i<3;++i){t+=20'000'000;s=l.observe(context(++seq,t),hud_evidence(),true,t);}return s;
}
DecisionSnapshot scene(std::uint64_t seq,Nanoseconds t) {
    DecisionSnapshot s;s.sequence=seq;s.context=context(seq,t);s.playing_gate=true;s.ui=GameUi::playing;return s;
}
GameTarget hold(Nanoseconds t) {
    GameTarget v;v.note_id=1;v.revision=1;v.evidence_ns=t;v.expires_ns=t+100'000'000;
    v.note.kind=NoteKind::hold;v.note.width=144;v.note.height=200;v.crossing_ns=t+35'000'000;
    v.uncertainty_ns=2'000'000;v.hit={500,540};v.line_id=1;v.reason="prediction_observe_only";return v;
}
}
TEST(ManualLifecycle, UnlimitedStandbyNoNotesOrTimeBasedStart) {
    PlaySessionLifecycle l;for(int i=0;i<100000;++i) {
        const auto t=Nanoseconds{i+1}*1'000'000'000;
        const auto s=l.observe(context(i+1,t),{},false,t);
        ASSERT_FALSE(s.active);ASSERT_EQ(s.round,0);ASSERT_EQ(s.state,PlaySessionState::standby);
    }
}
TEST(ManualLifecycle, TwoManualRoundsShortAndLongWithResultLatch) {
    PlaySessionLifecycle l;std::uint64_t seq=0;Nanoseconds t=0;
    for(int round=1;round<=2;++round) {
        auto s=playing(l,seq,t);ASSERT_EQ(s.state,PlaySessionState::playing);ASSERT_EQ(s.round,round);
        t+=round==1?300'000'000:400'000'000'000; // No fixed 185-second tail.
        s=l.observe(context(++seq,t),hud_evidence(),true,t);ASSERT_TRUE(s.active);
        for(int i=0;i<3;++i){t+=40'000'000;s=l.observe(context(++seq,t),result_evidence(),false,t);}
        ASSERT_TRUE(s.ended);ASSERT_EQ(s.state,PlaySessionState::result);
        for(int i=0;i<8;++i){t+=40'000'000;s=l.observe(context(++seq,t),result_evidence(),false,t);ASSERT_FALSE(s.active);ASSERT_EQ(s.round,round);}
        // A stray gameplay-looking frame on the still latched result cannot start.
        t+=20'000'000;s=l.observe(context(++seq,t),hud_evidence(),true,t);ASSERT_FALSE(s.active);
        SessionUiEvidence menu;menu.menu=true;t+=20'000'000;l.observe(context(++seq,t),menu,false,t);
    }
}
TEST(ManualLifecycle, BlackLoadingPauseAndHudLossDoNotEndOrResetRound) {
    PlaySessionLifecycle l;std::uint64_t seq=0;Nanoseconds t=0;ASSERT_TRUE(playing(l,seq,t).active);
    for(int i=0;i<4;++i){t+=5'000'000'000;const auto s=l.observe(context(++seq,t),{},false,t);EXPECT_TRUE(s.active);EXPECT_EQ(s.round,1);}
    EXPECT_EQ(playing(l,seq,t).round,1);
    t+=20'000'000;auto s=l.observe(context(++seq,t),result_evidence(),false,t);EXPECT_FALSE(s.ended);
    t+=20'000'000;s=l.observe(context(++seq,t),{},false,t);EXPECT_TRUE(s.active);EXPECT_FALSE(s.ended);
}
TEST(ManualLifecycle, ResultRequiresDistinctFreshFramesAndPhysicalGeometry) {
    PlaySessionLifecycle l;std::uint64_t seq=0;Nanoseconds t=0;playing(l,seq,t);
    t+=20'000'000;const auto c=context(++seq,t);
    l.observe(c,result_evidence(),false,t);
    for(int i=0;i<4;++i)EXPECT_FALSE(l.observe(c,result_evidence(),false,t).ended);
    t+=20'000'000;EXPECT_FALSE(l.observe(context(++seq,t),result_evidence(),false,t+100'000'000).ended);
    auto changed=context(++seq,t+20'000'000);changed.geometry=2;
    EXPECT_EQ(l.observe(changed,hud_evidence(),true,changed.capture_ns).state,PlaySessionState::fault);
}
TEST(ManualLifecycle, EpochRevocationDoesNotCreateANewRoundAndWatchdogAborts) {
    PlaySessionLifecycle l(500'000'000);std::uint64_t seq=0;Nanoseconds t=0;playing(l,seq,t);
    auto c=context(++seq,t+=20'000'000);c.epoch=900;
    EXPECT_EQ(l.observe(c,hud_evidence(),true,t).round,1);
    t+=500'000'000;const auto s=l.observe(context(++seq,t),{},false,t);
    EXPECT_EQ(s.state,PlaySessionState::fault);EXPECT_TRUE(s.ended);EXPECT_EQ(s.reason,"round_watchdog_aborted");
}
TEST(ManualUiPixels, AllSixLabelsPositiveMissingLabelsUniformHudAndBlankNegative) {
    FakeClock clock;GameObserver observer(clock);
    auto f=session_image(1,1);paint_result(f);auto s=observer.process(f);
    auto e=session_ui_pixels(f,s);EXPECT_TRUE(e.result);EXPECT_EQ(e.result_labels,6);EXPECT_DOUBLE_EQ(e.result_similarity,1);
    for(int i=0;i<6;++i){auto partial=session_image(i+2,20'000'001+i*20'000'000);paint_result(partial,i);
        EXPECT_FALSE(session_ui_pixels(partial,observer.process(partial)).result);}
    auto blank=session_image(10,300'000'000);EXPECT_FALSE(session_ui_pixels(blank,observer.process(blank)).result);
    std::fill(blank.rgb.begin(),blank.rgb.end(),255);EXPECT_FALSE(session_ui_pixels(blank,observer.process(blank)).result);
    auto hud=session_image(11,320'000'000);session_hud(hud);paint_result(hud);
    EXPECT_FALSE(session_ui_pixels(hud,observer.process(hud)).result);
    f.source_valid=false;EXPECT_FALSE(session_ui_pixels(f,s).result);
}
TEST(ManualUiPixels, ExistingResultAndMenuEvidenceOfflineOnly) {
    const auto root=std::filesystem::path(PAS_TEST_SOURCE_DIR)/"measurements/outline-contact-20260927";
    if(!std::filesystem::exists(root))GTEST_SKIP()<<"Historical local evidence unavailable";
    FakeClock clock;int positives=0,negatives=0;
    for(const auto& item:std::filesystem::directory_iterator(root)) {
        const auto path=item.path()/"diagnostic.png";if(!std::filesystem::exists(path))continue;
        const auto file=item.path().filename().string();
        if(file.find("result")==std::string::npos&&file.find("before-state")==std::string::npos)continue;
        auto f=load_diagnostic_png(path);f.source_rotation=1;GameObserver observer(clock);f.sequence=1;f.capture_complete_ns=1;
        const auto e=session_ui_pixels(f,observer.process(f));
        if(file.find("result")!=std::string::npos){EXPECT_TRUE(e.result)<<file<<" labels="<<e.result_labels<<" similarity="<<e.result_similarity;++positives;}
        else {EXPECT_FALSE(e.result)<<file;++negatives;}
    }
    EXPECT_GE(positives,10);EXPECT_GE(negatives,4);
}
TEST(ManualOwner, StartingAndStandbyNeverInjectAndStopReleasesHold) {
    FakeClock clock;FakeTouchBackend backend(clock);SessionGameOwner owner(clock,backend,5,{15,35'000'000,30'000'000});
    clock.set(1'000'000'000);auto s=scene(1,clock.now_ns());s.targets={hold(clock.now_ns())};
    EXPECT_TRUE(owner.accept(s,true).empty());EXPECT_TRUE(backend.receipts().empty());
    owner.start(1);EXPECT_TRUE(owner.accept(s,false).empty());EXPECT_TRUE(backend.receipts().empty());
    s=scene(2,clock.now_ns());s.targets={hold(clock.now_ns())};owner.accept(s,true);owner.poll();
    ASSERT_EQ(backend.contacts().size(),1);const auto r=owner.finish();EXPECT_EQ(r.requested_ids.size(),1);EXPECT_TRUE(backend.contacts().empty());
}
TEST(ManualOwner, HudLossCannotReplayCompletedIntentAndNewRoundCanUseSameNote) {
    FakeClock clock;FakeTouchBackend backend(clock);SessionGameOwner owner(clock,backend,5,{15,35'000'000,30'000'000});
    for(int round=1;round<=2;++round) {
        clock.set(Nanoseconds{round}*1'000'000'000);owner.start(round);
        auto s=scene(round*10,clock.now_ns());s.context.epoch=round;s.targets={hold(clock.now_ns())};
        owner.accept(s,true);owner.poll();ASSERT_EQ(backend.contacts().size(),1);
        owner.suspend("HUD_lost");EXPECT_TRUE(backend.contacts().empty());
        clock.set(clock.now_ns()+20'000'000);++s.sequence;s.context.frame++;s.context.capture_ns=clock.now_ns();
        s.targets={hold(clock.now_ns())};s.targets[0].revision=2;
        owner.accept(s,true);EXPECT_TRUE(backend.contacts().empty());owner.finish();
    }
    EXPECT_EQ(std::count_if(backend.receipts().begin(),backend.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),2);
}
TEST(ManualOwner, StaleEvidenceCannotDownAndUnknownInputCannotRebuild) {
    FakeClock clock;FakeTouchBackend backend(clock);SessionGameOwner owner(clock,backend,5,{15,35'000'000,30'000'000});
    owner.start(1);clock.set(1'000'000'000);auto s=scene(1,clock.now_ns()-100'000'000);s.targets={hold(s.context.capture_ns)};
    owner.accept(s,true);EXPECT_TRUE(backend.contacts().empty());EXPECT_TRUE(owner.poll().empty());owner.finish();
    class Unknown final:public TouchBackend {public:const Clock& clock;explicit Unknown(const Clock& c):clock(c){}
        bool injected=false;
        TouchReceipt inject(const TouchCommand& cmd) override{injected=true;return {cmd,clock.now_ns(),clock.now_ns(),false,"unknown"};}
        ReleaseReport release_all()override{ReleaseReport r;if(injected)r.unknown_ids={0};return r;}} bad(clock);
    SessionGameOwner fault(clock,bad,5,{15,35'000'000,30'000'000});fault.start(1);
    s=scene(2,clock.now_ns());s.targets={hold(clock.now_ns())};fault.accept(s,true);fault.poll();
    EXPECT_THROW(fault.finish(),std::runtime_error);
    EXPECT_THROW(fault.start(2),std::logic_error);
}
TEST(ManualStrategy, SamePixelsFakeClockTargetsPlansReceiptsMatchPremergeMainGolden) {
    const auto original=hd9_trace(false),wrapped=hd9_trace(true);EXPECT_EQ(original,wrapped);
    std::ifstream file(std::filesystem::path(PAS_TEST_SOURCE_DIR)/"tests/data/main-strategy-golden.json");
    ASSERT_TRUE(file);const auto golden=nlohmann::json::parse(file);EXPECT_EQ(wrapped,golden);
}
TEST(ManualIntegration, SameProcessPixelsTwoRoundsHoldResultAndFreshReset) {
    FakeClock clock;FakeTouchBackend backend(clock);SessionPerception perception(clock);
    SessionGameOwner owner(clock,backend,5,{15,35'000'000,30'000'000});
    std::uint64_t sequence=0,active=0;Nanoseconds t=1'000'000'000;
    const auto consume=[&](Frame f) {
        clock.set(f.capture_complete_ns);auto p=perception.process(f);
        if(!active&&p.status.active){active=p.status.round;owner.start(active);}
        if(active) {
            if(!p.status.active){owner.finish();active=0;}
            else {owner.accept(p.scene,p.allow_down);owner.poll();}
        }
        return p;
    };
    for(int round=1;round<=2;++round) {
        // Blank loading never injects. Both lifecycle and observer are the runtime's real classes.
        for(int i=0;i<4;++i){auto f=session_image(++sequence,t+=40'000'000);consume(std::move(f));}
        EXPECT_TRUE(backend.contacts().empty());
        SessionObservation p;
        for(int i=0;i<18;++i) {
            auto f=session_image(++sequence,t+=20'000'000);session_hud(f);f.epoch=round;
            box(f,100,540,1080,3,{255,255,255});const int head=340+i*14;
            box(f,420,head-140,144,140,{40,190,255});
            box(f,416,head-140,4,144,{245,245,245});box(f,564,head-140,4,144,{245,245,245});
            p=consume(std::move(f));
        }
        ASSERT_EQ(p.status.round,round);ASSERT_EQ(p.status.state,PlaySessionState::playing);
        ASSERT_FALSE(backend.receipts().empty());
        for(int i=0;i<4;++i){auto f=session_image(++sequence,t+=40'000'000);f.epoch=round;paint_result(f);p=consume(std::move(f));}
        EXPECT_FALSE(p.status.active);EXPECT_EQ(active,0);EXPECT_TRUE(backend.contacts().empty());
        const auto downs=std::count_if(backend.receipts().begin(),backend.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;});
        for(int i=0;i<5;++i){auto f=session_image(++sequence,t+=40'000'000);f.epoch=round;paint_result(f);consume(std::move(f));}
        EXPECT_EQ(std::count_if(backend.receipts().begin(),backend.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),downs);
    }
    EXPECT_GE(std::count_if(backend.receipts().begin(),backend.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}),2);
}
TEST(ManualIntegration, ResultCandidateClosesDownBeforeResultConfirmation) {
    FakeClock clock;SessionPerception perception(clock);SessionObservation p;
    for(int i=0;i<5;++i) {
        auto f=session_image(i+1,1'000'000'000+i*20'000'000);session_hud(f);clock.set(f.capture_complete_ns);p=perception.process(f);
    }
    ASSERT_TRUE(p.allow_down);const auto round=p.status.round;
    for(int i=0;i<3;++i) {
        auto f=session_image(i+6,1'200'000'000+i*40'000'000);paint_result(f);clock.set(f.capture_complete_ns);p=perception.process(f);
        EXPECT_FALSE(p.allow_down);EXPECT_EQ(p.status.round,round);
        EXPECT_EQ(p.status.active,i<2);
    }
    EXPECT_EQ(p.status.state,PlaySessionState::result);
}
TEST(ManualArchive, StandbyRotatesAndManyRoundsHaveIndependentHashesNoImages) {
    const auto root=std::filesystem::temp_directory_path()/("pas-session-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    SessionArchive archive(root,{{"strategy","HD9"}},1024,256);
    for(int i=0;i<1000;++i)archive.event(0,{{"event","standby"},{"sample",i}});
    for(int i=1;i<=20;++i) {
        for(int j=0;j<10;++j)archive.event(i,{{"event","test"},{"sample",j}});
        archive.complete(i,{{"status","result_confirmed"}});
    }
    archive.close();ASSERT_FALSE(archive.faulted())<<archive.error();EXPECT_LE(archive.peak_queue(),8192);
    int standby=0;for(const auto& file:std::filesystem::directory_iterator(root))
        if(file.path().filename().string().find("standby-")==0){++standby;EXPECT_LE(file.file_size(),256);}
    EXPECT_EQ(standby,4);
    for(int i=1;i<=20;++i) {
        const auto directory=root/("round-"+std::to_string(i));std::ifstream f(directory/"summary.json");
        const auto s=nlohmann::json::parse(f);EXPECT_EQ(s.at("round_id"),i);EXPECT_FALSE(std::filesystem::exists(directory/"result.png"));
        for(const auto& segment:s.at("event_segments"))EXPECT_EQ(segment.at("sha256"),sha256_file(directory/segment.at("path").get<std::string>()));
    }
    std::filesystem::remove_all(root);
}
TEST(ManualArchive, OneResultImageWrittenAndIncompleteRoundRemainsAborted) {
    const auto root=std::filesystem::temp_directory_path()/("pas-result-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    SessionArchive archive(root,{{"strategy","HD9"}});
    auto f=std::make_shared<Frame>(session_image(1,1));paint_result(*f);
    archive.event(1,{{"event","released"}});archive.complete(1,{{"status","result_confirmed"}},f);
    archive.event(2,{{"event","unfinished"}});archive.close();ASSERT_FALSE(archive.faulted())<<archive.error();
    std::ifstream a(root/"round-1/summary.json"),b(root/"round-2/summary.json");
    const auto completed=nlohmann::json::parse(a),partial=nlohmann::json::parse(b);
    a.close();b.close();
    EXPECT_EQ(completed.at("result_image_sha256"),sha256_file(root/"round-1/result.png"));
    EXPECT_EQ(partial.at("status"),"aborted");EXPECT_TRUE(partial.at("partial").get<bool>());
    EXPECT_FALSE(std::filesystem::exists(root/"round-2/result.png"));std::filesystem::remove_all(root);
}
TEST(ManualArchive, OversizedRecordFaultsRatherThanGrowingStandbyStorage) {
    const auto root=std::filesystem::temp_directory_path()/("pas-archive-bound-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    SessionArchive archive(root,{{"strategy","HD9"}},256,256);
    archive.event(0,{{"event","oversized"},{"value",std::string(512,'x')}});archive.close();
    EXPECT_TRUE(archive.faulted());EXPECT_FALSE(std::filesystem::exists(root/"standby-0.jsonl"));std::filesystem::remove_all(root);
}
