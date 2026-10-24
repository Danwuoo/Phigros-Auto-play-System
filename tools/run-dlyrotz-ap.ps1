[CmdletBinding()]
param(
    [switch]$CheckOnly,
    [string]$OutputDirectory,
    [string]$KernelProfile
)
$ErrorActionPreference = 'Stop'
$assetRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../local-assets/phigros-4.0.1'))
$buildRoot = Join-Path $assetRoot 'm7-whole-song-build-009'
function Assert-FileHash([string]$Path, [string]$Expected) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing local artifact: $Path" }
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Expected) { throw "Artifact hash mismatch: $Path" }
}
$buildManifest = Join-Path $buildRoot 'manifest.json'
Assert-FileHash $buildManifest '6b83fd5f2c5daace2078dd0b239a78d5b4877b257547020b399762e0709a599f'
$archive = Get-Content -LiteralPath $buildManifest -Raw | ConvertFrom-Json
foreach ($artifact in $archive.artifacts) {
    if ($artifact.file.StartsWith('bin/')) {
        Assert-FileHash (Join-Path $buildRoot $artifact.file) $artifact.sha256
    }
}
$executable = Join-Path $buildRoot 'bin/chart_experiment.exe'
Assert-FileHash $executable '2d080e1234c4ec105087f092b4785409d6befd2e999aa9ff1ad40d63a6435ad7'
$chart = Join-Path $assetRoot 'dlyrotz-in13-verified/Chart_IN.json'
$state = Join-Path $assetRoot 'state-calibration-001/dataset-with-paused-heldout.json'
$hud = Join-Path $assetRoot 'm7-hud-glyph-reference-001/clean-reference.rgb'
Assert-FileHash $chart 'aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90'
Assert-FileHash $state 'd6b0a2735f556159779347c134598d2b831bed3ba5eb181a31bc0123214538e1'
Assert-FileHash $hud '397431f2749bb38a845c6845977fc5c37cb2de51a79ff273dd3417ef2756e09a'
if (-not $KernelProfile) {
    $KernelProfile = Join-Path $assetRoot 'full-song-kernel-profile-002.json'
    Assert-FileHash $KernelProfile 'e4fbdc74f925d6dec6933fdf0d54f4650b364c2a6057fa883b79234a73e323da'
}
$KernelProfile = (Resolve-Path -LiteralPath $KernelProfile).Path
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $assetRoot ('m7-ap-replay-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw "Output directory must be new: $OutputDirectory" }
Write-Host 'Verified AP baseline: build009, 584 notes, 5669 events, phase -80 ms.'
Write-Host "Output: $OutputDirectory"
if ($CheckOnly) {
    Write-Host 'Offline artifact check complete. No device discovery or input performed.'
    return
}
Write-Host 'Keep Phigros 4.0.1 on the Dlyrotz IN13 selection/result screen. Press Start/Retry only after READY.'
Write-Host 'One round only. Esc/Ctrl+C or an output-directory STOP file cancels. A new emulator boot requires a revalidated kernel profile.'
& $executable run emulator-5554 phigros $chart $state $KernelProfile $OutputDirectory -80 --experimental-whole-song --experimental-hud-reference $hud
exit $LASTEXITCODE
