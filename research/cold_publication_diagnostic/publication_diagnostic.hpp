#pragma once
// Endpoint-free diagnostic only. No performance or qualification claim.
// This header is included only by the copied diagnostic core/chain TUs.
#ifndef PAS_PUBLICATION_DIAGNOSTIC_OVERLAY
#error "Diagnostic-only object overlay requires explicit opt-in"
#endif
#include "pas/core.hpp"
#include "producer_wait.hpp"
#include "poll_gap_diagnostic.hpp"
#include <array>
#include <cstdint>
#include <exception>
#include <type_traits>

namespace cold::publication_diag {
enum class Point : std::uint8_t {
 iteration_enter,wait_enter,wait_return,render_start,render_end,
 frame_construct_enter,frame_construct_return,frame_scalars_return,
 frame_backend_enter,frame_backend_return,frame_metadata_return,publish_call_enter,
 publish_enter,lock1_request,lock1_acquired,lock1_released,
 copy_enter,copy_return,target_scalars_return,target_backend_enter,target_backend_return,
 target_format_enter,target_format_return,target_metadata_return,
 lock2_request,lock2_acquired,published_clock_return,lock2_body_return,lock2_released,
 notify_enter,notify_return,publish_exit,publication_return,
 frame_destroy_enter,frame_destroy_return,iteration_return,count
};
inline constexpr std::size_t point_count=static_cast<std::size_t>(Point::count);
inline constexpr std::array<const char*,point_count> point_names{
 "iteration_enter","wait_enter","wait_return","render_start","render_end",
 "frame_construct_enter","frame_construct_return","frame_scalars_return",
 "frame_backend_enter","frame_backend_return","frame_metadata_return","publish_call_enter",
 "publish_enter","lock1_request","lock1_acquired","lock1_released",
 "copy_enter","copy_return","target_scalars_return","target_backend_enter","target_backend_return",
 "target_format_enter","target_format_return","target_metadata_return",
 "lock2_request","lock2_acquired","published_clock_return","lock2_body_return","lock2_released",
 "notify_enter","notify_return","publish_exit","publication_return",
 "frame_destroy_enter","frame_destroy_return","iteration_return"
};
static_assert(point_count==36 && point_count<64);
struct StringFacts {
 std::uint64_t before_size=0,before_capacity=0,after_size=0,after_capacity=0;
 bool before_available=false,assignment_returned=false,data_pointer_changed=false;
};
enum class PublishOutcome : std::uint8_t {not_entered,entered,closed,pool_drop,returned_true};
struct Attempt {
 std::array<pas::Nanoseconds,point_count> stamps{};
 pas::current_rails::ProducerWaitTrace wait{};
 cold::poll_gap::Snapshot gap{};
 StringFacts frame_backend{},slot_backend{},slot_format{};
 pas::Nanoseconds due_ns=0,last_recorded_ns=0;
 std::uint64_t present_mask=0,read_failure_mask=0;
 std::uint32_t attempt=0;
 PublishOutcome publish_outcome=PublishOutcome::not_entered;
 bool entered=false,completed=false,exception_seen=false,invalid_clock=false,published_ok=false;
};
static_assert(std::is_trivially_copyable_v<Attempt> && sizeof(Attempt)<=1024);
inline constexpr std::size_t slot_byte_bound=1024,slot_extra=64;
inline thread_local Attempt* active_attempt=nullptr;
inline thread_local const pas::Clock* active_clock=nullptr;
inline void record(Point point,pas::Nanoseconds value) noexcept {
 if(!active_attempt)return;
 auto& a=*active_attempt;const auto index=static_cast<std::size_t>(point);
 if(index>=point_count){a.invalid_clock=true;return;}
 if(value<0||(a.present_mask&&value<a.last_recorded_ns))a.invalid_clock=true;
 a.stamps[index]=value;a.present_mask|=std::uint64_t(1)<<index;a.last_recorded_ns=value;
}
inline void stamp(Point point) noexcept {
 if(!active_attempt||!active_clock)return;
 try{record(point,active_clock->now_ns());}
 catch(...){active_attempt->read_failure_mask|=std::uint64_t(1)<<static_cast<std::size_t>(point);active_attempt->invalid_clock=true;}
}
// Additional diagnostic reads never replace a business-operation exception.
// Original wait reads still use poll_gap::Clock and propagate unchanged.
struct StampOnExit {
 Point point;explicit StampOnExit(Point p) noexcept:point(p){}
 ~StampOnExit(){stamp(point);}
};
struct PublishExit {
 int exceptions=std::uncaught_exceptions();
 PublishExit() noexcept {if(active_attempt)active_attempt->publish_outcome=PublishOutcome::entered;stamp(Point::publish_enter);}
 ~PublishExit(){if(active_attempt&&std::uncaught_exceptions()>exceptions)active_attempt->exception_seen=true;stamp(Point::publish_exit);}
};
inline void outcome(PublishOutcome value) noexcept {if(active_attempt)active_attempt->publish_outcome=value;}
inline std::uintptr_t before_string(StringFacts& facts,const std::string& value) noexcept {
 facts.before_size=value.size();facts.before_capacity=value.capacity();facts.before_available=true;
 return reinterpret_cast<std::uintptr_t>(value.data());
}
inline void after_string(StringFacts& facts,const std::string& value,std::uintptr_t previous) noexcept {
 facts.after_size=value.size();facts.after_capacity=value.capacity();facts.assignment_returned=true;
 facts.data_pointer_changed=previous!=reinterpret_cast<std::uintptr_t>(value.data());
}
// The core caller has no TLS binding on the uninstrumented production path.
// Empty facts are used only as stack storage when this diagnostic core is
// called outside its diagnostic producer, never as an observed allocation.
inline StringFacts& slot_backend_facts(StringFacts& inactive) noexcept {return active_attempt?active_attempt->slot_backend:inactive;}
inline StringFacts& slot_format_facts(StringFacts& inactive) noexcept {return active_attempt?active_attempt->slot_format:inactive;}
struct AttemptBinding {
 Attempt* previous_attempt;const pas::Clock* previous_clock;
 int exceptions=std::uncaught_exceptions();
 AttemptBinding(Attempt& a,const pas::Clock& clock,std::uint32_t number,pas::Nanoseconds due) noexcept
  :previous_attempt(active_attempt),previous_clock(active_clock){
  a={};a.attempt=number;a.due_ns=due;a.entered=true;active_attempt=&a;active_clock=&clock;stamp(Point::iteration_enter);
 }
 void complete() noexcept {active_attempt->completed=true;}
 ~AttemptBinding(){
  if(std::uncaught_exceptions()>exceptions)active_attempt->exception_seen=true;
  stamp(Point::iteration_return);active_attempt=previous_attempt;active_clock=previous_clock;
 }
};
}
