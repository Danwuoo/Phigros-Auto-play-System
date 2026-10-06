param([Parameter(Mandatory)][string]$Specification,[Parameter(Mandatory)][string]$GateReceipt)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$spec=[IO.File]::ReadAllText($Specification)|ConvertFrom-Json
. (Join-Path $PSScriptRoot 'process.ps1') -TaskRepo $taskRoot -TaskAttempt 'windows-round4-20261006' -TaskEvidence $spec.stage_root -TaskSource $PSScriptRoot
$gate=Json $GateReceipt
if($gate.schema -cne 'pas.windows-offline-process-gate.v1' -or -not $gate.wrapper_positive -or $gate.native_controls_verified -ne 4 -or $gate.file_only_failed -ne 0){throw 'native-process-gate'}
CheckEntry $gate.diagnostic
foreach($r in $gate.native_controls){CheckEntry $r.receipt;$v=Json $r.receipt.path;foreach($e in $v.entries){CheckEntry $e}}
$previousEnvironment=@{}
try {
  if($spec.environment){foreach($property in $spec.environment.PSObject.Properties){
    if($property.Name -notin @('VSCMD_SKIP_SENDTELEMETRY','VC_DISABLE_SQM','MSBUILDDISABLENODEREUSE')){throw 'environment-allowlist'}
    if($property.Value -cne '1'){throw 'environment-value'}
    $previousEnvironment[$property.Name]=[Environment]::GetEnvironmentVariable($property.Name,'Process')
    [Environment]::SetEnvironmentVariable($property.Name,$property.Value,'Process')
  }}
  $result=InvokeLocalOwnedStage -StageRoot $spec.stage_root -Stage $spec.stage -Executable $spec.exe -Arguments ([string[]]$spec.argv) -NativeExit $spec.native_exit -RunnerExit $spec.runner_exit -TotalSeconds $spec.total_s -StreamCap $spec.stream_cap -Inputs (@($Specification,$GateReceipt,$PSCommandPath)+@($spec.inputs))
  $result|ConvertTo-Json -Depth 6 -Compress
} finally {
  foreach($name in $previousEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process')}
}
# Owned.Run reports exits in its durable receipt rather than launching a native
# command through PowerShell. Do not propagate a stale or null LASTEXITCODE.
exit 0
