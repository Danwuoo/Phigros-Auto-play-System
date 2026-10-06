#!/usr/bin/env bash
# Keep each CTest invocation's evidence instead of overwriting prior reports.
set -euo pipefail
[[ $# == 2 ]] || exit 64
mkdir -p "$2"
out=$(mktemp -d "$2/$(basename "$1").XXXXXX")
"$1" "$out/report.json"
printf 'Report: %s/report.json\n' "$out"
