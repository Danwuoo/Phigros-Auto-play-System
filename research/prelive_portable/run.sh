#!/usr/bin/env bash
# Offline-only C++20 lane. Does not install dependencies or fetch anything.
set -euo pipefail
[[ $# == 3 ]] || { echo 'usage: run.sh PINNED_JSON_INCLUDE MODE FRESH_OUTPUT' >&2; exit 64; }
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(realpath "$here/../.."); dep=$(realpath "$1"); mode=$2; out=$(realpath -m "$3")
case "$out" in "$repo/research/prelive_portable/evidence/"*) ;; *) echo 'output must be fresh evidence subdirectory' >&2;exit 64;;esac
[[ ! -e "$out" ]] || { echo 'refusing existing output' >&2;exit 64; }
case "$mode" in
 release) flags=(-O2 -DNDEBUG);;
 debug) flags=(-O0 -g1 -D_GLIBCXX_ASSERTIONS);;
 sanitizers) flags=(-O1 -g1 -fno-omit-frame-pointer -fsanitize=address,undefined);;
 *) exit 64;;
esac
mkdir -p "$out"; cd "$repo"
exec > >(tee "$out/run.log") 2>&1
printf '%s  %s\n' f2e1f3cac4e48fa851d6986291e4bd5b384d87f92c175d8b126cbd91580289cc "$dep/nlohmann/json.hpp" | sha256sum -c -
g++ --version; uname -a; git rev-parse HEAD
printf 'PAS_PORTABLE_DETECT_LEAKS=%s (LSan is a separate check from ASan/UBSan)\n' "${PAS_PORTABLE_DETECT_LEAKS:-1}"
cp "$here/run.sh" "$out/run-source.sh";cp "$here/closure.sh" "$out/closure-source.sh"
source "$here/closure.sh"
if [[ ${PAS_PORTABLE_SKIP_WORKLOAD:-0} == 1 ]];then
 filtered=();for item in "${tests[@]}";do [[ ${item%%:*} == cost_workload_tests ]] || filtered+=("$item");done
 tests=("${filtered[@]}");echo 'NOT_RUN cost_workload_tests explicitly excluded while its generator is under review'
fi
build=$(mktemp -d "$repo/out/prelive-portable-$mode.XXXXXX"); printf '%s\n' "$build" > "$out/build-directory.txt"
common=(-std=c++20 -pthread -ffunction-sections -fdata-sections -Wall -Wextra -Wpedantic "${flags[@]}"
 -I"$here/shim" -I"$repo/include" -I"$dep" -I"$repo/research/bvi_cold_v3"
 -I"$repo/research/bvi_windows/bridge" -I"$repo/research/hold_front_current" -I"$repo/research/prelive_current")
inputs=("${sources[@]}" include/pas/core.hpp include/pas/game.hpp include/pas/game_tracking.hpp include/pas/game_motion.hpp
 include/pas/game_session.hpp include/pas/result_ui_labels.hpp include/pas/strategy_version.hpp research/prelive_current/runtime.hpp
 research/prelive_current/current.hpp research/prelive_current/own_bridge.hpp "$here/shim/windows.h" "$here/run.sh" "$here/closure.sh" "$dep/nlohmann/json.hpp")
for item in "${tests[@]}"; do [[ ! -f ${item#*:} ]] || inputs+=("${item#*:}"); done
while IFS= read -r header;do inputs+=("$header");done < <(find include/pas research/bvi_cold_v3 research/bvi_windows/bridge research/hold_front_current -maxdepth 1 -type f -name '*.hpp' | sort)
sha256sum "${inputs[@]}" > "$out/source-before.sha256"
objects=()
for source in "${sources[@]}"; do
 object="$build/${source//\//_}.o"; objects+=("$object")
 printf 'COMPILE %q\n' "$source"
 timeout 240s g++ "${common[@]}" -c "$source" -o "$object" > "$out/${source//\//_}.compile.log" 2>&1
done
ar rcs "$build/portable_current.a" "${objects[@]}"
overall=0
for item in "${tests[@]}"; do
 name=${item%%:*}; source=${item#*:}
 if [[ ! -f $source ]]; then printf 'NOT_RUN %s source_missing\n' "$name"; overall=1;continue;fi
 printf 'BUILD_TEST %s\n' "$name"
 if ! timeout 240s g++ "${common[@]}" "$source" "$build/portable_current.a" -Wl,--gc-sections -o "$build/$name" > "$out/$name.compile.log" 2>&1;then echo 'COMPILE_FAILED' > "$out/$name.status";overall=1;continue;fi
 set +e
 ASAN_OPTIONS=detect_leaks=${PAS_PORTABLE_DETECT_LEAKS:-1}:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 timeout 180s "$build/$name" "$out/$name.json" > "$out/$name.stdout.log" 2> "$out/$name.stderr.log"
 code=$?;set -e; printf '%s\n' "$code" > "$out/$name.exit"; [[ $code == 0 ]] || overall=1
 printf 'TEST %s exit=%s\n' "$name" "$code"
done
for probe in scheduler flick_move;do
 source="docs/research/zero-miss-20261005/evidence/touch-scheduler/${probe}_probe.cpp"
 sha256sum "$source" >> "$out/source-before.sha256"
 timeout 120s g++ "${common[@]}" "$source" "$build/portable_current.a" -Wl,--gc-sections -o "$build/$probe" > "$out/$probe.compile.log" 2>&1
 set +e
 ASAN_OPTIONS=detect_leaks=${PAS_PORTABLE_DETECT_LEAKS:-1}:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 timeout 30s "$build/$probe" > "$out/$probe.stdout.log" 2> "$out/$probe.stderr.log"
 code=$?;set -e;printf '%s\n' "$code" > "$out/$probe.exit"; [[ $code == 0 ]] || overall=1
done
if ! sha256sum -c "$out/source-before.sha256" > "$out/source-after-check.log";then overall=1;fi
printf '%s\n' "$overall" > "$out/overall.exit"
exit "$overall"
