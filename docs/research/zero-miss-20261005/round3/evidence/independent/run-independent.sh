#!/usr/bin/env bash
set -uo pipefail
if [[ $# != 5 ]]; then echo 'Usage: run-independent.sh REPO JSON_INCLUDE baseline|candidate release|debug|sanitizers NEW_OUTPUT_DIRECTORY' >&2; exit 2; fi
repo=$(realpath "$1"); dependency=$(realpath "$2"); implementation=$3; mode=$4; out=$5
case "$implementation" in baseline) source_dir=research/x10d_o_bvi_build;; candidate) source_dir=research/bvi_cold_v2;; *) exit 2;; esac
case "$mode" in release) flags=(-O2);; debug) flags=(-O0 -g -D_GLIBCXX_ASSERTIONS);; sanitizers) flags=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer);; *) exit 2;; esac
if [[ -e "$out" ]]; then echo 'Refusing existing output directory' >&2; exit 2; fi
mkdir -p "$out" || exit 2
out=$(realpath "$out"); build=$(mktemp -d /tmp/bvi-independent.XXXXXX)
cd "$repo" || exit 2
{
 date -u +%FT%TZ; uname -a; g++ --version; git rev-parse HEAD
 echo "Implementation=$implementation; configuration=$mode"
 echo 'Native cold C++20 only; no production integration or device operations.'
 echo 'Sanitizers use ASAN_OPTIONS=detect_leaks=0; no leak-freedom claim.'
 sha256sum "$source_dir/bvi.cpp" "$source_dir/bvi.hpp" research/bvi_cold_v2/independent_tests.cpp docs/research/zero-miss-20261005/round3/evidence/independent/expectations-frozen.json "$dependency/nlohmann/json.hpp"
} > "$out/provenance.txt"
sha256sum -c docs/research/zero-miss-20261005/round3/evidence/independent/expectations-frozen.sha256 > "$out/frozen-check.log" 2>&1 || exit 2
sha256sum -c docs/research/zero-miss-20261005/round3/evidence/independent/merge-controls-frozen.sha256 >> "$out/frozen-check.log" 2>&1 || exit 2
sha256sum "$source_dir/bvi.cpp" "$source_dir/bvi.hpp" research/bvi_cold_v2/independent_tests.cpp > "$out/sources-before.sha256"
cmd=(g++ -std=c++20 -Wall -Wextra -Wpedantic "${flags[@]}" -I "$dependency" -I "$source_dir" "-DBVI_HEADER=\"../$(basename "$source_dir")/bvi.hpp\"" research/bvi_cold_v2/independent_tests.cpp "$source_dir/bvi.cpp" -o "$build/review")
printf '%q ' timeout 120 "${cmd[@]}" > "$out/command.txt"; printf '\n' >> "$out/command.txt"
timeout 120 "${cmd[@]}" > "$out/build.log" 2>&1
compile_status=$?; printf '%s\n' "$compile_status" > "$out/build.exit"
if [[ $compile_status != 0 ]]; then exit 2; fi
sha256sum "$build/review" > "$out/binary.sha256"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 timeout 120 "$build/review" "$out/results.json" > "$out/stdout.log" 2> "$out/stderr.log"
native_status=$?
sha256sum "$source_dir/bvi.cpp" "$source_dir/bvi.hpp" research/bvi_cold_v2/independent_tests.cpp > "$out/sources-after.sha256"
cmp "$out/sources-before.sha256" "$out/sources-after.sha256" > "$out/source-stability.log" 2>&1 || exit 2
printf '%s\n' "$native_status" > "$out/native.exit"
if [[ $native_status != 0 && $native_status != 1 ]]; then exit 2; fi
jq '{case_count,assertions,failed_assertions,denominators,failures:[.cases[]|select(.pass==false)|{id,checks:[.checks[]|select(.pass==false)]}],decisions:[.cases[]|select(.class=="decision_only")]}' "$out/results.json" > "$out/summary.json" || exit 2
sha256sum "$out/results.json" > "$out/results.sha256"
expected_status=$(jq 'if .failed_assertions == 0 then 0 else 1 end' "$out/results.json")
if [[ $expected_status != "$native_status" ]]; then echo 'Native/report result mismatch' >&2; exit 2; fi
exit "$native_status"
