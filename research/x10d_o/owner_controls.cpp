#include "pas/game.hpp"
#include "replay_support.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
using namespace pas;
namespace {
constexpr Nanoseconds ms=1'000'000;
DecisionSnapshot scene(std::uint64_t seq,Nanoseconds now){DecisionSnapshot s;s.sequence=seq;s.context={1,1,1,seq,now,1280,720,1};s.playing_gate=true;s.ui=GameUi::playing;return s;}
GameTarget hold(Nanoseconds now,Nanoseconds due,std::uint64_t id=1){GameTarget n;n.note_id=id;n.revision=1;n.evidence_ns=now;n.expires_ns=now+100*ms;n.note.kind=NoteKind::hold;n.note.center=n.hit={400,500};n.note.width=120;n.note.height=300;n.note.rails_geometry=n.note.head_on_line=true;n.samples=4;n.line_id=7;n.crossing_ns=due;n.uncertainty_ns=2*ms;n.reason="prediction_observe_only";return n;}
void refresh(GameTarget& n,Nanoseconds now){n.evidence_ns=now;n.expires_ns=now+100*ms;++n.revision;}
int phase(const FakeTouchBackend& t,Phase p){return static_cast<int>(std::count_if(t.receipts().begin(),t.receipts().end(),[&](const auto& r){return r.command.phase==p;}));}
}
TEST(OwnerControl, ActiveRootlessCurrentBodyMovesSameFingerWhileLineRotates){
 FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});auto n=hold(0,10*ms);auto s=scene(1,0);s.targets={n};o.accept(s);c.set(10*ms);o.poll();ASSERT_EQ(t.contacts().size(),1u);const auto finger=t.contacts().begin()->first;
 for(int k=0;k<8;++k){c.set((20+7*k)*ms);refresh(n,c.now_ns());n.crossing_ns.reset();n.note.head_on_line=false;n.note.held_body_evidence=true;n.note.tangent={std::cos(.05*k),std::sin(.05*k)};n.note.center=n.hit={400.0+6*k,500.0-3*k};n.reason="held_body_touch_only";s=scene(k+2,c.now_ns());s.lines={LineCandidate{{600,550},{std::cos(.08*k),std::sin(.08*k)},1280,4,.85}};s.targets={n};o.accept(s);o.poll();ASSERT_EQ(t.contacts().size(),1u);EXPECT_EQ(t.contacts().begin()->first,finger);EXPECT_EQ(t.contacts().begin()->second[0],n.hit.x);EXPECT_EQ(t.contacts().begin()->second[1],n.hit.y);}
 EXPECT_EQ(phase(t,Phase::down),1);EXPECT_GT(phase(t,Phase::move),0);const auto state=o.replay_state().at("identities").at(0);EXPECT_GT(state.at("prefix_offset").get<int>(),0);EXPECT_GT(state.at("cursor").get<int>(),0);o.stop();EXPECT_TRUE(t.contacts().empty());
}
TEST(OwnerControl, ShortAbsenceDoesNotMoveAndLongGapReleasesWithoutReturnReplay){
 FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});auto n=hold(0,10*ms);auto s=scene(1,0);s.targets={n};o.accept(s);c.set(10*ms);o.poll();auto count=t.receipts().size();c.set(20*ms);o.accept(scene(2,c.now_ns()));o.poll();EXPECT_EQ(t.receipts().size(),count);EXPECT_EQ(t.contacts().size(),1u);c.set(61*ms);o.accept(scene(3,c.now_ns()));o.poll();EXPECT_TRUE(t.contacts().empty());c.set(70*ms);refresh(n,c.now_ns());n.crossing_ns=80*ms;s=scene(4,c.now_ns());s.targets={n};o.accept(s);c.set(80*ms);o.poll();EXPECT_EQ(phase(t,Phase::down),1);o.stop();
}
TEST(OwnerControl, PendingMissingRetainsOriginalC36hGraceNoPHook){
 FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});auto s=scene(1,0);s.targets={hold(0,50*ms)};o.accept(s);c.set(20*ms);o.accept(scene(2,c.now_ns()));EXPECT_EQ(o.scheduler().pending_count(),1u);c.set(50*ms);o.poll();EXPECT_EQ(phase(t,Phase::down),1);o.stop();
}
TEST(OwnerControl, UnknownDownDoesNotRetryOnReturn){
 FakeClock c;x1::ReplayTouch t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});int downs=0;t.sink=[&](auto j){if(j.at("event")=="fake_receipt"&&j.at("command").at("phase")==static_cast<int>(Phase::down))++downs;};auto n=hold(0,30*ms);auto s=scene(1,0);s.targets={n};o.accept(s);t.fail_at=t.count;c.set(30*ms);o.poll();EXPECT_EQ(o.scheduler().fault(),"input_result_unknown");c.set(40*ms);o.accept(scene(2,c.now_ns()));c.set(50*ms);refresh(n,c.now_ns());n.crossing_ns=70*ms;s=scene(3,c.now_ns());s.targets={n};o.accept(s);c.set(70*ms);o.poll();EXPECT_EQ(downs,1);o.stop();EXPECT_TRUE(t.contacts.empty());
}
TEST(OwnerControl, CompletedTapNeverBecomesFreshDown){
 FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});auto n=hold(0,30*ms);n.note.kind=NoteKind::tap;n.note.rails_geometry=n.note.head_on_line=false;n.note.height=8;auto s=scene(1,0);s.targets={n};o.accept(s);c.set(30*ms);o.poll();c.set(48*ms);o.poll();EXPECT_TRUE(t.contacts().empty());c.set(55*ms);refresh(n,c.now_ns());n.crossing_ns=70*ms;s=scene(2,c.now_ns());s.targets={n};o.accept(s);c.set(70*ms);o.poll();EXPECT_EQ(phase(t,Phase::down),1);o.stop();
}
TEST(OwnerControl, GateEpochGeometryAndContextWithdrawPending){for(int mode=0;mode<4;++mode){FakeClock c;FakeTouchBackend t(c);SessionGameOwner o(c,t,5,{15,0,30*ms});o.start(1);auto s=scene(1,0);s.targets={hold(0,50*ms)};o.accept(s,true);c.set(20*ms);s=scene(2,c.now_ns());if(mode==0)s.playing_gate=false;if(mode==1)s.context.epoch=2;if(mode==2)s.context.geometry=2;if(mode==3)s.context.generation=2;o.accept(s,true);c.set(50*ms);o.poll();EXPECT_EQ(phase(t,Phase::down),0);o.finish();EXPECT_TRUE(t.contacts().empty());}}
TEST(OwnerControl, IndependentIncomingAndThinTapRetainNewContactEligibility){
 FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});auto h=hold(0,10*ms);auto s=scene(1,0);s.targets={h};o.accept(s);c.set(10*ms);o.poll();c.set(20*ms);refresh(h,c.now_ns());h.crossing_ns.reset();h.note.held_body_evidence=true;h.reason="held_body_touch_only";auto incoming=hold(c.now_ns(),50*ms,2);incoming.note.center.y=300;incoming.hit.y=500;incoming.note.head_on_line=false;auto tap=hold(c.now_ns(),50*ms,3);tap.note.kind=NoteKind::tap;tap.note.rails_geometry=tap.note.head_on_line=false;tap.note.height=8;tap.note.center.x=tap.hit.x=650;s=scene(2,c.now_ns());s.targets={h,incoming,tap};o.accept(s);c.set(50*ms);o.poll();EXPECT_EQ(phase(t,Phase::down),3);o.stop();EXPECT_TRUE(t.contacts().empty());
}
TEST(OwnerControl, GeometricTailPassAloneIsNotCompletionAndReturnCannotReplay){
 FakeClock c;FakeTouchBackend t(c);GamePlanOwner o(c,t,5,{15,0,30*ms});auto n=hold(0,10*ms);auto s=scene(1,0);s.targets={n};o.accept(s);c.set(10*ms);o.poll();c.set(20*ms);refresh(n,c.now_ns());n.crossing_ns.reset();n.note.head_on_line=false;n.note.held_body_evidence=true;n.note.tail={400,505};n.tail_crossing_ns=20*ms;n.reason="held_body_touch_only";s=scene(2,c.now_ns());s.targets={n};o.accept(s);o.poll();EXPECT_EQ(t.contacts().size(),1u);o.stop();c.set(30*ms);refresh(n,c.now_ns());n.crossing_ns=40*ms;s=scene(3,c.now_ns());s.targets={n};o.accept(s);c.set(40*ms);o.poll();EXPECT_EQ(phase(t,Phase::down),1);
}
