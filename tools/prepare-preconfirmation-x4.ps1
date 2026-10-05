. "$PSScriptRoot/x4-budget.ps1"
$export=Join-Path $script:X4Repo 'out/x4/main50-provisional'
if(Test-Path -LiteralPath $export){throw 'X4 export must be new'}
X4-Check 2097152
Copy-Item -LiteralPath (Join-Path $script:X4Repo 'out/x1/main50-v2') -Destination $export -Recurse
$file=Join-Path $export 'src/game_tracking.cpp'
$base=[IO.File]::ReadAllText($file)
$script:content=$base
function X4-Replace([string]$old,[string]$new) {
  if(($script:content.Split(@($old),[StringSplitOptions]::None)).Length -ne 2){throw "X4 anchor missing/nonunique: $old"}
  $script:content=$script:content.Replace($old,$new)
}
X4-Replace '#include "replay_trace.hpp"' "#include `"replay_trace.hpp`"`n#include `"x4_oracle.hpp`""
X4-Replace '        // A crossing line can momentarily be nearer' @'
        auto x4_choice=x4::choose(n,notes,out,match->confirmed_line_id,selected,best_score,second_score,match->id);
        // A crossing line can momentarily be nearer
'@
X4-Replace '            bool local_continuation=false;' @'
            if(!x4_choice.is_null())x4_choice["conflict_old_visible"]=old_visible;
            bool local_continuation=false;
'@
X4-Replace '                local_continuation=alignment>=.97&&' @'
                if(!x4_choice.is_null()) {x4_choice["continuation_tangent_dot"]=alignment;x4_choice["continuation_prior_hit_gap"]=std::abs(normal_distance(prior_hit,*selected));}
                local_continuation=alignment>=.97&&
'@
X4-Replace '        if(x1_trace_enabled)x1_emit({{"event","relation_final"}' @'
        x4::finish(std::move(x4_choice),target,ambiguous,relation_ambiguous,preserve_confirmed,relation_conflict,match->confirmed_line_id);
        if(x1_trace_enabled)x1_emit({{"event","relation_final"}
'@
[IO.File]::WriteAllText($file,$script:content,[Text.UTF8Encoding]::new($false))
X4-Write (Join-Path $script:X4Batch 'source/tracking-before.cpp') $base
X4-Write (Join-Path $script:X4Batch 'source/tracking-after.cpp') $script:content
$patch=@(git diff --no-index -- (Join-Path $script:X4Batch 'source/tracking-before.cpp') $file)
X4-Write (Join-Path $script:X4Batch 'source/provisional-only.patch') ($patch -join "`n")
$parentPath=Join-Path $script:X4Campaign 'contact-replay-x1/source-v2/main50-source-provenance.json'
$p=Get-Content -LiteralPath $parentPath -Raw|ConvertFrom-Json
$compiled=@{};foreach($f in Get-ChildItem "$export/src","$export/include" -File -Recurse){$compiled[$f.FullName.Substring($export.Length+1).Replace('\','/')]=X4-Sha $f.FullName}
X4-Json (Join-Path $script:X4Batch 'source/main50_preconfirmation_role_oracle-source-provenance.json') @{role='main50_preconfirmation_role_oracle';variant='proposed_role_winner_only';lineage='main50';strategy='50/27/11';saved_commit=$p.saved_commit;base_source_sha256=$p.base_source_sha256;instrumented_source_sha256=$compiled;export_root=$export;parent_provenance_path=$parentPath;parent_provenance_sha256=(X4-Sha $parentPath);base_instrumented_tracking_sha256=(X4-Sha (Join-Path $script:X4Batch 'source/tracking-before.cpp'));intervention='after score loop, before original preserve: selected pointer only, confirmed==0 and same-frame packet geometry; confirmed assignment remains unconditional';patch_sha256=(X4-Sha (Join-Path $script:X4Batch 'source/provisional-only.patch'));switch_default=$false;production_checkout_modified=$false;offline_tests_excluded_transport_cases=$p.offline_tests_excluded_transport_cases}
Write-Output 'X4 export prepared; confirmed winner override untouched.'
