// Independent bounded reviewer controls, explicitly corrected fixture v2.
// Original v1 source and its 90/91 report remain preserved. The unchanged
// sealed tail-final-release/cost_workload_family_v2.json already establishes
// no_tail_cap [0,0,0]: absence of the cap throughout approach denies entry.
// No production code, renderer, original gold, or prior report is altered.
#include "runtime.hpp"
#include "cost_workload_family_v2.hpp"
#include <fstream>
#include <iostream>
#include <set>
using namespace pas;
using namespace pas::current_rails;
using namespace pas::current_execution;
using J=nlohmann::json;
constexpr Nanoseconds ms=1'000'000;
struct Tests {
 J checks=J::array(),details=J::array(); int failed=0;
 void check(std::string id,bool pass){checks.push_back({{"id",id},{"pass",pass}});failed+=!pass;}
};
Frame fixed_frame(int phase,std::uint64_t sequence,cost_family_v2::Control control=cost_family_v2::Control::positive){
 Frame f;f.sequence=sequence;f.epoch=f.generation=f.geometry_version=1;
 f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;
 f.capture_complete_ns=f.pixels_ready_ns=1'000*ms+Nanoseconds(sequence)*20*ms;
 f.rgb=cost_family_v2::image(phase,0,control);return f;
}
J release_value(const ReleaseReport&r){return {{"requested",r.requested_ids},{"failed",r.failed_ids},{"unknown",r.unknown_ids},{"start",r.start_ns},{"return",r.return_ns}};}
bool acquire(Runtime&rt,FakeClock&clock,std::uint64_t&sequence,int&phase){
 for(phase=0;phase<cost_family_v2::frames_per_cycle;++phase){auto f=fixed_frame(phase,++sequence);clock.set(f.pixels_ready_ns);
  if(!rt.dispatch(f))return false;
  if(rt.backend.active_count())return rt.backend.downs==1&&rt.ledger.valid();
 }return false;
}
void release_stickiness(Tests&t,bool unknown,int prior){
 FakeClock clock;auto rt=std::make_unique<Runtime>(clock);std::uint64_t sequence=0;int phase=0;
 std::vector<BackendEvent> output;rt->receipt_output=[&](const BackendEvent&e){output.push_back(e);};
 const auto id=std::string(unknown?"unknown":"verified")+"_prior_"+std::to_string(prior);
 t.check(id+"_actual_RGB_owner_Down",acquire(*rt,clock,sequence,phase));output.clear();clock.set(clock.now_ns()+ms);
 if(unknown)rt->backend.failure=ReplayBackend::Failure::release_unknown;
 ReleaseReport prior_report;
 if(prior){rt->owner.scheduler()->cancel("fixed_independent_reviewer_cancel");prior_report=rt->owner.scheduler()->last_release();
  if(prior==2){rt->sync();output.clear();}rt->backend.failure=ReplayBackend::Failure::none;}
 bool threw=false,verified=false;try{verified=rt->finish();}catch(...){threw=true;}
 const auto first=release_value(rt->final_release);const auto emitted=output.size();
 t.check(id+"_structured_finish",!threw&&verified==!unknown);
 t.check(id+"_original_contact_report",rt->final_release.requested_ids==std::vector<int>{0}&&
   rt->final_release.unknown_ids==(unknown?std::vector<int>{0}:std::vector<int>{}));
 if(prior&&unknown)t.check(id+"_first_uncertain_report_exact",release_value(prior_report)==first);
 t.check(id+"_release_record_once",output.size()==std::size_t(prior==2?0:1)&&
   (output.empty()||(output[0].release&&release_value(output[0].report)==first)));
 t.check(id+"_contact_and_ledger_fail_closed",unknown?(!rt->ledger.valid()&&rt->backend.active_count()==1):
   (rt->ledger.valid()&&rt->backend.active_count()==0));
 rt->backend.failure=ReplayBackend::Failure::none;clock.set(clock.now_ns()+20*ms);
 bool second=false;try{second=rt->finish();}catch(...){threw=true;}
 t.check(id+"_second_no_retry_no_upgrade",!threw&&second==!unknown&&release_value(rt->final_release)==first&&
   output.size()==emitted&&rt->backend.downs==1&&rt->backend.active_count()==std::size_t(unknown));
 t.check(id+"_no_undrained_receipt",rt->backend.take_events().empty());
 t.details.push_back({{"id",id},{"verified",verified},{"report",first},{"output_count",emitted}});
}
void implicit_release(Tests&t,int control,bool output_enabled){
 FakeClock clock;auto rt=std::make_unique<Runtime>(clock);std::uint64_t sequence=0;int phase=0;
 std::vector<BackendEvent> output;if(output_enabled)rt->receipt_output=[&](const BackendEvent&e){output.push_back(e);};
 const auto id="implicit_"+std::to_string(control)+(output_enabled?"_output":"_no_diagnostic_output");
 t.check(id+"_actual_RGB_owner_Down",acquire(*rt,clock,sequence,phase));output.clear();
 bool ok=false;
 if(control==0){auto f=fixed_frame(phase+1,++sequence);f.source_valid=false;clock.set(f.pixels_ready_ns);ok=rt->dispatch(f);}
 if(control==1){clock.set(clock.now_ns()+100*ms);rt->owner.poll();ok=rt->sync();}
 if(control==2){rt->owner.scheduler()->set_dispatch_guard([]{return false;});rt->owner.poll();ok=rt->sync();}
 const bool has_release=std::any_of(output.begin(),output.end(),[](const auto&e){return e.release&&!e.report.requested_ids.empty();});
 t.check(id+"_ledger_consumes_implicit_release",ok&&rt->ledger.valid()&&rt->backend.active_count()==0&&rt->ledger.reservations()==0);
 t.check(id+"_diagnostics_are_not_admission",!output_enabled||has_release);
 t.check(id+"_no_receipt_left_behind",rt->backend.take_events().empty());
 t.check(id+"_finish_stays_safe",rt->finish()&&rt->backend.active_count()==0&&rt->backend.downs==1);
}
void fixed_family_controls(Tests&t){
 for(const auto control:{cost_family_v2::Control::positive,cost_family_v2::Control::absence,cost_family_v2::Control::effect,cost_family_v2::Control::no_tail_cap}){
  FakeClock clock;auto rt=std::make_unique<Runtime>(clock);std::size_t tails=0,tail_plans=0,confirmations=0,normal_ups=0;bool safe=true;
  rt->event_output=[&](J e){const auto kind=e.value("event",std::string{});
   if(kind=="own_accepted_plan"&&e.value("basis",std::string{}).find("tail_only")!=std::string::npos)++tail_plans;
   if(kind=="game_hold_tail_confirmed")++confirmations;
   if(kind=="game_contact_up"&&e.value("reason",std::string{})=="visible_tail_completed")++normal_ups;};
  for(int phase=0;phase<81;++phase){auto f=fixed_frame(phase,std::uint64_t(phase+1),control);clock.set(f.pixels_ready_ns);
   if(!rt->dispatch(f)){safe=false;break;}for(std::size_t k=0;k<rt->last.count;++k)if(rt->last.support[k].active&&rt->last.support[k].tail&&rt->last.support[k].accepted)++tails;
  }
  const auto id=std::string(cost_family_v2::name(control));
  t.check(id+"_fixed_81_phase_safe",safe&&rt->ledger.valid());
  const std::uint64_t expected_down=control==cost_family_v2::Control::no_tail_cap?0:1;
  t.check(id+"_unchanged_corpus_entry_denominator",rt->backend.downs==expected_down);
  if(control==cost_family_v2::Control::positive)t.check(id+"_normal_tail_fresh_support",tails>=2&&rt->backend.ups==1&&confirmations==1&&normal_ups==1);
  else t.check(id+"_negative_no_false_normal_tail",tail_plans==0&&confirmations==0&&normal_ups==0);
  t.check(id+"_verified_zero_active_close",rt->finish()&&rt->backend.active_count()==0);
  t.details.push_back({{"id",id},{"down",rt->backend.downs},{"move",rt->backend.moves},{"up",rt->backend.ups},{"active_tail_support",tails},{"tail_only_plans",tail_plans},{"confirmations",confirmations},{"normal_ups",normal_ups}});
 }
}
void command_mismatch(Tests&t){
 FakeClock clock;auto rt=std::make_unique<Runtime>(clock);rt->backend.failure=ReplayBackend::Failure::wrong_contact;
 bool rejected=false;for(int phase=0;phase<81;++phase){auto f=fixed_frame(phase,std::uint64_t(phase+1));clock.set(f.pixels_ready_ns);
  if(!rt->dispatch(f)){rejected=true;break;}}
 t.check("actual_receipt_command_mismatch_rejected",rejected&&!rt->ledger.valid()&&rt->ledger.reason()=="receipt_command_mismatch");
 t.check("mismatch_finish_cannot_claim_success",!rt->finish());
}
void diagnostic_exception(Tests&t,bool at_finish){FakeClock clock;auto rt=std::make_unique<Runtime>(clock);std::uint64_t sequence=0;int phase=0;
 const auto id=std::string(at_finish?"finish":"sync")+"_diagnostic_exception";
 t.check(id+"_actual_RGB_owner_Down",acquire(*rt,clock,sequence,phase));
 rt->receipt_output=[](const BackendEvent&){throw std::runtime_error("fixed_reviewer_diagnostic_output_failure");};bool threw=false;
 try{if(at_finish)rt->finish();else{rt->owner.scheduler()->cancel("independent_diagnostic_failure_control");rt->sync();}}catch(const std::exception&){threw=true;}
 t.check(id+"_output_failure_reached",threw);
 t.check(id+"_ledger_release_not_suppressed",rt->ledger.valid()&&rt->ledger.reservations()==0&&rt->backend.active_count()==0);
 const auto first=release_value(rt->final_release);rt->receipt_output={};
 t.check(id+"_finalization_survives_output_fault",rt->finish()&&rt->ledger.valid()&&rt->backend.active_count()==0);
 if(at_finish)t.check(id+"_repeat_report_not_overwritten",release_value(rt->final_release)==first&&rt->final_release.requested_ids==std::vector<int>{0});
}
void known_fault_owner_lifetime(Tests&t){FakeClock clock;auto rt=std::make_unique<Runtime>(clock);std::uint64_t sequence=0;int phase=0;
 t.check("known_scheduler_fault_actual_RGB_owner_Down",acquire(*rt,clock,sequence,phase));
 const auto epoch=rt->observer.candidate_batch().context.epoch;
 rt->owner.scheduler()->set_gate(epoch,true,clock.now_ns()+1);rt->sync();const auto original=rt->owner.scheduler()->last_release();
 t.check("known_scheduler_fault_has_verified_safety_release",!rt->owner.scheduler()->fault().empty()&&rt->backend.active_count()==0&&original.requested_ids==std::vector<int>{0});
 const bool finished=rt->finish();t.check("known_fault_finish_does_not_retry",finished&&release_value(rt->final_release)==release_value(original)&&rt->owner.owner());
 t.check("verified_report_cannot_reset_live_faulted_owner",!rt->step.start(2)&&rt->owner.owner()&&rt->step.finalization_recorded()&&rt->step.finalization_verified());
}
void expired_body_before_fresh_delayed_packet(Tests&t){
 // Parent-requested one fixed expiry boundary, added after the original 91
 // controls. Original source, 91-check reports, and failing order probe remain
 // preserved. Reuse the unchanged family-v2 renderer and actual owned Down.
 for(bool outer_poll:{false,true}){FakeClock clock;auto rt=std::make_unique<Runtime>(clock);std::uint64_t sequence=0;int phase=0;
  std::vector<BackendEvent> emitted;rt->receipt_output=[&](const BackendEvent&e){emitted.push_back(e);};
  const auto id=std::string(outer_poll?"runtime_outer_poll":"shared_accept_direct")+"_fixed_expiry";
  const bool owned=acquire(*rt,clock,sequence,phase);const auto evidence=clock.now_ns();const auto old_due=rt->owner.scheduler()->next_due_ns();
  auto f=fixed_frame(phase+1,++sequence);clock.set(evidence+100*ms);emitted.clear();
  t.check(id+"_actual_owned_Down_boundary_fixture",owned&&rt->backend.downs==1&&rt->backend.active_count()==1&&old_due&&*old_due==clock.now_ns()&&clock.now_ns()-f.capture_complete_ns==80*ms);
  bool ok=false;if(outer_poll)ok=rt->dispatch(f);else{auto s=rt->observer.process(f);ok=rt->step.accept(f,rt->observer.candidate_batch(),s,true);}
  const auto rows=rt->ledger.attachments();const bool verified_release=std::any_of(emitted.begin(),emitted.end(),[](const auto&e){return e.release&&e.dispatched&&e.report.requested_ids==std::vector<int>{0}&&e.report.failed_ids.empty()&&e.report.unknown_ids.empty();});
  t.check(id+"_expired_first_release_before_body_renewal",ok&&verified_release&&rt->ledger.valid()&&rt->backend.active_count()==0&&rt->last.accepted==0&&rows.size()==1&&rows[0].state==Execution::cancelled);
  t.check(id+"_no_replayed_Down_and_verified_finish",rt->backend.downs==1&&rt->finish()&&rt->backend.active_count()==0);
  t.details.push_back({{"id",id},{"prior_body_evidence",evidence},{"old_due",old_due?J(*old_due):J(nullptr)},{"packet_capture",f.capture_complete_ns},{"owner_now",clock.now_ns()},{"packet_age",clock.now_ns()-f.capture_complete_ns},{"active_final",rt->backend.active_count()},{"Down",rt->backend.downs}});
 }
}
int main(int argc,char**argv){try{
 if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh reviewer report required");
 Tests t;release_stickiness(t,false,0);for(int p=0;p<3;++p)release_stickiness(t,true,p);
 for(int control=0;control<3;++control)for(bool output:{false,true})implicit_release(t,control,output);
 fixed_family_controls(t);command_mismatch(t);diagnostic_exception(t,false);diagnostic_exception(t,true);known_fault_owner_lifetime(t);
 expired_body_before_fresh_delayed_packet(t);
 J out={{"schema","pas.independent-disabled-live-runtime-review.v2"},{"assertions",t.checks.size()},{"failed",t.failed},{"checks",t.checks},{"details",t.details},
  {"corpus","unchanged cost_family_v2 81 phases offset0"},{"ownership_fixture","actual RGB observer lifecycle Hook sole owner and backend receipts"},{"device_endpoints",0},{"physical_gold",0},{"Windows_qualification",false}};
 std::ofstream file(argv[1]);file<<out.dump(2)<<'\n';if(!file)throw std::runtime_error("report write");
 std::cout<<"independent live seam runtime assertions="<<t.checks.size()<<" failed="<<t.failed<<'\n';return t.failed?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
