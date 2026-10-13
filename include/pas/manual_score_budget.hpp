#pragma once
#include "pas/core.hpp"
#include <atomic>
#include <stdexcept>

namespace pas {
// Session limits only; never supplied to gameplay decisions or touch plans.
class ManualScoreBudget final {
public:
    ManualScoreBudget(Nanoseconds start,Nanoseconds watchdog,unsigned rounds)
        : start_(start),standby_since_(start),rounds_(rounds) {
        if(rounds<1||rounds>20||watchdog<=0||watchdog>600'000'000'000LL)
            throw std::invalid_argument("score-only requires 1..20 rounds and watchdog at most 600s");
    }
    bool finish(bool confirmed,Nanoseconds now) {
        standby_since_=now;
        return !confirmed||completed_.fetch_add(1)+1>=rounds_;
    }
    bool wall_expired(Nanoseconds now) const {return now-start_>=900'000'000'000LL*rounds_;}
    bool waiting_expired(Nanoseconds now) const {
        return now-standby_since_.load()>=(rounds_==1?300'000'000'000LL:1'800'000'000'000LL);
    }
    unsigned completed() const {return completed_.load();}
    unsigned limit() const {return rounds_;}
private:
    Nanoseconds start_;
    std::atomic<Nanoseconds> standby_since_;
    unsigned rounds_;
    std::atomic<unsigned> completed_{0};
};
}
