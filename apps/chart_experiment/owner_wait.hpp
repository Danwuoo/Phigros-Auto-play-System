#pragma once
#include "pas/core.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace rhythm::experimental {
// One owner, no system timer-resolution or priority changes. This is an OS
// wakeup request, not a timing guarantee; the scheduler checks real QPC again.
class OwnerWait {
    const pas::Clock& clock_;
    HANDLE timer_=nullptr;
public:
    std::uint64_t waits=0;
    pas::Nanoseconds maximum_elapsed_ns=0,maximum_overshoot_ns=0;
    explicit OwnerWait(const pas::Clock& clock):clock_(clock) {
        timer_=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_MODIFY_STATE|SYNCHRONIZE);
        if(!timer_)throw std::runtime_error("high-resolution owner timer unavailable");
    }
    ~OwnerWait(){if(timer_)CloseHandle(timer_);}
    OwnerWait(const OwnerWait&)=delete;
    OwnerWait& operator=(const OwnerWait&)=delete;
    void until(pas::Nanoseconds target) {
        const auto begin=clock_.now_ns();
        if(target<=begin)return;
        const auto requested=std::min(target-begin,pas::Nanoseconds{1'000'000});
        LARGE_INTEGER relative{};relative.QuadPart=-(requested+99)/100;
        if(!SetWaitableTimer(timer_,&relative,0,nullptr,nullptr,FALSE))
            throw std::runtime_error("owner timer arm failed");
        if(WaitForSingleObject(timer_,100)!=WAIT_OBJECT_0)
            throw std::runtime_error("owner timer failed or exceeded100ms");
        const auto end=clock_.now_ns();
        if(end<begin)throw std::runtime_error("owner timer clock regressed");
        ++waits;maximum_elapsed_ns=std::max(maximum_elapsed_ns,end-begin);
        maximum_overshoot_ns=std::max(maximum_overshoot_ns,end-begin-requested);
    }
};
} // namespace rhythm::experimental
