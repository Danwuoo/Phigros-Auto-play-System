#!/usr/bin/env bash
# Independent integration review of small research probes, from repository root.
# No device, transport, emulator, model, product runtime or original suite is run.
set -euo pipefail
ROOT=docs/research/zero-miss-20261005/evidence
BUILD=$(mktemp -d /tmp/phigros-cloud-review.XXXXXX)
printf 'scope=Linux isolated research probes; original Windows suites and gameplay not run\n'
printf 'baseline=74e54437d4a3ad2b2bd1a3b09312211a92f2359e\n'
g++ --version | head -1
run() { printf '\nCOMMAND:'; printf ' %q' "$@"; printf '\n'; "$@"; printf 'EXIT=0\n'; }
run timeout 60s g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic \
  -Iresearch/x10d_o_bvi_build research/x10d_o_bvi_build/bvi.cpp \
  "$ROOT/build-audit/bvi_core_probe.cpp" -o "$BUILD/bvi-core"
run timeout 30s "$BUILD/bvi-core"
run timeout 60s g++ -std=c++20 -O2 -Wall -Wextra \
  -I "$ROOT/touch-scheduler/shim" -I include src/core.cpp \
  "$ROOT/touch-scheduler/scheduler_probe.cpp" -o "$BUILD/scheduler"
run timeout 30s "$BUILD/scheduler"
run timeout 60s g++ -std=c++20 -O2 -Wall -Wextra \
  "$ROOT/vision-timing/formula_probe.cpp" -o "$BUILD/formula"
run timeout 30s "$BUILD/formula"
run timeout 60s g++ -std=c++20 -O2 -Wall -Wextra \
  -Iresearch/x10d_o_bvi_build research/x10d_o_bvi_build/bvi.cpp \
  "$ROOT/vision-timing/bvi_stationary_probe.cpp" -o "$BUILD/bvi-stationary"
run timeout 30s "$BUILD/bvi-stationary"
run timeout 60s g++ -std=c++20 -O2 -Wall -Wextra \
  -Iresearch/x10d_o_bvi_build research/x10d_o_bvi_build/bvi.cpp \
  "$ROOT/vision-timing/bvi_rgb_stationary_probe.cpp" -o "$BUILD/bvi-rgb-stationary"
run timeout 30s "$BUILD/bvi-rgb-stationary"
run timeout 60s g++ -std=c++20 -O2 -Wall -Wextra \
  -I "$ROOT/touch-scheduler/shim" -I include src/core.cpp \
  "$ROOT/touch-scheduler/flick_move_probe.cpp" -o "$BUILD/flick-scheduler"
run timeout 30s "$BUILD/flick-scheduler"
run git diff --exit-code 74e54437d4a3ad2b2bd1a3b09312211a92f2359e \
  -- src include configs tests fixtures research tools CMakeLists.txt CMakePresets.json
printf '\nRESULT=PASS_FOR_LISTED_CLOUD_PROBES_ONLY\n'
