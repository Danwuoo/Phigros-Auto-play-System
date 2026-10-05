. "$PSScriptRoot/x2-budget.ps1"
$manifest=Get-Content (Join-Path $script:X2Campaign 'contact-replay-x1/input-manifest.json') -Raw|ConvertFrom-Json -AsHashtable
$manifest.batch_root=$script:X2Batch
$manifest.campaign_before_bytes=X2-Bytes $script:X2Campaign
$manifest.status_working_sha256=X2-Sha (Join-Path $script:X2Repo 'docs/PROJECT_STATUS_NEXT_STEPS_20261001.md')
$manifest.experiment='confirmed_preserve_winner_only_x2';$manifest.batch_limit_bytes=[long]67108864
$manifest.x1_reference_manifest_sha256='99e8a364ea9f8575d67fbe39b1b76cccd6820816bc4d999d1b1bdfcfec2dd955'
$manifest.roles=@{}
foreach($which in '36','50') {
  $dest=Join-Path $script:X2Repo "out/x2/tools$which"
  if(Test-Path -LiteralPath $dest){throw 'Frozen tools must be new'}
  New-Item -ItemType Directory -Path $dest|Out-Null
  foreach($name in 'pas_frame_review.exe','x1_tests.exe','x1_subject_prefix_report.exe','z.dll','gtest.dll','gtest_main.dll') {Copy-Item -LiteralPath (Join-Path $script:X2Repo "out/x2/build$which/Release/$name") -Destination $dest}
}
foreach($role in 'c36h_reference','main50_control','main50_no_confirmed_winner_override') {
  $p=Join-Path $script:X2Batch "source/$role-source-provenance.json"
  $source=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json
  $binary=Join-Path $script:X2Repo $(if($role -eq 'c36h_reference'){'out/x2/tools36/pas_frame_review.exe'}else{'out/x2/tools50/pas_frame_review.exe'})
  $manifest.roles[$role]=@{lineage=$source.lineage;variant=$source.variant;source_provenance_path=$p;source_provenance_sha256=(X2-Sha $p);tracking_source_sha256=$source.instrumented_source_sha256.'src/game_tracking.cpp';binary_path=$binary;binary_sha256=(X2-Sha $binary)}
}
X2-Json (Join-Path $script:X2Batch 'input-manifest.json') $manifest
X2-Json (Join-Path $script:X2Batch 'input-equivalence.json') @{x1_manifest_sha256=$manifest.x1_reference_manifest_sha256;x2_manifest_sha256=(X2-Sha (Join-Path $script:X2Batch 'input-manifest.json'));equal_fields=@('session_root','index_sha256','profile','profile_sha256','windows','assumptions');differences='batch root, experiment budget and role/source/binary binding metadata; input_manifest file SHA intentionally different';all_primary_runs_use_same_x2_manifest=$true}
Write-Output 'Three explicit X2 roles share the same new input manifest.'
