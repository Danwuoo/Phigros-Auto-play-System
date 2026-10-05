param([Parameter(Mandatory)][string]$Batch,
      [Parameter(Mandatory)][string]$Tool,
      [Parameter(Mandatory)][string]$NewEvidenceRoot)
$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$false
$comparisonBatch=(Resolve-Path -LiteralPath $Batch).Path
$comparisonTool=(Resolve-Path -LiteralPath $Tool).Path
$comparisonEvidence=[IO.Path]::GetFullPath($NewEvidenceRoot)
if(!$comparisonEvidence.StartsWith($comparisonBatch+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Evidence must remain inside X1 batch' }
if(Test-Path -LiteralPath $comparisonEvidence) { throw 'Evidence destination must be new' }
$comparisonBefore=[long](Get-ChildItem -LiteralPath $comparisonBatch -Recurse -File | Measure-Object Length -Sum).Sum
if($comparisonBefore+1048576 -gt 134217728) { throw 'Insufficient X1 batch budget for comparison checks' }
New-Item -ItemType Directory -Path $comparisonEvidence | Out-Null
function Write-ComparisonJson([string]$path,$value) {
  if(Test-Path -LiteralPath $path) { throw "Preserve existing file: $path" }
  [IO.File]::WriteAllText($path,($value | ConvertTo-Json -Depth 30),[Text.UTF8Encoding]::new($false))
}
function Comparison-Sha([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
$comparisonManifest=Join-Path $comparisonBatch 'input-manifest.json'
$comparisonA=Join-Path $comparisonBatch 'c36h-verified-on-1'
$comparisonB=Join-Path $comparisonBatch 'main50-verified-on-1'
$comparisonChecks=@()
function Invoke-Comparison([string]$name,[string]$manifest,[string]$a,[string]$b,[string]$reason='',[string]$existing='') {
  $output=if($existing){$existing}else{Join-Path $comparisonEvidence "$name.json"}
  $beforeHash=if($existing){Comparison-Sha $output}else{$null}
  if(!$existing -and (Test-Path -LiteralPath $output)) { throw 'Comparison output must be new' }
  $arguments=@('contact-compare',$manifest,$a,$b,$output)
  $log=Join-Path $comparisonEvidence "$name.log"
  & $comparisonTool @arguments *> $log
  $code=$LASTEXITCODE
  $message=[IO.File]::ReadAllText($log).Trim()
  $written=Test-Path -LiteralPath $output
  if($reason) {
    if($code -eq 0 -or !$message.Contains($reason) -or (!$existing -and $written)) { throw "Expected pre-output rejection missing: $name ($message)" }
    if($existing -and (Comparison-Sha $output) -cne $beforeHash) { throw 'Existing evidence changed' }
  } elseif($code -ne 0 -or !$written) { throw "Valid comparison failed: $message" }
  $script:comparisonChecks+=@{name=$name;tool=$comparisonTool;arguments=$arguments;exit_code=$code;expected_reason=$reason;actual_reason=$message;new_report_written=(!$existing -and $written);existing_preserved=if($existing){$true}else{$null};log_sha256=(Comparison-Sha $log)}
}
Invoke-Comparison 'five-cases' $comparisonManifest $comparisonA $comparisonB
if((Comparison-Sha "$comparisonEvidence/five-cases.json") -cne (Comparison-Sha "$comparisonBatch/five-cases-verified.json")) { throw 'Valid comparison changed bytes' }
Invoke-Comparison 'r1-original' "$comparisonBatch/acceptance-20261002/wrong-input-manifest.json" $comparisonA $comparisonB 'comparison_manifest_SHA_mismatch'
$comparisonRaw=[IO.File]::ReadAllText($comparisonManifest)
[IO.File]::WriteAllText("$comparisonEvidence/changed-bytes-manifest.json",$comparisonRaw+"`n",[Text.UTF8Encoding]::new($false))
Invoke-Comparison 'manifest-bytes' "$comparisonEvidence/changed-bytes-manifest.json" $comparisonA $comparisonB 'comparison_manifest_SHA_mismatch'
$comparisonWindows=$comparisonRaw | ConvertFrom-Json
$comparisonWindows.windows[0].first++
Write-ComparisonJson "$comparisonEvidence/changed-windows-manifest.json" $comparisonWindows
Invoke-Comparison 'manifest-windows' "$comparisonEvidence/changed-windows-manifest.json" $comparisonA $comparisonB 'comparison_manifest_SHA_mismatch'
Invoke-Comparison 'r2-original-swapped' $comparisonManifest $comparisonB $comparisonA 'comparison_lineage_expected_c36h'
Invoke-Comparison 'same-c36h' $comparisonManifest $comparisonA $comparisonA 'comparison_lineage_expected_main50'
Invoke-Comparison 'same-main50' $comparisonManifest $comparisonB $comparisonB 'comparison_lineage_expected_c36h'
foreach($role in 'c36h','main50') {
  foreach($mutation in 'missing','unknown','failed','spoofed-source') {
    $source=if($role -eq 'c36h'){$comparisonA}else{$comparisonB}
    $summary=Get-Content -LiteralPath "$source/summary.json" -Raw | ConvertFrom-Json
    $reason="comparison_lineage_expected_$role"
    switch($mutation) {
      'missing' { $summary.PSObject.Properties.Remove('lineage') }
      'unknown' { $summary.lineage='unknown' }
      'failed' { $summary.success=$false; $reason='cannot_compare_failed_run' }
      'spoofed-source' { $summary.source_provenance_sha256='0'*64; $reason="comparison_provenance_binding_$role" }
    }
    $stub=Join-Path $comparisonEvidence "$role-$mutation-summary-only"
    New-Item -ItemType Directory -Path $stub | Out-Null
    Write-ComparisonJson "$stub/summary.json" $summary
    $a=if($role -eq 'c36h'){$stub}else{$comparisonA}
    $b=if($role -eq 'main50'){$stub}else{$comparisonB}
    Invoke-Comparison "$role-$mutation" $comparisonManifest $a $b $reason
  }
}
Invoke-Comparison 'mixed-policy' $comparisonManifest $comparisonA "$comparisonBatch/main50-final-all" 'comparison_policy_mismatch'
Invoke-Comparison 'existing-output' $comparisonManifest $comparisonA $comparisonB 'output_exists' "$comparisonBatch/five-cases-verified.json"
$comparisonBindings=@()
foreach($role in 'c36h','main50') {
  $run=if($role -eq 'c36h'){$comparisonA}else{$comparisonB}
  $summary=Get-Content -LiteralPath "$run/summary.json" -Raw | ConvertFrom-Json
  $provenance="$comparisonBatch/source-v2/$role-source-provenance.json"
  $oldTools=if($role -eq 'c36h'){'verified-tools36'}else{'verified-tools50'}
  $oldBinary=Join-Path (Split-Path -Parent $comparisonTool) "../$oldTools/pas_frame_review.exe"
  if($summary.source_provenance_sha256 -cne (Comparison-Sha $provenance) -or $summary.binary_sha256 -cne (Comparison-Sha $oldBinary)) { throw 'Recorded source/binary binding changed' }
  $comparisonBindings+=@{lineage=$role;run=$run;summary_sha256=(Comparison-Sha "$run/summary.json");source_provenance=$provenance;source_provenance_sha256=$summary.source_provenance_sha256;replay_binary=[IO.Path]::GetFullPath($oldBinary);replay_binary_sha256=$summary.binary_sha256;replay_binary_differs_from_comparison_tool=($summary.binary_sha256 -cne (Comparison-Sha $comparisonTool))}
}
Write-ComparisonJson "$comparisonEvidence/result.json" @{client_date='2026-10-02';timezone='Asia/Taipei';comparison_tool=$comparisonTool;comparison_tool_sha256=(Comparison-Sha $comparisonTool);replay_bindings=$comparisonBindings;valid_comparison_byte_identical=$true;checks=$comparisonChecks;new_full_replays=0;new_live_rounds=0;batch_before_bytes=$comparisonBefore}
$comparisonAfter=[long](Get-ChildItem -LiteralPath $comparisonBatch -Recurse -File | Measure-Object Length -Sum).Sum
if($comparisonAfter -gt 134217728) { throw 'X1 batch quota exceeded' }
Write-Output "Valid report byte-identical; $($comparisonChecks.Count-1) invalid comparisons rejected; batch=$comparisonAfter bytes."
