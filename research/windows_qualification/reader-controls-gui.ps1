param([Parameter(Mandatory)][string]$GateReceipt)
$ErrorActionPreference='Stop';$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $repo 'out/prelive-20261006/current-build-release-win04.json'
$sha=(Get-FileHash -LiteralPath $build).Hash.ToLowerInvariant();$commit='cd21144440b1d02c11eafb3f332e63d20c0079cb'
$stage=Join-Path $PSScriptRoot 'functional-gui-stage.ps1'
& $stage -StageName gui-nonzero-control-02 -Executable (Get-Command pwsh).Source -Arguments @('-NoProfile','-Command','exit 7') -GateReceipt $GateReceipt -ExpectedNative 7 -Inputs @($PSCommandPath)
if($LASTEXITCODE -ne 0){throw 'GUI-nonzero-control-STOP'}
& $stage -StageName reader-controls-02 -Executable (Join-Path $repo 'out/windows-qualification-release-reader03/pas_qualification_tests.exe') -Arguments @($build,$sha,$commit,(Join-Path $PSScriptRoot 'reader-controls-02')) -GateReceipt $GateReceipt -Inputs @($PSCommandPath,$build)
if($LASTEXITCODE -ne 0){throw 'reader-controls-STOP'}
