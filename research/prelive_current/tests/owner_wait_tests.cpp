#include "owner_candidate_step.hpp"
#include "owner_wait.hpp"
#include "pas/candidate_admission.hpp"
#include <fstream>
#include <iostream>
using namespace pas;using namespace pas::current_rails;using namespace pas::current_execution;
using J=nlohmann::json;constexpr Nanoseconds ms=1'000'000;
struct Tests {J checks=J::array();int failed=0;void check(std::string id,bool pass){checks.push_back({{"id",id},{"pass",pass}});failed+=!pass;}};
DecisionSnapshot scene(NoteKind kind) {
 DecisionSnapshot s;s.sequence=1;s.context={1,1,1,1,10*ms,1280,720,1};s.ui=GameUi::playing;s.playing_gate=true;
 LineCandidate l;l.track_id=7;l.center={640,500};l.tangent={1,0};l.length=1200;l.observed_ns=10*ms;l.confidence=1;s.lines={l};
 GameTarget t;t.note_id=42;t.revision=1;t.line_id=7;t.note.kind=kind;t.note.center={400,500};t.note.tangent={1,0};t.note.width=100;t.note.height=kind==NoteKind::hold?300:8;
 if(kind==NoteKind::hold){t.note.tail=Vec2{400,200};t.note.rails_geometry=true;t.note.head_on_line=true;t.current_contact=GameTarget::CurrentContact::hold_front_rails;t.reason="current_rails_v1_measured_front_overlap";}
 else {t.reason="prediction_observe_only";}
 t.hit={400,500};t.evidence_ns=10*ms;t.expires_ns=110*ms;t.crossing_ns=10*ms;t.samples=3;t.history_span_ns=40*ms;t.uncertainty_ns=2*ms;s.targets={t};return s;
}
struct Context {
 FakeClock clock;ReplayBackend backend{clock};SessionGameOwner owner{clock,backend,5,{15,35*ms,30*ms}};OwnerCandidateStep step{clock,owner,backend.receipts()};std::vector<BackendEvent> events;
 Context(){clock.set(10*ms);step.receipt_output=[&](const auto&e){events.push_back(e);};if(!step.start(1))throw std::runtime_error("test round");}
 bool start(NoteKind kind){owner.accept(scene(kind),true);return step.poll();}
 bool idle(){return step.poll();}
};
void tap(Tests&t){Context c;t.check("actual_owner_tap_down",c.start(NoteKind::tap)&&c.backend.downs==1&&c.backend.ups==0);
 const auto due=c.owner.scheduler()->next_due_ns();t.check("actual_tap_up_deadline",due&&*due==28*ms);
 t.check("watchdog_caps_distant_phase",owner_wait_ns(c.owner.scheduler(),10*ms)==10*ms);
 c.clock.set(20*ms);t.check("wait_shortens_before_up",owner_wait_ns(c.owner.scheduler(),c.clock.now_ns())==8*ms);
 const auto before=c.events.size();c.clock.set(28*ms);t.check("due_phase_no_wait",owner_wait_ns(c.owner.scheduler(),c.clock.now_ns())==0);
 t.check("up_executes_without_new_frame",c.idle()&&c.backend.ups==1&&c.backend.active_count()==0&&c.events.size()==before+1&&c.events.back().issued.phase==Phase::up&&c.events.back().receipt.injection_start_ns==28*ms);
 t.check("idle_up_ledger_completed",c.step.ledger.find(42)&&c.step.ledger.find(42)->state==Execution::completed);
 c.clock.set(30*ms);c.owner.accept(scene(NoteKind::tap),true);t.check("stale_packet_cannot_replay_completed_down",c.idle()&&c.backend.downs==1&&c.backend.ups==1);
 t.check("tap_final_release_verified",c.step.finish());}
void expiry(Tests&t){Context c;t.check("hold_down_before_idle",c.start(NoteKind::hold)&&c.backend.downs==1&&c.backend.active_count()==1);
 c.clock.set(109*ms);t.check("wait_includes_evidence_expiry",owner_wait_ns(c.owner.scheduler(),c.clock.now_ns())==ms);
 c.clock.set(110*ms);const bool ok=c.idle();t.check("no_frame_expiry_releases_contact",ok&&c.backend.active_count()==0&&c.backend.downs==1);
 const auto* a=c.step.ledger.find(42);t.check("expiry_is_terminal",a&&(a->state==Execution::cancelled||a->state==Execution::completed));
 c.clock.set(130*ms);c.owner.accept(scene(NoteKind::hold),true);c.idle();t.check("expired_down_never_revived",c.backend.downs==1);t.check("expiry_final_release_verified",c.step.finish());}
void uncertain(Tests&t){Context c;t.check("up_fault_setup_down",c.start(NoteKind::tap));c.backend.failure=ReplayBackend::Failure::unknown_up;c.clock.set(28*ms);t.check("idle_unknown_up_blocks",!c.idle()&&c.owner.scheduler()->fault()=="input_result_unknown"&&c.backend.failed==1);
 const auto downs=c.backend.downs,ups=c.backend.ups;c.clock.set(30*ms);c.idle();t.check("unknown_up_not_retried",c.backend.downs==downs&&c.backend.ups==ups);t.check("unknown_result_sticky",c.step.blocked());
 Context d;d.backend.failure=ReplayBackend::Failure::unknown_down;t.check("unknown_down_blocks",!d.start(NoteKind::tap));const auto count=d.backend.downs;d.clock.set(40*ms);d.idle();t.check("idle_never_retries_unknown_down",d.backend.downs==count);}
int main(int argc,char**argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh output");Tests t;t.check("no_scheduler_watchdog",owner_wait_ns(nullptr,0)==10*ms);tap(t);expiry(t);uncertain(t);t.check("live_stays_unqualified",CandidateLiveAdmission{}.state()==CandidateLiveAdmission::State::unqualified);
 J r={{"schema","pas.owner-deadline-wait-tests.v1"},{"checks",t.checks},{"assertions",t.checks.size()},{"failed",t.failed},{"device_endpoints",0}};std::ofstream f(argv[1]);f<<r.dump(2)<<'\n';if(!f)throw std::runtime_error("write");std::cout<<"assertions="<<t.checks.size()<<" failed="<<t.failed<<'\n';return t.failed?1:0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
