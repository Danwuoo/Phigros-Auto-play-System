$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'hold-ownership-x10d-o-bvi'
$outPath=Join-Path $repoPath 'out/x10d-o-bvi'
$timer=[Diagnostics.Stopwatch]::StartNew()
function Bytes($p){$sum=[long]0;foreach($f in Get-ChildItem -LiteralPath $p -File -Recurse){if($timer.Elapsed.TotalSeconds -gt 180){throw 'settle timeout'};$sum+=$f.Length};return $sum}
function Entry($p,$relative=$false){$f=Get-Item -LiteralPath $p;@{path=if($relative){[IO.Path]::GetRelativePath($batchPath,$f.FullName).Replace('\','/')}else{$f.FullName};bytes=$f.Length;sha256=(Get-FileHash -LiteralPath $f.FullName).Hash.ToLowerInvariant()}}
function Serialize($v){($v|ConvertTo-Json -Depth 24)+"`n"}
$ledgerPath="$batchPath/artifact-ledger.json";$receiptPath="$batchPath/final-receipt.json"
if((Test-Path -LiteralPath $ledgerPath)-or(Test-Path -LiteralPath $receiptPath)){throw 'settled output exists'}
$external=@(Get-ChildItem -LiteralPath $PSScriptRoot -File -Recurse|ForEach-Object{Entry $_.FullName})
$external+=@(Get-ChildItem -LiteralPath "$repoPath/docs" -File -Filter 'HOLD_OWNERSHIP_X10D_O_BVI*'|ForEach-Object{Entry $_.FullName})
$externalBytes=[long]0;foreach($f in $external){$externalBytes+=$f.bytes}
$files=@(Get-ChildItem -LiteralPath $batchPath -File -Recurse|ForEach-Object{Entry $_.FullName $true})
$ledger=@{schema=1;files=$files;external=$external;out=@(Get-ChildItem -LiteralPath $outPath -File -Recurse|ForEach-Object{Entry $_.FullName});self_and_receipt_excluded=$true;status='platform_partial_no_candidate_binary'}
$ledgerText=Serialize $ledger;$utf8=[Text.UTF8Encoding]::new($false)
$ledgerBytes=$utf8.GetByteCount($ledgerText)
$rootBase=Bytes $batchPath;$campaignBase=Bytes $campaignPath;$outBytes=Bytes $outPath
$carry=[long]45307809+72115+9546+54056+11851
$before=Get-Content -LiteralPath "$batchPath/protection-before.json" -Raw|ConvertFrom-Json
$after=Get-Content -LiteralPath "$batchPath/protection-after.json" -Raw|ConvertFrom-Json
$r=@{schema=1;date='2026-10-04';timezone='Asia/Taipei';decision='4I platform partial pending independent controller acceptance; no candidate validation';head=$after.head;index_sha256=$after.index_sha256;protected_files=$after.protected_count;original_git_paths=454;mismatches=$after.mismatches.Count;unchanged=$true;expanded_cases=69;top_vectors=20;supplemental_cases=22;case_layers_planned=207;case_layers_executed=0;candidate_builds=0;candidate_binary=0;release='configure root exit0, gate failed descendants-nonquiescent, no candidate build/test';debug='not_run';asan='not_run';png_audits=0;native_command_attempts=3;native_failures=1;gate_failures=1;tool_repairs=0;job_active_final=0;family_count=1;candidate_adopted=$false;full_replay=0;runtime_cost_stress=0;live=0;model=0;goals=0;automations=0;commit_push=0;new_dispatch=0;oracle_unchanged=$true;candidate_actual_dependency_closure='not_available_no_compile';dependency_manifest_sha256=(Get-FileHash -LiteralPath "$batchPath/dependency-manifest.json").Hash.ToLowerInvariant();artifact_ledger_sha256='';receipt_bytes=0;capacity=@{};self_hash_excluded=$true}
$ledgerHash=[Security.Cryptography.SHA256]::HashData($utf8.GetBytes($ledgerText));$r.artifact_ledger_sha256=([BitConverter]::ToString($ledgerHash)).Replace('-','').ToLowerInvariant()
$receiptBytes=0
for($i=0;$i -lt 12;$i++){
 $root=$rootBase+$ledgerBytes+$receiptBytes;$campaign=$campaignBase+$ledgerBytes+$receiptBytes;$aggregate=$campaign+$carry+$externalBytes
 $r.receipt_bytes=$receiptBytes;$r.capacity=@{root_bytes=$root;external_bytes=$externalBytes;development_charged=$root+$externalBytes;development_cap=58720256;development_remaining=58720256-$root-$externalBytes;controller_reserve=8388608;batch_total_cap=67108864;out_bytes=$outBytes;out_cap=268435456;campaign_entry_bytes=$campaign;prior=45307809;carried_O_external=72115;carried_O_controller=9546;carried_4R_external=54056;carried_4R_controller=11851;aggregate_charged=$aggregate;aggregate_cap=8589934592;aggregate_remaining=8589934592-$aggregate;disk_free=(Get-PSDrive C).Free;startup_minimum_free=5704253440;png_copies=0;old_data_deleted=0;method='conservative directory-entry logical lengths, not allocated bytes'}
 $receiptText=Serialize $r;$newBytes=$utf8.GetByteCount($receiptText);if($newBytes -eq $receiptBytes){break};$receiptBytes=$newBytes
}
if($newBytes -ne $receiptBytes -or $r.capacity.development_charged -gt 58720256 -or $aggregate -gt 8589934592 -or $outBytes -gt 268435456){throw 'settlement bound/stability'}
[IO.File]::WriteAllText($ledgerPath,$ledgerText,$utf8);[IO.File]::WriteAllText($receiptPath,$receiptText,$utf8)
$actualRoot=Bytes $batchPath;$actualCampaign=Bytes $campaignPath
if($actualRoot -ne $r.capacity.root_bytes -or $actualCampaign+$carry+$externalBytes -ne $r.capacity.aggregate_charged){throw 'settlement physical sum mismatch'}
Write-Output (@{status='platform_partial';protected=$after.protected_count;mismatches=$after.mismatches.Count;batch_charged=$r.capacity.development_charged;out=$outBytes;aggregate=$r.capacity.aggregate_charged;remaining=$r.capacity.aggregate_remaining;receipt_bytes=$receiptBytes;controller_reserve=8388608}|ConvertTo-Json -Compress)
