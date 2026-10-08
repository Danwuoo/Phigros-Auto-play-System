param([ValidatePattern('^main[0-9]{2}$')][string]$BuildAttempt='main10',[ValidatePattern('^[a-z0-9-]{1,16}$')][string]$Attempt='main10')
. (Join-Path $PSScriptRoot 'common.ps1');$script:maintenancePath=$PSCommandPath
LoadBuilds $BuildAttempt
$artifact=Join-Path $repo ('out/prelive-pre-emulator-faults-'+$Attempt);Fresh $artifact;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$plan=Join-Path $artifact 'plan.json';NewJson $plan @{schema='pas.literal-production-boundary-plan.v1';source_commit=$commit;scenarios=@('capture','writer','rpc','slow');requested_per_scenario=1000;warmup=256;measurement=744;cadence_ms=20;scope='faults in endpoint-free boundary doubles; literal unmodified manual_session and core';live_ready=$false;device_operations=0}
$inputs+=@($plan);$Operation='pipeline';$rows=@();$stopped=$false;$reason='';$manual=(Binding (Join-Path $repo 'src/manual_session.cpp')).sha256
try{foreach($scenario in @('capture','writer','rpc','slow')){
 $root=Join-Path $artifact $scenario;$stage='fault-pre-emulator-'+$Attempt+'-'+$scenario
 Native $stage (Exe 'fault_controls') @($scenario,$root) $inputs 900
 $report=Join-Path $root 'report.json';$audit=Join-Path $artifact ($scenario+'-audit.json');$raw=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object FullName)
 Native ('audit-fault-pre-emulator-'+$Attempt+'-'+$scenario) (Exe 'fault_audit') @($root,(Binding $report).sha256,$commit,$manual,$audit) @($inputs+$raw) 900
 $a=Get-Content -LiteralPath $audit -Raw|ConvertFrom-Json;if(-not $a.control_pass -or $a.device_operations -ne 0 -or $a.live_ready){throw 'literal boundary audit'}
 $rows+=@(@{scenario=$scenario;report=(Binding $report);audit=(Binding $audit);process=(ProcessEvidence $stage)})
 Write-Output ($scenario+' literal boundary control and independent audit PASS')
}}catch{$stopped=$true;$reason=$_.Exception.Message}
NewJson (Join-Path $artifact 'status.json') @{schema='pas.literal-production-boundary-status.v1';source_commit=$commit;plan=(Binding $plan);rows=$rows;stopped=$stopped;reason=$reason;remaining=@(@('capture','writer','rpc','slow')|Where-Object {$_ -notin $rows.scenario}|ForEach-Object {@{scenario=$_;status='NOT_RUN'}});cost_qualification=$false;physical_qualification=$false;live_ready=$false;device_operations=0}
if($stopped){throw $reason}
