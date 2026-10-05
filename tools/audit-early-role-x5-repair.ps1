$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'early-role-x5-repair'
$oldPath=Join-Path $campaignPath 'early-role-x5'
$priorPath=Join-Path $repoPath 'measurements/research-next-20261001'
$encoding=[Text.UTF8Encoding]::new($false)
function Bytes($path){[long](Get-ChildItem -LiteralPath $path -File -Recurse|Measure-Object Length -Sum).Sum}
function Hash($path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Save($name,$value){
    $payload=$encoding.GetBytes(($value|ConvertTo-Json -Depth 60)+"`n")
    if($payload.Length -gt 2097152 -or (Bytes $batchPath)+$payload.Length -gt 8388608 -or (Bytes $campaignPath)+(Bytes $priorPath)+$payload.Length -gt 8589934592){throw 'audit quota'}
    $stream=[IO.File]::Open((Join-Path $batchPath $name),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
    try{$stream.Write($payload,0,$payload.Length)}finally{$stream.Dispose()}
}
$mutex=[Threading.Mutex]::new($false,'Local\PAS_X1ContactReplayBudget');$owned=$false
try{
    $owned=$mutex.WaitOne(0);if(!$owned){throw 'batch_writer_busy'}
    $r1=[IO.File]::ReadAllText("$batchPath/Scope-release-1.json")
    $r2=[IO.File]::ReadAllText("$batchPath/Scope-release-2.json")
    $ra=[IO.File]::ReadAllText("$batchPath/Scope-asan-1.json")
    $scope=$r1|ConvertFrom-Json;$scopeAsan=$ra|ConvertFrom-Json
    if($r1 -cne $r2 -or $r1 -cne $ra.Replace($scopeAsan.binary_sha256,$scope.binary_sha256)){throw 'scope determinism mismatch'}
    $testResults=@(foreach($configuration in @('release','asan')){
        [xml]$xml=Get-Content -LiteralPath "$batchPath/Tests-$configuration-1.xml" -Raw
        $s=$xml.testsuites
        $skipped=@($s.testsuite.testcase|Where-Object {$_.status -eq 'notrun' -or $_.result -eq 'skipped'}).Count
        if([int]$s.tests -ne 34 -or [int]$s.failures -ne 0 -or [int]$s.disabled -ne 0 -or [int]$s.errors -ne 0 -or $skipped -ne 0){throw 'test denominator'}
        [ordered]@{config=$configuration;tests=34;failed=0;disabled=0;skipped=$skipped;xml_sha256=(Hash "$batchPath/Tests-$configuration-1.xml")}
    })
    $commands=@(Get-ChildItem -LiteralPath $batchPath -Filter '*-command.json' -File|ForEach-Object {
        $c=Get-Content -LiteralPath $_.FullName -Raw|ConvertFrom-Json
        if(($_.Name.StartsWith('Negative-') -and $c.exit -ne 1) -or (!$_.Name.StartsWith('Negative-') -and $c.exit -ne 0)){throw 'unexpected command exit'}
        [ordered]@{path=$_.FullName;sha256=(Hash $_.FullName);exit=$c.exit}
    })
    if(@($commands|Where-Object {$_.path -match 'Negative-'}).Count -ne 4){throw 'negative denominator'}
    foreach($config in @('release','asan')){
        $v=Get-Content -LiteralPath "$batchPath/Verify-$config-1.log" -Raw
        if(!$v.Contains('337 role frames') -or !$v.Contains('excluded only analysis_binary_sha256')){throw 'full reader verification missing'}
    }
    $protected=Get-Content -LiteralPath "$batchPath/protected-before.json" -Raw|ConvertFrom-Json
    $inherited=(Get-Content -LiteralPath "$oldPath/workspace-before.json" -Raw|ConvertFrom-Json).protected
    $failures=@();$inheritedCount=0
    foreach($p in $protected){if((Hash $p.path) -ne $p.sha256){$failures+=$p.path}}
    foreach($p in $inherited){if($p.path.Replace('\','/').EndsWith('/docs/PROJECT_STATUS_NEXT_STEPS_20261001.md')){continue};++$inheritedCount;if((Hash $p.path) -ne $p.sha256){$failures+=$p.path}}
    function Text-Normalized($path){[IO.File]::ReadAllText($path).Replace("`r`n","`n")}
    $banner=[regex]::new('(?m)^> \*\*2026-10-02.*\n')
    $statusBefore=$banner.Replace((Text-Normalized "$batchPath/before/docs__PROJECT_STATUS_NEXT_STEPS_20261001.md"),'',1)
    $statusAfter=$banner.Replace((Text-Normalized "$repoPath/docs/PROJECT_STATUS_NEXT_STEPS_20261001.md"),'',1)
    $handoffBefore=Text-Normalized "$batchPath/before/docs__EARLY_ROLE_ABSTAIN_X5_HANDOFF_20261002.md"
    if(!$statusAfter.StartsWith($statusBefore) -or !(Text-Normalized "$repoPath/docs/EARLY_ROLE_ABSTAIN_X5_HANDOFF_20261002.md").StartsWith($handoffBefore)){throw 'history text altered'}
    $formalDiff=@(git -C $repoPath diff --name-only -- src include CMakeLists.txt)
    Save 'preservation-after.json' ([ordered]@{current_protected_checked=$protected.Count;inherited_checked=$inheritedCount;mismatches=$failures;formal_diff=$formalDiff;status_history_preserved=$true;handoff_history_preserved=$true;old_x5_bytes=(Bytes $oldPath)})
    if($failures.Count -ne 0 -or $formalDiff.Count -ne 0){throw 'preservation failed'}
    $parent=Get-Content -LiteralPath "$oldPath/acceptance-report-1.json" -Raw|ConvertFrom-Json
    $viewed=@(foreach($ordinal in @(6173,6174,6175,6180,6188,6189,6190,6191)){
        $f=@($parent.frames|Where-Object {$_.role -eq 'c36h_reference' -and $_.ordinal -eq $ordinal})
        if($f.Count -ne 1 -or (Hash $f[0].png_path) -ne $f[0].png_sha256){throw 'visual PNG binding'}
        [ordered]@{ordinal=$ordinal;source_frame=$f[0].source_frame;png=$f[0].png_path;sha256=$f[0].png_sha256;raw_objects=@($f[0].objects|Select-Object note_id,kind,center,width,height,tangent,identity_grade,physical_role_gold)}
    })
    Save 'visual-audit.json' ([ordered]@{
        parent_report_sha256=(Hash "$oldPath/acceptance-report-1.json");human_gold_added=0;inspection='AI visual review of original PNG; object identity/role remains proposed';frames=$viewed
        findings=@(
            @{case='6173-6175';grade='Strong inference';finding='Yellow Drag and red Flick overlap; measured major-axis span shrinks then recovers with centroid y shift. The 1.04931rad turn proposal may be an observation-shape artifact, not a physical turn.'},
            @{case='6188-6191';grade='Strong inference';finding='Cyan Tap, red Flick and a white line change pose near 6190. A real pose-change control, not verified decoration-to-judgment-line late-turn role gold.'},
            @{case='6180';grade='Proposed';finding='Three separated vertical yellow cores with multiple line appearances; neighboring object roles remain unknown. Do not transfer the main D oracle role to these objects.'}
        );controls=@('E5287 remains user-confirmed normal','6210 missing bank line is not pixel absence','6194 line-driven closing is not role proof');new_production_claims=0
    })
    $sourcePaths=@('apps/frame_review/x5_adapter.hpp','apps/frame_review/x5_scope.hpp','apps/frame_review/x5_scope_report.cpp','apps/frame_review/x5_rule.hpp','apps/frame_review/x5_report.cpp','apps/frame_review/x5_report.hpp','apps/frame_review/x5_main.cpp','apps/frame_review/x5_scenarios.hpp','apps/frame_review/x5_offline/CMakeLists.txt','apps/frame_review/review_io.hpp','apps/frame_review/x3_features.hpp','tests/x5_rule_tests.cpp','tools/repair-early-role-x5.ps1','tools/audit-early-role-x5-repair.ps1','docs/X5_SCOPE_TIME_REPAIR_20261002.md','docs/PROJECT_STATUS_NEXT_STEPS_20261001.md','docs/EARLY_ROLE_ABSTAIN_X5_HANDOFF_20261002.md')
    $snapshotPath=Join-Path $batchPath 'source';if(Test-Path -LiteralPath $snapshotPath){throw 'source snapshot exists'}
    $snapshotBytes=($sourcePaths|ForEach-Object{(Get-Item -LiteralPath "$repoPath/$_").Length}|Measure-Object -Sum).Sum
    if((Bytes $batchPath)+$snapshotBytes+1048576 -gt 8388608 -or (Bytes $campaignPath)+(Bytes $priorPath)+$snapshotBytes+1048576 -gt 8589934592){throw 'snapshot reserve'}
    New-Item -ItemType Directory -Path $snapshotPath|Out-Null
    $sources=@(foreach($relative in $sourcePaths){$source="$repoPath/$relative";$snapshot=Join-Path $snapshotPath $relative.Replace('/','__');Copy-Item -LiteralPath $source -Destination $snapshot
        if((Hash $source) -ne (Hash $snapshot)){throw 'snapshot mismatch'}
        [ordered]@{path=$relative;sha256=(Hash $source);snapshot=$snapshot;bytes=(Get-Item -LiteralPath $source).Length}
    })
    $binaries=@(foreach($config in @('release','asan')){$sub=if($config -eq 'asan'){'Debug'}else{'Release'};Get-ChildItem -LiteralPath "$repoPath/out/x5-repair/$config/$sub" -File|Where-Object {$_.Extension -in @('.exe','.dll')}|ForEach-Object {[ordered]@{path=$_.FullName;sha256=(Hash $_.FullName);bytes=$_.Length}}})
    Save 'source-freeze.json' ([ordered]@{sources=$sources;binaries=$binaries;original_rule_unchanged=$true;formal_diff=$formalDiff;new_build_root='out/x5-repair'})
    $summary=[ordered]@{
        schema=1;date='2026-10-02';head=(git -C $repoPath rev-parse HEAD);branch=(git -C $repoPath branch --show-current);worktrees=@(git -C $repoPath worktree list)
        author_and_validator='controller direct repair; not separate-agent acceptance';baseline='C36h tint1';production_versions=@(50,27,11);new_live=0;new_full_contact_replays=0;NDA_v1_adoption='still_rejected'
        tests=$testResults;commands=$commands;original_full_report_recomputed=@('release','asan');original_report_sha256=(Hash "$oldPath/acceptance-report-1.json")
        scope_report_sha256=(Hash "$batchPath/Scope-release-1.json");scope_report_bytes=(Get-Item -LiteralPath "$batchPath/Scope-release-1.json").Length;scope_release_byte_identical=$true;scope_asan_equivalent_except_binary_sha=$true
        scope_denominators=$scope.counts;scope_role_frames=$scope.frames.Count;scope_targets=$scope.target_occurrences;packet_frames=$scope.real_case_packets.Count;packet_unique_png=$scope.unique_packet_png_verified;turn_proposals=$scope.turn_search.proposal_count;human_gold_added=0
        visual_audit_sha256=(Hash "$batchPath/visual-audit.json");preservation_sha256=(Hash "$batchPath/preservation-after.json");source_freeze_sha256=(Hash "$batchPath/source-freeze.json")
        asan_full_readers='512MiB process commit; quarantine_size_mb=16:thread_local_quarantine_size_kb=64; default-quarantine full-reader success not claimed'
        limitations=@('research scope does not authorize selection/Down','no new real late-turn physical role gold','old gameplay suites and 26-case CLI matrix not rerun','stale regression repairs diagnostics; pure rule unchanged','no runtime candidate or Miss improvement claimed')
        batch_limit_bytes=8388608;batch_bytes=0;batch_remaining_bytes=0;campaign_limit_bytes=8589934592;campaign_plus_prior_bytes=0;campaign_remaining_bytes=0;new_build_bytes=(Bytes "$repoPath/out/x5-repair");old_build_bytes=(Bytes "$repoPath/out/x5")
    }
    $batchBeforeSummary=Bytes $batchPath;$campaignBeforeSummary=(Bytes $campaignPath)+(Bytes $priorPath)
    for($i=0;$i -lt 12;$i++){
        $size=$encoding.GetByteCount(($summary|ConvertTo-Json -Depth 60)+"`n")
        if($summary.batch_bytes -eq $batchBeforeSummary+$size){break}
        $summary.batch_bytes=$batchBeforeSummary+$size;$summary.batch_remaining_bytes=8388608-$summary.batch_bytes
        $summary.campaign_plus_prior_bytes=$campaignBeforeSummary+$size;$summary.campaign_remaining_bytes=8589934592-$summary.campaign_plus_prior_bytes
    }
    Save 'repair-summary.json' $summary
    if((Bytes $batchPath) -ne $summary.batch_bytes -or (Bytes $campaignPath)+(Bytes $priorPath) -ne $summary.campaign_plus_prior_bytes){throw 'final ledger mismatch'}
    [ordered]@{summary_sha256=(Hash "$batchPath/repair-summary.json");batch_bytes=$summary.batch_bytes;batch_remaining=$summary.batch_remaining_bytes;campaign_plus_prior=$summary.campaign_plus_prior_bytes;campaign_remaining=$summary.campaign_remaining_bytes;protected=$protected.Count;inherited=$inheritedCount;new_build_bytes=$summary.new_build_bytes}|ConvertTo-Json
}finally{if($owned){$mutex.ReleaseMutex()};$mutex.Dispose()}
