#pragma once
#include "pas/game.hpp"
#include <algorithm>
#include <cmath>
#include <span>

namespace pas::x10 {
// Exact main50 fallback-claim predicate, isolated for donor falsification.
// This is NOT a validated physical-identity rule. In particular, the original
// predicate does not compare body intervals, tangent agreement or rail pixels.
inline bool donor_fallback_claim(const NoteCandidate& note,const LineCandidate& line,
                                 std::span<const NoteCandidate> claimed) {
    return std::any_of(claimed.begin(),claimed.end(),[&](const NoteCandidate& used) {
        const Vec2 delta{note.center.x-used.center.x,note.center.y-used.center.y};
        return std::abs(note.width-used.width)<20&&
            std::abs(delta.x*line.tangent.x+delta.y*line.tangent.y)<=20;
    });
}
}
