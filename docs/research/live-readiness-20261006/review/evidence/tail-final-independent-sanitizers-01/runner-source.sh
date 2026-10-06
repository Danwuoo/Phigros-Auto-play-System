#!/usr/bin/env bash
# Finite independent Linux contracts. Never a Windows or physical qualification.
# Archive and its source manifest come from a verified same-ABI portable build.
set -euo pipefail
[[ $# == 6 ]] || { echo 'usage: run_tail_reviews.sh JSON_INCLUDE ARCHIVE SOURCE_MANIFEST MODE FRESH_OUTPUT EXPECTED_ARCHIVE_SHA256' >&2;exit 64; }
repo=$(cd "$(dirname "$0")/../.." && pwd)
dep=$(realpath "$1");archive=$(realpath "$2");manifest=$(realpath "$3");mode=$4;out=$(realpath -m "$5");expected=$6
[[ ! -e "$out" ]] || { echo 'fresh output required' >&2;exit 64; }
[[ $expected =~ ^[0-9a-f]{64}$ ]] || exit 64
case $mode in
 release) flags=(-O2 -DNDEBUG);;
 debug) flags=(-O0 -g0 -D_GLIBCXX_ASSERTIONS);;
 sanitizers) flags=(-O1 -g0 -fno-omit-frame-pointer -fsanitize=address,undefined);;
 *) exit 64;;
esac
mkdir -p "$out";cd "$repo"
build=$(mktemp -d "$repo/out/independent-tail-${mode}.XXXXXX")
printf '%s\n' "$build" > "$out/build-directory.txt"
exec > >(tee "$out/run.log") 2>&1
printf '%s  %s\n' "$expected" "$archive" | sha256sum -c -
printf '%s  %s\n' f2e1f3cac4e48fa851d6986291e4bd5b384d87f92c175d8b126cbd91580289cc "$dep/nlohmann/json.hpp" | sha256sum -c -
sha256sum -c "$manifest" > "$out/closure-before.log"
cp "$manifest" "$out/closure-manifest.sha256"
cp research/live_readiness_review/run_tail_reviews.sh "$out/runner-source.sh"
sha256sum "$archive" "$dep/nlohmann/json.hpp" research/live_readiness_review/tail_only_owner_review.cpp research/live_readiness_review/tail_only_pixels_review.cpp > "$out/test-inputs.sha256"
printf 'mode=%s\nLinux only; leak detection=%s\n' "$mode" "${PAS_PORTABLE_DETECT_LEAKS:-0}"
overall=0
for kind in owner pixels;do
 g++ -std=c++20 -pthread -ffunction-sections -fdata-sections "${flags[@]}" \
   -Iresearch/prelive_portable/shim -Iinclude -I"$dep" -Iresearch/bvi_cold_v3 \
   -Iresearch/bvi_windows/bridge -Iresearch/hold_front_current -Iresearch/prelive_current \
   "research/live_readiness_review/tail_only_${kind}_review.cpp" "$archive" \
   -Wl,--gc-sections -o "$build/$kind" > "$out/$kind.compile.log" 2>&1
 sha256sum -c "$manifest" > "$out/$kind.closure-before-run.log"
 set +e
 ASAN_OPTIONS=detect_leaks=${PAS_PORTABLE_DETECT_LEAKS:-0}:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
   timeout 180s "$build/$kind" "$out/$kind.json" > "$out/$kind.stdout.log" 2> "$out/$kind.stderr.log"
 code=$?;set -e;printf '%s\n' "$code" > "$out/$kind.exit"
 [[ $code == 0 ]] || overall=1
 printf '%s exit=%s\n' "$kind" "$code"
done
sha256sum -c "$manifest" > "$out/closure-after.log"
sha256sum -c "$out/test-inputs.sha256" > "$out/test-inputs-after.log"
sha256sum "$build/owner" "$build/pixels" > "$out/test-binaries.sha256"
printf '%s\n' "$overall" > "$out/overall.exit"
exit "$overall"
