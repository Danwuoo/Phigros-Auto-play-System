#!/usr/bin/env bash
# Validate the separately frozen workload diagnosis against an already verified
# exact closure. Does not rebuild policy or reinterpret qualification=false.
set -euo pipefail
[[ $# == 4 ]] || { echo 'usage: run_workload.sh JSON_INCLUDE MODE VERIFIED_LANE FRESH_OUTPUT' >&2;exit 64; }
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd);repo=$(realpath "$here/../..")
dep=$(realpath "$1");mode=$2;prior=$(realpath "$3");out=$(realpath -m "$4")
[[ ! -e "$out" && $(cat "$prior/overall.exit") == 0 ]] || exit 64
case "$out" in "$here/evidence/"*) ;;*)exit 64;;esac
case "$mode" in release) flags=(-O2 -DNDEBUG);;debug)flags=(-O0 -g1 -D_GLIBCXX_ASSERTIONS);;sanitizers)flags=(-O1 -g1 -fno-omit-frame-pointer -fsanitize=address,undefined);;*)exit 64;;esac
cd "$repo";mkdir -p "$out";exec > >(tee "$out/run.log") 2>&1
sha256sum -c "$prior/source-before.sha256" > "$out/closure-source-check.log"
build=$(cat "$prior/build-directory.txt")
[[ -f "$build/portable_current.a" ]] || exit 64
sha256sum research/prelive_current/cost_workload.hpp research/prelive_current/cost_workload_tests.cpp "$build/portable_current.a" "$here/run_workload.sh" > "$out/source-before.sha256"
cp "$here/run_workload.sh" "$out/run-source.sh"
printf 'mode=%s prior_lane=%s detect_leaks=0 qualification_remains_false\n' "$mode" "$prior"
cmd=(g++ -std=c++20 -pthread -ffunction-sections -fdata-sections -Wall -Wextra -Wpedantic "${flags[@]}"
 -I"$here/shim" -Iinclude -I"$dep" -Iresearch/bvi_cold_v3 -Iresearch/hold_front_current -Iresearch/prelive_current
 research/prelive_current/cost_workload_tests.cpp "$build/portable_current.a" -Wl,--gc-sections -o "$build/cost_workload_final")
printf '%q ' "${cmd[@]}";printf '\n'
timeout 240s "${cmd[@]}" > "$out/compile.log" 2>&1
set +e
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 timeout 240s "$build/cost_workload_final" "$out/report.json" > "$out/stdout.log" 2> "$out/stderr.log"
code=$?;set -e;printf '%s\n' "$code" > "$out/test.exit"
sha256sum -c "$out/source-before.sha256" > "$out/source-after-check.log"
exit "$code"
