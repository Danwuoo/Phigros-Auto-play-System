#!/usr/bin/env bash
# Verify retained evidence and immutable candidate; does not execute candidate/device.
set -euo pipefail
repo=$(git -C "$(dirname "$0")" rev-parse --show-toplevel);cd "$repo"
r=docs/research/zero-miss-20261005/round4;e="$r/evidence"
sha256sum -c "$e/final-core.sha256"
for old in docs/research/zero-miss-20261005/evidence/research-files.sha256 docs/research/zero-miss-20261005/round2/research-files.sha256 docs/research/zero-miss-20261005/round3/research-files.sha256;do sha256sum -c "$old" >/dev/null;done
for mode in release debug sanitizers;do
 jq -e '.assertions==3938 and .failed_assertions==36' "$e/final-$mode/suite.json" >/dev/null
 jq -e '.layer_cases==356 and .new_case_failed_assertions==0 and .wrong_contact_verified==true' "$e/final-$mode/legacy-summary.json" >/dev/null
 [[ $(cat "$e/final-$mode/suite.exit") == 1 && $(cat "$e/final-$mode/wrong-contact.exit") == 1 ]]
 for name in contract independent;do [[ $(cat "$e/final-$mode/$name.exit") == 0 ]];done
 for name in relation contact line_ambiguity;do jq -e '.failed_assertions==0' "$e/contracts-final-$mode/$name.json" >/dev/null;done
 jq -e '.case_count==44 and .assertions==101 and .failed_assertions==0 and .legal_actions_observed==17 and .unsafe_false_positives==0' "$e/independent/final-$mode/results.json" >/dev/null
 supplemental="$e/independent/supplemental-final-$mode";[[ "$mode" != release ]] || supplemental="$supplemental-rerun"
 jq -e '.supplemental_cases==5 and .failed_cases==0 and .legal_actions_observed==2 and .unsafe_false_positives==0' "$supplemental/summary.json" >/dev/null
 cmp "$e/final-release/suite.json" "$e/final-$mode/suite.json"
 cmp "$e/independent/final-release/results.json" "$e/independent/final-$mode/results.json"
 for probe in scheduler flick_move;do [[ $(cat "$e/scheduler/final/$mode-$probe.exit") == 0 ]];done
done
jq -e '.original_54_failure_map|length==54' "$e/final-original54-map.json" >/dev/null
jq -e '.fixed_original_assertions==18 and .remaining_original_failed_assertions==36 and .new_failed_assertions==0' "$e/final-original54-map.json" >/dev/null
[[ $(cat "$e/cost/final/aggregate.exit") == 0 ]]
git diff --exit-code a53a9b7021bcc900f498047f5cc017b42e4711b0 -- src include configs tests fixtures tools research/bvi_cold_v2 research/x10d_o_bvi_build research/x10d_o_bvi research/x10d_o_bvi_r1 CMakeLists.txt CMakePresets.json
for s in research/bvi_cold_v3/*.sh "$e/independent/run-independent.sh" "$e/independent/run-supplemental.sh";do bash -n "$s";done
[[ ! -f "$r/research-files.sha256" ]] || sha256sum -c "$r/research-files.sha256" >/dev/null
printf 'PASS: retained candidate/evidence integrity only; legacy remains36fail; actual device/game not run.\n'
