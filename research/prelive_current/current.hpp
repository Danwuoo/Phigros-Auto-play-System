#pragma once
#include "own_bridge.hpp"
#include "front.hpp"
namespace pas::current_rails {
struct Support {
    std::uint64_t note=0,line=0,candidate=0;
    Vec2 hit{},front{};
    bool terminal=false,body=false,effect=false,unique=false,active=false,accepted=false,tail=false;
    int rows=0,samples=0;
    std::string_view reason="insufficient_current_support";
};
struct Evaluation {
    DecisionSnapshot filtered;
    std::array<Support,128> support{};
    std::size_t count=0,accepted=0,raw=0,unknown=0;
    bool valid=false;
    std::uint64_t probes=0;
};
// One current lease, 128 identities x six measured local distances / <=90ms.
// No device, annotation, frame ordinal, song, past keys or future-frame API.
class Hook {
    struct Point {Nanoseconds time=0;double distance=0;Vec2 front{};};
    struct Track {std::uint64_t note=0,line=0;std::array<Point,6> points{};int count=0;};
    std::array<Track,128> tracks_{};
    SceneContext context_{};
    bvi::Candidate tap_;
public:
    Evaluation evaluate(const Frame&,const CandidateBatch&,const DecisionSnapshot&,
        const current_execution::ExecutionLedger&,Nanoseconds);
    void reset(){tracks_={};context_={};tap_=bvi::Candidate{};}
    static constexpr std::size_t storage_bytes(){return sizeof(Hook);}
};
}
