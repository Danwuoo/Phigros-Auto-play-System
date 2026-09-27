#pragma once
#include "pas/game.hpp"
namespace pas {
// Distributed current luminance ridges validate a long thin PCA line.
bool current_line_ridge_support(const Frame& frame,const LineCandidate& line);
std::optional<LineCandidate> observe_current_line_extent(const Frame& frame,const LineCandidate& seed);
// Color-independent, paired current edge support; limited to an established
// Hold anchor. It cannot create a new Hold or infer an invisible tail.
std::optional<NoteCandidate> observe_held_outline(const Frame& frame,
    const NoteCandidate& anchor,const LineCandidate& line);
std::optional<NoteCandidate> observe_moving_held_front(const Frame& frame,
    const NoteCandidate& anchor,const LineCandidate& line);
std::optional<NoteCandidate> observe_held_body_patch(const Frame& frame,
    const NoteCandidate& anchor,const LineCandidate& line);
}
