$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue"
$batchPath="$campaignPath/pending-cancel-x10d-p"
$acceptPath="$campaignPath/pending-cancel-x10d-p-controller"
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$v){$data=[Text.UTF8Encoding]::new($false).GetBytes(($v|ConvertTo-Json -Depth 80)+"`n");$s=[IO.File]::Open("$acceptPath/$name",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$s.Write($data,0,$data.Length)}finally{$s.Dispose()}}
if(Test-Path $acceptPath){throw 'new acceptance already exists; inspect without overwriting'}
if((Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")+8388608 -gt 8589934592){throw 'acceptance reserve'}
New-Item -ItemType Directory $acceptPath|Out-Null
Save 'protocol.json' @{scope='Controller independent review: frozen binding and inverse patch, rerun six existing test targets including intentional failures, recompute three C++ reports; no build/full replay/live';limit_bytes=8388608;campaign_limit_bytes=8589934592;head=(git rev-parse HEAD);script_sha256=(Hash $PSCommandPath)}
$summary=Get-Content -Raw "$batchPath/final-summary.json"|ConvertFrom-Json
$freeze=Get-Content -Raw "$batchPath/final-source-freeze.json"|ConvertFrom-Json
if((Hash "$batchPath/final-source-freeze.json") -ne $summary.source_freeze_sha256){throw 'freeze hash'}
if((Hash "$batchPath/source-binding-before-replay.json") -ne $freeze.source_before_replay_sha256){throw 'pre-replay binding'}
$binding=Get-Content -Raw "$batchPath/source-binding-before-replay.json"|ConvertFrom-Json
$checks=0
foreach($f in $binding.sources){if((Hash "$repoPath/$($f.path)") -ne $f.sha256){throw "source changed $($f.path)"};$checks++}
foreach($f in $binding.export_files){if((Hash "$repoPath/out/x10d-p/$($f.role)/$($f.path)") -ne $f.sha256){throw 'export changed'};$checks++}
foreach($f in $binding.binaries){if((Hash "$repoPath/$($f.path)") -ne $f.sha256){throw 'binary changed'};$checks++}
foreach($f in $freeze.extra_sources){if((Hash "$batchPath/final-source/$($f.path)") -ne $f.sha256){throw 'snapshot changed'};$checks++}
foreach($f in $freeze.analysis_binary_and_linked_core){if((Hash "$repoPath/$($f.path)") -ne $f.sha256){throw 'analysis binary changed'};$checks++}
$ledger=Get-Content -Raw "$batchPath/artifact-ledger.json"|ConvertFrom-Json
foreach($f in $ledger.artifacts){if((Hash "$batchPath/$($f.path)") -ne $f.sha256 -or (Get-Item -LiteralPath "$batchPath/$($f.path)").Length -ne $f.bytes){throw "artifact changed $($f.path)"}}
$changed=@()
foreach($f in $binding.export_files){
 $parent="$repoPath/out/x10c/baseline/$($f.path)"
 if((Hash $parent) -ne $f.sha256){$changed+="$($f.role)/$($f.path)"}
}
if($changed.Count -ne 1 -or $changed[0] -ne 'variant/src/game.cpp'){throw 'nonisolated patch'}
$base=[IO.File]::ReadAllText("$repoPath/out/x10d-p/baseline/src/game.cpp").Replace("`r`n","`n")
$variant=[IO.File]::ReadAllText("$repoPath/out/x10d-p/variant/src/game.cpp").Replace("`r`n","`n")
$hook=@'
        // A newer complete snapshot with no current Note must withdraw a
        // pending Down immediately. Missing grace applies only after a
        // contact has started; it cannot license a new touch from old pixels.
        if(identity.submitted) if(const auto cursor=scheduler_.executed_steps(identity.intent);
           cursor&&*cursor==0) {
            cancel_contact(id,identity,"pending_down_current_object_missing");
            continue;
        }
'@
$hook=$hook.Replace("`r`n","`n")+"`n"
if($variant.Replace($hook,'') -ne $base -or !$variant.Contains($hook)){throw 'inverse patch'}
if(![IO.File]::ReadAllText("$repoPath/src/game.cpp").Replace("`r`n","`n").Contains($hook)){throw 'donor hook changed'}
foreach($r in $summary.runs){
 foreach($pair in @(@('summary.json','summary_sha256'),@('events.jsonl','events_sha256'),@('state-digests.jsonl','state_digests_sha256'))){if((Hash "$batchPath/$($r.name)/$($pair[0])") -ne $r.($pair[1])){throw 'run binding'}}
 $s=Get-Content -Raw "$batchPath/$($r.name)/summary.json"|ConvertFrom-Json
 if(!$s.success -or $s.verified_pngs -ne 7722 -or $s.consumed_frames -ne 7715 -or $s.contacts_at_exit -ne 0 -or $s.input_truncated -or $s.suppression){throw 'denominator'}
 if($s.input_manifest_sha256 -ne (Hash "$batchPath/input-manifest.json")){throw 'input binding'}
}
foreach($f in @('events.jsonl','state-digests.jsonl')){
 if((Hash "$batchPath/variant-on-1/$f") -ne (Hash "$batchPath/variant-off-1/$f")){throw 'ON/OFF mismatch'}
 if((Hash "$batchPath/baseline-on-1/$f") -ne (Hash "$campaignPath/hold-cascade-x10c/baseline-on-1/$f") -or (Hash "$batchPath/baseline-on-1/$f") -ne (Hash "$campaignPath/hold-cascade-x10c/baseline-off-1/$f")){throw 'baseline bridge'}
}
Save 'binding-checks.json' @{source_export_binary_snapshot_bindings=$checks;artifact_ledger_entries=$ledger.artifacts.Count;only_changed_export=$changed;inverse_restores_parent=$true;reference_bridge=$true;trace_on_off_equal=$true;original_summary_sha256=(Hash "$batchPath/final-summary.json");original_ledger_sha256=(Hash "$batchPath/artifact-ledger.json")}
Write-Output "Bindings passed: $checks and $($ledger.artifacts.Count) ledger artifacts."
$results=@()
foreach($t in $summary.tests){
 $original=Get-Content -Raw "$batchPath/$($t.name)-command.json"|ConvertFrom-Json
 $env:PAS_RGB_CLIP_ROOT=$original.PAS_RGB_CLIP_ROOT
 $argsList=@('--gtest_brief=1',"--gtest_output=xml:$acceptPath/$($t.name).xml")
 Save "$($t.name)-command.json" @{exe=$original.exe;binary_sha256=(Hash $original.exe);arguments=$argsList;PAS_RGB_CLIP_ROOT=$env:PAS_RGB_CLIP_ROOT;expected_failures=$t.failures}
 & $original.exe @argsList *> "$acceptPath/$($t.name).log"
 $code=$LASTEXITCODE
 [xml]$x=Get-Content -Raw "$acceptPath/$($t.name).xml"
 $cases=@($x.testsuites.testsuite.testcase)
 $failed=@($cases|Where-Object {$null -ne $_.failure}|ForEach-Object {$_.name}|Sort-Object)
 $expected=@($t.failures|Sort-Object)
 Save "$($t.name)-exit.json" @{exit=$code;tests=$cases.Count;failures=$failed;log_sha256=(Hash "$acceptPath/$($t.name).log");xml_sha256=(Hash "$acceptPath/$($t.name).xml")}
 if($cases.Count -ne $t.tests -or [int]$x.testsuites.errors -ne 0 -or [int]$x.testsuites.disabled -ne 0 -or @($cases|Where-Object {$null -ne $_.skipped}).Count -ne 0 -or ($failed -join '|') -ne ($expected -join '|') -or $code -ne [int]($expected.Count -gt 0)){throw "unexpected test result $($t.name)"}
 $results+=@{name=$t.name;tests=$cases.Count;failures=$failed;expected_failures_exact=$true;exit=$code}
 Write-Output "$($t.name): $($cases.Count) tests, $($failed.Count) expected failures."
}
$reports=@(
 @{name='action-audit';exe="$repoPath/out/x10d-p/build/Release/x10d_audit.exe";args=@("$batchPath/baseline-on-1","$batchPath/variant-on-1");old='full-action-audit-1.json'},
 @{name='lifecycle-audit';exe="$repoPath/out/x10d-p/build/Release/x10d_lifecycle_audit.exe";args=@("$batchPath/baseline-on-1","$batchPath/variant-on-1","$batchPath/full-action-audit-1.json");old='lifecycle-audit-1.json'},
 @{name='causal-review';exe="$repoPath/out/x10d-p/review/Release/x10d_causal_review.exe";args=@($batchPath);old='causal-review.json'}
)
foreach($r in $reports){
 Save "$($r.name)-command.json" @{exe=$r.exe;binary_sha256=(Hash $r.exe);arguments=$r.args}
 $a=$r.args
 & $r.exe @a > "$acceptPath/$($r.name).json" 2> "$acceptPath/$($r.name).stderr"
 if($LASTEXITCODE -ne 0){throw "analysis failed $($r.name)"}
 $fresh=Get-Content -Raw "$acceptPath/$($r.name).json"|ConvertFrom-Json
 $old=Get-Content -Raw "$batchPath/$($r.old)"|ConvertFrom-Json
 if(($fresh|ConvertTo-Json -Depth 80 -Compress) -ne ($old|ConvertTo-Json -Depth 80 -Compress)){throw "report differs $($r.name)"}
 Write-Output "$($r.name): recomputation identical."
}
git diff --quiet HEAD -- src include CMakeLists.txt
if($LASTEXITCODE -ne 0){throw 'formal source changed'}
Save 'verification-summary.json' @{status='mechanical verification passed; final controller decision in acceptance document';tests=$results;bindings=$checks;ledger_artifacts=$ledger.artifacts.Count;recomputed_reports=3;new_full_replays=0;new_builds=0;new_live=0;formal_source_unchanged=$true;batch_bytes_before_summary=(Bytes $acceptPath);limit_bytes=8388608}
if((Bytes $acceptPath) -gt 8388608 -or (Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001") -gt 8589934592){throw 'acceptance capacity'}
Write-Output 'Controller mechanical checks complete.'
