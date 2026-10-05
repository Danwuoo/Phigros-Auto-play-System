#include "pas/action_wake.hpp"
#include "pas/session_archive.hpp"
#include "pas/session_events.hpp"
#include "pas/game_session.hpp"
#include "meter_touch.hpp"
#include "gate_contract.hpp"
#include "archive_verify.hpp"
#include <gtest/gtest.h>
#include <cstdlib>
#include <fstream>
using namespace pas;using nlohmann::json;
namespace {
constexpr Nanoseconds ms=1'000'000;
std::filesystem::path output(const std::string& name) {
 const auto* root=std::getenv("R1_TEST_ROOT");if(!root)throw std::runtime_error("R1_TEST_ROOT required");
 auto p=std::filesystem::path(root)/name;if(std::filesystem::exists(p))throw std::runtime_error("new test output required");return p;
}
DecisionSnapshot scene(std::uint64_t seq,Nanoseconds now) {DecisionSnapshot s;s.sequence=seq;s.context={1,1,1,seq,now,1280,720,1};s.playing_gate=true;s.ui=GameUi::playing;return s;}
GameTarget note(NoteKind kind,Nanoseconds now,Nanoseconds due) {
 GameTarget t;t.note_id=t.revision=1;t.evidence_ns=now;t.expires_ns=now+100*ms;t.note.kind=kind;
 t.note.center=t.hit={400,500};t.note.width=120;t.note.height=300;t.crossing_ns=due;t.reason="prediction_observe_only";t.uncertainty_ns=2*ms;t.samples=4;t.line_id=7;
 t.note.rails_geometry=t.note.head_on_line=true;return t;
}
json fixture() {
 auto m=[](int n){return json{{"n",n},{"p50",1},{"p95",1},{"p99",1},{"max",1},{"jitter_p95_minus_p5",0}};};
 return {{"attempts",10},{"published",10},{"pool_drops",0},{"consumed",10},{"owner_seen",10},{"receipt_n",1},{"release_n",1},{"action_expectation","required"},{"hard_gate",true},{"raw_complete",true},
 {"metrics_ms",{{"recognition",m(10)},{"owner",m(10)},{"capture_to_owner",m(10)},{"lateness",m(1)}}}};
}
struct FakeWake {FakeClock& clock;bool pending=false;void notify(){pending=true;}void wait(Nanoseconds delay){if(delay<=0)return;if(pending)pending=false;else clock.set(clock.now_ns()+delay);}};
}
TEST(R1Gate, EmptyRequiredUnknownAndDenominatorMismatchReject) {
 auto r=fixture();EXPECT_TRUE(r1::normal(r));r["metrics_ms"]["lateness"]["n"]=0;EXPECT_FALSE(r1::normal(r));
 r=fixture();r["metrics_ms"]["owner"]["n"]=9;EXPECT_FALSE(r1::normal(r));
 r=fixture();r["metrics_ms"]["owner"]["p99"]=nullptr;EXPECT_FALSE(r1::normal(r));
 r=fixture();r["raw_complete"]=false;EXPECT_FALSE(r1::normal(r));
 r=fixture();r.erase("release_n");EXPECT_FALSE(r1::normal(r));
 r=fixture();r["metrics_ms"]["capture_to_owner"]["p99"]=101;EXPECT_FALSE(r1::normal(r));
}
TEST(R1Gate, ExplicitNoActionIsNotApplicableButEmptyRecognitionStillFails) {
 auto r=fixture();r["action_expectation"]="none";r["receipt_n"]=0;r["metrics_ms"]["lateness"]={{"n",0}};EXPECT_TRUE(r1::normal(r));
 r["metrics_ms"]["recognition"]["n"]=0;EXPECT_FALSE(r1::normal(r));r=fixture();r["action_expectation"]="unknown";EXPECT_FALSE(r1::normal(r));
}
TEST(R1Archive, FullRowsAndCompletionMissingRowReject) {
 auto root=output("full");HostClock c;SessionArchive a(root,{{"offline",true}},1024,256,{&c});
 a.event(1,{{"event","test"},{"steps",json::array({{{"phase",0},{"x",400},{"y",500},{"due_ns",35*ms}}})}});a.complete(1,{{"status","complete"}});a.close();
 ASSERT_FALSE(a.faulted())<<a.error();auto st=a.meter_stats();EXPECT_EQ(st.written,1u);EXPECT_EQ(st.admitted,2u);EXPECT_EQ(st.serialize_ms.size(),1u);EXPECT_EQ(st.write_ms.size(),1u);
 EXPECT_TRUE(r1::verify_archive(root,{{"test",1}}));EXPECT_FALSE(r1::verify_archive(root,{{"test",2}}));
 std::ofstream f(root/"round-1/events-0.jsonl",std::ios::trunc);f.close();EXPECT_FALSE(r1::verify_archive(root,{{"test",1}}));EXPECT_THROW(a.event(1,{{"event","after_close"}}),std::runtime_error);
}
TEST(R1Archive, FileOpenWriteFailureAndPartialCloseNeverVerify) {
 auto root=output("disk_failure");SessionArchive a(root,{{"offline",true}});
 std::filesystem::create_directories(root/"round-1/events-0.jsonl");a.event(1,{{"event","test"}});a.close();EXPECT_TRUE(a.faulted());EXPECT_FALSE(r1::verify_archive(root,{{"test",1}}));
 root=output("partial");SessionArchive b(root,{{"offline",true}});b.event(1,{{"event","test"}});b.close();EXPECT_FALSE(b.faulted());EXPECT_FALSE(r1::verify_archive(root,{{"test",1}}));
}
TEST(R1Archive, ScaledMailboxItemAndByteBoundsFaultAndClose) {
 for(int mode=0;mode<2;++mode){auto root=output("mailbox"+std::to_string(mode));SessionArchive a(root,{{"offline",true}},1024,256,{nullptr,20,mode?8192u:2u,mode?300u:16u*1024u*1024u});
  bool rejected=false;for(int i=0;i<20;++i){try{a.event(1,{{"event","test"},{"padding",std::string(100,'x')}});}catch(const std::runtime_error&){rejected=true;break;}}
  EXPECT_TRUE(rejected);a.close();EXPECT_TRUE(a.faulted());EXPECT_EQ(a.error(),"session archive mailbox overrun");EXPECT_LE(a.peak_queue(),mode?8192u:2u);
 }
}
TEST(R1Archive, ThirtyTwoSegmentsHardBoundRejectsThirtyThird) {
 auto root=output("segments");SessionArchive a(root,{{"offline",true}},256,256);
 for(int i=0;i<34;++i){try{a.event(1,{{"event","test"},{"n",i},{"padding",std::string(100,'x')}});}catch(const std::runtime_error&){break;}}
 a.close();EXPECT_TRUE(a.faulted());EXPECT_EQ(a.error(),"round journal 32-segment limit reached");
 EXPECT_TRUE(std::filesystem::is_regular_file(root/"round-1/events-31.jsonl"));EXPECT_FALSE(std::filesystem::exists(root/"round-1/events-32.jsonl"));
}
TEST(R1Wake, OsPreNotificationTimeoutAndNonpositiveDelay) {
 HostClock c;Wake w;const auto start=c.now_ns();w.notify();w.wait(200*ms);const auto notify_end=c.now_ns();
 EXPECT_LT(notify_end-start,200*ms);w.wait(2*ms);EXPECT_GE(c.now_ns()-notify_end,2*ms-100);w.wait(0);w.wait(-1);
 // QPC durations are observed OS data; no FakeClock latency claim.
 RecordProperty("qpc_frequency",std::to_string(c.frequency()));RecordProperty("notify_ns",std::to_string(notify_end-start));RecordProperty("timeout_and_tail_ns",std::to_string(c.now_ns()-notify_end));
}
TEST(R1Wake, FakeNotificationTimeoutDueGateAndFinishWithLiveLead) {
 FakeClock c;FakeWake w{c};r1::MeterTouch t(c,0);SessionGameOwner g(c,t,5,{15,35*ms,30*ms});g.start(1);
 auto s=scene(1,0);s.targets={note(NoteKind::hold,0,85*ms)};g.accept(s,true);auto p=g.owner()->take_accepted_plans();ASSERT_EQ(p.size(),1u);const auto due=p[0].steps.front().due_ns;EXPECT_EQ(due,50*ms);
 w.notify();w.wait(action_wait_delay(g.scheduler()->next_due_ns(),c.now_ns()));EXPECT_EQ(c.now_ns(),0);
 c.set(due-1);g.poll();EXPECT_TRUE(t.receipts.empty());w.wait(1);EXPECT_EQ(c.now_ns(),due);g.poll();ASSERT_EQ(t.receipts.size(),1u);
 c.set(due+1);g.poll();EXPECT_EQ(t.receipts.size(),1u);auto gate=scene(2,c.now_ns());g.accept(gate,false);EXPECT_TRUE(t.contacts.empty());
 g.finish();const auto calls=t.release_calls.size();g.finish();EXPECT_EQ(t.release_calls.size(),calls);g.start(2);g.finish();EXPECT_GT(t.release_calls.size(),calls);
}
TEST(R1Release, FailedAndUnknownReleasePersistDenominatorsAndNeverRetryDown) {
 for(bool unknown:{false,true}) {FakeClock c;r1::MeterTouch t(c,0);SessionGameOwner g(c,t,5,{15,35*ms,30*ms});g.start(1);
  auto s=scene(1,0);s.targets={note(NoteKind::hold,0,45*ms)};g.accept(s,true);c.set(10*ms);g.poll();ASSERT_EQ(t.receipts.size(),1u);
  t.fail_release=!unknown;t.unknown_release=unknown;auto s2=scene(2,20*ms);c.set(20*ms);g.accept(s2,false);
  ASSERT_FALSE(t.release_calls.empty());const auto& r=t.release_calls.back();EXPECT_EQ(r.requested_ids.size(),1u);EXPECT_EQ(r.failed_ids.size(),unknown?0u:1u);EXPECT_EQ(r.unknown_ids.size(),unknown?1u:0u);
  c.set(30*ms);s=scene(3,c.now_ns());s.targets={note(NoteKind::hold,c.now_ns(),70*ms)};g.accept(s,true);g.poll();EXPECT_EQ(t.receipts.size(),1u);EXPECT_EQ(g.scheduler()->fault(),"release_failed");EXPECT_THROW(g.finish(),std::runtime_error);
  t.fail_release=t.unknown_release=false;t.release_all();EXPECT_TRUE(t.contacts.empty());
 }
}
TEST(R1Release, UnknownDownAndCompletedIntentCannotResurrect) {
 for(bool unknown:{false,true}) {FakeClock c;r1::MeterTouch t(c,0);t.unknown_down=unknown;GamePlanOwner g(c,t,5,{15,35*ms,30*ms});
  auto s=scene(1,0);s.targets={note(NoteKind::tap,0,45*ms)};g.accept(s);c.set(10*ms);g.poll();ASSERT_EQ(t.receipts.size(),1u);c.set(30*ms);g.poll();
  auto before=t.receipts.size();c.set(40*ms);s=scene(2,c.now_ns());s.targets={note(NoteKind::tap,c.now_ns(),80*ms)};g.accept(s);c.set(60*ms);g.poll();EXPECT_EQ(t.receipts.size(),before);g.stop();EXPECT_TRUE(t.contacts.empty());
 }
}
