#include "pas/game.hpp"
#include "replay_support.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
using namespace pas;
namespace {
constexpr Nanoseconds ms=1'000'000;
DecisionSnapshot scene(std::uint64_t seq,Nanoseconds now) {
    DecisionSnapshot s;s.sequence=seq;s.context={1,1,1,seq,now,1280,720,1};
    s.playing_gate=true;s.ui=GameUi::playing;return s;
}
GameTarget note(NoteKind kind,Nanoseconds now,Nanoseconds root,std::uint64_t id=1) {
    GameTarget t;t.note_id=id;t.revision=1;t.evidence_ns=now;t.expires_ns=now+100*ms;
    t.note.kind=kind;t.note.center=t.hit={400,500};t.note.width=120;t.note.height=kind==NoteKind::hold?300:8;
    t.crossing_ns=root;t.reason="prediction_observe_only";t.uncertainty_ns=2*ms;t.samples=4;t.line_id=7;
    if(kind==NoteKind::hold)t.note.rails_geometry=t.note.head_on_line=true;
    return t;
}
int downs(const FakeTouchBackend& t){return static_cast<int>(std::count_if(t.receipts().begin(),t.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}));}
void refresh(GameTarget& t,Nanoseconds now) {t.evidence_ns=now;t.expires_ns=now+100*ms;++t.revision;}
}
TEST(X10dPending, AllKindsMissingBeforeDueWithdrawsAndFreshReturnCreatesOneNewIntent) {
 for(const auto kind:{NoteKind::tap,NoteKind::hold,NoteKind::drag,NoteKind::flick}) {
    SCOPED_TRACE(name(kind));FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});
    auto s=scene(1,0);auto n=note(kind,0,50*ms);s.targets={n};o.accept(s);
    const auto original=o.take_accepted_plans();ASSERT_EQ(original.size(),1u);ASSERT_EQ(o.scheduler().executed_steps(original[0].intent_id),0u);
    c.set(20*ms);o.accept(scene(2,c.now_ns()));o.poll();
    EXPECT_EQ(o.scheduler().pending_count(),0u);EXPECT_TRUE(t.receipts().empty());
    const auto cancel=o.take_plan_cancellations();ASSERT_EQ(cancel.size(),1u);
    EXPECT_EQ(cancel[0].at("reason"),"pending_down_current_object_missing");EXPECT_EQ(cancel[0].at("executed_steps"),0);
    EXPECT_TRUE(cancel[0].at("retry_without_prior_down"));EXPECT_FALSE(cancel[0].at("contact_started"));
    c.set(30*ms);s=scene(3,c.now_ns());refresh(n,c.now_ns());n.crossing_ns=60*ms;s.targets={n};o.accept(s);
    const auto fresh=o.take_accepted_plans();ASSERT_EQ(fresh.size(),1u);EXPECT_NE(fresh[0].intent_id,original[0].intent_id);
    EXPECT_EQ(fresh[0].source_frame_sequence,3u);const auto due=fresh[0].steps.front().due_ns;
    c.set(due-1);o.poll();EXPECT_EQ(downs(t),0);c.set(due);o.poll();ASSERT_EQ(downs(t),1);
    refresh(n,c.now_ns());s=scene(4,c.now_ns());s.targets={n};o.accept(s);o.poll();EXPECT_EQ(downs(t),1);
    o.stop();EXPECT_TRUE(t.contacts().empty());
 }
}
TEST(X10dPending, AllKindsMissingWithoutReturnNeverInjectsOldPlan) {
 for(const auto kind:{NoteKind::tap,NoteKind::hold,NoteKind::drag,NoteKind::flick}) {
    SCOPED_TRACE(name(kind));FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});
    auto s=scene(1,0);s.targets={note(kind,0,50*ms)};o.accept(s);o.take_accepted_plans();
    c.set(20*ms);o.accept(scene(2,c.now_ns()));c.set(50*ms);o.poll();EXPECT_EQ(downs(t),0);
    c.set(90*ms);o.accept(scene(3,c.now_ns()));o.poll();EXPECT_TRUE(t.receipts().empty());o.stop();
 }
}
TEST(X10dPending, NoNewSnapshotLeavesBetweenFrameDueEligible) {
 FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});
 auto s=scene(1,0);s.targets={note(NoteKind::hold,0,50*ms)};o.accept(s);
 c.set(49*ms);o.poll();EXPECT_TRUE(t.contacts().empty());c.set(50*ms);o.poll();EXPECT_EQ(downs(t),1);o.stop();
}
TEST(X10dPending, RejectedSequenceAndOlderContextAreNotAbsence) {
 for(int mode=0;mode<4;++mode){SCOPED_TRACE(mode);FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});
    auto s=scene(3,0);s.context.epoch=s.context.generation=s.context.geometry=2;s.targets={note(NoteKind::hold,0,50*ms)};o.accept(s);
    c.set(20*ms);auto old=scene(4,c.now_ns());old.context=s.context;old.context.capture_ns=c.now_ns();
    if(mode==0)old.sequence=3;if(mode==1)old.context.epoch=1;if(mode==2)old.context.generation=1;if(mode==3)old.context.geometry=1;
    o.accept(old);EXPECT_EQ(o.scheduler().pending_count(),1u);EXPECT_TRUE(o.take_plan_cancellations().empty());
    c.set(50*ms);o.poll();EXPECT_EQ(downs(t),1);o.stop();
 }
}
TEST(X10dPending, GateCapacityAndStaleSnapshotsRevokeWithoutMissingRetryClaim) {
 for(int mode=0;mode<4;++mode){SCOPED_TRACE(mode);FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});
    auto s=scene(1,0);s.targets={note(NoteKind::hold,0,50*ms)};o.accept(s);c.set(20*ms);s=scene(2,c.now_ns());
    if(mode==0)s.playing_gate=false;if(mode==1)s.capacity_valid=false;if(mode==2)s.context.capture_ns=-100*ms;if(mode==3)s.context.capture_ns=21*ms;
    o.accept(s);EXPECT_EQ(o.scheduler().pending_count(),0u);EXPECT_TRUE(o.take_plan_cancellations().empty());c.set(50*ms);o.poll();EXPECT_EQ(downs(t),0);o.stop();
 }
}
TEST(X10dPending, ReadyBeforeAtAfterDueUsesFrameFirstAndCannotRevokePastDown) {
 for(const auto ready:{49*ms,50*ms,51*ms}) {
    SCOPED_TRACE(ready);FakeClock c;FakeTouchBackend t(c);SessionGameOwner o(c,t,5,{15,0,30*ms});o.start(1);
    auto s=scene(1,0);s.targets={note(NoteKind::hold,0,50*ms)};o.accept(s,true);
    x1::advance_before_frame(o,c,ready,false,[]{});o.accept(scene(2,ready),true);o.poll();
    EXPECT_EQ(downs(t),ready>50*ms?1:0);
    if(ready<=50*ms){c.set(50*ms);o.poll();EXPECT_TRUE(t.contacts().empty());}
    else EXPECT_EQ(t.contacts().size(),1u);
    o.finish();EXPECT_TRUE(t.contacts().empty());
 }
}
TEST(X10dPending, CaptureTieWithLaterReadyCanDispatchBeforeAcceptance) {
 FakeClock c;FakeTouchBackend t(c);SessionGameOwner o(c,t,5,{15,0,30*ms});o.start(1);
 auto s=scene(1,0);s.targets={note(NoteKind::hold,0,50*ms)};o.accept(s,true);
 x1::advance_before_frame(o,c,50*ms,false,[]{});EXPECT_EQ(downs(t),0);
 x1::advance_before_frame(o,c,52*ms,false,[]{});EXPECT_EQ(downs(t),1);
 s=scene(2,50*ms);o.accept(s,true);o.poll();EXPECT_EQ(t.contacts().size(),1u);o.finish();
}
TEST(X10dPending, ActiveRootlessRotationAndMissingGraceKeepSameContactAndTrimmedCursor) {
 FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});auto n=note(NoteKind::hold,0,10*ms);
 auto s=scene(1,0);s.targets={n};o.accept(s);c.set(10*ms);o.poll();const auto finger=t.contacts().begin()->first;
 for(int k=0;k<5;++k){c.set((20+10*k)*ms);refresh(n,c.now_ns());n.crossing_ns.reset();n.note.head_on_line=false;n.note.held_body_evidence=true;
    n.note.tangent={std::cos(k*.15),std::sin(k*.15)};n.note.center=n.hit={400.0+k*8,500.0-k*4};n.reason="held_body_touch_only";
    s=scene(k+2,c.now_ns());s.targets={n};o.accept(s);o.poll();o.take_accepted_plans();ASSERT_EQ(t.contacts().size(),1u);EXPECT_EQ(t.contacts().begin()->first,finger);
 }
 const auto state=o.replay_state().at("identities").at(0);EXPECT_GT(state.at("prefix_offset").get<int>(),0);EXPECT_GT(state.at("cursor").get<int>(),state.at("prefix_offset").get<int>());
 const auto last=t.contacts().begin()->second;c.set(70*ms);o.accept(scene(7,c.now_ns()));o.poll();EXPECT_EQ(t.contacts().begin()->second,last);
 EXPECT_TRUE(o.take_plan_cancellations().empty());c.set(120*ms);o.accept(scene(8,c.now_ns()));o.poll();EXPECT_TRUE(t.contacts().empty());
 const auto canceled=o.take_plan_cancellations();ASSERT_EQ(canceled.size(),1u);EXPECT_EQ(canceled[0].at("reason"),"current_object_missing_or_region_lost");EXPECT_FALSE(canceled[0].at("retry_without_prior_down"));
 c.set(125*ms);refresh(n,c.now_ns());n.note.held_body_evidence=false;n.crossing_ns=140*ms;n.reason="prediction_observe_only";s=scene(9,c.now_ns());s.targets={n};o.accept(s);c.set(140*ms);o.poll();EXPECT_EQ(downs(t),1);o.stop();
}
TEST(X10dPending, CompletedAndExternallyMissingCursorRetainRetirement) {
 for(bool completed:{false,true}){SCOPED_TRACE(completed);FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});
    auto n=note(NoteKind::tap,0,30*ms);auto s=scene(1,0);s.targets={n};o.accept(s);const auto plans=o.take_accepted_plans();ASSERT_EQ(plans.size(),1u);
    if(completed){c.set(30*ms);o.poll();c.set(48*ms);o.poll();EXPECT_EQ(downs(t),1);}else{o.scheduler().cancel_intent(plans[0].intent_id);c.set(20*ms);}
    ASSERT_FALSE(o.scheduler().executed_steps(plans[0].intent_id));o.accept(scene(2,c.now_ns()));
    c.set(55*ms);refresh(n,c.now_ns());n.crossing_ns=70*ms;s=scene(3,c.now_ns());s.targets={n};o.accept(s);EXPECT_TRUE(o.take_accepted_plans().empty());c.set(70*ms);o.poll();EXPECT_EQ(downs(t),completed?1:0);o.stop();
 }
}
TEST(X10dPending, UnknownDownMissingAndFreshReturnNeverRetries) {
 FakeClock c;x1::ReplayTouch t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});std::vector<x1::json> receipts;
 t.sink=[&](auto j){if(j.at("event")=="fake_receipt")receipts.push_back(j);};
 auto n=note(NoteKind::hold,0,30*ms);auto s=scene(1,0);s.targets={n};o.accept(s);const auto plans=o.take_accepted_plans();t.fail_at=t.count;
 c.set(30*ms);o.poll();ASSERT_EQ(receipts.size(),1u);EXPECT_FALSE(receipts[0].at("success"));EXPECT_EQ(o.scheduler().fault(),"input_result_unknown");
 EXPECT_FALSE(o.scheduler().executed_steps(plans[0].intent_id));c.set(40*ms);o.accept(scene(2,c.now_ns()));
 c.set(50*ms);refresh(n,c.now_ns());n.crossing_ns=70*ms;s=scene(3,c.now_ns());s.targets={n};o.accept(s);c.set(70*ms);o.poll();EXPECT_EQ(receipts.size(),1u);EXPECT_TRUE(o.take_accepted_plans().empty());o.stop();EXPECT_TRUE(t.contacts.empty());
}
TEST(X10dPending, EpochGeometryAndSessionResetDoNotCarryPendingIntentAcrossBoundary) {
 for(int mode=0;mode<4;++mode){SCOPED_TRACE(mode);FakeClock c;FakeTouchBackend t(c);SessionGameOwner o(c,t,5,{15,0,30*ms});o.start(1);
    auto s=scene(1,0);s.targets={note(NoteKind::hold,0,50*ms)};o.accept(s,true);c.set(20*ms);s=scene(2,c.now_ns());
    if(mode==0)s.context.epoch=2;if(mode==1)s.context.geometry=2;if(mode==2)s.context.generation=2;
    if(mode==3){o.finish();o.start(2);}o.accept(s,true);c.set(50*ms);o.poll();EXPECT_EQ(downs(t),0);o.finish();EXPECT_TRUE(t.contacts().empty());
 }
}
TEST(X10dPending, ActiveHoldAliasPreservesFingerWhileIndependentPendingTapCancels) {
 FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});auto h=note(NoteKind::hold,0,10*ms);
 auto s=scene(1,0);s.targets={h};o.accept(s);c.set(10*ms);o.poll();ASSERT_EQ(t.contacts().size(),1u);const auto finger=t.contacts().begin()->first;
 c.set(20*ms);refresh(h,c.now_ns());h.crossing_ns.reset();h.reason="held_body_touch_only";h.note.held_body_evidence=true;
 s=scene(2,c.now_ns());s.targets={h,note(NoteKind::tap,c.now_ns(),50*ms,2)};o.accept(s);o.poll();o.take_accepted_plans();
 c.set(30*ms);refresh(h,c.now_ns());h.note_id=3;h.hit.x+=8;h.note.center=h.hit;s=scene(3,c.now_ns());s.targets={h};o.accept(s);o.poll();
 const auto cancel=o.take_plan_cancellations();ASSERT_EQ(cancel.size(),1u);EXPECT_EQ(cancel[0].at("note_id"),2);EXPECT_EQ(cancel[0].at("reason"),"pending_down_current_object_missing");
 ASSERT_EQ(t.contacts().size(),1u);EXPECT_EQ(t.contacts().begin()->first,finger);EXPECT_EQ(o.replay_state().at("aliases").size(),1u);
 c.set(50*ms);o.poll();EXPECT_EQ(downs(t),1);EXPECT_EQ(t.contacts().size(),1u);o.stop();
}
TEST(X10dPending, ActiveSharedDragSuccessorSurvivesIndependentPendingHoldCancellation) {
 FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});auto d=note(NoteKind::drag,0,15*ms);
 auto s=scene(1,0);s.targets={d};o.accept(s);o.poll();ASSERT_EQ(t.contacts().size(),1u);const auto finger=t.contacts().begin()->first;
 c.set(10*ms);refresh(d,c.now_ns());s=scene(2,c.now_ns());s.targets={d,note(NoteKind::hold,c.now_ns(),60*ms,2)};o.accept(s);o.poll();o.take_accepted_plans();
 c.set(20*ms);auto successor=note(NoteKind::drag,c.now_ns(),40*ms,3);successor.hit.x+=4;successor.note.center=successor.hit;
 s=scene(3,c.now_ns());s.targets={successor};o.accept(s);o.poll();
 const auto cancel=o.take_plan_cancellations();ASSERT_EQ(cancel.size(),1u);EXPECT_EQ(cancel[0].at("note_id"),2);EXPECT_EQ(cancel[0].at("reason"),"pending_down_current_object_missing");
 ASSERT_EQ(t.contacts().size(),1u);EXPECT_EQ(t.contacts().begin()->first,finger);EXPECT_EQ(downs(t),1);EXPECT_FALSE(o.take_coverage_updates().empty());o.stop();
}
