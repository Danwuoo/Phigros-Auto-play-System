param([ValidateSet('release','debug','asan')][string]$Mode)
$ErrorActionPreference='Stop'
$taskRepo='C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006'
$pwsh='C:\Users\wurre\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe'
$root=Join-Path $taskRepo ('out/bridge-win-withdraw-'+$Mode+'-01')
$gate=Join-Path $taskRepo 'out/windows-handoff/native-qualification-01/native-gate.json'
$tool=Join-Path $taskRepo 'out/windows-handoff/msvc-isolated-probe-01/tool-snapshot.json'
$prior=Get-Content -LiteralPath (Join-Path $taskRepo ('out/windows-handoff/build-bridge-'+$(if($Mode -eq 'release'){'release-08'}elseif($Mode -eq 'debug'){'debug-05'}else{'asan-05'})+'-inputs/spec.json')) -Raw|ConvertFrom-Json
$closure=@(Get-ChildItem -LiteralPath (Join-Path $taskRepo 'research/bvi_windows/bridge') -File|Where-Object Extension -in '.cpp','.hpp','.txt'|ForEach-Object FullName)
$inputs=@($prior.inputs|Where-Object {$_ -notmatch '-inputs\\.*\.cmd$'}|Sort-Object -Unique)+$closure+@($PSCommandPath,(Join-Path $PSScriptRoot 'stage.ps1'))
$configuration=if($Mode -eq 'release'){'Release'}else{'Debug'}
$options=@('-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake',
 '-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed',
 '-DVCPKG_MANIFEST_INSTALL=OFF','-DVCPKG_TARGET_TRIPLET=x64-windows')
if($Mode -eq 'asan'){$options+=@('-DPAS_BRIDGE_ASAN=ON','-DPAS_ASAN_SUPPORT_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')}
foreach($operation in @('Configure','Build')) {
 $arguments=@{SourceRoot=(Join-Path $taskRepo 'research/bvi_windows/bridge');BuildRoot=$root;
 StageName=($operation.ToLowerInvariant()+'-bridge-withdraw-'+$Mode+'-01');ToolSnapshot=$tool;
 GateReceipt=$gate;Operation=$operation;Configuration=$configuration;Inputs=$inputs;TotalSeconds=900}
 if($operation -eq 'Configure'){$arguments.Options=$options}
 $json=Join-Path $PSScriptRoot ($Mode+'-'+$operation.ToLowerInvariant()+'.json')
 $bytes=[Text.Encoding]::UTF8.GetBytes((@{script=(Join-Path $taskRepo 'tools/zero_miss_windows/cmake-stage.ps1');arguments=$arguments}|ConvertTo-Json -Depth 12))
 $stream=[IO.FileStream]::new($json,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
 try{$stream.Write($bytes);$stream.Flush($true)}finally{$stream.Dispose()}
 & $pwsh -NoProfile -File (Join-Path $PSScriptRoot 'stage.ps1') -Parameters $json
 if($LASTEXITCODE -ne 0){throw ($operation+'-'+$Mode+'-exit-'+$LASTEXITCODE)}
}
