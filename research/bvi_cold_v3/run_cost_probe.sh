#!/usr/bin/env bash
# Descriptive Linux function cost only; never a target Windows/live gate.
set -euo pipefail
[[ $# == 3 ]] || { echo 'usage: run_cost_probe.sh REPO JSON_INCLUDE FRESH_OUTPUT' >&2;exit 64; }
repo=$(realpath "$1");deps=$(realpath "$2");out=$(realpath -m "$3");[[ ! -e "$out" ]] || exit 64
case "$out" in "$repo"/docs/research/zero-miss-20261005/round4/evidence/*);;*)exit 64;;esac
mkdir -p "$out";cd "$repo";work=$(mktemp -d /tmp/pas-v3-cost.XXXXXX)
exec > >(tee "$out/run.log") 2>&1
date -u +%FT%TZ;uname -a;g++ --version | head -1;getconf _NPROCESSORS_ONLN
sha256sum research/bvi_cold_v3/*.{cpp,hpp} research/bvi_cold_v2/bvi.{cpp,hpp} "$deps/nlohmann/json.hpp" > "$out/source-before.sha256"
for variant in v2 v3;do
 dir="$repo/research/bvi_cold_$variant";sources=("$dir/bvi.cpp");[[ "$variant" != v3 ]] || sources+=("$dir/contact_policy.cpp" "$dir/relation_policy.cpp" "$dir/constraint_policy.cpp")
 command=(timeout 150s g++ -std=c++20 -O2 -DNDEBUG -I "$deps" -I "$dir" "-DBVI_HEADER=\"$dir/bvi.hpp\"" "${sources[@]}" research/bvi_cold_v3/cost_probe.cpp -o "$work/$variant")
 printf '%q ' "${command[@]}";printf '\n';"${command[@]}" > "$out/$variant.compile.log" 2>&1
 sha256sum "$work/$variant" > "$out/$variant.binary.sha256"
done
# Fixed ABBA order is descriptive only: no claim that it qualifies prior A/A noise gates.
for item in a1-v2 b1-v3 b2-v3 a2-v2;do
 variant=${item#*-};timeout 90s "$work/$variant" "$out/$item.json"
done
sha256sum -c "$out/source-before.sha256"
printf '0\n' > "$out/aggregate.exit"
