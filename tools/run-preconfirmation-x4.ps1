param([ValidateSet('on','off')][string]$Trace)
. "$PSScriptRoot/x4-budget.ps1"
$name="variant-$Trace-1"
$attempt=Join-Path $script:X4Batch "commands/$name-attempt.json"
if(Test-Path -LiteralPath $attempt){throw 'This bounded run attempt is already consumed'}
if($Trace -eq 'off') {
  $on=Get-Content "$script:X4Batch/variant-on-1/summary.json" -Raw|ConvertFrom-Json
  if(!$on.success){throw 'On run failed; preserve gap instead of treating repeat as repair'}
}
X4-Check $(if($Trace -eq 'on'){20971520}else{8388608})
$mPath=Join-Path $script:X4Batch 'input-manifest.json'
$m=Get-Content -LiteralPath $mPath -Raw|ConvertFrom-Json
$argv=@('contact-x4',$mPath,$m.variant_source.source_provenance_path,(Join-Path $script:X4Batch $name),$Trace,'owner','frame-first')
X4-Json $attempt @{name=$name;trace_policy=$Trace;manifest_sha256=(X4-Sha $mPath);binary_sha256=(X4-Sha $m.variant_source.binary_path);arguments=$argv;max_full_variant_attempts=2;before_bytes=(X4-Bytes $script:X4Batch);policy='trace-on then trace-off; no strategy tuning'}
$code=X4-Invoke $name $m.variant_source.binary_path $argv
if($code){throw "X4 $name failed; attempt retained"}
Write-Output "$name exit=$code"
