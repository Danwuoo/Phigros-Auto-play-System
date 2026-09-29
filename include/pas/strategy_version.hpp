#pragma once

#include <string>

namespace pas {

// One source for strategy metadata emitted by every runtime entry point.
inline constexpr int game_observer_version = 48;
inline constexpr int game_planner_version = 25;
inline constexpr int game_diagnostics_version = 11;

inline std::string game_strategy_name() {
    return "main observer" + std::to_string(game_observer_version) +
           "/planner" + std::to_string(game_planner_version);
}

} // namespace pas
