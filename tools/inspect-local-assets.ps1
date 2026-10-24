# Read-only inventory. This does not execute SDK tools, validate hashes, or prove bootability.
[CmdletBinding()]
param(
    [string]$SdkRoot,
    [string]$AvdRoot,
    [string]$LegacyRepositoryRoot,
    [string[]]$RecordingRoot = @()
)
$ErrorActionPreference = 'Stop'
if (-not $SdkRoot) { $SdkRoot = $env:ANDROID_SDK_ROOT }
if (-not $SdkRoot) { $SdkRoot = $env:ANDROID_HOME }
if (-not $SdkRoot) { $SdkRoot = Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
if (-not $AvdRoot) { $AvdRoot = $env:ANDROID_AVD_HOME }
if (-not $AvdRoot) { $AvdRoot = Join-Path $env:USERPROFILE '.android\avd' }
$SdkRoot = [IO.Path]::GetFullPath($SdkRoot)
$AvdRoot = [IO.Path]::GetFullPath($AvdRoot)
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Read-Ini([string]$Path) {
    $values = @{}
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        foreach ($line in Get-Content -LiteralPath $Path) {
            if ($line -match '^\s*([^#;=]+?)\s*=\s*(.*?)\s*$') {
                $values[$matches[1]] = $matches[2]
            }
        }
    }
    return $values
}
function Inspect-Path([string]$Path) {
    [ordered]@{ path = $Path; exists = (Test-Path -LiteralPath $Path) }
}
$toolPaths = @('platform-tools\adb.exe', 'emulator\emulator.exe')
$commandLineRoot = Join-Path $SdkRoot 'cmdline-tools'
if (Test-Path -LiteralPath $commandLineRoot -PathType Container) {
    foreach ($directory in Get-ChildItem -LiteralPath $commandLineRoot -Directory) {
        $toolPaths += 'cmdline-tools\' + $directory.Name + '\bin\avdmanager.bat'
        $toolPaths += 'cmdline-tools\' + $directory.Name + '\bin\sdkmanager.bat'
    }
} else {
    $toolPaths += 'cmdline-tools\latest\bin\avdmanager.bat', 'cmdline-tools\latest\bin\sdkmanager.bat'
}
$tools = @(foreach ($relative in $toolPaths) { Inspect-Path (Join-Path $SdkRoot $relative) })
$avds = @(if (Test-Path -LiteralPath $AvdRoot -PathType Container) {
    foreach ($directory in Get-ChildItem -LiteralPath $AvdRoot -Directory -Filter '*.avd') {
        $avdName = $directory.Name -replace '\.avd$', ''
        $configPath = Join-Path $directory.FullName 'config.ini'
        $config = Read-Ini $configPath
        $registrationPath = Join-Path $AvdRoot ($avdName + '.ini')
        $registration = Read-Ini $registrationPath
        $registeredDirectory = $null
        if ($registration['path']) { $registeredDirectory = $registration['path'] }
        elseif ($registration['path.rel']) {
            $registeredDirectory = Join-Path (Split-Path $AvdRoot -Parent) $registration['path.rel']
        }
        $registrationMatches = $null
        if ($registeredDirectory) {
            $registrationMatches = [IO.Path]::GetFullPath($registeredDirectory).TrimEnd('\', '/') -eq $directory.FullName.TrimEnd('\', '/')
        }
        $imagePath = $null
        if ($config['image.sysdir.1']) {
            $imagePath = $config['image.sysdir.1']
            if (-not [IO.Path]::IsPathRooted($imagePath)) { $imagePath = Join-Path $SdkRoot $imagePath }
            $imagePath = [IO.Path]::GetFullPath($imagePath)
        }
        $api = $registration['target']
        if (-not $api -and $imagePath -match '[/\\](android-[^/\\]+)') { $api = $matches[1] }
        [ordered]@{
            name = $avdName; directory = $directory.FullName
            config_exists = (Test-Path -LiteralPath $configPath -PathType Leaf)
            registration_ini = $registrationPath
            registration_exists = (Test-Path -LiteralPath $registrationPath -PathType Leaf)
            registration_matches_directory = $registrationMatches
            api_target = $api; image_sysdir = $config['image.sysdir.1']; image_path = $imagePath
            image_directory_exists = [bool]($imagePath -and (Test-Path -LiteralPath $imagePath -PathType Container))
            system_img_exists = [bool]($imagePath -and (Test-Path -LiteralPath (Join-Path $imagePath 'system.img') -PathType Leaf))
            abi = $config['abi.type']; cpu_arch = $config['hw.cpu.arch']; cores = $config['hw.cpu.ncore']
            ram_mb = $config['hw.ramSize']; width = $config['hw.lcd.width']; height = $config['hw.lcd.height']
            density = $config['hw.lcd.density']; gpu_enabled = $config['hw.gpu.enabled']; gpu_mode = $config['hw.gpu.mode']
            data_partition_size = $config['disk.dataPartition.size']
        }
    }
})
$knownSession = 'measurements\game-assist\2026-09-30-m0-manual-continue\full-recording36g-01\sessions\manual-session-22885039263800'
$roots = @($repositoryRoot)
if ($LegacyRepositoryRoot) { $roots += [IO.Path]::GetFullPath($LegacyRepositoryRoot) }
$candidates = @(foreach ($root in $roots) {
    [ordered]@{ source = 'known_repository_relative_path'; path = (Join-Path $root $knownSession) }
})
foreach ($root in $RecordingRoot) {
    $candidates += [ordered]@{ source = 'explicit_recording_root'; path = [IO.Path]::GetFullPath($root) }
}
$recordings = @(foreach ($candidate in $candidates) {
    $fullRecording = Join-Path $candidate.path 'full-recording'
    $frames = Join-Path $fullRecording 'frames'
    $pngCount = 0
    if (Test-Path -LiteralPath $frames -PathType Container) {
        $pngCount = @(Get-ChildItem -LiteralPath $frames -File -Filter '*.png').Count
    }
    [ordered]@{
        source = $candidate.source; session_root = $candidate.path
        session_exists = (Test-Path -LiteralPath $candidate.path -PathType Container)
        manifest = (Inspect-Path (Join-Path $candidate.path 'manifest.json'))
        summary = (Inspect-Path (Join-Path $fullRecording 'summary.json'))
        index = (Inspect-Path (Join-Path $fullRecording 'index.jsonl'))
        frames_directory = $frames; frames_png_count = $pngCount
    }
})
$metadata = @(foreach ($root in $roots) {
    Inspect-Path (Join-Path $root 'measurements\game-assist\2026-09-30-m0-manual-continue\full-recording36g-01\review\selected-clips-v1')
    Inspect-Path (Join-Path $root 'measurements\vision-dataset-20260927\manifest.json')
})
[ordered]@{
    schema_version = 1; inspection = 'read_only_offline_file_presence_only'
    limitations = @('No hashes verified.', 'No emulator boot or SDK command executed.', 'No recursive repository or disk search.')
    sdk_root = $SdkRoot; sdk_exists = (Test-Path -LiteralPath $SdkRoot -PathType Container); tools = $tools
    avd_root = $AvdRoot; avd_root_exists = (Test-Path -LiteralPath $AvdRoot -PathType Container); avds = $avds
    recordings = $recordings; known_metadata_candidates = $metadata
} | ConvertTo-Json -Depth 8
