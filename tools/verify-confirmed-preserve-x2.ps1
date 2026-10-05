param([switch]$ResumeChecks)
. "$PSScriptRoot/x2-budget.ps1"
if(!$ResumeChecks) {
$build=Join-Path $script:X2Repo 'out/x2/build50'
$code=X2-Invoke 'build-report' 'cmake' @('--build',$build,'--config','Release','--target','x2_report','--parallel','2')
if($code){throw 'Report build failed'}
$dest=Join-Path $script:X2Repo 'out/x2/tools50/x2_report.exe';if(Test-Path -LiteralPath $dest){throw 'Report binary freeze must be new'}
Copy-Item -LiteralPath "$build/Release/x2_report.exe" -Destination $dest
$code=X2-Invoke 'causal-report' $dest @((Join-Path $script:X2Batch 'input-manifest.json'),(Join-Path $script:X2Batch 'three-role-comparison.json'),(Join-Path $script:X2Campaign 'contact-replay-x1'),(Join-Path $script:X2Batch 'causal-chain-report.json')) 2097152
if($code){throw 'Canonical/causal report verification failed'}
}
./tools/check-confirmed-preserve-x2.ps1
foreach($role in '36','50') {
  $debug=Join-Path $script:X2Repo "out/x2/build$role/Debug"
  $code=X2-Invoke "build$role-asan" 'cmake' @('--build',"$script:X2Repo/out/x2/build$role",'--config','Debug','--target','x1_tests','--parallel','2')
  if($code){throw 'ASan build failed'}
  Copy-Item -LiteralPath "$script:X2Repo/out/vcpkg_installed/x64-windows/debug/bin/zd.dll","$script:X2Repo/out/vcpkg_installed/x64-windows/debug/bin/gtest.dll","$script:X2Repo/out/vcpkg_installed/x64-windows/debug/bin/gtest_main.dll" -Destination $debug
  $env:PAS_RGB_CLIP_ROOT="$script:X2Repo/measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
  $code=X2-Invoke "tests$role-asan-control" "$debug/x1_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$script:X2Batch/tests$role-asan-control.xml") 1048576
  if($code){throw 'ASan control regression failed'}
  if($role -eq '50') {
    $env:PAS_X2_NO_WINNER_OVERRIDE='1'
    $code=X2-Invoke 'tests50-asan-variant' "$debug/x1_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$script:X2Batch/tests50-asan-variant.xml") 1048576
    Remove-Item Env:PAS_X2_NO_WINNER_OVERRIDE
    if($code -ne 1){throw 'Expected preservation-only variant failure missing or ASan crashed'}
  }
}
Write-Output 'Reports, CLI source contracts and new Debug/ASan suites checked.'
$global:LASTEXITCODE=0
