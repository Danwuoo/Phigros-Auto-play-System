$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'hold-causal-x9'
function Hash($path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($path){[long](Get-ChildItem -LiteralPath $path -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$value){$bytes=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Depth 40)+"`n");if((Bytes $batchPath)+$bytes.Length -gt 134217728){throw 'quota'};$s=[IO.File]::Open("$batchPath/$name",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$s.Write($bytes,0,$bytes.Length)}finally{$s.Dispose()}}
$mutex=[Threading.Mutex]::new($false,'Local\PAS_X1ContactReplayBudget');$owned=$false
try{
  $owned=$mutex.WaitOne(0);if(!$owned){throw 'batch_writer_busy'}
  $checks=@(foreach($lineage in @('c36h','main50')){
    $r=Get-Content "$batchPath/$lineage-on-1/summary.json" -Raw|ConvertFrom-Json
    $old="$campaignPath/contact-replay-x1/$lineage-final-on-1"
    $original=Get-Content "$old/summary.json" -Raw|ConvertFrom-Json
    if(!$r.success -or $r.input_frames -ne 7722 -or $r.verified_pngs -ne 7722 -or $r.consumed_frames -ne 7715 -or $r.window_frames -ne 82 -or $r.contacts_at_exit -ne 0 -or $r.semantic_sha256 -ne $original.semantic_sha256){throw 'run invariants'}
    if((Hash "$batchPath/$lineage-on-1/events.jsonl") -ne (Hash "$old/events.jsonl")){throw 'events changed'}
    # Retained A/B/C/E trace rows must be identical, including diagnostics.
    $oldRows=@(Get-Content "$old/trace.jsonl"|Where-Object { $n=($_|ConvertFrom-Json).ordinal;!($n -ge 6158 -and $n -le 6220) })
    $newRows=@(Get-Content "$batchPath/$lineage-on-1/trace.jsonl"|Where-Object { $n=($_|ConvertFrom-Json).ordinal;!($n -ge 2460 -and $n -le 2495) })
    if($oldRows.Count -ne 46 -or $newRows.Count -ne 46 -or ($oldRows -join "`n") -cne ($newRows -join "`n")){throw 'control trace changed'}
    @{lineage=$lineage;summary_sha256=(Hash "$batchPath/$lineage-on-1/summary.json");trace_sha256=(Hash "$batchPath/$lineage-on-1/trace.jsonl");full_events_sha256=(Hash "$batchPath/$lineage-on-1/events.jsonl");semantic_sha256=$r.semantic_sha256;retained_control_rows_byte_identical=46;contacts_at_exit=0}
  })
  $formal=@(git -C $repoPath diff --name-only -- src include CMakeLists.txt);if($formal.Count){throw 'formal source changed'}
  if(Test-Path "$batchPath/source"){throw 'snapshot exists'}
  New-Item -ItemType Directory "$batchPath/source"|Out-Null
  $sources=@(foreach($relative in @('tools/hold-causal-x9.ps1','tools/audit-hold-causal-x9.ps1','docs/HOLD_CAUSAL_X9_20261003.md','docs/C36H_FORWARD_EXECUTION_PLAN_20261003.md','docs/PROJECT_STATUS_NEXT_STEPS_20261001.md')){
    $dest="$batchPath/source/$($relative.Replace('/','__'))";Copy-Item -LiteralPath "$repoPath/$relative" -Destination $dest
    @{path=$relative;sha256=(Hash $dest);snapshot=$dest}
  })
  Save 'source-freeze.json' @{sources=$sources;production_diff=$formal;frozen_reader_manifest_sha256=(Hash "$campaignPath/contact-replay-x1/repair-r1-r2-20261002/tool-freeze.json")}
  $summary=[ordered]@{schema=1;head=(git -C $repoPath rev-parse HEAD);author_and_validator='controller direct offline research; not independent-agent acceptance';new_replays=2;new_build=0;new_live=0;new_training=0;human_gold=0;production_changed=$false;checks=$checks;source_freeze_sha256=(Hash "$batchPath/source-freeze.json");finding_verified='C36h521 existing contact persists;539 new Down2478 and ambiguity Up2479; main50523 persists and541 no new Down';finding_strong_inference='same visible Hold body yields extra approaching fragment; physical identity not gold';next_hypothesis='selective main50 fallback rail-claim donor, with separated-body counterexample';batch_bytes=0;batch_limit_bytes=134217728;batch_remaining_bytes=0;campaign_plus_prior_bytes=0;campaign_limit_bytes=8589934592;campaign_remaining_bytes=0}
  $before=Bytes $batchPath;$total=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")
  for($i=0;$i -lt 8;++$i){$n=[Text.Encoding]::UTF8.GetByteCount(($summary|ConvertTo-Json -Depth 40)+"`n");$summary.batch_bytes=$before+$n;$summary.batch_remaining_bytes=134217728-$summary.batch_bytes;$summary.campaign_plus_prior_bytes=$total+$n;$summary.campaign_remaining_bytes=8589934592-$summary.campaign_plus_prior_bytes}
  if($summary.campaign_remaining_bytes -lt 0){throw 'campaign quota'}
  Save 'final-summary.json' $summary
  if((Bytes $batchPath) -ne $summary.batch_bytes){throw 'ledger mismatch'}
  Write-Output "X9 complete:92 control rows preserved;2 full-prefix digests/events preserved;batch=$($summary.batch_bytes)"
}finally{if($owned){$mutex.ReleaseMutex()};$mutex.Dispose()}
