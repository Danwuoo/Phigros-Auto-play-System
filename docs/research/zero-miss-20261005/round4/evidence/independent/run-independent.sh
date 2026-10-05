#!/usr/bin/env bash
set -uo pipefail
if [[ $# != 5 ]]; then echo 'Usage: run-independent.sh REPO JSON_INCLUDE baseline|candidate release|debug|sanitizers NEW_OUTPUT_DIRECTORY' >&2; exit 2; fi
repo=$(realpath "$1"); dependency=$(realpath "$2"); impl=$3; mode=$4; out=$5
case "$impl" in baseline) source_dir=research/bvi_cold_v2;; candidate) source_dir=research/bvi_cold_v3;; *) exit 2;; esac
case "$mode" in release) flags=(-O2);; debug) flags=(-O0 -g -D_GLIBCXX_ASSERTIONS);; sanitizers) flags=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer);; *) exit 2;; esac
if [[ -e "$out" ]]; then echo 'Refusing existing output directory' >&2; exit 2; fi
mkdir -p "$out" || exit 2; out=$(realpath "$out"); build=$(mktemp -d /tmp/bvi-v3-independent.XXXXXX)
cd "$repo" || exit 2
e=docs/research/zero-miss-20261005/round4/evidence/independent
{
 echo "6d894b7a13837a156e0818f1e56577d3cbf1a91e6d61df319458502af076eff2  $e/expectations-frozen.json"
 echo "f3af8682c3ce81b53579130b3ff02e5f60eb1bc4a5d186b00f6f7660ba69edc0  $e/addendum-expectations-frozen.json"
 echo "cb3e5420b317936192b4e2641d2b8995da44070b689f386b62ea0c0a1f59deb4  $e/pre-candidate-tests.cpp"
 echo "c302f8d280df58feff84f60d6456fc43d184474b46bab946ec44f4b07267b664  research/bvi_cold_v3/independent_v3_tests.cpp"
} | sha256sum -c > "$out/frozen-check.log" 2>&1 || exit 2
sources=("$source_dir/bvi.cpp"); inputs=("$source_dir/bvi.cpp" "$source_dir/bvi.hpp" research/bvi_cold_v3/independent_v3_tests.cpp)
for p in "$source_dir"/*_policy.cpp; do [[ -f "$p" ]] && sources+=("$p") && inputs+=("$p"); done
for p in "$source_dir"/*_policy.hpp; do [[ -f "$p" ]] && inputs+=("$p"); done
{
 date -u +%FT%TZ; uname -a; g++ --version; git rev-parse HEAD
 echo "implementation=$impl configuration=$mode"
 echo 'Independent current-RGB fake constraints only. v3 extract annotates legal current relation context internally.'
 echo 'No explicit relation annotation, canonical renderer, candidate private helper, production owner or device.'
 echo 'ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1; no LSan/leak-free claim.'
 sha256sum "${inputs[@]}" "$dependency/nlohmann/json.hpp"
} > "$out/provenance.txt"
sha256sum "${inputs[@]}" > "$out/sources-before.sha256"
cmd=(g++ -std=c++20 -Wall -Wextra -Wpedantic "${flags[@]}" -I "$dependency" -I "$source_dir" "-DBVI_HEADER=\"../$(basename "$source_dir")/bvi.hpp\"" research/bvi_cold_v3/independent_v3_tests.cpp "${sources[@]}" -o "$build/review")
printf '%q ' timeout 120 "${cmd[@]}" > "$out/command.txt"; printf '\n' >> "$out/command.txt"
timeout 120 "${cmd[@]}" > "$out/build.log" 2>&1; compiled=$?; printf '%s\n' "$compiled" > "$out/build.exit"
if [[ $compiled != 0 ]]; then exit 2; fi
sha256sum "$build/review" > "$out/binary.sha256"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 timeout 120 "$build/review" "$out/results.json" > "$out/stdout.log" 2> "$out/stderr.log"; native=$?
printf '%s\n' "$native" > "$out/native.exit"
sha256sum "${inputs[@]}" > "$out/sources-after.sha256"
cmp "$out/sources-before.sha256" "$out/sources-after.sha256" > "$out/source-stability.log" 2>&1 || exit 2
if [[ $native != 0 && $native != 1 ]]; then exit 2; fi
jq '{case_count,assertions,failed_assertions,legal_action_opportunities,legal_actions_observed,overrejections,safety_denial_opportunities,unsafe_false_positives,denominators,failures:[.cases[]|select(.pass==false)|{id,checks:[.checks[]|select(.pass==false)]}],decisions:[.cases[]|select(.class=="decision_only")]}' "$out/results.json" > "$out/summary.json" || exit 2
jq -r '.cases[].id' "$out/results.json" | sort > "$out/result-case-ids.txt"
{ jq -r '.positive_groups[][],.negative_groups[][],.metamorphic[],.decision_only[]' "$e/expectations-frozen.json"; jq -r '.cases[].id' "$e/addendum-expectations-frozen.json"; } | sort > "$out/expected-case-ids.txt"
cmp "$out/result-case-ids.txt" "$out/expected-case-ids.txt" > "$out/coverage-check.log" 2>&1 || exit 2
sha256sum "$out/results.json" > "$out/results.sha256"
expected=$(jq 'if .failed_assertions == 0 then 0 else 1 end' "$out/results.json")
if [[ "$expected" != "$native" ]]; then echo 'Native/report mismatch' >&2; exit 2; fi
exit "$native"
