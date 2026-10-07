param([Parameter(Mandatory)][string]$GateReceipt,[Parameter(Mandatory)][string]$ToolSnapshot)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh=(Get-Command pwsh -ErrorAction Stop).Source
$gate=[IO.Path]::GetFullPath($GateReceipt);$tool=[IO.Path]::GetFullPath($ToolSnapshot)
$Mode='release';$Operation='analysis';$script:inputClosures=@{};$script:maintenancePath=$PSCommandPath
$root=Join-Path $repo 'out/prelive-current-release-win04'
$recipe=Join-Path $repo 'research/prelive_current/run.ps1';$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'shared-recipe-parser'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage','Native')){
 $function=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name)
 if($function.Count -ne 1){throw 'exact-shared-function'}
 $text=$function[0].Extent.Text
 # A function reconstructed from an AST has no file-backed PSCommandPath.
 # Bind this literal maintenance caller explicitly; retain the original file.
 if($name -ceq 'Native'){$text=$text.Replace('$PSCommandPath','$script:maintenancePath')}
 . ([scriptblock]::Create($text))
}
$build=Join-Path $pkg 'current-build-release-win04.json';$seal=Get-Content -LiteralPath $build -Raw|ConvertFrom-Json
$Campaign=Join-Path $repo 'out/prelive-aa-aa01/campaign.json'
$campaignValue=Get-Content -LiteralPath $Campaign -Raw|ConvertFrom-Json
foreach($b in $campaignValue.bindings){CheckEntry $b}
$source=Get-Content -LiteralPath (@($campaignValue.bindings|Where-Object role -ceq 'source_manifest')[0].path) -Raw|ConvertFrom-Json
foreach($b in $source.files){CheckEntry $b}
$inputs=@($PSCommandPath,$recipe,$build,$Campaign)+$campaignValue.bindings.path+$source.files.path+$seal.files.path+$seal.source_files.path+$seal.input_closure_receipts.path+(GateInputs)
$producerStage='pipeline-prelive-release-aa01-tap1';Receipt $producerStage
$plan=$campaignValue.planned_runs[0];if($plan.run_id -cne 'aa01-tap1'){throw 'frozen-first-run'}
$review=Join-Path $repo 'out/prelive-aa-aa01-review02';Fresh $review;[IO.Directory]::CreateDirectory($review)|Out-Null
$audit=Join-Path $review 'raw-audit.json'
Native 'audit-prelive-release-aa01-tap1-review02' (Join-Path $root 'prelive_audit.exe') @($plan.report_path,$audit) @($inputs+$plan.report_path+($plan.report_path+'.attempts.json')+($plan.report_path+'.journal.jsonl')) 900
$entry=Binding $plan.report_path;$entry.run_id=$plan.run_id;$entry.scene=$plan.scene;$entry.audit_path=$audit;$entry.audit_sha256=(Binding $audit).sha256;$process=@{}
foreach($pair in @(@('specification',(Join-Path $pkg ($producerStage+'-direct-spec.json'))),@('input_closure',(Join-Path $pkg ($producerStage+'-input-closure-verification.json'))),
 @('result',(Join-Path $out ($producerStage+'/'+$producerStage+'-result.json'))),@('verification',(Join-Path $out ($producerStage+'/'+$producerStage+'-verification.json'))),
 @('freeze',(Join-Path $out ($producerStage+'/freeze.json'))),@('state',(Join-Path $out ($producerStage+'/state.json'))),@('contract',(Join-Path $out ($producerStage+'/contract.json'))),
 @('command',(Join-Path $out ($producerStage+'/'+$producerStage+'-command.json'))))){$process[$pair[0]]=Binding $pair[1]}
$entry.process_evidence=$process
$inputPath=Join-Path $review 'gate-input.json';NewJson $inputPath @{schema='pas.current-rails-cost-input.v2';campaign=(Binding $Campaign);plan_index=0;run=$entry}
$verdict=Join-Path $review 'cost-gate.json'
Native 'gate-prelive-release-aa01-tap1-review02' (Join-Path $root 'cost_analysis.exe') @('run',$inputPath,$verdict) @($inputs+$inputPath+$audit+$plan.report_path+($plan.report_path+'.attempts.json')+($plan.report_path+'.journal.jsonl')+$process.Values.path) 900
NewJson (Join-Path $review 'status.json') @{schema='pas.existing-cost-read-only-review.v1';entry=$entry;verdict=(Binding $verdict);new_producer_runs=0;
 original_batch_status=(Binding (Join-Path $repo 'out/prelive-aa-aa01/status.json'));original_status_modified=$false;live_ready=$false;device_operations=0}
