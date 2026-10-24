#include "rhythm/pas_bridge.hpp"
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace rhythm;
constexpr Ns ms = 1'000'000;
int checks = 0;
void require(bool ok, const std::string& label) {
    if (!ok) throw std::runtime_error(label);
    ++checks;
}
template<class F> void rejects(F f, const std::string& label) {
    bool caught = false;
    try { f(); } catch (const std::exception&) { caught = true; }
    require(caught, label);
}
Identity identity() {
    return {Game::phigros, "synthetic", "IN", "test-v1", std::string(64,'a'), std::string(64,'b'), "1280x720"};
}
Contact hold(std::uint64_t id, Ns start, Ns end, Ns interval = 0) {
    Contact contact{id, {{start, Phase::down, {.25,.5}}}};
    if (interval) {
        for (Ns t = start + interval; t < end; t += interval) {
            const double progress = static_cast<double>(t-start) / static_cast<double>(end-start);
            contact.steps.push_back({t, Phase::move, {.25+.5*progress,.5}});
        }
    }
    contact.steps.push_back({end, Phase::up, {.75,.5}});
    return contact;
}
Memory memory(std::vector<Contact> contacts) { return {identity(), std::move(contacts)}; }
ExecutionObservation observation(Ns host = 0, std::uint64_t sequence = 1, std::uint64_t epoch = 1) {
    return {identity(), {0,0,1}, {true,epoch,1,host,host+150*ms,ms}, sequence};
}
void run(const char* name, const std::function<void()>& test) {
    test(); std::cout << "PASS " << name << '\n';
}
class BrokenTouch final : public pas::TouchBackend {
public:
    explicit BrokenTouch(const pas::Clock& clock, bool fail_inject, bool fail_release)
        : clock_(clock), fail_inject_(fail_inject), fail_release_(fail_release) {}
    pas::TouchReceipt inject(const pas::TouchCommand& command) override {
        if (command.phase == pas::Phase::down) held_ = true;
        if (command.phase == pas::Phase::up) held_ = false;
        return {command,clock_.now_ns(),clock_.now_ns(),!fail_inject_,fail_inject_ ? "test_transport_failure" : ""};
    }
    pas::ReleaseReport release_all() override {
        pas::ReleaseReport result;
        if (held_) result.requested_ids.push_back(0);
        if (held_ && fail_release_) result.unknown_ids.push_back(0);
        if (!fail_release_) held_ = false;
        return result;
    }
private:
    const pas::Clock& clock_;
    bool fail_inject_, fail_release_, held_ = false;
};
}
int main() {
    try {
        run("long hold survives hundreds of rolling revisions with fixed pointer", [] {
            pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
            const auto song = memory({hold(77,0,2004*ms,4*ms)});
            PasBridge bridge(song,clock,touch);
            require(bridge.begin(observation()),"begin long hold");
            BridgeTick result;
            for (Ns t=0; t<=2004*ms; t+=ms) {
                clock.set(t);
                if (t && t%(40*ms)==0) require(bridge.update(observation(t,static_cast<std::uint64_t>(t/(40*ms)+1))),"fresh real observation");
                result=bridge.tick();
                require(result.pending<=1 && result.active<=1,"bounded plans/contact");
                require(result.running || result.finished,"long hold remains valid: "+result.reason);
            }
            require(result.finished && result.completed_contacts==1,"long hold finished");
            require(touch.receipts().size()==song.contacts[0].steps.size(),"all source steps exactly once");
            const int pointer=touch.receipts().front().command.contact_id;
            for (std::size_t i=0;i<touch.receipts().size();++i) {
                const auto& r=touch.receipts()[i];
                require(r.command.contact_id==pointer && r.command.scheduled_ns==song.contacts[0].steps[i].song_ns,"ownership/time preserved");
            }
            require(touch.contacts().empty() && !bridge.next_wake_ns(),"all released");
        });
        run("future chart is admitted only near its deadline", [] {
            pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
            PasBridge bridge(memory({hold(100,300*ms,310*ms)}),clock,touch);
            require(bridge.begin(observation()),"begin");
            auto result=bridge.tick(); require(result.pending==0 && touch.receipts().empty(),"no full-song admission");
            for(Ns t=50*ms;t<=200*ms;t+=50*ms) {
                clock.set(t); require(bridge.update(observation(t,static_cast<std::uint64_t>(t/(50*ms)+1))),"renew");
                result=bridge.tick();
            }
            require(result.pending==1 && touch.receipts().empty(),"100ms window admitted future");
        });
        run("fractional deadline UP drains without an invalid past-tail revision", [] {
            pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
            PasBridge bridge(memory({hold(1,428571,16*ms+428571)}),clock,touch);
            require(bridge.begin(observation()),"begin fractional tap");
            BridgeTick result;
            for(Ns t=0;t<=17*ms;t+=ms) {clock.set(t);result=bridge.tick();}
            require(result.finished && touch.receipts().size()==2,"slightly late UP completed");
            require(touch.receipts().back().command.scheduled_ns==16*ms+428571 &&
                    touch.receipts().back().injection_start_ns==17*ms,"deadline remains original, lateness observable");
        });
        run("small clock correction preserves committed down and revises future suffix", [] {
            pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
            PasBridge bridge(memory({hold(9,30*ms,90*ms,30*ms)}),clock,touch);
            require(bridge.begin(observation()),"begin"); bridge.tick();
            clock.set(25*ms); auto changed=observation(25*ms,2); changed.clock.host_anchor_ns=2*ms;
            require(bridge.update(changed),"phase update"); bridge.tick();
            clock.set(30*ms); bridge.tick();
            clock.set(60*ms); bridge.tick();
            require(touch.receipts().size()==1 && touch.receipts()[0].command.scheduled_ns==30*ms,"near-term down frozen");
            clock.set(62*ms); bridge.tick(); clock.set(92*ms); auto result=bridge.tick();
            require(result.finished && touch.receipts().size()==3,"suffix complete");
            require(touch.receipts()[1].command.scheduled_ns==62*ms && touch.receipts()[2].command.scheduled_ns==92*ms,"future corrected");
        });
        run("expiry releases held pointer and prevents late observation revival", [] {
            pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
            PasBridge bridge(memory({hold(1,0,1000*ms)}),clock,touch);
            require(bridge.begin(observation()),"begin"); bridge.tick();
            clock.set(150*ms);
            require(!bridge.update(observation(150*ms,2)),"fresh observation cannot repair expired old lease");
            auto result=bridge.tick();
            require(!result.running && !result.finished && result.reason=="previous_sync_expired","expired stop reason");
            require(touch.contacts().empty() && result.release.requested_ids.size()==1,"held pointer released");
            require(!bridge.begin(observation(150*ms,3)),"same epoch cannot rearm");
            require(bridge.begin(observation(150*ms,3,2)),"explicit newer epoch can arm");
        });
        run("planner ticks and duplicate IDs cannot renew evidence", [] {
            pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
            PasBridge bridge(memory({hold(1,0,1000*ms)}),clock,touch);
            auto initial=observation(); require(bridge.begin(initial),"begin"); bridge.tick();
            clock.set(100*ms); require(bridge.update(initial),"same exact observation is idempotent"); bridge.tick();
            clock.set(150*ms); auto result=bridge.tick();
            require(result.reason=="sync_expired" && touch.contacts().empty(),"tick did not create evidence");
            pas::FakeClock second_clock; pas::FakeTouchBackend second_touch(second_clock);
            PasBridge second(memory({hold(1,0,1000*ms)}),second_clock,second_touch);
            require(second.begin(initial),"second begin"); second.tick();
            second_clock.set(50*ms); initial.lease.expires_host_ns+=50*ms;
            require(!second.update(initial) && second.reason()=="observation_reused_with_new_data","lease laundering rejected");
        });
        run("identity geometry epoch and observation regressions revoke execution", [] {
            for(int mode=0;mode<5;++mode) {
                pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
                PasBridge bridge(memory({hold(1,0,1000*ms)}),clock,touch);
                require(bridge.begin(observation(0,2)),"begin"); bridge.tick();
                clock.set(20*ms); auto value=observation(20*ms,3);
                if(mode==0) value.identity.audio_sha256=std::string(64,'c');
                if(mode==1) value.lease.geometry_version=2;
                if(mode==2) value.lease.epoch=2;
                if(mode==3) value.sequence=1;
                if(mode==4) value.lease.uncertainty_ns=100*ms;
                require(!bridge.update(value) && touch.contacts().empty(),"mismatch stopped and released");
            }
        });
        run("expired trajectory cannot be hidden by refreshing its prefix window", [] {
            pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
            PasBridge bridge(memory({hold(1,0,100*ms,ms)}),clock,touch);
            require(bridge.begin(observation()),"begin"); bridge.tick();
            clock.set(10*ms); require(bridge.tick().running,"first slow tick within old coverage");
            const auto count=touch.receipts().size();
            clock.set(20*ms); auto result=bridge.tick();
            require(result.reason=="trajectory_coverage_expired" && !result.running,"old coverage deadline is hard");
            require(touch.receipts().size()==count && touch.contacts().empty(),"no blind catch-up after expiry");
        });
        run("late MOVE and host clock regression stop before dispatch", [] {
            pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
            PasBridge bridge(memory({hold(1,0,200*ms,50*ms)}),clock,touch);
            require(bridge.begin(observation()),"begin"); bridge.tick();
            clock.set(81*ms); auto result=bridge.tick();
            require(result.reason=="contact_step_too_late" && touch.receipts().size()==1,"late move rejected");
            pas::FakeClock c2; pas::FakeTouchBackend t2(c2);
            c2.set(10*ms); PasBridge b2(memory({hold(1,10*ms,200*ms)}),c2,t2);
            require(b2.begin(observation(10*ms)),"second begin"); b2.tick();
            c2.set(9*ms); result=b2.tick();
            require(result.reason=="host_clock_regressed" && t2.contacts().empty(),"backward host clock released");
        });
        run("backend failure and unknown release never report success or rearm", [] {
            pas::FakeClock clock; BrokenTouch touch(clock,true,true);
            PasBridge bridge(memory({hold(1,0,100*ms)}),clock,touch);
            require(bridge.begin(observation()),"begin"); auto result=bridge.tick();
            require(!result.running && !result.finished && !result.release.unknown_ids.empty(),"unknown release visible");
            require(!bridge.begin(observation(0,2,2)),"faulted backend cannot rearm");
        });
        run("user stop latch is sticky and cancels without additional dispatch", [] {
            pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
            PasBridge bridge(memory({hold(1,0,100*ms,10*ms)}),clock,touch);
            require(bridge.begin(observation()),"begin"); bridge.tick();
            bridge.request_stop(); clock.set(10*ms); auto result=bridge.tick();
            require(result.reason=="user_stop" && touch.contacts().empty() && touch.receipts().size()==1,"user stop released");
            require(!bridge.begin(observation(10*ms,2,2)),"new epoch does not reset user stop latch");
        });
        run("capacity 2 5 10 and time ties release move then down", [] {
            for(int capacity : {2,5,10}) {
                std::vector<Contact> contacts;
                for(int i=0;i<capacity;++i) contacts.push_back(hold(static_cast<std::uint64_t>(100-i),0,20*ms));
                pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
                PasBridgeConfig config; config.max_contacts=capacity;
                PasBridge bridge(memory(contacts),clock,touch,config);
                require(bridge.begin(observation()),"begin chord"); auto result=bridge.tick();
                require(result.active==static_cast<std::size_t>(capacity),"declared capacity accepted");
                contacts.push_back(hold(999,0,20*ms));
                rejects([&]{PasBridge invalid(memory(contacts),clock,touch,config);},"over capacity rejected");
            }
            pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
            auto song=memory({hold(99,0,20*ms),hold(5,0,40*ms,20*ms),hold(1,20*ms,30*ms)});
            PasBridge bridge(song,clock,touch);
            require(bridge.begin(observation()),"begin ties"); bridge.tick();
            clock.set(20*ms); auto result=bridge.tick();
            require(result.receipts.size()==3,"three simultaneous actions");
            require(result.receipts[0].command.phase==pas::Phase::up &&
                    result.receipts[1].command.phase==pas::Phase::move &&
                    result.receipts[2].command.phase==pas::Phase::down,"UP MOVE DOWN priority");
            require(bridge.scheduler().accepted_high_watermark()==3,"birth IDs independent of source ordering");
        });
        run("slow transport rechecks evidence between chord injections", [] {
            pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
            PasBridge bridge(memory({hold(1,0,300*ms),hold(2,0,300*ms)}),clock,touch);
            touch.on_touch=[&](const pas::TouchCommand&,Ns) { clock.set(200*ms); };
            require(bridge.begin(observation()),"begin"); auto result=bridge.tick();
            require(touch.receipts().size()==1 && !result.running && touch.contacts().empty(),"second chord contact not injected stale");
        });
        run("strict bounds prevent overflow and invalid normalized coordinates", [] {
            pas::FakeClock clock; pas::FakeTouchBackend touch(clock);
            auto song=memory({hold(1,0,100*ms)});
            auto config=PasBridgeConfig{}; config.max_steps=2;
            rejects([&]{PasBridge invalid(song,clock,touch,config);},"window too small");
            config=PasBridgeConfig{}; config.max_steps_in_song=1;
            rejects([&]{PasBridge invalid(song,clock,touch,config);},"source budget");
            song.contacts[0].steps[0].point.x=std::numeric_limits<double>::quiet_NaN();
            rejects([&]{PasBridge invalid(song,clock,touch);},"NaN point");
        });
        std::cout << checks << " checks passed\n"; return 0;
    } catch(const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
