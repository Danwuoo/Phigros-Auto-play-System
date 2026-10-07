param([Parameter(Mandatory)][string]$ExistingRoot,[Parameter(Mandatory)][string]$GateReceipt)
$ErrorActionPreference='Stop'
$taskRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $PSScriptRoot 'process.ps1') -TaskRepo $taskRepo -TaskAttempt 'guard-v2-evaluation-review-20261007' -TaskEvidence $ExistingRoot -TaskSource $PSScriptRoot
$rows=@()
foreach($dir in Get-ChildItem -LiteralPath $ExistingRoot -Directory|Sort-Object Name){
 $stage=$dir.Name;$statePath=Join-Path $dir.FullName 'state.json'
 $receiptPath=Join-Path $dir.FullName ($stage+'-verification.json')
 $state=Json $statePath;$verification=Json $receiptPath
 if($state.status -cne 'COMPLETE' -or $state.running -or $verification.pending -or $verification.verification_exit -ne 0){throw ('unqualified-control '+$stage)}
 foreach($entry in @($state.receipts)+@($verification.entries)){CheckEntry $entry}
 $freeze=Json (Join-Path $dir.FullName 'freeze.json');foreach($entry in $freeze.files){CheckEntry $entry}
 $result=Json (Join-Path $dir.FullName ($stage+'-result.json'))
 $spec=(Json (Join-Path $dir.FullName 'contract.json')).spec
 VerifyProcess $result.facts $spec
 $analyzer=$spec.exe.EndsWith('\cost_analysis.exe',[StringComparison]::OrdinalIgnoreCase)
 $numeric=if($analyzer){Entry $spec.argv[2]}else{$null}
 $rows+=@{stage=$stage;kind=$(if($analyzer){'read_only_analyzer_diagnostic_not_campaign_continuation'}else{'console_exit_control'});receipt=(Entry $receiptPath);facts=$result.facts;numeric_output=$numeric}
}
if($rows.Count -ne 27){throw '27-existing-qualified-controls-required'}
$durations=@($rows.facts.elapsed_s|Sort-Object)
function Quantile($q){$index=($durations.Count-1)*$q;$lo=[int][Math]::Floor($index);$hi=[int][Math]::Ceiling($index);return $durations[$lo]+($durations[$hi]-$durations[$lo])*($index-$lo)}
function NewReviewJson($path,$value,[int]$cap){$handle=NewHandle $path;try{PutHandle $handle $value $cap}finally{$handle.Dispose()}}
NewReviewJson (Join-Path $ExistingRoot 'evaluation-reviewed02.json') @{schema='pas.owned-held-identity-evaluation.v2';gate=(Entry $GateReceipt);rows=$rows;
 planned=27;completed=$rows.Count;stopped=$false;reason='';
 elapsed_s=@{n=$durations.Count;p50=(Quantile .5);p95=(Quantile .95);p99=(Quantile .99);max=$durations[-1]};
 aggregate_writer=(Entry $PSCommandPath);original_zero_byte_aggregate=(Entry (Join-Path $ExistingRoot 'evaluation.json'));
 previous_zero_byte_review=(Entry (Join-Path $ExistingRoot 'evaluation-reviewed.json'));previous_review_writer=(Entry (Join-Path $ExistingRoot 'review-writer-01.ps1'));
 maintenance_repair='Only rebuild aggregate from sealed complete controls with bounded 1 MiB output. No process relaunched.';
 original_failed_batch_requalified=$false;producer_repeats=0;device_operations=0;live_ready=$false} 1048576
'27_EXISTING_CONTROLS_REVERIFIED'
