$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue"
$sourcePath="$campaignPath/runtime-x11-p-r2";$reviewPath="$campaignPath/runtime-x11-p-r2-controller"
function Hash($p){(Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$v){if(Test-Path -LiteralPath "$reviewPath/$name"){throw 'existing output'};[IO.File]::WriteAllText("$reviewPath/$name",($v|ConvertTo-Json -Depth 70)+"`n",[Text.UTF8Encoding]::new($false))}
function Run($name,$exe,[string[]]$arguments,[int]$expected=0){Save "$name-command.json" @{exe=$exe;arguments=$arguments;expected_exit=$expected};& $exe @arguments *> "$reviewPath/$name.log";$code=$LASTEXITCODE;Save "$name-exit.json" @{exit=$code;log_sha256=(Hash "$reviewPath/$name.log")};if($code -ne $expected){throw "$name unexpected exit=$code"};Write-Output "$name exit=$code"}
if(Test-Path -LiteralPath $reviewPath){throw 'new root required'}
if((Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")+16MB -gt 8GB){throw 'reserve'}
New-Item -ItemType Directory $reviewPath|Out-Null
Save 'workspace-before.json' @{head=(git rev-parse HEAD);branch=(git branch --show-current);worktrees=@(git worktree list);status=@(git status --short);formal_diff=@(git diff HEAD -- src include CMakeLists.txt);batch_limit=16MB;new_cost_or_stress_runs=0}
$checks=@()
foreach($batch in @('runtime-x11-p-r1','runtime-x11-p-r2')){
 $names=if($batch -like '*r1'){@('source-binding-before-cost.json','compiled-dependency-freeze.json','negative-tool-freeze.json','artifact-ledger.json')}else{@('source-binding-before-stress.json','compiled-dependency-freeze.json','artifact-ledger.json')}
 foreach($name in $names){$path="$campaignPath/$batch/$name";$j=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json
  foreach($f in $j.files){if((Hash $f.path) -ne $f.sha256 -or (Get-Item -LiteralPath $f.path).Length -ne $f.bytes){throw "binding $($f.path)"}}
  $checks+=@{batch=$batch;name=$name;entries=$j.files.Count;sha256=(Hash $path)};Write-Output "$batch/$name verified=$($j.files.Count)"
 }
}
$final=Get-Content "$sourcePath/final-summary.json" -Raw|ConvertFrom-Json
if((Hash "$sourcePath/artifact-ledger.json") -ne $final.artifact_ledger_sha256){throw 'ledger root'}
Save 'bindings.json' @{checks=$checks;final_summary_sha256=(Hash "$sourcePath/final-summary.json")}
# R2's ledger binds two mutable authoritative docs. Preserve exact bound preimages before controller updates.
foreach($n in @('PROJECT_STATUS_NEXT_STEPS_20261001.md','C36H_FORWARD_EXECUTION_PLAN_20261003.md')){Copy-Item -LiteralPath "$repoPath/docs/$n" -Destination "$reviewPath/pre-review-$n"}
. "$repoPath/tools/collect-runtime-x11-p-r1-tests.ps1"
$tests=@()
foreach($role in @('B0','B1','asan')){
 $cfg=if($role -eq 'asan'){'Debug'}else{'Release'}
 Run "tests-$role-r2" "$repoPath/out/x11-p-r2/build-$role/$cfg/r2_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$reviewPath/tests-$role-r2.xml")
 $t=Get-R1TestResult "$reviewPath/tests-$role-r2.xml";if($t.tests -ne 7 -or $t.failures -or $t.errors -or $t.disabled -or $t.skipped){throw 'new test result'};$tests+=@{role=$role;result=$t}
 if($role -ne 'asan'){
  Run "bridge-$role" "$repoPath/out/x11-p-r2/build-$role/Release/r2_active_meter.exe" @('bridge',"$reviewPath/bridge-$role","$campaignPath/runtime-x11-p/bridge-$role/public-events.jsonl")
  if((Hash "$reviewPath/bridge-$role/summary.json") -ne (Hash "$sourcePath/bridge-$role/summary.json")){throw 'bridge mismatch'}
 }
}
$exe="$repoPath/out/x11-p-r2/raw-audit/Release/r2_raw_audit.exe"
Run 'existing-raw-analysis' $exe @("$campaignPath/runtime-x11-p-r1","$reviewPath/existing-raw-analysis.json")
Run 'active-raw-analysis' $exe @($sourcePath,"$reviewPath/active-raw-analysis.json",'active')
if((Hash "$reviewPath/existing-raw-analysis.json") -ne (Hash "$sourcePath/existing-raw-analysis-final.json")){throw 'existing raw report mismatch'}
if((Hash "$reviewPath/active-raw-analysis.json") -ne (Hash "$sourcePath/active-raw-analysis.json")){throw 'active raw report mismatch'}
Run 'reject-normal' "$repoPath/out/x11-p-r2/build-B0/Release/r2_active_meter.exe" @('cost','owner',"$reviewPath/must-not-create",'normal') 1
if(Test-Path -LiteralPath "$reviewPath/must-not-create"){throw 'normal rejection wrote run directory'}
$collected=Get-Content "$sourcePath/tests-collected.json" -Raw|ConvertFrom-Json
$historical=@();foreach($suite in $collected.suites){$t=Get-R1TestResult "$sourcePath/$($suite.name).xml";if($t.sha256 -ne $suite.result.sha256){throw 'XML SHA'};$historical+=@{name=$suite.name;result=$t}}
Save 'verification.json' @{new_tests_rerun=$tests;development_XML_verified_only=$historical;bridge_summaries_byte_equal=$true;existing_12_raw_report_byte_equal=$true;active_3_raw_report_byte_equal=$true;normal_rejected_before_output=$true;new_cost_runs=0;new_stress_runs=0;new_full_replays=0;state='not-ready'}
Copy-Item -LiteralPath $PSCommandPath -Destination "$reviewPath/accept-runtime-x11-p-r2.ps1"
Write-Output "verification completed; bytes=$(Bytes $reviewPath)"
