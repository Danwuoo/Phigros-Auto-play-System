. "$PSScriptRoot/x2-budget.ps1"
$before=Get-Content (Join-Path $script:X2Batch 'workspace-before.json') -Raw|ConvertFrom-Json
$mismatch=@()
foreach($p in @($before.protected+$before.formal)){if(!(Test-Path -LiteralPath $p.path) -or (X2-Sha $p.path) -cne $p.sha256){$mismatch+=$p.path}}
if($mismatch.Count){throw "Protected sources/evidence changed: $($mismatch -join ', ')"}
$status=Join-Path $script:X2Repo 'docs/PROJECT_STATUS_NEXT_STEPS_20261001.md'
$prefix=[IO.File]::ReadAllText((Join-Path $script:X2Batch 'status-before-append.md'))
if(![IO.File]::ReadAllText($status).StartsWith($prefix,[StringComparison]::Ordinal)){throw 'Status historical prefix changed'}
$prefixBytes=[IO.File]::ReadAllBytes((Join-Path $script:X2Batch 'status-before-append.md'));$statusBytes=[IO.File]::ReadAllBytes($status)
if($statusBytes.Length -lt $prefixBytes.Length){throw 'Status byte prefix missing'}
for($i=0;$i -lt $prefixBytes.Length;$i++){if($statusBytes[$i] -ne $prefixBytes[$i]){throw 'Status historical byte prefix changed'}}
$manifest=Get-Content (Join-Path $script:X2Batch 'input-manifest.json') -Raw|ConvertFrom-Json
if((X2-Sha (Join-Path $script:X2Batch 'status-before-append.md')) -cne $manifest.status_working_sha256){throw 'Status prefix snapshot was not original manifest-bound bytes'}
if((X2-Bytes (Join-Path $script:X2Campaign 'contact-replay-x1')) -ne 130356292){throw 'X1 batch bytes changed'}
$formalDiff=@(git diff --name-only -- src include CMakeLists.txt)
if($formalDiff.Count){throw 'Formal Git diff changed'}
X2-Json (Join-Path $script:X2Batch 'preservation-check.json') @{protected_checks=$before.protected.Count;formal_checks=$before.formal.Count;mismatches=0;x1_bytes_unchanged=$true;status_prior_bytes_and_text_preserved=$true;status_prefix_original_SHA_verified=$true;root_CMake_sha256=(X2-Sha (Join-Path $script:X2Repo 'CMakeLists.txt'));head=(git rev-parse HEAD);branch=(git branch --show-current);worktrees=@(git worktree list);formal_git_diff=$formalDiff}
$sourceFiles=@('apps/frame_review/main.cpp','apps/frame_review/contact_replay.cpp','apps/frame_review/contact_replay.hpp','apps/frame_review/comparison_input.hpp','apps/frame_review/replay_trace.hpp','apps/frame_review/replay_support.hpp','apps/frame_review/review_io.hpp','apps/frame_review/subject_prefix_report.cpp','apps/frame_review/x2_ablation.hpp','apps/frame_review/x2_input.hpp','apps/frame_review/x2_report.cpp','apps/frame_review/offline/CMakeLists.txt','tests/contact_replay_tests.cpp','tests/x2_ablation_tests.cpp')
$sourceFiles+=@(Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*confirmed-preserve-x2.ps1' | ForEach-Object {"tools/$($_.Name)"})
$sourceFiles+='tools/x2-budget.ps1';$sources=@()
foreach($p in $sourceFiles) {
  $absolute=Join-Path $script:X2Repo $p;$snapshot=Join-Path $script:X2Batch "tool-source/$p"
  X2-Write $snapshot ([IO.File]::ReadAllText($absolute))
  if((X2-Sha $absolute) -cne (X2-Sha $snapshot)){throw "Source byte snapshot mismatch $p"}
  $sources+=@{path=$p;sha256=(X2-Sha $absolute);snapshot=$snapshot}
}
$products=@();foreach($root in @('out/x2/tools36','out/x2/tools50','out/x2/build36/Debug','out/x2/build50/Debug')){foreach($f in Get-ChildItem -LiteralPath (Join-Path $script:X2Repo $root) -File){if($f.Extension -in '.exe','.dll'){$products+=@{path=$f.FullName;bytes=$f.Length;sha256=(X2-Sha $f.FullName)}}}}
$exports=@();foreach($r in @('out/x1/c36h-v3','out/x2/main50-winner-only')) {foreach($f in Get-ChildItem -LiteralPath (Join-Path $script:X2Repo $r) -File -Recurse){if($f.FullName -match '\\(src|include|tests)\\'){$exports+=@{path=$f.FullName;bytes=$f.Length;sha256=(X2-Sha $f.FullName)}}}}
$tests=@();foreach($f in Get-ChildItem -LiteralPath $script:X2Batch -Filter 'tests*.xml') {
  [xml]$xml=Get-Content -LiteralPath $f.FullName -Raw
  $failed=@();foreach($suite in $xml.testsuites.testsuite){foreach($case in $suite.testcase){if($case.failure){$failed+=@{name="$($case.classname).$($case.name)";message=@($case.failure | ForEach-Object {$_.message})}}}}
  $tests+=@{path=$f.Name;sha256=(X2-Sha $f.FullName);tests=[int]$xml.testsuites.tests;failures=[int]$xml.testsuites.failures;disabled=[int]$xml.testsuites.disabled;skipped=@($xml.SelectNodes('//testcase/skipped')).Count;failed_cases=$failed}
}
X2-Json (Join-Path $script:X2Batch 'tool-freeze.json') @{client_date='2026-10-02';timezone='Asia/Taipei';sources=$sources;binaries=$products;exports=$exports;tests=$tests;intervention='winner assignment only; original preserve flag and ambiguity bypass retained';same_ablation_capable_main50_binary_serves_control_and_variant=$true;new_full_replays=5;new_live_rounds=0;acceptance='development self-check; awaiting independent orchestrator acceptance'}
$out=@();foreach($d in Get-ChildItem -LiteralPath (Join-Path $script:X2Repo 'out/x2') -Directory){$out+=@{path=$d.FullName;bytes=(X2-Bytes $d.FullName)}}
$artifacts=@();foreach($f in Get-ChildItem -LiteralPath $script:X2Batch -File -Recurse){$artifacts+=@{path=$f.FullName.Substring($script:X2Batch.Length+1).Replace('\','/');bytes=$f.Length;sha256=(X2-Sha $f.FullName)}}
$baseBatch=X2-Bytes $script:X2Batch;$baseCampaign=X2-Bytes $script:X2Campaign;$prior=X2-Bytes $script:X2Prior
$ledger=@{batch_limit_bytes=[long]67108864;campaign_limit_bytes=[long]8589934592;batch_before_bytes=0;x1_preserved_bytes=[long]130356292;campaign_before_bytes=[long]7762085288;prior_research_bytes=$prior;batch_after_bytes=0;campaign_after_bytes=0;campaign_plus_prior_after_bytes=0;batch_remaining_bytes=0;campaign_remaining_bytes=0;out_x2=$out;artifacts=$artifacts;ledger_self_bytes=0;ledger_self_sha256=$null;reason_self_SHA_omitted='avoid recursive self-hash; own bytes included';failures_and_all_attempts_retained=$true;raw_PNG_copies=0}
$encoded='';for($i=0;$i -lt 12;$i++) {
  $encoded=$ledger|ConvertTo-Json -Depth 60;$bytes=[Text.UTF8Encoding]::new($false).GetByteCount($encoded)
  if($ledger.ledger_self_bytes -eq $bytes){break}
  $ledger.ledger_self_bytes=$bytes;$ledger.batch_after_bytes=$baseBatch+$bytes;$ledger.campaign_after_bytes=$baseCampaign+$bytes;$ledger.campaign_plus_prior_after_bytes=$baseCampaign+$prior+$bytes;$ledger.batch_remaining_bytes=67108864-$ledger.batch_after_bytes;$ledger.campaign_remaining_bytes=8589934592-$ledger.campaign_plus_prior_after_bytes
}
X2-Write (Join-Path $script:X2Batch 'capacity-ledger.json') $encoded
if((X2-Bytes $script:X2Batch) -ne $ledger.batch_after_bytes){throw 'Ledger self bytes mismatch'}
Write-Output "X2 batch=$($ledger.batch_after_bytes)/67108864; remaining=$($ledger.batch_remaining_bytes); campaign+prior=$($ledger.campaign_plus_prior_after_bytes)."
