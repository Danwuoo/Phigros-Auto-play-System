#include "x4_input.hpp"
#include "replay_support.hpp"
#include "pas/game_tracking.hpp"
#include <gtest/gtest.h>
using namespace pas;
namespace {
using json=nlohmann::json;
class X4Oracle:public ::testing::Test {
protected:
    NoteCandidate n;LineCandidate vertical,horizontal;DecisionSnapshot s;
    std::vector<GameTrackHistory> tracks;std::uint64_t next=0;
    void SetUp() override {
        x4::reset();n.kind=NoteKind::drag;n.center={21,287};n.width=152;n.height=16;n.tangent={0,1};n.confidence=.8;
        vertical.center={938,363.5};vertical.tangent={0,1};vertical.length=711;vertical.confidence=.85;vertical.track_id=20;vertical.observed_ns=1'000'000'000;
        horizontal=vertical;horizontal.center={640,273};horizontal.tangent={1,0};horizontal.length=1280;horizontal.track_id=10;
        s.context={1,1,1,17927,1'000'000'000,1280,720,1};s.sequence=1;s.playing_gate=true;s.ui=GameUi::playing;s.lines={horizontal,vertical};
        bind();
    }
    void TearDown() override {x4::reset();}
    void bind() {
        x4::packets[6160]={{"ordinal",6160},{"source_frame",s.context.frame},{"png_sha256",std::string(64,'a')},
            {"core",x4::note_geometry(n)},{"line",x4::line_geometry(vertical)},{"proposal_source","synthetic independent geometry"}};
    }
    json run(bool enabled=true,std::vector<NoteCandidate> notes={}) {
        if(notes.empty())notes={n};x4::enabled=enabled;x4::begin_frame(6160,s.context.frame,std::string(64,'a'));
        track_legacy_batch(s,notes,std::vector<std::optional<NoteCandidate>>(notes.size()),tracks,next);
        return x4::drain();
    }
    void overlap_history() {
        n.center.x=935;horizontal.center.y=500;s.lines={horizontal,vertical};bind();run();
        s.targets.clear();s.context.capture_ns+=20'000'000;s.context.frame++;s.sequence++;
        vertical.observed_ns=horizontal.observed_ns=s.context.capture_ns;s.lines={horizontal,vertical};bind();run();
        ASSERT_EQ(s.targets[0].samples,2);ASSERT_GE(s.targets[0].history_span_ns,10'000'000);
    }
};
TEST_F(X4Oracle, DefaultOffPreservesOriginalTrackingAndOwnerActions) {
    ASSERT_FALSE(x4::enabled);run(false);const auto a=decision_json(s);EXPECT_EQ(s.targets[0].line_id,10);
    FakeClock clock;clock.set(s.context.capture_ns);x1::ReplayTouch touch(clock);GamePlanOwner owner(clock,touch,5,{15,35'000'000,30'000'000});owner.accept(s);owner.poll();EXPECT_TRUE(touch.contacts.empty());
    s.targets.clear();tracks.clear();next=0;run(false);EXPECT_EQ(decision_json(s),a);
}
TEST_F(X4Oracle, OnlyProvisionalWinnerChangesAndScoresRemainOriginal) {
    const auto r=run();ASSERT_EQ(r.size(),1);EXPECT_EQ(r[0]["original_winner"],10);EXPECT_EQ(r[0]["override_winner"],20);EXPECT_TRUE(r[0]["applied"].get<bool>());
    EXPECT_NEAR(r[0]["best_score"].get<double>(),26.6,1e-9);EXPECT_EQ(s.targets[0].line_id,20);EXPECT_EQ(tracks[0].confirmed_line_id,0);EXPECT_FALSE(s.targets[0].crossing_ns);
}
TEST_F(X4Oracle, OutsideWindowAndNonTargetNeverIntervene) {
    x4::enabled=true;x4::begin_frame(6157,s.context.frame,std::string(64,'a'));track_legacy_batch(s,{n},{std::nullopt},tracks,next);EXPECT_EQ(s.targets[0].line_id,10);EXPECT_TRUE(x4::drain().empty());
    s.targets.clear();tracks.clear();next=0;n.kind=NoteKind::hold;bind();run();EXPECT_FALSE(x4::choices.size());EXPECT_EQ(x4::applied_count,0);
}
TEST_F(X4Oracle, ConfirmedWinnerOverrideRemainsOriginal) {
    GameTrackHistory h;h.id=1;h.kind=n.kind;h.last=n.center;h.observed=980'000'000;h.appearance=n;h.confirmed_line_id=10;h.revision=3;
    for(int i=0;i<3;++i){auto l=horizontal;l.observed_ns=940'000'000+i*20'000'000;h.points.push_back({l.observed_ns,n.center,l,{}});}tracks={h};next=1;
    const auto r=run();ASSERT_EQ(r.size(),1);EXPECT_EQ(r[0]["reason"],"already_confirmed");EXPECT_EQ(r[0]["preserve_flag"],true);EXPECT_EQ(s.targets[0].line_id,10);
}
TEST_F(X4Oracle, MissingInvalidStaleAndNonuniqueCandidatesFailClosed) {
    for(int mode=0;mode<4;++mode) {
        s.targets.clear();tracks.clear();next=0;s.lines={horizontal,vertical};
        if(mode==0)s.lines.pop_back();if(mode==1)s.lines.back().association_valid=false;if(mode==2)--s.lines.back().observed_ns;if(mode==3)s.lines.push_back(vertical);
        const auto r=run();ASSERT_EQ(r.size(),1);EXPECT_EQ(r[0]["applied"],false);EXPECT_EQ(x4::applied_count,0);
    }
}
TEST_F(X4Oracle, SourceFrameAndPngSHABindingRejectBeforeTracking) {
    x4::enabled=true;EXPECT_THROW(x4::begin_frame(6160,s.context.frame+1,std::string(64,'a')),std::runtime_error);
    EXPECT_THROW(x4::begin_frame(6160,s.context.frame,std::string(64,'b')),std::runtime_error);
}
TEST_F(X4Oracle, CoreAndLineGeometryMismatchNeverCreateSupport) {
    x4::packets[6160]["core"]["x"]=22;run();EXPECT_EQ(x4::applied_count,0);
    s.targets.clear();tracks.clear();next=0;bind();x4::packets[6160]["line"]["x"]=939;const auto r=run();EXPECT_EQ(r[0]["reason"],"line_geometry_missing");EXPECT_EQ(s.targets[0].line_id,10);
}
TEST_F(X4Oracle, OriginalLengthAndExtentQualificationRemainNecessary) {
    for(int mode=0;mode<2;++mode) {
        s.targets.clear();tracks.clear();next=0;
        if(mode==0)vertical.length=200;else {vertical.length=711;vertical.center.y=1200;}s.lines={horizontal,vertical};bind();
        const auto r=run();EXPECT_EQ(r[0]["applied"],false);EXPECT_EQ(s.targets[0].line_id,10);
    }
}
TEST_F(X4Oracle, RelationAmbiguityRejectsOracleWithoutRescoring) {
    auto competitor=horizontal;competitor.center.y=269;competitor.track_id=30;s.lines.push_back(competitor);
    const auto r=run();EXPECT_EQ(r[0]["applied"],true);EXPECT_EQ(r[0]["relation_ambiguous"],true);EXPECT_EQ(s.targets[0].line_id,0);EXPECT_EQ(s.targets[0].samples,0);
}
TEST_F(X4Oracle, IdentityAmbiguityRejectsOracleWithoutIdentityRepair) {
    GameTrackHistory h;h.id=1;h.kind=n.kind;h.last=n.center;h.appearance=n;h.observed=980'000'000;h.revision=2;tracks={h,h};tracks[1].id=2;next=2;
    const auto r=run();ASSERT_EQ(r.size(),1);EXPECT_EQ(r[0]["applied"],true);EXPECT_EQ(r[0]["identity_ambiguous"],true);EXPECT_EQ(s.targets[0].line_id,0);EXPECT_EQ(s.targets[0].samples,0);
}
TEST_F(X4Oracle, LateAlignmentAndLineChasingNoteAreNotExcludedByOracle) {
    n.tangent={1,0};bind();const auto r=run();EXPECT_EQ(r[0]["applied"],true);EXPECT_EQ(s.targets[0].line_id,20);
    for(int i=1;i<=3;++i){s.targets.clear();s.context.capture_ns+=20'000'000;s.context.frame++;s.sequence++;vertical.center.x-=20;vertical.observed_ns=s.context.capture_ns;horizontal.observed_ns=s.context.capture_ns;s.lines={horizontal,vertical};bind();run();EXPECT_EQ(s.targets[0].line_id,20);}
    EXPECT_EQ(tracks[0].confirmed_line_id,20);EXPECT_GE(s.targets[0].samples,3);
}
TEST_F(X4Oracle, SameDirectionNeighborAndFragmentStayBoundToCurrentGeometry) {
    auto neighbor=vertical;neighbor.center.x+=50;neighbor.track_id=30;s.lines.push_back(neighbor);const auto r=run();EXPECT_EQ(r[0]["override_winner"],20);
    s.targets.clear();tracks.clear();next=0;vertical.track_id=900;s.lines={horizontal,vertical};bind();run();EXPECT_EQ(s.targets[0].line_id,900);
}
TEST_F(X4Oracle, CompletedOwnerCannotResurrectAfterNewProvisionalChoice) {
    overlap_history();
    FakeClock clock;clock.set(s.context.capture_ns);x1::ReplayTouch touch(clock);std::size_t downs=0;touch.sink=[&](json e){if(e.at("event")=="fake_receipt"&&e.at("command").at("phase")==0)++downs;};
    GamePlanOwner owner(clock,touch,5,{15,35'000'000,30'000'000});owner.accept(s);owner.poll();ASSERT_EQ(downs,1);
    clock.set(s.context.capture_ns+101'000'000);owner.poll();EXPECT_TRUE(touch.contacts.empty());
    s.targets.clear();s.context.capture_ns=clock.now_ns();s.context.frame++;s.sequence++;vertical.observed_ns=horizontal.observed_ns=clock.now_ns();s.lines={horizontal,vertical};bind();run();owner.accept(s);owner.poll();EXPECT_EQ(downs,1);
}
TEST_F(X4Oracle, UnknownDownNeverRetriesThroughHookAndOwner) {
    overlap_history();
    FakeClock clock;clock.set(s.context.capture_ns);x1::ReplayTouch touch(clock);std::size_t unknown_downs=0;
    touch.sink=[&](json e){if(e.at("event")=="fake_receipt"&&e.at("command").at("phase")==0&&e.at("success")==false)++unknown_downs;};
    GamePlanOwner owner(clock,touch,5,{15,35'000'000,30'000'000});owner.accept(s);touch.fail_at=touch.count;owner.poll();ASSERT_EQ(unknown_downs,1);const auto count=touch.count;owner.poll();EXPECT_EQ(touch.count,count);EXPECT_TRUE(touch.contacts.empty());
}
TEST_F(X4Oracle, AbsenceCannotManufactureCurrentLineOrDown) {
    s.lines.clear();const auto r=run();EXPECT_EQ(r[0]["applied"],false);ASSERT_EQ(s.targets.size(),1);EXPECT_EQ(s.targets[0].line_id,0);EXPECT_FALSE(s.targets[0].crossing_ns);
    FakeClock clock;clock.set(s.context.capture_ns);x1::ReplayTouch touch(clock);GamePlanOwner owner(clock,touch,5,{15,35'000'000,30'000'000});owner.accept(s);owner.poll();EXPECT_TRUE(touch.contacts.empty());
}
}
