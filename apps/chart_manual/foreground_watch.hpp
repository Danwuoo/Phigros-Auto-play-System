#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace rhythm::manual_ui {
using WatchNs=std::int64_t;
struct ForegroundWatchConfig {
    WatchNs poll_interval_ns=1'000'000'000,max_check_ns=2'000'000'000;
    // Conservative request-start age, independent of the UI frame's 100 ms age.
    // A slow check does not renew this deadline until it returns successfully.
    WatchNs stale_after_ns=4'000'000'000;
    std::chrono::milliseconds stop_wait{2500};
};
struct ForegroundSeed {WatchNs started_ns=-1,returned_ns=-1;}; // Actual successful preflight only.
struct ForegroundWatchStatus {
    bool healthy=false,failed=false,stale=false,in_progress=false,stopped=false,stop_joined=true;
    std::uint64_t successful_checks=1;
    WatchNs successful_started_ns=-1,successful_returned_ns=-1;
    std::string reason;
};

// Callback and clock must own their dependencies, remain bounded, and never touch
// UI lifecycle/journal. The clock callback must also be nonblocking: status calls
// it under its short state lock. Failure is sticky. Only this worker invokes check;
// status/stop have one owner. A timed-out stop detaches owned state, never a `this`
// pointer or CLI reference, and is reported as failure rather than known cleanup.
class ForegroundWatch {
    struct Shared {
        std::mutex mutex;std::condition_variable cv;
        std::function<void()> check;std::function<WatchNs()> now;ForegroundWatchConfig config;
        ForegroundWatchStatus result;
        bool stop=false,done=false;WatchNs last_owner_now=-1;
    };
    std::shared_ptr<Shared> shared_;
    std::thread worker_;
    static void fail(Shared& state,const std::string& reason) {
        if(!state.result.failed) {state.result.failed=true;state.result.reason=reason.substr(0,512);}
    }
    static bool expire(Shared& state,WatchNs now) {
        if(now>=state.result.successful_started_ns&&
           now-state.result.successful_started_ns>=state.config.stale_after_ns) {
            state.result.stale=true;fail(state,"foreground_check_expired");return true;
        }
        return false;
    }
    static void work(const std::shared_ptr<Shared>& state) noexcept {
        try {
            std::unique_lock lock(state->mutex);
            while(!state->cv.wait_for(lock,std::chrono::nanoseconds(state->config.poll_interval_ns),[&]{return state->stop||state->result.failed;})) {
                const auto started=state->now();
                if(started<state->result.successful_returned_ns) {fail(*state,"foreground_worker_clock_rollback");break;}
                if(expire(*state,started))break;
                state->result.in_progress=true;lock.unlock();
                std::string error;bool callback_failed=false;
                try {state->check();}catch(const std::exception& e) {callback_failed=true;error=e.what();}
                catch(...) {callback_failed=true;error="unknown callback failure";}
                const auto returned=state->now();lock.lock();state->result.in_progress=false;
                if(callback_failed)fail(*state,"foreground_check_failed: "+error);
                else if(returned<started||returned-started>state->config.max_check_ns)
                    fail(*state,"foreground_check_clock_or_duration_limit");
                // The owner may not have polled inside this expired interval.
                // A successful callback cannot repair that missing coverage.
                else if(expire(*state,returned)) {}
                else if(!state->result.failed) {
                    state->result.successful_started_ns=started;state->result.successful_returned_ns=returned;
                    ++state->result.successful_checks;
                }
                state->cv.notify_all();
                if(state->result.failed)break;
            }
            state->done=true;state->cv.notify_all();
        }catch(...) {
            std::lock_guard lock(state->mutex);fail(*state,"foreground_worker_exception");
            state->result.in_progress=false;state->done=true;state->cv.notify_all();
        }
    }
public:
    ForegroundWatch(std::function<void()> check,std::function<WatchNs()> now,
                    ForegroundSeed seed,ForegroundWatchConfig config={}):shared_(std::make_shared<Shared>()) {
        if(!check||!now||config.poll_interval_ns<=0||config.poll_interval_ns>1'000'000'000||
           config.max_check_ns<=0||config.max_check_ns>2'000'000'000||
           config.stale_after_ns<config.max_check_ns||config.stale_after_ns>10'000'000'000||
           config.stop_wait.count()<1||config.stop_wait.count()>3000||seed.started_ns<0||
           seed.returned_ns<seed.started_ns||seed.returned_ns-seed.started_ns>config.max_check_ns)
            throw std::invalid_argument("invalid foreground watch configuration or preflight seed");
        shared_->check=std::move(check);shared_->now=std::move(now);shared_->config=config;
        shared_->result.successful_started_ns=seed.started_ns;shared_->result.successful_returned_ns=seed.returned_ns;
        if(shared_->now()<seed.returned_ns)throw std::invalid_argument("foreground seed is in the future");
        worker_=std::thread([state=shared_]{work(state);});
    }
    ForegroundWatch(const ForegroundWatch&)=delete;
    ForegroundWatch& operator=(const ForegroundWatch&)=delete;
    ~ForegroundWatch() {
        try {(void)stop();}catch(...) {if(worker_.joinable())worker_.detach();}
    }
    ForegroundWatchStatus status() {
        std::lock_guard lock(shared_->mutex);const auto now=shared_->now();
        if(now<0||now<shared_->last_owner_now||now<shared_->result.successful_returned_ns)
            fail(*shared_,"foreground_owner_clock_rollback");
        shared_->last_owner_now=now;
        expire(*shared_,now);
        if(shared_->result.failed)shared_->cv.notify_all();
        auto result=shared_->result;result.stopped=shared_->stop;
        result.healthy=!result.failed&&!result.stopped;return result;
    }
    bool stop() {
        std::unique_lock lock(shared_->mutex);shared_->stop=true;shared_->cv.notify_all();
        if(!worker_.joinable())return shared_->result.stop_joined;
        if(!shared_->cv.wait_for(lock,shared_->config.stop_wait,[&]{return shared_->done;})) {
            shared_->result.stop_joined=false;fail(*shared_,"foreground_worker_stop_timeout");
            lock.unlock();worker_.detach();return false;
        }
        lock.unlock();worker_.join();return true;
    }
};
} // namespace rhythm::manual_ui
