#!/usr/bin/env python3
"""Bounded, source-only Windows recipe checks. Never launches native binaries.

This is a maintenance check, not a PowerShell parser, CMake configure/build,
MSVC test run, process qualification, or measured cost campaign.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CURRENT = 'research/prelive_current/'
AUDIT = 'research/prelive_audit/'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def ps_delimiters(source):
    """Check ordinary quoted strings/comments and delimiter balance only."""
    stack, quote, i = [], None, 0
    pairs = {')': '(', ']': '[', '}': '{'}
    while i < len(source):
        char = source[i]
        if quote:
            if quote == '"' and char == '`':
                i += 2
                continue
            if char == quote:
                if i + 1 < len(source) and source[i + 1] == quote:
                    i += 2
                    continue
                quote = None
        elif char in "'\"":
            quote = char
        elif char == '#':
            end = source.find('\n', i)
            i = len(source) if end == -1 else end
            continue
        elif char in '([{':
            stack.append(char)
        elif char in ')]}':
            if not stack or stack.pop() != pairs[char]:
                return False
        i += 1
    return quote is None and not stack

def checks(cmake, run, audit):
    result = []
    def check(name, condition):
        result.append({'id': name, 'pass': bool(condition)})
    targets = {
        'cost_analysis_tests': 'tests/cost_analysis_tests.cpp',
        'cost_analysis_review_tests': 'tests/cost_analysis_review_tests.cpp',
        'input_closure_review_tests': 'tests/input_closure_review_tests.cpp',
        'producer_admission_review_tests': 'tests/producer_admission_review_tests.cpp',
        'cost_workload_family': 'cost_workload_family.cpp',
        'cost_workload_family_v2': 'cost_workload_family_v2.cpp',
        'cost_workload_family_verify': 'cost_workload_family_verify.cpp',
        'cost_workload_family_v2_verify': 'cost_workload_family_v2_verify.cpp',
        'verify_tail_family_goal': 'research/prelive_portable/verify_tail_family_goal.cpp',
        'prelive_audit': 'research/prelive_audit/audit.cpp',
        'disabled_live_seam_tests': 'disabled_live_seam_tests.cpp',
    }
    for target, source in targets.items():
        check('cmake_target:' + target,
              ('add_executable(' + target + ' ') in cmake and source in cmake)
    check('auditor_object_uses_exact_source',
          'add_library(producer_review_audit OBJECT "${PAS_REPO}/research/prelive_audit/audit.cpp")' in cmake)
    check('only_auditor_object_entry_is_renamed',
          'target_compile_definitions(producer_review_audit PRIVATE main=raw_audit_entry)' in cmake)
    check('producer_review_links_real_auditor_object',
          'target_link_libraries(producer_admission_review_tests PRIVATE producer_review_audit nlohmann_json::nlohmann_json)' in cmake)
    check('no_portable_shim_added_to_windows_build', '/shim' not in cmake)
    for label, text in [('current', run), ('audit', audit)]:
        check(label + '_source_delimiter_sanity', ps_delimiters(text))
        check(label + '_explicit_gate_parameter', '[Parameter(Mandatory)][string]$GateReceipt' in text)
        check(label + '_explicit_tool_parameter', '[Parameter(Mandatory)][string]$ToolSnapshot' in text)
        check(label + '_no_historical_gate_default', "native-qualification-01/native-gate.json'" not in text)
        check(label + '_gate_schema_and_controls',
              "pas.windows-offline-process-gate.v1'" in text and '$g.native_controls_verified -ne 4' in text and '$g.file_only_failed -ne 0' in text)
        check(label + '_gate_input_entries_verified', 'CheckEntry $g.diagnostic' in text and 'foreach($e in $v.entries){CheckEntry $e;' in text)
        check(label + '_tool_copies_hash_checked', '$f.copy' in text and "$f.sha256){throw 'tool-snapshot-changed'}" in text)
        check(label + '_complete_owned_receipt_checked',
              "$s.status -cne 'COMPLETE'" in text and '$s.freeze_sha -cne' in text and '$s.contract_sha -cne' in text and 'CheckEntry $anchors[0]' in text)
        check(label + '_process_identity_quiescence_streams_held',
              all(token in text for token in ['$r.facts.identity_trusted', '$r.facts.natural_quiescence', '$r.facts.streams_completed', '$r.facts.held_all_signaled', '$r.facts.active_final -ne 0']))
        check(label + '_exact_build_binding_and_all_artifacts',
              'pas.current-windows-build-binding.v1' in text and '$seal.build_root -cne $root' in text and '$seal.gate.path -cne $gate' in text and '$seal.tool_snapshot.path -cne $tool' in text and '$seal.files' in text and '$seal.build_receipts' in text)
        check(label + '_all_build_freeze_dependencies_in_inputs', '$files.path' in text and '$seal.source_files.path' in text)
        check(label + '_unchanged_owned_core_invocation', "tools/zero_miss_windows/invoke.ps1'" in text)
        check(label + '_retained_capacity_and_free_floor', '12884901888' in text and '21474836480' in text and '67108864' in text)
        check(label + '_create_new_metadata', '[IO.FileMode]::CreateNew' in text)
        check(label + '_no_destructive_cleanup_or_device_invocation',
              not any(token in text for token in ['Remove-Item', 'Start-Process', 'manual-session --', 'adb.exe', 'emulator.exe']))
    check('preserved_initial_source_audit_guard', "throw 'initial-audit-required'" in run and "throw 'initial-audit-required'" in audit)
    check('preserved_bridge_source_before_guard', "build-bridge-withdraw-'" in run and "formal-source-before.json'" in run and "throw 'before-source-freeze'" in run)
    check('current_native_root_and_gate_tool_frozen', 'exe=$executable;argv=@($arguments)' in run and '@($PSCommandPath,$pwsh,$gate,$tool,' in run)
    check('all_native_roots_are_current_sealed_binaries', "throw 'native-not-in-current-build-binding'" in run and '$audit -notin $seal.files.path' in audit)
    check('current_unit_exact_cli', "'cost_analysis_tests.exe') @($fixture,$repo) $inputs" in run)
    check('current_review_exact_cli', "'cost_analysis_review_tests.exe') @($fixture,$review) @($inputs+$fixtureFiles.path)" in run)
    check('review_fixtures_restored_and_rechecked', 'foreach($f in $fixtureFiles){CheckEntry $f}' in run)
    check('current_input_closure_review_exact_cli', "'input_closure_review_tests.exe') @($fixture,$closureReview) @($inputs+$fixtureFiles.path)" in run)
    check('current_producer_exact_cli', "'producer_admission_review_tests.exe') @($producer) $inputs" in run)
    check('fixtures_produced_before_review', run.index("'cost_analysis_tests.exe'") < run.index("'cost_analysis_review_tests.exe'"))
    check('nested_test_and_auditor_sources_in_build_inputs', "Join-Path $PSScriptRoot 'tests'" in run and "Join-Path $repo 'research/prelive_audit'" in run)
    check('unit_profile_source_and_protocol_frozen', 'configs/phigros-hd-assist-five-lead35.json' in run and 'PRELIVE_CURRENT_COST_PROTOCOL.md' in run)
    check('explicit_fresh_family_operation', "'family'" in run and "throw 'explicit-frozen-v1-before-report-required'" in run)
    check('v1_family_verifier_exact_cli', "'cost_workload_family_verify.exe') @($FamilyBefore,$family1,$verify1)" in run)
    check('v2_family_verifier_exact_cli', "'cost_workload_family_v2_verify.exe') @($family2,$verify2)" in run)
    check('normal_tail_goal_exact_cli', "'verify_tail_family_goal.exe') @($family2,$tailGoal)" in run)
    check('family_inputs_frozen_between_producer_and_verifier', '@($inputs+$FamilyBefore+$family1)' in run and '@($inputs+$family2+$verify2)' in run)
    check('fresh_five_family_outputs', 'foreach($path in @($family1,$verify1,$family2,$verify2,$tailGoal)){Fresh $path}' in run)
    check('analysis_uses_direct_current_binary', "Native $stage (Join-Path $root 'cost_analysis.exe')" in run)
    check('pixels_and_pipeline_use_direct_current_binary', "Native $stage (Join-Path $root 'current_chain.exe')" in run)
    check('pixel_bytes_in_owned_inputs', '@($selected.frames.path)' in run)
    check('auditor_uses_fresh_same_tree_build', "$audit=Join-Path $root 'prelive_audit.exe'" in audit and "$audit=Join-Path $repo 'out/prelive-audit-release-01/prelive_audit.exe'" not in audit)
    check('auditor_direct_exact_cli', 'exe=$audit;argv=@($Report,$output)' in audit)
    check('auditor_report_raw_sidecars_frozen', '@($suffix|ForEach-Object {$Report+$_})' in audit)
    check('auditor_capacity_and_receipt_after_stage', "Capacity $name 'after';$afterRecorded=$true" in audit and 'Receipt $name $ExpectedNative' in audit)
    for label, text in [('current', run), ('audit', audit)]:
        check(label + '_bounded_input_closure_schemas', all(schema in text for schema in ['pas.current-input-closure.v1', 'pas.current-input-closure-leaf.v1', 'pas.current-input-closure-verification.v1']))
        check(label + '_bounded_leaf_and_total_counts', '$canonical.Count -gt 8192' in text and '$offset+=64' in text and '$leaves.Count -gt 128' in text)
        check(label + '_closure_root_declares_fresh_receipt', 'verification_output=$verificationPath' in text and 'Fresh $verificationPath' in text)
        check(label + '_closure_pre_post_hash_checks', 'foreach($f in @($closure.leaves)+@($closure.files)){CheckEntry $f}' in text)
        check(label + '_closure_receipt_core_joins', 'root=$closure.root;state=$state;verification=$verification' in text)
        check(label + '_closure_failure_writes_STOP', "$status=if($pass){'COMPLETE'}else{'STOP'}" in text and ('ClosureReceipt $stage $false $reason' in text or 'ClosureReceipt $name $false $reason' in text))
        check(label + '_closure_byte_limits', 'NewJson $leafPath $leaf 32768' in text and '} 1048576' in text)
        check(label + '_build_requires_closure_pass', "throw 'build-input-closure-not-verified'" in text)
    check('native_campaign_is_direct_anchor', 'if($Campaign){$direct+=@($Campaign)}' in run)
    check('source_migration_external_seal_and_commit', '$SourceMigrationSha256' in run and '$m.target_commit -cne $CandidateCommit' in run and "throw 'handoff-source-migration-seal'" in run)
    check('source_migration_exact_lf_crlf_hashes', "@('LF','CRLF')" in run and "throw 'migration-precomputed-target-encoding-proof'" in run and "throw 'migration-exact-current-target-bytes'" in run)
    check('source_migration_original_bytes_proven', '$previous.preserved_original' in run and '$previous.canonical_original' in run and "throw 'migration-original-encoding-proof'" in run)
    check('original_game_before_exemption_closed', "throw 'original-before-source-requires-target-migration'" in run)
    check('source_migration_full_old_path_anchored', 'Join-Path $historicalRoot $rel' in run and "throw 'migration-old-source-entry-join'" in run)
    check('source_migration_new_headers_bound', all(path in run for path in ['include/pas/candidate_admission.hpp', 'include/pas/candidate_packet.hpp', 'research/prelive_current/owner_candidate_step.hpp', 'research/prelive_current/receipt_stream.hpp']))
    check('portable_migration_repo_and_packet_paths', 'MigrationBlob $f $repo' in run and 'MigrationBlob $row.canonical_target $packet' in run and "throw 'migration-reparse'" in run and "throw 'migration-path-scope'" in run)
    check('migration_requires_new_closure_header', 'research/prelive_current/input_closure.hpp' in run)
    return result

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True, help='Fresh static receipt JSON; never overwritten')
args = parser.parse_args()
if args.output.exists():
    raise SystemExit('fresh static receipt required')
cmake = (ROOT / CURRENT / 'CMakeLists.txt').read_text()
run = (ROOT / CURRENT / 'run.ps1').read_text()
audit = (ROOT / AUDIT / 'run.ps1').read_text()
rows = checks(cmake, run, audit)
# Check the actual source entry signatures, without executing those sources.
for path, signature in {
    CURRENT + 'tests/cost_analysis_tests.cpp': 'if(argc!=3)',
    CURRENT + 'tests/cost_analysis_review_tests.cpp': 'if(argc!=3)',
    CURRENT + 'tests/input_closure_review_tests.cpp': 'if(argc!=3',
    CURRENT + 'disabled_live_seam_tests.cpp': 'if(argc!=2',
    CURRENT + 'tests/producer_admission_review_tests.cpp': 'if(argc!=2)',
    CURRENT + 'cost_workload_family.cpp': 'if(argc!=2||',
    CURRENT + 'cost_workload_family_v2.cpp': 'if(argc!=2||',
    CURRENT + 'cost_workload_family_verify.cpp': 'if(argc!=4||',
    CURRENT + 'cost_workload_family_v2_verify.cpp': 'if(argc!=3||',
    'research/prelive_portable/verify_tail_family_goal.cpp': 'if(argc!=3||',
}.items():
    rows.append({'id': 'source_cli_arity:' + path, 'pass': signature in (ROOT / path).read_text()})
# These deliberately corrupted in-memory recipes must fail focused checks.
mutations = [
    ('missing_real_auditor_object', cmake.replace('main=raw_audit_entry', 'main=wrong_entry'), run, audit),
    ('missing_unit_invocation', cmake, run.replace("'cost_analysis_tests.exe'", "'missing-unit.exe'"), audit),
    ('missing_review_fixture_freeze', cmake, run.replace('@($inputs+$fixtureFiles.path)', '@($inputs)'), audit),
    ('missing_normal_tail_goal', cmake, run.replace("'verify_tail_family_goal.exe'", "'missing-goal.exe'"), audit),
    ('missing_explicit_gate', cmake, run.replace('[Parameter(Mandatory)][string]$GateReceipt', '[string]$GateReceipt'), audit),
    ('old_auditor_binary', cmake, run, audit.replace("$audit=Join-Path $root 'prelive_audit.exe'", "$audit=Join-Path $repo 'out/prelive-audit-release-01/prelive_audit.exe'")),
]
for name, c, r, a in mutations:
    try:
        rejected = not all(row['pass'] for row in checks(c, r, a))
    except ValueError:
        rejected = True
    rows.append({'id': 'static_negative_control:' + name, 'pass': rejected})
# The safety core and adapters are byte-identical to the pre-edit observation.
before = (ROOT / CURRENT / 'windows_recipe_evidence/source-before.sha256').read_text().splitlines()
for line in before:
    digest, path = line.split(maxsplit=1)
    if not path.startswith((CURRENT, AUDIT)):
        rows.append({'id': 'unchanged_safety_core:' + path, 'pass': sha(ROOT / path) == digest})
source_paths = [CURRENT + 'CMakeLists.txt', CURRENT + 'run.ps1', AUDIT + 'run.ps1',
                CURRENT + 'windows_recipe_evidence/validate.py',
                CURRENT + 'windows_recipe_evidence/WINDOWS_COMMANDS.md']
source_paths += [path for path in [
    CURRENT + 'cost_analysis.cpp', CURRENT + 'campaign_provenance.hpp', CURRENT + 'chain.cpp',
    CURRENT + 'owner_candidate_step.hpp', CURRENT + 'receipt_stream.hpp', CURRENT + 'input_closure.hpp', CURRENT + 'disabled_live_seam_tests.cpp',
    'include/pas/candidate_admission.hpp', 'include/pas/candidate_packet.hpp', 'include/pas/runtime.hpp',
    'include/pas/game_session.hpp', 'src/game_session.cpp', 'src/manual_session.cpp', 'CMakeLists.txt',
    CURRENT + 'producer_timing.hpp', CURRENT + 'offline_output.hpp', CURRENT + 'streaming_workload.hpp',
    CURRENT + 'tests/cost_analysis_tests.cpp', CURRENT + 'tests/cost_analysis_review_tests.cpp', CURRENT + 'tests/input_closure_review_tests.cpp',
    CURRENT + 'tests/producer_admission_review_tests.cpp', CURRENT + 'cost_workload_family.cpp',
    CURRENT + 'cost_workload_family_v2.cpp', CURRENT + 'cost_workload_family_verify.cpp',
    CURRENT + 'cost_workload_family_v2_verify.cpp', 'research/prelive_portable/verify_tail_family_goal.cpp',
    AUDIT + 'audit.cpp', 'tools/zero_miss_windows/invoke.ps1', 'tools/zero_miss_windows/process.ps1',
    'tools/zero_miss_windows/native-stage.ps1', 'research/x10d_o_bvi_build/owned.cs']]
receipt = {
    'schema': 'pas.windows-offline-recipe-static-review.v1',
    'scope': 'Source inspection, CLI-arity checks, simple delimiter lint, safety-core SHA parity and in-memory negative controls only.',
    'pass': all(row['pass'] for row in rows), 'checks': rows,
    'source_bindings': [{'relative_path': path, 'bytes': (ROOT / path).stat().st_size, 'sha256': sha(ROOT / path)} for path in source_paths],
    'powershell_parser_run': False, 'cmake_configure_run': False, 'windows_build_run': False,
    'windows_tests_run': False, 'windows_cost_qualified': False, 'live_ready': False,
    'limitations': [
        'Windows/PowerShell/MSVC are not available on this Linux host.',
        'The protected initial audit, formal-source-before and bridge-build freezes must be present and valid on Windows; none are bypassed.',
        'Actual compiler/SDK/vcpkg dependency closure, generated protobuf files, workstation paths and fresh native qualification must be independently verified before building.',
        'Static checks do not prove Windows execution, source-to-binary compilation, measured cost, physical acceptance or game adoption.',
    ],
}
with args.output.open('x') as stream:
    json.dump(receipt, stream, indent=2)
    stream.write('\n')
failed = [row['id'] for row in rows if not row['pass']]
print(json.dumps({'checks': len(rows), 'failed': failed, 'pass': receipt['pass'], 'receipt': str(args.output)}))
raise SystemExit(0 if receipt['pass'] else 1)
