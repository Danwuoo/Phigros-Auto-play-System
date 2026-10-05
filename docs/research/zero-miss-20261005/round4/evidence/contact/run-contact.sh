#!/usr/bin/env bash
# Fresh-output, cold-only contact subpolicy reproduction. No devices or network.
set -euo pipefail
[[ $# == 3 ]] || { echo 'usage: run-contact.sh release|debug|sanitizers direct|integration FRESH_OUTPUT' >&2; exit 64; }
mode=$1; scope=$2
repo=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
output=$(realpath -m "$3")
case "$output" in "$repo"/docs/research/zero-miss-20261005/round4/evidence/contact/*) ;; *) echo 'output must be a new contact evidence subdirectory' >&2; exit 64;; esac
[[ ! -e "$output" ]] || { echo 'refusing to overwrite prior results' >&2; exit 64; }
case "$mode" in release) flags=(-O2);; debug) flags=(-O0 -g -D_GLIBCXX_ASSERTIONS);; sanitizers) flags=(-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined);; *) exit 64;; esac
src="$repo/research/bvi_cold_v3"
sources=("$src/contact_policy.cpp" "$src/contact_tests.cpp")
headers=("$src/bvi.hpp" "$src/contact_policy.hpp")
case "$scope" in direct) ;; integration) sources+=("$src/bvi.cpp" "$src/relation_policy.cpp" "$src/constraint_policy.cpp"); headers+=("$src/relation_policy.hpp"); flags+=(-DCONTACT_INTEGRATION);; *) exit 64;; esac
json=${JSON_INCLUDE:-/tmp/phigros-bvi-round2-deps}
[[ -r "$json/nlohmann/json.hpp" ]] || { echo 'missing pinned JSON dependency' >&2; exit 66; }
printf '%s  %s\n' f2e1f3cac4e48fa851d6986291e4bd5b384d87f92c175d8b126cbd91580289cc "$json/nlohmann/json.hpp" | sha256sum -c -
mkdir -p "$output/source"
exec > >(tee "$output/run.log") 2>&1
date -u +%FT%TZ
g++ --version | head -1
git -C "$repo" rev-parse HEAD
sha256sum "${sources[@]}" "${headers[@]}" "$json/nlohmann/json.hpp" "$(realpath "$0")" > "$output/before.sha256"
cp "${sources[@]}" "${headers[@]}" "$(realpath "$0")" "$output/source/"
work=$(mktemp -d /tmp/contact-reproduction.XXXXXX)
printf 'build_directory=%s mode=%s scope=%s\n' "$work" "$mode" "$scope"
command=(timeout 150s g++ -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -I"$src" -I"$json" "${sources[@]}" -o "$work/contact-tests")
printf '%q ' "${command[@]}"; printf '\n'
set +e
"${command[@]}" > "$output/build.log" 2>&1
code=$?
set -e
printf '%s\n' "$code" > "$output/build.exit"
[[ "$code" == 0 ]] || { cat "$output/build.log"; exit "$code"; }
sha256sum "$work/contact-tests" > "$output/binary.sha256"
if [[ "$mode" == sanitizers ]]; then
 export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
 echo 'ASan and UBSan; LSan disabled for known ptrace restriction, no leak-free claim.'
fi
set +e
timeout 90s "$work/contact-tests" "$output/result.json" > "$output/stdout.log" 2> "$output/stderr.log"
code=$?
set -e
printf '%s\n' "$code" > "$output/native.exit"
sha256sum -c "$output/before.sha256" > "$output/after-verification.txt"
jq '{assertions,failed_assertions,denominators}' "$output/result.json"
date -u +%FT%TZ
exit "$code"
