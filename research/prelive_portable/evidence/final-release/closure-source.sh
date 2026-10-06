#!/usr/bin/env bash
# Sourced by run.sh. All production translation units compile unchanged.
sources=(
 src/core.cpp src/game.cpp src/game_motion.cpp src/game_tracking.cpp src/game_session.cpp
 research/bvi_cold_v3/bvi.cpp research/bvi_cold_v3/contact_policy.cpp
 research/bvi_cold_v3/relation_policy.cpp research/bvi_cold_v3/constraint_policy.cpp
 research/bvi_windows/bridge/bridge.cpp research/hold_front_current/front.cpp
 research/prelive_current/own_bridge.cpp research/prelive_current/current.cpp
)
tests=(
 original_owner_prefix:research/bvi_windows/bridge/bridge_tests.cpp
 original_front_contract:research/hold_front_current/front_tests.cpp
 current_rails_contract:research/prelive_current/current_tests.cpp
 own_prefix_contract:research/prelive_current/own_prefix.cpp
 portable_independent:research/prelive_portable/independent_tests.cpp
 cost_workload_tests:research/prelive_current/cost_workload_tests.cpp
)
