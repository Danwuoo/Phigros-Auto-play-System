#include "../apps/chart_manual/foreground_watch.hpp"
#include <atomic>
#include <future>
#include <iostream>

namespace {
using namespace rhythm::manual_ui;
constexpr WatchNs ms=1'000'000;
int checks=0;
void require(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
struct Gate {
    std::mutex mutex;std::condition_variable cv;bool entered=false,released=false,returned=false;
    void call() {std::unique_lock lock(mutex);entered=true;cv.notify_all();cv.wait(lock,[&]{return released;});returned=true;cv.notify_all();}
    void release() {std::lock_guard lock(mutex);released=true;cv.notify_all();}
    bool wait(bool completion=false) {std::unique_lock lock(mutex);return cv.wait_for(lock,std::chrono::seconds(1),[&]{return completion?returned:entered;});}
    ~Gate() {release();}
};
ForegroundWatchConfig quick() {ForegroundWatchConfig config;config.poll_interval_ns=ms;return config;}
using FakeNow=std::shared_ptr<std::atomic<WatchNs>>;
auto now_fn(const FakeNow& now) {return [now]{return now->load();};}
template<class Predicate> bool until(Predicate predicate) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
    while(std::chrono::steady_clock::now()<deadline) {if(predicate())return true;std::this_thread::yield();}
    return false;
}
}
int main() {
try {
    {
        auto gate=std::make_shared<Gate>();auto now=std::make_shared<std::atomic<WatchNs>>(100*ms);
        ForegroundWatch watch([gate]{gate->call();},now_fn(now),{90*ms,95*ms},quick());
        require(gate->wait(),"worker callback did not start");
        auto query=std::async(std::launch::async,[&watch]{return watch.status();});
        const bool ready=query.wait_for(std::chrono::milliseconds(100))==std::future_status::ready;
        gate->release();const auto status=query.get();
        require(ready,"owner status blocked behind callback");
        require(status.healthy&&status.in_progress&&status.successful_started_ns==90*ms,"pending callback renewed evidence");
        require(watch.stop(),"cooperative callback stop did not join");
    }
    {
        auto calls=std::make_shared<std::atomic<int>>(0);auto now=std::make_shared<std::atomic<WatchNs>>(100*ms);
        ForegroundWatch watch([calls]{++*calls;throw std::runtime_error("");},now_fn(now),{90*ms,95*ms},quick());
        require(until([&]{return watch.status().failed;}),"callback failure not observed");
        const auto failed=watch.status();*now=10'000*ms;const auto later=watch.status();
        require(!failed.healthy&&failed.reason.find("foreground_check_failed")==0,"empty exception treated as success");
        require(later.failed&&later.reason==failed.reason&&*calls==1,"failure retried or reason replaced");
        require(watch.stop(),"failed worker did not join");
    }
    {
        auto gate=std::make_shared<Gate>();auto now=std::make_shared<std::atomic<WatchNs>>(100*ms);
        ForegroundWatch watch([gate]{gate->call();},now_fn(now),{90*ms,95*ms},quick());
        require(gate->wait(),"stale test callback did not start");*now=4090*ms;
        const auto stale=watch.status();gate->release();require(watch.stop(),"stale worker failed to join");
        const auto after=watch.status();
        require(stale.stale&&stale.failed&&!stale.healthy&&stale.reason=="foreground_check_expired","exact stale boundary not rejected");
        require(after.successful_started_ns==90*ms&&after.reason==stale.reason,"late result revived stale watch");
    }
    {
        auto now=std::make_shared<std::atomic<WatchNs>>(100*ms);auto calls=std::make_shared<std::atomic<int>>(0);
        ForegroundWatch watch([calls]{++*calls;},now_fn(now),{90*ms,95*ms});
        require(watch.stop(),"waiting worker stop did not wake");
        require(watch.stop()&&*calls==0&&watch.status().stopped,"stop not idempotent or callback ran after stop");
    }
    // No owner status call occurs while this callback crosses the old deadline.
    // Exercise success immediately before, exactly at, and after expiry.
    for(WatchNs returned_ms:{3999LL,4000LL,5000LL}) {
        auto gate=std::make_shared<Gate>();auto now=std::make_shared<std::atomic<WatchNs>>(3000*ms);
        ForegroundWatch watch([gate]{gate->call();},now_fn(now),{0,2000*ms},quick());
        require(gate->wait(),"unpolled-expiry callback did not start");
        *now=returned_ms*ms;gate->release();
        require(watch.stop(),"unpolled-expiry worker did not join");
        const auto after=watch.status();
        if(returned_ms<4000) {
            require(!after.failed&&!after.stale&&after.successful_checks==2&&
                after.successful_started_ns==3000*ms,"pre-expiry result rejected");
        } else {
            require(after.failed&&after.stale&&after.reason=="foreground_check_expired"&&
                after.successful_checks==1&&after.successful_started_ns==0,
                "successful callback hid an expiry gap without owner polling");
        }
    }
    {
        auto gate=std::make_shared<Gate>();auto now=std::make_shared<std::atomic<WatchNs>>(100*ms);
        auto config=quick();config.stop_wait=std::chrono::milliseconds(10);
        ForegroundWatch watch([gate]{gate->call();},now_fn(now),{90*ms,95*ms},config);
        require(gate->wait(),"timeout test callback did not start");
        const auto started=std::chrono::steady_clock::now();const bool joined=watch.stop();
        const auto elapsed=std::chrono::steady_clock::now()-started;gate->release();
        require(!joined&&elapsed<std::chrono::milliseconds(250),"noncooperative stop was not bounded");
        require(gate->wait(true),"detached owned callback failed to finish after release");
        require(watch.status().failed&&!watch.status().stop_joined,"detached worker reported known cleanup");
    }
    for(bool rollback:{false,true}) {
        auto now=std::make_shared<std::atomic<WatchNs>>(100*ms);
        ForegroundWatch watch([now,rollback]{*now=rollback?99*ms:2101*ms;},now_fn(now),{90*ms,95*ms},quick());
        require(until([&]{return watch.status().failed;}),"clock/duration failure not rejected");
        require(!watch.status().healthy&&watch.stop(),"clock/duration worker did not stop");
    }
    std::cout<<checks<<" foreground watch checks passed; callback-controlled, no device.\n";return 0;
}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
