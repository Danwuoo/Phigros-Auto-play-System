#!/usr/bin/env bash
# Isolated Linux cold research only. Never changes the historical attempt or inputs.
set -euo pipefail
if [[ $# != 5 && $# != 6 ]]; then
  echo "usage: $0 REPO UNPACKED_EVIDENCE JSON_INCLUDE_ROOT FRESH_OUTPUT_DIR release|debug|diagnostics|sanitizers [identity|typed-singleton]" >&2
  exit 64
fi
repo=$(realpath "$1")
inputs=$(realpath "$2")
json_include=$(realpath "$3")
output=$(realpath -m "$4")
mode=$5
io_mode=${6:-identity}
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
attempt=cloud-bvi-frozen-20261005-round2
case "$output" in "$repo"/docs/research/zero-miss-20261005/round2/evidence/bvi-frozen-suite/*) ;; *) echo 'output outside new research evidence root' >&2; exit 64;; esac
[[ ! -e "$output" ]] || { echo 'refusing to overwrite an existing run' >&2; exit 64; }
case "$mode" in
  release) flags=(-O2);;
  debug) flags=(-O0 -g -D_GLIBCXX_ASSERTIONS);;
  diagnostics) flags=(-O0 -g -D_GLIBCXX_ASSERTIONS -DJSON_DIAGNOSTICS=1);;
  sanitizers) flags=(-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined);;
  *) echo 'unsupported mode' >&2; exit 64;;
esac
mkdir "$output"
exec > >(tee "$output/run.log") 2>&1
date -u +%FT%TZ
uname -a
g++ --version
printf 'mode=%s io_adapter=%s attempt=%s\n' "$mode" "$io_mode" "$attempt"
git -C "$repo" rev-parse HEAD
work=$(mktemp -d /tmp/phigros-bvi-round2.XXXXXX)
printf 'temporary_work=%s\n' "$work"
source_dir="$repo/research/x10d_o_bvi_build"
check_hash() { local expected=$1 path=$2; printf '%s  %s\n' "$expected" "$path" | sha256sum -c -; }
check_hash f45c079548c9e0e78472f31c1613f0b2aaddede0ca6e383ae663f31cfe91fdce "$source_dir/bvi.cpp"
check_hash 6f6313ef23ae473404d07f22f3438abc25364d65bce9f08fa35eaad5872eff0f "$source_dir/bvi.hpp"
check_hash ba0328c622bcd44974e4d7e66414546d1e8de71cfd28f379d796096f2e67f0b3 "$source_dir/driver.inc"
check_hash fc3199dc45bb7caf32557d8b8099ddc58cf0a8eec527fe25690b3423b802a407 "$source_dir/main.cpp"
check_hash f2e1f3cac4e48fa851d6986291e4bd5b384d87f92c175d8b126cbd91580289cc "$json_include/nlohmann/json.hpp"
m="$inputs/measurements/game-assist/2026-09-30-m0-manual-continue"
args=("$m/hold-ownership-x10d-o-bvi/normalized-execution.json" "$m/hold-ownership-x10d-o-bvi/oracle.json" "$m/hold-ownership-x10d-o-bvi-r1/typed-r1.json" "$repo/research/x10d_o_bvi/supplemental.json" "$m/hold-ownership-x10d-o-bvi-r1/r1-cases.json" "$m/hold-ownership-x10d-o-bvi-r1/expected-coverage.json")
expected=(96d536c5b5e9e01e359da2ead01596428b46f4265f28ba295ee9d9b4297650c5 90f8680c4362d07a718ac2bb7142b37396871b7ffd81523a06ddd8a685ec5425 ce69f19d5c488e780c7d1cb7785601d3e3f80750b134440c636dda20f19ff8c3 45ab5f3d97c3513fb6836211801922a9d86863de273739e35b5e4263b7a6c0a1 8da656609b424eda46633145728dd485d12bfb25e83f1850235a527891e28f46 4985ec8aac5d89f0863125daa6be483b26c1b200b00f5bbcb23fc691cc517f2a)
for i in "${!args[@]}"; do check_hash "${expected[$i]}" "${args[$i]}"; done
sha256sum "${args[@]}" | tee "$output/inputs-before.sha256"
# Always rebind output identity; typed-singleton additionally adapts the verified I/O shape.
# Candidate computation, original driver, and all expected values remain unchanged.
timeout 60s g++ -std=c++20 -O2 -I"$json_include" "$script_dir/audit_io.cpp" -o "$work/audit-io"
if [[ $io_mode == typed-singleton ]]; then
  "$work/audit-io" --check-input "${args[4]}" > "$output/singleton-input-audit.json"
fi
[[ $(grep -o 'bvi-build-20261005-01' "$source_dir/main.cpp" | wc -l) == 1 ]]
sed "s/bvi-build-20261005-01/$attempt/" "$source_dir/main.cpp" > "$work/main.cpp"
sed "s/$attempt/bvi-build-20261005-01/" "$work/main.cpp" | cmp -s - "$source_dir/main.cpp"
case "$io_mode" in
  identity) ;;
  typed-singleton) patch --fuzz=0 "$work/main.cpp" < "$script_dir/typed-singleton-lines.patch";;
  *) echo 'unsupported I/O adapter' >&2; exit 64;;
esac
diff -u --label research/x10d_o_bvi_build/main.cpp --label isolated-cloud/main.cpp "$source_dir/main.cpp" "$work/main.cpp" > "$output/adapter.diff" || [[ $? == 1 ]]
sha256sum "$work/main.cpp" "$source_dir/bvi.cpp" "$source_dir/bvi.hpp" "$source_dir/driver.inc" "$json_include/nlohmann/json.hpp" > "$output/build-inputs.sha256"
compile=(timeout 120s g++ -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -I"$source_dir" -I"$json_include" "$source_dir/bvi.cpp" "$work/main.cpp" -o "$work/bvi-tests")
printf '%q ' "${compile[@]}"; printf '\n'
set +e
"${compile[@]}" > "$output/compile.log" 2>&1
compile_exit=$?
set -e
printf 'compile_exit=%s\n' "$compile_exit"
cat "$output/compile.log"
[[ $compile_exit == 0 ]] || exit "$compile_exit"
sha256sum "$work/bvi-tests" | tee "$output/binary.sha256"
if [[ $mode == sanitizers ]]; then
  # LSan is incompatible with the current ptrace sandbox; explicit, not a clean leak report.
  export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
  printf 'ASAN_OPTIONS=%s UBSAN_OPTIONS=%s\n' "$ASAN_OPTIONS" "$UBSAN_OPTIONS"
fi
for test in wrong-contact suite; do
  report="$output/$test.json"
  printf '{"attempt":"%s","pending":true}\n' "$attempt" > "$report"
  cp "$report" "$output/$test.reservation.json"
  command=(timeout 60s "$work/bvi-tests" "${args[@]}" "$report")
  [[ $test != wrong-contact ]] || command+=(wrong-contact-only)
  printf '%q ' "${command[@]}"; printf '\n'
  set +e
  "${command[@]}" > "$output/$test.stdout.log" 2> "$output/$test.stderr.log"
  code=$?
  set -e
  printf '%s\n' "$code" > "$output/$test.exit"
  printf '%s_exit=%s\n' "$test" "$code"
  cat "$output/$test.stdout.log" "$output/$test.stderr.log"
done
sha256sum -c "$output/inputs-before.sha256"
git -C "$repo" diff --exit-code -- src include tests fixtures research CMakeLists.txt CMakePresets.json
date -u +%FT%TZ
set +e
"$work/audit-io" --check-results "$output" > "$output/summary.json" 2> "$output/summary.stderr.log"
aggregate_exit=$?
set -e
printf '%s\n' "$aggregate_exit" > "$output/aggregate.exit"
printf 'aggregate_exit=%s (0=full pass; 1=assertion/protocol failure; 2=missing or invalid result)\n' "$aggregate_exit"
exit "$aggregate_exit"
