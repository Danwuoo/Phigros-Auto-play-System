#!/usr/bin/env bash
# All executions here are offline Linux analysis controls, not Windows-qualified
# runtime stages or timing measurements. Sources are compiled without edits.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
json_include=${1:?Pass the verified nlohmann include root}
out=${2:?Pass a fresh absolute output root}
[[ ! -e "$out" ]] || { echo 'fresh output root required' >&2; exit 2; }
mkdir -p "$out/bin"
git -C "$repo" show e9e305e3aba13f05a786c74540419428610b2f76:research/prelive_audit/audit.cpp > "$out/bin/original-audit.cpp"
git -C "$repo" show e9e305e3aba13f05a786c74540419428610b2f76:research/prelive_current/cost_analysis.cpp > "$out/bin/original-cost-analysis.cpp"
g++ -std=c++20 -O2 -I"$json_include" "$out/bin/original-audit.cpp" -o "$out/bin/original-audit"
g++ -std=c++20 -O2 -I"$json_include" "$out/bin/original-cost-analysis.cpp" -o "$out/bin/original-cost-analysis"
g++ -std=c++20 -O2 -I"$json_include" "$repo/research/live_readiness_review/make_audit_counterexamples.cpp" -o "$out/bin/make-counterexamples"
"$out/bin/make-counterexamples" "$out/cases" "$repo/docs/research/zero-miss-20261006/windows/prelive-evidence-01/package/pipeline-release-aa-tap-1.json"
for name in control-valid receipt-plan-mismatch cross-intent-up completed-intent-rebirth unknown-release-overwritten fabricated-cost-distribution 500ms-raw-latency-green-summary attempts-field-mismatch pixel-incomplete; do
  "$out/bin/original-audit" "$out/cases/$name.json" "$out/cases/$name-audit.json"
done
"$out/bin/original-cost-analysis" run "$out/cases/500ms-raw-latency-green-summary-gate-input.json" "$out/cases/500ms-raw-latency-green-summary-gate.json"
"$out/bin/original-cost-analysis" aa "$out/cases/repeated-tap-aa-input.json" "$out/cases/repeated-tap-aa-freeze.json"
sha256sum "$out/bin/original-audit.cpp" "$out/bin/original-cost-analysis.cpp" "$repo/research/live_readiness_review/make_audit_counterexamples.cpp" "$json_include/nlohmann/json.hpp" > "$out/inputs.sha256"
