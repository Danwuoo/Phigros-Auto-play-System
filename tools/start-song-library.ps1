#Requires -Version 7.2
[CmdletBinding()]
param(
    [string]$Library,
    [string]$LegacyConfiguration,
    [switch]$SelfTest
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $Library) { $Library = Join-Path $root 'assets/phigros/4.0.1.2/library.json' }
$panel = Join-Path $PSScriptRoot 'song-library-panel.ps1'
$arguments = @{}
if ($SelfTest) { $arguments.SelfTest = $true }
else {
    if (-not (Test-Path -LiteralPath $Library -PathType Leaf)) {
        throw 'Catalogue index is missing. Run python tools/sync_song_catalog.py sync --output assets/phigros/4.0.1.2, or pull the completed catalogue branch.'
    }
    $arguments.Library = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Library)
    if ($LegacyConfiguration) { $arguments.LegacyConfiguration = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($LegacyConfiguration) }
}
& $panel @arguments
