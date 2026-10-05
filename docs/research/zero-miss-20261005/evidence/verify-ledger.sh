#!/usr/bin/env bash
# Read-only JSON/document maintenance check; no product or device execution.
set -euo pipefail
SOURCE=docs/catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json
TEMPLATE=docs/research/zero-miss-20261005/LEGACY_ACCEPTANCE_MANIFEST_TEMPLATE.json
sha256sum "$SOURCE"
jq -e '. as $d | {
  runs: (.runs|length), sessions: (.sessions|length), songs: (.songs|length),
  HD: ([.runs[]|select(.difficulty=="HD")]|length),
  IN: ([.runs[]|select(.difficulty=="IN")]|length),
  miss0: ([.runs[]|select(.miss==0)]|length),
  all_statuses: ([.runs[].status]|unique),
  run_ids_unique: ((.runs|map(.id)|unique|length)==(.runs|length)),
  session_refs_resolve: all(.runs[];
    .session_ref as $ref|any($d.sessions[];.path==$ref)),
  song_run_refs_resolve: all(.songs[];
    all((.hd_run_ids+.in_run_ids)[];
      . as $ref|any($d.runs[];.id==$ref))),
  per_song_counts_match: all(.songs[];. as $s|
    (([$d.runs[]|select(.song==$s.song and .difficulty=="HD")]|length)
      ==($s.hd_run_ids|length)) and
    (([$d.runs[]|select(.song==$s.song and .difficulty=="IN")]|length)
      ==($s.in_run_ids|length))),
  judgment_sums_consistent: all(.runs[];
    .judgments==(.perfect+.good+.bad+.miss)),
  chapter_listing_verified,
  current_chapter_denominator
} | if (.runs==89 and .sessions==33 and .songs==20 and .HD==70 and .IN==19
  and .miss0==0 and .run_ids_unique and .session_refs_resolve
  and .song_run_refs_resolve and .per_song_counts_match and .judgment_sums_consistent
  and .chapter_listing_verified==false and .current_chapter_denominator==null)
  then . else error("historical ledger consistency changed") end' "$SOURCE"
jq -e '
  (.current_inventory.candidates|length)==20 and
  (.historical_in_runs|length)==19 and (.attempts|length)==0 and
  .execution_authorized==false and
  .current_inventory.chapter_listing_verified==false and
  .current_inventory.current_chapter_denominator==null and
  all(.current_inventory.candidates[];
    .current_membership=="unknown" and
    .current_in_unlock_state=="unknown" and .current_in_level==null) and
  all(.historical_in_runs[];.miss>0)
' "$TEMPLATE"
printf 'RESULT=PASS_FOR_TRACKED_JSON_CONSISTENCY_ONLY\n'
