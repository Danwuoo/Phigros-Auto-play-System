$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue"
$batchPath="$campaignPath/pending-cancel-x10d-p"
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$value){$s=[IO.File]::Open("$batchPath/$name",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$b=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Depth 60)+"`n");$s.Write($b,0,$b.Length)}finally{$s.Dispose()}}
$binding=Get-Content -LiteralPath "$batchPath/source-binding-before-replay.json" -Raw|ConvertFrom-Json
foreach($f in $binding.sources){if((Hash "$repoPath/$($f.path)") -ne $f.sha256){throw "source changed $($f.path)"}}
foreach($f in $binding.export_files){if((Hash "$repoPath/out/x10d-p/$($f.role)/$($f.path)") -ne $f.sha256){throw 'export changed'}}
foreach($f in $binding.binaries){if((Hash "$repoPath/$($f.path)") -ne $f.sha256){throw 'binary changed'}}
$before=Get-Content -LiteralPath "$batchPath/workspace-before.json" -Raw|ConvertFrom-Json
foreach($f in $before.frozen_checked){if((Hash $f.path) -ne $f.sha256){throw 'X10c frozen artifact changed'}}
$preserved=@();foreach($f in $before.existing_dirty_file_hashes){
 if($f.path -in @('docs/PROJECT_STATUS_NEXT_STEPS_20261001.md','docs/C36H_FORWARD_EXECUTION_PLAN_20261003.md','tools/prepare-pending-cancel-x10d-p.ps1','docs/PENDING_CANCEL_X10D_P_PROTOCOL_20261003.md')){continue}
 if((Hash "$repoPath/$($f.path)") -ne $f.sha256){throw "existing dirty file changed $($f.path)"};$preserved+=$f
}
git diff --quiet HEAD -- src include CMakeLists.txt;if($LASTEXITCODE -ne 0){throw 'production changed'}
$runs=@();foreach($spec in @(@('baseline','on'),@('variant','on'),@('variant','off'))){
 $role=$spec[0];$trace=$spec[1];$name="$role-$trace-1";$p="$batchPath/$name";$s=Get-Content -LiteralPath "$p/summary.json" -Raw|ConvertFrom-Json
 if(!$s.success -or $s.input_truncated -or $s.verified_pngs -ne 7722 -or $s.consumed_frames -ne 7715 -or $s.window_frames -ne 196 -or $s.preroll_frames -ne 32 -or $s.contacts_at_exit -ne 0 -or $s.suppression -ne $false){throw 'run denominator'}
 if($s.input_manifest_sha256 -ne (Hash "$batchPath/input-manifest.json") -or $s.source_provenance_sha256 -ne (Hash "$batchPath/$role-provenance.json") -or $s.binary_sha256 -ne (Hash "$repoPath/out/x10d-p/build/Release/$($role)_replay.exe")){throw 'run binding'}
 if($s.cadence -ne 'owner' -or $s.tie -ne 'frame-first' -or $s.recognition -ne 'zero_fake_time' -or $s.receipt_policy -ne 'success_zero_duration_five_contacts'){throw 'run policy'}
 if($trace -eq 'off' -and ($s.lifecycle_bytes -ne 0 -or $s.fallback_witness_rows -ne 0 -or $s.trace_peak_bytes -ne 0)){throw 'off diagnostic output'}
 $runs+=@{name=$name;summary_sha256=(Hash "$p/summary.json");events_sha256=(Hash "$p/events.jsonl");state_digests_sha256=(Hash "$p/state-digests.jsonl");semantic_sha256=$s.semantic_sha256;binary_sha256=$s.binary_sha256;lifecycle_bytes=$s.lifecycle_bytes;verified_pngs=$s.verified_pngs;consumed_frames=$s.consumed_frames;preroll_frames=$s.preroll_frames;window_frames=$s.window_frames;contacts_at_exit=$s.contacts_at_exit;host_cost_scope=$s.host_cpu_cost_scope;host_cpu_ms=$s.host_cpu_ms;jitter_p95_minus_p5_ms=$s.jitter_p95_minus_p5_ms}
}
$original="$campaignPath/hold-cascade-x10c/baseline-on-1"
$old=Get-Content -LiteralPath "$original/summary.json" -Raw|ConvertFrom-Json
if($old.semantic_sha256 -ne $runs[0].semantic_sha256 -or (Hash "$original/events.jsonl") -ne $runs[0].events_sha256 -or (Hash "$original/state-digests.jsonl") -ne $runs[0].state_digests_sha256){throw 'reference bridge'}
if($runs[1].semantic_sha256 -ne $runs[2].semantic_sha256 -or $runs[1].events_sha256 -ne $runs[2].events_sha256 -or $runs[1].state_digests_sha256 -ne $runs[2].state_digests_sha256){throw 'candidate diagnostics feedback'}
$audit=Get-Content -LiteralPath "$batchPath/full-action-audit-1.json" -Raw|ConvertFrom-Json
$life=Get-Content -LiteralPath "$batchPath/lifecycle-audit-1.json" -Raw|ConvertFrom-Json
$review=Get-Content -LiteralPath "$batchPath/causal-review.json" -Raw|ConvertFrom-Json
if($audit.unexamined_actions -ne 0 -or $life.fullprefix_frames -ne 7722 -or $life.observer_scene_differences -ne 0 -or $life.observer_history_differences -ne 0 -or $life.observer_bank_differences -ne 0){throw 'audit incomplete/observer changed'}
if($review.action_audit_sha256 -ne (Hash "$batchPath/full-action-audit-1.json") -or $review.lifecycle_audit_sha256 -ne (Hash "$batchPath/lifecycle-audit-1.json")){throw 'causal review binding'}
$extra=@();foreach($f in @(Get-Item -LiteralPath "$repoPath/docs/PENDING_CANCEL_X10D_P_RESULT_20261003.md","$repoPath/docs/PROJECT_STATUS_NEXT_STEPS_20261001.md","$repoPath/docs/C36H_FORWARD_EXECUTION_PLAN_20261003.md","$repoPath/apps/frame_review/x10d_p_causal_review.cpp","$repoPath/apps/frame_review/x10d_p_review_offline/CMakeLists.txt","$PSCommandPath")){
 $rel=$f.FullName.Substring($repoPath.Length+1).Replace('\','/');$dest="$batchPath/final-source/$rel";[IO.Directory]::CreateDirectory((Split-Path $dest))|Out-Null;[IO.File]::Copy($f.FullName,$dest,$false);$extra+=@{path=$rel;sha256=(Hash $dest)}
}
$analysisBindings=@();foreach($f in @(Get-Item -LiteralPath "$repoPath/out/x10d-p/review/Release/x10d_causal_review.exe","$repoPath/out/x10d-p/review/Release/z.dll","$repoPath/out/x10d-p/build/Release/baseline_core.lib")){$analysisBindings+=@{path=$f.FullName.Substring($repoPath.Length+1).Replace('\','/');sha256=(Hash $f.FullName)}}
if((Hash "$repoPath/out/x10d-p/review/Release/x10d_causal_review.exe") -ne $review.analysis_binary_sha256){throw 'review binary binding'}
Save 'final-source-freeze.json' @{schema=1;source_before_replay_sha256=(Hash "$batchPath/source-binding-before-replay.json");extra_sources=$extra;analysis_binary_and_linked_core=$analysisBindings;old_dirty_preserved=$preserved;formal_source_unchanged=$true;replay_source_binary_unchanged=$true}
$artifacts=@();foreach($f in Get-ChildItem -LiteralPath $batchPath -File -Recurse){$artifacts+=@{path=$f.FullName.Substring($batchPath.Length+1).Replace('\','/');bytes=$f.Length;sha256=(Hash $f.FullName)}}
Save 'artifact-ledger.json' @{schema=1;artifacts=$artifacts;scope='All existing batch artifacts before this ledger and final-summary; ledger and final-summary bytes included by settled final capacity. PNGs are references only.'}
$attempts=@(Get-ChildItem -LiteralPath $batchPath -Filter '*-replay-command.json');$failures=@();foreach($f in $attempts){$p=$f.FullName.Replace('-command.json','-exit.json');$e=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json;if($e.exit -ne 0){$failures+=$f.Name}}
$value=[ordered]@{schema=1;status='development and self-validation delivered; awaiting independent controller acceptance';decision=$review.decision;cold_contract_pass=$review.cold_contract_pass;formal_candidate_accepted=$false;baseline='C36h tint1 behavioral/experimental (77 Miss unchanged)';donor='main50 mechanism donor/control live0';suppression=$false;new_full_replays=$attempts.Count;replay_failures=$failures;new_live_rounds=0;tests=$binding.tests;reference_frozen_bridge=$true;reference_OFF_control='reused frozen X10c baseline ON/OFF with exact events/semantic/state-digest equality';candidate_ON_OFF_equal=$true;runs=$runs;action_denominator=@{reference=$audit.reference_actions;candidate=$audit.variant_actions;matched=$audit.matched_normalized_actions;unmatched_reference=$audit.unmatched_reference_actions;unmatched_candidate=$audit.unmatched_variant_actions;unexamined=$audit.unexamined_actions};kind_denominators=@{reference=$life.reference_denominators;candidate=$life.candidate_denominators};pending_cancels=$life.pending_cancels;first_owner_divergence=$life.first_owner_divergence;all_observer_digests_equal=$true;source_freeze_sha256=(Hash "$batchPath/final-source-freeze.json");action_audit_sha256=(Hash "$batchPath/full-action-audit-1.json");lifecycle_audit_sha256=(Hash "$batchPath/lifecycle-audit-1.json");causal_review_sha256=(Hash "$batchPath/causal-review.json");physical_gold=0;gameplay_effect='unknown';miss_improvement='unknown';cross_song='unknown';engineering_failures=@('ASan build1 succeeded; post-build debug z.dll copy failed before tests, corrected to installed zd.dll; logs retained. Intentional baseline red cases and original candidate contract failure retained separately.');capacity=@{batch_bytes=0;batch_limit_bytes=134217728;out_bytes=(Bytes "$repoPath/out/x10d-p");out_limit_bytes=805306368;campaign_plus_prior_bytes=0;campaign_limit_bytes=8589934592;disk_free_bytes=(Get-PSDrive -Name C).Free};next='Controller independent acceptance of X10d-P; X10d-O/X11/X12/live unstarted. No inference of suppression acceptance from this owner contract.'}
Save 'final-summary.json' $value
for($i=0;$i -lt 8;$i++){
 $b=Bytes $batchPath;$c=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")
 if($b -gt 134217728 -or $c -gt 8589934592 -or $value.capacity.out_bytes -gt 805306368){throw 'capacity exceeded'}
 if($b -eq $value.capacity.batch_bytes -and $c -eq $value.capacity.campaign_plus_prior_bytes){break}
 $value.capacity.batch_bytes=$b;$value.capacity.campaign_plus_prior_bytes=$c
 [IO.File]::WriteAllText("$batchPath/final-summary.json",($value|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))
}
if((Bytes $batchPath) -ne $value.capacity.batch_bytes){throw 'capacity did not settle'}
$value.capacity|ConvertTo-Json -Depth 10
