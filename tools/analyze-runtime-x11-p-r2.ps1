. "$PSScriptRoot/r2-common.ps1"
$buildPath="$outPath/raw-audit"
R2Run 'configure-raw-audit' cmake (R2ConfigureArgs "$repoPath/tools/runtime_x11_p_r2_audit" $buildPath)
R2Run 'build-raw-audit' cmake @('--build',$buildPath,'--config','Release','--parallel','2')
R2Run 'raw-audit' "$buildPath/Release/r2_raw_audit.exe" @("$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-r1","$batchPath/existing-raw-analysis.json")
R2Capacity | ConvertTo-Json
