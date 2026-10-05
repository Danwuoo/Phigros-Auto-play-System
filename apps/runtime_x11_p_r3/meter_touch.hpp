#pragma once
#include "pas/core.hpp"
#include <algorithm>
#include <chrono>
#include <thread>
namespace r3 {
class MeterTouch final:public pas::TouchBackend {
public:
    const pas::Clock& clock;int delay;std::map<int,std::array<double,2>> contacts;
    std::vector<pas::TouchReceipt> receipts;std::vector<pas::ReleaseReport> release_calls;
    std::size_t peak=0;bool fail_release=false,unknown_release=false,unknown_down=false;
    MeterTouch(const pas::Clock& c,int d):clock(c),delay(d){receipts.reserve(8192);release_calls.reserve(8192);}
    pas::TouchReceipt inject(const pas::TouchCommand& c) override {
        if(receipts.size()>=8192)throw std::runtime_error("receipt_capacity");
        if(c.contact_id<0||c.contact_id>=5)throw std::runtime_error("contact_capacity");
        if((c.phase==pas::Phase::down)==contacts.contains(c.contact_id))throw std::runtime_error("contact_lifecycle");
        const auto begin=clock.now_ns();if(delay)std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        if(c.phase==pas::Phase::up)contacts.erase(c.contact_id);else contacts[c.contact_id]={c.x,c.y};
        peak=std::max(peak,contacts.size());
        const bool ok=!(unknown_down&&c.phase==pas::Phase::down);
        pas::TouchReceipt r{c,begin,clock.now_ns(),ok,ok?"fake_success":"fake_unknown"};receipts.push_back(r);return r;
    }
    pas::ReleaseReport release_all() override {
        if(release_calls.size()>=8192)throw std::runtime_error("release_capacity");
        pas::ReleaseReport r;r.start_ns=clock.now_ns();
        for(const auto& [id,p]:contacts){(void)p;r.requested_ids.push_back(id);if(fail_release)r.failed_ids.push_back(id);if(unknown_release)r.unknown_ids.push_back(id);}
        if(!fail_release&&!unknown_release)contacts.clear();
        r.return_ns=clock.now_ns();release_calls.push_back(r);return r;
    }
};
}
