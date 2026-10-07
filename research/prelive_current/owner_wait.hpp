#pragma once
#include "pas/core.hpp"
#include <algorithm>
namespace pas::current_rails {
// Called only by the sole owner. A frame arrival may wake this wait earlier.
// next_due includes gate/plan expiry as well as the next injection deadline.
// No new frame, prediction, or diagnostic data is admitted by this helper.
inline Nanoseconds owner_wait_ns(const ContactScheduler* scheduler, Nanoseconds now) {
 constexpr Nanoseconds watchdog = 10'000'000;
 if (!scheduler) return watchdog;
 const auto due = scheduler->next_due_ns();
 if (!due) return watchdog;
 if (*due <= now) return 0;
 // Ordered signed times can still span more than INT64_MAX. Subtract as
 // unsigned, then clamp before converting the bounded duration back.
 const auto remaining = static_cast<std::uint64_t>(*due) - static_cast<std::uint64_t>(now);
 return static_cast<Nanoseconds>(std::min(remaining, static_cast<std::uint64_t>(watchdog)));
}
}
