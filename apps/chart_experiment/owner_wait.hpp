#pragma once
#include "pas/core.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <limits>
#include <optional>
#include <stdexcept>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace rhythm::experimental {
using OwnerWaitNs=pas::Nanoseconds;
struct OwnerWaitOptions {
    OwnerWaitNs poll_limit_ns=1'000'000,critical_guard_ns=50'000'000,spin_slice_ns=250'000;
    std::uint64_t spin_iteration_limit=65536;
    bool above_normal_owner_thread=true,timer_resolution_1ms=true;
};
inline void validate_owner_wait_options(const OwnerWaitOptions& o) {
    if(o.poll_limit_ns<=0||o.poll_limit_ns>1'000'000||o.critical_guard_ns<1'000'000||
       o.critical_guard_ns>100'000'000||o.spin_slice_ns<=0||o.spin_slice_ns>250'000||
       !o.spin_iteration_limit||o.spin_iteration_limit>65536)
        throw std::invalid_argument("owner wait policy outside finite app-local bounds");
}
enum class OwnerWaitKind { target_due,input_overdue,timer,spin };
inline const char* owner_wait_kind_name(OwnerWaitKind kind)noexcept {
    switch(kind) {
    case OwnerWaitKind::target_due:return "target_due";
    case OwnerWaitKind::input_overdue:return "input_overdue";
    case OwnerWaitKind::timer:return "timer";
    case OwnerWaitKind::spin:return "spin";
    }
    return "invalid";
}
struct OwnerWaitPlan {
    OwnerWaitKind kind=OwnerWaitKind::target_due;
    OwnerWaitNs planned_return_ns=0,timer_request_ns=0;
    bool critical_deadline=false;
};
// A polling target is not an input deadline. The optional deadline must be an
// actual compiled step time, including an overdue value (never clamped to now).
inline OwnerWaitPlan plan_owner_wait(OwnerWaitNs begin,OwnerWaitNs target,
                                    std::optional<OwnerWaitNs> input_due,const OwnerWaitOptions& o={}) {
    validate_owner_wait_options(o);constexpr auto safe=std::numeric_limits<OwnerWaitNs>::max()/4;
    if(begin<0||begin>safe||target<0||target>safe||(input_due&&(*input_due<0||*input_due>safe)))
        throw std::invalid_argument("owner wait clock/target outside safe host-time range");
    if(input_due&&*input_due<=begin)return {OwnerWaitKind::input_overdue,begin,0,true};
    if(target<=begin)return {OwnerWaitKind::target_due,begin,0,false};
    const bool critical=input_due&&*input_due-begin<=o.critical_guard_ns;
    auto duration=std::min(target-begin,o.poll_limit_ns);
    if(critical)duration=std::min({duration,o.spin_slice_ns,*input_due-begin});
    else if(input_due)duration=std::min(duration,*input_due-begin-o.critical_guard_ns);
    if(critical||duration<=o.spin_slice_ns)return {OwnerWaitKind::spin,begin+duration,0,critical};
    return {OwnerWaitKind::timer,begin+duration,duration,false};
}
struct OwnerWaitPlatformStatus {
    DWORD owner_thread_id=0;
    int original_thread_priority=THREAD_PRIORITY_NORMAL,applied_thread_priority=THREAD_PRIORITY_NORMAL;
    bool priority_requested=false,priority_changed=false,priority_restore_attempted=false,priority_restored=false;
    DWORD priority_error=0,priority_restore_error=0;
    bool high_resolution_timer=false,timer_resolution_requested=false,timer_resolution_active=false;
    bool timer_resolution_end_attempted=false,finished=false;
    UINT timer_resolution_ms=1,timer_resolution_begin_result=~UINT{0},timer_resolution_end_result=~UINT{0};
    DWORD timer_resolution_load_error=0;
};
// Production changes only the calling thread, one timer and one balanced
// timeBeginPeriod request. Backends must roll back on destruction, including
// when OwnerWait construction fails. Tests use a deterministic fake backend.
class OwnerWaitBackend {
public:
    virtual ~OwnerWaitBackend()=default;
    virtual void wait_timer(OwnerWaitNs duration)=0;
    virtual void spin_pause()=0;
    virtual OwnerWaitPlatformStatus platform_status()const noexcept=0;
    virtual void finish()noexcept=0;
};
class WindowsOwnerWaitBackend final:public OwnerWaitBackend {
    using PeriodFn=UINT(WINAPI*)(UINT);
    struct Resources {
        HANDLE timer=nullptr,thread=nullptr;HMODULE winmm=nullptr;PeriodFn end_period=nullptr;
        OwnerWaitPlatformStatus status;
        void finish()noexcept {
            if(status.finished)return;
            if(timer){CancelWaitableTimer(timer);CloseHandle(timer);timer=nullptr;}
            if(status.timer_resolution_active&&end_period) {
                status.timer_resolution_end_attempted=true;
                status.timer_resolution_end_result=end_period(status.timer_resolution_ms);
                status.timer_resolution_active=status.timer_resolution_end_result!=0;
            }
            if(status.priority_changed&&thread) {
                status.priority_restore_attempted=true;
                status.priority_restored=SetThreadPriority(thread,status.original_thread_priority)!=FALSE;
                if(!status.priority_restored)status.priority_restore_error=GetLastError();
            }
            if(thread){CloseHandle(thread);thread=nullptr;}
            if(winmm){FreeLibrary(winmm);winmm=nullptr;}
            status.finished=true;
        }
        ~Resources(){finish();}
    } resources_;
    static PeriodFn period_function(HMODULE module,const char* name)noexcept {
        const auto address=GetProcAddress(module,name);PeriodFn result=nullptr;
        static_assert(sizeof(result)==sizeof(address));std::memcpy(&result,&address,sizeof(result));return result;
    }
public:
    explicit WindowsOwnerWaitBackend(const OwnerWaitOptions& o) {
        validate_owner_wait_options(o);auto& r=resources_;auto& s=r.status;
        s.owner_thread_id=GetCurrentThreadId();s.priority_requested=o.above_normal_owner_thread;
        s.timer_resolution_requested=o.timer_resolution_1ms;
        r.timer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_MODIFY_STATE|SYNCHRONIZE);
        if(!r.timer)throw std::runtime_error("high-resolution owner timer unavailable");
        s.high_resolution_timer=true;
        r.thread=OpenThread(THREAD_QUERY_INFORMATION|THREAD_SET_INFORMATION,FALSE,s.owner_thread_id);
        if(!r.thread)throw std::runtime_error("owner thread policy handle unavailable");
        s.original_thread_priority=GetThreadPriority(r.thread);
        if(s.original_thread_priority==THREAD_PRIORITY_ERROR_RETURN)throw std::runtime_error("owner thread priority query failed");
        s.applied_thread_priority=s.original_thread_priority;
        if(o.above_normal_owner_thread&&s.original_thread_priority<THREAD_PRIORITY_ABOVE_NORMAL) {
            if(!SetThreadPriority(r.thread,THREAD_PRIORITY_ABOVE_NORMAL)) {
                s.priority_error=GetLastError();throw std::runtime_error("owner ABOVE_NORMAL thread priority unavailable");
            }
            s.priority_changed=true;s.applied_thread_priority=THREAD_PRIORITY_ABOVE_NORMAL;
        }
        s.applied_thread_priority=GetThreadPriority(r.thread);
        if(s.applied_thread_priority==THREAD_PRIORITY_ERROR_RETURN||
           (o.above_normal_owner_thread&&s.applied_thread_priority<THREAD_PRIORITY_ABOVE_NORMAL))
            throw std::runtime_error("owner thread priority readback failed");
        if(o.timer_resolution_1ms) {
            r.winmm=LoadLibraryExW(L"winmm.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
            if(r.winmm) {
                const auto begin=period_function(r.winmm,"timeBeginPeriod");r.end_period=period_function(r.winmm,"timeEndPeriod");
                if(begin&&r.end_period) {
                    s.timer_resolution_begin_result=begin(s.timer_resolution_ms);
                    s.timer_resolution_active=s.timer_resolution_begin_result==0;
                } else s.timer_resolution_load_error=ERROR_PROC_NOT_FOUND;
            } else s.timer_resolution_load_error=GetLastError();
        }
    }
    ~WindowsOwnerWaitBackend()override{finish();}
    void wait_timer(OwnerWaitNs duration)override {
        if(resources_.status.finished)throw std::logic_error("owner timer used after finish");
        LARGE_INTEGER relative{};relative.QuadPart=-(duration+99)/100;
        if(!SetWaitableTimer(resources_.timer,&relative,0,nullptr,nullptr,FALSE))throw std::runtime_error("owner timer arm failed");
        if(WaitForSingleObject(resources_.timer,100)!=WAIT_OBJECT_0)throw std::runtime_error("owner timer failed or exceeded100ms");
    }
    void spin_pause()override{YieldProcessor();}
    OwnerWaitPlatformStatus platform_status()const noexcept override{return resources_.status;}
    void finish()noexcept override{resources_.finish();}
};
struct OwnerWaitSample {
    std::uint64_t sequence=0,spin_iterations=0;OwnerWaitKind kind=OwnerWaitKind::target_due;
    OwnerWaitNs begin_ns=-1,target_ns=-1,input_due_ns=-1,planned_return_ns=-1,end_ns=-1;
    OwnerWaitNs timer_begin_ns=-1,timer_end_ns=-1,spin_begin_ns=-1,spin_end_ns=-1;
    OwnerWaitNs timer_request_ns=0,elapsed_ns=0,overshoot_ns=0,input_lateness_ns=0;
    bool critical_deadline=false,spin_iteration_limit_reached=false,failed=false;
};
struct OwnerWaitDiagnostics {
    OwnerWaitOptions options;OwnerWaitPlatformStatus platform;
    std::uint64_t calls=0,waits=0,timer_count=0,spin_count=0,no_wait_count=0,failed_count=0;
    std::uint64_t spin_iterations=0,spin_iteration_limit_reached=0;
    OwnerWaitNs spin_elapsed_ns=0,maximum_elapsed_ns=0,maximum_overshoot_ns=0;
    OwnerWaitSample last,last_timer,last_spin,maximum_elapsed,maximum_overshoot;
    std::array<OwnerWaitSample,16> recent{}; // Circular; sequence gives ordering.
    std::size_t recent_size=0,recent_next=0;
};
// An actual step <=50ms away forbids kernel timer waits. Each <=250us spin
// slice returns for owner stop/UI/tick checks, possibly occupying one CPU core
// during dense input. OS preemption remains possible; real lateness and UI
// freshness guards remain unchanged outside this app-local waiting policy.
class OwnerWait {
    const pas::Clock& clock_;std::unique_ptr<OwnerWaitBackend> backend_;
    OwnerWaitDiagnostics metrics_;bool finished_=false;
    void record(OwnerWaitSample sample)noexcept {
        auto& m=metrics_;sample.elapsed_ns=std::max<OwnerWaitNs>(0,sample.end_ns-sample.begin_ns);
        sample.overshoot_ns=std::max<OwnerWaitNs>(0,sample.end_ns-sample.planned_return_ns);
        if(sample.input_due_ns>=0)sample.input_lateness_ns=std::max<OwnerWaitNs>(0,sample.end_ns-sample.input_due_ns);
        ++m.calls;sample.sequence=m.calls;
        if(sample.kind==OwnerWaitKind::timer){++m.timer_count;m.last_timer=sample;}
        else if(sample.kind==OwnerWaitKind::spin) {
            ++m.spin_count;m.last_spin=sample;m.spin_iterations+=sample.spin_iterations;
            m.spin_elapsed_ns+=std::max<OwnerWaitNs>(0,sample.spin_end_ns-sample.spin_begin_ns);
            if(sample.spin_iteration_limit_reached)++m.spin_iteration_limit_reached;
        } else ++m.no_wait_count;
        if(sample.kind==OwnerWaitKind::timer||sample.kind==OwnerWaitKind::spin)++m.waits;
        if(sample.failed)++m.failed_count;
        if(!m.maximum_elapsed.sequence||sample.elapsed_ns>m.maximum_elapsed_ns){m.maximum_elapsed_ns=sample.elapsed_ns;m.maximum_elapsed=sample;}
        if(!m.maximum_overshoot.sequence||sample.overshoot_ns>m.maximum_overshoot_ns){m.maximum_overshoot_ns=sample.overshoot_ns;m.maximum_overshoot=sample;}
        m.last=sample;m.recent[m.recent_next]=sample;m.recent_next=(m.recent_next+1)%m.recent.size();
        m.recent_size=std::min(m.recent_size+1,m.recent.size());
        waits=m.waits;maximum_elapsed_ns=m.maximum_elapsed_ns;maximum_overshoot_ns=m.maximum_overshoot_ns;
    }
public:
    std::uint64_t waits=0;OwnerWaitNs maximum_elapsed_ns=0,maximum_overshoot_ns=0;
    explicit OwnerWait(const pas::Clock& clock,OwnerWaitOptions options={})
      :OwnerWait(clock,std::make_unique<WindowsOwnerWaitBackend>(options),options){}
    OwnerWait(const pas::Clock& clock,std::unique_ptr<OwnerWaitBackend> backend,OwnerWaitOptions options={})
      :clock_(clock),backend_(std::move(backend)) {
        validate_owner_wait_options(options);if(!backend_)throw std::invalid_argument("owner wait backend missing");
        metrics_.options=options;const auto initial=clock_.now_ns();(void)plan_owner_wait(initial,initial,std::nullopt,options);
    }
    ~OwnerWait(){finish();}
    OwnerWait(const OwnerWait&)=delete;OwnerWait& operator=(const OwnerWait&)=delete;
    void until(OwnerWaitNs target,std::optional<OwnerWaitNs> input_due=std::nullopt) {
        if(finished_)throw std::logic_error("owner wait used after finish");
        OwnerWaitSample sample;sample.begin_ns=clock_.now_ns();sample.target_ns=target;sample.input_due_ns=input_due.value_or(-1);
        if(metrics_.last.sequence&&sample.begin_ns<metrics_.last.end_ns)throw std::runtime_error("owner wait clock regressed between calls");
        auto plan=plan_owner_wait(sample.begin_ns,target,input_due,metrics_.options);
        sample.kind=plan.kind;sample.planned_return_ns=plan.planned_return_ns;
        sample.timer_request_ns=plan.timer_request_ns;sample.critical_deadline=plan.critical_deadline;
        auto latest=sample.begin_ns;
        const auto now=[&] {
            const auto value=clock_.now_ns();if(value<latest)throw std::runtime_error("owner wait clock regressed");
            latest=value;return value;
        };
        try {
            // A preempted planning stage must not enter a timer on its stale
            // decision after the real input deadline has become critical.
            if(plan.kind==OwnerWaitKind::timer)plan=plan_owner_wait(now(),target,input_due,metrics_.options);
            sample.kind=plan.kind;sample.planned_return_ns=plan.planned_return_ns;
            sample.timer_request_ns=plan.timer_request_ns;sample.critical_deadline=plan.critical_deadline;
            if(plan.kind==OwnerWaitKind::timer) {
                sample.timer_begin_ns=now();backend_->wait_timer(plan.timer_request_ns);sample.timer_end_ns=now();
            } else if(plan.kind==OwnerWaitKind::spin) {
                sample.spin_begin_ns=now();
                while(latest<plan.planned_return_ns&&sample.spin_iterations<metrics_.options.spin_iteration_limit) {
                    backend_->spin_pause();++sample.spin_iterations;now();
                }
                sample.spin_end_ns=latest;sample.spin_iteration_limit_reached=latest<plan.planned_return_ns;
            }
            sample.end_ns=now();record(sample);
        } catch(...) {
            sample.failed=true;
            try {now();}catch(...){}
            if(sample.kind==OwnerWaitKind::timer&&sample.timer_begin_ns>=0)sample.timer_end_ns=latest;
            if(sample.kind==OwnerWaitKind::spin&&sample.spin_begin_ns>=0)sample.spin_end_ns=latest;
            sample.end_ns=latest;record(sample);throw;
        }
    }
    // Call after release to persist confirmed rollback. Exceptional exits still
    // receive the same idempotent RAII cleanup from the destructor.
    void finish()noexcept {if(!finished_){backend_->finish();finished_=true;}}
    OwnerWaitDiagnostics diagnostics()const noexcept {auto result=metrics_;result.platform=backend_->platform_status();return result;}
};
} // namespace rhythm::experimental
