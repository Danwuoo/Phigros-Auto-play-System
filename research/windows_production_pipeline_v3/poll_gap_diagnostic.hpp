#pragma once
#include "pas/core.hpp"
#include <cstdint>
#include <type_traits>

// Cold producer diagnostics only. The wrapped Clock is called exactly once
// per now_ns(); its value or exception passes through unchanged. A gap locates
// two successive reads, not a scheduler, power, or CPU execution verdict.
namespace cold::poll_gap {
inline constexpr std::uint32_t interval_cap=1u<<24;
inline constexpr std::uint32_t sample_cap=interval_cap+1;
struct Snapshot {
 pas::Nanoseconds max_gap_ns=0,gap_begin_ns=0,gap_end_ns=0;
 std::uint32_t end_sample_index=0,sample_count=0,interval_count=0;
 // available records an observed positive interval. After invalid becomes
 // sticky, any retained maximum is partial and is not valid timing evidence.
 bool available=false,invalid=false;
};
static_assert(std::is_trivially_copyable_v<Snapshot> && sizeof(Snapshot)<=40);

class Clock final:public pas::Clock {
 const pas::Clock& source_;
 Snapshot& snapshot_;
 mutable pas::Nanoseconds previous_=0;
 mutable bool previous_available_=false;
public:
 Clock(const pas::Clock& source,Snapshot& snapshot) noexcept:source_(source),snapshot_(snapshot){snapshot_={};}
 pas::Nanoseconds now_ns()const override {
  pas::Nanoseconds now;
  try{now=source_.now_ns();}catch(...){snapshot_.invalid=true;throw;}
  // No diagnostic error replaces the original wait's domain/monotonic/cap
  // error. Even an out-of-contract direct caller still receives its Clock.
  if(snapshot_.sample_count>=sample_cap){snapshot_.invalid=true;return now;}
  ++snapshot_.sample_count;
  if(snapshot_.invalid)return now;
  if(now<0||(previous_available_&&now<previous_)){snapshot_.invalid=true;return now;}
  if(previous_available_){
   if(snapshot_.interval_count>=interval_cap){snapshot_.invalid=true;return now;}
   ++snapshot_.interval_count;
   const auto gap=now-previous_; // both endpoints are nonnegative and ordered
   // A single sample (including an already-late wait) has no measured gap.
   // Equal reads are counted but do not claim a positive gap. Ties retain the
   // first maximum, so endpoints/index are deterministic.
   if(gap>snapshot_.max_gap_ns){
    snapshot_.max_gap_ns=gap;snapshot_.gap_begin_ns=previous_;snapshot_.gap_end_ns=now;
    snapshot_.end_sample_index=snapshot_.sample_count;snapshot_.available=true;
   }
  }
  previous_=now;previous_available_=true;return now;
 }
};
}
