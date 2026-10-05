. "$PSScriptRoot/r2-common.ps1"
. "$repoPath/tools/collect-runtime-x11-p-r1-tests.ps1"
$results=@()
foreach($role in @('B0','B1','asan')){
 $config=if($role -eq 'asan'){'Debug'}else{'Release'}
 R2Run "tests-$role-r2" "$outPath/build-$role/$config/r2_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/tests-$role-r2.xml")
 $test=Get-R1TestResult "$batchPath/tests-$role-r2.xml";if($test.tests -ne 7 -or $test.failures -or $test.skipped -or $test.errors -or $test.disabled){throw 'R2 suite'};$results+=@{name="tests-$role-r2";result=$test}
 $oldBuild="$repoPath/out/x11-p-r1/build-$role/$config"
 $env:R1_TEST_ROOT="$batchPath/test-artifacts-$role"
 foreach($suite in @('pending','r1')){
  $exe=if($suite -eq 'pending'){'x11_pending_tests.exe'}else{'x11_r1_tests.exe'};$expected=if($role -eq 'B0' -and $suite -eq 'pending'){1}else{0}
  R2Run "tests-$role-$suite" "$oldBuild/$exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/tests-$role-$suite.xml") $expected
  $test=Get-R1TestResult "$batchPath/tests-$role-$suite.xml";$old=Get-R1TestResult "$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-r1/tests-$role-$suite.xml"
  if($test.tests -ne $old.tests -or ($test.failed_names -join '|') -cne ($old.failed_names -join '|') -or $test.skipped -or $test.errors -or $test.disabled){throw 'old suite mismatch'};$results+=@{name="tests-$role-$suite";result=$test}
 }
}
$env:PAS_RGB_CLIP_ROOT="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
foreach($role in @('B0','B1')){
 $expected=if($role -eq 'B1'){1}else{0}
 R2Run "tests-$role-original" "$repoPath/out/x11-p-r1/build-$role/Release/pas_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/tests-$role-original.xml") $expected
 $test=Get-R1TestResult "$batchPath/tests-$role-original.xml";$old=Get-R1TestResult "$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-r1/tests-$role-original.xml"
 if($test.tests -ne 238 -or ($test.failed_names -join '|') -cne ($old.failed_names -join '|') -or $test.skipped -or $test.errors -or $test.disabled){throw 'original suite mismatch'};$results+=@{name="tests-$role-original";result=$test}
 R2Run "bridge-$role" "$outPath/build-$role/Release/r2_active_meter.exe" @('bridge',"$batchPath/bridge-$role","$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p/bridge-$role/public-events.jsonl")
 if((R2Hash "$batchPath/bridge-$role/summary.json") -ne (R2Hash "$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-r1/bridge-$role/summary.json")){throw 'streaming bridge changed'}
 R2Run "provenance-$role" "$repoPath/out/x11-p-r1/runtime-$role/pas.exe" @('x11-provenance')
}
R2Save 'tests-collected.json' @{suites=$results;R2_new_tests=7;initial_B0_tests='separately preserved before final scripted run';ASan_scope='new R2 meter/tests plus existing frozen R1 self-authored core; third-party not fully instrumented; no real PNG ASan suite';bridge='512-input public output streamed against old events; no 30MB duplicate';normal_cost_runs=0}
R2Capacity | ConvertTo-Json
