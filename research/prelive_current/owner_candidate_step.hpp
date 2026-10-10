#pragma once
#include "current.hpp"
#include "pas/game_session.hpp"
#include "pas/game_tracking.hpp"
#include "pas/diagnostic_snapshot.hpp"
#include "diagnostic_phase_profile.hpp"

namespace pas::current_rails {
// Endpoint-free action seam. References the existing sole owner and its bounded
// receipt stream; owns no observer, lifecycle, capture or backend.
class OwnerCandidateStep final {
 const Clock& clock_;SessionGameOwner& owner_;current_execution::ReceiptStream& receipts_;
 bool finalization_recorded_=false,finalization_verified_=false;
 std::optional<nlohmann::json> first_failure_;
 std::uint64_t accepting_frame_=0;
 Nanoseconds accepting_capture_=0;
 bool fail(std::string_view where,std::string_view cause) {
  if(!first_failure_)first_failure_=nlohmann::json{{"where",where},{"cause",cause.substr(0,512)},
   {"qpc_ns",clock_.now_ns()},{"source_frame",accepting_frame_},{"capture_complete_ns",accepting_capture_},
   {"ledger_valid",ledger.valid()},{"ledger_reason",ledger.reason()},{"receipt_stream_healthy",receipts_.healthy()},
   {"scheduler_fault",owner_.scheduler()?owner_.scheduler()->fault().substr(0,512):""},
   {"finalization_recorded",finalization_recorded_},{"finalization_verified",finalization_verified_}};
  return false;
 }
 std::string_view failure_cause() const {
  if(owner_.scheduler()&&!owner_.scheduler()->fault().empty())return "scheduler_fault";
  if(!ledger.valid())return ledger.reason();
  if(!receipts_.healthy())return "receipt_stream_unhealthy";
  if(finalization_recorded_&&!finalization_verified_)return "first_release_unverified";
  return "owner_step_failed_without_recorded_cause";
 }
public:
 current_execution::ExecutionLedger ledger;
 Hook hook;
 std::unique_ptr<bvi::Candidate> baseline=std::make_unique<bvi::Candidate>();
 std::size_t raw=0,accepted=0,unknown=0,terminals=0,bodies=0;
 Nanoseconds hook_compute=0,owner_compute=0;
 bool owner_processed=false;
 Evaluation last;
 std::function<void(const current_execution::BackendEvent&)> receipt_output;
 std::function<void(nlohmann::json)> event_output;
 std::function<void(DiagnosticSnapshot)> snapshot_output;
 ReleaseReport final_release;
 std::string_view evaluation_outcome="not_evaluated";
 std::uint64_t checkpoint_calls=0;
 Nanoseconds checkpoint_max_gap=0;
 diagnostic_phase::Profile diagnostic_profile;
 OwnerCandidateStep(const Clock& c,SessionGameOwner& owner,current_execution::ReceiptStream& stream)
     :clock_(c),owner_(owner),receipts_(stream) {}
 OwnerCandidateStep(const OwnerCandidateStep&)=delete;
 OwnerCandidateStep& operator=(const OwnerCandidateStep&)=delete;
 bool finalization_recorded() const {return finalization_recorded_;}
 bool finalization_verified() const {return finalization_verified_;}
 nlohmann::json first_failure() const {return first_failure_?*first_failure_:nlohmann::json(nullptr);}
 std::string stop_reason() const {return first_failure_?first_failure_->at("cause").get<std::string>():std::string(failure_cause());}
 bool blocked() const {return first_failure_.has_value()||(finalization_recorded_&&!finalization_verified_)||!receipts_.healthy()||!ledger.valid();}
 bool sync() {
  const auto events=receipts_.take_events();
  if(!owner_.owner()) {if(receipt_output)for(const auto& e:events)receipt_output(e);return events.empty()&&receipts_.healthy();}
  auto plans=owner_.owner()->take_accepted_plans();auto coverage=owner_.owner()->take_coverage_updates();auto cancellations=owner_.owner()->take_plan_cancellations();
  const bool observed=ledger.observe(plans,events,*owner_.scheduler());
  if(receipt_output)for(const auto& e:events)receipt_output(e);
  if(event_output){for(const auto&p:plans){nlohmann::json steps=nlohmann::json::array();for(const auto&v:p.steps)steps.push_back({{"phase",int(v.phase)},{"x",v.x},{"y",v.y},{"due",v.due_ns}});event_output({{"event","own_accepted_plan"},{"epoch",p.epoch},{"generation",p.generation},{"geometry",p.geometry_version},{"note",p.note_id},{"intent",p.intent_id},{"revision",p.revision},{"source_frame",p.source_frame_sequence},{"evidence",p.evidence_ns},{"valid_until",p.valid_until_ns},{"prefix_offset",p.prefix_offset},{"basis",p.basis},{"steps",steps}});}for(auto&v:coverage)event_output(std::move(v));for(auto&v:cancellations)event_output(std::move(v));}
  // Observe even on overflow so every retained receipt is joined and uncertainty
  // cannot be hidden by the diagnostic fault.
  return receipts_.healthy()&&observed&&owner_.scheduler()->fault().empty();
 }
 bool poll() {if(blocked())return fail("poll_blocked",failure_cause());owner_.poll();return sync()||fail("poll_sync",failure_cause());}
 bool start(std::uint64_t round) {
  if(blocked()||owner_.owner()||!receipts_.reset_after_verified_round())return false;
  owner_.start(round);finalization_recorded_=finalization_verified_=false;final_release={};
  ledger=current_execution::ExecutionLedger{};baseline=std::make_unique<bvi::Candidate>();hook.reset();first_failure_.reset();return true;
 }
 bool accept(const Frame& f,const CandidateBatch& batch,const DecisionSnapshot& s,bool allow,bool candidate=true) {
  owner_processed=false;hook_compute=owner_compute=0;accepting_frame_=f.sequence;accepting_capture_=f.capture_complete_ns;
  evaluation_outcome="current_evidence";checkpoint_calls=0;checkpoint_max_gap=0;
  diagnostic_profile.begin_frame(f.sequence);
  if(blocked())return fail("accept_blocked",failure_cause());
  // Linearize due/expired work before a fresh-but-delayed observation can
  // revise an old lease. All callers share this expired-first boundary.
  if(!poll())return false;
  raw+=batch.candidates.size();
  if(snapshot_output){
   std::optional<DiagnosticSnapshot> scene,raw_batch;
   diagnostic_profile.run("current_scene_snapshot_build",[&]{scene.emplace(DiagnosticSnapshot::current(s));});
   diagnostic_profile.run("current_scene_output",[&]{snapshot_output(std::move(*scene));});
   diagnostic_profile.run("candidate_batch_snapshot_build",[&]{raw_batch.emplace(DiagnosticSnapshot::batch(batch));});
   diagnostic_profile.run("candidate_batch_output",[&]{snapshot_output(std::move(*raw_batch));});
  }else if(event_output){
   nlohmann::json scene,raw_batch;
   diagnostic_profile.run("current_scene_json_build",[&]{scene={{"event","current_observer_scene"},{"scene",decision_json(s)}};});
   diagnostic_profile.run("current_scene_output",[&]{event_output(std::move(scene));});
   diagnostic_profile.run("candidate_batch_json_build",[&]{raw_batch={{"event","complete_raw_candidate_batch"},{"batch",candidate_batch_json(batch)}};});
   diagnostic_profile.run("candidate_batch_output",[&]{event_output(std::move(raw_batch));});
  }
  HostClock meter;auto t0=meter.now_ns();DecisionSnapshot filtered;
  const auto evaluate_start=clock_.now_ns();auto checkpoint_previous=evaluate_start;
  const current_execution::OwnerCheckpoint checkpoint{clock_,[this,&checkpoint_previous](){const auto qpc=clock_.now_ns();++checkpoint_calls;checkpoint_max_gap=std::max(checkpoint_max_gap,qpc-checkpoint_previous);checkpoint_previous=qpc;return poll();}};
  if(candidate){diagnostic_profile.run("hook_evaluate",[&]{last=hook.evaluate(f,batch,s,ledger,clock_.now_ns(),&checkpoint);});accepted+=last.accepted;unknown+=last.unknown;
   for(std::size_t k=0;k<last.count;++k){terminals+=last.support[k].terminal;bodies+=last.support[k].body;}filtered=last.filtered;evaluation_outcome=last.outcome;
   if(last.interrupted)return fail("candidate_evaluate",last.outcome);
  }else{bool interrupted=false;diagnostic_profile.run("hook_evaluate",[&]{const auto e=current_execution::evaluate(*baseline,f,batch,s,ledger,clock_.now_ns(),&checkpoint);evaluation_outcome=e.outcome;if(e.interrupted){interrupted=true;return;}accepted+=e.allowed;filtered=e.filtered;if(e.input.valid&&!e.source_expired&&ledger.valid())for(const auto&t:s.targets)if((t.note.kind==NoteKind::drag||t.note.kind==NoteKind::flick)&&ledger.permits_current_target(t.note_id)){filtered.targets.push_back(t);++accepted;}});if(interrupted)return fail("baseline_evaluate",evaluation_outcome);}
  if(blocked())return fail("after_evaluate",failure_cause());
  hook_compute=meter.now_ns()-t0;
  if(snapshot_output){std::optional<DiagnosticSnapshot> row;
   diagnostic_profile.run("filtered_scene_snapshot_build",[&]{row.emplace(DiagnosticSnapshot::filtered(filtered,allow));});
   diagnostic_profile.run("filtered_scene_output",[&]{snapshot_output(std::move(*row));});
  }else if(event_output){nlohmann::json row;
   diagnostic_profile.run("filtered_scene_json_build",[&]{row={{"event","filtered_owner_scene"},{"allow",allow},{"scene",decision_json(filtered)}};});
   diagnostic_profile.run("filtered_scene_output",[&]{event_output(std::move(row));});
  }
  t0=meter.now_ns();const auto commit_now=clock_.now_ns();
  if(commit_now<evaluate_start)return fail("before_commit","owner_clock_reversed");
  if(commit_now-f.capture_complete_ns>=100'000'000){evaluation_outcome="source_expired_before_commit";filtered.targets.clear();filtered.playing_gate=false;filtered.capacity_valid=false;allow=false;}
  // Every late frame is still processed as a revoke, with no new action.
  // The same owner and ledger retain completed/cancelled/unknown identities.
  bool ok=false;diagnostic_profile.run("owner_accept_poll_sync",[&]{owner_.accept(filtered,allow);owner_processed=bool(owner_.owner());owner_.poll();ok=sync();});owner_compute=meter.now_ns()-t0;
  if(diagnostic_profile.enabled&&event_output)event_output({{"event","diagnostic_phase_profile"},{"profile",diagnostic_profile.json()}});
  if(event_output)event_output({{"event","owner_step_outcome"},{"source_frame",f.sequence},{"capture_complete_ns",f.capture_complete_ns},{"commit_qpc_ns",commit_now},{"outcome",evaluation_outcome},{"checkpoint_calls",checkpoint_calls},{"checkpoint_max_gap_ns",checkpoint_max_gap},{"ledger_valid",ledger.valid()},{"ledger_reason",ledger.reason()}});
  return ok||fail("accept_sync",failure_cause());
 }
 bool suspend(const std::string& reason) {
  if(blocked())return false;
  bool ok=true;try{owner_.suspend(reason);}catch(const std::exception&){ok=false;}
  return sync()&&ok;
 }
 bool finish() {
  // Freeze the first release attempt. No uncertain release retry or upgrade.
  if(finalization_recorded_)return finalization_verified_;
  if(!owner_.owner())return receipts_.active_count()==0&&receipts_.healthy();
  finalization_recorded_=true;bool owner_finished=true;
  if(receipts_.first_uncertain_release())final_release=*receipts_.first_uncertain_release();
  else if(!owner_.scheduler()->fault().empty())final_release=owner_.scheduler()->last_release();
  else try{final_release=owner_.finish();}
  catch(const std::exception&){owner_finished=false;if(receipts_.first_uncertain_release())final_release=*receipts_.first_uncertain_release();else if(owner_.scheduler())final_release=owner_.scheduler()->last_release();}
  const auto events=receipts_.take_events();
  finalization_verified_=ledger.finalize(final_release)&&receipts_.active_count()==0&&owner_finished&&receipts_.healthy();
  if(receipt_output)for(const auto&e:events)receipt_output(e);
  return finalization_verified_;
 }
};
}
