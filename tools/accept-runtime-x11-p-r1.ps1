$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue"
$sourcePath="$campaignPath/runtime-x11-p-r1";$reviewPath="$campaignPath/runtime-x11-p-r1-controller"
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$v){$p="$reviewPath/$name";if(Test-Path -LiteralPath $p){throw "existing $p"};[IO.File]::WriteAllText($p,($v|ConvertTo-Json -Depth 70)+"`n",[Text.UTF8Encoding]::new($false))}
function Run($name,$exe,[string[]]$arguments,[int]$expected){Save "$name-command.json" @{exe=$exe;arguments=$arguments;expected_exit=$expected;test_root=$env:R1_TEST_ROOT;rgb_root=$env:PAS_RGB_CLIP_ROOT};& $exe @arguments *> "$reviewPath/$name.log";$code=$LASTEXITCODE;Save "$name-exit.json" @{exit=$code;log_sha256=(Hash "$reviewPath/$name.log")};if($code -ne $expected){throw "$name exit $code expected $expected"};Write-Output "$name exit=$code"}
if(Test-Path -LiteralPath $reviewPath){throw 'new review root required'}
if((Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")+16MB -gt 8GB){throw 'reserve'}
New-Item -ItemType Directory -Path $reviewPath|Out-Null
Save 'workspace-before.json' @{head=(git rev-parse HEAD);branch=(git branch --show-current);worktrees=@(git worktree list);status=@(git status --short);formal_diff=@(git diff HEAD -- src include CMakeLists.txt);batch_limit=16MB;cost_runs_authorized=0;stress_runs_authorized=0}
$checks=@()
foreach($name in @('source-binding-before-cost.json','compiled-dependency-freeze.json','negative-tool-freeze.json','artifact-ledger.json')){
 $j=Get-Content -LiteralPath "$sourcePath/$name" -Raw|ConvertFrom-Json
 foreach($f in $j.files){if((Hash $f.path) -ne $f.sha256 -or (Get-Item -LiteralPath $f.path).Length -ne $f.bytes){throw "binding $($f.path)"}}
 $checks+=@{name=$name;sha256=(Hash "$sourcePath/$name");verified=$j.files.Count};Write-Output "$name verified=$($j.files.Count)"
}
$original=Get-Content "$sourcePath/final-summary.json" -Raw|ConvertFrom-Json
if((Hash "$sourcePath/artifact-ledger.json") -ne $original.artifact_ledger_sha256){throw 'ledger root'}
Save 'bindings.json' @{checks=$checks;original_summary_sha256=(Hash "$sourcePath/final-summary.json")}
$hook=@'
        // A newer complete snapshot with no current Note must withdraw a
        // pending Down immediately. Missing grace applies only after a
        // contact has started; it cannot license a new touch from old pixels.
        if(identity.submitted) if(const auto cursor=scheduler_.executed_steps(identity.intent);
           cursor&&*cursor==0) {
            cancel_contact(id,identity,"pending_down_current_object_missing");
            continue;
        }
'@
$hook=$hook.Replace("`r`n","`n")+"`n";$count=0
foreach($f in Get-ChildItem "$repoPath/out/x11-p-r1/B0/src","$repoPath/out/x11-p-r1/B0/include" -File -Recurse){
 $other=$f.FullName.Replace('\B0\','\B1\');$a=[IO.File]::ReadAllText($f.FullName).Replace("`r`n","`n");$b=[IO.File]::ReadAllText($other).Replace("`r`n","`n")
 if($f.Name -eq 'game.cpp'){if(!$b.Contains($hook)){throw 'missing hook'};$b=$b.Replace($hook,'')}
 if($a -cne $b){throw "source inverse $other"};++$count
}
Save 'source-inverse.json' @{files=$count;only_decision_delta='pending cursor zero missing hook'}
foreach($d in Get-ChildItem -LiteralPath $sourcePath -Directory|Where-Object Name -Like 'aa-*'){
 New-Item -ItemType Directory -Path "$reviewPath/$($d.Name)"|Out-Null
 Copy-Item -LiteralPath "$($d.FullName)/summary.json" -Destination "$reviewPath/$($d.Name)/summary.json"
}
Run 'noise' "$repoPath/out/x11-p-r1/build-B0/Release/x11_gate.exe" @('noise',$reviewPath) 2
if((Hash "$reviewPath/noise-frozen.json") -ne (Hash "$sourcePath/noise-frozen.json")){throw 'gate mismatch'}
. "$repoPath/tools/collect-runtime-x11-p-r1-tests.ps1"
$hist=Get-R1TestResult "$campaignPath/runtime-x11-p/tests-B0-original.xml"
if($hist.skipped -ne 1 -or $hist.passed -ne 237){throw 'historical skip collector'}
Save 'historical-collector.json' $hist
$env:PAS_RGB_CLIP_ROOT="$campaignPath/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
$testResults=@()
foreach($role in @('B0','B1','asan')){
 $build=if($role -eq 'asan'){"$repoPath/out/x11-p-r1/build-asan/Debug"}else{"$repoPath/out/x11-p-r1/build-$role/Release"}
 $env:R1_TEST_ROOT="$reviewPath/test-artifacts-$role"
 $suites=if($role -eq 'asan'){@('pending','r1')}else{@('original','pending','r1')}
 foreach($suite in $suites){
  $exe=switch($suite){'original'{'pas_tests.exe'}'pending'{'x11_pending_tests.exe'}'r1'{'x11_r1_tests.exe'}}
  $expected=if(($role -eq 'B1' -and $suite -eq 'original') -or ($role -eq 'B0' -and $suite -eq 'pending')){1}else{0}
  $name="tests-$role-$suite";Run $name "$build/$exe" @('--gtest_brief=1',"--gtest_output=xml:$reviewPath/$name.xml") $expected
  $result=Get-R1TestResult "$reviewPath/$name.xml";$old=Get-R1TestResult "$sourcePath/$name.xml"
  if($result.tests -ne $old.tests -or ($result.failed_names -join '|') -cne ($old.failed_names -join '|') -or $result.skipped -or $result.errors -or $result.disabled){throw "unexpected tests $name"}
  $testResults+=@{name=$name;result=$result}
 }
 if($role -ne 'asan'){
  Run "provenance-$role" "$repoPath/out/x11-p-r1/runtime-$role/pas.exe" @('x11-provenance') 0
  $prov=Get-Content "$reviewPath/provenance-$role.log" -Raw|ConvertFrom-Json
  foreach($p in $prov.compiled_source_sha256.PSObject.Properties){if((Hash "$repoPath/out/x11-p-r1/$role/$($p.Name)") -ne $p.Value){throw 'provenance'}}
  Run "bridge-$role" "$build/x11_cost.exe" @('bridge',"$reviewPath/bridge-$role","$campaignPath/runtime-x11-p/bridge-$role/public-events.jsonl") 0
  if((Hash "$reviewPath/bridge-$role/summary.json") -ne (Hash "$sourcePath/bridge-$role/summary.json")){throw 'bridge mismatch'}
 }
}
Run 'release-negatives' "$repoPath/out/x11-p-r1/negative/Release/r1_negative.exe" @("$reviewPath/release-negatives") 0
foreach($f in Get-ChildItem "$sourcePath/release-negatives" -File){if((Hash "$reviewPath/release-negatives/$($f.Name)") -ne (Hash $f.FullName)){throw 'release negative mismatch'}}
Save 'verification.json' @{tests=$testResults;noise_byte_equal=$true;bridge_summaries_byte_equal=$true;release_negatives_byte_equal=$true;source_inverse_files=$count;cost_runs_added=0;stress_runs_added=0;full_replays_added=0;state='not-ready';script_sha256=(Hash $PSCommandPath)}
Copy-Item -LiteralPath $PSCommandPath -Destination "$reviewPath/accept-runtime-x11-p-r1.ps1"
Write-Output "verification complete; bytes=$(Bytes $reviewPath); awaiting controller report/final ledger"
