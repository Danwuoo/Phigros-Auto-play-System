#pragma once
#include "pas/game.hpp"
#include <span>
#include <optional>
#include <string>
namespace pas::x10d_o {
struct Anchor {std::uint64_t id=0; NoteCandidate note; SceneContext context; Nanoseconds observed=0;};
struct Claim {bool pixel_support=false; double measured_depth=0; std::optional<std::uint64_t> owner; std::string reason="unknown";std::size_t probes=0;};
// A hypothesis boundary: no Note deletion, identity merge or owner mutation.
Claim resolve(const Frame&,const NoteCandidate&,const LineCandidate&,std::span<const Anchor>);
}
