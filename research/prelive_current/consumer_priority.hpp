#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace pas::current_rails {
enum class ConsumerPriorityFailure : std::uint8_t {
 none,process_query,normal_process_required,thread_query,priority_query,
 priority_set,priority_readback_query,priority_readback_mismatch,
 wrong_thread,rollback_set,rollback_readback_query,rollback_readback_mismatch,
 restore_set,restore_readback_query,restore_readback_mismatch,fallback_api_exception
};
inline const char* consumer_priority_failure_name(ConsumerPriorityFailure failure) noexcept {
 switch(failure) {
 case ConsumerPriorityFailure::none:return "none";
 case ConsumerPriorityFailure::process_query:return "consumer_priority_process_query";
 case ConsumerPriorityFailure::normal_process_required:return "consumer_priority_normal_process_required";
 case ConsumerPriorityFailure::thread_query:return "consumer_priority_thread_query";
 case ConsumerPriorityFailure::priority_query:return "consumer_priority_query";
 case ConsumerPriorityFailure::priority_set:return "consumer_priority_set";
 case ConsumerPriorityFailure::priority_readback_query:return "consumer_priority_readback_query";
 case ConsumerPriorityFailure::priority_readback_mismatch:return "consumer_priority_readback_mismatch";
 case ConsumerPriorityFailure::wrong_thread:return "consumer_priority_wrong_thread";
 case ConsumerPriorityFailure::rollback_set:return "consumer_priority_rollback_set";
 case ConsumerPriorityFailure::rollback_readback_query:return "consumer_priority_rollback_readback_query";
 case ConsumerPriorityFailure::rollback_readback_mismatch:return "consumer_priority_rollback_readback_mismatch";
 case ConsumerPriorityFailure::restore_set:return "consumer_priority_restore_set";
 case ConsumerPriorityFailure::restore_readback_query:return "consumer_priority_restore_readback_query";
 case ConsumerPriorityFailure::restore_readback_mismatch:return "consumer_priority_restore_readback_mismatch";
 case ConsumerPriorityFailure::fallback_api_exception:return "consumer_priority_fallback_api_exception";
 }
 return "consumer_priority_unknown_failure";
}
inline constexpr int consumer_priority_above_normal=1;
inline constexpr int consumer_priority_query_error=std::numeric_limits<int>::max();
inline constexpr std::uint32_t consumer_priority_normal_process_class=0x20;

// One external, fixed-scalar receipt per scope, surviving constructor failure.
// This is scheduling evidence only; it grants no cost, device or live admission.
struct ConsumerPrioritySnapshot {
 std::uint32_t thread_id=0,process_class=0,process_class_after_close=0;
 int previous=consumer_priority_query_error;
 int applied_readback=consumer_priority_query_error;
 int rollback_readback=consumer_priority_query_error;
 int restored_readback=consumer_priority_query_error;
 int fallback_readback=consumer_priority_query_error;
 bool set_succeeded=false,applied=false,rollback_attempted=false,rollback_restored=false;
 bool close_attempted=false,restore_set_succeeded=false,restored=false,fallback_attempted=false;
 ConsumerPriorityFailure failure=ConsumerPriorityFailure::none;
 ConsumerPriorityFailure rollback_failure=ConsumerPriorityFailure::none;
 ConsumerPriorityFailure fallback_failure=ConsumerPriorityFailure::none;
 bool verified() const noexcept {
  return applied&&close_attempted&&restored&&failure==ConsumerPriorityFailure::none&&
   rollback_failure==ConsumerPriorityFailure::none&&fallback_failure==ConsumerPriorityFailure::none&&
   process_class==consumer_priority_normal_process_class&&
   process_class_after_close==consumer_priority_normal_process_class&&
   applied_readback==consumer_priority_above_normal&&restored_readback==previous;
 }
};
class ConsumerPriorityError final : public std::runtime_error {
 ConsumerPriorityFailure reason_,rollback_;
public:
 explicit ConsumerPriorityError(ConsumerPriorityFailure reason,
   ConsumerPriorityFailure rollback=ConsumerPriorityFailure::none)
  :std::runtime_error(consumer_priority_failure_name(reason)),reason_(reason),rollback_(rollback){}
 ConsumerPriorityFailure reason() const noexcept{return reason_;}
 ConsumerPriorityFailure rollback_reason() const noexcept{return rollback_;}
};

// Api queries and setters are nonthrowing. The scope touches only its current
// thread, never process priority, another thread, affinity, boost or CPU sets.
// Both A and B must use the same scope. Explicit close is the only verified
// restoration path; destructor fallback never upgrades an earlier failure.
template<class Api> class ConsumerPriorityScope final {
 Api& api_;ConsumerPrioritySnapshot& receipt_;bool active_=false;
 [[noreturn]] void reject(ConsumerPriorityFailure reason) {
  if(receipt_.failure==ConsumerPriorityFailure::none)receipt_.failure=reason;
  throw ConsumerPriorityError(reason,receipt_.rollback_failure);
 }
 void rollback() noexcept {
  receipt_.rollback_attempted=true;
  if(api_.thread()!=receipt_.thread_id){receipt_.rollback_failure=ConsumerPriorityFailure::wrong_thread;return;}
  if(!api_.set(receipt_.previous)){receipt_.rollback_failure=ConsumerPriorityFailure::rollback_set;return;}
  receipt_.rollback_readback=api_.current();
  if(receipt_.rollback_readback==consumer_priority_query_error)
   receipt_.rollback_failure=ConsumerPriorityFailure::rollback_readback_query;
  else if(receipt_.rollback_readback!=receipt_.previous)
   receipt_.rollback_failure=ConsumerPriorityFailure::rollback_readback_mismatch;
  else receipt_.rollback_restored=true;
 }
public:
 ConsumerPriorityScope(Api& api,ConsumerPrioritySnapshot& receipt):api_(api),receipt_(receipt) {
  receipt_={};receipt_.process_class=api_.process_class();
  if(!receipt_.process_class)reject(ConsumerPriorityFailure::process_query);
  if(receipt_.process_class!=consumer_priority_normal_process_class)reject(ConsumerPriorityFailure::normal_process_required);
  receipt_.thread_id=api_.thread();if(!receipt_.thread_id)reject(ConsumerPriorityFailure::thread_query);
  receipt_.previous=api_.current();
  if(receipt_.previous==consumer_priority_query_error)reject(ConsumerPriorityFailure::priority_query);
  if(api_.thread()!=receipt_.thread_id)reject(ConsumerPriorityFailure::wrong_thread);
  if(!api_.set(consumer_priority_above_normal))reject(ConsumerPriorityFailure::priority_set);
  receipt_.set_succeeded=true;receipt_.applied_readback=api_.current();
  if(receipt_.applied_readback!=consumer_priority_above_normal) {
   const auto reason=receipt_.applied_readback==consumer_priority_query_error?
    ConsumerPriorityFailure::priority_readback_query:ConsumerPriorityFailure::priority_readback_mismatch;
   receipt_.failure=reason;rollback();reject(reason);
  }
  receipt_.applied=true;active_=true;
 }
 ConsumerPriorityScope(const ConsumerPriorityScope&)=delete;
 ConsumerPriorityScope& operator=(const ConsumerPriorityScope&)=delete;
 ~ConsumerPriorityScope() noexcept {
  if(!active_)return;receipt_.fallback_attempted=true;
  try {
   if(api_.thread()!=receipt_.thread_id){receipt_.fallback_failure=ConsumerPriorityFailure::wrong_thread;return;}
   if(!api_.set(receipt_.previous)){receipt_.fallback_failure=ConsumerPriorityFailure::restore_set;return;}
   receipt_.fallback_readback=api_.current();
   if(receipt_.fallback_readback==consumer_priority_query_error)
    receipt_.fallback_failure=ConsumerPriorityFailure::restore_readback_query;
   else if(receipt_.fallback_readback!=receipt_.previous)
    receipt_.fallback_failure=ConsumerPriorityFailure::restore_readback_mismatch;
  }catch(...){receipt_.fallback_failure=ConsumerPriorityFailure::fallback_api_exception;}
 }
 void close() {
  if(!active_)return;receipt_.close_attempted=true;
  if(api_.thread()!=receipt_.thread_id)reject(ConsumerPriorityFailure::wrong_thread);
  if(!api_.set(receipt_.previous))reject(ConsumerPriorityFailure::restore_set);
  receipt_.restore_set_succeeded=true;receipt_.restored_readback=api_.current();
  if(receipt_.restored_readback==consumer_priority_query_error)reject(ConsumerPriorityFailure::restore_readback_query);
  if(receipt_.restored_readback!=receipt_.previous)reject(ConsumerPriorityFailure::restore_readback_mismatch);
  receipt_.restored=true;active_=false;receipt_.process_class_after_close=api_.process_class();
  if(!receipt_.process_class_after_close)reject(ConsumerPriorityFailure::process_query);
  if(receipt_.process_class_after_close!=consumer_priority_normal_process_class)reject(ConsumerPriorityFailure::normal_process_required);
 }
};
#ifdef _WIN32
static_assert(consumer_priority_above_normal==THREAD_PRIORITY_ABOVE_NORMAL);
static_assert(consumer_priority_query_error==THREAD_PRIORITY_ERROR_RETURN);
static_assert(consumer_priority_normal_process_class==NORMAL_PRIORITY_CLASS);
struct WindowsConsumerPriorityApi {
 std::uint32_t process_class() const noexcept{return GetPriorityClass(GetCurrentProcess());}
 int current() const noexcept{return GetThreadPriority(GetCurrentThread());}
 std::uint32_t thread() const noexcept{return GetCurrentThreadId();}
 bool set(int priority) const noexcept{return SetThreadPriority(GetCurrentThread(),priority)!=0;}
};
#endif
}
