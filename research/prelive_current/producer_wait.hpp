#pragma once
#include "pas/core.hpp"
#include <stdexcept>
#include <limits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace pas::current_rails {
// Only the offline pixel producer uses this scope. Both A and B use the same
// policy; consumer, writer, process class, cadence and QPC epoch are unchanged.
template<class Api> class ProducerPriorityScope final {
 Api& api_;int previous_;std::uint32_t thread_;bool active_=false;
public:
 explicit ProducerPriorityScope(Api& api):api_(api),previous_(api.current()),thread_(api.thread()){
  if(previous_==std::numeric_limits<int>::max())throw std::runtime_error("producer priority query");
  if(!api_.set(1))throw std::runtime_error("producer priority set");
  active_=true;
  if(api_.current()!=1){api_.set(previous_);active_=false;throw std::runtime_error("producer priority readback");}
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
#ifdef _WIN32
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
 void wait_until(const Clock& clock,Nanoseconds deadline){wait_producer_deadline(clock,deadline,[&](Nanoseconds ticks){
  LARGE_INTEGER due;due.QuadPart=-ticks;
  if(!SetWaitableTimerEx(timer_,&due,0,nullptr,nullptr,nullptr,0))throw std::runtime_error("producer timer arm");
  const auto result=WaitForSingleObject(timer_,250);
  if(result!=WAIT_OBJECT_0)throw std::runtime_error("producer bounded timer wait");
 });}
};
#endif
}
