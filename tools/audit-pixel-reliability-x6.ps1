$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'pixel-reliability-x6'
$priorPath=Join-Path $repoPath 'measurements/research-next-20261001'
function Bytes($path){[long](Get-ChildItem -LiteralPath $path -File -Recurse|Measure-Object Length -Sum).Sum}
function Hash($path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Save($name,$value){
    $data=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Depth 60)+"`n")
    if((Bytes $batchPath)+$data.Length -gt 16777216 -or (Bytes $campaignPath)+(Bytes $priorPath)+$data.Length -gt 8589934592){throw 'audit quota'}
    $file=[IO.File]::Open("$batchPath/$name",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
    try{$file.Write($data,0,$data.Length)}finally{$file.Dispose()}
}
$mutex=[Threading.Mutex]::new($false,'Local\PAS_X1ContactReplayBudget');$owned=$false
try{
    $owned=$mutex.WaitOne(0);if(!$owned){throw 'batch_writer_busy'}
    $r1=[IO.File]::ReadAllText("$batchPath/Report-release-3.json");$r2=[IO.File]::ReadAllText("$batchPath/Report-release-4.json");$ra=[IO.File]::ReadAllText("$batchPath/Report-asan-2.json")
    $j=$r1|ConvertFrom-Json;$ja=$ra|ConvertFrom-Json
    if($r1 -cne $r2 -or $r1 -cne $ra.Replace($ja.binary_sha256,$j.binary_sha256)){throw 'determinism mismatch'}
    $checks=@(foreach($config in @('release','asan')){
        [xml]$xml=Get-Content "$batchPath/Tests-$config-2.xml" -Raw;$s=$xml.testsuites
        $skips=@($s.testsuite.testcase|Where-Object {$_.status -eq 'notrun' -or $_.result -eq 'skipped'}).Count
        if([int]$s.tests -ne 13 -or [int]$s.failures -ne 0 -or [int]$s.errors -ne 0 -or [int]$s.disabled -ne 0 -or $skips){throw 'tests incomplete'}
        @{config=$config;tests=13;failures=0;skipped=0;xml_sha256=(Hash "$batchPath/Tests-$config-2.xml")}
    })
    $negative=@(foreach($spec in @('release-2','asan-1')){foreach($kind in @('argc','parent')){
        $c=Get-Content "$batchPath/Negative-$spec-$kind-command.json" -Raw|ConvertFrom-Json
        if($c.exit -ne 1){throw 'negative exit'}
        @{case="$spec-$kind";exit=1;sha256=$c.output_sha256}
    }})
    $protected=Get-Content "$batchPath/protected-source-before.json" -Raw|ConvertFrom-Json
    foreach($p in $protected){if((Hash "$repoPath/$($p.path)") -ne $p.sha256){throw "protected source changed: $($p.path)"}}
    $prov=Get-Content "$campaignPath/contact-replay-x1/source-v2/main50-source-provenance.json" -Raw|ConvertFrom-Json
    $exportChecks=0
    foreach($p in $prov.instrumented_source_sha256.PSObject.Properties){
        if((Hash "$repoPath/out/x1/main50-v2/$($p.Name)") -ne $p.Value){throw "export changed: $($p.Name)"};++$exportChecks
    }
    $formal=@(git -C $repoPath diff --name-only -- src include CMakeLists.txt);if($formal.Count){throw 'formal source changed'}
    if((Hash "$campaignPath/early-role-x5/acceptance-report-1.json") -ne $j.parent_report_sha256){throw 'parent changed'}
    if((Hash "$campaignPath/early-role-x5-repair/repair-summary.json") -ne '72575d28aed06c377611d251e64a3eb31ea7afbef5c28bd5db609647d1b7e0b9'){throw 'parent repair changed'}
    $sourceList=@('apps/frame_review/x6_pixel.hpp','apps/frame_review/x6_report.cpp','apps/frame_review/x6_offline/CMakeLists.txt','tests/x6_pixel_tests.cpp','tools/pixel-reliability-x6.ps1','tools/audit-pixel-reliability-x6.ps1','apps/frame_review/offline/CMakeLists.txt','apps/frame_review/replay_trace.hpp','apps/frame_review/review_io.hpp','docs/PIXEL_CENTER_RELIABILITY_X6_20261003.md','docs/C36H_FORWARD_EXECUTION_PLAN_20261003.md')
    if(Test-Path "$batchPath/source"){throw 'source snapshot exists'}
    if((Bytes $batchPath)+1048576 -gt 16777216){throw 'snapshot reserve'}
    New-Item -ItemType Directory "$batchPath/source"|Out-Null
    $sources=@(foreach($relative in $sourceList){$snapshot="$batchPath/source/$($relative.Replace('/','__'))";Copy-Item -LiteralPath "$repoPath/$relative" -Destination $snapshot
        @{path=$relative;sha256=(Hash $snapshot);bytes=(Get-Item $snapshot).Length;snapshot=$snapshot}
    })
    $binaries=@(foreach($config in @('release','asan')){$sub=if($config -eq 'asan'){'Debug'}else{'Release'};Get-ChildItem "$repoPath/out/x6/$config/$sub" -File|Where-Object {$_.Extension -in @('.exe','.dll')}|ForEach-Object {@{path=$_.FullName;sha256=(Hash $_.FullName);bytes=$_.Length}}})
    Save 'source-freeze.json' @{sources=$sources;binaries=$binaries;reuse_provenance_sha256=(Hash "$campaignPath/contact-replay-x1/source-v2/main50-source-provenance.json");protected_sources_checked=$protected.Count;export_sources_checked=$exportChecks;formal_diff=$formal}
    $summary=[ordered]@{schema=1;head=(git -C $repoPath rev-parse HEAD);author_and_validator='controller direct implementation and validation; no separate-agent acceptance';decision='retain offline measurable features; reject foreign-gap-v1 as runtime reliability gate; next X9 Hold diagnosis';counts=$j.counts;role_frames=$j.frames.Count;targets=$j.target_occurrences;unique_png=$j.unique_png_verified;human_gold_added=0;production_changed=$false;new_live=0;new_full_contact_replays=0;tests=$checks;negative_cli=$negative;release_report_sha256=(Hash "$batchPath/Report-release-3.json");release_byte_identical=$true;asan_equal_except_binary_sha=$true;source_freeze_sha256=(Hash "$batchPath/source-freeze.json");batch_bytes=0;batch_remaining_bytes=0;batch_limit_bytes=25165824;development_limit_bytes=16777216;campaign_plus_prior_bytes=0;campaign_remaining_bytes=0;campaign_limit_bytes=8589934592;build_bytes=(Bytes "$repoPath/out/x6");build_limit_bytes=1073741824;limitations=@('same color two-object ambiguity','6174 foreign-gap-v1 false negative versus proposed occlusion','Hold excluded','no pixel recall for undetected objects','no physical role/identity gold','ASan reduced quarantine','older reports retain pre-fix signed offset differences')}
    $batchBefore=Bytes $batchPath;$campaignBefore=(Bytes $campaignPath)+(Bytes $priorPath)
    for($iteration=0;$iteration -lt 8;++$iteration){$n=[Text.Encoding]::UTF8.GetByteCount(($summary|ConvertTo-Json -Depth 60)+"`n");$summary.batch_bytes=$batchBefore+$n;$summary.batch_remaining_bytes=25165824-$summary.batch_bytes;$summary.campaign_plus_prior_bytes=$campaignBefore+$n;$summary.campaign_remaining_bytes=8589934592-$summary.campaign_plus_prior_bytes}
    if($summary.build_bytes -gt 1073741824){throw 'build quota'}
    Save 'final-summary.json' $summary
    if((Bytes $batchPath) -ne $summary.batch_bytes){throw 'ledger mismatch'}
    Write-Output "X6 complete: 13/13 each; reports equal; batch $($summary.batch_bytes); runtime adoption rejected"
}finally{if($owned){$mutex.ReleaseMutex()};$mutex.Dispose()}
