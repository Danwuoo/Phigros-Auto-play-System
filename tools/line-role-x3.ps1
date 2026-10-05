param([ValidateSet('Init','Configure','Build','Tests','Reports','Regressions','Negatives','Evidence','Freeze','Finalize')][string]$Mode='Init',[string]$Attempt='')
$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$priorPath=Join-Path $repoPath 'measurements/research-next-20261001'
$batchPath=Join-Path $campaignPath 'line-role-x3'
$x2Path=Join-Path $campaignPath 'confirmed-preserve-x2'
function Bytes([string]$p){if(!(Test-Path -LiteralPath $p)){return [long]0};return [long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Write-Bounded([string]$name,[string]$value){
    $targetPath=Join-Path $batchPath $name
    if(Test-Path -LiteralPath $targetPath){throw "output exists: $targetPath"}
    $payload=[Text.UTF8Encoding]::new($false).GetBytes($value)
    if((Bytes $batchPath)+$payload.Length -gt 33554432 -or (Bytes $campaignPath)+(Bytes $priorPath)+$payload.Length -gt 8589934592){throw 'X3 quota'}
    [IO.File]::WriteAllBytes($targetPath,$payload)
}
function Save-Bounded([string]$name,$value){Write-Bounded $name (($value|ConvertTo-Json -Depth 40)+"`n")}
function Invoke-Logged([string]$name,[string]$exe,[string[]]$arguments){
    $logPath=Join-Path $batchPath "$name.log"
    if(Test-Path -LiteralPath $logPath){throw 'log exists'}
    $batchBytes=Bytes $batchPath;$campaignBytes=(Bytes $campaignPath)+(Bytes $priorPath);$used=[long]0
    & $exe @arguments 2>&1 | ForEach-Object {
        $data=[Text.UTF8Encoding]::new($false).GetBytes("$_`n")
        if($used+$data.Length -gt 1048576 -or $batchBytes+$used+$data.Length -gt 33554432 -or $campaignBytes+$used+$data.Length -gt 8589934592){throw 'log append quota'}
        $stream=[IO.File]::Open($logPath,[IO.FileMode]::Append,[IO.FileAccess]::Write);try{$stream.Write($data,0,$data.Length)}finally{$stream.Dispose()};$used+=$data.Length
    }
    $code=$LASTEXITCODE
    if(!(Test-Path -LiteralPath $logPath)){Write-Bounded "$name.log" ''}
    Save-Bounded "$name-command.json" @{exe=$exe;arguments=$arguments;exit_code=$code;log_sha256=(Hash $logPath)}
    if($code -ne 0){throw "$name exit=$code"}
}
$lease=[Threading.Mutex]::new($false,'Local\PAS_X1ContactReplayBudget');$owned=$false
try{
    $owned=$lease.WaitOne(0);if(!$owned){throw 'batch_writer_busy'}
    if($Mode -eq 'Init'){
        if(Test-Path -LiteralPath $batchPath){throw 'X3 batch exists'}
        New-Item -ItemType Directory -Path $batchPath|Out-Null
        $formal=@(Get-ChildItem "$repoPath/src","$repoPath/include" -Recurse -File)+@(Get-Item "$repoPath/CMakeLists.txt")
        $priorSources=@(git -C $repoPath ls-files --modified;git -C $repoPath ls-files --others --exclude-standard)|Where-Object{$_ -notmatch 'x3|line-role'}
        $protected=@($formal.FullName)+@($priorSources|ForEach-Object{Join-Path $repoPath $_})+@(Get-ChildItem -LiteralPath $x2Path -Recurse -File|Select-Object -ExpandProperty FullName)
        Save-Bounded 'workspace-before.json' @{date='2026-10-02';timezone='Asia/Taipei';head=(git -C $repoPath rev-parse HEAD);branch=(git -C $repoPath branch --show-current);worktrees=@(git -C $repoPath worktree list);status=@(git -C $repoPath status --short);formal_sha=@($formal|ForEach-Object{@{path=$_.FullName;sha256=(Hash $_.FullName)}});protected=@($protected|Where-Object{$_ -ne (Join-Path $repoPath 'docs/PROJECT_STATUS_NEXT_STEPS_20261001.md')}|ForEach-Object{@{path=$_;sha256=(Hash $_)}});campaign_before=(Bytes $campaignPath);prior_bytes=(Bytes $priorPath);x1_bytes=(Bytes (Join-Path $campaignPath 'contact-replay-x1'));x2_bytes=(Bytes $x2Path)}
        Write-Bounded 'status-before.md' ([IO.File]::ReadAllText("$repoPath/docs/PROJECT_STATUS_NEXT_STEPS_20261001.md"))
        Save-Bounded 'hypotheses-before-report.json' @{
            question='Can current geometry plus <=90ms measured pairs distinguish initial role selection, confirmed competition, wrong preserved relation, geometric continuation and overlap-only evidence?'
            shadow_rule='none; role truth insufficient. Measurements and existing guard reconstruction only.'
            hypotheses=@(
                @{id='H1';claim='D later measured note displacement is mostly tangent to horizontal and normal to vertical; nearest-line initial scoring may be wrong.';falsify='Vertical does not show closing measured distance, or correct synthetic late-alignment is rejected';controls='correct/wrong initial relations, late alignment, neighbor, line motion invalid'},
                @{id='H2';claim='Shrinking distance protects a correct confirmed line but cannot uniquely establish its role.';falsify='wrong horizontal association can also shrink distance due moving line';controls='original crossing test, moving horizontal crossing D6200'},
                @{id='H3';claim='Old-visible and old-missing orthogonal relation guards are distinct.';falsify='same prior/current geometry reaches same first guard';controls='fragment same geometry, old-visible competition, missing orthogonal, rotating Hold'},
                @{id='H4';claim='Current core overlap and measured approach do not establish judgment role or authorize retry.';falsify='independent physical role/game adoption evidence proves a unique role';controls='current overlap, absence, unknown Down, completed return; A no-root body, B absence, C Flick, E normal'}
            )
            estimates=@{report_each_max_bytes=6291456;two_reports_max_bytes=12582912;logs_metadata_tests_reserved_bytes=4194304;new_replays=0;batch_limit_bytes=33554432}
        }
        $roles=@('c36h_reference','main50_control','main50_no_confirmed_winner_override');$runs=@('c36h-reference-on-1','main50-control-on-1','main50-no-override-on-1')
        $inputs=@();for($i=0;$i -lt 3;$i++){$runPath=Join-Path $x2Path $runs[$i];$inputs+=@{role=$roles[$i];root=$runPath;files=@('summary.json','trace.jsonl','first-intervention.jsonl','events.jsonl')|ForEach-Object{@{name=$_;sha256=(Hash (Join-Path $runPath $_))}}}}
        Save-Bounded 'input-manifest.json' @{schema=1;experiment='line_role_causal_features_x3';batch_root=$batchPath;campaign_root=$campaignPath;prior_research_root=$priorPath;batch_limit_bytes=33554432;x2_manifest_path=(Join-Path $x2Path 'input-manifest.json');x2_manifest_sha256=(Hash (Join-Path $x2Path 'input-manifest.json'));acceptance_path=(Join-Path $x2Path 'acceptance-20261002/acceptance-summary.json');acceptance_sha256=(Hash (Join-Path $x2Path 'acceptance-20261002/acceptance-summary.json'));runs=$inputs}
    }elseif($Mode -eq 'Configure'){
        Invoke-Logged 'configure' 'cmake' @('-S',"$repoPath/apps/frame_review/offline",'-B',"$repoPath/out/x3/build",'-G','Visual Studio 18 2026','-A','x64','-T','v145','-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275',"-DX1_SOURCE=$repoPath/out/x1/main50-v2","-DX1_REPO=$repoPath","-DCMAKE_PREFIX_PATH=$repoPath/out/vcpkg_installed/x64-windows",'-DX3_REPORT=ON')
    }elseif($Mode -eq 'Build'){
        Invoke-Logged "build$Attempt" 'cmake' @('--build',"$repoPath/out/x3/build",'--config','Release','--target','x3_report','x3_tests','--parallel','2')
        Copy-Item -LiteralPath "$repoPath/out/vcpkg_installed/x64-windows/bin/z.dll","$repoPath/out/vcpkg_installed/x64-windows/bin/gtest.dll","$repoPath/out/vcpkg_installed/x64-windows/bin/gtest_main.dll" -Destination "$repoPath/out/x3/build/Release"
    }elseif($Mode -eq 'Tests'){
        $lease.ReleaseMutex();$owned=$false
        Invoke-Logged "tests$Attempt" "$repoPath/out/x3/build/Release/x3_tests.exe" @('--gtest_brief=1')
    }elseif($Mode -eq 'Reports'){
        # The executable acquires the same budget lease. Release this outer lease first.
        $lease.ReleaseMutex();$owned=$false
        Invoke-Logged "report1$Attempt" "$repoPath/out/x3/build/Release/x3_report.exe" @("$batchPath/input-manifest.json","$batchPath/causal-features-1$Attempt.json")
        Invoke-Logged "report2$Attempt" "$repoPath/out/x3/build/Release/x3_report.exe" @("$batchPath/input-manifest.json","$batchPath/causal-features-2$Attempt.json")
        Save-Bounded "determinism$Attempt.json" @{byte_equal=((Hash "$batchPath/causal-features-1$Attempt.json") -eq (Hash "$batchPath/causal-features-2$Attempt.json"));sha256=(Hash "$batchPath/causal-features-1$Attempt.json")}
    }elseif($Mode -eq 'Regressions'){
        $lease.ReleaseMutex();$owned=$false
        $env:PAS_RGB_CLIP_ROOT="$campaignPath/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
        if($env:PAS_X2_NO_WINNER_OVERRIDE){throw 'control environment must keep winner override'}
        Invoke-Logged 'regression36' "$repoPath/out/x2/tools36/x1_tests.exe" @('--gtest_brief=1')
        Invoke-Logged 'regression50' "$repoPath/out/x2/tools50/x1_tests.exe" @('--gtest_brief=1')
    }elseif($Mode -eq 'Negatives'){
        $lease.ReleaseMutex();$owned=$false
        $result=@();$exePath="$repoPath/out/x3/build/Release/x3_report.exe"
        foreach($caseName in @('missing-role','swapped-role','wrong-trace-sha','missing-file-binding','wrong-parent','budget','float-schema','existing-output','missing-manifest')){
            $manifestValue=Get-Content -Raw -LiteralPath "$batchPath/input-manifest.json"|ConvertFrom-Json
            $outputPath="$batchPath/negative-$caseName-report.json"
            switch($caseName){
                'missing-role'{$manifestValue.runs[1].role='unknown'}
                'swapped-role'{$manifestValue.runs[1].root=$manifestValue.runs[2].root}
                'wrong-trace-sha'{$manifestValue.runs[1].files[1].sha256=('0'*64)}
                'missing-file-binding'{$manifestValue.runs[1].files=@($manifestValue.runs[1].files|Select-Object -First 3)}
                'wrong-parent'{$manifestValue.x2_manifest_sha256=('0'*64)}
                'budget'{$manifestValue.batch_limit_bytes=67108864}
                'float-schema'{$manifestValue.schema=1.5}
                'existing-output'{$outputPath="$batchPath/causal-features-1verified.json"}
            }
            if($caseName -ne 'missing-manifest'){Save-Bounded "negative-$caseName-input.json" $manifestValue}
            $inputPath="$batchPath/negative-$caseName-input.json"
            $message=(& $exePath $inputPath $outputPath 2>&1|Out-String);$code=$LASTEXITCODE
            Write-Bounded "negative-$caseName.log" $message
            $result+=@{name=$caseName;exit_code=$code;reason=$message.Trim();output_exists=(Test-Path -LiteralPath $outputPath);existing_output=($caseName -eq 'existing-output');arguments=@($inputPath,$outputPath)}
            if($code -eq 0 -or ($caseName -ne 'existing-output' -and (Test-Path -LiteralPath $outputPath))){throw 'negative unexpectedly accepted'}
        }
        Save-Bounded 'negative-results.json' @{expected_rejections=9;unexpected_success=0;cases=$result}
    }elseif($Mode -eq 'Evidence'){
        $x2Before=Get-Content -Raw -LiteralPath "$x2Path/workspace-before.json"|ConvertFrom-Json
        $x2Freeze=Get-Content -Raw -LiteralPath "$x2Path/tool-freeze.json"|ConvertFrom-Json
        $checks=@($x2Before.protected)+@($x2Freeze.binaries)+@($x2Freeze.exports)
        $mismatches=@($checks|Where-Object{(Hash $_.path) -ne $_.sha256})
        $snapshotMismatches=@($x2Freeze.sources|Where-Object{(Hash $_.snapshot) -ne $_.sha256})
        Save-Bounded 'existing-frozen-preservation.json' @{inherited_x1_protected_checks=$x2Before.protected.Count;x2_binary_checks=$x2Freeze.binaries.Count;x2_export_checks=$x2Freeze.exports.Count;x2_snapshot_checks=$x2Freeze.sources.Count;mismatches=$mismatches;snapshot_mismatches=$snapshotMismatches;counts_are_file_checks_not_independent_scenes=$true}
        if($mismatches.Count -or $snapshotMismatches.Count){throw 'inherited frozen preservation mismatch'}
        $r=Get-Content -Raw -LiteralPath "$batchPath/causal-features-1verified.json"|ConvertFrom-Json
        $ordinals=@(1533,1534,1535,1536,1537,3498,4986,5287,5520,6160,6161,6174,6194,6199,6200,6205,6211,6212,6213,6214)
        Save-Bounded 'manual-rgb-review.json' @{reviewer='AI visual review of original PNGs; proposed, never human gold';reviewed=@($r.frames|Where-Object{$_.role -eq 'main50_control' -and $_.ordinal -in $ordinals}|Select-Object ordinal,source_frame,png_path,png_sha256);count=20;notes=@('D6160/6161: rightward yellow core, nearby moving horizontal ridge and distant vertical ridge; proposed same object, no role gold.','D6174: yellow Drag overlaps another red object; runtime 1649->1650 continuity remains proposed.','D6194/6199/6200: horizontal ridge approaches/crosses core while core moves right; geometric overlap is visible, judgment adoption unknown.','D6211-6214: upper horizontal ridge remains visible as gray pixels after disappearance from current candidate bank; proposed physical continuation.','G1533-1537: horizontal note descends, vertical ridge appears in1535 near note184; line-role truth unknown.','A3498: left held body/rails visible; B4986: no current Hold body; C5520: Flick merge control; E5287: user normal control preserved.')}
        $det=Get-Content -Raw -LiteralPath "$batchPath/determinismverified.json"|ConvertFrom-Json
        Save-Bounded 'post-negative-report-preservation.json' @{expected_sha256=$det.sha256;actual_sha256=(Hash "$batchPath/causal-features-1verified.json");second_sha256=(Hash "$batchPath/causal-features-2verified.json");unchanged=((Hash "$batchPath/causal-features-1verified.json") -eq $det.sha256)}
        Save-Bounded 'attempt-notes.json' @{build_nonzero_attempts=@('build','buildrepair1','buildrepair2');build_success_attempts=@('buildrepair3','buildfinal','buildschema','buildrgb','buildverified');first_report_failure='report1 failed on absent C36h optional line_projection_only. Retained log; direct current observed ID replaces the optional-field requirement. No output was created.';initial_test_limitation='initial 13/13 had missing-manifest rejected by outer mutex. Assertion and wrapper fixed; testsverified explicitly excludes batch_writer_busy and requires the missing path.';presentation_query_failures=@('PowerShell report display had one brace parser error; corrected without modifying any artifact.','A read-only rg lookup used an absent maintenance-script name; no write was attempted.');new_replays=0;variant_regression_not_rerun='X2 frozen nonzero crossing result preserved, not relabeled or excluded'}
        $os=Get-CimInstance Win32_OperatingSystem;$cpu=Get-CimInstance Win32_Processor
        Save-Bounded 'host-environment.json' @{os=$os.Caption;os_version=$os.Version;cpu=@($cpu.Name);logical_processors=@($cpu.NumberOfLogicalProcessors);release='C++20/MSVC v145 x64 Release';generator='Visual Studio 18 2026 BuildTools 18.9.12105.275';test_scope='X3 13 tests + existing C36h 213 / main50 control 244 with 27 original RGB fixtures; no gameplay or report speed claim'}
    }elseif($Mode -eq 'Freeze'){
        $sourceNames=@('apps/frame_review/x3_features.hpp','apps/frame_review/x3_report.hpp','apps/frame_review/x3_main.cpp','apps/frame_review/x3_report.cpp','apps/frame_review/offline/CMakeLists.txt','tests/x3_features_tests.cpp','tools/line-role-x3.ps1')
        New-Item -ItemType Directory -Path "$batchPath/source$Attempt"|Out-Null
        $frozen=@();foreach($p in $sourceNames){$sourcePath=Join-Path $repoPath $p;$name=$p.Replace('/','__');Write-Bounded "source$Attempt/$name" ([IO.File]::ReadAllText($sourcePath));$frozen+=@{path=$sourcePath;sha256=(Hash $sourcePath);snapshot="source$Attempt/$name";snapshot_sha256=(Hash "$batchPath/source$Attempt/$name")}}
        Save-Bounded "tool-freeze$Attempt.json" @{sources=$frozen;binaries=@(Get-ChildItem -LiteralPath "$repoPath/out/x3/build/Release" -File|Where-Object Extension -In '.exe','.dll'|ForEach-Object{@{path=$_.FullName;bytes=$_.Length;sha256=(Hash $_.FullName)}});reused_export=@(Get-ChildItem -LiteralPath "$repoPath/out/x1/main50-v2" -File -Recurse|ForEach-Object{@{path=$_.FullName;sha256=(Hash $_.FullName)}});source='x3 report-only, frozen X1 core linkage for SHA/PNG utilities; no replay invoked'}
    }elseif($Mode -eq 'Finalize'){
        $before=Get-Content -Raw -LiteralPath "$batchPath/workspace-before.json"|ConvertFrom-Json
        $allowedPath=Join-Path $repoPath 'apps/frame_review/offline/CMakeLists.txt'
        $allowedStatusPath=Join-Path $repoPath 'docs/PROJECT_STATUS_NEXT_STEPS_20261001.md'
        $mismatches=@($before.protected|Where-Object{$_.path -ne $allowedPath -and $_.path -ne $allowedStatusPath -and (Hash $_.path) -ne $_.sha256})
        Save-Bounded "preservation$Attempt.json" @{checked=$before.protected.Count;intentional_changed_existing=@{path=$allowedPath;before_sha256=($before.protected|Where-Object path -EQ $allowedPath|Select-Object -ExpandProperty sha256);after_sha256=(Hash $allowedPath)};allowed_status_change='top / J8 only; separately validate historical text';mismatches=$mismatches;formal_git_diff=@(git -C $repoPath diff --name-only -- src include CMakeLists.txt);x1_bytes=(Bytes (Join-Path $campaignPath 'contact-replay-x1'));x2_bytes=(Bytes $x2Path)}
        $oldStatus=[IO.File]::ReadAllText("$batchPath/status-before.md");$newStatus=[IO.File]::ReadAllText("$repoPath/docs/PROJECT_STATUS_NEXT_STEPS_20261001.md")
        $marker='**C36h tint1';$historical=$oldStatus.Substring($oldStatus.IndexOf($marker));$preserved=$newStatus.Substring($newStatus.IndexOf($marker)).StartsWith($historical,[StringComparison]::Ordinal)
        Save-Bounded "status-history-preservation$Attempt.json" @{original_body_from_C36h_baseline_through_J7_preserved=$preserved;allowed='top update and J8 append only';original_sha256=(Hash "$batchPath/status-before.md")}
        if(!$preserved){throw 'status history changed'}
        if($mismatches.Count){throw 'preservation mismatch'}
        if($Attempt){Save-Bounded "finalization-notes$Attempt.json" @{previous_failure='First Finalize threw preservation mismatch after saving the ledger. Only authorized status file was listed, because Init excluded a forward-slash spelling instead of the resolved native path. Historical J1-J7 text already matched. Initial source, preservation, capacity-ledger and final-summary are retained as failed attempt evidence.';old_summary='final-summary.json';current_summary="final-summary$Attempt.json";repair='normalize status path exclusion; retain exact historical-body check; attempt-qualified CREATE_NEW snapshots/ledgers'} }
        $items=@(Get-ChildItem -LiteralPath $batchPath -File -Recurse|ForEach-Object{@{path=$_.FullName;bytes=$_.Length;sha256=(Hash $_.FullName)}})
        Save-Bounded "capacity-ledger$Attempt.json" @{batch_before_ledger=(Bytes $batchPath);batch_limit=33554432;campaign_plus_prior_before_ledger=((Bytes $campaignPath)+(Bytes $priorPath));campaign_limit=8589934592;out_x3_bytes=(Bytes "$repoPath/out/x3");exports='none; reused frozen out/x1/main50-v2';artifacts=$items;ledger_self_sha='omitted; length accounted in final summary'}
        $batchBeforeSelf=Bytes $batchPath;$totalBeforeSelf=(Bytes $campaignPath)+(Bytes $priorPath)
        $summaryValue=@{batch_bytes_before_self=$batchBeforeSelf;campaign_plus_prior_before_self=$totalBeforeSelf;ledger_bytes=(Get-Item -LiteralPath "$batchPath/capacity-ledger$Attempt.json").Length;final_summary_bytes=0;batch_bytes=0;batch_remaining_bytes=0;campaign_plus_prior_bytes=0;campaign_remaining_bytes=0;preservation_mismatches=$mismatches.Count;pending_independent_acceptance=$true}
        do{$previousSize=$summaryValue.final_summary_bytes;$summaryValue.batch_bytes=$batchBeforeSelf+$previousSize;$summaryValue.batch_remaining_bytes=33554432-$summaryValue.batch_bytes;$summaryValue.campaign_plus_prior_bytes=$totalBeforeSelf+$previousSize;$summaryValue.campaign_remaining_bytes=8589934592-$summaryValue.campaign_plus_prior_bytes;$summaryValue.final_summary_bytes=[Text.UTF8Encoding]::new($false).GetByteCount(($summaryValue|ConvertTo-Json -Depth 40)+"`n")}while($previousSize -ne $summaryValue.final_summary_bytes)
        Save-Bounded "final-summary$Attempt.json" $summaryValue
    }
}finally{if($owned){$lease.ReleaseMutex()};$lease.Dispose()}
