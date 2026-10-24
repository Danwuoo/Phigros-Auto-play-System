#include "rhythm/manual_lifecycle.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace rhythm;
constexpr Ns ms=1'000'000;
unsigned checks=0,groups=0;
void require(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
void run(const char* name,const std::function<void()>& test) {test();++groups;std::cout<<"PASS "<<name<<'\n';}
template<class Function> void rejects(Function action) {
    bool caught=false;try {action();}catch(const std::invalid_argument&) {caught=true;}
    require(caught,"invalid configuration accepted");
}
struct Feed {
    std::uint64_t seq=0;Ns time=1'000*ms;
    ManualObservation next(bool hud=false,bool result=false,bool menu=false,bool pause=false,Ns delta=20*ms) {
        time+=delta;ManualObservation observation;
        observation.sequence=++seq;observation.generation=2;observation.geometry_version=7;
        observation.width=1280;observation.height=720;observation.rotation=1;
        observation.timestamp_kind=ManualTimestampKind::arrival_only;
        observation.received_host_ns=time;observation.evidence_id="synthetic-frame-"+std::to_string(seq);
        observation.source_valid=true;observation.hud=hud;observation.result=result;
        observation.menu=menu;observation.pause=pause;return observation;
    }
    ManualStatus send(ManualLifecycle& lifecycle,bool hud=false,bool result=false,bool menu=false,bool pause=false,Ns delta=20*ms) {
        const auto observation=next(hud,result,menu,pause,delta);return lifecycle.observe(observation,time);
    }
    ManualStatus playing(ManualLifecycle& lifecycle) {
        send(lifecycle,true);send(lifecycle,true);return send(lifecycle,true);
    }
    ManualStatus finish(ManualLifecycle& lifecycle) {
        send(lifecycle,false,true,false,false,30*ms);
        send(lifecycle,false,true,false,false,30*ms);
        return send(lifecycle,false,true,false,false,30*ms);
    }
};
}
int main() {
try {
    run("configuration limits and names",[] {
        for(unsigned value:{0u,21u})rejects([&]{ManualConfig config;config.round_limit=value;ManualLifecycle lifecycle(config);});
        for(Ns value:{Ns{-1},3'600'000*ms+1})rejects([&]{ManualConfig config;config.round_watchdog_ns=value;ManualLifecycle lifecycle(config);});
        for(Ns value:{Ns{0},Ns{-1},1'000*ms+1})rejects([&]{ManualConfig config;config.max_observation_age_ns=value;ManualLifecycle lifecycle(config);});
        for(Ns value:{Ns{0},10'000*ms+1})rejects([&]{ManualConfig config;config.max_continuity_gap_ns=value;ManualLifecycle lifecycle(config);});
        require(std::string(manual_state_name(ManualState::playing))=="PLAYING","state name");
        require(std::string(manual_timestamp_kind_name(ManualTimestampKind::arrival_only))=="arrival_only","timestamp kind name");
    });
    run("standby never starts by time or missing notes",[] {
        ManualLifecycle lifecycle;Feed feed;
        for(int index=0;index<1000;++index) {
            const auto status=feed.send(lifecycle,false,false,false,false,1'000*ms);
            require(!status.active&&!status.ui_playing&&status.round==0&&!status.session_done,"standby self-started");
        }
    });
    run("first HUD starts round and three frames require elapsed time",[] {
        ManualLifecycle lifecycle;Feed feed;
        auto status=feed.send(lifecycle,true,false,false,false,ms);
        require(status.new_round&&status.active&&status.state==ManualState::starting&&status.round==1,"first HUD round");
        require(!status.ui_playing,"first HUD armed UI");
        status=feed.send(lifecycle,true,false,false,false,ms);
        status=feed.send(lifecycle,true,false,false,false,ms);
        require(status.state==ManualState::starting&&!status.ui_playing,"three rapid frames passed 20ms");
        status=feed.send(lifecycle,true,false,false,false,18*ms);
        require(status.state==ManualState::playing&&status.ui_playing&&!status.new_round,"20ms distinct HUD failed");
    });
    run("poll is not observation and expiry happens at exact boundary",[] {
        ManualLifecycle lifecycle;Feed feed;auto status=feed.playing(lifecycle);
        const auto sequence=status.observation_sequence;const auto observed=status.observed_host_ns;
        for(Ns delta:{Ns{0},10*ms,99*ms}) {
            status=lifecycle.status(feed.time+delta);
            require(status.ui_playing&&status.observation_sequence==sequence&&status.observed_host_ns==observed,"poll renewed or revoked fresh frame");
        }
        status=lifecycle.poll(feed.time+100*ms);
        require(!status.ui_playing&&status.active&&!status.ended&&status.state==ManualState::suspended,"age boundary not suspended");
        require(status.observation_sequence==sequence&&status.observed_host_ns==observed,"expiry fabricated observation");
    });
    run("poll cannot complete HUD or result debounce",[] {
        ManualLifecycle lifecycle;Feed feed;feed.send(lifecycle,true);
        for(int index=0;index<15;++index)require(!lifecycle.poll(feed.time+index*ms).ui_playing,"timer advanced HUD");
        feed.playing(lifecycle);auto status=feed.send(lifecycle,false,true);
        for(int index=0;index<80;++index) {
            status=lifecycle.status(feed.time+index*ms);
            require(!status.result_confirmed&&!status.ended&&status.completed_rounds==0,"timer confirmed result");
        }
    });
    run("result candidate immediately suspends and 60ms confirmation is bounded",[] {
        ManualLifecycle lifecycle;Feed feed;feed.playing(lifecycle);
        auto status=feed.send(lifecycle,false,true,false,false,ms);
        require(!status.ui_playing&&status.active&&!status.ended,"first result did not suspend");
        feed.send(lifecycle,false,true,false,false,29*ms);
        status=feed.send(lifecycle,false,true,false,false,30*ms);
        require(status.active&&!status.result_confirmed,"result confirmed before 60ms");
        status=feed.send(lifecycle,false,true,false,false,ms);
        require(status.state==ManualState::result&&status.result_confirmed&&status.ended&&!status.active&&
            status.completed_rounds==1&&status.session_done,"final confirmed result missing");
        status=lifecycle.status(feed.time+ms);
        require(!status.ended&&status.session_done&&status.result_confirmed,"result edge repeated or result lost");
        feed.send(lifecycle,false,false,true);status=feed.playing(lifecycle);
        require(status.round==1&&!status.active&&status.session_done,"one-round limit bypassed");
    });
    run("result debounce resets on unknown and pause",[] {
        for(bool pause:{false,true}) {
            ManualLifecycle lifecycle;Feed feed;feed.playing(lifecycle);
            feed.send(lifecycle,false,true);feed.send(lifecycle,false,true);
            feed.send(lifecycle,false,false,false,pause);
            auto status=feed.send(lifecycle,false,true,false,false,60*ms);
            require(!status.result_confirmed&&status.active,"non-result did not reset debounce");
        }
    });
    run("multi-round latch needs actual menu or non-HUD transition",[] {
        ManualConfig config;config.round_limit=3;ManualLifecycle lifecycle(config);Feed feed;
        feed.playing(lifecycle);auto status=feed.finish(lifecycle);
        require(status.completed_rounds==1&&!status.session_done,"first multi round ended session");
        for(int index=0;index<6;++index)feed.send(lifecycle,false,true);
        for(int index=0;index<5;++index) {
            status=feed.send(lifecycle,true);require(!status.active&&status.round==1,"stray HUD bypassed result latch");
        }
        feed.send(lifecycle,false,false,false,true);
        status=feed.send(lifecycle,true);require(!status.active,"pause rearmed result");
        feed.send(lifecycle,false,false,true);status=feed.playing(lifecycle);
        require(status.ui_playing&&status.round==2,"manual menu failed to rearm");
        feed.finish(lifecycle);feed.send(lifecycle);status=feed.playing(lifecycle);
        require(status.ui_playing&&status.round==3,"unknown loading transition failed rearm");
        status=feed.finish(lifecycle);require(status.session_done&&status.completed_rounds==3,"round bound missing");
    });
    run("pause HUD loss and long gaps suspend same round and require fresh debounce",[] {
        ManualLifecycle lifecycle;Feed feed;feed.playing(lifecycle);
        for(bool pause:{true,false}) {
            auto status=feed.send(lifecycle,false,false,false,pause);
            require(status.active&&!status.ended&&!status.ui_playing&&status.round==1,"pause ended round");
            status=feed.send(lifecycle,true);require(!status.ui_playing&&!status.new_round,"resume skipped debounce");
            feed.send(lifecycle,true);status=feed.send(lifecycle,true);require(status.ui_playing,"resume HUD failed");
        }
        const auto status=feed.send(lifecycle,true,false,false,false,251*ms);
        require(status.active&&!status.ui_playing&&status.round==1,"long gap bridged HUD continuity");
    });
    run("same sequence cannot refresh frame or complete result",[] {
        ManualLifecycle lifecycle;Feed feed;feed.playing(lifecycle);
        const auto result=feed.next(false,true);auto status=lifecycle.observe(result,feed.time);
        for(Ns delta:{20*ms,40*ms,60*ms}) {
            status=lifecycle.observe(result,feed.time+delta);
            require(!status.ui_playing&&!status.result_confirmed&&status.completed_rounds==0&&
                status.observation_sequence==result.sequence,"same sequence refreshed evidence");
        }
    });
    run("same evidence ID or capture time cannot add support",[] {
        for(bool same_id:{false,true}) {
            ManualLifecycle lifecycle;Feed feed;const auto first=feed.next(true);lifecycle.observe(first,feed.time);
            auto next=feed.next(true);if(same_id)next.evidence_id=first.evidence_id;else next.received_host_ns=first.received_host_ns;
            auto status=lifecycle.observe(next,feed.time);
            require(!status.ui_playing&&status.observation_sequence==first.sequence,"non-distinct frame advanced evidence");
            status=feed.send(lifecycle,true);require(!status.ui_playing,"non-distinct debounce retained");
        }
    });
    run("nonadjacent source-frame replay cannot complete HUD or result",[] {
        for(bool result:{false,true}) {
            ManualLifecycle lifecycle;Feed feed;if(result)feed.playing(lifecycle);
            const auto first=feed.next(!result,result);lifecycle.observe(first,feed.time);
            feed.send(lifecycle,!result,result,false,false,30*ms);
            auto repeated=feed.next(!result,result,false,false,30*ms);repeated.evidence_id=first.evidence_id;
            auto status=lifecycle.observe(repeated,feed.time);
            require(!status.ui_playing&&!status.result_confirmed&&status.active&&
                status.completed_rounds==0&&status.reason=="replayed_frame_provenance","A/B/A source replay passed debounce");
            status=feed.send(lifecycle,!result,result);require(!status.ui_playing&&!status.result_confirmed,"replay failed to reset support");
        }
    });
    run("filled counter with insufficient span retains every pending source ID",[] {
        for(bool result:{false,true}) {
            ManualLifecycle lifecycle;Feed feed;if(result)feed.playing(lifecycle);
            const auto first=feed.next(!result,result,false,false,ms);lifecycle.observe(first,feed.time);
            for(int index=0;index<5;++index)feed.send(lifecycle,!result,result,false,false,ms);
            auto repeated=feed.next(!result,result,false,false,result?55*ms:15*ms);repeated.evidence_id=first.evidence_id;
            const auto status=lifecycle.observe(repeated,feed.time);
            require(!status.ui_playing&&!status.result_confirmed&&status.reason=="replayed_frame_provenance",
                "full counter discarded first support ID before time threshold");
        }
    });
    run("pending support budget fails closed while confirmed HUD remains bounded",[] {
        ManualLifecycle lifecycle;Feed feed;
        feed.send(lifecycle,true,false,false,false,1);
        for(int index=1;index<64;++index)feed.send(lifecycle,true,false,false,false,1);
        auto status=feed.send(lifecycle,true,false,false,false,1);
        require(!status.ui_playing&&status.reason=="debounce_support_budget","pending support overflow not rejected");
        status=feed.playing(lifecycle);require(status.ui_playing,"support overflow not recoverable with fresh evidence");
        for(int index=0;index<100;++index) {
            status=feed.send(lifecycle,true);require(status.ui_playing,"bounded confirmed HUD ring interrupted normal source");
        }
    });
    run("replayed provenance cannot renew playing or bridge another UI class",[] {
        for(bool result:{false,true}) {
            ManualLifecycle lifecycle;Feed feed;const auto first=feed.next(true);lifecycle.observe(first,feed.time);
            feed.send(lifecycle,true);auto before=feed.send(lifecycle,true);
            if(result)before=feed.send(lifecycle,false,true);
            auto repeated=feed.next(!result,result);repeated.evidence_id=first.evidence_id;
            const auto status=lifecycle.observe(repeated,feed.time);
            require(!status.ui_playing&&status.active&&!status.result_confirmed&&
                status.observed_host_ns==before.observed_host_ns&&status.received_host_ns==before.received_host_ns&&
                status.observation_sequence==before.observation_sequence,"replayed frame refreshed provenance or result support");
        }
    });
    run("mapped capture checks actual age and retains clock provenance",[] {
        ManualLifecycle lifecycle;Feed feed;auto observation=feed.next(true);
        observation.timestamp_kind=ManualTimestampKind::mapped_capture;observation.capture_host_ns=feed.time-99*ms;
        auto status=lifecycle.observe(observation,feed.time);
        require(status.new_round&&status.capture_timestamp_known&&status.observed_host_ns==observation.capture_host_ns&&
            status.received_host_ns==feed.time&&status.evidence_id==observation.evidence_id,"mapped provenance lost");
        status=lifecycle.poll(feed.time+ms);require(!status.ui_playing&&status.state==ManualState::suspended,"mapped age used receipt");
        ManualLifecycle stale;observation.capture_host_ns=feed.time-100*ms;
        status=stale.observe(observation,feed.time);require(!status.active&&status.round==0,"stale mapped frame started round");
    });
    run("arrival-only has explicit unknown capture timestamp",[] {
        ManualLifecycle lifecycle;Feed feed;const auto status=feed.playing(lifecycle);
        require(status.timestamp_kind==ManualTimestampKind::arrival_only&&!status.capture_timestamp_known&&
            status.observed_host_ns==status.received_host_ns,"arrival-only pretended capture mapping");
    });
    run("unknown and malformed observation contracts fault closed",[] {
        const std::function<void(ManualObservation&)> mutations[]={
            [](auto& o){o.sequence=0;},[](auto& o){o.generation=0;},[](auto& o){o.geometry_version=0;},
            [](auto& o){o.width=1;},[](auto& o){o.height=16385;},[](auto& o){o.rotation=90;},
            [](auto& o){o.timestamp_kind=ManualTimestampKind::unknown;},[](auto& o){o.evidence_id.clear();},
            [](auto& o){o.evidence_id=std::string(513,'x');},[](auto& o){o.pause=true;},
            [](auto& o){o.received_host_ns=-1;},[](auto& o){++o.received_host_ns;},
            [](auto& o){o.capture_host_ns=o.received_host_ns;},
            [](auto& o){o.timestamp_kind=ManualTimestampKind::mapped_capture;},
            [](auto& o){o.timestamp_kind=ManualTimestampKind::mapped_capture;o.capture_host_ns=o.received_host_ns+1;}
        };
        for(const auto& mutation:mutations) {
            ManualLifecycle lifecycle;Feed feed;auto observation=feed.next(true);mutation(observation);
            const auto status=lifecycle.observe(observation,feed.time);
            require(status.state==ManualState::fault&&status.session_done&&!status.ui_playing&&!status.active,"malformed observation accepted");
        }
    });
    run("invalid source does not contribute confirmation",[] {
        ManualLifecycle lifecycle;Feed feed;feed.playing(lifecycle);auto observation=feed.next(true);
        observation.source_valid=false;auto status=lifecycle.observe(observation,feed.time);
        require(status.active&&!status.ui_playing&&!status.ended,"invalid source not suspended");
        status=feed.send(lifecycle,true);require(!status.ui_playing,"invalid source retained HUD support");
    });
    run("source geometry and timestamp-kind changes are terminal",[] {
        const std::function<void(ManualObservation&)> changes[]={
            [](auto& o){++o.generation;},[](auto& o){++o.geometry_version;},[](auto& o){++o.width;},
            [](auto& o){++o.height;},[](auto& o){o.rotation=0;},
            [](auto& o){o.timestamp_kind=ManualTimestampKind::mapped_capture;o.capture_host_ns=o.received_host_ns;}
        };
        for(const auto& change:changes) {
            ManualLifecycle lifecycle;Feed feed;feed.playing(lifecycle);auto observation=feed.next(true);change(observation);
            auto status=lifecycle.observe(observation,feed.time);
            require(status.state==ManualState::fault&&status.ended&&status.session_done&&!status.active,"source change continued round");
            status=feed.playing(lifecycle);require(!status.active&&status.round==1,"source fault not sticky");
        }
    });
    run("host or source time and sequence rollback fault closed",[] {
        for(int variant=0;variant<4;++variant) {
            ManualLifecycle lifecycle;Feed feed;feed.playing(lifecycle);auto observation=feed.next(true);
            ManualStatus status;
            if(variant==0)status=lifecycle.poll(feed.time-21*ms);
            else {
                if(variant==1)observation.sequence=1;
                if(variant==2)observation.received_host_ns-=21*ms;
                if(variant==3) {observation.timestamp_kind=ManualTimestampKind::mapped_capture;observation.capture_host_ns=observation.received_host_ns;}
                status=lifecycle.observe(observation,feed.time);
            }
            require(status.state==ManualState::fault&&!status.ui_playing&&!status.active,"rollback accepted");
        }
        ManualLifecycle lifecycle;require(lifecycle.poll(-1).state==ManualState::fault,"negative now accepted");
    });
    run("watchdog abort has no result and cannot start another round",[] {
        ManualConfig config;config.round_watchdog_ns=80*ms;config.round_limit=20;
        ManualLifecycle lifecycle(config);Feed feed;const auto first=feed.send(lifecycle,true);
        auto status=lifecycle.poll(first.observed_host_ns+79*ms);require(status.active,"watchdog early");
        status=lifecycle.status(first.observed_host_ns+80*ms);
        require(status.state==ManualState::aborted&&status.ended&&status.session_done&&
            !status.result_confirmed&&status.completed_rounds==0,"watchdog mistaken for result");
        feed.time=first.observed_host_ns+80*ms;status=feed.playing(lifecycle);require(!status.active&&status.round==1,"watchdog abort rearmed");
        config.round_watchdog_ns=0;ManualLifecycle unlimited(config);Feed other;other.playing(unlimited);
        status=other.send(unlimited,false,false,false,true,4'000'000*ms);
        require(status.active&&!status.ended&&!status.ui_playing,"disabled watchdog ended pause");
    });
    run("sticky user stop including concurrent request",[] {
        ManualLifecycle lifecycle;Feed feed;feed.playing(lifecycle);
        std::thread stopper([&]{lifecycle.request_user_stop();});stopper.join();
        auto status=lifecycle.poll(feed.time);
        require(status.state==ManualState::stopped&&status.ended&&status.session_done&&!status.active,"user stop failed");
        status=feed.playing(lifecycle);require(status.state==ManualState::stopped&&!status.ended&&status.round==1,"user stop rearmed");
        ManualLifecycle waiting;waiting.request_user_stop();Feed other;status=other.playing(waiting);
        require(status.round==0&&status.state==ManualState::stopped,"stop in standby started round");
    });
    run("cleanup stop cannot erase first terminal fault abort or result",[] {
        for(int outcome=0;outcome<3;++outcome) {
            ManualConfig config;config.round_watchdog_ns=200*ms;
            ManualLifecycle lifecycle(config);Feed feed;ManualStatus before;
            if(outcome==0)before=lifecycle.poll(-1);
            else if(outcome==1) {const auto first=feed.send(lifecycle,true);before=lifecycle.poll(first.observed_host_ns+200*ms);}
            else {feed.playing(lifecycle);before=feed.finish(lifecycle);}
            require(before.session_done,"expected terminal outcome missing");
            require(before.state==(outcome==0?ManualState::fault:outcome==1?ManualState::aborted:ManualState::result),
                "terminal test did not reach its intended outcome");
            lifecycle.request_user_stop();const auto after=lifecycle.poll(-1);
            require(after.state==before.state&&after.reason==before.reason&&
                after.result_confirmed==before.result_confirmed&&after.completed_rounds==before.completed_rounds&&
                !after.ended&&!after.new_round,"cleanup erased first terminal outcome");
        }
    });
    run("already elapsed watchdog is checked on first mapped HUD",[] {
        ManualConfig config;config.round_watchdog_ns=10*ms;ManualLifecycle lifecycle(config);Feed feed;
        auto observation=feed.next(true);observation.timestamp_kind=ManualTimestampKind::mapped_capture;
        observation.capture_host_ns=feed.time-20*ms;const auto status=lifecycle.observe(observation,feed.time);
        require(status.state==ManualState::aborted&&!status.ui_playing&&!status.active&&status.session_done,
            "first mapped HUD ignored elapsed watchdog");
    });
    std::cout<<groups<<" groups / "<<checks<<" checks passed; portable UI lifecycle only.\n";
    return 0;
}catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
