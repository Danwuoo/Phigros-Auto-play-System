param([Parameter(Mandatory)][string]$GateReceipt,[Parameter(Mandatory)][string]$ToolSnapshot)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$source=Join-Path $PSScriptRoot 'functional-gui-source';$root=Join-Path $repo 'out/functional-gui-build-03'
$inputs=@($PSCommandPath)+@(Get-ChildItem -LiteralPath $source -File|ForEach-Object FullName)
$inputs+=@(Get-ChildItem -LiteralPath 'C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed/x64-windows/include/nlohmann' -File -Recurse|ForEach-Object FullName)
foreach($operation in @('Configure','Build')){
 $arguments=@{Operation=$operation;SourceRoot=$source;BuildRoot=$root;StageName=($operation.ToLowerInvariant()+'-functional-gui-03');GateReceipt=$GateReceipt;ToolSnapshot=$ToolSnapshot;Inputs=$inputs}
 if($operation -eq 'Configure'){$arguments.Options=@('-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake','-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed','-DVCPKG_MANIFEST_INSTALL=OFF','-DVCPKG_TARGET_TRIPLET=x64-windows')}
 & (Join-Path $PSScriptRoot 'cmake-gui-stage.ps1') @arguments
 if($LASTEXITCODE -ne 0){throw ('functional-gui-'+$operation+'-STOP')}
}
