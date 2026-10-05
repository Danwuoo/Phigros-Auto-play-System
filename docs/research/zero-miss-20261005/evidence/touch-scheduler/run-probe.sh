#!/usr/bin/env bash
# Run from repository root. Never constructs transport, emulator or game runtime.
set -euo pipefail
E=docs/research/zero-miss-20261005/evidence/touch-scheduler
BUILD=$(mktemp -d /tmp/phigros-scheduler-probe.XXXXXX)
printf '%s\n' "$BUILD" > "$E/build-path.txt"
g++ -std=c++20 -O2 -Wall -Wextra -I "$E/shim" -I include src/core.cpp "$E/scheduler_probe.cpp" -o "$BUILD/probe" 2> "$E/build-rerun.log"
"$BUILD/probe" | tee "$E/results-rerun.txt"
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I "$E/shim" -I include src/core.cpp "$E/scheduler_probe.cpp" -o "$BUILD/probe-asan-ubsan" 2> "$E/build-sanitizers.log"
"$BUILD/probe-asan-ubsan" > "$E/results-sanitizers.txt" 2> "$E/sanitizers.log"
sha256sum src/core.cpp include/pas/core.hpp "$E/scheduler_probe.cpp" "$E/shim/windows.h" "$E/run-probe.sh" "$BUILD/probe" "$BUILD/probe-asan-ubsan" > "$E/verified-sha256.txt"
