param([Parameter(Mandatory)][string]$GateReceipt,[Parameter(Mandatory)][string]$ToolSnapshot)
$ErrorActionPreference='Stop'
$BuildAttempt='win04';$Attempt='aa01'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh=(Get-Command pwsh -ErrorAction Stop).Source
$gate=[IO.Path]::GetFullPath($GateReceipt);$tool=[IO.Path]::GetFullPath($ToolSnapshot)
$Mode='release';$Operation='pipeline';$Campaign='';$script:inputClosures=@{}
$root=Join-Path $repo ('out/prelive-current-release-'+$BuildAttempt)
$script:maintenancePath=$PSCommandPath
$recipe=Join-Path $repo 'research/prelive_current/run.ps1';$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'shared-recipe-parser'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage','Native')){
 $function=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name)
 if($function.Count -ne 1){throw ('shared-recipe-function '+$name)};$functionText=$function[0].Extent.Text;if($name -ceq 'Native'){$functionText=$functionText.Replace('$PSCommandPath','$script:maintenancePath')};. ([scriptblock]::Create($functionText))
}

$Campaign=Join-Path $repo 'out/prelive-aa-aa01/campaign.json'
$frozen=Get-Content -LiteralPath $Campaign -Raw|ConvertFrom-Json
$commit=(& git -C $repo rev-parse HEAD)
if($commit -cne $frozen.source_commit -or (& git -C $repo status --porcelain)){throw 'same-clean-source-required'}
foreach($b in $frozen.bindings){CheckEntry $b}
$buildSeal=Join-Path $pkg 'current-build-release-win04.json';$seal=Get-Content -LiteralPath $buildSeal -Raw|ConvertFrom-Json
$sourceManifest=@($frozen.bindings|Where-Object role -ceq 'source_manifest')[0].path
$sourceValue=Get-Content -LiteralPath $sourceManifest -Raw|ConvertFrom-Json
foreach($b in $sourceValue.files){CheckEntry $b}
$firstPath=Join-Path $repo 'out/prelive-aa-aa01-review02/status.json';$first=Get-Content -LiteralPath $firstPath -Raw|ConvertFrom-Json
CheckEntry $first.verdict;CheckEntry $first.entry
$firstVerdict=Get-Content -LiteralPath $first.verdict.path -Raw|ConvertFrom-Json
if(-not $firstVerdict.validity.pass -or $first.entry.run_id -cne 'aa01-tap1' -or $first.new_producer_runs -ne 0){throw 'actual-first-run-gate-required'}
$originalStatus=Join-Path $repo 'out/prelive-aa-aa01/status.json';$old=Get-Content -LiteralPath $originalStatus -Raw|ConvertFrom-Json
if($old.reason -cne 'Exception calling "GetFullPath" with "1" argument(s): "The path is empty. (Parameter ''path'')"' -or $old.statuses.Count -ne 0){throw 'maintenance-only-gap-required'}
Receipt 'pipeline-prelive-release-aa01-tap1'
foreach($s in @('audit-prelive-release-aa01-tap1-review02','gate-prelive-release-aa01-tap1-review02')){Receipt $s}
$plans=$frozen.planned_runs
$bindings=$frozen.bindings
$Prerequisites=@($bindings|Where-Object role -ceq 'prerequisites')[0].path
$inputs=@($PSCommandPath,$recipe,$buildSeal,$Prerequisites,$Campaign,$firstPath,$originalStatus)+$bindings.path+$sourceValue.files.path+$seal.files.path+$seal.source_files.path+$seal.input_closure_receipts.path+(GateInputs)
$campaignRoot=Join-Path $repo 'out/prelive-aa-aa01-continuation01';Fresh $campaignRoot;[IO.Directory]::CreateDirectory($campaignRoot)|Out-Null
NewJson (Join-Path $campaignRoot 'scope.json') @{schema='pas.frozen-aa-maintenance-continuation.v1';campaign=(Binding $Campaign);original_status=(Binding $originalStatus);
 reason='AST-imported Native had an empty PSCommandPath before audit preparation. No necessary process/cost gate had failed. First raw report independently passed before remaining frozen stages.';
 first_existing_review=(Binding $firstPath);first_run_repeated=$false;source_changed=$false;protocol_changed=$false;corpus_changed=$false;order_changed=$false;STOP_native_stage_resumed=$false;device_operations=0}
$runs=@($first.entry);$statuses=@(@{run_id='aa01-tap1';status='PASS';verdict=$first.verdict});$stopped=$false;$reason=''
try {
 for($i=1;$i -lt $plans.Count;$i++){
  $plan=$plans[$i];$id=$plan.run_id;$stage='pipeline-prelive-release-'+$id
  & $pwsh -NoProfile -File $recipe -Operation pipeline -Mode release -BuildAttempt $BuildAttempt -Attempt $id -Variant A -Scene $plan.scene -Frames 2560 -Campaign $Campaign -CampaignRunId $id -GateReceipt $gate -ToolSnapshot $tool
  if($LASTEXITCODE -ne 0){throw ('qualified-direct-producer-STOP '+$id)}
  $audit=Join-Path $campaignRoot ($id+'-audit.json');Native ('audit-prelive-release-'+$id) (Join-Path $root 'prelive_audit.exe') @($plan.report_path,$audit) @($inputs+$plan.report_path+($plan.report_path+'.attempts.json')+($plan.report_path+'.journal.jsonl')) 900
  $entry=Binding $plan.report_path;$entry.run_id=$id;$entry.scene=$plan.scene;$entry.audit_path=$audit;$entry.audit_sha256=(Binding $audit).sha256;$process=@{}
  foreach($pair in @(@('specification',(Join-Path $pkg ($stage+'-direct-spec.json'))),@('input_closure',(Join-Path $pkg ($stage+'-input-closure-verification.json'))),
    @('result',(Join-Path $out ($stage+'/'+$stage+'-result.json'))),@('verification',(Join-Path $out ($stage+'/'+$stage+'-verification.json'))),
    @('freeze',(Join-Path $out ($stage+'/freeze.json'))),@('state',(Join-Path $out ($stage+'/state.json'))),@('contract',(Join-Path $out ($stage+'/contract.json'))),
    @('command',(Join-Path $out ($stage+'/'+$stage+'-command.json'))))){$process[$pair[0]]=Binding $pair[1]}
  $entry.process_evidence=$process;$runs+=@($entry)
  $input=Join-Path $campaignRoot ($id+'-gate-input.json');NewJson $input @{schema='pas.current-rails-cost-input.v2';campaign=(Binding $Campaign);plan_index=$i;run=$entry}
  $verdict=Join-Path $campaignRoot ($id+'-gate.json');Native ('gate-prelive-release-'+$id) (Join-Path $root 'cost_analysis.exe') @('run',$input,$verdict) @($inputs+$input+$audit+$plan.report_path+($plan.report_path+'.attempts.json')+($plan.report_path+'.journal.jsonl')+$process.Values.path) 900
  $gateValue=Get-Content -LiteralPath $verdict -Raw|ConvertFrom-Json;$statuses+=@(@{run_id=$id;status=$(if($gateValue.validity.pass){'PASS'}else{'NOT_READY'});verdict=(Binding $verdict)})
  if(-not $gateValue.validity.pass){throw ('necessary-cost-validity-failed '+$id)}
 }
}catch{$stopped=$true;$reason=$_.Exception.Message}
$remaining=@($plans|Where-Object {$_.run_id -notin $statuses.run_id}|ForEach-Object {@{run_id=$_.run_id;status=$(if(Test-Path -LiteralPath $_.report_path){'PRODUCED_BUT_UNQUALIFIED'}else{'NOT_RUN'})}})
NewJson (Join-Path $campaignRoot 'status.json') @{schema='pas.windows-aa-batch-status.v1';campaign=(Binding $Campaign);source_commit=$commit;
 normal_stopped=$stopped;reason=$reason;runs=$runs;statuses=$statuses;remaining=$remaining;all_12_cost_validity_pass=($statuses.Count -eq 12 -and -not $stopped);
 noise_qualified=$false;ABBA='NOT_RUN';production_packet_cost_qualified=$false;live_ready=$false;device_operations=0}
if($stopped){throw $reason}
$aaInput=Join-Path $campaignRoot 'aa-input.json';NewJson $aaInput @{schema='pas.current-rails-cost-input.v2';campaign=(Binding $Campaign);runs=$runs;normal_stopped=$false}
$noise=Join-Path $campaignRoot 'noise-freeze.json'
$runPaths=@($runs|ForEach-Object {$_.path;$_.audit_path;$_.path+'.attempts.json';$_.path+'.journal.jsonl';$_.process_evidence.Values.path})
Native ('noise-prelive-release-'+$Attempt) (Join-Path $root 'cost_analysis.exe') @('aa',$aaInput,$noise) @($inputs+$aaInput+$runPaths) 900
$noiseValue=Get-Content -LiteralPath $noise -Raw|ConvertFrom-Json
NewJson (Join-Path $campaignRoot 'noise-status.json') @{noise=(Binding $noise);adequate=[bool]$noiseValue.ready;comparison_allowed=[bool]$noiseValue.comparison_allowed;
 ABBA='NOT_RUN';live_ready=$false;device_operations=0}
if(-not $noiseValue.ready){throw 'AA-noise-inadequate-ABBA-not-permitted'}
