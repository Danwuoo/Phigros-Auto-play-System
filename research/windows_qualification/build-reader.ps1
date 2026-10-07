param([Parameter(Mandatory)][string]$GateReceipt,[Parameter(Mandatory)][string]$ToolSnapshot,
 [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$Attempt='reader01')
$ErrorActionPreference='Stop'
$taskQualificationRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$inputs=@(Get-ChildItem -LiteralPath $PSScriptRoot -File|Where-Object {$_.Extension -in @('.cpp','.hpp') -or $_.Name -eq 'CMakeLists.txt'}|ForEach-Object FullName)
$inputs+=@($PSCommandPath,(Join-Path $taskQualificationRepo 'research/prelive_current/evidence_hash.hpp'),(Join-Path $taskQualificationRepo 'research/prelive_current/input_closure.hpp'),
 (Join-Path $taskQualificationRepo 'research/prelive_current/cost_workload.hpp'),(Join-Path $taskQualificationRepo 'research/prelive_current/cost_workload_family_v2.hpp'))
$inputs+=@(Get-ChildItem -LiteralPath 'C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed/x64-windows/include/nlohmann' -File -Recurse|ForEach-Object FullName)
foreach($operation in @('Configure','Build')){
 $arguments=@{SourceRoot=$PSScriptRoot;BuildRoot=(Join-Path $taskQualificationRepo ('out/windows-qualification-release-'+$Attempt));
  StageName=($operation.ToLowerInvariant()+'-qualification-release-'+$Attempt);ToolSnapshot=$ToolSnapshot;GateReceipt=$GateReceipt;
  Operation=$operation;Configuration='Release';Inputs=$inputs}
 if($operation -eq 'Configure'){$arguments.Options=@('-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake',
  '-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed','-DVCPKG_MANIFEST_INSTALL=OFF','-DVCPKG_TARGET_TRIPLET=x64-windows')}
 & (Join-Path $taskQualificationRepo 'tools/zero_miss_windows/cmake-stage.ps1') @arguments
 if($LASTEXITCODE -ne 0){throw ('reader-build-stage '+$operation)}
}
