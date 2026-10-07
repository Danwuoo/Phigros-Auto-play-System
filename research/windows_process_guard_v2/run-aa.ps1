param([Parameter(Mandatory)][string]$GateReceipt,[Parameter(Mandatory)][string]$ToolSnapshot,
 [Parameter(Mandatory)][string]$Evaluation,
 [ValidatePattern('^[a-z0-9-]{1,12}$')][string]$Attempt='aa02')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh=(Get-Command pwsh -ErrorAction Stop).Source
$gate=[IO.Path]::GetFullPath($GateReceipt);$tool=[IO.Path]::GetFullPath($ToolSnapshot)
$Mode='release';$Operation='pipeline';$Campaign='';$script:inputClosures=@{}
$root=Join-Path $repo 'out/prelive-current-release-win04'
$script:maintenancePath=$PSCommandPath
$recipe=Join-Path $repo 'research/prelive_current/run.ps1';$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'shared-recipe-parser'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage','Native')){
 $definition=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name)
 if($definition.Count -ne 1){throw ('shared-recipe-function '+$name)}
 $functionText=$definition[0].Extent.Text
 if($name -ceq 'Native'){$functionText=$functionText.Replace('$PSCommandPath','$script:maintenancePath').Replace('tools/zero_miss_windows/invoke.ps1','research/windows_process_guard_v2/invoke.ps1')}
 if($name -ceq 'GateInputs'){$functionText=$functionText.Replace('pas.windows-offline-process-gate.v1','pas.windows-offline-process-gate.v2')}
 . ([scriptblock]::Create($functionText))
}
$evaluationValue=Get-Content -LiteralPath $Evaluation -Raw|ConvertFrom-Json
if($evaluationValue.stopped -or $evaluationValue.completed -ne 27 -or $evaluationValue.planned -ne 27 -or $evaluationValue.device_operations -ne 0){throw 'complete-27-process-evaluation-required'}
CheckEntry $evaluationValue.gate
if($evaluationValue.gate.path -cne $gate){throw 'same-evaluated-guard-required'}
foreach($row in $evaluationValue.rows){CheckEntry $row.receipt}
$oldCampaign=Join-Path $repo 'out/prelive-aa-aa01/campaign.json'
$old=Get-Content -LiteralPath $oldCampaign -Raw|ConvertFrom-Json
foreach($b in $old.bindings){CheckEntry $b}
$sourceBinding=@($old.bindings|Where-Object role -ceq 'source_manifest')[0]
$sourceValue=Get-Content -LiteralPath $sourceBinding.path -Raw|ConvertFrom-Json
foreach($b in $sourceValue.files){CheckEntry $b}
$buildSeal=Join-Path $pkg 'current-build-release-win04.json'
$seal=Get-Content -LiteralPath $buildSeal -Raw|ConvertFrom-Json
if($seal.target_commit -cne $old.source_commit){throw 'unchanged-frozen-candidate-required'}
foreach($b in @($seal.files)+@($seal.source_files)+@($seal.migration_targets)+@($seal.input_closure_receipts)+@($seal.build_receipts)+@($seal.gate,$seal.tool_snapshot,$seal.source_migration)){CheckEntry $b}
$maintenanceCommit=(& git -C $repo rev-parse HEAD)
if((& git -C $repo status --porcelain)){throw 'clean-maintenance-commit-required'}
$campaignRoot=Join-Path $repo ('out/prelive-aa-'+$Attempt);Fresh $campaignRoot
[IO.Directory]::CreateDirectory($campaignRoot)|Out-Null
$environment=Join-Path $campaignRoot 'environment.json'
NewJson $environment @{platform='windows-x64';time_utc=[DateTime]::UtcNow.ToString('o');os=[Environment]::OSVersion.VersionString;
 processor=@(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);memory_bytes=(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory;
 background_processes=@(Get-Process|Sort-Object Id|ForEach-Object {@{pid=$_.Id;name=$_.ProcessName;cpu_s=$_.CPU;working_set=$_.WorkingSet64}});
 source_commit=$old.source_commit;source_dirty=$false;configuration='Release';maintenance_commit=$maintenanceCommit;guard=(Binding $gate);build_binding=(Binding $buildSeal);tool_snapshot=(Binding $tool)}
$bindings=@($old.bindings|Where-Object role -cne 'environment')
$envBinding=Binding $environment;$envBinding.role='environment';$bindings+=@($envBinding)
foreach($pair in @(@('process_guard',$gate),@('maintenance_recipe',$PSCommandPath),@('process_evaluation',$Evaluation))){$b=Binding $pair[1];$b.role=$pair[0];$bindings+=@($b)}
$plans=@();foreach($plan in $old.planned_runs){$copy=@{run_id=$plan.run_id.Replace('aa01',$Attempt);scene=$plan.scene;mode=$plan.mode;report_path=$plan.report_path.Replace('aa01',$Attempt)};$plans+=@($copy);Fresh $copy.report_path}
$Campaign=Join-Path $campaignRoot 'campaign.json'
NewJson $Campaign @{schema=$old.schema;id=('windows-aa-'+$Attempt);configuration=$old.configuration;platform=$old.platform;
 source_commit=$old.source_commit;maintenance_commit=$maintenanceCommit;bindings=$bindings;planned_runs=$plans;
 original_stopped_campaign=(Binding $oldCampaign);old_STOP_resumed=$false;old_results_reused_as_new_runs=$false;
 candidate_source_and_binary_changed=$false;process_guard_changed=$true;guard_version='pas.owned-held-identity.v2';
 thresholds_corpus_cadence_and_order_changed=$false;device_operations=0}
$g=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$inputs=@($PSCommandPath,$recipe,$buildSeal,$Campaign,$Evaluation)+$bindings.path+$sourceValue.files.path+$seal.files.path+$seal.source_files.path+$seal.input_closure_receipts.path+(GateInputs)+@($g.guard_source.path,$g.process_source.path,$g.cached_identity_controls.path,$g.identity_wait_controls.path)
$runs=@();$statuses=@();$stopped=$false;$reason=''
try{
 for($i=0;$i -lt $plans.Count;$i++){
  $plan=$plans[$i];$id=$plan.run_id;$stage='pipeline-prelive-release-'+$id
  Native $stage (Join-Path $root 'current_chain.exe') @('pipeline','A',$plan.scene,'2560',$Campaign,$id,$plan.report_path) $inputs 900
  $audit=Join-Path $campaignRoot ($id+'-audit.json')
  Native ('audit-prelive-release-'+$id) (Join-Path $root 'prelive_audit.exe') @($plan.report_path,$audit) @($inputs+$plan.report_path+($plan.report_path+'.attempts.json')+($plan.report_path+'.journal.jsonl')) 900
  $entry=Binding $plan.report_path;$entry.run_id=$id;$entry.scene=$plan.scene;$entry.audit_path=$audit;$entry.audit_sha256=(Binding $audit).sha256;$process=@{}
  foreach($pair in @(@('specification',(Join-Path $pkg ($stage+'-direct-spec.json'))),@('input_closure',(Join-Path $pkg ($stage+'-input-closure-verification.json'))),
   @('result',(Join-Path $out ($stage+'/'+$stage+'-result.json'))),@('verification',(Join-Path $out ($stage+'/'+$stage+'-verification.json'))),
   @('freeze',(Join-Path $out ($stage+'/freeze.json'))),@('state',(Join-Path $out ($stage+'/state.json'))),@('contract',(Join-Path $out ($stage+'/contract.json'))),
   @('command',(Join-Path $out ($stage+'/'+$stage+'-command.json'))))){$process[$pair[0]]=Binding $pair[1]}
  $entry.process_evidence=$process;$runs+=@($entry)
  $input=Join-Path $campaignRoot ($id+'-gate-input.json');NewJson $input @{schema='pas.current-rails-cost-input.v2';campaign=(Binding $Campaign);plan_index=$i;run=$entry}
  $verdict=Join-Path $campaignRoot ($id+'-gate.json')
  Native ('gate-prelive-release-'+$id) (Join-Path $root 'cost_analysis.exe') @('run',$input,$verdict) @($inputs+$input+$audit+$plan.report_path+($plan.report_path+'.attempts.json')+($plan.report_path+'.journal.jsonl')+$process.Values.path) 900
  $value=Get-Content -LiteralPath $verdict -Raw|ConvertFrom-Json
  $statuses+=@(@{run_id=$id;status=$(if($value.validity.pass){'PASS'}else{'NOT_READY'});verdict=(Binding $verdict)})
  if(-not $value.validity.pass){throw ('necessary-cost-validity-failed '+$id)}
 }
}catch{$stopped=$true;$reason=$_.Exception.Message}
NewJson (Join-Path $campaignRoot 'status.json') @{schema='pas.windows-aa-batch-status.v1';campaign=(Binding $Campaign);source_commit=$old.source_commit;maintenance_commit=$maintenanceCommit;
 normal_stopped=$stopped;reason=$reason;runs=$runs;statuses=$statuses;
 remaining=@($plans|Where-Object {$_.run_id -notin $statuses.run_id}|ForEach-Object {@{run_id=$_.run_id;status=$(if(Test-Path -LiteralPath $_.report_path){'PRODUCED_BUT_UNQUALIFIED'}else{'NOT_RUN'})}});
 all_12_cost_validity_pass=($statuses.Count -eq 12 -and -not $stopped);noise_qualified=$false;ABBA='NOT_RUN';live_ready=$false;device_operations=0}
if($stopped){throw $reason}
$aaInput=Join-Path $campaignRoot 'aa-input.json';NewJson $aaInput @{schema='pas.current-rails-cost-input.v2';campaign=(Binding $Campaign);runs=$runs;normal_stopped=$false}
$noise=Join-Path $campaignRoot 'noise-freeze.json'
$runPaths=@($runs|ForEach-Object {$_.path;$_.audit_path;$_.path+'.attempts.json';$_.path+'.journal.jsonl';$_.process_evidence.Values.path})
Native ('noise-prelive-release-'+$Attempt) (Join-Path $root 'cost_analysis.exe') @('aa',$aaInput,$noise) @($inputs+$aaInput+$runPaths) 900
$noiseValue=Get-Content -LiteralPath $noise -Raw|ConvertFrom-Json
NewJson (Join-Path $campaignRoot 'noise-status.json') @{noise=(Binding $noise);adequate=[bool]$noiseValue.ready;comparison_allowed=[bool]$noiseValue.comparison_allowed;ABBA='NOT_RUN';live_ready=$false;device_operations=0}
if(-not $noiseValue.ready){throw 'AA-noise-inadequate-ABBA-not-permitted'}
