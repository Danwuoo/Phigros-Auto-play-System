#pragma once
#include "current.hpp"
#include "pas/game_session.hpp"
#include "pas/game_tracking.hpp"

namespace pas::current_rails {
// Endpoint-free action seam. References the existing sole owner and its bounded
// receipt stream; owns no observer, lifecycle, capture or backend.
class OwnerCandidateStep final {
 const Clock& clock_;SessionGameOwner& owner_;current_execution::ReceiptStream& receipts_;
 bool finalization_recorded_=false,finalization_verified_=false;
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
 ReleaseReport final_release;
 OwnerCandidateStep(const Clock& c,SessionGameOwner& owner,current_execution::ReceiptStream& stream)
     :clock_(c),owner_(owner),receipts_(stream) {}
 OwnerCandidateStep(const OwnerCandidateStep&)=delete;
 OwnerCandidateStep& operator=(const OwnerCandidateStep&)=delete;
 bool finalization_recorded() const {return finalization_recorded_;}
 bool finalization_verified() const {return finalization_verified_;}
 bool blocked() const {return (finalization_recorded_&&!finalization_verified_)||!receipts_.healthy()||!ledger.valid();}
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
 bool poll() {if(blocked())return false;owner_.poll();return sync();}
 bool start(std::uint64_t round) {
  if(blocked()||owner_.owner()||!receipts_.reset_after_verified_round())return false;
  owner_.start(round);finalization_recorded_=finalization_verified_=false;final_release={};
  ledger=current_execution::ExecutionLedger{};baseline=std::make_unique<bvi::Candidate>();hook.reset();return true;
 }
 bool accept(const Frame& f,const CandidateBatch& batch,const DecisionSnapshot& s,bool allow,bool candidate=true) {
  owner_processed=false;hook_compute=owner_compute=0;if(blocked())return false;
  // Linearize due/expired work before a fresh-but-delayed observation can
  // revise an old lease. All callers share this expired-first boundary.
  if(!poll())return false;
  raw+=batch.candidates.size();
  if(event_output){event_output({{"event","current_observer_scene"},{"scene",decision_json(s)}});event_output({{"event","complete_raw_candidate_batch"},{"batch",candidate_batch_json(batch)}});}
  HostClock meter;auto t0=meter.now_ns();DecisionSnapshot filtered;
  const current_execution::OwnerCheckpoint checkpoint{clock_,[this](){return poll();}};
  if(candidate){last=hook.evaluate(f,batch,s,ledger,clock_.now_ns(),&checkpoint);accepted+=last.accepted;unknown+=last.unknown;
   for(std::size_t k=0;k<last.count;++k){terminals+=last.support[k].terminal;bodies+=last.support[k].body;}filtered=last.filtered;
  }else{const auto e=current_execution::evaluate(*baseline,f,batch,s,ledger,clock_.now_ns(),&checkpoint);if(e.interrupted)return false;accepted+=e.allowed;filtered=e.filtered;if(e.input.valid&&ledger.valid())for(const auto&t:s.targets)if((t.note.kind==NoteKind::drag||t.note.kind==NoteKind::flick)&&ledger.permits_current_target(t.note_id)){filtered.targets.push_back(t);++accepted;}}
  if(blocked())return false;
  hook_compute=meter.now_ns()-t0;if(event_output)event_output({{"event","filtered_owner_scene"},{"allow",allow},{"scene",decision_json(filtered)}});
  t0=meter.now_ns();owner_.accept(filtered,allow);owner_processed=bool(owner_.owner());owner_.poll();const bool ok=sync();owner_compute=meter.now_ns()-t0;return ok;
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
