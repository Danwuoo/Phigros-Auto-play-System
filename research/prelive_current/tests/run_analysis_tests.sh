#!/usr/bin/env bash
# Source-only C++20 tools; test doubles do not certify a Windows cost campaign.
set -euo pipefail
[[ $# == 3 ]] || { echo 'usage: run_analysis_tests.sh JSON_INCLUDE MODE FRESH_OUT';exit 64; }
repo=$(cd "$(dirname "$0")/../../.." && pwd)
json=$(realpath "$1");mode=$2;out=$(realpath -m "$3")
[[ ! -e "$out" ]] || { echo 'fresh output required';exit 64; }
case "$mode" in release) flags=(-O2 -DNDEBUG);; debug) flags=(-O0 -g1 -D_GLIBCXX_ASSERTIONS);; sanitizers) flags=(-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer);; *)exit 64;;esac
mkdir -p "$out";cd "$repo"
sha256sum research/prelive_current/{cost_analysis.cpp,campaign_provenance.hpp,evidence_hash.hpp} research/prelive_current/tests/{cost_analysis_tests.cpp,run_analysis_tests.sh} "$json/nlohmann/json.hpp" > "$out/source-before.sha256"
g++ --version > "$out/compiler.txt"; uname -a > "$out/host.txt"
g++ -std=c++20 -Wall -Wextra -Wpedantic "${flags[@]}" -I"$json" research/prelive_current/tests/cost_analysis_tests.cpp -o "$out/analysis-tests" > "$out/compile.log" 2>&1
set +e
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$out/analysis-tests" "$out/fixtures" "$repo" > "$out/stdout.log" 2> "$out/stderr.log"
rc=$?;set -e;printf '%s\n' "$rc" > "$out/exit.txt"
sha256sum -c "$out/source-before.sha256" > "$out/source-after-check.txt"
# Linux ASan/UBSan only; leak checking is unavailable under this host's ptrace.
exit "$rc"
