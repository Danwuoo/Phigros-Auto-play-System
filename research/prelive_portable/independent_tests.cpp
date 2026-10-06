// Independent fake-clock checks: no reuse of existing fixture builders/oracles.
// Exact production scheduler and current ExecutionLedger implementations link below.
#include "runtime.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
using namespace pas;
using namespace pas::current_execution;
using J=nlohmann::json;
constexpr Nanoseconds ms=1'000'000;
struct Checks {
 J rows=J::array();int failed=0;
 void check(std::string name,bool result){rows.push_back({{"id",name},{"pass",result}});failed+=!result;}
};
ContactPlan plan(std::uint64_t id,Nanoseconds evidence=0,Nanoseconds down=10*ms,Nanoseconds up=80*ms){
 ContactPlan p;p.epoch=1;p.generation=p.geometry_version=1;p.note_id=100+id;p.intent_id=id;p.revision=1;
 p.evidence_ns=evidence;p.valid_until_ns=down+30*ms;p.source_frame_sequence=1;p.basis="independent_portable_control";
 p.steps={{Phase::down,100.+double(id)*10,500,down},{Phase::up,100.+double(id)*10,500,up}};return p;
}
void scheduler_checks(Checks& c){
 for(int capacity=1;capacity<=5;++capacity){
  FakeClock clock;ReplayBackend backend(clock);ContactScheduler scheduler(clock,backend,capacity,128,16,350*ms,100*ms);
  scheduler.set_context(1,1,1,true,0);bool submitted=true;
  for(int i=1;i<=capacity+1;++i)submitted&=scheduler.submit(plan(i));
  clock.set(10*ms);const auto down=scheduler.run_due();const auto notices=scheduler.take_notices();
  std::set<int> ids;for(const auto& r:down)ids.insert(r.command.contact_id);
  c.check("capacity_"+std::to_string(capacity)+"_exact_contact_bound",submitted&&down.size()==std::size_t(capacity)&&ids.size()==down.size()&&backend.active_count()==down.size());
  c.check("capacity_"+std::to_string(capacity)+"_excess_conflict_retired",notices.size()==1&&notices[0].reason=="contact_conflict"&&!scheduler.submit(plan(capacity+1)));
  clock.set(80*ms);auto up=scheduler.run_due();c.check("capacity_"+std::to_string(capacity)+"_own_up_all",up.size()==down.size()&&backend.active_count()==0);
 }
 for(int delta=-1;delta<=1;++delta){
  FakeClock clock;ReplayBackend backend(clock);ContactScheduler scheduler(clock,backend,5,128,16,350*ms,100*ms);
  scheduler.set_context(1,1,1,true,0);scheduler.submit(plan(1,0,10*ms,200*ms));clock.set(10*ms);scheduler.run_due();backend.take_events();
  clock.set(90*ms);scheduler.set_context(1,1,1,true,90*ms);clock.set(100*ms+delta);auto r=scheduler.run_due();
  c.check("independent_target_expiry_boundary_"+std::to_string(delta),delta<0?(r.empty()&&backend.active_count()==1):(r.size()==1&&r[0].command.phase==Phase::up&&backend.active_count()==0));
  scheduler.cancel("test_complete");
 }
 for(int delta=-1;delta<=1;++delta){
  FakeClock clock;ReplayBackend backend(clock);ContactScheduler scheduler(clock,backend,5,128,16,350*ms,100*ms);
  scheduler.set_context(1,1,1,true,0);auto p=plan(1,0,40*ms,80*ms);p.valid_until_ns=40*ms;scheduler.submit(p);clock.set(40*ms+delta);auto r=scheduler.run_due();
  c.check("pending_deadline_no_down_"+std::to_string(delta),r.empty()&&backend.downs==0&&scheduler.pending_count()==std::size_t(delta<0));
 }
 {FakeClock clock;ReplayBackend backend(clock);ContactScheduler scheduler(clock,backend,5,128,16,350*ms,100*ms);
  scheduler.set_context(1,1,1,true,0);bool all=true;for(int i=1;i<=128;++i)all&=scheduler.submit(plan(i));
  c.check("128_pending_admitted",all&&scheduler.pending_count()==128);
  c.check("129th_pending_fail_closed",!scheduler.submit(plan(129))&&scheduler.fault()=="plan_capacity"&&!scheduler.armed()&&scheduler.pending_count()==0&&backend.downs==0);
 }
 {FakeClock clock;ReplayBackend backend(clock);ContactScheduler scheduler(clock,backend,5,128,16,350*ms,100*ms);
  scheduler.set_context(1,1,1,true,0);auto p=plan(1);scheduler.submit(p);clock.set(10*ms);scheduler.run_due();backend.take_events();
  auto revised=p;revised.revision=2;revised.evidence_ns=10*ms;revised.steps[0].x+=1;
  c.check("executed_down_immutable",!scheduler.submit(revised)&&backend.downs==1);
  revised.steps[0]=p.steps[0];revised.steps.insert(revised.steps.begin()+1,{Phase::move,180,502,20*ms});
  c.check("future_move_revision_accepted",scheduler.submit(revised));clock.set(20*ms);auto r=scheduler.run_due();
  c.check("revision_same_contact_no_down_replay",r.size()==1&&r[0].command.phase==Phase::move&&r[0].command.contact_id==0&&backend.downs==1);
  clock.set(80*ms);scheduler.run_due();revised.revision=3;revised.evidence_ns=80*ms;
  c.check("terminal_intent_revision_rejected",!scheduler.submit(revised)&&backend.downs==1&&backend.active_count()==0);
 }
 for(const auto failure:{ReplayBackend::Failure::unknown_down,ReplayBackend::Failure::unknown_move,ReplayBackend::Failure::unknown_up}){
  FakeClock clock;ReplayBackend backend(clock);ContactScheduler scheduler(clock,backend,5,128,16,350*ms,100*ms);
  scheduler.set_context(1,1,1,true,0);auto p=plan(1);p.steps.insert(p.steps.begin()+1,{Phase::move,160,500,20*ms});scheduler.submit(p);backend.failure=failure;
  clock.set(10*ms);scheduler.run_due();clock.set(20*ms);scheduler.run_due();clock.set(80*ms);scheduler.run_due();
  c.check("unknown_phase_"+std::to_string(int(failure))+"_latched_no_retry",scheduler.fault()=="input_result_unknown"&&backend.downs==1&&!scheduler.set_context(1,1,1,true,80*ms)&&!scheduler.submit(plan(2,80*ms,80*ms,90*ms)));
  c.check("unknown_phase_"+std::to_string(int(failure))+"_release_duty",backend.active_count()==0&&!scheduler.last_release().requested_ids.empty());
 }
}
void ledger_checks(Checks& c){
 // Every attacked record comes from this scheduler's accepted plan and actual
 // replay-backend receipt, then exactly one command field is corrupted.
 for(int field=0;field<7;++field){
  FakeClock clock;ReplayBackend backend(clock);ContactScheduler scheduler(clock,backend,5,128,16,350*ms,100*ms);ExecutionLedger ledger;
  scheduler.set_context(1,1,1,true,0);backend.take_events();auto p=plan(1);scheduler.submit(p);clock.set(10*ms);scheduler.run_due();auto events=backend.take_events();
  auto& r=events[0].receipt.command;
  switch(field){case 0:++r.intent_id;break;case 1:++r.contact_id;break;case 2:r.phase=Phase::move;break;case 3:r.x+=1;break;case 4:r.y+=1;break;case 5:++r.scheduled_ns;break;case 6:++r.source_frame_sequence;break;}
  c.check("receipt_field_"+std::to_string(field)+"_tamper_rejected",!ledger.observe(std::span{&p,1},events,scheduler)&&ledger.reason()=="receipt_command_mismatch");scheduler.cancel("test_release");
 }
 {FakeClock clock;ReplayBackend backend(clock);ContactScheduler scheduler(clock,backend,5,128,16,350*ms,100*ms);ExecutionLedger ledger;
  scheduler.set_context(1,1,1,true,0);backend.take_events();auto p=plan(1);scheduler.submit(p);clock.set(10*ms);scheduler.run_due();auto events=backend.take_events();
  c.check("unjoined_receipt_rejected",!ledger.observe({},events,scheduler)&&ledger.reason()=="receipt_without_accepted_plan");scheduler.cancel("test_release");
 }
 for(bool unknown:{false,true}){
  FakeClock clock;ReplayBackend backend(clock);ContactScheduler scheduler(clock,backend,5,128,16,350*ms,100*ms);ExecutionLedger ledger;
  scheduler.set_context(1,1,1,true,0);backend.take_events();auto p=plan(1);scheduler.submit(p);clock.set(10*ms);scheduler.run_due();auto events=backend.take_events();
  c.check("final_release_fixture_"+std::to_string(unknown),ledger.observe(std::span{&p,1},events,scheduler));
  auto release=backend.release_all();if(unknown)release.unknown_ids={0};else release.requested_ids.clear();
  c.check("final_release_"+std::to_string(unknown)+"_missing_or_unknown_denied",!ledger.finalize(release)&&!ledger.valid());scheduler.cancel("test_release");
 }
 {FakeClock clock;ReplayBackend backend(clock);for(int i=0;i<64;++i)backend.release_all();
  c.check("64_backend_events_within_bound",!backend.overflow());backend.release_all();
  c.check("65_backend_events_overflow_explicit",backend.overflow()&&backend.take_events().size()==64);
 }
}
void pool_and_runtime_checks(Checks& c){
 {FakeClock clock;LatestFrame latest(2,2,3,&clock);std::array<std::uint8_t,12> rgb{};Frame f;f.width=f.height=2;f.stride=6;
  std::vector<std::shared_ptr<const Frame>> leases;
  for(int i=1;i<=3;++i){f.sequence=i;rgb[0]=i;latest.publish(rgb.data(),rgb.size(),f);leases.push_back(latest.read_after(i-1,0));}
  f.sequence=4;rgb[0]=4;c.check("three_leases_force_pool_drop",!latest.publish(rgb.data(),rgb.size(),f)&&latest.counters().pool_drops==1);
  c.check("leased_pixels_never_overwritten",leases[0]->rgb[0]==1&&leases[1]->rgb[0]==2&&leases[2]->rgb[0]==3);
  leases[0].reset();c.check("released_buffer_reusable",latest.publish(rgb.data(),rgb.size(),f));latest.close();
 }
 // Native observer/lifecycle/hook/owner header is unchanged. Blank pixels must
 // not acquire playing permission; no fabricated snapshot or seeded receipt.
 {FakeClock clock;auto rt=std::make_unique<pas::current_rails::Runtime>(clock);int observer_events=0,raw_events=0,filtered_events=0;
  rt->event_output=[&](J j){const auto type=j.at("event").get<std::string>();observer_events+=type=="current_observer_scene";raw_events+=type=="complete_raw_candidate_batch";filtered_events+=type=="filtered_owner_scene";};
  Frame f;f.epoch=f.generation=f.geometry_version=1;f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;f.rgb.assign(1280*720*3,30);
  bool ok=true;for(int i=1;i<=6;++i){f.sequence=i;f.capture_complete_ns=f.pixels_ready_ns=i*20*ms;clock.set(f.pixels_ready_ns);ok&=rt->dispatch(f);}
  c.check("blank_current_runtime_no_down",ok&&rt->backend.downs==0&&!rt->owner.owner());
  c.check("blank_current_runtime_raw_diagnostic_denominator",observer_events==6&&raw_events==6&&filtered_events==6);
  c.check("blank_current_runtime_final_release",rt->finish());
 }
}
void diagnostic_contract_checks(Checks& c){
 DecisionSnapshot scene;GameTarget target;target.note_id=73;target.note.kind=NoteKind::hold;
 target.reason="current_rails_v1_measured_front_overlap";scene.targets={target};
 const auto untyped=decision_json(scene);
 scene.targets[0].current_contact=GameTarget::CurrentContact::hold_front_rails;
 const auto typed=decision_json(scene);
 c.check("decision_schema4_explicit",untyped.at("decision_schema")==4&&typed.at("decision_schema")==4);
 c.check("reason_alone_serializes_no_typed_proof",untyped.at("targets").at(0).at("current_contact")=="none");
 c.check("typed_current_contact_serializes_hold_front_rails",typed.at("targets").at(0).at("current_contact")=="hold_front_rails");
 c.check("typed_proof_distinguishable_with_identical_reason",typed.at("targets").at(0).at("reason")==untyped.at("targets").at(0).at("reason")&&typed!=untyped);
}
int main(int argc,char**argv){try{
 if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh JSON report required");
 Checks c;scheduler_checks(c);ledger_checks(c);pool_and_runtime_checks(c);diagnostic_contract_checks(c);
 J result={{"schema","pas.prelive-portable-independent.v1"},{"assertions",c.rows.size()},{"failed",c.failed},{"checks",c.rows},
  {"clock","FakeClock business; Linux steady_clock diagnostic shim only"},{"device_endpoints",0},{"physical_gold",0},{"windows_acceptance",false},{"cost_qualification",false}};
 std::ofstream out(argv[1]);out<<result.dump(2)<<'\n';if(!out)throw std::runtime_error("report write");
 std::cout<<"portable independent assertions="<<c.rows.size()<<" failed="<<c.failed<<'\n';return c.failed?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
