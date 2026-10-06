#pragma once
#include "owner_candidate_step.hpp"
namespace pas::current_rails {
// Research host retains its observer, lifecycle, ReplayBackend and sole owner.
// Production uses only OwnerCandidateStep with its already-existing pipeline.
class Runtime {
 const Clock& clock_;
public:
 current_execution::ReplayBackend backend;
 GameObserver observer;
 PlaySessionLifecycle lifecycle;
 SessionGameOwner owner;
 OwnerCandidateStep step;
 // Compatibility aliases all reference the shared action seam, not a second
 // ledger/hook/owner or a disconnected parallel diagnostic implementation.
 current_execution::ExecutionLedger& ledger;
 Hook& hook;
 std::unique_ptr<bvi::Candidate>& baseline;
 std::size_t &raw,&accepted,&unknown,&terminals,&bodies;
 Nanoseconds observer_compute=0;
 Nanoseconds &hook_compute,&owner_compute;
 bool& owner_processed;
 Evaluation& last;
 std::function<void(const current_execution::BackendEvent&)>& receipt_output;
 std::function<void(nlohmann::json)>& event_output;
 ReleaseReport& final_release;
 explicit Runtime(const Clock& c):clock_(c),backend(c),observer(c),owner(c,backend,5,{15,35'000'000,30'000'000}),step(c,owner,backend.receipts()),
   ledger(step.ledger),hook(step.hook),baseline(step.baseline),raw(step.raw),accepted(step.accepted),unknown(step.unknown),terminals(step.terminals),bodies(step.bodies),
   hook_compute(step.hook_compute),owner_compute(step.owner_compute),owner_processed(step.owner_processed),last(step.last),receipt_output(step.receipt_output),event_output(step.event_output),final_release(step.final_release) {}
 Runtime(const Runtime&)=delete;
 Runtime& operator=(const Runtime&)=delete;
 bool sync(){return step.sync();}
 bool dispatch(const Frame& f,bool candidate=true) {
  owner_processed=false;observer_compute=hook_compute=owner_compute=0;
  if(step.blocked()||!step.poll())return false;
  HostClock meter;const auto t0=meter.now_ns();auto s=observer.process(f);observer_compute=meter.now_ns()-t0;const auto ui=session_ui_pixels(f,s);
  const auto status=lifecycle.observe(s.context,ui,s.playing_gate,clock_.now_ns());
  if(status.ended&&owner.owner()){if(!finish())return false;}
  if(status.new_round){if(!step.start(status.round))return false;observer.reset();s.playing_gate=false;s.targets.clear();s.lines.clear();}
  const bool allow=status.state==PlaySessionState::playing&&status.active&&s.playing_gate&&s.capacity_valid&&!ui.result;
  return step.accept(f,observer.candidate_batch(),s,allow,candidate);
 }
 bool finish(){return step.finish();}
};
}
