#pragma once
#include "current.hpp"
#include "pas/game_session.hpp"
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
 Evaluation last;
 std::function<void(const current_execution::BackendEvent&)> receipt_output;
 explicit Runtime(const Clock& c):clock_(c),backend(c),observer(c),owner(c,backend,5,{3,35'000'000,30'000'000}){}
 bool sync(){const auto events=backend.take_events();if(receipt_output)for(const auto& e:events)receipt_output(e);if(!owner.owner())return events.empty();
  auto plans=owner.owner()->take_accepted_plans();owner.owner()->take_coverage_updates();owner.owner()->take_plan_cancellations();
  return !backend.overflow()&&ledger.observe(plans,events,*owner.scheduler());}
 bool dispatch(const Frame& f,bool candidate=true){
  owner.poll();if(!sync())return false;
  HostClock meter;auto t0=meter.now_ns();auto s=observer.process(f);observer_compute=meter.now_ns()-t0;const auto ui=session_ui_pixels(f,s);
  const auto status=lifecycle.observe(s.context,ui,s.playing_gate,clock_.now_ns());
  if(status.ended&&owner.owner()){if(!finish())return false;}
  if(status.new_round){owner.start(status.round);ledger=current_execution::ExecutionLedger{};baseline=std::make_unique<bvi::Candidate>();observer.reset();hook.reset();s.playing_gate=false;s.targets.clear();s.lines.clear();}
  const bool allow=status.state==PlaySessionState::playing&&status.active&&s.playing_gate&&s.capacity_valid&&!ui.result;
  const auto& batch=observer.candidate_batch();raw+=batch.candidates.size();
  t0=meter.now_ns();DecisionSnapshot filtered;
  if(candidate){last=hook.evaluate(f,batch,s,ledger,clock_.now_ns());accepted+=last.accepted;unknown+=last.unknown;
   for(std::size_t k=0;k<last.count;++k){terminals+=last.support[k].terminal;bodies+=last.support[k].body;}
   filtered=last.filtered;
  }else{const auto e=current_execution::evaluate(*baseline,f,batch,s,ledger,clock_.now_ns());accepted+=e.allowed;filtered=e.filtered;}
  hook_compute=meter.now_ns()-t0;t0=meter.now_ns();owner.accept(filtered,allow);owner.poll();const bool ok=sync();owner_compute=meter.now_ns()-t0;return ok;
 }
 bool finish(){if(!owner.owner())return backend.active_count()==0;
  if(!owner.scheduler()->fault().empty()){const auto& r=owner.scheduler()->last_release();return r.failed_ids.empty()&&r.unknown_ids.empty()&&backend.active_count()==0;}
  const auto r=owner.finish();backend.take_events();return r.failed_ids.empty()&&r.unknown_ids.empty()&&backend.active_count()==0;}
};
}
