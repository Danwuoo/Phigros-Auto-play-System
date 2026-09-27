#pragma once
#include "pas/game.hpp"
namespace pas {
// Color-independent, paired current edge support; limited to an established
// Hold anchor. It cannot create a new Hold or infer an invisible tail.
std::optional<NoteCandidate> observe_held_outline(const Frame& frame,
    const NoteCandidate& anchor,const LineCandidate& line);
std::optional<NoteCandidate> observe_moving_held_front(const Frame& frame,
    const NoteCandidate& anchor,const LineCandidate& line);
}
