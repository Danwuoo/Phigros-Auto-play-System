param([ValidatePattern('^main[0-9]{2}$')][string]$BuildAttempt='main08',[ValidatePattern('^[a-z0-9-]{1,16}$')][string]$Attempt='main08')
. (Join-Path $PSScriptRoot 'common.ps1');$script:maintenancePath=$PSCommandPath
LoadBuilds $BuildAttempt
$artifact=Join-Path $repo ('out/prelive-pre-emulator-production-'+$Attempt);Fresh $artifact;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$plans=@();foreach($scene in @('tap','hold','dense')){$plans+=@(@{id=('normal-'+$scene);mode='release';scene=$scene;n=2560;cadence=20})}
foreach($scene in @('tap','dense')){$plans+=@(@{id=('stress-'+$scene);mode='release';scene=$scene;n=10000;cadence=8})}
foreach($mode in @('debug','asan')){foreach($scene in @('tap','hold')){$plans+=@(@{id=($mode+'-'+$scene);mode=$mode;scene=$scene;n=1000;cadence=20})}}
$planPath=Join-Path $artifact 'plan.json';NewJson $planPath @{schema='pas.pre-emulator-production-plan.v1';source_commit=$commit;plans=$plans;production_binary_executed=$false;device_operations=0;live_ready=$false;remaining_endpoint_free_fault_scenarios=@('slow consumer','writer fault','unknown RPC result')}
$inputs+=@($planPath);$Operation='pipeline';$rows=@();$stopped=$false;$reason=''
try{foreach($p in $plans){
 $Mode=$p.mode;$id=$Attempt+'-'+$p.id;$root=Join-Path $artifact $p.id;$stage='production-pre-emulator-'+$id
 Native $stage (Exe 'production_pipeline_cold' $p.mode) @($p.scene,[string]$p.n,[string]$p.cadence,$root) $inputs 900
 $report=Join-Path $root 'report.json';$r=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
 if($r.source_build_commit -cne $commit -or $r.source_build_dirty -or $r.live_ready -or $r.device_operations -ne 0 -or -not $r.production_packet_action_wake_archive_executed -or -not $r.public_entry_rejected){throw 'literal same-source endpoint-free production required'}
 $audit=Join-Path $artifact ($p.id+'-audit.json');$raw=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object FullName)
 Native ('audit-production-pre-emulator-'+$id) (Exe 'production_pipeline_audit' $p.mode) @('--root',$root,'--expected-report-sha256',(Binding $report).sha256,'--source-commit',$commit,'--output',$audit) @($inputs+$raw) 900
 $a=Get-Content -LiteralPath $audit -Raw|ConvertFrom-Json;if(-not $a.integrity_verified){throw 'production raw integrity'}
 $rows+=@(@{id=$p.id;report=(Binding $report);audit=(Binding $audit);process=(ProcessEvidence $stage);normal_cost_valid=$a.normal_cost_valid})
 Write-Output ($p.id+' raw integrity PASS; numerical cost '+$a.normal_cost_valid)
 if($p.id.StartsWith('normal-') -and -not $a.normal_cost_valid){throw ('production necessary numerical gate failed '+$p.id)}
 $bytes=[long]((Get-ChildItem -LiteralPath $artifact -File -Recurse|Measure-Object Length -Sum).Sum);if($bytes+1073741824 -gt 12884901888){throw 'production raw capacity headroom'}
}}catch{$stopped=$true;$reason=$_.Exception.Message}
NewJson (Join-Path $artifact 'status.json') @{schema='pas.pre-emulator-production-status.v1';plan=(Binding $planPath);source_commit=$commit;rows=$rows;stopped=$stopped;reason=$reason;remaining=@($plans|Where-Object id -notin $rows.id|ForEach-Object {@{id=$_.id;status='NOT_RUN'}});live_ready=$false;device_operations=0}
if($stopped){throw $reason}
