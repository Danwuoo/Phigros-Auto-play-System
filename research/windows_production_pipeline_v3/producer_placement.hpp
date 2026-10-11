#pragma once
#include "producer_wait.hpp"
#include "worker_affinity.hpp"
#include <exception>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <nlohmann/json.hpp>

// Cold synthetic-capture orchestration only. No formal runtime includes this
// header. The caller is already the capture worker; this creates no threads and
// changes no process, consumer or writer policy. OS facts never select a song.
namespace cold::producer_placement {
namespace rails=pas::current_rails;
enum class Failure : std::uint8_t {
 none,placement,affinity_enter,priority_query,priority_thread,priority_enter,
 priority_applied_readback,body_exception,priority_restore,
 priority_restored_readback,affinity_restore,process_class_after_close
};
inline const char* failure_name(Failure value) noexcept {
 switch(value){
 case Failure::none:return "none";
 case Failure::placement:return "placement";
 case Failure::affinity_enter:return "affinity_enter";
 case Failure::priority_query:return "priority_query";
 case Failure::priority_thread:return "priority_thread";
 case Failure::priority_enter:return "priority_enter";
 case Failure::priority_applied_readback:return "priority_applied_readback";
 case Failure::body_exception:return "body_exception";
 case Failure::priority_restore:return "priority_restore";
 case Failure::priority_restored_readback:return "priority_restored_readback";
 case Failure::affinity_restore:return "affinity_restore";
 case Failure::process_class_after_close:return "process_class_after_close";
 }
 return "unknown";
}
struct Snapshot {
 rails::PlacementPlan placement{};
 rails::WorkerAffinitySnapshot affinity{};
 int priority_previous=std::numeric_limits<int>::max();
 int priority_applied=std::numeric_limits<int>::max();
 int priority_restored=std::numeric_limits<int>::max();
 std::uint32_t priority_thread=0;
 std::uint32_t process_class_after_close=0;
 bool priority_previous_available=false,priority_constructor_attempted=false;
 bool priority_applied_available=false,priority_close_attempted=false;
 bool priority_restored_available=false,priority_explicit_restored=false;
 bool priority_rollback_attempted=false,priority_rollback_restored=false;
 bool body_entered=false,body_completed=false;
 Failure failure=Failure::none,cleanup_failure=Failure::none;
 bool verified() const noexcept {
  return failure==Failure::none&&cleanup_failure==Failure::none&&placement.valid()&&affinity.verified()&&
   priority_previous_available&&priority_constructor_attempted&&priority_applied_available&&
   priority_close_attempted&&priority_restored_available&&priority_explicit_restored&&
   priority_applied==rails::producer_priority_requested&&priority_restored==priority_previous&&
   priority_thread==affinity.thread_id&&process_class_after_close==0x20&&body_entered&&body_completed;
 }
};
static_assert(std::is_trivially_copyable_v<Snapshot> && sizeof(Snapshot)<=512,
              "cold producer scheduling snapshot is fixed scalar storage");

// Shared by the real fixture and injectable controls. A body exception remains
// a failure after successful explicit cleanup. Both scopes are cleaned even if
// the first cleanup fails; destructors are fallback only and cannot grant PASS.
template<class AffinityApi,class PriorityApi,class Body>
void run(AffinityApi& affinity_api,PriorityApi& priority_api,const rails::PlacementPlan& plan,
         Snapshot& snapshot,Body&& body){
 snapshot={};snapshot.placement=plan;
 std::optional<rails::WorkerAffinityScope<AffinityApi>> affinity;
 std::optional<rails::ProducerPriorityScope<PriorityApi>> priority;
 std::exception_ptr first_error;
 Failure phase=Failure::placement;
 try {
  if(!plan.valid())throw rails::WorkerAffinityError(plan.failure==rails::WorkerAffinityFailure::none?
      rails::WorkerAffinityFailure::topology_incomplete:plan.failure);
  phase=Failure::affinity_enter;affinity.emplace(affinity_api,snapshot.affinity,plan.producer_mask);
  phase=Failure::priority_query;snapshot.priority_previous=priority_api.current();
  if(snapshot.priority_previous==std::numeric_limits<int>::max())throw std::runtime_error("cold producer priority query");
  snapshot.priority_previous_available=true;
  phase=Failure::priority_thread;snapshot.priority_thread=priority_api.thread();
  if(!snapshot.priority_thread||snapshot.priority_thread!=snapshot.affinity.thread_id)
   throw std::runtime_error("cold producer priority/affinity thread");
  phase=Failure::priority_enter;snapshot.priority_constructor_attempted=true;priority.emplace(priority_api);
  phase=Failure::priority_applied_readback;snapshot.priority_applied=priority_api.current();
  snapshot.priority_applied_available=snapshot.priority_applied!=std::numeric_limits<int>::max();
  if(!snapshot.priority_applied_available||snapshot.priority_applied!=rails::producer_priority_requested)
   throw std::runtime_error("cold producer actual HIGHEST readback");
  phase=Failure::body_exception;snapshot.body_entered=true;
  std::forward<Body>(body)();snapshot.body_completed=true;
 }catch(...){snapshot.failure=phase;first_error=std::current_exception();}
 const auto cleanup_failed=[&](Failure reason){
  if(snapshot.cleanup_failure==Failure::none)snapshot.cleanup_failure=reason;
  if(!first_error)first_error=std::current_exception();
 };
 if(priority){
  snapshot.priority_close_attempted=true;
  try {
   if(priority_api.thread()!=snapshot.priority_thread)throw std::runtime_error("cold producer priority restore thread");
   priority->close();snapshot.priority_explicit_restored=true;
  }catch(...){cleanup_failed(Failure::priority_restore);}
  if(snapshot.priority_explicit_restored){
   try {
    snapshot.priority_restored=priority_api.current();
    snapshot.priority_restored_available=snapshot.priority_restored!=std::numeric_limits<int>::max();
    if(!snapshot.priority_restored_available||snapshot.priority_restored!=snapshot.priority_previous)
     throw std::runtime_error("cold producer exact original priority restore readback");
   }catch(...){cleanup_failed(Failure::priority_restored_readback);}
  }
 }else if(snapshot.priority_constructor_attempted&&snapshot.priority_previous_available){
  // A failed legacy priority constructor has no live scope. Explicitly check
  // its rollback against the previously queried value; failure stays sticky.
  snapshot.priority_rollback_attempted=true;
  try {
   if(priority_api.thread()!=snapshot.priority_thread||!priority_api.set(snapshot.priority_previous))
    throw std::runtime_error("cold producer failed-constructor priority rollback");
   snapshot.priority_restored=priority_api.current();
   snapshot.priority_restored_available=snapshot.priority_restored!=std::numeric_limits<int>::max();
   if(!snapshot.priority_restored_available||snapshot.priority_restored!=snapshot.priority_previous)
    throw std::runtime_error("cold producer failed-constructor rollback readback");
   snapshot.priority_rollback_restored=true;
  }catch(...){cleanup_failed(Failure::priority_restore);}
 }
 if(affinity){try{affinity->close();}catch(...){cleanup_failed(Failure::affinity_restore);}}
 // Trigger any necessary unverified destructor fallback before returning the
 // snapshot to a caller, so failed cleanup facts are never read prematurely.
 priority.reset();affinity.reset();
 snapshot.process_class_after_close=affinity_api.process_class();
 if(snapshot.process_class_after_close!=0x20){
  try{throw std::runtime_error("cold producer process must remain NORMAL after scope cleanup");}
  catch(...){cleanup_failed(Failure::process_class_after_close);}
 }
 if(first_error)std::rethrow_exception(first_error);
 if(!snapshot.verified())throw std::runtime_error("cold producer scopes not explicitly verified");
}

inline nlohmann::json affinity_json(const rails::WorkerAffinitySnapshot& s){
 return {{"verified",s.verified()},{"thread_id",s.thread_id},{"active_groups",s.active_groups},
  {"process_class",s.process_class},{"process_mask",s.process_mask},{"system_mask",s.system_mask},
  {"previous_group",s.previous.group},{"previous_mask",s.previous.mask},
  {"requested_group",s.requested.group},{"requested_mask",s.requested.mask},
  {"applied_group",s.applied_readback.group},{"applied_mask",s.applied_readback.mask},
  {"restored_group",s.restored_readback.group},{"restored_mask",s.restored_readback.mask},
  {"previous_available",s.previous_available},{"applied_available",s.applied_available},
  {"restored_available",s.restored_available},{"set_succeeded",s.set_succeeded},
  {"applied",s.applied},{"close_attempted",s.close_attempted},{"restored",s.restored},
  {"rollback_attempted",s.rollback_attempted},{"rollback_restored",s.rollback_restored},
  {"rollback_available",s.rollback_available},{"rollback_group",s.rollback_readback.group},
  {"rollback_mask",s.rollback_readback.mask},{"fallback_attempted",s.fallback_attempted},
  {"fallback_available",s.fallback_available},{"fallback_group",s.fallback_readback.group},
  {"fallback_mask",s.fallback_readback.mask},{"failure",rails::worker_affinity_failure_name(s.failure)},
  {"rollback_failure",rails::worker_affinity_failure_name(s.rollback_failure)},
  {"fallback_failure",rails::worker_affinity_failure_name(s.fallback_failure)}};
}
inline nlohmann::json plan_json(const rails::PlacementPlan& p){
 return {{"schema","pas.cold-worker-placement.v1"},{"valid",p.valid()},
  {"failure",rails::worker_affinity_failure_name(p.failure)},{"active_groups",p.active_groups},
  {"process_class",p.process_class},{"group",p.group},{"process_mask",p.process_mask},
  {"system_mask",p.system_mask},{"consumer_mask",p.consumer_mask},{"producer_mask",p.producer_mask},
  {"consumer_core",p.consumer_core},{"producer_core",p.producer_core},
  {"consumer_number",p.consumer_number},{"producer_number",p.producer_number},
  {"efficiency_class",p.efficiency_class},{"topology_bytes",p.topology_bytes},
  {"topology_error",p.topology_error},{"topology_cores",p.topology_cores},
  {"topology_logical_processors",p.topology_logical_processors},
  {"policy","producer only; same AA highest raw EfficiencyClass last-two-core plan; producer higher lowest LP; consumer mask reserved, not applied; writer already created"},
  {"cost_qualification",false},{"device_operations",0}};
}
inline nlohmann::json priority_json(const Snapshot& s){
 return {{"schema","pas.cold-producer-priority-snapshot.v1"},{"verified",s.verified()},
  {"requested",rails::producer_priority_requested},{"thread_id",s.priority_thread},
  {"process_class_after_close",s.process_class_after_close},
  {"previous",s.priority_previous},{"applied",s.priority_applied},{"restored",s.priority_restored},
  {"previous_available",s.priority_previous_available},{"constructor_attempted",s.priority_constructor_attempted},
  {"applied_available",s.priority_applied_available},{"close_attempted",s.priority_close_attempted},
  {"restored_available",s.priority_restored_available},{"explicit_restored",s.priority_explicit_restored},
  {"rollback_attempted",s.priority_rollback_attempted},{"rollback_restored",s.priority_rollback_restored},
  {"body_entered",s.body_entered},{"body_completed",s.body_completed},
  {"failure",failure_name(s.failure)},{"cleanup_failure",failure_name(s.cleanup_failure)}};
}
} // namespace cold::producer_placement
