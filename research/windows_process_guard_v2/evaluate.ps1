param([Parameter(Mandatory)][string]$GateReceipt,[Parameter(Mandatory)][string]$FreshRoot,
 [ValidateRange(1,40)][int]$Repetitions=24)
$ErrorActionPreference='Stop'
$taskRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(Test-Path -LiteralPath $FreshRoot){throw 'fresh-evaluation'}
[IO.Directory]::CreateDirectory($FreshRoot)|Out-Null
. (Join-Path $PSScriptRoot 'process.ps1') -TaskRepo $taskRepo -TaskAttempt 'guard-v2-evaluation-20261007' -TaskEvidence $FreshRoot -TaskSource $PSScriptRoot
$gateValue=Json $GateReceipt
if($gateValue.schema -cne 'pas.windows-offline-process-gate.v2'){throw 'v2-process-gate-required'}
foreach($b in @($gateValue.guard_source,$gateValue.process_source,$gateValue.cached_identity_controls,$gateValue.identity_wait_controls,$gateValue.diagnostic)){CheckEntry $b}
foreach($control in $gateValue.native_controls){CheckEntry $control.receipt;$v=Json $control.receipt.path;foreach($b in $v.entries){CheckEntry $b}}
$pwsh=(Get-Command pwsh -ErrorAction Stop).Source
$fixture=Join-Path $FreshRoot 'fixture.ps1'
[IO.File]::WriteAllText($fixture,'param([int]$Code) Write-Output "held-identity control"; Start-Sleep -Milliseconds 100; exit $Code',[Text.UTF8Encoding]::new($false))
$originalInput=Join-Path $taskRepo 'out/prelive-aa-aa01-continuation01/aa01-tap3-gate-input.json'
$analyzer=Join-Path $taskRepo 'out/prelive-current-release-win04/cost_analysis.exe'
$rows=@();$stopped=$false;$failure=''
try{
 for($i=0;$i -lt ($Repetitions+3);$i++){
  $name='guard-v2-eval-'+(Split-Path $FreshRoot -Leaf)+'-'+$i.ToString('00')
  $stageRoot=Join-Path $FreshRoot $name
  $isAnalyzer=$i -ge $Repetitions
  $code=if(-not $isAnalyzer -and $i%2){7}else{0}
  $output=Join-Path $FreshRoot ($name+'-numeric-diagnostic.json')
  $exe=if($isAnalyzer){$analyzer}else{$pwsh}
  $argv=if($isAnalyzer){@('run',$originalInput,$output)}else{@('-NoProfile','-File',$fixture,'-Code',"$code")}
  $specPath=Join-Path $FreshRoot ($name+'-spec.json')
  NewJson $specPath @{stage_root=$stageRoot;stage=$name;exe=$exe;argv=$argv;native_exit=$code;runner_exit=$code;total_s=120;stream_cap=4194304;inputs=@($PSCommandPath,$fixture,$originalInput,$GateReceipt)}
  & $pwsh -NoProfile -File (Join-Path $PSScriptRoot 'invoke.ps1') -Specification $specPath -GateReceipt $GateReceipt
  if($LASTEXITCODE -ne 0){throw ('process-control-failed '+$name)}
  $result=Json (Join-Path $stageRoot ($name+'-result.json'))
  $rows+=@{stage=$name;kind=$(if($isAnalyzer){'read_only_analyzer_diagnostic_not_campaign_continuation'}else{'console_exit_control'});
   receipt=(Entry (Join-Path $stageRoot ($name+'-verification.json')));facts=$result.facts;numeric_output=$(if($isAnalyzer){Entry $output}else{$null})}
 }
}catch{$stopped=$true;$failure=$_.Exception.Message}
$durations=@($rows.facts.elapsed_s|Sort-Object)
function Quantile($q){if(-not $durations.Count){return $null};$index=($durations.Count-1)*$q;$lo=[int][Math]::Floor($index);$hi=[int][Math]::Ceiling($index);return $durations[$lo]+($durations[$hi]-$durations[$lo])*($index-$lo)}
NewJson (Join-Path $FreshRoot 'evaluation.json') @{schema='pas.owned-held-identity-evaluation.v2';gate=(Entry $GateReceipt);rows=$rows;
 planned=$Repetitions+3;completed=$rows.Count;stopped=$stopped;reason=$failure;
 elapsed_s=@{n=$durations.Count;p50=(Quantile .5);p95=(Quantile .95);p99=(Quantile .99);max=$(if($durations.Count){$durations[-1]}else{$null})};
 original_failed_batch_requalified=$false;producer_repeats=0;device_operations=0;live_ready=$false}
if($stopped){throw $failure}
