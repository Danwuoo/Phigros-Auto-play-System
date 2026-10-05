. "$PSScriptRoot/x4-budget.ps1"
$before=Get-Content (Join-Path $script:X4Batch 'workspace-before.json') -Raw|ConvertFrom-Json
$allowed=@('apps/frame_review/main.cpp','apps/frame_review/contact_replay.cpp','apps/frame_review/offline/CMakeLists.txt','docs/PROJECT_STATUS_NEXT_STEPS_20261001.md')
$mismatch=@();$checked=0
foreach($item in $before.protected) {
  if($item.path -in $allowed){continue}
  $path=Join-Path $script:X4Repo $item.path
  $checked++
  if(!(Test-Path -LiteralPath $path) -or (X4-Sha $path) -ne $item.sha256){$mismatch+=$item.path}
}
$frozen=Get-Content (Join-Path $script:X4Batch 'tool-freeze.json') -Raw|ConvertFrom-Json
foreach($item in @($frozen.binaries)+@($frozen.exports)) {
  $checked++
  if((X4-Sha $item.path) -ne $item.sha256){$mismatch+=$item.path}
}
foreach($item in $frozen.source) {
  $checked++
  if((X4-Sha $item.snapshot) -ne $item.snapshot_sha256){$mismatch+=$item.snapshot}
  if($item.path -notin @('apps/frame_review/x4_report.cpp','tools/query-preconfirmation-x4.ps1') -and (X4-Sha (Join-Path $script:X4Repo $item.path)) -ne $item.sha256){$mismatch+=$item.path}
}
$oldStatus=[IO.File]::ReadAllText((Join-Path $script:X4Batch 'status-before.md'))
$newStatus=[IO.File]::ReadAllText((Join-Path $script:X4Repo 'docs/PROJECT_STATUS_NEXT_STEPS_20261001.md'))
$marker='**C36h tint1 是主要'
$bodyPreserved=$newStatus.Substring($newStatus.IndexOf($marker)).StartsWith($oldStatus.Substring($oldStatus.IndexOf($marker)),[StringComparison]::Ordinal)
$formal=@(git diff -- src include CMakeLists.txt)
$sizes=@{x1=(X4-Bytes (Join-Path $script:X4Campaign 'contact-replay-x1'));x2=(X4-Bytes (Join-Path $script:X4Campaign 'confirmed-preserve-x2'));x3=(X4-Bytes (Join-Path $script:X4Campaign 'line-role-x3'))}
$identity=((git rev-parse HEAD) -eq $before.head -and (git branch --show-current) -eq $before.branch -and ((@(git worktree list --porcelain)|ConvertTo-Json -Compress) -eq ($before.worktrees|ConvertTo-Json -Compress)))
$preservation=@{checked=$checked;mismatches=$mismatch;authorized_edit_paths=$allowed;status_old_body_J1_J9_preserved=$bodyPreserved;formal_diff_empty=($formal.Count -eq 0);head_branch_worktrees_unchanged=$identity;old_batches_bytes=$sizes;old_batches_bytes_unchanged=($sizes.x1 -eq $before.x1_bytes -and $sizes.x2 -eq $before.x2_bytes -and $sizes.x3 -eq $before.x3_bytes)}
X4-Json (Join-Path $script:X4Batch 'preservation-check.json') $preservation
if($mismatch.Count -or !$bodyPreserved -or $formal.Count -or !$identity -or !$preservation.old_batches_bytes_unchanged){throw 'Preservation failure retained; do not claim delivery complete'}
$source=@()
foreach($f in Get-ChildItem -LiteralPath "$script:X4Repo/apps/frame_review","$script:X4Repo/tools" -File -Recurse) {
  if($f.FullName -like '*\tools\*' -and $f.Name -notlike '*x4*'){continue}
  $rel=$f.FullName.Substring($script:X4Repo.Length+1).Replace('\','/')
  $snapshot=Join-Path $script:X4Batch "final-tool-source/$rel"
  X4-Write $snapshot ([IO.File]::ReadAllText($f.FullName))
  $source+=@{path=$rel;sha256=(X4-Sha $f.FullName);snapshot=$snapshot;snapshot_sha256=(X4-Sha $snapshot)}
}
foreach($rel in 'tests/x4_oracle_tests.cpp','tests/contact_replay_tests.cpp') {
  $snapshot=Join-Path $script:X4Batch "final-tool-source/$rel"
  X4-Write $snapshot ([IO.File]::ReadAllText((Join-Path $script:X4Repo $rel)))
  $source+=@{path=$rel;sha256=(X4-Sha (Join-Path $script:X4Repo $rel));snapshot=$snapshot;snapshot_sha256=(X4-Sha $snapshot)}
}
$bins=@()
foreach($root in 'out/x4/tools','out/x4/query-final','out/x4/build/Debug') {
  foreach($f in Get-ChildItem -LiteralPath (Join-Path $script:X4Repo $root) -File|Where-Object {$_.Extension -in @('.exe','.dll')}){$bins+=@{path=$f.FullName;bytes=$f.Length;sha256=(X4-Sha $f.FullName)}}
}
X4-Json (Join-Path $script:X4Batch 'final-tool-freeze.json') @{source=$source;binaries=$bins;replay_binary_unchanged=$true;initial_freeze_sha256=(X4-Sha (Join-Path $script:X4Batch 'tool-freeze.json'));report_only_revision='query role/trace labels now explicitly validated; original frozen tools retained';strategy='50/27/11';full_variant_attempts=2}
$commands=@();foreach($f in Get-ChildItem -LiteralPath (Join-Path $script:X4Batch 'commands') -File){$commands+=@{path=$f.Name;sha256=(X4-Sha $f.FullName);data=([IO.File]::ReadAllText($f.FullName)|ConvertFrom-Json -AsHashtable)}}
X4-Json (Join-Path $script:X4Batch 'attempt-ledger.json') @{commands=$commands;full_variant_attempt_count=2;full_variant_failures=0;engineering_failures=@('initial native C7692 compile; wrapper stopped only after native failure; partial log and unknown native exit retained','initial new completed test: 248/249, repaired test precondition only','first ASan build wrapper mutex failure: native exit unknown, console unavailable; sequential build confirmation and tests pass','ASan test launcher referenced absent path; no native test started, corrected preserved fixture path','initial CLI harness 18/20: R1 used valid manifest; R2 expected wrong reason. Actual valid positive and proper lineage rejection retained; corrected original R1/R2 pass');other_tool_attempts=@('two atomic apply_patch failures from unmatched main.cpp anchors; no partial edit','read-only missing-file and rg glob probes produced errors, no evidence or source mutation');post_replay_change='report-only query role/trace validation; original replay binary/export/packet unchanged';no_strategy_retry=$true}
$r=Get-Content (Join-Path $script:X4Batch 'causal-report.json') -Raw|ConvertFrom-Json
$tests=@();foreach($name in 'tests-release-final','tests-asan','tests-reference-release','tests-control-release') {
  $xmlPath=Join-Path $script:X4Batch "$name.xml";[xml]$xml=[IO.File]::ReadAllText($xmlPath)
  $binary=Join-Path $script:X4Repo $(switch($name){'tests-asan' {'out/x4/build/Debug/x1_tests.exe'};'tests-reference-release' {'out/x2/tools36/x1_tests.exe'};'tests-control-release' {'out/x2/tools50/x1_tests.exe'};default {'out/x4/tools/x1_tests.exe'}})
  $tests+=@{name=$name;tests=[int]$xml.testsuites.tests;failures=[int]$xml.testsuites.failures;disabled=[int]$xml.testsuites.disabled;skipped=@($xml.testsuites.testsuite.testcase|Where-Object {$null -ne $_.skipped}).Count;errors=[int]$xml.testsuites.errors;binary_path=$binary;binary_sha256=(X4-Sha $binary);xml_sha256=(X4-Sha $xmlPath);log_sha256=(X4-Sha (Join-Path $script:X4Batch "logs/$name.log"))}
}
$neg=Get-Content (Join-Path $script:X4Batch 'cli-negative-final.json') -Raw|ConvertFrom-Json
X4-Json (Join-Path $script:X4Batch 'delivery-summary.json') @{status='development delivery; pending controller independent acceptance';handoff='docs/PRECONFIRMATION_ROLE_ORACLE_X4_HANDOFF_20261002.md';causal_report_sha256=(X4-Sha (Join-Path $script:X4Batch 'causal-report.json'));query_manifest_sha256=(X4-Sha (Join-Path $script:X4Batch 'query-manifest.json'));first_state_divergence=$r.first_raw_state_divergence;first_action_divergence=$r.first_action_divergence_ignoring_note_intent_ids;tests=$tests;CLI_negative_count=$neg.case_count;CLI_negative_failures=$neg.failures;full_variant_replays=2;new_live=0;mechanism_mutation='selected pointer at confirmed==0 only';completed_unknown_down_tests='original and new direct-owner guards retained';unknowns=@('physical identity/judgment role gold','gameplay acceptance/Miss','source render age and original perception race','real recognition/RPC latency','cross-song generalization')}
$outRoot=Join-Path $script:X4Repo 'out/x4';$outFiles=@();foreach($f in Get-ChildItem -LiteralPath $outRoot -File -Recurse){$outFiles+=@{path=$f.FullName.Substring($outRoot.Length+1).Replace('\','/');bytes=$f.Length}}
$files=@();foreach($f in Get-ChildItem -LiteralPath $script:X4Batch -File -Recurse){$files+=@{path=$f.FullName.Substring($script:X4Batch.Length+1).Replace('\','/');bytes=$f.Length;sha256=(X4-Sha $f.FullName)}}
$base=X4-Bytes $script:X4Batch;$campaignBase=(X4-Bytes $script:X4Campaign)+(X4-Bytes $script:X4Prior)
$ledger=@{schema=1;batch_limit_bytes=25165824;batch_bytes=0;batch_remaining_bytes=0;campaign_plus_prior_limit_bytes=8589934592;campaign_plus_prior_bytes=0;campaign_plus_prior_remaining_bytes=0;new_batch_bytes=0;old_batches_bytes=$sizes;prior_bytes=(X4-Bytes $script:X4Prior);files=$files;self=@{path='capacity-ledger.json';bytes=0;sha256=$null;reason='self digest omitted to avoid recursion; own exact length included'};out_x4_bytes=(X4-Bytes $outRoot);out_x4_files=$outFiles;out_scope='export/build/frozen binaries separately accounted; all experiment artifacts stay in batch'}
$n=[long]0
for($i=0;$i -lt 10;$i++) {
  $ledger.self.bytes=$n;$ledger.batch_bytes=$base+$n;$ledger.new_batch_bytes=$ledger.batch_bytes;$ledger.batch_remaining_bytes=25165824-$ledger.batch_bytes;$ledger.campaign_plus_prior_bytes=$campaignBase+$n;$ledger.campaign_plus_prior_remaining_bytes=8589934592-$ledger.campaign_plus_prior_bytes
  $text=$ledger|ConvertTo-Json -Depth 80;$next=[Text.UTF8Encoding]::new($false).GetByteCount($text)
  if($next -eq $n){break};$n=$next
}
if($next -ne $ledger.self.bytes){throw 'Ledger size did not converge'}
X4-Write (Join-Path $script:X4Batch 'capacity-ledger.json') $text
if((X4-Bytes $script:X4Batch) -ne $ledger.batch_bytes -or ((X4-Bytes $script:X4Campaign)+(X4-Bytes $script:X4Prior)) -ne $ledger.campaign_plus_prior_bytes){throw 'Ledger final bytes mismatch'}
Write-Output ($ledger|Select-Object batch_bytes,batch_remaining_bytes,campaign_plus_prior_bytes,campaign_plus_prior_remaining_bytes,out_x4_bytes|ConvertTo-Json)
