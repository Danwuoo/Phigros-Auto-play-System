#pragma once
#include "pas/core.hpp"
#include <stdexcept>
#include <limits>
#include <array>
#include <type_traits>
#include <optional>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace pas::current_rails {
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
#ifdef _WIN32
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
#endif
}
