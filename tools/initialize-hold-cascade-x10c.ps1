$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'hold-cascade-x10c'
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){if(!(Test-Path -LiteralPath $p)){return 0};[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$value){$data=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Depth 60)+"`n");$s=[IO.File]::Open("$batchPath/$name",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$s.Write($data,0,$data.Length)}finally{$s.Dispose()}}
if((Test-Path -LiteralPath $batchPath) -or (Test-Path -LiteralPath "$repoPath/out/x10c")){throw 'new output root required'}
$priorPath=Join-Path $repoPath 'measurements/research-next-20261001'
$existingBytes=(Bytes $campaignPath)+(Bytes $priorPath)
$freeBytes=(Get-PSDrive -Name C).Free
if($existingBytes+134217728 -gt 8589934592 -or $freeBytes -lt 805306368+134217728+5368709120){throw 'capacity reserve unavailable'}
New-Item -ItemType Directory $batchPath|Out-Null
Save 'workspace-before.json' @{head=(git rev-parse HEAD);branch=(git branch --show-current);status=@(git status --short);worktrees=@(git worktree list);campaign_plus_prior_bytes=$existingBytes;disk_free_bytes=$freeBytes;batch_limit_bytes=134217728;build_export_limit_bytes=805306368;protocol_sha256=(Hash "$repoPath/docs/HOLD_CASCADE_X10C_PROTOCOL_20261003.md")}
[IO.File]::Copy("$repoPath/docs/HOLD_CASCADE_X10C_PROTOCOL_20261003.md","$batchPath/protocol-before.md",$false)
$old=Join-Path $campaignPath 'hold-claim-x10b'
$freeze=Get-Content -LiteralPath "$old/source-freeze.json" -Raw|ConvertFrom-Json
if((Hash "$old/source-freeze.json") -ne '4907fdcde7d8f4ccbfc1ccbf9d83ac60529fd705c194b92a6aeaeafc8a88fcf4'){throw 'freeze binding'}
$checked=@()
foreach($e in $freeze.sources){$p="$old/source/$($e.path)";if((Hash $p) -ne $e.sha256){throw "frozen source mismatch $p"};$checked+=@{kind='source_snapshot';path=$p;sha256=$e.sha256}}
foreach($e in $freeze.export_files){$p="$($freeze.actual_export_root)/$($e.path)";if((Hash $p) -ne $e.sha256){throw "export mismatch $p"};$checked+=@{kind='export';path=$p;sha256=$e.sha256}}
foreach($e in $freeze.binaries){$p="$repoPath/$($e.path)";if((Hash $p) -ne $e.sha256){throw "binary mismatch $p"};$checked+=@{kind='binary';path=$p;sha256=$e.sha256}}
$tests=@()
foreach($spec in @(@('original-tests-1.xml',207),@('witness-tests-1.xml',13),@('asan-original-tests.xml',207),@('asan-witness-tests.xml',13))){
  [xml]$x=Get-Content -LiteralPath "$old/$($spec[0])" -Raw;$t=$x.testsuites
  $cases=@($t.testsuite.testcase);$skip=@($cases|Where-Object {$null -ne $_.skipped}).Count
  if([int]$t.tests -ne $spec[1] -or $cases.Count -ne $spec[1] -or [int]$t.failures -ne 0 -or [int]$t.errors -ne 0 -or [int]$t.disabled -ne 0 -or $skip -ne 0){throw 'test XML invalid'}
  $tests+=@{file=$spec[0];sha256=(Hash "$old/$($spec[0])");tests=$cases.Count;skip=$skip;failures=0;case_names=@($cases|ForEach-Object {"$($_.classname).$($_.name)"})}
}
$runs=@();$summaries=@{}
$manifestHash=Hash "$old/input-manifest.json"
foreach($name in @('baseline-1','variant-1','variant-off-1')){
  $p="$old/$name";$s=Get-Content -LiteralPath "$p/summary.json" -Raw|ConvertFrom-Json
  if(!$s.success -or $s.verified_pngs -ne 7722 -or $s.consumed_frames -ne 7715 -or $s.preroll_frames -ne 32 -or $s.contacts_at_exit -ne 0 -or $s.input_truncated -or $s.input_manifest_sha256 -ne $manifestHash){throw 'run denominator invalid'}
  if($s.recognition -ne 'zero_fake_time' -or $s.cadence -ne 'owner' -or $s.tie -ne 'frame-first' -or $s.receipt_policy -ne 'success_zero_duration_five_contacts'){throw 'policy mismatch'}
  $expected=if($name -eq 'baseline-1'){'5919a4d1316b4e7a8a6bc4d44a416f606e5108a1e97a95f7255dfb1fff5902b0'}else{'f164a3fdd6e43bf7b6017b5cf4c956b7b6ca25d6e64fc6e87cf0e68ffb08bfd5'}
  if($s.binary_sha256 -ne $expected){throw 'binary identity mismatch'}
  $provenance=if($name -eq 'baseline-1'){"$campaignPath/contact-replay-x1/source-v2/c36h-source-provenance.json"}else{"$old/c36h-source-provenance.json"}
  if((Hash $provenance) -ne $s.source_provenance_sha256){throw 'source provenance mismatch'}
  $summaries[$name]=$s
  $runs+=@{name=$name;summary_sha256=(Hash "$p/summary.json");events_sha256=(Hash "$p/events.jsonl");semantic_sha256=$s.semantic_sha256;binary_sha256=$s.binary_sha256}
}
if((Hash "$old/variant-1/events.jsonl") -ne (Hash "$old/variant-off-1/events.jsonl") -or $summaries['variant-1'].semantic_sha256 -ne $summaries['variant-off-1'].semantic_sha256){throw 'trace changed policy'}
$original="$campaignPath/contact-replay-x1/c36h-final-on-1"
$ref=Get-Content -LiteralPath "$original/summary.json" -Raw|ConvertFrom-Json
if((Hash "$old/baseline-1/events.jsonl") -ne (Hash "$original/events.jsonl") -or $ref.semantic_sha256 -ne $summaries['baseline-1'].semantic_sha256){throw 'baseline bridge'}
$prov=Get-Content -LiteralPath "$old/c36h-source-provenance.json" -Raw|ConvertFrom-Json
foreach($p in $prov.instrumented_source_sha256.PSObject.Properties){if((Hash "$($freeze.actual_export_root)/$($p.Name)") -ne $p.Value){throw 'provenance/export mismatch'}}
git diff --quiet HEAD -- src include CMakeLists.txt
if($LASTEXITCODE -ne 0){throw 'production differs'}
Save 'x10b-independent-file-audit.json' @{schema=1;checked=$checked;tests=$tests;runs=$runs;source_freeze_sha256=(Hash "$old/source-freeze.json");comparison_sha256=(Hash "$old/comparison-1.json");action_audit_sha256=(Hash "$old/full-action-audit.json");source_binding_verified=$true;historical_xml_verified=$true;historical_xml_is_new_execution=$false;baseline_bridge=$true;trace_on_off_equal=$true;new_full_replays=0;candidate_acceptance=$false;notes=@('Independent rehash of frozen sources/export/binaries and historical outputs. Source hook and comparison logic are reviewed separately.','No new audit of all raw PNG until new replay; original summaries retain their historical scope.')}
Write-Output "X10b file audit passed; campaign=$existingBytes disk=$freeBytes"
