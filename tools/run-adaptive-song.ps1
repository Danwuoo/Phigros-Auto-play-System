[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BuildArchive,
    [Parameter(Mandatory=$true)][string]$Chart,
    [Parameter(Mandatory=$true)][string]$SongProfile,
    [Parameter(Mandatory=$true)][string]$StateCalibration,
    [Parameter(Mandatory=$true)][string]$HudReference,
    [Parameter(Mandatory=$true)][string]$KernelProfile,
    [string]$CapacityEvidence,
    [ValidateRange(-100,100)][int]$PhaseMs = -80,
    [ValidateSet('none','result','diagnostic')][string]$ImageRetention = 'none',
    [string]$OutputDirectory,
    [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
$BuildArchive = (Resolve-Path -LiteralPath $BuildArchive).Path
$archiveManifest = Get-Content -LiteralPath (Join-Path $BuildArchive 'manifest.json') -Raw | ConvertFrom-Json
if ($archiveManifest.schema -ne 'pas.adaptive-build-archive.v1') { throw 'Unsupported build archive.' }
$verifiedExecutable = $false
foreach ($artifact in $archiveManifest.artifacts) {
    if (-not $artifact.file.StartsWith('bin/')) { continue }
    $path = [IO.Path]::GetFullPath((Join-Path $BuildArchive $artifact.file))
    $binRoot = [IO.Path]::GetFullPath((Join-Path $BuildArchive 'bin')) + [IO.Path]::DirectorySeparatorChar
    if (-not $path.StartsWith($binRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Archive binary path escapes bin.' }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $artifact.sha256) {
        throw "Archived binary hash mismatch: $path"
    }
    if ($artifact.file -eq 'bin/chart_experiment.exe') { $verifiedExecutable = $true }
}
if (-not $verifiedExecutable) { throw 'Archive does not pin chart_experiment.exe.' }
$Chart = (Resolve-Path -LiteralPath $Chart).Path
$SongProfile = (Resolve-Path -LiteralPath $SongProfile).Path
$StateCalibration = (Resolve-Path -LiteralPath $StateCalibration).Path
$HudReference = (Resolve-Path -LiteralPath $HudReference).Path
$KernelProfile = (Resolve-Path -LiteralPath $KernelProfile).Path
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path (Split-Path -Parent $SongProfile) ('live-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Output directory must be new.' }
$mode = if ($CheckOnly) { 'check-adaptive' } else { 'run-adaptive' }
$executable = Join-Path $BuildArchive 'bin/chart_experiment.exe'
if (-not $CheckOnly) {
    Write-Host 'One experimental round. Select the exact profile song/difficulty manually; press Play only after READY.'
    Write-Host 'Esc/Ctrl+C or an output-directory STOP file cancels. Revalidate the kernel profile after each emulator boot.'
}
$runArguments = @($mode, 'emulator-5554', 'phigros', $Chart, $StateCalibration, $KernelProfile, $OutputDirectory, $PhaseMs,
    '--experimental-whole-song', '--song-profile', $SongProfile, '--experimental-hud-reference', $HudReference)
if ($CapacityEvidence) {
    $runArguments += @('--capacity-evidence', (Resolve-Path -LiteralPath $CapacityEvidence).Path)
}
if ($ImageRetention -ne 'none') { $runArguments += @('--image-retention', $ImageRetention) }
& $executable @runArguments
exit $LASTEXITCODE
