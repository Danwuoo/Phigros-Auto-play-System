#pragma once
#include "pas/core.hpp"
#include <stdexcept>
#include <limits>
#include <array>
#include <type_traits>
#include <optional>
#include <exception>
#include <utility>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace pas::current_rails {
// Experimental policy for an owned cold process only. Raw ProcessPowerThrottling
// state is observable; core choice, frequency and scheduling cause are not.
inline constexpr char cold_process_qos_policy[]="own_cold_process_execution_speed_high_qos";
inline constexpr std::uint32_t cold_process_qos_version=1,cold_process_qos_execution_speed_mask=1,
 cold_process_qos_supported_mask=5;
struct ColdProcessQosState {
 std::uint32_t version=0,control_mask=0,state_mask=0;
 bool operator==(const ColdProcessQosState&)const=default;
};
struct ColdProcessQosApiResult {std::uint32_t status=0,error=0;bool sampled=false;};
enum class ColdProcessQosFailure : std::uint8_t {
 none,identity,process_class,prior_query,prior_version,prior_bits,requested_set,
 applied_query,applied_mismatch,wrong_process,wrong_thread,restore_set,
 restored_query,restored_mismatch,process_class_after_close
};
inline const char* cold_process_qos_failure_name(ColdProcessQosFailure f)noexcept{
 switch(f){
 case ColdProcessQosFailure::none:return "none";case ColdProcessQosFailure::identity:return "identity";
 case ColdProcessQosFailure::process_class:return "process_class";case ColdProcessQosFailure::prior_query:return "prior_query";
 case ColdProcessQosFailure::prior_version:return "prior_version";case ColdProcessQosFailure::prior_bits:return "prior_bits";
 case ColdProcessQosFailure::requested_set:return "requested_set";case ColdProcessQosFailure::applied_query:return "applied_query";
 case ColdProcessQosFailure::applied_mismatch:return "applied_mismatch";case ColdProcessQosFailure::wrong_process:return "wrong_process";
 case ColdProcessQosFailure::wrong_thread:return "wrong_thread";case ColdProcessQosFailure::restore_set:return "restore_set";
 case ColdProcessQosFailure::restored_query:return "restored_query";case ColdProcessQosFailure::restored_mismatch:return "restored_mismatch";
 case ColdProcessQosFailure::process_class_after_close:return "process_class_after_close";
 }return "unknown";
}
inline bool cold_process_qos_state_known(const ColdProcessQosState& s)noexcept{
 return s.version==cold_process_qos_version&&((s.control_mask|s.state_mask)&~cold_process_qos_supported_mask)==0;
}
inline ColdProcessQosState cold_process_qos_requested(ColdProcessQosState s)noexcept{
 s.control_mask|=cold_process_qos_execution_speed_mask;s.state_mask&=~cold_process_qos_execution_speed_mask;return s;
}
inline bool cold_process_qos_api_ok(const ColdProcessQosApiResult& r)noexcept{return r.sampled&&r.status!=0&&r.error==0;}
struct ColdProcessQosSnapshot {
 std::uint32_t process_id=0,owner_thread_id=0,close_process_id=0,close_thread_id=0,
  process_class_before=0,process_class_after_close=0;
 ColdProcessQosState previous,requested,applied_readback,restored_readback,rollback_readback,fallback_readback;
 ColdProcessQosApiResult query_previous,set_requested,query_applied,set_restore,query_restored,
  set_rollback,query_rollback,set_fallback,query_fallback;
 bool constructor_attempted=false,previous_available=false,requested_available=false,set_attempted=false,set_succeeded=false,
  applied_available=false,applied=false,close_attempted=false,restore_set_attempted=false,restore_set_succeeded=false,
  restored_available=false,restored=false,rollback_attempted=false,rollback_available=false,rollback_restored=false,
  fallback_attempted=false,fallback_available=false,fallback_restored=false;
 ColdProcessQosFailure failure=ColdProcessQosFailure::none,rollback_failure=ColdProcessQosFailure::none,
  fallback_failure=ColdProcessQosFailure::none;
 bool verified()const noexcept{
  return failure==ColdProcessQosFailure::none&&rollback_failure==ColdProcessQosFailure::none&&fallback_failure==ColdProcessQosFailure::none&&
   constructor_attempted&&previous_available&&requested_available&&set_attempted&&set_succeeded&&applied_available&&applied&&
   close_attempted&&restore_set_attempted&&restore_set_succeeded&&restored_available&&restored&&
   !rollback_attempted&&!rollback_available&&!rollback_restored&&!fallback_attempted&&!fallback_available&&!fallback_restored&&
   process_id!=0&&owner_thread_id!=0&&process_id==close_process_id&&owner_thread_id==close_thread_id&&
   process_class_before==0x20&&process_class_after_close==0x20&&cold_process_qos_state_known(previous)&&
   requested==cold_process_qos_requested(previous)&&applied_readback==requested&&restored_readback==previous&&
   cold_process_qos_api_ok(query_previous)&&cold_process_qos_api_ok(set_requested)&&cold_process_qos_api_ok(query_applied)&&
   cold_process_qos_api_ok(set_restore)&&cold_process_qos_api_ok(query_restored);
 }
};
static_assert(std::is_trivially_copyable_v<ColdProcessQosSnapshot>);
class ColdProcessQosError final:public std::runtime_error {
 ColdProcessQosFailure reason_;
public:
 explicit ColdProcessQosError(ColdProcessQosFailure f):std::runtime_error(cold_process_qos_failure_name(f)),reason_(f){}
 ColdProcessQosFailure reason()const noexcept{return reason_;}
};
template<class Api> class ColdProcessQosScope final {
 Api& api_;ColdProcessQosSnapshot& s_;bool active_=false;
 using F=ColdProcessQosFailure;
 bool same_identity(F& f)noexcept{
  if(api_.process()!=s_.process_id){f=F::wrong_process;return false;}
  if(api_.thread()!=s_.owner_thread_id){f=F::wrong_thread;return false;}return true;
 }
 void recover(bool fallback)noexcept{
  auto& failure=fallback?s_.fallback_failure:s_.rollback_failure;
  auto& attempted=fallback?s_.fallback_attempted:s_.rollback_attempted;
  auto& set=fallback?s_.set_fallback:s_.set_rollback;auto& query=fallback?s_.query_fallback:s_.query_rollback;
  auto& value=fallback?s_.fallback_readback:s_.rollback_readback;
  auto& available=fallback?s_.fallback_available:s_.rollback_available;
  auto& restored=fallback?s_.fallback_restored:s_.rollback_restored;
  attempted=true;if(!same_identity(failure))return;
  set=api_.set(s_.previous);query=api_.get(value);available=cold_process_qos_api_ok(query);
  if(!cold_process_qos_api_ok(set))failure=F::restore_set;
  else if(!available)failure=F::restored_query;
  else if(value!=s_.previous)failure=F::restored_mismatch;
  else {restored=true;active_=false;}
 }
 [[noreturn]] void fail(F f){
  if(s_.failure==F::none)s_.failure=f;
  if(active_){recover(false);if(active_)recover(true);}throw ColdProcessQosError(s_.failure);
 }
public:
 ColdProcessQosScope(Api& api,ColdProcessQosSnapshot& snapshot):api_(api),s_(snapshot){
  s_={};s_.constructor_attempted=true;s_.process_id=api_.process();s_.owner_thread_id=api_.thread();
  s_.process_class_before=api_.process_class();
  if(!s_.process_id||!s_.owner_thread_id)fail(F::identity);
  if(s_.process_class_before!=0x20)fail(F::process_class);
  s_.query_previous=api_.get(s_.previous);s_.previous_available=cold_process_qos_api_ok(s_.query_previous);
  if(!s_.previous_available)fail(F::prior_query);
  if(s_.previous.version!=cold_process_qos_version)fail(F::prior_version);
  if(!cold_process_qos_state_known(s_.previous))fail(F::prior_bits);
  s_.requested=cold_process_qos_requested(s_.previous);s_.requested_available=true;s_.set_attempted=true;
  active_=true; // Even a failed set may have side effects; restore the saved state.
  s_.set_requested=api_.set(s_.requested);s_.set_succeeded=cold_process_qos_api_ok(s_.set_requested);
  if(!s_.set_succeeded)fail(F::requested_set);
  s_.query_applied=api_.get(s_.applied_readback);s_.applied_available=cold_process_qos_api_ok(s_.query_applied);
  if(!s_.applied_available)fail(F::applied_query);
  if(s_.applied_readback!=s_.requested)fail(F::applied_mismatch);
  s_.applied=true;
 }
 ColdProcessQosScope(const ColdProcessQosScope&)=delete;ColdProcessQosScope& operator=(const ColdProcessQosScope&)=delete;
 ~ColdProcessQosScope(){if(active_&&!s_.fallback_attempted)recover(true);}
 void close(){
  if(!active_){if(s_.failure!=F::none)throw ColdProcessQosError(s_.failure);return;}
  s_.close_attempted=true;s_.close_process_id=api_.process();s_.close_thread_id=api_.thread();
  s_.process_class_after_close=api_.process_class();F identity=F::none;
  if(!same_identity(identity)){if(s_.failure==F::none)s_.failure=identity;throw ColdProcessQosError(s_.failure);}
  s_.restore_set_attempted=true;s_.set_restore=api_.set(s_.previous);s_.restore_set_succeeded=cold_process_qos_api_ok(s_.set_restore);
  s_.query_restored=api_.get(s_.restored_readback);s_.restored_available=cold_process_qos_api_ok(s_.query_restored);
  F failure=F::none;
  if(!s_.restore_set_succeeded)failure=F::restore_set;
  else if(!s_.restored_available)failure=F::restored_query;
  else if(s_.restored_readback!=s_.previous)failure=F::restored_mismatch;
  else {s_.restored=true;active_=false;}
  // Read priority class after the restore/query, not only before closing.
  s_.process_class_after_close=api_.process_class();
  if(failure==F::none&&s_.process_class_after_close!=0x20)failure=F::process_class_after_close;
  if(failure!=F::none&&s_.failure==F::none)s_.failure=failure;
  if(s_.failure!=F::none)throw ColdProcessQosError(s_.failure);
 }
};
// The body owns and joins its workers before explicit process-state restore.
// Its original exception survives a cleanup error, whose facts remain sticky.
template<class Api,class Body> void run_cold_process_qos(Api& api,ColdProcessQosSnapshot& s,Body&& body){
 std::exception_ptr error;
 {ColdProcessQosScope scope(api,s);try{body();}catch(...){error=std::current_exception();}
  try{scope.close();}catch(...){if(!error)error=std::current_exception();}}
 if(error)std::rethrow_exception(error);
}
inline nlohmann::json cold_process_qos_json(const ColdProcessQosSnapshot& s){
 using J=nlohmann::json;auto state=[](const ColdProcessQosState& v){return J{{"version",v.version},{"control_mask",v.control_mask},{"state_mask",v.state_mask}};};
 auto api=[](const ColdProcessQosApiResult& v){return J{{"sampled",v.sampled},{"status",v.status},{"error",v.error}};};
 return {{"schema","pas.cold-process-qos.v1"},{"policy",cold_process_qos_policy},{"version",cold_process_qos_version},
 {"execution_speed_mask",cold_process_qos_execution_speed_mask},{"supported_mask",cold_process_qos_supported_mask},
 {"process_id",s.process_id},{"owner_thread_id",s.owner_thread_id},{"close_process_id",s.close_process_id},{"close_thread_id",s.close_thread_id},
 {"process_class_before",s.process_class_before},{"process_class_after_close",s.process_class_after_close},
 {"previous",state(s.previous)},{"requested",state(s.requested)},{"applied_readback",state(s.applied_readback)},
 {"restored_readback",state(s.restored_readback)},{"rollback_readback",state(s.rollback_readback)},{"fallback_readback",state(s.fallback_readback)},
 {"query_previous",api(s.query_previous)},{"set_requested",api(s.set_requested)},{"query_applied",api(s.query_applied)},
 {"set_restore",api(s.set_restore)},{"query_restored",api(s.query_restored)},{"set_rollback",api(s.set_rollback)},
 {"query_rollback",api(s.query_rollback)},{"set_fallback",api(s.set_fallback)},{"query_fallback",api(s.query_fallback)},
 {"constructor_attempted",s.constructor_attempted},{"previous_available",s.previous_available},{"requested_available",s.requested_available},
 {"set_attempted",s.set_attempted},{"set_succeeded",s.set_succeeded},{"applied_available",s.applied_available},{"applied",s.applied},
 {"close_attempted",s.close_attempted},{"restore_set_attempted",s.restore_set_attempted},{"restore_set_succeeded",s.restore_set_succeeded},
 {"restored_available",s.restored_available},{"restored",s.restored},{"rollback_attempted",s.rollback_attempted},
 {"rollback_available",s.rollback_available},{"rollback_restored",s.rollback_restored},{"fallback_attempted",s.fallback_attempted},
 {"fallback_available",s.fallback_available},{"fallback_restored",s.fallback_restored},{"verified",s.verified()},
 {"failure",cold_process_qos_failure_name(s.failure)},{"rollback_failure",cold_process_qos_failure_name(s.rollback_failure)},
 {"fallback_failure",cold_process_qos_failure_name(s.fallback_failure)},{"device_operations",0},{"cost_qualification",false},{"live_ready",false}};
}
inline constexpr int producer_priority_requested=2;
#ifdef _WIN32
static_assert(producer_priority_requested==THREAD_PRIORITY_HIGHEST);
#endif
// Only the offline pixel producer uses this scope. Both A and B use the same
// policy; consumer, writer, process class, cadence and QPC epoch are unchanged.
template<class Api> class ProducerPriorityScope final {
 Api& api_;int previous_;std::uint32_t thread_;bool active_=false;
public:
 explicit ProducerPriorityScope(Api& api):api_(api),previous_(api.current()),thread_(api.thread()){
  if(previous_==std::numeric_limits<int>::max())throw std::runtime_error("producer priority query");
  if(!api_.set(producer_priority_requested))throw std::runtime_error("producer priority set");
  active_=true;
  if(api_.current()!=producer_priority_requested){api_.set(previous_);active_=false;throw std::runtime_error("producer priority readback");}
 }
 ProducerPriorityScope(const ProducerPriorityScope&)=delete;
 ProducerPriorityScope&operator=(const ProducerPriorityScope&)=delete;
 ~ProducerPriorityScope(){if(active_&&api_.thread()==thread_)api_.set(previous_);}
 void close(){if(!active_)return;if(api_.thread()!=thread_||!api_.set(previous_)||api_.current()!=previous_)throw std::runtime_error("producer priority restore");active_=false;}
};
// QPC absolute deadline, unchanged producer epoch/cadence. Conversion rounds
// UP to 100ns, and an early wake is rechecked against QPC rather than becoming
// a fabricated capture timestamp. No skipped attempts or phase reset.
template<class Wait>
void wait_producer_deadline(const Clock& clock, Nanoseconds deadline, Wait&& wait) {
 for(int attempt=0;attempt<4;++attempt){
  const auto now=clock.now_ns();
  if(now<0||deadline<0)throw std::runtime_error("producer QPC domain");
  if(now>=deadline)return;
  const auto remaining=deadline-now;
  if(remaining>100'000'000)throw std::runtime_error("producer wait horizon");
  wait(remaining/100+(remaining%100!=0));
 }
 if(clock.now_ns()<deadline)throw std::runtime_error("producer repeated early wake");
}
inline constexpr Nanoseconds producer_active_tail_ns=2'000'000;
inline constexpr std::uint32_t producer_active_tail_poll_cap=1u<<24;
inline constexpr std::size_t producer_wait_kernel_cap=4;
enum class ProducerWaitPhase : std::uint8_t {none,qpc,timer_arm,kernel_wait,spin,complete};
enum class ProducerWaitFailure : std::uint8_t {
 none,clock_read,clock_domain,nonmonotonic_clock,horizon,early_wake_bound,
 spin_bound,kernel_callback,pause_callback
};
struct ProducerKernelTrace {
 Nanoseconds ticks_100ns=0,arm_begin_ns=0,arm_end_ns=0,kernel_begin_ns=0,kernel_end_ns=0;
 std::uint32_t arm_status=std::numeric_limits<std::uint32_t>::max(),arm_error=0;
 std::uint32_t wait_result=std::numeric_limits<std::uint32_t>::max(),wait_error=0;
 std::uint32_t timestamp_flags=0; // bits: arm begin/end, kernel begin/end
 std::uint32_t api_flags=0; // bits: arm returned, wait returned (even failure)
};
struct ProducerWaitTrace {
 Nanoseconds due_ns=0,first_qpc_ns=0,last_qpc_ns=0,spin_enter_ns=0,spin_return_ns=0;
 std::array<ProducerKernelTrace,producer_wait_kernel_cap> kernels{};
 std::uint32_t qpc_reads=0,kernel_calls=0,spin_poll_count=0,spin_pause_calls=0;
 ProducerWaitPhase phase=ProducerWaitPhase::none,failure_phase=ProducerWaitPhase::none;
 ProducerWaitFailure failure=ProducerWaitFailure::none;
 bool entered=false,completed=false,spin_entered=false,spin_completed=false;
};
static_assert(std::is_trivially_copyable_v<ProducerWaitTrace> && sizeof(ProducerWaitTrace)<=320);
// Raw API facts, not a timing or scheduling verdict. Initial UINT32_MAX and
// actual WAIT_FAILED have the same value; api_flags distinguish whether a call
// returned, independently of a later failed diagnostic QPC read.
struct ProducerTimerResult {std::uint32_t status=0,error=0;};
template<class Api>
void producer_kernel_wait_phase(const Clock& clock,Nanoseconds ticks,Api& api,
                               ProducerKernelTrace* trace=nullptr,
                               ProducerWaitPhase* phase=nullptr){
 if(trace){trace->ticks_100ns=ticks;if(phase)*phase=ProducerWaitPhase::timer_arm;trace->arm_begin_ns=clock.now_ns();trace->timestamp_flags|=1;}
 const auto armed=api.arm(ticks);
 if(trace){trace->arm_status=armed.status;trace->arm_error=armed.error;trace->api_flags|=1;trace->arm_end_ns=clock.now_ns();trace->timestamp_flags|=2;}
 if(!armed.status)throw std::runtime_error("producer timer arm");
 if(trace){if(phase)*phase=ProducerWaitPhase::kernel_wait;trace->kernel_begin_ns=clock.now_ns();trace->timestamp_flags|=4;}
 const auto result=api.wait(250);
 if(trace){trace->wait_result=result.status;trace->wait_error=result.error;trace->api_flags|=2;trace->kernel_end_ns=clock.now_ns();trace->timestamp_flags|=8;}
 if(result.status!=0)throw std::runtime_error("producer bounded timer wait");
}
struct ProducerThreadCpuSample {
 std::uint64_t kernel_100ns=0,user_100ns=0;
 std::uint32_t status=std::numeric_limits<std::uint32_t>::max(),error=0;
 bool sampled=false;
};
inline std::optional<std::uint64_t> producer_thread_cpu_delta_100ns(
 const ProducerThreadCpuSample& before,const ProducerThreadCpuSample& after) noexcept {
 if(!before.sampled||!after.sampled||before.status==0||after.status==0||
    before.kernel_100ns>after.kernel_100ns||before.user_100ns>after.user_100ns)return {};
 const auto kernel=after.kernel_100ns-before.kernel_100ns,user=after.user_100ns-before.user_100ns;
 if(kernel>std::numeric_limits<std::uint64_t>::max()-user)return {};
 return kernel+user;
}
// Fixed, symmetric A/B candidate: sleep only outside the 2ms active tail.
// The active tail uses QPC and processor pause, with no OS sleep/yield or due
// reset. Its additional CPU cost and any actual late return remain measurable.
template<class Wait,class Pause>
void wait_producer_deadline_with_active_tail(const Clock& clock,Nanoseconds deadline,Wait&& wait,Pause&& pause,
                                            ProducerWaitTrace* trace=nullptr){
 if(trace){*trace={};trace->due_ns=deadline;trace->entered=true;}
 Nanoseconds prior=-1;
 const auto read=[&](){
  if(trace)trace->phase=ProducerWaitPhase::qpc;
  const auto now=clock.now_ns();
  if(trace){if(!trace->qpc_reads)trace->first_qpc_ns=now;trace->last_qpc_ns=now;++trace->qpc_reads;}
  if(now<0||deadline<0){if(trace)trace->failure=ProducerWaitFailure::clock_domain;throw std::runtime_error("producer QPC domain");}
  if(now<prior){if(trace)trace->failure=ProducerWaitFailure::nonmonotonic_clock;throw std::runtime_error("producer nonmonotonic QPC");}
  prior=now;return now;
 };
 const auto complete=[&](){if(trace){trace->completed=true;trace->phase=ProducerWaitPhase::complete;}};
 try {
 auto now=read();if(now>=deadline){complete();return;}
 if(deadline-now>100'000'000){if(trace)trace->failure=ProducerWaitFailure::horizon;throw std::runtime_error("producer wait horizon");}
 int waits=0;
 while(now<deadline&&deadline-now>producer_active_tail_ns){
  if(waits==4){if(trace)trace->failure=ProducerWaitFailure::early_wake_bound;throw std::runtime_error("producer repeated early wake");}
  const auto remaining=deadline-now-producer_active_tail_ns;
  if(trace){trace->phase=ProducerWaitPhase::kernel_wait;trace->kernel_calls=std::uint32_t(waits+1);trace->kernels[std::size_t(waits)].ticks_100ns=remaining/100+(remaining%100!=0);}
  wait(remaining/100+(remaining%100!=0));++waits;now=read();
 }
 std::uint32_t polls=0;
 if(trace&&now<deadline){trace->spin_entered=true;trace->spin_enter_ns=now;}
 while(now<deadline){
  if(polls==producer_active_tail_poll_cap){if(trace)trace->failure=ProducerWaitFailure::spin_bound;throw std::runtime_error("producer active tail poll bound");}
  if(trace){trace->phase=ProducerWaitPhase::spin;++trace->spin_pause_calls;}
  pause();++polls;if(trace)trace->spin_poll_count=polls;now=read();
 }
 if(trace&&trace->spin_entered){trace->spin_completed=true;trace->spin_return_ns=now;}
 complete();
 }catch(...){
  if(trace){trace->failure_phase=trace->phase;if(trace->failure==ProducerWaitFailure::none)
   trace->failure=trace->phase==ProducerWaitPhase::qpc?ProducerWaitFailure::clock_read:
    trace->phase==ProducerWaitPhase::spin?ProducerWaitFailure::pause_callback:ProducerWaitFailure::kernel_callback;}
  throw;
 }
}
// Separate cold-only candidate. The whole remaining QPC period is active;
// there is no timer creation, arming, blocking wait, Sleep or OS thread yield.
// CPU cost is unknown until measured. YieldProcessor is only a processor pause.
inline constexpr Nanoseconds producer_active_wait_horizon_ns=100'000'000;
inline constexpr std::uint32_t producer_active_wait_poll_cap=1u<<24;
inline constexpr char producer_active_wait_policy[]="full_period_active_qpc";
template<class Pause>
void wait_producer_deadline_active(const Clock& clock,Nanoseconds deadline,Pause&& pause,
                                   ProducerWaitTrace* trace=nullptr){
 if(trace){*trace={};trace->due_ns=deadline;trace->entered=true;}
 Nanoseconds prior=-1;
 const auto read=[&](){
  if(trace)trace->phase=ProducerWaitPhase::qpc;
  const auto now=clock.now_ns();
  if(trace){if(!trace->qpc_reads)trace->first_qpc_ns=now;trace->last_qpc_ns=now;++trace->qpc_reads;}
  if(now<0||deadline<0){if(trace)trace->failure=ProducerWaitFailure::clock_domain;throw std::runtime_error("producer QPC domain");}
  if(now<prior){if(trace)trace->failure=ProducerWaitFailure::nonmonotonic_clock;throw std::runtime_error("producer nonmonotonic QPC");}
  prior=now;return now;
 };
 const auto complete=[&](){if(trace){trace->completed=true;trace->phase=ProducerWaitPhase::complete;}};
 try {
  auto now=read();if(now>=deadline){complete();return;}
  if(deadline-now>producer_active_wait_horizon_ns){if(trace)trace->failure=ProducerWaitFailure::horizon;throw std::runtime_error("producer wait horizon");}
  if(trace){trace->spin_entered=true;trace->spin_enter_ns=now;}
  std::uint32_t polls=0;
  while(now<deadline){
   if(trace)trace->phase=ProducerWaitPhase::spin;
   if(polls==producer_active_wait_poll_cap){if(trace)trace->failure=ProducerWaitFailure::spin_bound;throw std::runtime_error("producer active wait poll bound");}
   if(trace)++trace->spin_pause_calls;
   pause();++polls;if(trace)trace->spin_poll_count=polls;now=read();
  }
  if(trace){trace->spin_completed=true;trace->spin_return_ns=now;}
  complete();
 }catch(...){
  if(trace){trace->failure_phase=trace->phase;if(trace->failure==ProducerWaitFailure::none)
   trace->failure=trace->phase==ProducerWaitPhase::qpc?ProducerWaitFailure::clock_read:ProducerWaitFailure::pause_callback;}
  throw;
 }
}
#ifdef _WIN32
static_assert(cold_process_qos_version==PROCESS_POWER_THROTTLING_CURRENT_VERSION);
static_assert(cold_process_qos_execution_speed_mask==PROCESS_POWER_THROTTLING_EXECUTION_SPEED);
static_assert(cold_process_qos_supported_mask==(PROCESS_POWER_THROTTLING_EXECUTION_SPEED|PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION));
struct WindowsColdProcessQosApi {
 std::uint32_t process()const noexcept{return GetCurrentProcessId();}
 std::uint32_t thread()const noexcept{return GetCurrentThreadId();}
 std::uint32_t process_class()const noexcept{return GetPriorityClass(GetCurrentProcess());}
 ColdProcessQosApiResult get(ColdProcessQosState& value)const noexcept{
  PROCESS_POWER_THROTTLING_STATE state{};state.Version=PROCESS_POWER_THROTTLING_CURRENT_VERSION;
  const auto status=GetProcessInformation(GetCurrentProcess(),ProcessPowerThrottling,&state,sizeof(state));
  const auto error=status?0u:GetLastError();value={state.Version,state.ControlMask,state.StateMask};
  return {static_cast<std::uint32_t>(status),error,true};
 }
 ColdProcessQosApiResult set(const ColdProcessQosState& value)const noexcept{
  PROCESS_POWER_THROTTLING_STATE state{value.version,value.control_mask,value.state_mask};
  const auto status=SetProcessInformation(GetCurrentProcess(),ProcessPowerThrottling,&state,sizeof(state));
  return {static_cast<std::uint32_t>(status),status?0u:GetLastError(),true};
 }
};
inline ProducerThreadCpuSample sample_producer_thread_cpu() noexcept {
 FILETIME created{},exited{},kernel{},user{};const auto status=GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user);
 const auto error=status?0u:GetLastError();
 return {(std::uint64_t(kernel.dwHighDateTime)<<32)|kernel.dwLowDateTime,
         (std::uint64_t(user.dwHighDateTime)<<32)|user.dwLowDateTime,
         static_cast<std::uint32_t>(status),error,true};
}
struct WindowsProducerPriorityApi {
 WindowsProducerPriorityApi(){if(GetPriorityClass(GetCurrentProcess())!=NORMAL_PRIORITY_CLASS)throw std::runtime_error("producer normal process class required");}
 int current()const noexcept{return GetThreadPriority(GetCurrentThread());}
 std::uint32_t thread()const noexcept{return GetCurrentThreadId();}
 bool set(int priority)const noexcept{return SetThreadPriority(GetCurrentThread(),priority)!=0;}
};
class ProducerDeadlineWait final {
 HANDLE timer_{};
public:
 ProducerDeadlineWait(){timer_=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_MODIFY_STATE|SYNCHRONIZE);if(!timer_)throw std::runtime_error("producer high-resolution timer creation");}
 ProducerDeadlineWait(const ProducerDeadlineWait&)=delete;
 ProducerDeadlineWait& operator=(const ProducerDeadlineWait&)=delete;
 ~ProducerDeadlineWait(){if(timer_){CancelWaitableTimer(timer_);CloseHandle(timer_);}}
 void wait_until(const Clock& clock,Nanoseconds deadline,ProducerWaitTrace* trace=nullptr){
  struct TimerApi {
   HANDLE timer;
   ProducerTimerResult arm(Nanoseconds ticks)const noexcept {
    LARGE_INTEGER due;due.QuadPart=-ticks;const auto status=SetWaitableTimerEx(timer,&due,0,nullptr,nullptr,nullptr,0);
    return {static_cast<std::uint32_t>(status),status?0u:GetLastError()};
   }
   ProducerTimerResult wait(std::uint32_t timeout)const noexcept {
    const auto status=WaitForSingleObject(timer,timeout);return {status,status==WAIT_FAILED?GetLastError():0u};
   }
  } api{timer_};
  wait_producer_deadline_with_active_tail(clock,deadline,[&](Nanoseconds ticks){
   auto* kernel=trace?&trace->kernels[trace->kernel_calls-1]:nullptr;
   producer_kernel_wait_phase(clock,ticks,api,kernel,trace?&trace->phase:nullptr);
  },[]{YieldProcessor();},trace);
 }
};
class ActiveProducerDeadlineWait final {
public:
 void wait_until(const Clock& clock,Nanoseconds deadline,ProducerWaitTrace* trace=nullptr){
  wait_producer_deadline_active(clock,deadline,[]{YieldProcessor();},trace);
 }
};
#endif
}
