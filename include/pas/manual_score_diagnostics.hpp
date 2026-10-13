#pragma once
#include "pas/session_archive.hpp"
#include <utility>

namespace pas {
template<class Factory>
void emit_manual_diagnostic(SessionArchive& archive,std::uint64_t round,bool score_only,Factory&& factory) {
    // Keep the archive's closed-state checks; skip discarded JSON construction.
    if(score_only)archive.event(round,nlohmann::json(nullptr));
    else archive.event(round,std::forward<Factory>(factory)());
}
}
