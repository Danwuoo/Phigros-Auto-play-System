. "$PSScriptRoot/x2-budget.ps1"
$manifest=Join-Path $script:X2Batch 'input-manifest.json'
if(!(Test-Path -LiteralPath $manifest)){throw 'Freeze manifest first'}
X2-Check 42000000
$attempts=@(
 @{name='c36h-reference-on-1';role='c36h_reference';trace='on';tools='36'},
 @{name='main50-control-on-1';role='main50_control';trace='on';tools='50'},
 @{name='main50-no-override-on-1';role='main50_no_confirmed_winner_override';trace='on';tools='50'},
 @{name='main50-no-override-on-2';role='main50_no_confirmed_winner_override';trace='on';tools='50'},
 @{name='main50-no-override-off';role='main50_no_confirmed_winner_override';trace='off';tools='50'}
)
foreach($attempt in $attempts) {
  $tool=Join-Path $script:X2Repo "out/x2/tools$($attempt.tools)/pas_frame_review.exe"
  $p=Join-Path $script:X2Batch "source/$($attempt.role)-source-provenance.json"
  $output=Join-Path $script:X2Batch $attempt.name
  if(Test-Path -LiteralPath $output){throw 'Preserve every previous attempt'}
  $code=X2-Invoke $attempt.name $tool @('contact-x2',$manifest,$p,$output,$attempt.trace,'owner','frame-first')
  if($code){throw "Primary replay failed: $($attempt.name)"}
}
$code=X2-Invoke 'three-role-comparison' (Join-Path $script:X2Repo 'out/x2/tools50/pas_frame_review.exe') @('contact-x2-compare',$manifest,(Join-Path $script:X2Batch 'c36h-reference-on-1'),(Join-Path $script:X2Batch 'main50-control-on-1'),(Join-Path $script:X2Batch 'main50-no-override-on-1'),(Join-Path $script:X2Batch 'three-role-comparison.json'))
if($code){throw 'X2 comparison failed'}
Write-Output 'Five primary replays and explicit three-role comparison completed.'
