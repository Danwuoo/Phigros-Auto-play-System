#pragma once
#include "pas/core.hpp"
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace pas::current_rails {
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
