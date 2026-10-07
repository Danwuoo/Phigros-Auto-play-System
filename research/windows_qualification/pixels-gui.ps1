param([Parameter(Mandatory)][string]$GateReceipt,[Parameter(Mandatory)][string]$ToolSnapshot,
 [Parameter(Mandatory)][string]$Selection,[ValidatePattern('^[a-z0-9-]{1,24}$')][string]$Attempt='prefix04')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'));$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh=(Get-Command pwsh -ErrorAction Stop).Source;$gate=[IO.Path]::GetFullPath($GateReceipt);$tool=[IO.Path]::GetFullPath($ToolSnapshot)
$Mode='release';$Operation='pixels';$root=Join-Path $repo 'out/prelive-current-release-win04';$script:inputClosures=@{}
$recipe=Join-Path $repo 'research/prelive_current/run.ps1';$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'shared-parser'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage')){$f=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name);if($f.Count -ne 1){throw 'shared-function'};. ([scriptblock]::Create($f[0].Extent.Text))}
Receipt 'build-prelive-release-win04'
$buildSeal=Join-Path $pkg 'current-build-release-win04.json';$seal=Get-Content -LiteralPath $buildSeal -Raw|ConvertFrom-Json
foreach($b in @($seal.files)+@($seal.source_files)+@($seal.migration_targets)+@($seal.build_receipts)+@($seal.input_closure_receipts)+@($seal.gate,$seal.tool_snapshot,$seal.source_migration)){CheckEntry $b}
$selected=Get-Content -LiteralPath $Selection -Raw|ConvertFrom-Json
if($selected.frames.Count -ne 3063){throw 'original-complete-prefix-required'}
foreach($b in $selected.frames){CheckEntry $b}
$inputs=@($PSCommandPath,$recipe,$buildSeal,$Selection,$selected.index_path,(Join-Path $selected.record_root 'manifest.json'))+$selected.frames.path+$seal.files.path+$seal.source_files.path+$seal.input_closure_receipts.path+(GateInputs)
$report=Join-Path $pkg ('pixels-release-'+$Attempt+'.json');Fresh $report
$stage='pixels-prelive-release-'+$Attempt
Stage $stage (Join-Path $PSScriptRoot 'functional-gui-stage.ps1') @{StageName=$stage;Executable=(Join-Path $root 'current_chain.exe');Arguments=@('pixels',$Selection,$report);GateReceipt=$gate;Inputs=$inputs;TotalSeconds=900}
foreach($b in $selected.frames){CheckEntry $b}
$audit=Join-Path $pkg ('pixels-audit-release-'+$Attempt+'.json');Fresh $audit
$auditStage='audit-pixels-prelive-release-'+$Attempt
Stage $auditStage (Join-Path $PSScriptRoot 'functional-gui-stage.ps1') @{StageName=$auditStage;Executable=(Join-Path $root 'prelive_audit.exe');Arguments=@($report,$audit);GateReceipt=$gate;
 Inputs=@($inputs+$report+($report+'.rows.jsonl')+($report+'.events.jsonl'));TotalSeconds=900}
# Complete WIC/functionality and raw audit only; no direct cost provenance.
