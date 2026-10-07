param([Parameter(Mandatory)][string]$ReceiptRepo,[Parameter(Mandatory)][string]$SourceRoot,
 [Parameter(Mandatory)][string]$BuildRoot,[Parameter(Mandatory)][string]$StageName,
 [Parameter(Mandatory)][string]$ToolSnapshot,[Parameter(Mandatory)][string]$GateReceipt,
 [ValidateSet('Configure','Build')][string]$Operation,[ValidateSet('Release','Debug')][string]$Configuration='Release',
 [string[]]$Options=@(),[string[]]$Inputs=@())
$ErrorActionPreference='Stop'
$taskRepo=[IO.Path]::GetFullPath($ReceiptRepo)
$candidateRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$SourceRoot=[IO.Path]::GetFullPath($SourceRoot);$BuildRoot=[IO.Path]::GetFullPath($BuildRoot)
if($SourceRoot -cne (Join-Path $candidateRepo 'research/prelive_current') -or -not $BuildRoot.StartsWith((Join-Path $taskRepo 'out/prelive-current-'),[StringComparison]::OrdinalIgnoreCase)){throw 'explicit-candidate-build-scope'}
# Reuse the frozen build adapter verbatim except for the explicit receipt root
# and already qualified v3 guard. No shell command built from JSON quoting.
$adapter=Join-Path $taskRepo 'research/prelive_current/cmake-stage.ps1'
$text=[IO.File]::ReadAllText($adapter)
$body=$text.Substring($text.IndexOf("`$ErrorActionPreference='Stop'"))
$body=$body.Replace("`$taskRepo=[IO.Path]::GetFullPath((Join-Path `$PSScriptRoot '../..'))",'')
$body=$body.Replace('tools/zero_miss_windows/invoke.ps1','research/windows_process_guard_v3/invoke.ps1')
$TotalSeconds=900
. ([scriptblock]::Create($body))
