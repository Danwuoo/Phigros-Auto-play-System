. "$PSScriptRoot/x2-budget.ps1"
$export=Join-Path $script:X2Repo 'out/x2/main50-winner-only'
$evidence=Join-Path $script:X2Batch 'source'
if(Test-Path -LiteralPath $export){throw 'X2 export must be new'}
X2-Check 2097152
$code=X2-Invoke 'prepare-base' 'powershell.exe' @('-NoProfile','-File',"$PSScriptRoot/prepare-contact-replay.ps1",'-Lineage','main50','-ExportRoot',$export,'-EvidenceRoot',$evidence)
if($code){throw 'X2 base preparation failed'}
$file=Join-Path $export 'src/game_tracking.cpp'
$base=[IO.File]::ReadAllText($file)
$content=$base
function X2-Replace([string]$old,[string]$new) {
  if(!$script:content.Contains($old)){throw "X2 anchor missing: $old"}
  if(($script:content.Split(@($old),[StringSplitOptions]::None)).Length -ne 2){throw "X2 anchor not unique: $old"}
  $script:content=$script:content.Replace($old,$new)
}
$script:content=$content
X2-Replace '#include "replay_trace.hpp"' "#include `"replay_trace.hpp`"`n#include `"x2_ablation.hpp`""
X2-Replace 'bool preserve_confirmed=false;' @'
const auto x2_pre_winner=selected?selected->track_id:0;
        const auto x2_prior_confirmed=match->confirmed_line_id;
        bool x2_override_applied=false;
        bool preserve_confirmed=false;
'@
# This is the only altered strategy statement. Flag assignment stays untouched.
X2-Replace 'selected=&*established;' 'if(x2::winner_override_enabled) {selected=&*established;x2_override_applied=true;}'
X2-Replace 'const bool relation_ambiguous=!preserve_confirmed&&second_score-best_score<8;' @'
const auto x2_post_winner=selected?selected->track_id:0;
        const bool relation_ambiguous=!preserve_confirmed&&second_score-best_score<8;
'@
X2-Replace 'if(x1_trace_enabled)x1_emit({{"event","relation_final"}' @'
nlohmann::json x2_choice={{"event","x2_winner_only"},{"note_id",target.note_id},
            {"evidence_ns",now},{"source_frame",out.context.frame},
            {"preserve_eligible",preserve_confirmed},{"preserve_flag",preserve_confirmed},
            {"preserve_flag_meaning","original_eligibility_flag_including_ambiguity_bypass"},
            {"override_enabled",x2::winner_override_enabled},{"override_applied",x2_override_applied},
            {"pre_override_winner",x2_pre_winner},{"post_override_winner",x2_post_winner},
            {"confirmed_line_id",x2_prior_confirmed},{"best_score",best_score},{"second_score",second_score},
            {"ambiguity",relation_ambiguous},{"identity_ambiguous",ambiguous},
            {"relation_conflict",relation_conflict},{"final_line_id",target.line_id},
            {"final_valid_relation",selected&&!ambiguous&&target.line_id!=0},
            {"projection_only",target.line_projection_only},{"samples",target.samples},{"reason",target.reason}};
        x2::selection(x2_choice);if(x1_trace_enabled)x1_emit(std::move(x2_choice));
        if(x1_trace_enabled)x1_emit({{"event","relation_final"}
'@
[IO.File]::WriteAllText($file,$script:content,[Text.UTF8Encoding]::new($false))
X2-Write (Join-Path $evidence 'x2-instrumented-before.cpp') $base
X2-Write (Join-Path $evidence 'x2-instrumented-after.cpp') $script:content
$diff=@(git diff --no-index -- (Join-Path $evidence 'x2-instrumented-before.cpp') $file)
X2-Write (Join-Path $evidence 'winner-only.patch') ($diff -join "`n")
$baseProvenance=Get-Content (Join-Path $evidence 'main50-source-provenance.json') -Raw|ConvertFrom-Json
$baseProvenance.instrumented_source_sha256.'src/game_tracking.cpp'=X2-Sha $file
foreach($role in 'main50_control','main50_no_confirmed_winner_override') {
  $p=@{role=$role;variant=if($role -eq 'main50_control'){'winner_override_enabled'}else{'winner_override_disabled_flag_preserved'};lineage='main50';export_root=$export;strategy='50/27/11';base_provenance_sha256=(X2-Sha (Join-Path $evidence 'main50-source-provenance.json'));saved_commit=$baseProvenance.saved_commit;base_source_sha256=$baseProvenance.base_source_sha256;instrumented_source_sha256=$baseProvenance.instrumented_source_sha256;intervention='only selected=&*established is conditional; preserve_confirmed and downstream gates unchanged';patch_sha256=(X2-Sha (Join-Path $evidence 'winner-only.patch'));switch_default=$true;production_checkout_modified=$false;offline_tests_excluded_transport_cases=$baseProvenance.offline_tests_excluded_transport_cases}
  X2-Json (Join-Path $evidence "$role-source-provenance.json") $p
}
$ref=Get-Content (Join-Path $script:X2Campaign 'contact-replay-x1/source-v2/c36h-source-provenance.json') -Raw|ConvertFrom-Json
X2-Json (Join-Path $evidence 'c36h_reference-source-provenance.json') @{role='c36h_reference';variant='c36h_unmodified_reference';lineage='c36h';strategy='37/19';export_root=$ref.export_root;original_provenance_sha256=(X2-Sha (Join-Path $script:X2Campaign 'contact-replay-x1/source-v2/c36h-source-provenance.json'));freeze_commit=$ref.freeze_commit;freeze_dirty=$ref.freeze_dirty;frozen_binary_sha256=$ref.frozen_binary_sha256;base_source_sha256=$ref.base_source_sha256;instrumented_source_sha256=$ref.instrumented_source_sha256;offline_tests_excluded_transport_cases=$ref.offline_tests_excluded_transport_cases}
Write-Output 'X2 main50 export patched; strategy intervention is one conditional winner assignment.'
