param([ValidatePattern('^main[0-9]{2}$')][string]$BuildAttempt='main12',[ValidatePattern('^[a-z0-9-]{1,16}$')][string]$Attempt='main12')
. (Join-Path $PSScriptRoot 'common.ps1');$script:maintenancePath=$PSCommandPath
LoadBuilds $BuildAttempt
$artifact=Join-Path $repo ('out/prelive-pre-emulator-faults-'+$Attempt);Fresh $artifact;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$plan=Join-Path $artifact 'plan.json';NewJson $plan @{schema='pas.literal-production-boundary-plan.v1';source_commit=$commit;scenarios=@('capture','writer','rpc','slow');requested_per_scenario=1000;warmup=256;measurement=744;cadence_ms=20;scope='faults in endpoint-free boundary doubles; literal unmodified manual_session and core';live_ready=$false;device_operations=0}
$inputs+=@($plan);$Operation='pipeline';$rows=@();$stopped=$false;$reason='';$failure=$null;$activeScenario=$null;$activeStage=$null;$phase='producer';$manual=(Binding (Join-Path $repo 'src/manual_session.cpp')).sha256
try{foreach($scenario in @('capture','writer','rpc','slow')){
 $root=Join-Path $artifact $scenario;$stage='fault-pre-emulator-'+$Attempt+'-'+$scenario
 $activeScenario=$scenario;$activeStage=$stage;$phase='producer'
 Native $stage (Exe 'fault_controls') @($scenario,$root) $inputs 900
 $report=Join-Path $root 'report.json';$audit=Join-Path $artifact ($scenario+'-audit.json');$raw=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object FullName)
 $phase='audit_check';$r=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
 if(-not $r.keyboard_is_test_double -or -not $r.console_registration_is_test_double){throw 'controlled endpoint-free user input required'}
 $activeStage='audit-fault-pre-emulator-'+$Attempt+'-'+$scenario;$phase='auditor'
 Native $activeStage (Exe 'fault_audit') @($root,(Binding $report).sha256,$commit,$manual,$audit) @($inputs+$raw) 900
 $phase='audit_check'
 $a=Get-Content -LiteralPath $audit -Raw|ConvertFrom-Json;if(-not $a.control_pass -or $a.device_operations -ne 0 -or $a.live_ready){throw 'literal boundary audit'}
 $rows+=@(@{scenario=$scenario;report=(Binding $report);audit=(Binding $audit);process=(ProcessEvidence $stage)})
 Write-Output ($scenario+' literal boundary control and independent audit PASS')
}}catch{$stopped=$true;$reason=$_.Exception.Message;$failure=@{scenario=$activeScenario;stage=$activeStage;phase=$phase;status=$(if($phase -ceq 'audit_check'){'FAILED_STAGE'}else{'FAILED_EXECUTION'});reason=$reason}}
NewJson (Join-Path $artifact 'status.json') @{schema='pas.literal-production-boundary-status.v1';source_commit=$commit;plan=(Binding $plan);rows=$rows;stopped=$stopped;reason=$reason;failure=$failure;remaining=@(@('capture','writer','rpc','slow')|Where-Object {$_ -notin $rows.scenario -and $_ -cne $activeScenario}|ForEach-Object {@{scenario=$_;status='NOT_RUN'}});cost_qualification=$false;physical_qualification=$false;live_ready=$false;device_operations=0}
if($stopped){throw $reason}
