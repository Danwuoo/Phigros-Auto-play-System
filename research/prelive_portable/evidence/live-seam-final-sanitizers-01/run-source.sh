#!/usr/bin/env bash
# Fresh, exact-source Linux evidence only. No endpoint, native Windows or live gate.
# Parent must explicitly authorize source freeze before setting the launch flag.
set -euo pipefail
[[ $# == 3 ]] || { echo 'usage: run_live_seam_final.sh PINNED_JSON_INCLUDE MODE FRESH_EVIDENCE_PATH' >&2; exit 64; }
[[ ${PAS_LIVE_SEAM_SOURCE_FROZEN:-0} == 1 ]] || { echo 'STOP: parent source-freeze launch authorization required' >&2; exit 65; }
[[ ${PAS_PORTABLE_DETECT_LEAKS:-0} == 0 ]] || { echo 'STOP: this ptrace container requires explicit detect_leaks=0; no leak-free claim' >&2; exit 65; }
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(realpath "$here/../.."); dep=$(realpath "$1"); mode=$2; out=$(realpath -m "$3")
case "$out" in "$repo/research/prelive_portable/evidence/live-seam-final-"*) ;; *) echo 'fresh live-seam-final evidence directory required' >&2; exit 64;; esac
[[ ! -e "$out" ]] || { echo 'refusing existing evidence output' >&2; exit 64; }
case "$mode" in
 release) flags=(-O2 -DNDEBUG); reserve=$((64*1024*1024));;
 debug) flags=(-O0 -g0 -D_GLIBCXX_ASSERTIONS); reserve=$((128*1024*1024));;
 sanitizers) flags=(-O1 -g0 -fno-omit-frame-pointer -fsanitize=address,undefined); reserve=$((640*1024*1024));;
 *) exit 64;;
esac
floor=21474836480; write_margin=$((16*1024*1024))
available=$(df -B1 --output=avail "$repo" | tail -1 | tr -d ' ')
(( available >= floor+reserve+write_margin )) || { printf 'STOP capacity before evidence creation: available=%s floor=%s mode_reserve=%s write_margin=%s\n' "$available" "$floor" "$reserve" "$write_margin" >&2; exit 75; }
mkdir -p "$out"; cd "$repo"
exec > >(tee "$out/run.log") 2>&1
overall=0; stage=initialization
on_exit() { local code=$?; if ((code)); then printf 'STOP stage=%s exit=%s\n' "$stage" "$code" | tee "$out/STOP.txt"; printf '%s\n' "$code" > "$out/overall.exit"; fi; }
trap on_exit EXIT
capacity() {
 local need=$1; local label=$2; local free
 free=$(df -B1 --output=avail "$repo" | tail -1 | tr -d ' ')
 printf '%s\t%s\t%s\t%s\t%s\n' "$label" "$free" "$floor" "$need" "$write_margin" >> "$out/capacity.tsv"
 (( free >= floor+need+write_margin )) || { printf 'STOP capacity: stage=%s available=%s floor=%s reserve=%s write_margin=%s\n' "$label" "$free" "$floor" "$need" "$write_margin"; exit 75; }
}
printf 'stage\tavailable_bytes\tfloor_bytes\tstage_reserve_bytes\twrite_margin_bytes\n' > "$out/capacity.tsv"
capacity "$reserve" mode_start
printf '%s  %s\n' f2e1f3cac4e48fa851d6986291e4bd5b384d87f92c175d8b126cbd91580289cc "$dep/nlohmann/json.hpp" | sha256sum -c -
g++ --version; uname -a; git rev-parse HEAD
printf 'Linux only; ASan+UBSan detect_leaks=0. LeakSanitizer unsupported under ptrace; not leak-free.\n'
cp "$here/run_live_seam_final.sh" "$out/run-source.sh"; cp "$here/closure.sh" "$out/closure-source.sh"
source "$here/closure.sh"
tests+=(runtime_finish_contract:research/prelive_portable/runtime_finish_tests.cpp runtime_finish_rounds:research/prelive_portable/runtime_finish_pixels.cpp)
tests+=(active_body_contract:research/prelive_current/active_body_tests.cpp tail_guard_contract:research/prelive_current/tail_guard_tests.cpp)
tests+=(cost_workload_family:research/prelive_current/cost_workload_family.cpp cost_workload_family_v2:research/prelive_current/cost_workload_family_v2.cpp cost_tail_seam_probe:research/prelive_current/cost_tail_seam_probe.cpp)
tests+=(typed_tail_owner_review:research/live_readiness_review/tail_only_owner_review.cpp typed_tail_pixels_review:research/live_readiness_review/tail_only_pixels_review.cpp)
seam_source=${PAS_LIVE_SEAM_TEST_SOURCE:-research/prelive_current/disabled_live_seam_tests.cpp}
tests+=(disabled_live_seam_contract:"$seam_source")
for item in "${tests[@]}"; do [[ -f ${item#*:} ]] || { echo "STOP required test source missing: ${item#*:}"; exit 66; }; done
build=$(mktemp -d "$repo/out/prelive-live-seam-$mode.XXXXXX"); printf '%s\n' "$build" > "$out/build-directory.txt"
common=(-std=c++20 -pthread -ffunction-sections -fdata-sections -Wall -Wextra -Wpedantic "${flags[@]}"
 -I"$here/shim" -I"$repo/include" -I"$dep" -I"$repo/research/bvi_cold_v3"
 -I"$repo/research/bvi_windows/bridge" -I"$repo/research/hold_front_current" -I"$repo/research/prelive_current")
declare -p flags common > "$out/build-flags.txt"
sha256sum "$(readlink -f "$(command -v g++)")" "$(readlink -f "$(command -v ar)")" > "$out/toolchain.sha256"
selection=${PAS_LIVE_SEAM_SELECTION:-/workspace/shared/phigros-live-restored-20261006/PAS-zero-miss-r4-20261006/out/prelive-20261006/full-prefix-selection.json}
frame_root=${PAS_LIVE_SEAM_FRAME_ROOT:-/workspace/shared/phigros-live-restored-20261006/Phigros-Auto-play-System/measurements/game-assist/2026-09-30-m0-manual-continue/full-recording36g-01/sessions/manual-session-22885039263800/full-recording}
cards=${PAS_LIVE_SEAM_CARDS:-/workspace/shared/phigros-live-restored-20261006/PAS-zero-miss-r4-20261006/out/prelive-20261006/review-cards-02/cards.json}
png_headers=/workspace/shared/phigros-libpng-1.6.48-headers
png_lib=/usr/lib/x86_64-linux-gnu/libpng16.so.16.48.0
z_lib=/usr/lib/x86_64-linux-gnu/libz.so.1.3.1
before_family=research/prelive_current/workload_family_evidence/before-repair.json
inputs=("${sources[@]}" "$here/run_live_seam_final.sh" "$here/closure.sh" "$dep/nlohmann/json.hpp" "$before_family")
inputs+=(CMakeLists.txt CMakePresets.json vcpkg.json vcpkg-configuration.json src/manual_session.cpp apps/pas/main.cpp)
for item in "${tests[@]}"; do inputs+=("${item#*:}"); done
inputs+=(research/prelive_current/cost_workload_family_verify.cpp research/prelive_current/cost_workload_family_v2_verify.cpp "$here/verify_tail_family_goal.cpp")
inputs+=(research/live_readiness_review/live_seam_runtime_review.cpp research/live_readiness_review/live_seam_runtime_review_v2.cpp research/live_readiness_review/live_seam_packet_review.cpp research/live_readiness_review/live_seam_run_review.sh docs/research/live-readiness-20261006/review/live-seam/FIXED_PLAN.md)
inputs+=(docs/research/zero-miss-20261005/evidence/touch-scheduler/scheduler_probe.cpp docs/research/zero-miss-20261005/evidence/touch-scheduler/flick_move_probe.cpp)
inputs+=("$here/png_contract_tests.cpp" "$here/verify_review_samples.cpp" "$here/pixel_prefix.cpp" "$here/portable_png.hpp" "$here/shim/windows.h")
while IFS= read -r header; do inputs+=("$header"); done < <(find include/pas research/bvi_cold_v3 research/bvi_windows/bridge research/hold_front_current research/prelive_current -maxdepth 1 -type f -name '*.hpp' | sort)
inputs+=("$png_headers/png.h" "$png_headers/pngconf.h" "$png_headers/pnglibconf.h" "$png_lib" "$z_lib" "$selection" "$cards")
printf '%s\n' "${inputs[@]}" | sort -u > "$out/source-input-paths.txt"
mapfile -t inputs < "$out/source-input-paths.txt"
sha256sum "${inputs[@]}" > "$out/source-before.sha256"
capacity $((16*1024*1024)) source_snapshot
tar -czf "$out/source-snapshot.tar.gz" --files-from "$out/source-input-paths.txt" 2> "$out/source-snapshot.log"
sha256sum "$out/source-snapshot.tar.gz" > "$out/source-snapshot.sha256"
printf '%s  %s\n' a2f91ae9e121a713283a0017e7fef012f307cd8557b80c1b71f4f7e1d2aabe44 "$selection" | sha256sum -c -
if [[ -n ${PAS_LIVE_SEAM_FREEZE_MANIFEST:-} ]]; then sha256sum -c "$PAS_LIVE_SEAM_FREEZE_MANIFEST" > "$out/parent-freeze-before.log"; cp "$PAS_LIVE_SEAM_FREEZE_MANIFEST" "$out/parent-freeze.sha256"; fi
objects=()
for source in "${sources[@]}"; do
 stage="compile:$source"; capacity $((64*1024*1024)) "$stage"
 object="$build/${source//\//_}.o"; objects+=("$object")
 printf 'COMPILE %q\n' "$source"
 timeout 240s g++ "${common[@]}" -c "$source" -o "$object" > "$out/${source//\//_}.compile.log" 2>&1
done
stage=archive; capacity $((64*1024*1024)) "$stage"
ar rcs "$build/portable_current.a" "${objects[@]}"
sha256sum "${objects[@]}" > "$out/object-members.sha256"
# Exact archive-member receipts make these newly created object files recoverable.
# Preserve all source/failed artifacts; only successful own transient objects unlink.
for object in "${objects[@]}"; do
 expected=$(sha256sum "$object" | cut -d' ' -f1); actual=$(ar p "$build/portable_current.a" "$(basename "$object")" | sha256sum | cut -d' ' -f1)
 [[ $actual == "$expected" ]] || { echo "STOP archive-member mismatch: $object"; exit 74; }
 printf '%s  %s\n' "$actual" "$(basename "$object")" >> "$out/archive-member-byte-exact.sha256"
done
rm -- "${objects[@]}"
sha256sum "$build/portable_current.a" > "$out/archive.sha256"
test_stage_reserve=$((16*1024*1024)); [[ $mode != sanitizers ]] || test_stage_reserve=$((64*1024*1024))
build_test() {
 local name=$1 source=$2; shift 2
 stage="test_compile:$name"; capacity "$test_stage_reserve" "$stage"
 printf 'BUILD_TEST %s\n' "$name"
 if ! timeout 240s g++ "${common[@]}" "$source" "$build/portable_current.a" -Wl,--gc-sections -Wl,--strip-debug "$@" -o "$build/$name" > "$out/$name.compile.log" 2>&1; then echo COMPILE_FAILED > "$out/$name.status"; overall=1; return 1; fi
 sha256sum "$build/$name" >> "$out/test-binaries.sha256"
}
run_test() {
 local name=$1; shift
 stage="test_run:$name"; capacity $((16*1024*1024)) "$stage"
 sha256sum -c "$out/source-before.sha256" > "$out/$name.source-before-run.log"
 set +e
 ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 timeout 180s "$build/$name" "$@" > "$out/$name.stdout.log" 2> "$out/$name.stderr.log"
 local code=$?; set -e
 printf '%s\n' "$code" > "$out/$name.exit"; [[ $code == 0 ]] || overall=1
 printf 'TEST %s exit=%s\n' "$name" "$code"
}
for item in "${tests[@]}"; do
 name=${item%%:*}; source=${item#*:}
 if build_test "$name" "$source"; then run_test "$name" "$out/$name.json"; fi
done
for probe in scheduler flick_move; do
 source="docs/research/zero-miss-20261005/evidence/touch-scheduler/${probe}_probe.cpp"
 if build_test "$probe" "$source"; then run_test "$probe"; fi
done
if build_test family_v1_verify research/prelive_current/cost_workload_family_verify.cpp; then run_test family_v1_verify "$before_family" "$out/cost_workload_family.json" "$out/family_v1_verify.json"; fi
if build_test family_v2_verify research/prelive_current/cost_workload_family_v2_verify.cpp; then run_test family_v2_verify "$out/cost_workload_family_v2.json" "$out/family_v2_verify.json"; fi
if build_test normal_tail_goal "$here/verify_tail_family_goal.cpp"; then run_test normal_tail_goal "$out/cost_workload_family_v2.json" "$out/normal_tail_goal.json"; fi
if build_test png_contract "$here/png_contract_tests.cpp" -I"$png_headers" "$png_lib" "$z_lib"; then run_test png_contract "$frame_root/frames/frame-000001.png" "$out/png_contract.json"; fi
if build_test original_WIC_samples "$here/verify_review_samples.cpp" -I"$png_headers" "$png_lib" "$z_lib"; then run_test original_WIC_samples "$cards" "$frame_root" "$out/original_WIC_samples.json"; fi
stage=full_denominator_check
declare -a expected_checks=(original_owner_prefix:113 original_front_contract:56 current_rails_contract:68 own_prefix_contract:113 portable_independent:57 runtime_finish_contract:32 runtime_finish_rounds:12 active_body_contract:35 tail_guard_contract:15 cost_workload_tests:145 typed_tail_owner_review:130 typed_tail_pixels_review:57 family_v1_verify:104 family_v2_verify:104 normal_tail_goal:25)
printf 'group\texpected_assertions\tactual_assertions\tall_checks_pass\n' > "$out/full-denominators.tsv"
total=66
for item in "${expected_checks[@]}"; do
 name=${item%%:*}; expected=${item#*:}; actual=missing; all_pass=false
 if [[ -f "$out/$name.json" ]]; then
  actual=$(jq -r '.assertions' "$out/$name.json")
  if jq -e --argjson expected "$expected" '.assertions==$expected and (.checks|length)==$expected and all(.checks[]; .pass==true)' "$out/$name.json" >/dev/null; then all_pass=true; else overall=1; fi
 else overall=1; fi
 printf '%s\t%s\t%s\t%s\n' "$name" "$expected" "$actual" "$all_pass" >> "$out/full-denominators.tsv"
 total=$((total+expected))
done
grep -F 'RESULT cases=13 checks=66 failures=0;' "$out/scheduler.stdout.log" >/dev/null || overall=1
grep -F 'RESULT cases=1 failures=0;' "$out/flick_move.stdout.log" >/dev/null || overall=1
[[ $total == 1132 ]] || { echo 'STOP internal expected denominator mismatch'; exit 70; }
printf 'scheduler\t66\t66\t%s\n' "$(grep -Fq 'RESULT cases=13 checks=66 failures=0;' "$out/scheduler.stdout.log" && echo true || echo false)" >> "$out/full-denominators.tsv"
printf 'existing_matrix_assertions=%s\nflick_cases=1\nnew_seam_assertions=47\nnew_seam_assertions_report=%s\npng_controls_assertions=11\noriginal_WIC_samples=108\n' "$total" "$out/disabled_live_seam_contract.json" > "$out/declared-denominators.txt"
jq -e '.assertions==47 and (.checks|length)==47 and all(.checks[]; .pass==true) and .failed==0 and .device_endpoints==0 and .live_admission=="UNQUALIFIED" and .changed_Windows_branch_executed==false' "$out/disabled_live_seam_contract.json" >/dev/null || overall=1
for item in png_contract:11 original_WIC_samples:108; do
 name=${item%%:*}; expected=${item#*:}
 jq -e --argjson expected "$expected" '.assertions==$expected and (.checks|length)==$expected and all(.checks[]; .pass==true)' "$out/$name.json" >/dev/null || overall=1
done
if [[ ${PAS_LIVE_SEAM_RUN_PREFIX:-0} == 1 ]]; then
 [[ $mode == release ]] || { echo 'STOP: final original prefix runs once with the final Release archive'; exit 64; }
 [[ $overall == 0 ]] || { echo 'STOP: full matrix failed; no final original prefix'; exit 1; }
 stage=prefix_prepare; capacity $((64*1024*1024)) "$stage"
 prefix_out="$out/prefix-3063"; mkdir "$prefix_out"
 cp "$out/source-before.sha256" "$prefix_out/source-before.sha256"
 cp "$out/archive.sha256" "$prefix_out/archive.sha256"
 printf '%s\n' "$build" > "$prefix_out/build-directory.txt"
 if build_test pixel_prefix "$here/pixel_prefix.cpp" -I"$png_headers" "$png_lib" "$z_lib"; then
  sha256sum "$build/pixel_prefix" > "$prefix_out/binary.sha256"
  run_test pixel_prefix "$selection" "$frame_root" "$prefix_out/report.json"
  cp "$out/pixel_prefix.exit" "$prefix_out/run.exit"
  sha256sum -c "$out/source-before.sha256" > "$prefix_out/source-after-check.log" || overall=1
  if [[ -f "$prefix_out/report.json" ]]; then
   sha256sum "$prefix_out"/report.json* > "$prefix_out/raw-evidence.sha256"
   jq -e '.frames==3063 and .processed==3063 and .safe==true and .own_down==55 and .own_move==121 and .own_up==51 and .release_verified==true and .active_final==0 and .ledger_valid==true and .all_png_sha_verified==true and .device_endpoints==0' "$prefix_out/report.json" >/dev/null || overall=1
  else overall=1; fi
 fi
fi
stage=source_after; sha256sum -c "$out/source-before.sha256" > "$out/source-after-check.log" || overall=1
if [[ -n ${PAS_LIVE_SEAM_FREEZE_MANIFEST:-} ]]; then sha256sum -c "$PAS_LIVE_SEAM_FREEZE_MANIFEST" > "$out/parent-freeze-after.log" || overall=1; fi
capacity 0 mode_end
printf '%s\n' "$overall" > "$out/overall.exit"
exit "$overall"
