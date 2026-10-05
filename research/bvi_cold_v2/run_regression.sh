#!/usr/bin/env bash
# Cold-only v2 regression. Reads immutable historical fixtures; never runs a device.
set -euo pipefail
if [[ $# -lt 6 || $# -gt 7 ]]; then
  echo 'usage: run_regression.sh REPO EVIDENCE_ROOT JSON_INCLUDE FRESH_OUTPUT release|debug|sanitizers baseline|candidate [legacy|contract|independent|all]' >&2
  exit 64
fi
repo=$(realpath "$1"); inputs=$(realpath "$2"); json_include=$(realpath "$3")
output=$(realpath -m "$4"); mode=$5; variant=$6; suite=${7:-all}
new="$repo/research/bvi_cold_v2"
old="$repo/research/x10d_o_bvi_build"
prior="$repo/docs/research/zero-miss-20261005/round2/evidence/bvi-frozen-suite"
attempt=cloud-bvi-cold-v2-20261005
case "$output" in "$repo"/docs/research/zero-miss-20261005/round3/evidence/*) ;; *) echo 'fresh output must be in round3 evidence' >&2; exit 64;; esac
[[ ! -e "$output" ]] || { echo 'refusing to overwrite results' >&2; exit 64; }
case "$mode" in
  release) flags=(-O2);;
  debug) flags=(-O0 -g -D_GLIBCXX_ASSERTIONS);;
  sanitizers) flags=(-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined);;
  *) exit 64;;
esac
case "$variant" in baseline) selected=$old;; candidate) selected=$new;; *) exit 64;; esac
case "$suite" in legacy|contract|independent|all) ;; *) exit 64;; esac
mkdir -p "$(dirname "$output")"
mkdir "$output"
exec > >(tee "$output/run.log") 2>&1
work=$(mktemp -d /tmp/phigros-bvi-cold-v2.XXXXXX)
date -u +%FT%TZ
printf 'variant=%s mode=%s suite=%s attempt=%s\n' "$variant" "$mode" "$suite" "$attempt"
g++ --version | head -1
git -C "$repo" rev-parse HEAD
printf 'temporary_build=%s\n' "$work"
verify() { printf '%s  %s\n' "$1" "$2" | sha256sum -c -; }
verify f45c079548c9e0e78472f31c1613f0b2aaddede0ca6e383ae663f31cfe91fdce "$old/bvi.cpp"
verify 6f6313ef23ae473404d07f22f3438abc25364d65bce9f08fa35eaad5872eff0f "$old/bvi.hpp"
verify ba0328c622bcd44974e4d7e66414546d1e8de71cfd28f379d796096f2e67f0b3 "$old/driver.inc"
verify fc3199dc45bb7caf32557d8b8099ddc58cf0a8eec527fe25690b3423b802a407 "$old/main.cpp"
verify f2e1f3cac4e48fa851d6986291e4bd5b384d87f92c175d8b126cbd91580289cc "$json_include/nlohmann/json.hpp"
m="$inputs/measurements/game-assist/2026-09-30-m0-manual-continue"
args=("$m/hold-ownership-x10d-o-bvi/normalized-execution.json" "$m/hold-ownership-x10d-o-bvi/oracle.json" "$m/hold-ownership-x10d-o-bvi-r1/typed-r1.json" "$repo/research/x10d_o_bvi/supplemental.json" "$m/hold-ownership-x10d-o-bvi-r1/r1-cases.json" "$m/hold-ownership-x10d-o-bvi-r1/expected-coverage.json")
expected=(96d536c5b5e9e01e359da2ead01596428b46f4265f28ba295ee9d9b4297650c5 90f8680c4362d07a718ac2bb7142b37396871b7ffd81523a06ddd8a685ec5425 ce69f19d5c488e780c7d1cb7785601d3e3f80750b134440c636dda20f19ff8c3 45ab5f3d97c3513fb6836211801922a9d86863de273739e35b5e4263b7a6c0a1 8da656609b424eda46633145728dd485d12bfb25e83f1850235a527891e28f46 4985ec8aac5d89f0863125daa6be483b26c1b200b00f5bbcb23fc691cc517f2a)
for i in "${!args[@]}"; do verify "${expected[$i]}" "${args[$i]}"; done
sha256sum "${args[@]}" "$selected/bvi.cpp" "$selected/bvi.hpp" > "$output/immutable-before.sha256"
cp "$selected/bvi.cpp" "$selected/bvi.hpp" "$old/driver.inc" "$work/"
sed "s/bvi-build-20261005-01/$attempt/" "$old/main.cpp" > "$work/main.cpp"
patch --fuzz=0 "$work/main.cpp" < "$prior/typed-singleton-lines.patch"
diff -u --label historical/main.cpp --label cold-v2-io/main.cpp "$old/main.cpp" "$work/main.cpp" > "$output/io-adapter.diff" || [[ $? == 1 ]]
sha256sum "$work/bvi.cpp" "$work/bvi.hpp" "$work/driver.inc" "$work/main.cpp" > "$output/build-sources.sha256"
if [[ "$mode" == sanitizers ]]; then
  export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
  echo 'AddressSanitizer+UBSan enabled; LeakSanitizer disabled due to known ptrace limitation; no leak-pass claim'
fi
compile() {
  local name=$1 source=$2
  local command=(timeout 150s g++ -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -I"$work" -I"$json_include" "$work/bvi.cpp" "$source" -o "$work/$name")
  printf 'COMPILE'; printf ' %q' "${command[@]}"; printf '\n'
  set +e
  "${command[@]}" > "$output/$name.compile.log" 2>&1
  local code=$?
  set -e
  printf '%s\n' "$code" > "$output/$name.compile.exit"
  [[ "$code" == 0 ]] || { cat "$output/$name.compile.log"; exit "$code"; }
  sha256sum "$work/$name" > "$output/$name.binary.sha256"
}
run_native() {
  local name=$1; shift
  printf 'RUN'; printf ' %q' "$@"; printf '\n'
  set +e
  "$@" > "$output/$name.stdout.log" 2> "$output/$name.stderr.log"
  local code=$?
  set -e
  printf '%s\n' "$code" > "$output/$name.exit"
  printf '%s_native_exit=%s\n' "$name" "$code"
  cat "$output/$name.stderr.log"
}
aggregate=0
if [[ "$suite" == legacy || "$suite" == all ]]; then
  timeout 60s g++ -std=c++20 -O2 -I"$json_include" "$prior/audit_io.cpp" -o "$work/audit-io"
  "$work/audit-io" --check-input "${args[4]}" > "$output/singleton-input-audit.json"
  compile legacy "$work/main.cpp"
  for name in wrong-contact suite; do
    printf '{"attempt":"%s","pending":true}\n' "$attempt" > "$output/$name.json"
    cp "$output/$name.json" "$output/$name.reservation.json"
    command=(timeout 90s "$work/legacy" "${args[@]}" "$output/$name.json")
    [[ "$name" != wrong-contact ]] || command+=(wrong-contact-only)
    run_native "$name" "${command[@]}"
  done
  set +e
  "$work/audit-io" --check-results "$output" > "$output/legacy-summary.json" 2> "$output/legacy-summary.stderr.log"
  legacy_aggregate=$?
  set -e
  printf '%s\n' "$legacy_aggregate" > "$output/legacy-aggregate.exit"
  if [[ "$legacy_aggregate" != 0 ]]; then aggregate=$legacy_aggregate; fi
fi
for name in contract independent; do
  if [[ "$suite" == "$name" || "$suite" == all ]]; then
    case "$name" in contract) source="$new/contract_fixture_v2.cpp";; independent) source="$new/independent_tests.cpp";; esac
    [[ -s "$source" ]] || { echo "missing $source" >&2; exit 66; }
    cp "$source" "$work/$name.cpp"
    sha256sum "$source" >> "$output/build-sources.sha256"
    compile "$name" "$work/$name.cpp"
    if [[ "$name" == contract ]]; then
      run_native "$name" timeout 90s "$work/$name" "$repo/docs/research/zero-miss-20261005/round2/BVI_FAILURE_SEMANTICS_CLASSIFICATION.json" "$output/contract.json" "$output/contract-change-map.json"
    else
      run_native "$name" timeout 90s "$work/$name" "$output/independent.json"
    fi
    code=$(cat "$output/$name.exit")
    if [[ "$code" != 0 ]]; then aggregate=1; fi
  fi
done
sha256sum -c "$output/immutable-before.sha256"
git -C "$repo" diff --exit-code acb27fdb095b82253ef9388eab52948a59663d02 -- src include configs tests fixtures research/x10d_o_bvi_build research/x10d_o_bvi_r1 tools CMakeLists.txt CMakePresets.json
printf '%s\n' "$aggregate" > "$output/aggregate.exit"
printf 'aggregate_exit=%s; original-contract failures remain failures, not a collector pass\n' "$aggregate"
date -u +%FT%TZ
exit "$aggregate"
