#pragma once
#include "current.hpp"
#include "pas/game_session.hpp"
#include "pas/game_tracking.hpp"
namespace pas::current_rails {
// The integration host is an actual observer -> lifecycle -> pre-dispatch
// filter -> sole SessionGameOwner/scheduler. It owns no capture/device API.
class Runtime {
 const Clock& clock_;
public:
 current_execution::ReplayBackend backend;
 GameObserver observer;
 PlaySessionLifecycle lifecycle;
 SessionGameOwner owner;
 current_execution::ExecutionLedger ledger;
 Hook hook;
 std::unique_ptr<bvi::Candidate> baseline=std::make_unique<bvi::Candidate>();
 std::size_t raw=0,accepted=0,unknown=0,terminals=0,bodies=0;
 Nanoseconds observer_compute=0,hook_compute=0,owner_compute=0;
 bool owner_processed=false;
 Evaluation last;
 std::function<void(const current_execution::BackendEvent&)> receipt_output;
 std::function<void(nlohmann::json)> event_output;
 ReleaseReport final_release;
 explicit Runtime(const Clock& c):clock_(c),backend(c),observer(c),owner(c,backend,5,{15,35'000'000,30'000'000}){}
 bool sync(){const auto events=backend.take_events();if(receipt_output)for(const auto& e:events)receipt_output(e);if(!owner.owner())return events.empty();
  auto plans=owner.owner()->take_accepted_plans();auto coverage=owner.owner()->take_coverage_updates();auto cancellations=owner.owner()->take_plan_cancellations();
  if(event_output){for(const auto&p:plans){nlohmann::json steps=nlohmann::json::array();for(const auto&v:p.steps)steps.push_back({{"phase",int(v.phase)},{"x",v.x},{"y",v.y},{"due",v.due_ns}});event_output({{"event","own_accepted_plan"},{"epoch",p.epoch},{"generation",p.generation},{"geometry",p.geometry_version},{"note",p.note_id},{"intent",p.intent_id},{"revision",p.revision},{"source_frame",p.source_frame_sequence},{"evidence",p.evidence_ns},{"valid_until",p.valid_until_ns},{"prefix_offset",p.prefix_offset},{"basis",p.basis},{"steps",steps}});}for(auto&v:coverage)event_output(std::move(v));for(auto&v:cancellations)event_output(std::move(v));}
  return !backend.overflow()&&ledger.observe(plans,events,*owner.scheduler())&&owner.scheduler()->fault().empty();}
 bool dispatch(const Frame& f,bool candidate=true){
  owner_processed=false;observer_compute=hook_compute=owner_compute=0;
  owner.poll();if(!sync())return false;
  HostClock meter;auto t0=meter.now_ns();auto s=observer.process(f);observer_compute=meter.now_ns()-t0;const auto ui=session_ui_pixels(f,s);
  const auto status=lifecycle.observe(s.context,ui,s.playing_gate,clock_.now_ns());
  if(status.ended&&owner.owner()){if(!finish())return false;}
  if(status.new_round){owner.start(status.round);ledger=current_execution::ExecutionLedger{};baseline=std::make_unique<bvi::Candidate>();observer.reset();hook.reset();s.playing_gate=false;s.targets.clear();s.lines.clear();}
  const bool allow=status.state==PlaySessionState::playing&&status.active&&s.playing_gate&&s.capacity_valid&&!ui.result;
  const auto& batch=observer.candidate_batch();raw+=batch.candidates.size();
  if(event_output){event_output({{"event","current_observer_scene"},{"scene",decision_json(s)}});event_output({{"event","complete_raw_candidate_batch"},{"batch",candidate_batch_json(batch)}});}
  t0=meter.now_ns();DecisionSnapshot filtered;
  if(candidate){last=hook.evaluate(f,batch,s,ledger,clock_.now_ns());accepted+=last.accepted;unknown+=last.unknown;
   for(std::size_t k=0;k<last.count;++k){terminals+=last.support[k].terminal;bodies+=last.support[k].body;}
   filtered=last.filtered;
  }else{const auto e=current_execution::evaluate(*baseline,f,batch,s,ledger,clock_.now_ns());accepted+=e.allowed;filtered=e.filtered;if(e.input.valid&&ledger.valid())for(const auto&t:s.targets)if((t.note.kind==NoteKind::drag||t.note.kind==NoteKind::flick)&&ledger.permits_current_target(t.note_id)){filtered.targets.push_back(t);++accepted;}}
  hook_compute=meter.now_ns()-t0;if(event_output)event_output({{"event","filtered_owner_scene"},{"allow",allow},{"scene",decision_json(filtered)}});t0=meter.now_ns();owner.accept(filtered,allow);owner_processed=bool(owner.owner());owner.poll();const bool ok=sync();owner_compute=meter.now_ns()-t0;return ok;
 }
 bool finish(){if(!owner.owner())return backend.active_count()==0;
  if(!owner.scheduler()->fault().empty()){final_release=owner.scheduler()->last_release();return ledger.finalize(final_release)&&backend.active_count()==0;}
  final_release=owner.finish();const auto events=backend.take_events();if(receipt_output)for(const auto&e:events)receipt_output(e);return ledger.finalize(final_release)&&backend.active_count()==0;}
};
}
