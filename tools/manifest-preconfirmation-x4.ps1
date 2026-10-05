. "$PSScriptRoot/x4-budget.ps1"
$tool=Join-Path $script:X4Repo 'out/x4/tools/x4_tool.exe'
$oldPath=Join-Path $script:X4Campaign 'confirmed-preserve-x2/input-manifest.json'
$packetPath=Join-Path $script:X4Batch 'oracle-packet.json'
$code=X4-Invoke 'packet' $tool @('packet',$oldPath,$packetPath);if($code){throw 'packet failed'}
$m=Get-Content -LiteralPath $oldPath -Raw|ConvertFrom-Json -AsHashtable
$old=$m.Clone()
$m.experiment='preconfirmation_role_oracle_x4';$m.batch_limit_bytes=25165824;$m.batch_root=$script:X4Batch
$m.Remove('roles');$m.Remove('x1_reference_manifest_sha256')
$acceptance=Join-Path $script:X4Campaign 'confirmed-preserve-x2/acceptance-20261002/acceptance-summary.json'
$x3Acceptance=Join-Path $script:X4Campaign 'line-role-x3/acceptance-20261002/acceptance-summary.json'
$m.parent=@{manifest_path=$oldPath;manifest_sha256=(X4-Sha $oldPath);acceptance_path=$acceptance;acceptance_path_sha256=(X4-Sha $acceptance);x3_acceptance_path=$x3Acceptance;x3_acceptance_path_sha256=(X4-Sha $x3Acceptance);bridge='Reused runs were produced by original X2 manifest; new input freezes identical pixels/profile/windows/clock/receipt policy with different output batch and single intervention.'}
$m.reused_runs=@{}
foreach($role in 'c36h_reference','main50_control') {
  $name=if($role -eq 'c36h_reference'){'c36h-reference-on-1'}else{'main50-control-on-1'}
  $root=Join-Path $script:X4Campaign "confirmed-preserve-x2/$name"
  $files=@{};foreach($f in 'summary.json','trace.jsonl','events.jsonl','state-digests.jsonl','first-intervention.jsonl'){$files[$f]=X4-Sha (Join-Path $root $f)}
  $m.reused_runs[$role]=@{root=$root;role=$role;old_input_manifest_sha256=(X4-Sha $oldPath);files=$files;source_binding=$old.roles[$role]}
}
$p=Join-Path $script:X4Batch 'source/main50_preconfirmation_role_oracle-source-provenance.json'
$binary=Join-Path $script:X4Repo 'out/x4/tools/pas_frame_review.exe'
$m.variant_source=@{role='main50_preconfirmation_role_oracle';variant='proposed_role_winner_only';lineage='main50';binary_path=$binary;binary_sha256=(X4-Sha $binary);source_provenance_path=$p;source_provenance_sha256=(X4-Sha $p);tracking_source_sha256=(X4-Sha "$script:X4Repo/out/x4/main50-provisional/src/game_tracking.cpp")}
$m.oracle=@{path=$packetPath;sha256=(X4-Sha $packetPath);grade='proposed';runtime_ID_binding=$false;future_feedback=$false}
$m.run_policy=@{runs_max=2;first='trace_on';second='trace_off';cadence='owner';tie='frame-first';recognition='zero_fake_time';receipt_policy='success_zero_duration_five_contacts';preroll_max=32;from='complete original available prefix';to='EOF'}
$m.campaign_before_bytes=X4-Bytes $script:X4Campaign
X4-Json (Join-Path $script:X4Batch 'input-manifest.json') $m
Write-Output (X4-Sha (Join-Path $script:X4Batch 'input-manifest.json'))
