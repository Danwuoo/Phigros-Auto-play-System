param([Parameter(Mandatory)][string]$NoiseRepairBinding,
      [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedScriptSha256,
      [ValidatePattern('^[a-z0-9-]{1,32}$')][string]$Attempt='main69-e-normal-r01')
$ErrorActionPreference='Stop'
$taskNormalSource=$PSCommandPath
. (Join-Path $PSScriptRoot '../common.ps1')
. (Join-Path $PSScriptRoot '../noise_repair_build.ps1')
$script:maintenancePath=$taskNormalSource
$own=Binding $taskNormalSource
if($own.sha256 -cne $ExpectedScriptSha256){throw 'externally pinned cold Normal3 recipe changed'}
LoadNoiseRepairBuild $NoiseRepairBinding
$delta=$builds[0]
if(-not $delta.cold_harness_delta -or $delta.schema -cne 'pas.cold-producer-eligibility-delta-build.v1'){throw 'explicit eligibility build required'}
$formal='12ec0550e882e3b06751d67fa67c43dda3e9312b'
$base=NoiseRepairReadBinding $delta.base_production_build
if($base.source_commit -cne $formal){throw 'exact base55 formal production required'}
$auditors=@($base.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'production_pipeline_audit.exe'})
if($auditors.Count -ne 1){throw 'one preserved independent raw auditor required'}
$auditor=$auditors[0];CheckEntry $auditor
$producer=Exe 'production_pipeline_cold'
$script:seal.files+=@($auditor)
$inputs=@($inputs+$auditor.path+$own.path|Sort-Object -Unique)
if(@(Get-Process cl,cmake,ninja -ErrorAction SilentlyContinue).Count){throw 'builds must finish before timed cost'}
$artifact=Join-Path $repo ('out/prelive-pre-emulator-production-'+$Attempt);Fresh $artifact
$null=[IO.Directory]::CreateDirectory($artifact)
$plans=@();foreach($scene in @('tap','hold','dense')){$plans+=@(@{id=('normal-'+$scene);mode='release';scene=$scene;n=2560;cadence=20})}
$planPath=Join-Path $artifact 'plan.json'
NewJson $planPath @{schema='pas.pre-emulator-production-plan.v1';source_commit=$formal;execution_source_commit=$commit;cold_harness_delta=$true;delta_build=(Binding $NoiseRepairBinding);base_production_build=$delta.base_production_build;maintenance_recipe=$own;plans=$plans;historical_production9_prerequisite_only=$true;device_operations=0;live_ready=$false}
$inputs+=@($planPath);$Operation='pipeline';$rows=@();$stopped=$false;$reason='';$activePlan=$null;$activeStage='';$phase=''
try{foreach($p in $plans){
 $activePlan=$p;$root=Join-Path $artifact $p.id;$id=$Attempt+'-'+$p.id
 $activeStage='production-pre-emulator-'+$id;$phase='producer'
 Native $activeStage $producer @($p.scene,'2560','20',$root) $inputs 900
 $reportPath=Join-Path $root 'report.json';$reportPin=Binding $reportPath
 $r=Get-Content -LiteralPath $reportPath -Raw|ConvertFrom-Json
 if($r.source_build_commit -cne $formal -or $r.source_build_dirty -or $r.live_ready -or $r.device_operations -ne 0 -or -not $r.public_entry_rejected -or -not $r.production_packet_action_wake_archive_executed){throw 'literal base55 formal identity and cold scope required'}
 $identity=$r.producer_eligibility_delta_identity
 if($identity.schema -cne 'pas.cold-producer-eligibility-identity.v1' -or $identity.source_commit -cne $delta.source_commit -or $identity.base_source_commit -cne $formal -or $identity.source_dirty){throw 'actual compiled eligibility identity differs'}
 NoiseRepairVerifyWorkerPlacement $r
 $producerProof=ProcessEvidence $activeStage
 $raw=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object FullName)
 $auditPath=Join-Path $artifact ($p.id+'-audit.json');$phase='auditor';$activeStage='audit-production-pre-emulator-'+$id
 Native $activeStage $auditor.path @('--root',$root,'--expected-report-sha256',$reportPin.sha256,'--source-commit',$formal,'--output',$auditPath) @($inputs+$raw) 900
 $audit=Get-Content -LiteralPath $auditPath -Raw|ConvertFrom-Json
 if(-not $audit.integrity_verified){throw 'independent full production raw integrity failed'}
 $rows+=@(@{id=$p.id;report=$reportPin;audit=(Binding $auditPath);process=$producerProof;auditor_process=(ProcessEvidence $activeStage);normal_cost_valid=$audit.normal_cost_valid})
 Write-Output ($p.id+' raw integrity PASS; original numerical cost '+$audit.normal_cost_valid)
 if(-not $audit.normal_cost_valid){throw ('necessary original normal cost failed '+$p.id)}
 $bytes=[long]((Get-ChildItem -LiteralPath $artifact -File -Recurse|Measure-Object Length -Sum).Sum)
 if($bytes+1073741824 -gt 12884901888){throw 'original production raw reserve/cap'}
}}catch{$stopped=$true;$reason=$_.Exception.Message}
$failure=$null
if($stopped){$failure=@{id=$activePlan.id;phase=$phase;stage=$activeStage;reason=$reason;status='STOP_PRESERVED'}}
$status=Join-Path $artifact 'status.json'
NewJson $status @{schema='pas.pre-emulator-production-status.v1';source_commit=$formal;execution_source_commit=$commit;cold_harness_delta=$true;delta_build=(Binding $NoiseRepairBinding);base_production_build=$delta.base_production_build;plan=(Binding $planPath);rows=$rows;failure=$failure;stopped=$stopped;reason=$reason;remaining=@($plans|Where-Object {$_.id -notin $rows.id -and ($null -eq $failure -or $_.id -cne $failure.id)}|ForEach-Object {@{id=$_.id;status='NOT_RUN'}});qualification_issued=$false;cost_ready=$false;physical_qualification=$false;live_ready=$false;device_operations=0;new_etw_recordings=0}
if($stopped){throw $reason}
Write-Output 'current eligibility Normal3 full raw/original numerical gates PASS; AA/noise/ABBA qualification pending'
