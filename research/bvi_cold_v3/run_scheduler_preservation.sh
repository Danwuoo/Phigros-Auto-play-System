#!/usr/bin/env bash
# Existing scheduler probe with the previously frozen Linux clock shim.
# Not a Windows owner, native QPC, process-control, transport or game test.
set -euo pipefail
[[ $# == 2 ]] || exit 64
repo=$(realpath "$1"); out=$(realpath -m "$2"); [[ ! -e "$out" ]] || exit 64
case "$out" in "$repo"/docs/research/zero-miss-20261005/round4/evidence/*) ;;*)exit 64;;esac
mkdir -p "$out";cd "$repo";work=$(mktemp -d /tmp/pas-v3-scheduler.XXXXXX);e=docs/research/zero-miss-20261005/evidence/touch-scheduler
exec > >(tee "$out/run.log") 2>&1
sha256sum src/core.cpp include/pas/core.hpp "$e"/{scheduler_probe.cpp,flick_move_probe.cpp,shim/windows.h} > "$out/source-before.sha256"
for mode in release debug sanitizers;do
 case "$mode" in release) flags=(-O2);;debug) flags=(-O0 -g -D_GLIBCXX_ASSERTIONS);;sanitizers) flags=(-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined);;esac
 for probe in scheduler flick_move;do
  cmd=(timeout 120s g++ -std=c++20 "${flags[@]}" -I "$e/shim" -I include src/core.cpp "$e/${probe}_probe.cpp" -o "$work/$mode-$probe")
  printf '%q ' "${cmd[@]}";printf '\n';"${cmd[@]}" > "$out/$mode-$probe.compile.log" 2>&1
  ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 timeout 30s "$work/$mode-$probe" > "$out/$mode-$probe.log" 2> "$out/$mode-$probe.stderr.log"
  printf '0\n' > "$out/$mode-$probe.exit"
 done
done
sha256sum -c "$out/source-before.sha256"
