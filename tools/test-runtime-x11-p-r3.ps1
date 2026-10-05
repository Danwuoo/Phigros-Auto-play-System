param([switch]$Final)
. "$PSScriptRoot/r3-common.ps1"
. "$repoPath/tools/collect-runtime-x11-p-r1-tests.ps1"
if(Test-Path "$batchPath/source-binding-before-cost.json"){throw 'tests must use separate controller root after freeze'}
$tests=@()
$prefix=if($Final){'final-'}else{''};$expectedCount=if($Final){10}else{9}
foreach($role in @('B0','B1','asan')) {
 $config=if($role -eq 'asan'){'Debug'}else{'Release'}
 $env:R3_TEST_ROOT="$batchPath/tests-${prefix}fixtures-$role"
 R3Run "tests-$prefix$role" "$outPath/build-$role/$config/r3_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/tests-$prefix$role.xml") | Out-Null
 $t=Get-R1TestResult "$batchPath/tests-$prefix$role.xml";if($t.tests -ne $expectedCount -or $t.failures -or $t.errors -or $t.disabled -or $t.skipped){throw 'R3 tests'};$tests+=@{role=$role;result=$t;instrumented=($role -eq 'asan')}
 if($role -ne 'asan') {
  R3Run "bridge-$prefix$role" "$outPath/build-$role/Release/r3_meter.exe" @('bridge',"$batchPath/bridge-$prefix$role","$campaignPath/runtime-x11-p/bridge-$role/public-events.jsonl") | Out-Null
  $a=Get-Content "$batchPath/bridge-$prefix$role/summary.json" -Raw|ConvertFrom-Json
  $b=Get-Content "$campaignPath/runtime-x11-p-r2/bridge-$role/summary.json" -Raw|ConvertFrom-Json
  if($a.comparison.bytes -ne $b.comparison.bytes -or $a.comparison.rows -ne $b.comparison.rows -or !$a.comparison.equal -or $a.receipts -ne $b.receipts -or $a.stimulus_sha256 -ne $b.stimulus_sha256){throw 'bridge'}
 }
}
$historical=Get-Content "$campaignPath/runtime-x11-p-r2/tests-collected.json" -Raw|ConvertFrom-Json
$verified=@();foreach($s in $historical.suites){$t=Get-R1TestResult "$campaignPath/runtime-x11-p-r2/$($s.name).xml";if($t.sha256 -ne $s.result.sha256){throw 'historical XML'};$verified+=@{name=$s.name;result=$t;rerun=$false}}
R3Save "tests-${prefix}collected.json" @{new_suites=$tests;historical_XML_verified_only=$verified;new_tests_per_suite=$expectedCount;new_stress=0;R2_active_reference="$campaignPath/runtime-x11-p-r2/active-raw-analysis.json";R2_active_sha256=(R3Hash "$campaignPath/runtime-x11-p-r2/active-raw-analysis.json");ASan='new collector/archive object/tests and frozen instrumented B1 core; no OS concurrency rerun; third party not comprehensive'}
R3Capacity | ConvertTo-Json
