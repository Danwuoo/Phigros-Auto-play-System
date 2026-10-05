#!/usr/bin/env bash
set -uo pipefail
if [[ $# != 5 ]]; then echo 'Usage: run-supplemental.sh REPO JSON_INCLUDE SOURCE_DIR release|debug|sanitizers NEW_OUTPUT_DIRECTORY' >&2; exit 2; fi
repo=$(realpath "$1"); dependency=$(realpath "$2"); source_dir=$(realpath "$3"); mode=$4; out=$5
case "$mode" in release) flags=(-O2);; debug) flags=(-O0 -g -D_GLIBCXX_ASSERTIONS);; sanitizers) flags=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer);; *) exit 2;; esac
if [[ -e "$out" ]]; then echo 'Refusing existing output directory' >&2; exit 2; fi
mkdir -p "$out" || exit 2; out=$(realpath "$out"); build=$(mktemp -d /tmp/bvi-v3-supplemental.XXXXXX); cd "$repo" || exit 2
e=docs/research/zero-miss-20261005/round4/evidence/independent
sha256sum -c "$e/query-noise-matrix-frozen.sha256" > "$out/frozen-check.log" 2>&1 || exit 2
sha256sum -c "$e/query-accumulation-frozen.sha256" >> "$out/frozen-check.log" 2>&1 || exit 2
inputs=("$source_dir/bvi.cpp" "$source_dir/bvi.hpp" "$source_dir"/*_policy.cpp "$source_dir"/*_policy.hpp research/bvi_cold_v3/independent_v3_tests.cpp "$e/query-noise-matrix.cpp" "$e/query-accumulation-probe.cpp")
sha256sum "${inputs[@]}" > "$out/sources-before.sha256"
{ date -u +%FT%TZ; uname -a; g++ --version; echo 'Source-review supplement. Original 44 cases unchanged. LSan disabled; no leak-free claim.'; sha256sum "$dependency/nlohmann/json.hpp"; } > "$out/provenance.txt"
status=0
for test in query-noise-matrix query-accumulation-probe; do
 cmd=(g++ -std=c++20 -Wall -Wextra -Wpedantic "${flags[@]}" -I "$repo" -I "$dependency" "-DBVI_HEADER=\"$source_dir/bvi.hpp\"" "$e/$test.cpp" "$source_dir/bvi.cpp" "$source_dir"/*_policy.cpp -o "$build/$test")
 printf '%q ' timeout 120 "${cmd[@]}" > "$out/$test.command.txt"; printf '\n' >> "$out/$test.command.txt"
 timeout 120 "${cmd[@]}" > "$out/$test.build.log" 2>&1; compiled=$?; printf '%s\n' "$compiled" > "$out/$test.build.exit"; [[ "$compiled" == 0 ]] || exit 2
 sha256sum "$build/$test" > "$out/$test.binary.sha256"
 ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 timeout 120 "$build/$test" "$out/$test.json" > "$out/$test.stdout.log" 2> "$out/$test.stderr.log"; native=$?
 printf '%s\n' "$native" > "$out/$test.native.exit"; [[ "$native" == 0 || "$native" == 1 ]] || exit 2; [[ "$native" == 0 ]] || status=1
 done
sha256sum "${inputs[@]}" > "$out/sources-after.sha256"; cmp "$out/sources-before.sha256" "$out/sources-after.sha256" > "$out/source-stability.log" 2>&1 || exit 2
jq -n --slurpfile m "$out/query-noise-matrix.json" --slurpfile a "$out/query-accumulation-probe.json" '{supplemental_cases:5,legal_opportunities:2,legal_actions_observed:([$m[0].cases[]|select(.expected_down_count==1 and .actual.down_count==1)]|length)+(if $a[0].actual.down_count==1 then 1 else 0 end),safety_denials:3,unsafe_false_positives:([$m[0].cases[]|select(.expected_down_count==0 and .actual.down_count>0)]|length),failed_cases:$m[0].failed_cases+(if $a[0].pass then 0 else 1 end),matrix:$m[0],accumulation:$a[0]}' > "$out/summary.json" || exit 2
exit "$status"
