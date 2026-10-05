#!/usr/bin/env bash
# Read-only file/hash/receipt maintenance audit. Does not execute supplied scripts.
set -euo pipefail
if [[ $# != 2 ]]; then echo 'Usage: bash audit_receipts.sh PACKAGE_ROOT REPO_ROOT' >&2; exit 2; fi
package=$(realpath "$1")
repo=$(realpath "$2")
base=measurements/game-assist/2026-09-30-m0-manual-continue
c=$base/hold-ownership-x10d-o-bvi-r2f-control
d=$base/hold-ownership-x10d-o-bvi-build
cmd_rel=research/x10d_o_bvi_r2f_control/configure-release.cmd
manifest=$package/manifest.json
temp=$(mktemp -d /tmp/phigros-windows-receipt-audit.XXXXXX)
trap 'rm -rf -- "$temp"' EXIT
sha() { sha256sum "$1" | cut -d ' ' -f1; }
bytes() { stat -c %s "$1"; }
# Sources are explicit repository-relative suffixes; supplied absolute paths are never opened.
source_row() {
 local entry=$1 rel path original_sha original_bytes actual_sha actual_bytes normalized_sha normalized_bytes comparison
 rel=$(jq -r '.path | gsub("\\\\";"/") | capture("(?<rel>research/.*)$").rel' <<< "$entry")
 [[ "$rel" == research/* && "$rel" != *..* ]] || return 1
 path=$repo/$rel
 original_sha=$(jq -r .sha256 <<< "$entry"); original_bytes=$(jq -r .bytes <<< "$entry")
 actual_sha=$(sha "$path"); actual_bytes=$(bytes "$path")
 # Only a stream is re-encoded, never a source file. This is not a Windows execution.
 normalized_sha=$(sed 's/\r$//' "$path" | sed 's/$/\r/' | sha256sum | cut -d ' ' -f1)
 normalized_bytes=$(sed 's/\r$//' "$path" | sed 's/$/\r/' | wc -c)
 comparison=mismatch
 if [[ "$actual_sha" == "$original_sha" && "$actual_bytes" == "$original_bytes" ]]; then comparison=exact_bytes
 elif [[ "$normalized_sha" == "$original_sha" && "$normalized_bytes" == "$original_bytes" ]]; then comparison=in_memory_CRLF_reencode_matches; fi
 jq -n --arg rel "$rel" --arg hs "$original_sha" --argjson hb "$original_bytes" \
  --arg cs "$actual_sha" --argjson cb "$actual_bytes" --arg comparison "$comparison" \
  '{relative_path:$rel,historical_bytes:$hb,historical_sha256:$hs,checkout_bytes:$cb,checkout_sha256:$cs,comparison:$comparison,reencoded_file_written:false}'
}
while IFS= read -r entry; do
 rel=$(jq -r .relative_path <<< "$entry")
 [[ "$rel" != /* && "$rel" != *..* ]] || exit 1
 jq -n --argjson e "$entry" --arg h "$(sha "$package/$rel")" --argjson n "$(bytes "$package/$rel")" \
  '$e | {relative_path,original,derived,actual_bytes:$n,actual_sha256:$h,derived_integrity_match:(.derived.bytes==$n and .derived.sha256==$h),original_bytes_unchanged_according_to_manifest:(.original==.derived)}'
done < <(jq -c --arg c "$c/" --arg d "$d/" --arg cmd "$cmd_rel" '.entries[] | select((.relative_path|startswith($c)) or (.relative_path|startswith($d)) or .relative_path==$cmd)' "$manifest") > "$temp/selected.jsonl"
while IFS= read -r entry; do source_row "$entry"; done < <(jq -c '.files[]' "$package/$c/freeze.json") > "$temp/sources.jsonl"
while IFS= read -r entry; do source_row "$entry"; done < <(jq -c '.shared_source[]' "$package/$d/diagnostic-pretest.json") > "$temp/shared.jsonl"
cmd_bytes=$package/$cmd_rel
# Byte categories are counted as numeric octets, with no text decoding of CMD content.
od -An -v -tu1 "$cmd_bytes" | awk '
 { for(i=1;i<=NF;i++){count[$i]++; if(previous==13 && $i==10)crlf++;previous=$i} }
 END { printf "{\"CRLF\":%d,\"LF\":%d,\"NUL\":%d,\"double_quotes\":%d,\"forward_slashes\":%d,\"backslashes\":%d}\n",crlf,count[10],count[0],count[34],count[47],count[92] }' > "$temp/cmd-shape.json"
controls=0
for leaf in natural-verification.json nonzero-verification.json owned-child-verification.json; do
 if [[ -f "$package/$c/$leaf" ]]; then controls=$((controls+1)); fi
done
argv_readback=false; [[ ! -e "$package/$c/argv-readback.json" ]] || argv_readback=true
jq -n \
 --slurpfile manifest "$manifest" \
 --slurpfile selected "$temp/selected.jsonl" \
 --slurpfile sources "$temp/sources.jsonl" \
 --slurpfile shared "$temp/shared.jsonl" \
 --slurpfile command "$package/$c/configure-release-command.json" \
 --slurpfile result "$package/$c/configure-release-result.json" \
 --slurpfile verification "$package/$c/configure-release-verification.json" \
 --slurpfile control_state "$package/$c/state.json" \
 --slurpfile contract "$package/$c/contract.json" \
 --slurpfile freeze "$package/$c/freeze.json" \
 --slurpfile pretest "$package/$d/diagnostic-pretest.json" \
 --slurpfile oracle "$package/$d/diagnostic-oracle.json" \
 --slurpfile failure "$package/$d/failure-classification.json" \
 --slurpfile build_state "$package/$d/state.json" \
 --slurpfile final "$package/$d/final-receipt.json" \
 --slurpfile shape "$temp/cmd-shape.json" \
 --rawfile stdout "$package/$c/configure-release.stdout.log" \
 --rawfile stderr "$package/$c/configure-release.stderr.log" \
 --arg manifest_sha "$(sha "$manifest")" --arg c "$c/" --arg d "$d/" --arg cmd_rel "$cmd_rel" \
 --argjson stdout_n "$(bytes "$package/$c/configure-release.stdout.log")" \
 --argjson stderr_n "$(bytes "$package/$c/configure-release.stderr.log")" \
 --argjson final_n "$(bytes "$package/$d/final-receipt.json")" \
 --argjson controls "$controls" --argjson argv_readback "$argv_readback" '
 $result[0].facts as $f |
 ($contract[0].stages[] | select(.name=="configure-release")) as $spec |
 ($manifest[0].entries | map({key:.relative_path,value:.})|from_entries) as $entries |
 {
 schema:"phigros.round2.windows_receipts_audit.v2",
 mode:"offline_read_only_bash_jq_file_hash_receipt_maintenance",
 package_manifest_sha256:$manifest_sha,supplied_manifest_repo_head:$manifest[0].repo_head,
 selected_integrity:$selected,
 configure:{
  attempt:$result[0].attempt,command_spec_equals_contract:($command[0].spec==$spec),
  command_pending:$command[0].pending,command_initial_launch:$command[0].launch,
  spec_executable:$spec.exe,root_image:$f.root_image,argv:$spec.argv,
  expected_native_exit:$spec.native_exit,expected_runner_exit:$spec.runner_exit,
  actual_facts:($f|{launch,created,resumed,assigned,root_exited,identity_trusted,root_image,exit_code,runner_exit,elapsed_s,active_at_exit,active_final,natural_quiescence,cleanup,held_all_signaled,streams_completed,stdout_seen,stdout_written,stderr_seen,stderr_written,stdout_overflow,stderr_overflow,errors,errors_overflow,termination_returned}),
  result_facts_equal_verification:($f==$verification[0].facts),
  verification_exit:$verification[0].verification_exit,verification_reason:$verification[0].reason,
  stderr_ascii:$stderr,stdout_bytes:$stdout_n,stderr_bytes:$stderr_n,
  stream_lengths_match_facts:($stdout_n==$f.stdout_seen and $stdout_n==$f.stdout_written and $stderr_n==$f.stderr_seen and $stderr_n==$f.stderr_written),
  initial_snapshot_identity_matches_root:all($f.snapshots[0].members[]; .pid==$f.pid and .creation_filetime==$f.creation_filetime and .image==$f.root_image and .identity_complete and .membership_before and .membership_after),
  held_waits_within_deadline:all($f.held_waits[]; .signaled and .end_s<=.deadline_s),
  state_status:$control_state[0].status,state_reason:$control_state[0].reason,state_revision:$control_state[0].revision,
  state_running:$control_state[0].running,state_native_stages_consumed:$control_state[0].native_stages_consumed,
  state_native_launches_known:$control_state[0].native_launches_known,
  successful_control_receipt_references:($control_state[0].receipts|length),
  successful_control_receipts_in_package:$controls,
  runtime_commandline_to_CreateProcess_record_present:($command[0]|has("commandline_to_CreateProcess")),
  argv_readback_in_package:$argv_readback,
  wrapper_entry_or_vcvars_or_cmake_markers_present:(($stdout+$stderr)|test("TRACE |CMAKE|vcvars")),
  dependency_references:($contract[0].dependencies|length),
  contract_historical_sha_matches_state:($entries[$c+"contract.json"].original.sha256==$control_state[0].contract_sha and $entries[$c+"contract.json"].original.sha256==$freeze[0].contract_sha),
  freeze_historical_sha_matches_state:($entries[$c+"freeze.json"].original.sha256==$control_state[0].freeze_sha)
 },
 frozen_source_comparisons:$sources,
 frozen_source_summary:($sources|group_by(.comparison)|map({key:.[0].comparison,value:length})|from_entries),
 diagnostic_shared_source_comparisons:$shared,
 diagnostic:{
  attempt:$build_state[0].attempt,
  oracle_case_tuples_equal_result:([$oracle[0].cases[]|[.id,.mode,.expected]]==[$pretest[0].cases[]|[.id,.mode,.expected]]),
  case_count:($pretest[0].cases|length),passed:([$pretest[0].cases[]|select(.pass==true)]|length),
  failed_reported:$pretest[0].failed,failed_recomputed:([$pretest[0].cases[]|select(.pass==false)]|length),
  failed_cases:[$pretest[0].cases[]|select(.pass==false)|{id,mode,expected,error,pass,facts}],
  actual_CreateProcess:$pretest[0].actual_CreateProcess,fake_facts_not_native_verification:$pretest[0].fake_facts_not_native_verification,
  oracle_rounds:$oracle[0].rounds,oracle_actual_CreateProcess_allowed:$oracle[0].actual_CreateProcess_allowed,
  root_prefix_comparison:{
   actual_path:$failure[0].actual_path,required_prefix:$failure[0].required_prefix,
   ascii_case_insensitive_literal_startswith:(($failure[0].actual_path|ascii_downcase)|startswith($failure[0].required_prefix|ascii_downcase)),
   slash_normalized_literal_startswith:(($failure[0].actual_path|gsub("/";"\\")|ascii_downcase)|startswith($failure[0].required_prefix|ascii_downcase)),
   windows_or_powershell_executed:false,canonicalization_or_security_validation:false
  },state:$build_state[0],
  final_native_controls:$final[0].native_controls,final_native_wrapper_probes:$final[0].native_wrapper_probes,
  final_native_product_commands:$final[0].native_product_commands,final_candidate_layer_cases:$final[0].candidate_layer_cases,
  final_product_adopted:$final[0].product_adopted,final_out_new:$final[0].capacity.out_new,
  final_self_bytes_is_historical:$final[0].self_bytes,actual_derived_final_bytes:$final_n
 },
 cmd_shape:($shape[0]+{original_entry:$entries[$cmd_rel].original,derived_entry:$entries[$cmd_rel].derived,runtime_equivalence_proven:false}),
 limits:["No Windows, PowerShell, cmd, vcvars, compiler or supplied runner executed.",
  "Historical dependency/protection/control receipt closure is not included.",
  "Source CRLF re-encoding is a streamed hash comparison, not the supplied derivative CMD.",
  "Path string comparison is not a full transaction or guard-security test.",
  "A9 cmd syntax offending step remains unknown."]
 }' > "$temp/audit.json"
jq -e '
 (.selected_integrity|length==15) and all(.selected_integrity[];.derived_integrity_match) and
 (.frozen_source_comparisons|length==23) and (.diagnostic_shared_source_comparisons|length==3) and all(.frozen_source_comparisons[];.comparison!="mismatch") and all(.diagnostic_shared_source_comparisons[];.comparison=="exact_bytes") and
 .configure.command_spec_equals_contract and .configure.result_facts_equal_verification and .configure.stream_lengths_match_facts and
 .configure.contract_historical_sha_matches_state and .configure.freeze_historical_sha_matches_state and
 .configure.initial_snapshot_identity_matches_root and .configure.held_waits_within_deadline and
 .diagnostic.oracle_case_tuples_equal_result and .diagnostic.failed_reported==2 and .diagnostic.failed_recomputed==2
' "$temp/audit.json" > /dev/null
cat "$temp/audit.json"
