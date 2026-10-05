#!/usr/bin/env bash
# Synthetic-only final v3 contract regression, separate from unchanged legacy oracle.
set -euo pipefail
[[ $# == 4 ]] || { echo 'usage: run_new_contracts.sh REPO JSON_INCLUDE release|debug|sanitizers FRESH_OUTPUT' >&2; exit 64; }
repo=$(realpath "$1"); deps=$(realpath "$2"); mode=$3; out=$(realpath -m "$4")
[[ ! -e "$out" ]] || exit 64
case "$out" in "$repo"/docs/research/zero-miss-20261005/round4/evidence/*) ;; *) exit 64;; esac
case "$mode" in release) flags=(-O2);;debug) flags=(-O0 -g -D_GLIBCXX_ASSERTIONS);;sanitizers) flags=(-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined);;*)exit 64;;esac
mkdir -p "$out"; work=$(mktemp -d /tmp/pas-v3-contracts.XXXXXX);cd "$repo"
exec > >(tee "$out/run.log") 2>&1
sha256sum research/bvi_cold_v3/*.{cpp,hpp} "$deps/nlohmann/json.hpp" > "$out/source-before.sha256"
date -u +%FT%TZ;g++ --version | head -1;git rev-parse HEAD
cp research/bvi_cold_v3/*.{cpp,hpp} "$work/"
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
aggregate=0
for suite in relation contact line_ambiguity; do
 definitions=();[[ "$suite" != contact ]] || definitions=(-DCONTACT_INTEGRATION)
 command=(timeout 150s g++ -std=c++20 "${flags[@]}" "${definitions[@]}" -I "$work" -I "$deps" "$work/bvi.cpp" "$work/contact_policy.cpp" "$work/relation_policy.cpp" "$work/constraint_policy.cpp" "$work/${suite}_tests.cpp" -o "$work/$suite")
 printf '%q ' "${command[@]}";printf '\n'
 "${command[@]}" > "$out/$suite.compile.log" 2>&1
 sha256sum "$work/$suite" > "$out/$suite.binary.sha256"
 set +e
 timeout 90s "$work/$suite" "$out/$suite.json" > "$out/$suite.stdout.log" 2> "$out/$suite.stderr.log"
 native=$?;set -e;printf '%s\n' "$native" > "$out/$suite.exit"
 [[ $native == 0 ]] || aggregate=1
 jq '{assertions,failed_assertions}' "$out/$suite.json"
done
sha256sum -c "$out/source-before.sha256"
printf '%s\n' "$aggregate" > "$out/aggregate.exit"
exit "$aggregate"
