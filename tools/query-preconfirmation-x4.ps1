. "$PSScriptRoot/x4-budget.ps1"
$mPath=Join-Path $script:X4Batch 'input-manifest.json'
$q=@{schema=1;experiment='preconfirmation_role_oracle_x4_query';input_manifest_path=$mPath;input_manifest_sha256=(X4-Sha $mPath);runs=@{}}
foreach($trace in 'on','off') {
  $root=Join-Path $script:X4Batch "variant-$trace-1"
  $files=@{};foreach($f in 'summary.json','trace.jsonl','events.jsonl','state-digests.jsonl','oracle-attempts.jsonl','first-intervention.jsonl','G1533-1537.jsonl'){$files[$f]=X4-Sha (Join-Path $root $f)}
  $q.runs["variant_$trace"]=@{root=$root;role='main50_preconfirmation_role_oracle';trace=$trace;files=$files}
}
$query=Join-Path $script:X4Batch 'query-manifest.json'
X4-Json $query $q
$code=X4-Invoke 'comparison' "$script:X4Repo/out/x4/query-final/x4_tool.exe" @('compare',$query,$q.runs.variant_on.root,$q.runs.variant_off.root,(Join-Path $script:X4Batch 'causal-report.json'))
if($code){throw 'comparison failed'}
