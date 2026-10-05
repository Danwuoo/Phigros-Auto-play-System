param([switch]$RepairX1)
. "$PSScriptRoot/x4-budget.ps1"
$inputPath=Join-Path $script:X4Batch 'input-manifest.json'
$queryPath=Join-Path $script:X4Batch 'query-manifest.json'
$inputText=[IO.File]::ReadAllText($inputPath)
$queryText=[IO.File]::ReadAllText($queryPath)
$inputData=$inputText|ConvertFrom-Json -AsHashtable
$queryData=$queryText|ConvertFrom-Json -AsHashtable
$results=[Collections.Generic.List[object]]::new()
function Test-X4Reject([string]$name,[string]$exe,[string[]]$argv,[string]$reason,[string]$output) {
  $exists=Test-Path -LiteralPath $output
  $before=if($exists){X4-Sha $output}else{$null}
  $code=X4-Invoke "negative-$name" $exe $argv
  $log=Join-Path $script:X4Batch "logs/negative-$name.log"
  $errorText=[IO.File]::ReadAllText($log)
  $afterExists=Test-Path -LiteralPath $output
  $preserved=if($exists){$afterExists -and (X4-Sha $output) -eq $before}else{!$afterExists}
  $results.Add(@{name=$name;exit_code=$code;expected_reason=$reason;observed_error=$errorText.Trim();output_existed_before=$exists;output_preserved=$preserved;pass=($code -ne 0 -and $errorText.Contains($reason) -and $preserved);scope='actual CLI rejection; no variant replay started';arguments=$argv})
}
function Test-X4Input([string]$name,[scriptblock]$edit,[string]$reason) {
  $m=$inputText|ConvertFrom-Json -AsHashtable
  & $edit $m
  $path=Join-Path $script:X4Batch "cli/$name-input.json"
  X4-Json $path $m
  $output=Join-Path $script:X4Batch "cli/$name-output"
  Test-X4Reject $name $inputData.variant_source.binary_path @('contact-x4',$path,$inputData.variant_source.source_provenance_path,$output,'off','owner','frame-first') $reason $output
}
function Test-X4Query([string]$name,[scriptblock]$edit,[string]$reason) {
  $q=$queryText|ConvertFrom-Json -AsHashtable
  & $edit $q
  $path=Join-Path $script:X4Batch "cli/$name-query.json"
  X4-Json $path $q
  $output=Join-Path $script:X4Batch "$name-invalid-report.json"
  Test-X4Reject $name "$script:X4Repo/out/x4/query-final/x4_tool.exe" @('compare',$path,$queryData.runs.variant_on.root,$queryData.runs.variant_off.root,$output) $reason $output
}
if($RepairX1) {
  $x1=Join-Path $script:X4Campaign 'contact-replay-x1'
  $output=Join-Path $script:X4Batch 'x1-r1-corrected-invalid-report.json'
  Test-X4Reject 'x1-r1-corrected' $inputData.variant_source.binary_path @('contact-compare',"$x1/acceptance-20261002/wrong-input-manifest.json","$x1/c36h-verified-on-1","$x1/main50-verified-on-1",$output) 'comparison_manifest_SHA_mismatch' $output
  $output=Join-Path $script:X4Batch 'x1-r2-corrected-invalid-report.json'
  Test-X4Reject 'x1-r2-corrected' $inputData.variant_source.binary_path @('contact-compare',"$x1/input-manifest.json","$x1/main50-verified-on-1","$x1/c36h-verified-on-1",$output) 'comparison_lineage_expected_c36h' $output
  $initial=Get-Content (Join-Path $script:X4Batch 'cli-negative-results.json') -Raw|ConvertFrom-Json
  $final=@($initial.cases|Where-Object {$_.name -notin @('x1-r1-original','x1-r2-swapped')})+@($results.ToArray())
  $positive=Join-Path $script:X4Batch 'x1-r1-invalid-report.json'
  $oldPositive=Join-Path $x1 'five-cases-verified.json'
  $same=(X4-Sha $positive) -eq (X4-Sha $oldPositive)
  X4-Json (Join-Path $script:X4Batch 'cli-negative-final.json') @{cases=$final;case_count=$final.Count;failures=@($final|Where-Object {!$_.pass}).Count;initial_results_sha256=(X4-Sha (Join-Path $script:X4Batch 'cli-negative-results.json'));harness_failures_retained=2;repair='R1 supplied the valid original manifest by mistake, producing a valid report; R2 expected SHA reason despite correct original lineage rejection';X1_actual_positive=@{path=$positive;sha256=(X4-Sha $positive);equals_protected_verified_report=$same};no_new_full_replay=$true}
  if(@($final|Where-Object {!$_.pass}).Count -or !$same){throw 'Corrected X1 validation failed; artifacts retained'}
  Write-Output "Final CLI negatives $($final.Count)/$($final.Count) pass; X1 positive identical"
  return
}
Test-X4Input 'parent-sha' {$args[0].parent.manifest_sha256='0'*64} 'x4_parent_SHA'
Test-X4Input 'input-index' {$args[0].index_sha256='0'*64} 'input_manifest_SHA'
Test-X4Input 'input-windows' {$args[0].windows[0].first++} 'x4_input_bridge_windows'
Test-X4Input 'reused-sha' {$args[0].reused_runs.main50_control.files['trace.jsonl']='0'*64} 'x4_reused_artifact_SHA'
Test-X4Input 'missing-file-binding' {$args[0].reused_runs.main50_control.files.Remove('events.jsonl')} 'x4_missing_file_binding'
Test-X4Input 'source-sha' {$args[0].variant_source.source_provenance_sha256='0'*64} 'x4_variant_binding_source_provenance_sha256'
Test-X4Input 'binary-sha' {$args[0].variant_source.binary_sha256='0'*64} 'x4_variant_binding_binary_sha256'
Test-X4Input 'tracking-sha' {$args[0].variant_source.tracking_source_sha256='0'*64} 'x4_variant_binding_tracking_source_sha256'
Test-X4Input 'oracle-sha' {$args[0].oracle.sha256='0'*64} 'x4_oracle_SHA'
Test-X4Input 'reused-role' {$m=$args[0];$a=$m.reused_runs.c36h_reference;$m.reused_runs.c36h_reference=$m.reused_runs.main50_control;$m.reused_runs.main50_control=$a} 'x2_role_expected_c36h_reference'
Test-X4Query 'query-input-sha' {$args[0].input_manifest_sha256='0'*64} 'x4_query_input_SHA'
Test-X4Query 'query-role' {$args[0].runs.variant_on.role='main50_control'} 'x4_query_role_trace'
Test-X4Query 'query-trace' {$args[0].runs.variant_off.trace='on'} 'x4_query_role_trace'
Test-X4Query 'query-file-sha' {$args[0].runs.variant_on.files['events.jsonl']='0'*64} 'x4_query_artifact_SHA'
Test-X4Query 'query-missing-binding' {$args[0].runs.variant_on.files.Remove('summary.json')} 'x4_query_missing_binding'
# PowerShell ConvertTo-Json normalizes 1.0 to 1; retain actual float JSON bytes.
$floatPath=Join-Path $script:X4Batch 'cli/query-float-schema.json'
X4-Write $floatPath ($queryText -replace '("schema"\s*:\s*)1\b','${1}1.0')
$output=Join-Path $script:X4Batch 'query-float-invalid-report.json'
Test-X4Reject 'query-float' "$script:X4Repo/out/x4/query-final/x4_tool.exe" @('compare',$floatPath,$queryData.runs.variant_on.root,$queryData.runs.variant_off.root,$output) 'x4_query_contract' $output
$output=Join-Path $script:X4Batch 'query-swapped-invalid-report.json'
Test-X4Reject 'query-swapped' "$script:X4Repo/out/x4/query-final/x4_tool.exe" @('compare',$queryPath,$queryData.runs.variant_off.root,$queryData.runs.variant_on.root,$output) 'x4_query_run_role_root' $output
$output=Join-Path $script:X4Batch 'causal-report.json'
Test-X4Reject 'existing-output' "$script:X4Repo/out/x4/query-final/x4_tool.exe" @('compare',$queryPath,$queryData.runs.variant_on.root,$queryData.runs.variant_off.root,$output) 'x4_output_exists_or_open' $output
$x1=Join-Path $script:X4Campaign 'contact-replay-x1'
$output=Join-Path $script:X4Batch 'x1-r1-invalid-report.json'
Test-X4Reject 'x1-r1-original' $inputData.variant_source.binary_path @('contact-compare',"$x1/acceptance-20261002/wrong-input-manifest.json","$x1/c36h-verified-on-1","$x1/main50-verified-on-1",$output) 'comparison_manifest_SHA_mismatch' $output
$output=Join-Path $script:X4Batch 'x1-r2-invalid-report.json'
Test-X4Reject 'x1-r2-swapped' $inputData.variant_source.binary_path @('contact-compare',"$x1/input-manifest.json","$x1/main50-verified-on-1","$x1/c36h-verified-on-1",$output) 'comparison_lineage_expected_c36h' $output
X4-Json (Join-Path $script:X4Batch 'cli-negative-results.json') @{cases=$results;case_count=$results.Count;failures=@($results|Where-Object {!$_.pass}).Count;no_new_full_replay=$true}
if(@($results|Where-Object {!$_.pass}).Count){throw 'Preserved CLI mismatch; inspect actual reason before repairing harness'}
Write-Output "CLI negatives $($results.Count)/$($results.Count) pass"
