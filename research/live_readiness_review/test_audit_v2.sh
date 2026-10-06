#!/usr/bin/env bash
# Bounded synthetic software verification, not Windows/live cost qualification.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
json_include=${1:?Verified nlohmann include root required}
out=${2:?Fresh output root required}
[[ ! -e "$out" ]] || { echo 'fresh output root required' >&2; exit 2; }
mkdir -p "$out/bin"
flags=(-std=c++20 -O2 -Wall -Wextra -Wpedantic)
if [[ ${AUDIT_SANITIZERS:-0} == 1 ]]; then flags=(-std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie); fi
for pair in 'prelive_audit/audit:audit-v2' 'live_readiness_review/make_audit_counterexamples:generate' 'live_readiness_review/check_audit_results:check'; do
  source=${pair%:*}; target=${pair#*:}
  g++ "${flags[@]}" -I"$json_include" "$repo/research/$source.cpp" -o "$out/bin/$target"
done
"$out/bin/generate" "$out/cases" "$repo/docs/research/zero-miss-20261006/windows/prelive-evidence-01/package/pipeline-release-aa-tap-1.json"
for input in "$out"/cases/*.json; do
  [[ -e "$input.journal.jsonl" || -e "$input.events.jsonl" ]] || continue
  if [[ "$input" == *malformed-int64-overflow.json ]]; then
    set +e
    "$out/bin/audit-v2" "$input" "${input%.json}-audit-v2.json" > "$out/malformed.stdout" 2> "$out/malformed.stderr"
    status=$?
    set -e
    [[ $status == 2 && ! -e "${input%.json}-audit-v2.json" ]] || exit 1
  else
    "$out/bin/audit-v2" "$input" "${input%.json}-audit-v2.json"
  fi
done
"$out/bin/check" "$out/cases" "$out/results.json"
sha256sum "$repo/research/prelive_audit/audit.cpp" "$repo/research/prelive_current/evidence_hash.hpp" "$repo/research/live_readiness_review/make_audit_counterexamples.cpp" "$repo/research/live_readiness_review/check_audit_results.cpp" "$json_include/nlohmann/json.hpp" "$out/bin/audit-v2" > "$out/inputs-and-binary.sha256"
