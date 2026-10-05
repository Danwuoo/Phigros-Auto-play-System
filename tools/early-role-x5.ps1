param([ValidateSet('Init','SaveRule','Manifest','Configure','Build','Tests','Reports','AsanReport','Negatives','QuotaNegatives','Regressions','PreserveReportTool','VerifyFinalReader','AttemptNotes','Freeze','Finalize')][string]$Mode='Init',[string]$Attempt='release',[string]$RunTag='')
$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$priorPath=Join-Path $repoPath 'measurements/research-next-20261001'
$batchPath=Join-Path $campaignPath 'early-role-x5'
$x4Path=Join-Path $campaignPath 'preconfirmation-role-x4'
function Bytes([string]$p){if(!(Test-Path -LiteralPath $p)){return [long]0};[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Write-Bounded([string]$name,[string]$value){
    $targetPath=Join-Path $batchPath $name
    $payload=[Text.UTF8Encoding]::new($false).GetBytes($value)
    $mutex=[Threading.Mutex]::new($false,'Local\PAS_X1ContactReplayBudget');$owned=$false
    try{$owned=$mutex.WaitOne(0);if(!$owned){throw 'batch_writer_busy'}
        if((Bytes $batchPath)+$payload.Length -gt 16777216 -or (Bytes $campaignPath)+(Bytes $priorPath)+$payload.Length -gt 8589934592){throw 'X5 development quota; acceptance reserve 8MiB'}
        $stream=[IO.File]::Open($targetPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
        try{$stream.Write($payload,0,$payload.Length)}finally{$stream.Dispose()}
    }finally{if($owned){$mutex.ReleaseMutex()};$mutex.Dispose()}
}
function Save-Bounded([string]$name,$value){Write-Bounded $name (($value|ConvertTo-Json -Depth 60)+"`n")}
function Invoke-Logged([string]$name,[string]$exe,[string[]]$arguments){
    if(Test-Path -LiteralPath "$batchPath/$name-command.json"){throw 'command exists'}
    $message=(& $exe @arguments 2>&1|Out-String);$code=$LASTEXITCODE
    if([Text.Encoding]::UTF8.GetByteCount($message) -gt 1048576){throw 'console cap exceeded; no silent truncation'}
    Write-Bounded "$name.log" $message
    Save-Bounded "$name-command.json" @{exe=$exe;arguments=$arguments;exit_code=$code;log_sha256=(Hash "$batchPath/$name.log");environment=@{ASAN_OPTIONS=$env:ASAN_OPTIONS;PAS_RGB_CLIP_ROOT=$env:PAS_RGB_CLIP_ROOT}}
    Write-Output "$name exit=$code"
    if($code -ne 0){throw "$name exit=$code"}
}
if($Mode -eq 'Init'){
    if(Test-Path -LiteralPath $batchPath){throw 'X5 batch exists'}
    $campaignBefore=Bytes $campaignPath;$priorBefore=Bytes $priorPath
    New-Item -ItemType Directory -Path $batchPath|Out-Null
    $formal=@(Get-ChildItem "$repoPath/src","$repoPath/include" -Recurse -File)+@(Get-Item "$repoPath/CMakeLists.txt")
    $dirty=@(git -C $repoPath ls-files --modified;git -C $repoPath ls-files --others --exclude-standard)|Where-Object{$_ -ne 'tools/early-role-x5.ps1'}
    $oldRoots=@('contact-replay-x1','confirmed-preserve-x2','line-role-x3','preconfirmation-role-x4')
    $protected=@($formal.FullName)+@($dirty|ForEach-Object{Join-Path $repoPath $_})
    foreach($root in $oldRoots){$protected+=@(Get-ChildItem -LiteralPath (Join-Path $campaignPath $root) -File -Recurse|Select-Object -ExpandProperty FullName)}
    foreach($root in @('out/x1/c36h-v3','out/x1/main50-v2','out/x2/tools36','out/x2/tools50','out/x3/build/Release','out/x4/main50-provisional','out/x4/tools','out/x4/query-final','out/x4/asan/Debug',"measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/candidate36h-tint1-runtime")){
        if(Test-Path -LiteralPath "$repoPath/$root"){$protected+=@(Get-ChildItem -LiteralPath "$repoPath/$root" -File -Recurse|Select-Object -ExpandProperty FullName)}
    }
    Save-Bounded 'workspace-before.json' @{date='2026-10-02';timezone='Asia/Taipei';head=(git -C $repoPath rev-parse HEAD);branch=(git -C $repoPath branch --show-current);worktrees=@(git -C $repoPath worktree list --porcelain);status=@(git -C $repoPath status --short);strategy=[IO.File]::ReadAllText("$repoPath/include/pas/strategy_version.hpp");campaign_before=$campaignBefore;prior_before=$priorBefore;total_before=$campaignBefore+$priorBefore;old_batch_bytes=@($oldRoots|ForEach-Object{@{name=$_;bytes=(Bytes (Join-Path $campaignPath $_))}});protected=@($protected|Sort-Object -Unique|ForEach-Object{@{path=$_;sha256=(Hash $_)}})}
    Write-Bounded 'status-before.md' ([IO.File]::ReadAllText("$repoPath/docs/PROJECT_STATUS_NEXT_STEPS_20261001.md"))
    Write-Bounded 'experiment-spec-before.md' @'
# X5 experiment spec before tool changes, 2026-10-02 Asia/Taipei
Question: can current candidates and <=6 processed frames / <=90ms geometry discriminate early role eligibility, competition and abstain without per-frame proposals?
Inputs: accepted X2 C36h reference + original main50 control and accepted X4 trace-ON factual history, five fixed windows (109 frames each); main50 control and X4 have G1533-1537, C36h G is unavailable. Evaluate ALL current target occurrences and ALL current lines in these traces, not just D ROI. D selector/ordinal only marks research annex; no sampling key enters rule. Three roles are factual histories on ONE C36g Dlyrotz recording, not independent song samples. X2 disabled-preserve variant is excluded.
Features to explore: current note center/dimensions/tangent/quality; explicit pre-selection confirmed state and identity assignment/ambiguity; line center/tangent/span/association/current timestamp; measured note/line displacement, relative normal distance secant, support/along extent, competition. Model motion-valid is separately measured; false does not erase pose secants. Unknown confirmation or correspondence cannot qualify.
H1: a note-driven normal approach to a sufficiently supported line can outrank a nearby tangential line before confirmation. H2: line-driven closing, incidental overlap, first observation, ambiguous identity, same-direction neighbors or insufficient evidence must abstain. H3: appearance alignment cannot be mandatory for late alignment.
Candidate family: restricted stable note-driven approach; no appearance-only initial guess, no closing-only or overlap-only guess. Compare C36h role-class gate and main50 score diagnostics before freezing at most ONE rule/threshold version. No threshold sweep after results. A restricted rule may abstain on moving/rotating lines; must report that lost scope rather than call it universal.
Controls/falsifiers: original crossing gold + permutation/local-ID rename + rigid transforms; late alignment/turning note; line chasing note and moving-line closing; parallel neighbor/short fragment/missing/stale/current invalid/tie; missing history/identity ambiguity; Hold head/body/tail and rotating held contact excluded from rule, original owner regressions preserve unknown/completed no retry. Geometry alone cannot disambiguate constructed identical-pixels/different-role worlds; selecting such a witness falsifies unique-role sufficiency.
Success: nonempty useful early recommendations, explicit unknowns/margins, order/rigid-transform invariance, no selection under declared abstain boundary; only a rule surviving meaningful role counterexamples merits a next isolated replay proposal. Failure is deliverable: if counterexample beats it, freeze negative result, retain failure, STOP rule search. All-abstain is not an improvement.
Stop: one frozen rule formal evaluation, no new full contact replay; no runtime wiring, owner mutation, emulator/live/training/goal/automation/commit/push. Never claim shadow restores Down or avoids Miss. No precision/recall/accuracy without role gold.
Bounds: report<=6MiB; rows<=2MiB; trace input<=16MiB per role; JSON query<=2MiB; lines<=16, notes<=128 per frame, history<=6 frames/90ms, retained report frames<=337, index<=36000. New evidence only early-role-x5<=24MiB, DEVELOPMENT<=16MiB to reserve >=8MiB; campaign+prior<=8GiB. CREATE_NEW/shared budget mutex. Build products only out/x5 and separately counted. Original PNG/trace referenced, no copies. Logs/failed attempts/snapshots/ledgers count. Two byte-deterministic reports, actual CLI rejection tests, Release and Debug-ASan for new bounded reader/state. No runtime latency claims.
'@
    $bindings=@();foreach($p in @('acceptance-20261002/acceptance-summary.json','input-manifest.json','query-manifest.json','causal-report.json')){$bindings+=@{path=(Join-Path $x4Path $p);sha256=(Hash (Join-Path $x4Path $p))}}
    Save-Bounded 'parent-checks.json' @{x4=$bindings;x3_acceptance_sha256=(Hash "$campaignPath/line-role-x3/acceptance-20261002/acceptance-summary.json");expected_x4_acceptance='7f685429c59de8cf4bd0ab6b65f5f14da1c0bb8ea245344fc8d8fef67cd01fb8';expected_x4_input='7c266f7b1163e5bba920fe901d48b48b3248b9b9e8c17647e165a93406261266';expected_x4_report='cab91f05748691f81849eda8b5861986ae3e2afb8323321e1511509302299e47';expected_x3_acceptance='75ae973b9003432493c0096b5c2d281669a27953b2a4e708f5e5b054d4c88fd6'}
    Write-Output "X5 initialized; prior campaign+research=$($campaignBefore+$priorBefore), remaining=$(8589934592-$campaignBefore-$priorBefore)"
}elseif($Mode -eq 'SaveRule'){
    Write-Bounded 'rule-freeze-v1.json' @'
{"schema":1,"rule":"X5-NDA-v1","frozen_before_new_rule_code_and_formal_evaluation":true,"history_frames":6,"history_ns":90000000,"minimum_poses":3,"minimum_span_ns":12000000,"minimum_travel_px":3.0,"minimum_closing_px":3.0,"maximum_speed_px_s":4000.0,"minimum_normal_fraction":0.85,"maximum_line_contribution_ratio":0.5,"maximum_pair_angle_rad":0.10,"minimum_margin":0.15,"minimum_line_length_frame_fraction":0.32,"minimum_line_confidence":0.5,"early_scope":"explicit confirmed=0, explicit unambiguous correspondence, strong current non-Hold core; no owner or action input","history":"same run exported unique identity edges linking only stored current observations, candidate line same-run ID at all note timestamps; IDs only index continuity, numeric value never score","competition":"rank eligible by minimum measured note-normal displacement fraction; another current supported candidate with missing paired history or line-driven/rotating closing is unresolved and vetoes selection; parallel eligible tie/margin<.15 abstains","appearance":"measured but never eligibility predicate","overlap":"distance<=height/2+2 abstains, not role proof","falsifier":"same prefix supports note approaching a crossing decoration while true role has future late turn/alignment; geometrically selected line need not be judgment role","decision_after_falsifier":"reject v1, no threshold revision, no isolated replay proposal"}
'@
    Save-Bounded 'exploration-before-freeze.json' @{source='existing accepted X3 causal-features-1verified.json, raw accepted X2/X4 traces and frozen C36h/main50 source; no new threshold evaluation';x3_report_sha256=(Hash "$campaignPath/line-role-x3/causal-features-1verified.json");observations=@('6160 first current core has no prior measured motion; no 6161 backfill.','6161 pair spans 4.2067ms; note speed about5941px/s vs6162 717px/s, so single short pair cannot represent stable model.','Vertical315 pose secants remain valid across model motion_valid false, while horizontal316 recedes in6161-64.','6194 horizontal317 closes -1052.7px/s with note-normal0; closing-only is confounded by line motion.','main50 exports confirmed_line_id and identity assignment, C36h does not export confirmed state.','G C36h context absent; other songs have no bound compatible contact trace in this input scope.');threshold_choice='C36h .85 normal fraction and .32 support reused; 3 current poses/12ms excludes initial/burst-only inference, 0.5 line-contribution and .10rad restrict regime; .15 competition margin predeclared. One version only, not fitted role truth.'}
}elseif($Mode -eq 'Manifest'){
    $m=Get-Content -LiteralPath "$x4Path/input-manifest.json" -Raw|ConvertFrom-Json
    $q=Get-Content -LiteralPath "$x4Path/query-manifest.json" -Raw|ConvertFrom-Json
    Save-Bounded 'input-manifest.json' @{schema=1;experiment='early_role_abstain_x5';batch_root=$batchPath;campaign_root=$campaignPath;prior_research_root=$priorPath;development_limit_bytes=16777216;batch_limit_bytes=25165824;rule_path="$batchPath/rule-freeze-v1.json";rule_sha256=(Hash "$batchPath/rule-freeze-v1.json");x4_manifest_path="$x4Path/input-manifest.json";x4_manifest_sha256=(Hash "$x4Path/input-manifest.json");x4_acceptance_path="$x4Path/acceptance-20261002/acceptance-summary.json";x4_acceptance_sha256=(Hash "$x4Path/acceptance-20261002/acceptance-summary.json");x4_query_path="$x4Path/query-manifest.json";x4_query_sha256=(Hash "$x4Path/query-manifest.json");index_sha256=$m.index_sha256;runs=@(@{role='c36h_reference';root=$m.reused_runs.c36h_reference.root;files=$m.reused_runs.c36h_reference.files;source=$m.reused_runs.c36h_reference.source_binding},@{role='main50_control';root=$m.reused_runs.main50_control.root;files=$m.reused_runs.main50_control.files;source=$m.reused_runs.main50_control.source_binding},@{role='main50_preconfirmation_role_oracle';root=$q.runs.variant_on.root;files=$q.runs.variant_on.files;source=$m.variant_source});bridge='C36h/main50 old runs produced under X2 manifest; X4 under X4 manifest; X5 reads only and does not regenerate state/action trajectories.'}
}elseif($Mode -eq 'Configure'){
    $buildPath="$repoPath/out/x5/$Attempt"
    $argsList=@('-S',"$repoPath/apps/frame_review/x5_offline",'-B',$buildPath,'-G','Visual Studio 18 2026','-A','x64','-T','v145','-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275',"-DX1_SOURCE=$repoPath/out/x1/main50-v2","-DX1_REPO=$repoPath","-DCMAKE_PREFIX_PATH=$repoPath/out/vcpkg_installed/x64-windows")
    if($Attempt -eq 'asan'){$argsList+='-DX1_SANITIZER_SUPPORT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include'}
    Invoke-Logged "configure-$Attempt$RunTag" 'cmake' $argsList
}elseif($Mode -eq 'Build'){
    $configName=if($Attempt -eq 'asan'){'Debug'}else{'Release'}
    Invoke-Logged "build-$Attempt$RunTag" 'cmake' @('--build',"$repoPath/out/x5/$Attempt",'--config',$configName,'--target','x5_report','x5_tests','--parallel','2')
    $dllRoot=if($Attempt -eq 'asan'){"$repoPath/out/vcpkg_installed/x64-windows/debug/bin"}else{"$repoPath/out/vcpkg_installed/x64-windows/bin"}
    $zName=if($Attempt -eq 'asan'){'zlibd1.dll'}else{'z.dll'}
    if(!(Test-Path -LiteralPath "$dllRoot/$zName")){$zName=if($Attempt -eq 'asan'){'zd.dll'}else{'z.dll'}}
    Copy-Item -LiteralPath "$dllRoot/$zName","$dllRoot/gtest.dll","$dllRoot/gtest_main.dll" -Destination "$repoPath/out/x5/$Attempt/$configName"
}elseif($Mode -eq 'Tests'){
    $configName=if($Attempt -eq 'asan'){'Debug'}else{'Release'}
    Invoke-Logged "tests-$Attempt$RunTag" "$repoPath/out/x5/$Attempt/$configName/x5_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/tests-$Attempt$RunTag.xml")
}elseif($Mode -eq 'Reports'){
    Invoke-Logged "report-1-$Attempt" "$repoPath/out/x5/release/Release/x5_report.exe" @("$batchPath/input-manifest.json","$batchPath/shadow-report-1-$Attempt.json")
    Invoke-Logged "report-2-$Attempt" "$repoPath/out/x5/release/Release/x5_report.exe" @("$batchPath/input-manifest.json","$batchPath/shadow-report-2-$Attempt.json")
    Save-Bounded "determinism-$Attempt.json" @{byte_identical=((Hash "$batchPath/shadow-report-1-$Attempt.json") -eq (Hash "$batchPath/shadow-report-2-$Attempt.json"));sha256=(Hash "$batchPath/shadow-report-1-$Attempt.json");bytes=(Get-Item -LiteralPath "$batchPath/shadow-report-1-$Attempt.json").Length;new_full_replays=0}
}elseif($Mode -eq 'Negatives'){
    $toolPath="$repoPath/out/x5/release/Release/x5_report.exe";$results=@();$existingPath="$batchPath/shadow-report-1-compact1.json";$beforeHash=Hash $existingPath;$prefix="negative$RunTag-"
    foreach($caseName in @('schema-float','id-float','wrong-parent','wrong-acceptance','wrong-query','wrong-rule','wrong-index','wrong-role','swapped-root','missing-files','wrong-trace-sha','wrong-binary-sha','wrong-source-sha','wrong-tracking-sha','wrong-budget','outside-output','missing-input','existing-output')){
        $m=Get-Content -LiteralPath "$batchPath/input-manifest.json" -Raw|ConvertFrom-Json
        $outputPath="$batchPath/$prefix$caseName-output.json";$inputPath="$batchPath/$prefix$caseName-input.json"
        $expectedReason=''
        switch($caseName){
            'schema-float'{$m.schema=1.0;$expectedReason='x5_manifest_contract'}
            'id-float'{$m.batch_limit_bytes=25165824.0;$expectedReason='x5_integer_schema'}
            'wrong-parent'{$m.x4_manifest_sha256=('0'*64);$expectedReason='x5_parent_SHA'}
            'wrong-acceptance'{$m.x4_acceptance_sha256=('0'*64);$expectedReason='x5_acceptance_SHA'}
            'wrong-query'{$m.x4_query_sha256=('0'*64);$expectedReason='x5_query_SHA'}
            'wrong-rule'{$m.rule_sha256=('0'*64);$expectedReason='x5_rule_SHA'}
            'wrong-index'{$m.index_sha256=('0'*64);$expectedReason='x5_index_SHA'}
            'wrong-role'{$m.runs[1].role='main50_no_confirmed_winner_override';$expectedReason='x5_role_order'}
            'swapped-root'{$m.runs[1].root=$m.runs[2].root;$expectedReason='x5_run_root_binding'}
            'missing-files'{$m.runs[1].files.PSObject.Properties.Remove('trace.jsonl');$expectedReason='x5_file_binding'}
            'wrong-trace-sha'{$m.runs[1].files.'trace.jsonl'=('0'*64);$expectedReason='x5_file_binding'}
            'wrong-binary-sha'{$m.runs[1].source.binary_sha256=('0'*64);$expectedReason='x5_source_binary_binding'}
            'wrong-source-sha'{$m.runs[1].source.source_provenance_sha256=('0'*64);$expectedReason='x5_source_binary_binding'}
            'wrong-tracking-sha'{$m.runs[1].source.tracking_source_sha256=('0'*64);$expectedReason='x5_source_binary_binding'}
            'wrong-budget'{$m.development_limit_bytes=25165824;$expectedReason='x5_manifest_contract'}
            'outside-output'{$outsidePath="$batchPath/outside$RunTag";$outputPath="$outsidePath/negative-output.json";$expectedReason='x5_output_outside_batch';New-Item -ItemType Directory -Path $outsidePath|Out-Null}
            'missing-input'{$inputPath="$batchPath/deliberately-missing-input.json";$expectedReason='file_size'}
            'existing-output'{$outputPath=$existingPath;$expectedReason='x5_output_exists'}
        }
        if($caseName -ne 'missing-input'){Save-Bounded "$prefix$caseName-input.json" $m}
        $message=(& $toolPath $inputPath $outputPath 2>&1|Out-String);$code=$LASTEXITCODE
        Write-Bounded "$prefix$caseName.log" $message
        $pass=$code -ne 0 -and $message.Contains($expectedReason) -and !$message.Contains('batch_writer_busy') -and ($caseName -eq 'existing-output' -or !(Test-Path -LiteralPath $outputPath))
        $results+=@{name=$caseName;exe=$toolPath;arguments=@($inputPath,$outputPath);exit=$code;reason=$message.Trim();expected_reason=$expectedReason;passed=$pass;output_exists=(Test-Path -LiteralPath $outputPath)}
    }
    foreach($oldCase in @('R1','R2')){
        $x1="$campaignPath/contact-replay-x1";$oldTool="$repoPath/out/x2/tools50/pas_frame_review.exe";$a="$x1/c36h-verified-on-1";$b="$x1/main50-verified-on-1";$inputPath="$x1/input-manifest.json";$expectedReason='comparison_lineage_expected_c36h'
        if($oldCase -eq 'R1'){$inputPath="$x1/acceptance-20261002/wrong-input-manifest.json";$expectedReason='comparison_manifest_SHA_mismatch'}else{$a="$x1/main50-verified-on-1";$b="$x1/c36h-verified-on-1"}
        $argsList=@('contact-compare',$inputPath,$a,$b,"$batchPath/$prefix$oldCase-output.json")
        $message=(& $oldTool @argsList 2>&1|Out-String);$code=$LASTEXITCODE;Write-Bounded "$prefix$oldCase.log" $message
        $results+=@{name=$oldCase;exe=$oldTool;arguments=$argsList;exit=$code;reason=$message.Trim();expected_reason=$expectedReason;passed=($code -ne 0 -and $message.Contains($expectedReason) -and !(Test-Path -LiteralPath $argsList[-1]))}
    }
    Save-Bounded "cli-negative$RunTag-results.json" @{cases=$results;expected_rejections=$results.Count;passed=@($results|Where-Object passed).Count;existing_before=$beforeHash;existing_after=(Hash $existingPath);existing_unchanged=($beforeHash -eq (Hash $existingPath));no_mutex_false_coverage=$true;reader_sha256=(Hash $toolPath)}
    if(@($results|Where-Object{!$_.passed}).Count){throw 'negative mismatch; full results retained'}
}elseif($Mode -eq 'QuotaNegatives'){
    $toolPath="$repoPath/out/x5/release/Release/x5_report.exe";$results=@();$manifest="$batchPath/input-manifest.json";$existing="$batchPath/shadow-report-1-compact1.json";$beforeHash=Hash $existing
    $cases=@(@{name='unknown-mode';input=$manifest;output="$batchPath/unknown-mode-output.json";flag='--anything';reason='x5_unknown_output_mode'},@{name='acceptance-existing';input=$manifest;output=$existing;flag='--acceptance';reason='x5_output_exists'},@{name='acceptance-parent';input="$batchPath/negative-quota1-wrong-parent-input.json";output="$batchPath/acceptance-parent-output.json";flag='--acceptance';reason='x5_parent_SHA'},@{name='acceptance-outside';input=$manifest;output="$batchPath/outside-quota1/acceptance-outside.json";flag='--acceptance';reason='x5_output_outside_batch'},@{name='default-development-cap';input=$manifest;output="$batchPath/default-cap-output.json";flag=$null;reason='x5_output_quota'},@{name='verify-missing';input=$manifest;output="$batchPath/verify-missing.json";flag='--verify-existing';reason='file_size'})
    foreach($c in $cases){$argsList=@($c.input,$c.output);if($c.flag){$argsList+=$c.flag};$message=(& $toolPath @argsList 2>&1|Out-String);$code=$LASTEXITCODE;Write-Bounded "quota-negative-$($c.name).log" $message;$pass=$code -ne 0 -and $message.Contains($c.reason) -and !$message.Contains('batch_writer_busy') -and ($c.output -eq $existing -or !(Test-Path -LiteralPath $c.output));$results+=@{name=$c.name;arguments=$argsList;exit_code=$code;reason=$message.Trim();expected_reason=$c.reason;passed=$pass}}
    Save-Bounded 'quota-cli-negative-results.json' @{exe=$toolPath;reader_sha256=(Hash $toolPath);cases=$results;expected=6;passed=@($results|Where-Object passed).Count;existing_unchanged=($beforeHash -eq (Hash $existing));acceptance_success_write_not_performed_to_preserve_reserve=$true}
    if(@($results|Where-Object{!$_.passed}).Count){throw 'quota CLI mismatch; results retained'}
}elseif($Mode -eq 'AsanReport'){
    $savedAsanOptions=$env:ASAN_OPTIONS
    try{
        if($RunTag -eq '-bounded1'){$env:ASAN_OPTIONS='quarantine_size_mb=16:thread_local_quarantine_size_kb=64'}
        Invoke-Logged "asan-real-reader$RunTag" "$repoPath/out/x5/asan/Debug/x5_report.exe" @("$batchPath/input-manifest.json","$batchPath/shadow-report-asan.json")
    }finally{$env:ASAN_OPTIONS=$savedAsanOptions}
    $releaseText=[IO.File]::ReadAllText("$batchPath/shadow-report-1-compact1.json");$asanText=[IO.File]::ReadAllText("$batchPath/shadow-report-asan.json")
    $releaseBinary=Hash "$repoPath/out/x5/release/Release/x5_report.exe";$asanBinary=Hash "$repoPath/out/x5/asan/Debug/x5_report.exe"
    $needle='"analysis_binary_sha256":"'+$asanBinary+'"';$replacement='"analysis_binary_sha256":"'+$releaseBinary+'"'
    if(!$asanText.Contains($needle)){throw 'ASan report binary binding absent'}
    $equal=$releaseText -ceq $asanText.Replace($needle,$replacement)
    Save-Bounded 'asan-reader-equivalence.json' @{scope='Actual bound reader processed all 337 role frames under Debug-ASan; binary SHA differs, no replay';semantic_check='exact UTF8 document text after replacing only analysis_binary_sha256';semantic_equal=$equal;release_binary_sha256=$releaseBinary;asan_binary_sha256=$asanBinary;release_sha256=(Hash "$batchPath/shadow-report-1-compact1.json");asan_sha256=(Hash "$batchPath/shadow-report-asan.json");asan_bytes=(Get-Item -LiteralPath "$batchPath/shadow-report-asan.json").Length}
    if(!$equal){throw 'Release / ASan semantic mismatch; evidence retained'}
}elseif($Mode -eq 'PreserveReportTool'){
    $snap='source-pre-acceptance__x5_report.cpp';Write-Bounded $snap ([IO.File]::ReadAllText("$repoPath/apps/frame_review/x5_report.cpp"))
    $products=@();foreach($config in @(@{build='release';name='Release'},@{build='asan';name='Debug'})){
        $destination="$repoPath/out/x5/report-tool-compact1/$($config.name)";if(Test-Path -LiteralPath $destination){throw 'retained report tool exists'};New-Item -ItemType Directory -Path $destination|Out-Null
        foreach($p in Get-ChildItem -LiteralPath "$repoPath/out/x5/$($config.build)/$($config.name)" -File|Where-Object Extension -In '.exe','.dll'){
            $copy=Join-Path $destination $p.Name;Copy-Item -LiteralPath $p.FullName -Destination $copy
            if((Hash $copy) -ne (Hash $p.FullName)){throw 'retained product mismatch'};$products+=@{original_run_path=$p.FullName;retained_path=$copy;bytes=$p.Length;sha256=(Hash $copy)}
        }
    }
    Save-Bounded 'report-tool-before-acceptance.json' @{changed_source_snapshot=$snap;sha256=(Hash "$batchPath/$snap");unchanged_other_analysis_sources=@('apps/frame_review/x5_rule.hpp','apps/frame_review/x5_adapter.hpp','apps/frame_review/x5_scenarios.hpp','apps/frame_review/x5_main.cpp','apps/frame_review/x5_report.hpp','tests/x5_rule_tests.cpp','apps/frame_review/x5_offline/CMakeLists.txt')|ForEach-Object{@{path=(Join-Path $repoPath $_);sha256=(Hash (Join-Path $repoPath $_))}};products=$products;purpose='Preserve actual binaries/source of compact1 Release reports and successful bounded-ASan reader before quota-only CLI revision; one rule version';only_compiled_products_in_out=$true}
}elseif($Mode -eq 'VerifyFinalReader'){
    Invoke-Logged 'final-release-reader-verification' "$repoPath/out/x5/release/Release/x5_report.exe" @("$batchPath/input-manifest.json","$batchPath/shadow-report-1-compact1.json",'--verify-existing')
    Invoke-Logged 'final-release-reader-verification-2' "$repoPath/out/x5/release/Release/x5_report.exe" @("$batchPath/input-manifest.json","$batchPath/shadow-report-2-compact1.json",'--verify-existing')
    $savedAsanOptions=$env:ASAN_OPTIONS;try{$env:ASAN_OPTIONS='quarantine_size_mb=16:thread_local_quarantine_size_kb=64';Invoke-Logged 'final-asan-reader-verification' "$repoPath/out/x5/asan/Debug/x5_report.exe" @("$batchPath/input-manifest.json","$batchPath/shadow-report-1-compact1.json",'--verify-existing')}finally{$env:ASAN_OPTIONS=$savedAsanOptions}
    Save-Bounded 'final-reader-verification.json' @{release_binary_sha256=(Hash "$repoPath/out/x5/release/Release/x5_report.exe");asan_binary_sha256=(Hash "$repoPath/out/x5/asan/Debug/x5_report.exe");semantic_equal=$true;scope='Final quota/verification CLI binaries recompute all 337 role frames from pinned source inputs and compare complete JSON to both retained reports, excluding only analysis_binary_sha256; no new full replay or new report bytes';verified_commands=@('final-release-reader-verification-command.json','final-release-reader-verification-2-command.json','final-asan-reader-verification-command.json');acceptance_cli='--acceptance: batch 24MiB/campaign 8GiB; does not change rule or features; default writer remains development 16MiB';repair_reason='Original development reader also limited acceptance to 16MiB, making reserved headroom unusable for report recomputation; explicit output-only mode fixes this without consuming acceptance reserve during development';previous_report_tool='report-tool-before-acceptance.json';rule_threshold_versions=1;new_full_replays=0}
}elseif($Mode -eq 'Regressions'){
    $env:PAS_RGB_CLIP_ROOT="$campaignPath/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
    if($env:PAS_X2_NO_WINNER_OVERRIDE){throw 'control environment must preserve original override'}
    Invoke-Logged 'regression36' "$repoPath/out/x2/tools36/x1_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/regression36.xml")
    Invoke-Logged 'regression50' "$repoPath/out/x2/tools50/x1_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/regression50.xml")
    Invoke-Logged 'regression-x3' "$repoPath/out/x3/build/Release/x3_tests.exe" @('--gtest_brief=1')
    Invoke-Logged 'regression-x4-owner-guards' "$repoPath/out/x4/tools/x1_tests.exe" @('--gtest_filter=X4Oracle.UnknownDownNeverRetriesThroughHookAndOwner:X4Oracle.CompletedOwnerCannotResurrectAfterNewProvisionalChoice','--gtest_brief=1',"--gtest_output=xml:$batchPath/regression-x4-owner-guards.xml")
    Invoke-Logged 'regression-x1-positive' "$repoPath/out/x2/tools50/pas_frame_review.exe" @('contact-compare',"$campaignPath/contact-replay-x1/input-manifest.json","$campaignPath/contact-replay-x1/c36h-verified-on-1","$campaignPath/contact-replay-x1/main50-verified-on-1","$batchPath/old-entry-positive.json")
    Save-Bounded 'old-entry-equivalence.json' @{expected='37d469caa0017be00fb78736fd69fb5cc87585c40b9fd671bb7e8397c420e7f7';actual=(Hash "$batchPath/old-entry-positive.json");byte_equal=((Hash "$batchPath/old-entry-positive.json") -eq '37d469caa0017be00fb78736fd69fb5cc87585c40b9fd671bb7e8397c420e7f7');new_full_replays=0}
}elseif($Mode -eq 'AttemptNotes'){
    $failed=@(Get-ChildItem -LiteralPath $batchPath -Filter '*-command.json' -File|ForEach-Object{Get-Content -LiteralPath $_.FullName -Raw|ConvertFrom-Json|Where-Object exit_code -NE 0})
    Save-Bounded 'attempt-notes.json' @{date='2026-10-02';saved_failed_commands=$failed;additional_wrapper_failure=@{command='./tools/early-role-x5.ps1 -Mode AsanReport';reason='batch_writer_busy at wrapper Write-Bounded while Negatives held shared budget mutex';output_existed_after_attempt=$false;child_exit_code=$null;console_log_not_saved=$true;disposition='not counted as ASan pass or CLI rejection coverage; sequential rerun after Negatives/Regressions completion';source='functions.exec tool result, execution exit=1';timestamp_not_measured=$true};rule_threshold_versions=1;repairs=@('GTest imported target visibility in standalone CMake scope','GTest macro label collision on same source line','CLI error-text assertion accepts actual MSVC file_size message','target lacks tangent: read only bound current candidate geometry; absent/mismatched appearance stays unknown','full report exceeded 6MiB: shared column arrays retain every frame/object/feature without truncation','ASan reader default quarantine failed mmap under unchanged 512MiB process-commit cap; same binary passes with quarantine_size_mb=16:thread_local_quarantine_size_kb=64; shorter retained freed-block window is an explicit instrumentation limitation');no_deleted_attempts=$true;negative_results_passed=(Get-Content -LiteralPath "$batchPath/cli-negative-results.json" -Raw|ConvertFrom-Json).passed;regression_positive_equal=(Get-Content -LiteralPath "$batchPath/old-entry-equivalence.json" -Raw|ConvertFrom-Json).byte_equal;environment=@{powershell=$PSVersionTable.PSVersion.ToString();os=[Environment]::OSVersion.VersionString;processor_count=[Environment]::ProcessorCount;processor=@(Get-CimInstance Win32_Processor|Select-Object Name);cmake=(& cmake --version|Select-Object -First 1);compiler=([IO.File]::ReadAllLines("$repoPath/out/x5/release/CMakeFiles/4.2.1/CMakeCXXCompiler.cmake")|Where-Object{$_ -match 'CMAKE_CXX_COMPILER_VERSION|CMAKE_CXX_COMPILER_ID'});input_geometry='1280x720, factual traces derive from original RGB only; synthetic rule fixtures are typed geometry, not rendered PNG';new_runtime_latency_measurements=0}}
}elseif($Mode -eq 'Freeze'){
    $names=@('apps/frame_review/x5_rule.hpp','apps/frame_review/x5_adapter.hpp','apps/frame_review/x5_scenarios.hpp','apps/frame_review/x5_report.hpp','apps/frame_review/x5_main.cpp','apps/frame_review/x5_report.cpp','apps/frame_review/x5_offline/CMakeLists.txt','tests/x5_rule_tests.cpp','tools/early-role-x5.ps1')
    New-Item -ItemType Directory -Path "$batchPath/source"|Out-Null
    $sources=@();foreach($name in $names){$path=Join-Path $repoPath $name;$snapshot="source/$($name.Replace('/','__'))";Write-Bounded $snapshot ([IO.File]::ReadAllText($path));if((Hash $path) -ne (Hash "$batchPath/$snapshot")){throw 'source snapshot differs'};$sources+=@{path=$path;sha256=(Hash $path);snapshot=$snapshot;snapshot_sha256=(Hash "$batchPath/$snapshot")}}
    Save-Bounded 'tool-freeze.json' @{sources=$sources;binaries=@(Get-ChildItem -LiteralPath "$repoPath/out/x5/release/Release","$repoPath/out/x5/asan/Debug" -File|Where-Object Extension -In '.exe','.dll'|ForEach-Object{@{path=$_.FullName;bytes=$_.Length;sha256=(Hash $_.FullName)}});reused_export=@(Get-ChildItem -LiteralPath "$repoPath/out/x1/main50-v2" -File -Recurse|ForEach-Object{@{path=$_.FullName;sha256=(Hash $_.FullName)}});shared_reader_sources=@('apps/frame_review/x3_features.hpp','apps/frame_review/review_io.hpp','apps/frame_review/x4_input.hpp','apps/frame_review/x2_input.hpp','apps/frame_review/comparison_input.hpp','apps/frame_review/offline/CMakeLists.txt')|ForEach-Object{@{path=(Join-Path $repoPath $_);sha256=(Hash (Join-Path $repoPath $_))}};new_export=0;core='frozen X1 main50-v2 linked for existing SHA/reader utilities; observer/owner never called by x5_report'}
}elseif($Mode -eq 'Finalize'){
    $before=Get-Content -LiteralPath "$batchPath/workspace-before.json" -Raw|ConvertFrom-Json
    $statusPath=Join-Path $repoPath 'docs/PROJECT_STATUS_NEXT_STEPS_20261001.md'
    $mismatches=@($before.protected|Where-Object{$_.path -ne $statusPath -and (Hash $_.path) -ne $_.sha256})
    $old=[IO.File]::ReadAllText("$batchPath/status-before.md");$current=[IO.File]::ReadAllText($statusPath);$marker='**C36h tint1';$history=$old.Substring($old.IndexOf($marker));$same=$current.Substring($current.IndexOf($marker)).StartsWith($history,[StringComparison]::Ordinal)
    $freeze=Get-Content -LiteralPath "$batchPath/tool-freeze.json" -Raw|ConvertFrom-Json
    $sourceBad=@($freeze.sources|Where-Object{(Hash $_.path) -ne $_.sha256 -or (Hash "$batchPath/$($_.snapshot)") -ne $_.snapshot_sha256})
    $binaryBad=@($freeze.binaries|Where-Object{(Hash $_.path) -ne $_.sha256})
    $inherited=@();$f1=Get-Content -LiteralPath "$campaignPath/contact-replay-x1/tool-freeze-final.json" -Raw|ConvertFrom-Json;$inherited+=@($f1.binaries);foreach($export in $f1.exports){$inherited+=@($export.files|ForEach-Object{@{path=(Join-Path $export.root $_.path);sha256=$_.sha256}})}
    $f2=Get-Content -LiteralPath "$campaignPath/confirmed-preserve-x2/tool-freeze.json" -Raw|ConvertFrom-Json;$inherited+=@($f2.binaries)+@($f2.exports)+@($f2.sources|ForEach-Object{@{path=$_.snapshot;sha256=$_.sha256}})
    $f3=Get-Content -LiteralPath "$campaignPath/line-role-x3/tool-freeze.json" -Raw|ConvertFrom-Json;$inherited+=@($f3.binaries)+@($f3.reused_export)+@($f3.sources|ForEach-Object{@{path=(Join-Path "$campaignPath/line-role-x3" $_.snapshot);sha256=$_.snapshot_sha256}})
    $f4=Get-Content -LiteralPath "$x4Path/final-tool-freeze.json" -Raw|ConvertFrom-Json;$inherited+=@($f4.binaries)+@($f4.source|ForEach-Object{@{path=(Join-Path $repoPath $_.path);sha256=$_.sha256};@{path=$_.snapshot;sha256=$_.snapshot_sha256}})
    $legacy=Get-Content -LiteralPath "$batchPath/report-tool-before-acceptance.json" -Raw|ConvertFrom-Json
    $inherited+=@($legacy.products|ForEach-Object{@{path=$_.retained_path;sha256=$_.sha256}})+@($legacy.unchanged_other_analysis_sources)+@(@{path=(Join-Path $batchPath $legacy.changed_source_snapshot);sha256=$legacy.sha256})
    $inheritedBad=@($inherited|Where-Object{(Hash $_.path) -ne $_.sha256})
    $formalDiff=@(git -C $repoPath diff --name-only -- src include CMakeLists.txt);$head=git -C $repoPath rev-parse HEAD;$branch=git -C $repoPath branch --show-current;$worktrees=@(git -C $repoPath worktree list --porcelain)
    $oldBytes=@($before.old_batch_bytes|ForEach-Object{@{name=$_.name;before=$_.bytes;after=(Bytes (Join-Path $campaignPath $_.name))}})
    $externalBytes=(Bytes $campaignPath)+(Bytes $priorPath)-(Bytes $batchPath);$externalSame=$externalBytes -eq $before.total_before
    Save-Bounded 'preservation-check.json' @{protected_file_checks=$before.protected.Count;mismatches=$mismatches;formal_diff=$formalDiff;head=$head;branch=$branch;worktrees=$worktrees;source_snapshot_mismatches=$sourceBad;binary_mismatches=$binaryBad;inherited_freeze_file_checks=$inherited.Count;inherited_freeze_mismatches=$inheritedBad;earlier_shared_workspace_source_is_checked_against_latest_X4_or_before_not_obsolete_X2_X3_source=$true;status_A_I_J1_J11_exact_body_preserved=$same;original_status_sha256=(Hash "$batchPath/status-before.md");old_batch_bytes=$oldBytes;campaign_outside_X5_bytes=$externalBytes;campaign_outside_X5_unchanged=$externalSame}
    if($mismatches.Count -or !$same -or $sourceBad.Count -or $binaryBad.Count -or $inheritedBad.Count -or $formalDiff.Count -or $head -ne $before.head -or $branch -ne $before.branch -or ($worktrees -join "`n") -cne ($before.worktrees -join "`n") -or @($oldBytes|Where-Object{$_.before -ne $_.after}).Count -or !$externalSame){throw 'X5 preservation mismatch'}
    $det=Get-Content -LiteralPath "$batchPath/determinism-compact1.json" -Raw|ConvertFrom-Json;$asan=Get-Content -LiteralPath "$batchPath/asan-reader-equivalence.json" -Raw|ConvertFrom-Json;$neg=Get-Content -LiteralPath "$batchPath/cli-negative-results.json" -Raw|ConvertFrom-Json;$oldEntry=Get-Content -LiteralPath "$batchPath/old-entry-equivalence.json" -Raw|ConvertFrom-Json
    $finalReader=Get-Content -LiteralPath "$batchPath/final-reader-verification.json" -Raw|ConvertFrom-Json;$finalNeg=Get-Content -LiteralPath "$batchPath/cli-negative-quota1-results.json" -Raw|ConvertFrom-Json;$quotaNeg=Get-Content -LiteralPath "$batchPath/quota-cli-negative-results.json" -Raw|ConvertFrom-Json
    if(!$det.byte_identical -or !$asan.semantic_equal -or $neg.passed -ne 20 -or !$neg.existing_unchanged -or !$oldEntry.byte_equal -or (Hash "$batchPath/shadow-report-1-compact1.json") -ne $det.sha256 -or (Hash "$batchPath/shadow-report-2-compact1.json") -ne $det.sha256 -or (Hash "$batchPath/shadow-report-asan.json") -ne $asan.asan_sha256 -or !$finalReader.semantic_equal -or $finalNeg.passed -ne 20 -or !$finalNeg.existing_unchanged -or $quotaNeg.passed -ne 6 -or !$quotaNeg.existing_unchanged -or (Hash "$repoPath/out/x5/release/Release/x5_report.exe") -ne $finalReader.release_binary_sha256 -or (Hash "$repoPath/out/x5/asan/Debug/x5_report.exe") -ne $finalReader.asan_binary_sha256){throw 'X5 self-check result mismatch'}
    $items=@(Get-ChildItem -LiteralPath $batchPath -File -Recurse|ForEach-Object{@{path=$_.FullName;bytes=$_.Length;sha256=(Hash $_.FullName)}})
    Save-Bounded 'capacity-ledger.json' @{artifacts=$items;out_x5_bytes=(Bytes "$repoPath/out/x5");new_export=0;batch_limit=25165824;development_limit=16777216;campaign_limit=8589934592;ledger_self_sha='omitted; self bytes included in final-summary';build_is_only_compiled_products_not_evidence=$true}
    $b=Bytes $batchPath;$c=(Bytes $campaignPath)+(Bytes $priorPath)
    $s=@{batch_before_self=$b;total_before_self=$c;final_summary_bytes=0;batch_bytes=0;development_remaining_bytes=0;acceptance_remaining_bytes=0;campaign_plus_prior_bytes=0;campaign_remaining_bytes=0;out_x5_bytes=(Bytes "$repoPath/out/x5");pending_independent_acceptance=$true;rule_decision='NDA-v1 rejected by late-turn indistinguishable-prefix counterexample; no isolated replay proposed';new_full_replays=0}
    do{$oldLength=$s.final_summary_bytes;$s.batch_bytes=$b+$oldLength;$s.development_remaining_bytes=16777216-$s.batch_bytes;$s.acceptance_remaining_bytes=25165824-$s.batch_bytes;$s.campaign_plus_prior_bytes=$c+$oldLength;$s.campaign_remaining_bytes=8589934592-$s.campaign_plus_prior_bytes;$s.final_summary_bytes=[Text.Encoding]::UTF8.GetByteCount(($s|ConvertTo-Json -Depth 60)+"`n")}while($oldLength -ne $s.final_summary_bytes)
    Save-Bounded 'final-summary.json' $s
}
