#include "runtime.hpp"
#include <fstream>
#include <iostream>
using namespace pas;
using namespace pas::current_rails;
using namespace pas::current_execution;
using J=nlohmann::json;
constexpr Nanoseconds ms=1'000'000;
struct Tests{J checks=J::array(),cases=J::array();int failed=0;void check(std::string id,bool ok){checks.push_back({{"id",id},{"pass",ok}});failed+=!ok;}};
J encode(const ReleaseReport&r){return {{"requested",r.requested_ids},{"failed",r.failed_ids},{"unknown",r.unknown_ids},{"start",r.start_ns},{"return",r.return_ns}};}
bool establish_owned_down(Runtime&rt,FakeClock&clock){
 // A synthetic typed scene enters the real sole SessionGameOwner. No cursor,
 // ledger attachment or receipt is fabricated or assigned by this fixture.
 clock.set(10*ms);rt.owner.start(1);DecisionSnapshot s;s.sequence=1;s.context={1,1,1,1,10*ms,1280,720,1};s.ui=GameUi::playing;s.playing_gate=true;
 LineCandidate l;l.track_id=7;l.center={640,500};l.tangent={1,0};l.length=1200;l.observed_ns=10*ms;l.confidence=1;s.lines={l};
 GameTarget t;t.note_id=42;t.revision=1;t.line_id=7;t.note.kind=NoteKind::hold;t.note.center={400,500};t.note.tangent={1,0};t.note.width=100;t.note.height=300;t.note.tail=Vec2{400,200};t.note.rails_geometry=true;t.note.head_on_line=true;t.hit={400,500};
 t.evidence_ns=10*ms;t.expires_ns=110*ms;t.crossing_ns=10*ms;t.samples=3;t.history_span_ns=40*ms;t.uncertainty_ns=2*ms;
 t.current_contact=GameTarget::CurrentContact::hold_front_rails;t.reason="current_rails_v1_measured_front_overlap";s.targets={t};
 rt.owner.accept(s,true);rt.owner.poll();const bool synced=rt.sync();
 const auto*a=rt.ledger.find(42);return synced&&rt.backend.downs==1&&rt.backend.active_count()==1&&a&&a->state==Execution::active&&a->cursor&&*a->cursor==1;
}
void exercise(Tests&t,bool unknown,int prior){
 FakeClock clock;auto rt=std::make_unique<Runtime>(clock);std::vector<BackendEvent> emitted;
 rt->receipt_output=[&](const auto&e){emitted.push_back(e);};
 const auto label=std::string(unknown?"unknown":"known")+"_prior"+std::to_string(prior);
 t.check(label+"_own_typed_down_fixture",establish_owned_down(*rt,clock));emitted.clear();clock.set(20*ms);
 if(unknown)rt->backend.failure=ReplayBackend::Failure::release_unknown;
 if(prior){rt->owner.scheduler()->cancel("independent_preexisting_release_fault");if(prior==2){rt->sync();emitted.clear();}rt->backend.failure=ReplayBackend::Failure::none;}
 bool threw=false,verified=false;try{verified=rt->finish();}catch(const std::exception&){threw=true;}
 const auto first=encode(rt->final_release);const auto count=emitted.size();
 t.check(label+"_finish_returns_structured_result",!threw&&verified==!unknown);
 t.check(label+"_first_report_preserved",rt->final_release.requested_ids==std::vector<int>{0}&&rt->final_release.unknown_ids==(unknown?std::vector<int>{0}:std::vector<int>{})&&rt->final_release.start_ns==20*ms);
 t.check(label+"_release_event_drained_exactly_once",emitted.size()==std::size_t(prior==2?0:1)&&(emitted.empty()||(emitted[0].release&&encode(emitted[0].report)==first)));
 t.check(label+"_ledger_and_contact_fail_closed",unknown?(!rt->ledger.valid()&&rt->backend.active_count()==1):(rt->ledger.valid()&&rt->backend.active_count()==0));
 rt->backend.failure=ReplayBackend::Failure::none;clock.set(30*ms);bool again=false,second_threw=false;try{again=rt->finish();}catch(const std::exception&){second_threw=true;}
 t.check(label+"_repeat_does_not_retry_or_upgrade",!second_threw&&again==!unknown&&encode(rt->final_release)==first&&emitted.size()==count&&rt->backend.active_count()==std::size_t(unknown));
 t.check(label+"_no_undrained_release_record",rt->backend.take_events().empty());
 t.check(label+"_down_not_replayed",rt->backend.downs==1);
 t.cases.push_back({{"id",label},{"threw",threw},{"release_verified",verified},{"first_report",first},{"release_events",count},{"ledger_valid",rt->ledger.valid()},{"active_final",rt->backend.active_count()}});
}
int main(int argc,char**argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh report required");Tests t;
 exercise(t,false,0);exercise(t,true,0);exercise(t,true,1);exercise(t,true,2);
 J out={{"schema","pas.runtime-finish-evidence.contract.v1"},{"assertions",t.checks.size()},{"failed",t.failed},{"checks",t.checks},{"cases",t.cases},{"own_accepted_down_only",true},{"fixture","typed synthetic scene through actual owner, Runtime sync/finish"},{"device_endpoints",0},{"physical_gold",0}};
 std::ofstream f(argv[1]);f<<out.dump(2)<<'\n';if(!f)throw std::runtime_error("report write");std::cout<<"runtime finish assertions="<<t.checks.size()<<" failed="<<t.failed<<'\n';return t.failed?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
