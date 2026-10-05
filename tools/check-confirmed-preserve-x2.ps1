param([string]$Attempt='final')
. "$PSScriptRoot/x2-budget.ps1"
$tool=Join-Path $script:X2Repo 'out/x2/tools50/pas_frame_review.exe'
$manifest=Join-Path $script:X2Batch 'input-manifest.json'
$a=Join-Path $script:X2Batch 'c36h-reference-on-1';$b=Join-Path $script:X2Batch 'main50-control-on-1';$c=Join-Path $script:X2Batch 'main50-no-override-on-1'
$evidence=Join-Path $script:X2Batch "cli-checks-$Attempt";if(Test-Path -LiteralPath $evidence){throw 'Check evidence must be new'}
$checks=@()
function X2-Negative([string]$name,[string]$inputManifest,[string]$aroot,[string]$broot,[string]$croot,[string]$reason,[string]$existing='') {
  $output=if($existing){$existing}else{Join-Path $evidence "$name-report.json"}
  $before=if($existing){X2-Sha $existing}else{$null}
  $code=X2-Invoke "cli-$Attempt-$name" $tool @('contact-x2-compare',$inputManifest,$aroot,$broot,$croot,$output)
  $errorText=[IO.File]::ReadAllText((Join-Path $script:X2Batch "logs/cli-$Attempt-$name.log"))
  if($code -eq 0 -or !$errorText.Contains($reason) -or (!$existing -and (Test-Path -LiteralPath $output))){throw "Expected CLI rejection missing $name"}
  if($existing -and (X2-Sha $existing) -cne $before){throw 'Existing output changed'}
  $script:checks+=@{name=$name;exit_code=$code;reason=$errorText.Trim();no_new_valid_report=$true;existing_preserved=if($existing){$true}else{$null}}
}
$raw=[IO.File]::ReadAllText($manifest)
X2-Write (Join-Path $evidence 'wrong-bytes.json') ($raw+"`n")
X2-Negative 'wrong-bytes' (Join-Path $evidence 'wrong-bytes.json') $a $b $c 'x2_manifest_SHA_mismatch'
foreach($key in 'index','window') {
  $m=$raw|ConvertFrom-Json
  if($key -eq 'index'){$m.index_sha256='0'*64}else{$m.windows[0].first++}
  X2-Json (Join-Path $evidence "wrong-$key.json") $m
  X2-Negative "wrong-$key" (Join-Path $evidence "wrong-$key.json") $a $b $c 'x2_manifest_SHA_mismatch'
}
X2-Negative 'swapped-main50' $manifest $a $c $b 'x2_role_expected_main50_control'
X2-Negative 'swapped-reference' $manifest $b $a $c 'x2_role_expected_c36h_reference'
X2-Negative 'duplicate-control' $manifest $a $b $b 'x2_role_expected_main50_no_confirmed_winner_override'
foreach($key in 'variant','source_provenance_sha256','tracking_source_sha256','binary_sha256','tie','success') {
  $s=Get-Content -LiteralPath "$c/summary.json" -Raw|ConvertFrom-Json
  $reason="x2_run_binding_$key"
  if($key -eq 'success'){$s.success=$false;$reason='x2_failed_run'}
  elseif($key -eq 'tie'){$s.tie='due-first';$reason='x2_policy_mismatch'}
  else{$s.$key=if($key -eq 'variant'){'original-main50'}else{'0'*64}}
  $stub=Join-Path $evidence "$key-summary-only";X2-Json (Join-Path $stub 'summary.json') $s
  X2-Negative "wrong-$key" $manifest $a $b $stub $reason
}
X2-Negative 'existing-output' $manifest $a $b $c 'output_exists' (Join-Path $script:X2Batch 'three-role-comparison.json')
$x1=Join-Path $script:X2Campaign 'contact-replay-x1'
$code=X2-Invoke "x1-$Attempt-valid-compatibility" $tool @('contact-compare',"$x1/input-manifest.json","$x1/c36h-verified-on-1","$x1/main50-verified-on-1",(Join-Path $evidence 'x1-five-cases.json')) 1048576
if($code -or (X2-Sha (Join-Path $evidence 'x1-five-cases.json')) -cne '37d469caa0017be00fb78736fd69fb5cc87585c40b9fd671bb7e8397c420e7f7'){throw 'Original X1 report changed'}
foreach($case in @(@{name='x1-original-R1';manifest="$x1/acceptance-20261002/wrong-input-manifest.json";a="$x1/c36h-verified-on-1";b="$x1/main50-verified-on-1";reason='comparison_manifest_SHA_mismatch'},@{name='x1-original-R2';manifest="$x1/input-manifest.json";a="$x1/main50-verified-on-1";b="$x1/c36h-verified-on-1";reason='comparison_lineage_expected_c36h'})) {
  $output=Join-Path $evidence "$($case.name).json"
  $code=X2-Invoke "$Attempt-$($case.name)" $tool @('contact-compare',$case.manifest,$case.a,$case.b,$output)
  $text=[IO.File]::ReadAllText((Join-Path $script:X2Batch "logs/$Attempt-$($case.name).log"))
  if($code -eq 0 -or !$text.Contains($case.reason) -or (Test-Path -LiteralPath $output)){throw 'X1 original negative regressed'}
  $checks+=@{name=$case.name;exit_code=$code;reason=$text.Trim();no_new_valid_report=$true}
}
X2-Json (Join-Path $evidence 'result.json') @{checks=$checks;expected_rejections=$checks.Count;unexpected_success=0;x1_positive_report_byte_equal=$true;x1_positive_report_sha256=(X2-Sha (Join-Path $evidence 'x1-five-cases.json'));primary_three_role_report=(X2-Sha (Join-Path $script:X2Batch 'three-role-comparison.json'))}
Write-Output "$($checks.Count) expected rejections; X1 report byte-identical."
$global:LASTEXITCODE=0
