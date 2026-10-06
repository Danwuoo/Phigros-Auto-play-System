param([ValidateSet('release','debug','asan')][string]$Mode)
$ErrorActionPreference='Stop'
$taskRepo='C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006'
$pwsh='C:\Users\wurre\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe'
$root=Join-Path $taskRepo ('out/bridge-win-review-'+$Mode+'-01')
$buildStage='build-bridge-review-'+$Mode+'-01'
$buildReceipt=Join-Path $taskRepo ('out/windows-handoff/'+$buildStage+'/'+$buildStage+'-verification.json')
$verified=Get-Content -LiteralPath $buildReceipt -Raw|ConvertFrom-Json
if($verified.pending -or $verified.native_exit -ne 0 -or $verified.runner_exit -ne 0 -or $verified.verification_exit -ne 0){throw 'build-not-verified'}
$freeze=Join-Path $taskRepo ('out/windows-handoff/'+$buildStage+'/freeze.json')
$frozen=Get-Content -LiteralPath $freeze -Raw|ConvertFrom-Json
$inputs=@($frozen.files.path)+@($PSCommandPath,$buildReceipt,$freeze)+@(Get-ChildItem -LiteralPath $root -File|Where-Object Extension -in '.exe','.lib','.dll'|ForEach-Object FullName)
$report=Join-Path $taskRepo ('out/windows-handoff/prefix-review-'+$Mode+'-01.json')
$arguments=@{StageName=('prefix-bridge-review-'+$Mode+'-01');Executable=(Join-Path $root 'bridge_tests.exe');
 Arguments=@($report);GateReceipt=(Join-Path $taskRepo 'out/windows-handoff/native-qualification-01/native-gate.json');Inputs=$inputs;TotalSeconds=300}
$json=Join-Path $PSScriptRoot ($Mode+'-prefix.json')
$data=@{script=(Join-Path $taskRepo 'tools/zero_miss_windows/native-stage.ps1');arguments=$arguments}|ConvertTo-Json -Depth 12
$stream=[IO.FileStream]::new($json,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try{$bytes=[Text.Encoding]::UTF8.GetBytes($data);$stream.Write($bytes);$stream.Flush($true)}finally{$stream.Dispose()}
& $pwsh -NoProfile -File (Join-Path $PSScriptRoot 'stage.ps1') -Parameters $json
if($LASTEXITCODE -ne 0){throw ('prefix-'+$Mode+'-exit-'+$LASTEXITCODE)}
$result=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
if($result.assertions -ne 113 -or $result.failed_assertions -ne 0){throw 'prefix-count-or-failure'}
$result|Select-Object assertions,failed_assertions,device_commands|ConvertTo-Json -Compress
