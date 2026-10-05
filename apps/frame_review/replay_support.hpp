#pragma once
#include "pas/game_session.hpp"
#include <nlohmann/json.hpp>
#include <functional>
#include <set>
#include <stdexcept>
namespace pas::x1 {
using json=nlohmann::json;
inline json command_json(const TouchCommand& c) {
    return {{"intent_id",c.intent_id},{"contact_id",c.contact_id},{"phase",static_cast<int>(c.phase)},
        {"x",c.x},{"y",c.y},{"scheduled_ns",c.scheduled_ns},{"source_frame",c.source_frame_sequence}};
}
inline json release_json(const ReleaseReport& r) {
    return {{"requested_ids",r.requested_ids},{"failed_ids",r.failed_ids},{"unknown_ids",r.unknown_ids},
        {"start_ns",r.start_ns},{"return_ns",r.return_ns}};
}
// No receipt history vector. Synchronous consumers drain one event at a time.
class ReplayTouch final:public TouchBackend {
public:
    explicit ReplayTouch(FakeClock& c):clock(c){}
    FakeClock& clock;
    std::map<int,std::array<double,2>> contacts;
    std::function<void(json)> sink;
    std::size_t count=0,bytes=0,peak_contacts=0,limit_count=100000,limit_bytes=32*1024*1024;
    std::optional<std::size_t> fail_at;
    bool unknown_release=false,failed_release=false;
    ReleaseReport last_release_report;
    Nanoseconds delay_ns=0;
    void event(json j) {
        const auto size=j.dump().size();
        if(count>=limit_count||bytes+size>limit_bytes)throw std::runtime_error("receipt_event_capacity");
        ++count;bytes+=size;if(sink)sink(std::move(j));
    }
    TouchReceipt inject(const TouchCommand& c) override {
        // Reserve before applying a command, including the unknown-receipt case.
        if(count+1>=limit_count||bytes+1024>limit_bytes)throw std::runtime_error("receipt_event_capacity");
        if(c.contact_id<0||c.contact_id>=5)throw std::runtime_error("fake_contact_id");
        if(c.phase==Phase::down&&contacts.contains(c.contact_id))throw std::runtime_error("duplicate_down");
        if(c.phase!=Phase::down&&!contacts.contains(c.contact_id))throw std::runtime_error("unknown_contact");
        const auto start=clock.now_ns();clock.set(start+delay_ns);
        if(c.phase==Phase::up)contacts.erase(c.contact_id);else contacts[c.contact_id]={c.x,c.y};
        peak_contacts=std::max(peak_contacts,contacts.size());
        TouchReceipt r{c,start,clock.now_ns(),!fail_at||count!=*fail_at,"fake_success"};
        if(!r.success)r.reason="fake_unknown_transport_result";
        event({{"event","fake_receipt"},{"command",command_json(c)},{"injection_start_ns",r.injection_start_ns},
            {"injection_return_ns",r.injection_return_ns},{"success",r.success},{"reason",r.reason},{"contacts_after",contacts}});
        return r;
    }
    ReleaseReport release_all() override {
        ReleaseReport r;r.start_ns=clock.now_ns();
        for(const auto& [id,_]:contacts){r.requested_ids.push_back(id);if(unknown_release)r.unknown_ids.push_back(id);if(failed_release)r.failed_ids.push_back(id);}
        contacts.clear();r.return_ns=clock.now_ns();
        last_release_report=r;
        // The terminal report also has a reserved count slot and byte bound.
        event({{"event","fake_release"},{"report",release_json(r)}});return r;
    }
};
// Shared event-loop rule. A tie frame runs first unless due-first was requested.
inline void advance_before_frame(SessionGameOwner& owner,FakeClock& clock,Nanoseconds time,bool due_first,
    const std::function<void()>& drain) {
    if(time<clock.now_ns())throw std::runtime_error("replay_time_regression");
    std::size_t iterations=0;
    while(owner.scheduler()) {
        const auto due=owner.scheduler()->next_due_ns();
        if(!due||*due>time||(!due_first&&*due==time))break;
        if(++iterations>100000||*due<clock.now_ns())throw std::runtime_error("scheduler_progress_or_time_guard");
        clock.set(*due);owner.poll();drain();
        const auto next=owner.scheduler()->next_due_ns();
        if(next&&*next<=clock.now_ns())throw std::runtime_error("scheduler_no_progress");
    }
    clock.set(time);
}
}
