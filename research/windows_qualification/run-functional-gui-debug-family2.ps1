param([ValidateSet('release','debug','asan')][string]$Mode='release',
 [ValidateSet('tests','family')][string]$Operation='tests',
 [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$BuildAttempt='win02',
 [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$Attempt='functional01',
 [Parameter(Mandatory)][string]$GateReceipt,[Parameter(Mandatory)][string]$ToolSnapshot,
 [string]$ClipRoot='C:/Users/wurre/Desktop/Phigros-Auto-play-System/measurements/game-assist/manual-session-3906621440800/pixel-clips',
 [string]$FamilyBefore='')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh=(Get-Command pwsh -ErrorAction Stop).Source
$gate=[IO.Path]::GetFullPath($GateReceipt);$tool=[IO.Path]::GetFullPath($ToolSnapshot)
$root=Join-Path $repo ('out/prelive-current-'+$Mode+'-'+$BuildAttempt)
$script:inputClosures=@{}
$recipe=Join-Path $repo 'research/prelive_current/run.ps1';$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'functional-shared-recipe-parser'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage')){
 $function=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name)
 if($function.Count -ne 1){throw ('functional-shared-function '+$name)}
 . ([scriptblock]::Create($function[0].Extent.Text))
}
function Native($stage,$executable,$arguments,$stageInputs,[int]$seconds=300,[int]$expected=0){
 if($Operation -ne 'family' -or $Mode -ne 'debug' -or $BuildAttempt -ne 'win03' -or $Attempt -ne 'regress06'){throw 'explicit-Debug-family2-continuation-scope'}
 $seconds=900
 # Hash-heavy synthetic admission controls have a separately predeclared
 # cold-only 900s deadline. Normal cost cadence/thresholds are unaffected.
 if($stage -match '^(cost-unit|cost-review|input-closure-review|producer-review)-'){$seconds=900}
 if($executable -notin $seal.files.path){throw 'functional-binary-not-build-bound'}
 Stage $stage (Join-Path $repo 'out/readiness-20261007-01/functional-gui-stage.ps1') @{
  StageName=$stage;Executable=$executable;Arguments=@($arguments);GateReceipt=$gate;ExpectedNative=$expected;TotalSeconds=$seconds;
  Inputs=@($stageInputs+$PSCommandPath+$recipe+(Join-Path $repo 'out/readiness-20261007-01/functional-gui-stage.ps1'))}
}
$buildStage='build-prelive-'+$Mode+'-'+$BuildAttempt;Receipt $buildStage
$buildSeal=Join-Path $pkg ('current-build-'+$Mode+'-'+$BuildAttempt+'.json')
$seal=Get-Content -LiteralPath $buildSeal -Raw|ConvertFrom-Json
if($seal.schema -cne 'pas.current-windows-build-binding.v1' -or $seal.mode -cne $Mode -or $seal.build_root -cne $root -or $seal.gate.path -cne $gate -or $seal.tool_snapshot.path -cne $tool){throw 'functional-exact-build-binding'}
foreach($entry in @($seal.files)+@($seal.build_receipts)+@($seal.source_files)+@($seal.migration_targets)+@($seal.input_closure_receipts)+@($seal.gate,$seal.tool_snapshot,$seal.source_migration)){CheckEntry $entry}
$inputs=@($buildSeal,$PSCommandPath,$recipe)+@($seal.files.path)+@($seal.build_receipts.path)+@($seal.source_files.path)+@($seal.migration_targets.path)+@($seal.input_closure_receipts.path)+(GateInputs)
$inputs+=@((Join-Path $repo 'docs/research/zero-miss-20261006/windows/PRELIVE_CURRENT_COST_PROTOCOL.md'),(Join-Path $repo 'configs/phigros-hd-assist-five-lead35.json'))
# Execute the literal original tests/family body. Cases, ordering, fixtures,
# before/after checks and native argv are unchanged; only the process envelope
# is adapted. Pixel/cost/analysis operations are deliberately unavailable here.
$selected=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.IfStatementAst]},$true)|Where-Object {
 $_.Clauses.Count -eq 1 -and $_.Clauses[0].Item1.Extent.Text -ceq ('$Operation -eq '+"'$Operation'")})
if($selected.Count -ne 1){throw 'functional-original-operation-body'}
$body=$selected[0].Clauses[0].Item2.Extent.Text
$old1='Native (''family-v1-prelive-''+$Mode+''-''+$Attempt) (Join-Path $root ''cost_workload_family.exe'') @($family1) $inputs'
$old2='Native (''verify-family-v1-prelive-''+$Mode+''-''+$Attempt) (Join-Path $root ''cost_workload_family_verify.exe'') @($FamilyBefore,$family1,$verify1) @($inputs+$FamilyBefore+$family1)'
if(-not $body.Contains($old1) -or -not $body.Contains($old2)){throw 'exact-original-family-prefix'}
foreach($prior in @('family-v1-prelive-debug-regress04','verify-family-v1-prelive-debug-regress04')){
 Receipt $prior;$closure=Get-Content -LiteralPath (Join-Path $pkg ($prior+'-input-closure-verification.json')) -Raw|ConvertFrom-Json
 if(-not $closure.pass -or -not $closure.before_verified -or -not $closure.after_verified){throw 'verified-prior-Debug-family-prefix'}
}
$body=$body.Replace($old1,'Receipt ''family-v1-prelive-debug-regress04''').Replace($old2,'Receipt ''verify-family-v1-prelive-debug-regress04''')
& ([scriptblock]::Create($body.Substring(1,$body.Length-2)))
