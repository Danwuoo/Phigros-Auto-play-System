$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'hold-cascade-x10c'
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$value){$data=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Depth 60)+"`n");$s=[IO.File]::Open("$batchPath/$name",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$s.Write($data,0,$data.Length)}finally{$s.Dispose()}}
$binding=Get-Content -LiteralPath "$batchPath/source-binding-before-replay.json" -Raw|ConvertFrom-Json
foreach($f in $binding.source_files){if((Hash "$repoPath/$($f.path)") -ne $f.sha256){throw "compiled source changed: $($f.path)"}}
foreach($f in $binding.export_files){if((Hash "$repoPath/out/x10c/$($f.role)/$($f.path)") -ne $f.sha256){throw 'export changed'}}
foreach($f in $binding.binaries){if((Hash "$repoPath/$($f.path)") -ne $f.sha256){throw 'bound binary changed'}}
$runs=@();$sources=@{}
foreach($role in @('baseline','variant')){
  $frozenRoot=if($role -eq 'baseline'){"$campaignPath/hold-claim-x10b/baseline-1"}else{"$campaignPath/hold-claim-x10b/variant-1"}
  $frozen=Get-Content -LiteralPath "$frozenRoot/summary.json" -Raw|ConvertFrom-Json
  foreach($trace in @('on','off')){
    $name="$role-$trace-1";$path="$batchPath/$name";$s=Get-Content -LiteralPath "$path/summary.json" -Raw|ConvertFrom-Json
    if(!$s.success -or $s.input_truncated -or $s.verified_pngs -ne 7722 -or $s.consumed_frames -ne 7715 -or $s.window_frames -ne 196 -or $s.preroll_frames -ne 32 -or $s.contacts_at_exit -ne 0){throw 'run denominator'}
    if($s.input_manifest_sha256 -ne (Hash "$batchPath/input-manifest.json") -or $s.source_provenance_sha256 -ne (Hash "$batchPath/$role-provenance.json") -or $s.binary_sha256 -ne (Hash "$repoPath/out/x10c/build/Release/$($role)_replay.exe")){throw 'run binding'}
    if($s.cadence -ne 'owner' -or $s.tie -ne 'frame-first' -or $s.recognition -ne 'zero_fake_time' -or $s.receipt_policy -ne 'success_zero_duration_five_contacts'){throw 'policy'}
    if($s.semantic_sha256 -ne $frozen.semantic_sha256 -or (Hash "$path/events.jsonl") -ne (Hash "$frozenRoot/events.jsonl")){throw 'frozen semantic bridge'}
    if($trace -eq 'off' -and ($s.fallback_witness_rows -ne 0 -or $s.trace_peak_bytes -ne 0)){throw 'off diagnostics retained'}
    $runs+=@{name=$name;summary_sha256=(Hash "$path/summary.json");events_sha256=(Hash "$path/events.jsonl");state_digests_sha256=(Hash "$path/state-digests.jsonl");semantic_sha256=$s.semantic_sha256;binary_sha256=$s.binary_sha256;fallback_witness_rows=$s.fallback_witness_rows;verified_pngs=$s.verified_pngs;consumed_frames=$s.consumed_frames;preroll_frames=$s.preroll_frames;window_frames=$s.window_frames;contacts_at_exit=$s.contacts_at_exit;host_cost_scope=$s.host_cpu_cost_scope;host_cpu_ms=$s.host_cpu_ms;jitter_p95_minus_p5_ms=$s.jitter_p95_minus_p5_ms}
  }
  if((Hash "$batchPath/$role-on-1/state-digests.jsonl") -ne (Hash "$batchPath/$role-off-1/state-digests.jsonl")){throw 'per-frame diagnostic altered state'}
}
$tests=@();foreach($spec in @(@('baseline-tests-1.xml',207),@('variant-tests-1.xml',207),@('diagnostic-tests-1.xml',9),@('asan-diagnostic-tests-2.xml',9))){
  [xml]$x=Get-Content -LiteralPath "$batchPath/$($spec[0])" -Raw;$t=$x.testsuites;$cases=@($t.testsuite.testcase)
  if([int]$t.tests -ne $spec[1] -or $cases.Count -ne $spec[1] -or [int]$t.failures -ne 0 -or [int]$t.errors -ne 0 -or [int]$t.disabled -ne 0 -or @($cases|Where-Object {$null -ne $_.skipped}).Count -ne 0){throw 'test XML'}
  $tests+=@{file=$spec[0];sha256=(Hash "$batchPath/$($spec[0])");tests=$cases.Count;failures=0;skip=0}
}
$audit=Get-Content -LiteralPath "$batchPath/full-action-audit-1.json" -Raw|ConvertFrom-Json
$old=Get-Content -LiteralPath "$batchPath/independent-old-action-audit.json" -Raw|ConvertFrom-Json
foreach($k in @('reference_actions','variant_actions','matched_normalized_actions','unmatched_reference_actions','unmatched_variant_actions','alignment_blocks','first_ordered_difference')){
  if(($audit.$k|ConvertTo-Json -Depth 60 -Compress) -ne ($old.$k|ConvertTo-Json -Depth 60 -Compress)){throw 'action re-audit mismatch'}
}
if($audit.reference_actions -ne 2165 -or $audit.variant_actions -ne 2163 -or $audit.matched_normalized_actions -ne 2161 -or $audit.unexamined_actions -ne 0 -or $audit.frame_digests.semantic_sha256.first -ne 2477){throw 'audit denominator'}
$causal=Get-Content -LiteralPath "$batchPath/causal-K2883.json" -Raw|ConvertFrom-Json
if($causal.frame_denominator -ne 31 -or $causal.first_684_target_divergence -ne 2881 -or $causal.first_684_history_divergence -ne 2881 -or !$causal.'2881_pre_suppression_witness_equal_except_applied'){throw 'causal report'}
$probe=Get-Content -LiteralPath "$batchPath/pending-probe.json" -Raw|ConvertFrom-Json
if(!$probe.mechanism_reproduced -or $probe.new_strategy_candidate){throw 'probe'}
git diff --quiet HEAD -- src include CMakeLists.txt
if($LASTEXITCODE -ne 0){throw 'production changed'}
$extraSource=@(Get-Item -LiteralPath "$repoPath/apps/frame_review/x10c_inspect.cpp","$repoPath/apps/frame_review/x10c_causal_report.cpp","$repoPath/apps/frame_review/x10c_pending_probe.cpp","$repoPath/apps/frame_review/x10c_inspect_offline/CMakeLists.txt","$repoPath/tools/verify-hold-cascade-x10c-exports.ps1","$repoPath/tools/finalize-hold-cascade-x10c.ps1","$repoPath/docs/HOLD_CASCADE_X10C_PROTOCOL_20261003.md","$repoPath/docs/HOLD_CASCADE_X10C_RESULT_20261003.md")
$extra=@();foreach($f in $extraSource){$rel=$f.FullName.Substring($repoPath.Length+1).Replace('\','/');$dest=Join-Path "$batchPath/final-source" $rel;[IO.Directory]::CreateDirectory((Split-Path $dest))|Out-Null;[IO.File]::Copy($f.FullName,$dest,$false);$extra+=@{path=$rel;sha256=(Hash $dest)}}
$extraBinaries=@(Get-ChildItem -LiteralPath "$repoPath/out/x10c/inspect/Release" -File|Where-Object {$_.Extension -in @('.exe','.dll')})+@(Get-Item -LiteralPath "$repoPath/out/x10c/build/Release/baseline_core.lib")
$analysisBindings=@();foreach($f in $extraBinaries){$analysisBindings+=@{path=$f.FullName.Substring($repoPath.Length+1).Replace('\','/');sha256=(Hash $f.FullName)}}
Save 'final-source-freeze.json' @{schema=1;source_before_replay_sha256=(Hash "$batchPath/source-binding-before-replay.json");extra_sources=$extra;analysis_binaries_and_linked_core=$analysisBindings;note='Replay sources/binaries unchanged after pre-replay binding. New standalone inspection/probe tools linked separately, no replay/core rebuild; original X10b source/binaries/evidence preserved.'}
$value=[ordered]@{schema=1;decision='X10b engineering evidence independently verified; standalone suppression family rejected for loss-of-target/pending-grace interaction, no candidate/live acceptance';new_full_replays=4;replay_failures=0;new_live_rounds=0;new_strategy_candidates=0;formal_candidate_accepted=$false;tests=$tests;runs=$runs;all_runs_frozen_bridge=$true;per_frame_trace_on_off_equal=$true;first_observable_semantic_state_divergence=2477;first_K684_local_divergence=2881;full_action_denominator=@{baseline=2165;variant=2163;matched_normalized=2161;unmatched_baseline=4;unmatched_variant=2;unexamined=0};raw_frame_digest_differences=$audit.frame_digests;normalized_selected_target_differences=$audit.trace_target_comparison;source_freeze_sha256=(Hash "$batchPath/final-source-freeze.json");action_audit_sha256=(Hash "$batchPath/full-action-audit-1.json");causal_report_sha256=(Hash "$batchPath/causal-K2883.json");fake_clock_probe_sha256=(Hash "$batchPath/pending-probe.json");engineering_failures=@('Release build2 and ASan build1: GTest exception macros on same source line; repaired without changing expectations; all logs retained');physical_identity_gold=0;gameplay_effect='unknown';capacity=@{batch_bytes=0;batch_limit_bytes=134217728;export_build_bytes=(Bytes "$repoPath/out/x10c");export_build_limit_bytes=805306368;campaign_plus_prior_bytes=0;campaign_limit_bytes=8589934592;disk_free_bytes=(Get-PSDrive -Name C).Free};next='Independent current-body claim ownership / explicit suppression and pending cancellation contract. Predeclare two-separated-Hold/pending-vs-active counterexamples before any new candidate; X11-X13 conditional and unstarted.'}
Save 'final-summary.json' $value
for($i=0;$i -lt 8;$i++){
  $b=Bytes $batchPath;$c=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")
  if($b -gt 134217728 -or $c -gt 8589934592 -or $value.capacity.export_build_bytes -gt 805306368){throw 'capacity'}
  if($value.capacity.batch_bytes -eq $b -and $value.capacity.campaign_plus_prior_bytes -eq $c){break}
  $value.capacity.batch_bytes=$b;$value.capacity.campaign_plus_prior_bytes=$c
  [IO.File]::WriteAllText("$batchPath/final-summary.json",($value|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))
}
if((Bytes $batchPath) -ne $value.capacity.batch_bytes){throw 'capacity did not settle'}
$value.capacity|ConvertTo-Json -Depth 5
