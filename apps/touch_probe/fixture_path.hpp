#pragma once
#include "pas/touch_evidence.hpp"
#include <algorithm>
#include <cstdint>
#include <map>

namespace pas::fixture_probe {
inline constexpr const char* path_adapter_version="stationary-snapshot-move-v1";
struct FixturePathCheck {
    PathCheck path;
    std::size_t redundant_stationary_moves=0;
};
// Probe-only adaptation of the fixture's per-pointer MOVE snapshots. Original
// evidence/counters remain unchanged and must be logged before calling this.
inline FixturePathCheck verify_fixture_path(const TouchEvidence& raw,
        const std::vector<ExpectedPointer>& expected,int tolerance,int final_active,
        const std::vector<int>& stationary_ids) {
    const auto failed=[](const char* reason) {return FixturePathCheck{{false,reason},0};};
    if(raw.samples.size()>64||expected.size()>64||stationary_ids.size()>32||tolerance<0||tolerance>20)
        return failed("fixture path adapter bound exceeded");
    const auto within_tolerance=[&](int x,int y,int anchor_x,int anchor_y) {
        const auto dx=static_cast<std::int64_t>(x)-anchor_x,dy=static_cast<std::int64_t>(y)-anchor_y;
        return dx>=-tolerance&&dx<=tolerance&&dy>=-tolerance&&dy<=tolerance;
    };
    struct Anchor {int expected_x=0,expected_y=0,observed_x=0,observed_y=0;bool seen=false,active=false;};
    std::map<int,Anchor> anchors;
    for(const int id:stationary_ids) {
        if(id<0||id>31||anchors.contains(id))return failed("invalid explicit stationary ID");
        const auto down=std::find_if(expected.begin(),expected.end(),[&](const auto& point) {
            return point.android_id==id&&point.phase==1;
        });
        if(down==expected.end())return failed("stationary ID lacks expected DOWN anchor");
        for(const auto& point:expected)if(point.android_id==id&&
                (point.phase==2||!within_tolerance(point.x,point.y,down->x,down->y)))
            return failed("stationary ID has requested motion or inconsistent anchor");
        anchors.emplace(id,Anchor{down->x,down->y});
    }
    TouchEvidence filtered=raw;filtered.samples.clear();
    std::size_t removed=0;
    for(const auto& sample:raw.samples) {
        const auto at=anchors.find(sample.android_id);
        if(at!=anchors.end()) {
            auto& anchor=at->second;
            if(sample.phase==1) {
                if(anchor.seen||!within_tolerance(sample.x,sample.y,anchor.expected_x,anchor.expected_y))
                    return failed("stationary DOWN invalid or repeated");
                anchor.seen=anchor.active=true;anchor.observed_x=sample.x;anchor.observed_y=sample.y;
            } else if(sample.phase==2) {
                if(!anchor.active)return failed("stationary MOVE without active observed DOWN");
                if(!within_tolerance(sample.x,sample.y,anchor.expected_x,anchor.expected_y)||
                   !within_tolerance(sample.x,sample.y,anchor.observed_x,anchor.observed_y))
                    return failed("stationary snapshot MOVE drifted");
                ++removed;continue; // Only this checked phase/ID may be omitted.
            } else if(sample.phase==3||sample.phase==4)anchor.active=false;
        }
        filtered.samples.push_back(sample); // Primary/unknown IDs and all other phases remain exact.
    }
    return {verify_touch_path(filtered,expected,tolerance,final_active,stationary_ids),removed};
}
} // namespace pas::fixture_probe
