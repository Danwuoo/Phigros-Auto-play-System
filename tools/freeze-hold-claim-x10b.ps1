$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'hold-claim-x10b'
$exportPath=Join-Path $repoPath 'out/x10b/c36h-interior'
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function SaveNew($path,$value){$data=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Depth 40)+"`n");$s=[IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$s.Write($data,0,$data.Length)}finally{$s.Dispose()}}
if((Test-Path "$batchPath/source") -or (Test-Path "$batchPath/final-summary.json")){throw 'freeze already started; inspect, do not overwrite'}
$testResults=@()
foreach($spec in @(@('original-tests-1.xml',207),@('witness-tests-1.xml',13),@('asan-original-tests.xml',207),@('asan-witness-tests.xml',13))){
  [xml]$xml=Get-Content -LiteralPath "$batchPath/$($spec[0])" -Raw
  $t=$xml.testsuites
  $skipped=@($t.testsuite.testcase|Where-Object {$null -ne $_.skipped}).Count
  if([int]$t.tests -ne $spec[1] -or [int]$t.failures -ne 0 -or [int]$t.errors -ne 0 -or [int]$t.disabled -ne 0 -or $skipped -ne 0){throw "test verification failed: $($spec[0])"}
  $testResults+=@{file=$spec[0];tests=[int]$t.tests;failures=0;skipped=0;sha256=(Hash "$batchPath/$($spec[0])")}
}
$runResults=@();$summaries=@{}
foreach($name in @('baseline-1','variant-1','variant-off-1')){
  $s=Get-Content -LiteralPath "$batchPath/$name/summary.json" -Raw|ConvertFrom-Json
  if(!$s.success -or $s.verified_pngs -ne 7722 -or $s.consumed_frames -ne 7715 -or $s.preroll_frames -ne 32 -or $s.contacts_at_exit -ne 0 -or $s.input_truncated){throw "replay verification failed: $name"}
  if($s.recognition -ne 'zero_fake_time' -or $s.tie -ne 'frame-first' -or $s.cadence -ne 'owner'){throw 'clock policy mismatch'}
  if($name -ne 'variant-off-1' -and $s.window_frames -ne 156){throw 'window mismatch'}
  $summaries[$name]=$s
  $runResults+=@{name=$name;summary_sha256=(Hash "$batchPath/$name/summary.json");events_sha256=(Hash "$batchPath/$name/events.jsonl");semantic_sha256=$s.semantic_sha256;binary_sha256=$s.binary_sha256;contacts_at_exit=0;verified_pngs=7722;consumed_frames=7715;window_frames=$s.window_frames}
}
if($summaries['variant-1'].semantic_sha256 -ne $summaries['variant-off-1'].semantic_sha256 -or (Hash "$batchPath/variant-1/events.jsonl") -ne (Hash "$batchPath/variant-off-1/events.jsonl")){throw 'on/off difference'}
$originalRun=Join-Path $campaignPath 'contact-replay-x1/c36h-final-on-1'
if(!(Test-Path "$originalRun/summary.json")){throw 'reference path missing'}
$original=Get-Content -LiteralPath "$originalRun/summary.json" -Raw|ConvertFrom-Json
if($original.semantic_sha256 -ne $summaries['baseline-1'].semantic_sha256 -or (Hash "$originalRun/events.jsonl") -ne (Hash "$batchPath/baseline-1/events.jsonl")){throw 'baseline bridge changed'}
$binding=Get-Content -LiteralPath "$batchPath/patch-binding.json" -Raw|ConvertFrom-Json
if((Hash "$exportPath/src/game.cpp") -ne $binding.after_game_sha256 -or (Hash "$repoPath/apps/frame_review/x10b_claim.hpp") -ne $binding.helper_sha256){throw 'variant source changed'}
$provenance=Get-Content -LiteralPath "$batchPath/c36h-source-provenance.json" -Raw|ConvertFrom-Json
foreach($p in $provenance.instrumented_source_sha256.PSObject.Properties){if((Hash "$exportPath/$($p.Name)") -ne $p.Value){throw "export changed: $($p.Name)"}}
git -C $repoPath diff --quiet HEAD -- src include CMakeLists.txt
if($LASTEXITCODE -ne 0){throw 'formal production differs from HEAD'}
$sourceFiles=@(Get-ChildItem -LiteralPath "$repoPath/apps/frame_review" -File -Recurse)+@(
  Get-Item -LiteralPath "$repoPath/tests/contact_replay_tests.cpp","$repoPath/tests/x10b_claim_tests.cpp","$repoPath/tools/hold-claim-x10b.ps1","$repoPath/tools/freeze-hold-claim-x10b.ps1","$repoPath/docs/CURRENT_INTERIOR_CLAIM_X10B_20261003.md"
)
$sourceManifest=@()
foreach($file in $sourceFiles){
  $relative=$file.FullName.Substring($repoPath.Length+1).Replace('\','/')
  $dest=Join-Path "$batchPath/source" $relative
  [IO.Directory]::CreateDirectory((Split-Path $dest))|Out-Null
  [IO.File]::Copy($file.FullName,$dest,$false)
  $sourceManifest+=@{path=$relative;sha256=(Hash $dest)}
}
$exportHashes=@()
foreach($file in Get-ChildItem -LiteralPath $exportPath -File -Recurse){$exportHashes+=@{path=$file.FullName.Substring($exportPath.Length+1).Replace('\','/');sha256=(Hash $file.FullName)}}
$binaryFiles=@(Get-ChildItem -LiteralPath "$repoPath/out/x10b/build","$repoPath/out/x10b/asan" -File -Recurse|Where-Object {$_.Extension -in @('.exe','.dll')})+@(Get-Item -LiteralPath "$repoPath/out/x10/build/Release/x10_compare.exe","$repoPath/out/x1/repair-tools36/pas_frame_review.exe")
$binaries=@();foreach($file in $binaryFiles){$binaries+=@{path=$file.FullName.Substring($repoPath.Length+1).Replace('\','/');sha256=(Hash $file.FullName)}}
if((Hash "$repoPath/out/x10b/build/reused/Release/pas_frame_review.exe") -ne $summaries['variant-1'].binary_sha256){throw 'replay binary mismatch'}
SaveNew "$batchPath/source-freeze.json" @{
  schema=1;head=(git -C $repoPath rev-parse HEAD);actual_export_root=$exportPath
  inherited_provenance_export_root_note='c36h-source-provenance.json retains its parent export_root; actual variant source root is explicitly recorded here; inherited snapshot_checks describe parent history, not new edits'
  sources=$sourceManifest;export_files=$exportHashes;binaries=$binaries
  production_src_include_root_cmake_match_HEAD=$true
  raw_protection_scope='No original raw copied or modified. Replay summaries record 7722 verified PNGs. Freeze rechecks instrumented export hashes, baseline events/digest and source/binary bindings; does not claim a new hash audit of every old raw file.'
}
$buildBytes=Bytes "$repoPath/out/x10b"
if($buildBytes -gt 536870912){throw 'build cap'}
$result=[ordered]@{
  schema=1;decision='offline result reproducible; adoption withheld pending unexplained ordinal2883 action divergence'
  new_full_replays=3;new_live=0;formal_candidate_accepted=$false;independent_agent_acceptance=$false
  tests=$testResults;runs=$runResults;trace_on_off_events_and_semantic_equal=$true;baseline_bridge_equal=$true
  source_freeze_sha256=(Hash "$batchPath/source-freeze.json");comparison_sha256=(Hash "$batchPath/comparison-1.json");full_action_audit_sha256=(Hash "$batchPath/full-action-audit.json")
  full_prefix_first_state_divergence='unknown';gameplay_effect='unknown';remaining_variant_actions_after_unmatched=1453
  batch_bytes=0;batch_limit_bytes=134217728;batch_remaining_bytes=0
  campaign_plus_prior_bytes=0;campaign_limit_bytes=8589934592;campaign_remaining_bytes=0
  export_build_bytes=$buildBytes;export_build_limit_bytes=536870912;export_build_remaining_bytes=(536870912-$buildBytes)
  note='Whole action audit uses bounded deletion alignment; not a full equivalence proof. Further experiments require a new batch/export; no builds in frozen out/x10b. Capacity includes this final summary.'
}
SaveNew "$batchPath/final-summary.json" $result
for($i=0;$i -lt 6;$i++){
  $batchBytes=Bytes $batchPath
  $campaignBytes=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")
  if($batchBytes -gt 134217728 -or $campaignBytes -gt 8589934592){throw 'data cap'}
  if($result.batch_bytes -eq $batchBytes -and $result.campaign_plus_prior_bytes -eq $campaignBytes){break}
  $result.batch_bytes=$batchBytes;$result.batch_remaining_bytes=134217728-$batchBytes
  $result.campaign_plus_prior_bytes=$campaignBytes;$result.campaign_remaining_bytes=8589934592-$campaignBytes
  [IO.File]::WriteAllText("$batchPath/final-summary.json",($result|ConvertTo-Json -Depth 40)+"`n",[Text.UTF8Encoding]::new($false))
}
if((Bytes $batchPath) -ne $result.batch_bytes){throw 'capacity did not settle'}
$result|ConvertTo-Json -Depth 8
