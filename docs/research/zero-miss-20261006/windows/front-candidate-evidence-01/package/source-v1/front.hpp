#pragma once
#include "pas/game.hpp"
#include <array>
#include <optional>
#include <span>
#include <string_view>

namespace pas::hold_front {
inline constexpr int radius=32, edge_radius=20, lane_count=5;
inline constexpr std::size_t candidate_cap=128, probes_per_candidate=65*15;
enum class Kind {unknown, visible_terminal, body_interior, clipped, unsupported};
enum class Reason {supported, frame_invalid, context_mismatch, capacity, geometry,
    body_not_head, unsupported_kind, rejected_proposal, normal_unknown,
    roi_clipped, no_terminal, multiple_terminals, seam_mismatch};
std::string_view name(Kind);
std::string_view name(Reason);
struct Pixel {
    int x=0,y=0;
    std::array<std::uint8_t,3> rgb{};
    bool operator==(const Pixel&) const = default;
};
struct Lane {
    double along=0;
    int last_inside_offset=0;
    Pixel inside,outside;
    bool operator==(const Lane&) const = default;
};
// Identity is frame-local. This is geometry evidence, never an execution token.
struct Result {
    SceneContext context;
    Nanoseconds pixels_ready_ns=0;
    std::uint64_t candidate_id=0;
    std::array<char,65> origin{};
    Vec2 observer_front{},normal{},tangent{};
    double width=0,offset_low=0,offset_high=0;
    std::optional<Vec2> boundary;
    Kind kind=Kind::unknown;
    Reason reason=Reason::no_terminal;
    bool current_tail_roi=false; // proposal only; not a measured tail boundary
    std::uint32_t probes=0;
    int left_rail_rows=0,right_rail_rows=0,terminal_clusters=0,support_anchor_offset=0;
    std::array<Lane,lane_count> lanes{};
};
struct Batch {
    SceneContext context;
    std::array<Result,candidate_cap> results{};
    std::size_t count=0;
    bool valid=false;
    Reason reason=Reason::frame_invalid;
    std::uint64_t probes=0;
};
Result measure(const Frame&,const SceneContext&,const TrackingCandidate&);
Batch produce(const Frame&,const CandidateBatch&);
struct Projection {
    Vec2 visible_boundary;
    double normal_interval_low=0,normal_interval_high=0;
    // Pixels prove a terminal edge; head role remains proposed. No owner API.
    bool head_role_confirmed=false,action_authorized=false;
};
// Rechecks the typed producer against the SAME current RGB and proposal.
// Neither a body patch nor a plausible edited coordinate can pass this seam.
std::optional<Projection> consume(const Frame&,const SceneContext&,
    const TrackingCandidate&,const Result&);
bool equivalent(const Result&,const Result&);
nlohmann::json encode(const Result&);
} // namespace pas::hold_front
