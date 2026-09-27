#include "pas/game_tracking.hpp"
#include "pas/game_tracking_shadow.hpp"
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <set>

using namespace pas;
namespace {
CandidateBatch batch(std::uint64_t frame,Nanoseconds t,double y=420,double x=500){
 CandidateBatch b;b.context={1,1,1,frame,t,1280,720,1};b.ui=GameUi::playing;b.playing_gate=true;b.lines.push_back({{640,576},{1,0},1080,3,1});
 TrackingCandidate c;c.candidate_id=1;c.quality=ObservationQuality::strong_current;c.action_support=true;c.note.kind=NoteKind::hold;c.note.center={x,y};c.note.width=60;c.note.height=120;
 c.note.tail=Vec2{x,y-120};c.note.rails_geometry=c.body_visible=c.left_rail=c.right_rail=true;b.candidates.push_back(c);return b;
}
Frame pixels(std::uint64_t seq,Nanoseconds t,int y){Frame f;f.sequence=seq;f.epoch=f.generation=f.geometry_version=1;f.capture_complete_ns=t;f.width=1280;f.height=720;f.stride=3840;f.rgb.resize(1280*720*3);
 const auto rect=[&](int x,int top,int w,int h,std::array<unsigned char,3> color){for(int py=top;py<top+h;++py)for(int px=x;px<x+w;++px)std::copy(color.begin(),color.end(),f.rgb.data()+py*f.stride+px*3);};
 rect(20,20,6,22,{255,255,255});rect(34,20,6,22,{255,255,255});for(int i=0;i<6;++i)rect(1020+i*24,20,12,20,{255,255,255});rect(100,576,1080,3,{255,255,255});
 rect(470,y,60,8,{40,190,255});rect(750,y-80,60,8,{40,190,255});return f;
}
}
TEST(GameTracking, BaselinePixelSequenceAndSerializedBankAreEquivalent){
 FakeClock clock;GameObserver observer(clock);GameTrackingComparison baseline("legacy");
 for(int i=0;i<12;++i){auto f=pixels(i+1,i*16'000'000,380+i*8);clock.set(f.capture_complete_ns);const auto scene=observer.process(f);
  const auto serialized=candidate_batch_json(observer.candidate_batch());const auto b=parse_candidate_batch(serialized);EXPECT_EQ(candidate_batch_json(b),serialized);
  const auto comparison=baseline.update(b);const auto a=decision_json(scene),z=decision_json(comparison.executable);
  EXPECT_EQ(a.at("targets"),z.at("targets"));EXPECT_EQ(a.at("lines"),z.at("lines"));EXPECT_EQ(a.at("playing_gate"),z.at("playing_gate"));}
}
TEST(GameTracking, WeakNewCandidatesAndPredictionCannotCreateOrRefreshActions){
 for(const auto* method:{"byte_association","oc_observation"}){GameTrackingComparison tracker(method);auto b=batch(1,10'000'000);b.candidates[0].quality=ObservationQuality::weak_current;
  EXPECT_TRUE(tracker.update(b).observations.empty());b=batch(2,20'000'000);const auto first=tracker.update(b);ASSERT_EQ(first.observations.size(),1);const auto id=first.observations[0].track_id;
  b=batch(3,40'000'000);b.candidates[0].quality=ObservationQuality::weak_current;b.candidates[0].head_visible=false;b.candidates[0].action_support=false;
  const auto weak=tracker.update(b);ASSERT_EQ(weak.observations.size(),1);EXPECT_EQ(weak.observations[0].track_id,id);EXPECT_EQ(weak.observations[0].supported_ns,20'000'000);EXPECT_TRUE(weak.executable.targets.empty());
  b=batch(4,60'000'000);b.candidates.clear();const auto lost=tracker.update(b);ASSERT_EQ(lost.observations.size(),1);EXPECT_TRUE(lost.observations[0].prediction_only);EXPECT_EQ(lost.observations[0].observed_ns,40'000'000);EXPECT_TRUE(lost.executable.targets.empty());
  b=batch(5,140'000'000);b.candidates.clear();EXPECT_TRUE(tracker.update(b).observations.empty());}
}
TEST(GameTracking, CurrentWeakRailsRequireBothSidesFreshAnchorAndVisibleHead){
 for(const auto* method:{"byte_association","oc_observation"})for(int invalid=0;invalid<4;++invalid){GameTrackingComparison tracker(method);tracker.update(batch(1,10'000'000));auto b=batch(2,30'000'000);
  auto& c=b.candidates[0];c.quality=ObservationQuality::weak_current;c.hint_age_ns=90'000'000;c.hint_source_frame=1;c.note.head_on_line=true;
  if(invalid==1)c.right_rail=false;if(invalid==2)c.hint_age_ns=90'000'001;if(invalid==3)c.head_visible=false;
  const auto r=tracker.update(b);ASSERT_EQ(r.observations.size(),1);EXPECT_EQ(r.observations[0].action_evidence_valid,invalid==0);EXPECT_EQ(r.executable.targets.empty(),invalid!=0);}
}
TEST(GameTracking, ActualDtAndBoundedReupdateNeverInsertSyntheticEvidence){
 for(const auto* method:{"byte_association","oc_observation"}){GameTrackingComparison tracker(method,std::string(method)=="oc_observation");Nanoseconds t=10'000'000;
  for(int i=0;i<4;++i){const auto r=tracker.update(batch(i+1,t,420+t/1e9*400));ASSERT_EQ(r.executable.targets.size(),1);EXPECT_LE(r.executable.targets[0].samples,6);t+=16'000'000;}
  auto absent=batch(5,t);absent.candidates.clear();tracker.update(absent);t+=51'000'000;const auto r=tracker.update(batch(6,t,420+t/1e9*400));
  ASSERT_EQ(r.executable.targets.size(),1);EXPECT_LE(r.executable.targets[0].samples,5);EXPECT_LE(r.executable.targets[0].history_span_ns,90'000'000);EXPECT_LE(r.observations[0].synthetic_updates,6);
  if(std::string(method)=="oc_observation")EXPECT_EQ(r.observations[0].synthetic_updates,3);}
}
TEST(GameTracking, AdjacentHoldsAndThinOverlappingTapStayDistinct){
 for(const auto* method:{"byte_association","oc_observation"}){GameTrackingComparison tracker(method);std::set<std::uint64_t> ids;
  for(int i=0;i<5;++i){auto b=batch(i+1,i*20'000'000);auto second=b.candidates[0];second.candidate_id=2;second.note.center.x+=80;auto tap=b.candidates[0];tap.candidate_id=3;tap.note.kind=NoteKind::tap;tap.note.height=8;tap.note.tail.reset();b.candidates.push_back(second);b.candidates.push_back(tap);
   const auto r=tracker.update(b);ASSERT_EQ(r.observations.size(),3);std::set<std::uint64_t> current;for(const auto& o:r.observations){current.insert(o.track_id);EXPECT_TRUE(o.identity_supported);}if(i)EXPECT_EQ(current,ids);ids=current;}
  EXPECT_EQ(ids.size(),3);}
}
TEST(GameTracking, CompetingIdentityIsVisibleAndCannotBecomeNewBirth){
 for(const auto* method:{"byte_association","oc_observation"}){GameTrackingComparison tracker(method);tracker.update(batch(1,10'000'000));auto b=batch(2,26'000'000);auto duplicate=b.candidates[0];duplicate.candidate_id=2;duplicate.note.center.x+=4;b.candidates.push_back(duplicate);
  const auto r=tracker.update(b);ASSERT_EQ(r.observations.size(),1);EXPECT_EQ(r.observations[0].reason,"association_ambiguous");EXPECT_FALSE(r.observations[0].action_evidence_valid);EXPECT_TRUE(r.executable.targets.empty());}
}
TEST(GameTracking, DuplicateNewCandidatesCannotBothCreateExecutionBirths){
 for(const auto* method:{"byte_association","oc_observation"}){GameTrackingComparison tracker(method);auto b=batch(1,0);auto duplicate=b.candidates[0];duplicate.candidate_id=2;duplicate.note.center.x+=4;b.candidates.push_back(duplicate);
  const auto r=tracker.update(b);ASSERT_EQ(r.observations.size(),2);EXPECT_TRUE(r.executable.targets.empty());for(const auto& o:r.observations){EXPECT_EQ(o.track_id,0);EXPECT_EQ(o.reason,"birth_candidate_competition");EXPECT_FALSE(o.action_evidence_valid);}}
}
TEST(GameTracking, CapacityRevokeDoesNotForgetRetiredExecutionBirth){
 for(const auto* method:{"byte_association","oc_observation"}){GameTrackingComparison tracker(method);const auto first=tracker.update(batch(1,0,576));auto b=batch(2,16'000'000,576);b.capacity_valid=false;EXPECT_FALSE(tracker.update(b).capacity_valid);
  b=batch(3,32'000'000,576);b.candidates[0].note.head_on_line=true;const auto r=tracker.update(b);ASSERT_EQ(r.observations.size(),1);EXPECT_EQ(r.observations[0].birth_id,first.observations[0].birth_id);}
}
TEST(GameTracking, ShortenedHoldFrontContinuesButThinTapRemainsNew){
 for(const auto* method:{"byte_association","oc_observation"}){GameTrackingComparison tracker(method);const auto first=tracker.update(batch(1,10'000'000));auto b=batch(2,26'000'000);
  b.candidates[0].shortened_hold=b.candidates[0].note;b.candidates[0].note.kind=NoteKind::tap;b.candidates[0].note.height=24;
  const auto shorter=tracker.update(b);ASSERT_EQ(shorter.executable.targets.size(),1);EXPECT_EQ(shorter.executable.targets[0].note.kind,NoteKind::hold);EXPECT_EQ(shorter.observations[0].track_id,first.observations[0].track_id);
  b.context.frame=3;b.context.capture_ns+=16'000'000;b.candidates[0].shortened_hold.reset();b.candidates[0].note.height=8;const auto tap=tracker.update(b);ASSERT_EQ(tap.executable.targets.size(),1);EXPECT_EQ(tap.executable.targets[0].note.kind,NoteKind::tap);EXPECT_NE(tap.executable.targets[0].note_id,first.executable.targets[0].note_id);}
}
TEST(GameTracking, SixtyNinetyAndHundredMsUseActualSourceTime){
 for(const auto* method:{"byte_association","oc_observation"})for(const auto gap:{60'000'000LL,90'000'000LL,99'999'999LL,100'000'000LL,441'000'000LL}){GameTrackingComparison tracker(method);const auto first=tracker.update(batch(1,10'000'000,576));auto b=batch(2,10'000'000+gap,576);b.candidates[0].note.head_on_line=true;
  const auto r=tracker.update(b);ASSERT_EQ(r.observations.size(),1);EXPECT_EQ(r.observations[0].track_id==first.observations[0].track_id,gap<100'000'000);EXPECT_EQ(r.observations[0].birth_id,first.observations[0].birth_id);}
}
TEST(GameTracking, RetiredHoldBirthPersistsThrough441msAndRevisionIsMonotonic){
 for(const auto* method:{"byte_association","oc_observation"}){GameTrackingComparison tracker(method);const auto first=tracker.update(batch(1,10'000'000,576));const auto birth=first.observations[0].birth_id;
  auto second=tracker.update(batch(2,26'000'000,576));EXPECT_GT(second.observations[0].revision,first.observations[0].revision);
  auto b=batch(3,467'000'000,576);b.candidates[0].note.head_on_line=true;const auto restored=tracker.update(b);ASSERT_EQ(restored.observations.size(),1);EXPECT_NE(restored.observations[0].track_id,first.observations[0].track_id);EXPECT_EQ(restored.observations[0].birth_id,birth);
  EXPECT_EQ(restored.executable.targets[0].note_id,birth);b.context.geometry=2;b.context.frame=4;b.context.capture_ns+=16'000'000;const auto changed=tracker.update(b);EXPECT_NE(changed.observations[0].birth_id,birth);}
}
TEST(GameTracking, SourceInvalidAndReplayedTimeCannotArm){
 GameTrackingComparison tracker("byte_association");auto b=batch(1,10'000'000);const auto first=tracker.update(b);const auto replay=tracker.update(b);EXPECT_EQ(replay.reset_reason,"time_discontinuity");EXPECT_EQ(replay.observations[0].birth_id,first.observations[0].birth_id);
 b.context.frame=2;b.context.capture_ns+=16'000'000;b.source_valid=false;const auto invalid=tracker.update(b);EXPECT_FALSE(invalid.executable.playing_gate);EXPECT_TRUE(invalid.executable.targets.empty());
 b.context.frame=3;b.context.capture_ns+=16'000'000;b.source_valid=true;const auto restored=tracker.update(b);EXPECT_EQ(restored.observations[0].birth_id,first.observations[0].birth_id);
}
TEST(GameTracking, MissingOrPerpendicularLineCannotProvideActionEvidence){
 for(const auto* method:{"byte_association","oc_observation"})for(bool perpendicular:{false,true}){GameTrackingComparison tracker(method);auto b=batch(1,0);b.lines.clear();if(perpendicular)b.lines.push_back({{715,360},{0,1},600,3,.5});
  const auto r=tracker.update(b);ASSERT_EQ(r.observations.size(),1);EXPECT_FALSE(r.observations[0].action_evidence_valid);EXPECT_TRUE(r.executable.targets.empty());}
}
TEST(GameTracking, AssignmentIndicesAndCandidateGeometryAreRejected){
 DecisionSnapshot s;s.context={1,1,1,1,0,1280,720,1};auto b=batch(1,0);std::vector<NoteCandidate> notes{b.candidates[0].note};std::vector<std::optional<NoteCandidate>> shortened(1);std::vector<GameTrackHistory> histories(1);histories[0].kind=NoteKind::hold;std::uint64_t id=0;
 for(const auto indices:{std::vector<int>{},std::vector<int>{-1},std::vector<int>{1}})EXPECT_THROW(track_legacy_batch(s,notes,shortened,histories,id,&indices),std::invalid_argument);
 GameTrackingComparison tracker("byte_association");b.candidates[0].note.center.x=std::numeric_limits<double>::quiet_NaN();EXPECT_THROW(tracker.update(b),std::invalid_argument);
 b=batch(1,0);b.candidates.push_back(b.candidates[0]);EXPECT_THROW(tracker.update(b),std::invalid_argument);
 auto j=candidate_batch_json(batch(1,0));j["lines"][0]["thickness"]=0;EXPECT_THROW(parse_candidate_batch(j),std::invalid_argument);
}
TEST(GameTracking, TrackAndRetirementCapacityHaveConservativeFailure){
 GameTrackingComparison tracker("byte_association");auto b=batch(1,10'000'000);b.candidates.clear();
 for(int i=0;i<128;++i){auto c=batch(1,0,100+i*3.,i*9.).candidates[0];c.candidate_id=i+1;b.candidates.push_back(c);}EXPECT_TRUE(tracker.update(b).capacity_valid);
 b.context.frame=2;b.context.capture_ns+=100'000'000;for(auto& c:b.candidates)c.note.width=100;tracker.update(b);
 b.context.frame=3;b.context.capture_ns+=100'000'000;const auto overflow=tracker.update(b);EXPECT_FALSE(overflow.capacity_valid);EXPECT_EQ(overflow.reset_reason,"retirement_guard_capacity");EXPECT_TRUE(overflow.executable.targets.empty());
 b.candidates.push_back(batch(1,0).candidates[0]);EXPECT_FALSE(tracker.update(b).capacity_valid);
}
TEST(GameTracking, FakeOwnerMissingAndStopReleaseWithoutPredictedContinuation){
 FakeClock clock;FakeTouchBackend backend(clock);GamePlanOwner owner(clock,backend,2,{15,35'000'000,30'000'000});GameTrackingComparison tracker("byte_association");
 for(int i=0;i<6;++i){auto b=batch(i+1,i*16'000'000,536+i*8.);clock.set(b.context.capture_ns);const auto r=tracker.update(b);owner.accept(r.executable);owner.poll();owner.take_accepted_plans();owner.take_coverage_updates();owner.take_plan_cancellations();}
 ASSERT_FALSE(backend.contacts().empty());auto b=batch(7,156'000'000);b.candidates.clear();clock.set(b.context.capture_ns);owner.accept(tracker.update(b).executable);owner.poll();EXPECT_TRUE(backend.contacts().empty());
 owner.stop();EXPECT_TRUE(backend.contacts().empty());const auto n=backend.receipts().size();b=batch(8,172'000'000,576);b.candidates[0].note.head_on_line=true;clock.set(b.context.capture_ns);owner.accept(tracker.update(b).executable);owner.poll();EXPECT_EQ(backend.receipts().size(),n);
}
TEST(GameTracking, ShadowAndBankAreBoundedAndHaveNoBackend){
 TrackingShadow shadow("byte_association");HostClock clock;for(int i=0;i<300;++i)shadow.submit(batch(i+1,clock.now_ns()));shadow.stop();const auto report=shadow.stats();EXPECT_EQ(report.at("mailbox_capacity"),1);EXPECT_FALSE(report.at("backend_created").get<bool>());EXPECT_EQ(report.at("submitted"),report.at("processed").get<std::uint64_t>()+report.at("skipped").get<std::uint64_t>());
 TrackingBankRecorder bank;for(int i=0;i<3000;++i)bank.append(batch(i+1,i*16'000'000));const auto stats=bank.stats();EXPECT_LE(stats.at("retained_batches").get<int>(),2048);EXPECT_LE(stats.at("charged_bytes_upper").get<int>(),16*1024*1024);EXPECT_GT(stats.at("dropped_batches").get<int>(),0);
}
