. "$PSScriptRoot/x4-budget.ps1"
if(Test-Path -LiteralPath $script:X4Batch){throw 'X4 batch must be new'}
X4-Check 25165824
$protected=@()
foreach($root in 'src','include','apps/frame_review','tests','tools','docs','out/x1/main50-v2','out/x1/c36h-v3','out/x2/main50-winner-only','out/x2/tools36','out/x2/tools50','out/x3/build/Release') {
  foreach($f in Get-ChildItem -LiteralPath (Join-Path $script:X4Repo $root) -File -Recurse) {
    $relative=$f.FullName.Substring($script:X4Repo.Length+1).Replace('\','/')
    if($relative -like 'tools/*x4*'){continue}
    $protected+=@{path=$relative;bytes=$f.Length;sha256=(X4-Sha $f.FullName)}
  }
}
foreach($root in 'contact-replay-x1','confirmed-preserve-x2','line-role-x3') {
  foreach($f in Get-ChildItem -LiteralPath (Join-Path $script:X4Campaign $root) -File -Recurse) {
    $protected+=@{path=$f.FullName.Substring($script:X4Repo.Length+1).Replace('\','/');bytes=$f.Length;sha256=(X4-Sha $f.FullName)}
  }
}
$before=@{head=(git rev-parse HEAD);branch=(git branch --show-current);worktrees=@(git worktree list --porcelain);status=@(git status --short);strategy='observer50/planner27/diagnostics11';formal_diff=@(git diff -- src include CMakeLists.txt);protected=$protected;campaign_bytes=(X4-Bytes $script:X4Campaign);prior_bytes=(X4-Bytes $script:X4Prior);x1_bytes=(X4-Bytes (Join-Path $script:X4Campaign 'contact-replay-x1'));x2_bytes=(X4-Bytes (Join-Path $script:X4Campaign 'confirmed-preserve-x2'));x3_bytes=(X4-Bytes (Join-Path $script:X4Campaign 'line-role-x3'))}
X4-Json (Join-Path $script:X4Batch 'workspace-before.json') $before
X4-Write (Join-Path $script:X4Batch 'status-before.md') ([IO.File]::ReadAllText((Join-Path $script:X4Repo 'docs/PROJECT_STATUS_NEXT_STEPS_20261001.md')))
X4-Write (Join-Path $script:X4Batch 'experiment-spec-before.md') @'
# X4 pre-confirmation proposed-role oracle — frozen before implementation
Hypothesis: replacing only provisional line winner for the D current Drag core while confirmed_line_id==0 may prevent the horizontal relation chain and allow a supported vertical relation → fresh root or original current overlap → plan → Down. Failure locates the first remaining original guard. This is proposed/unknown role, never human/expert gold or gameplay validation.
Insertion: after the original score loop, immediately before the original confirmed-preserve block. Only selected pointer can change. best_score/second_score, identity/relation ambiguity, confirmed preserve assignment and flag, conflict/continuation, history/confirmation/motion, owner/lease/scheduler and clocks remain unchanged. X2 disabled winner override is never enabled.
Eligibility: D ordinal 6158–6220 only, Drag only, confirmed==0; packet binds ordinal/source-frame/PNG SHA plus same-frame core geometry, and proposed vertical current candidate geometry from accepted original main50 control. Match no runtime ID. Candidate must be current (observed_ns==now), association-valid and pass original length/extent qualification; exactly one matching note and line required. Missing/nonunique geometry means unknown/no intervention. Original identity ambiguity may still reject the selected pointer. No synthetic lines, identity oracle, recorded action, future feedback/history, control state injection, or second intervention.
Packet: generated from same-frame accepted main50 current target/current bank geometry, reviewed against cited RGB. Oracle output is frozen before replay; future frames can be reviewed only as outcome. 6174 continuity remains proposed and identity ambiguity is retained. A packet may choose nothing.
Controls: reuse C36h reference and main50 control frozen X2 runs with explicit old-manifest bridge and shared input/clock/receipt policy. Full pre-intervention canonical state/action equality against main50. A/B/C/E selected windows and G1533–1537 crossing context; full-prefix canonical events/digests include owner/contact, scheduler events and receipts. Existing original crossing gold and tracking/owner synthetic regressions retained.
Runs: at most TWO variant full-prefix→EOF replays: first trace ON, second trace OFF. Both run original recorded owner cadence plus ≤32 preroll, frame-first, zero fake recognition/RPC delay, original input/profile. Canonical states and events must be byte-identical between runs; trace affects diagnostics only. No full control recomputation planned. All attempts including failures retained.
Success: vertical current relation reaches original root/overlap qualification, plan and Down with natural identity/history and no completed/unknown Down revival. This supports early choice causal sufficiency on fixed pixels only. Falsifier: any original guard prevents that chain; record its earliest boundary and stop. No threshold tuning or additional mechanism after outcome.
Capacity: batch cap 25,165,824 B (24MiB), campaign+prior cap 8,589,934,592 B. Reserve two runs total 18MiB (ON ≤12MiB; OFF ≤6MiB), source/packet/spec/validation/logs/ledger ≤6MiB. Old PNG/trace referenced, not copied. Existing named writer mutex used for every evidence write/append; hard failure rather than truncation. out/x4 export/build separately accounted, never stores experiment evidence.
'@
Write-Output ($before | Select-Object head,branch,campaign_bytes,prior_bytes,x1_bytes,x2_bytes,x3_bytes | ConvertTo-Json)
