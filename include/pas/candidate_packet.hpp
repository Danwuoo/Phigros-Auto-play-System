#pragma once
#include "pas/game_session.hpp"
#include <limits>
namespace pas {
// Complete batch copied from the same consumed observer frame. RGB stays in
// the existing three-slot physical pool through one immutable shared lease.
struct CandidatePacketEvidence final {
    std::shared_ptr<const Frame> frame;
    CandidateBatch batch;
    bool eligible(const SessionObservation& p,Nanoseconds now) const {
        if(!frame||p.status.new_round||p.status.ended||!p.status.active||p.status.state!=PlaySessionState::playing||!p.allow_down||!p.scene.playing_gate||p.evidence.result)return false;
        const auto& f=*frame;const SceneContext c{f.epoch,f.generation,f.geometry_version,f.sequence,f.capture_complete_ns,f.width,f.height,f.source_rotation};
        return f.source_valid&&batch.source_valid&&batch.capacity_valid&&p.scene.capacity_valid&&
            f.epoch==p.status.round&&c==batch.context&&c==p.scene.context&&
            f.epoch&&f.generation&&f.geometry_version&&f.sequence&&
            f.width==1280&&f.height==720&&f.stride==3840&&f.source_rotation==1&&f.rgb.size()==1280*720*3&&
            batch.candidates.size()<=128&&batch.lines.size()<=16&&p.scene.targets.size()<=128&&
            f.capture_complete_ns>=0&&f.capture_complete_ns<=std::numeric_limits<Nanoseconds>::max()-100'000'000&&
            f.capture_complete_ns<=f.pixels_ready_ns&&f.pixels_ready_ns<=now&&now-f.capture_complete_ns<100'000'000;
    }
};
}
