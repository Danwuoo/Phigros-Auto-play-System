$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/../..").Path
$campaignPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue"
$batchPath="$campaignPath/hold-ownership-x10d-o"
$outPath="$repoPath/out/x10d-o"
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){if(!(Test-Path -LiteralPath $p)){return 0};[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$v){[IO.File]::WriteAllText("$batchPath/$name",($v|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))}
if((Test-Path "$batchPath/workspace-before.json") -or @(Get-ChildItem $outPath -File -Recurse -ErrorAction SilentlyContinue).Count -gt 0){throw 'initialized roots cannot overwrite'}
$existing=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")
$free=(Get-PSDrive C).Free
if($existing+268435456 -gt 8589934592 -or $free -lt 268435456+1073741824+5368709120){throw 'capacity'}
New-Item -ItemType Directory $batchPath,$outPath -Force|Out-Null
$files=@();foreach($p in @(git -c core.quotepath=false ls-files --cached --others --exclude-standard)){
 if($p -like 'tools/x10d_o/*' -or $p -like 'research/x10d_o/*' -or $p -like 'docs/HOLD_OWNERSHIP_X10D_O_*'){continue}
 $files+=@{path=$p;bytes=(Get-Item -LiteralPath "$repoPath/$p").Length;sha256=Hash "$repoPath/$p"}
}
Save 'workspace-before.json' @{head=(git rev-parse HEAD);status=@(git status --porcelain=v1);worktrees=@(git worktree list);index_sha256=Hash "$repoPath/.git/index";protected=$files;campaign_bytes=Bytes $campaignPath;prior_bytes=Bytes "$repoPath/measurements/research-next-20261001";aggregate_before=$existing;disk_free=$free;scope='All pre-existing Git tracked/untracked files; ignored raw protected through selected inputs and frozen bindings. No assertion of exhaustive ignored-raw rehash.'}
$prior="$campaignPath/hold-cascade-x10c"
$binding=Get-Content "$prior/source-binding-before-replay.json" -Raw|ConvertFrom-Json
$checked=@()
foreach($f in $binding.export_files){$p="$repoPath/out/x10c/$($f.role)/$($f.path)";if((Hash $p) -ne $f.sha256){throw "frozen export $p"};$checked+=@{path=$p;sha256=$f.sha256}}
foreach($f in $binding.binaries){$p="$repoPath/$($f.path)";if((Hash $p) -ne $f.sha256){throw "frozen binary $p"};$checked+=@{path=$p;sha256=$f.sha256}}
$on=Get-Content "$prior/baseline-on-1/summary.json" -Raw|ConvertFrom-Json
$off=Get-Content "$prior/baseline-off-1/summary.json" -Raw|ConvertFrom-Json
foreach($name in @('events.jsonl','state-digests.jsonl')){if((Hash "$prior/baseline-on-1/$name") -ne (Hash "$prior/baseline-off-1/$name")){throw 'trace bridge'}}
if($on.semantic_sha256 -ne $off.semantic_sha256 -or $on.verified_pngs -ne 7722 -or $on.consumed_frames -ne 7715 -or $on.contacts_at_exit -ne 0){throw 'baseline denominator'}
$parent=Get-Content "$prior/baseline-provenance.json" -Raw|ConvertFrom-Json
$x1=Get-Content "$campaignPath/contact-replay-x1/source-v2/c36h-source-provenance.json" -Raw|ConvertFrom-Json
$bridge=@();foreach($e in $x1.instrumented_source_sha256.PSObject.Properties){$p="$repoPath/out/x1/c36h-v3/$($e.Name)";if((Hash $p) -ne $e.Value){throw 'X1 parent'};$bridge+=@{path=$p;sha256=$e.Value}}
Save 'parent-binding.json' @{x10c_checked=$checked;x1_checked=$bridge;binding_sha256=Hash "$prior/source-binding-before-replay.json";baseline_on_sha256=Hash "$prior/baseline-on-1/summary.json";events_sha256=Hash "$prior/baseline-on-1/events.jsonl";state_sha256=Hash "$prior/baseline-on-1/state-digests.jsonl";semantic_sha256=$on.semantic_sha256;historical_baseline_trace_equal=$true;new_replay_executed=$false;suppression=$false;pending_hook=$false}
foreach($role in @('reference','candidate')){
 New-Item -ItemType Directory "$outPath/$role"|Out-Null
 Copy-Item -LiteralPath "$repoPath/out/x10c/baseline/src","$repoPath/out/x10c/baseline/include","$repoPath/out/x10c/baseline/tests" -Destination "$outPath/$role" -Recurse
 $entries=@();foreach($f in Get-ChildItem "$outPath/$role" -File -Recurse){$rel=$f.FullName.Substring(("$outPath/$role").Length+1).Replace('\','/');$p="$repoPath/out/x10c/baseline/$rel";if((Hash $f.FullName) -ne (Hash $p)){throw 'copy mismatch'};$entries+=@{path=$rel;bytes=$f.Length;sha256=Hash $f.FullName}}
 Save "$role-export.json" @{root="$outPath/$role";parent='out/x10c/baseline';files=$entries;runtime_policy_modified=$false;hypothesis='BCC-v1 isolated helper; integration contingent on all contracts'}
}
Copy-Item -LiteralPath "$repoPath/docs/HOLD_OWNERSHIP_X10D_O_PROTOCOL_20261004.md" -Destination "$batchPath/protocol-before.md"
$m=Get-Content "$prior/input-manifest.json" -Raw|ConvertFrom-Json
$recording="$($m.session_root)/full-recording"
if((Hash "$recording/index.jsonl") -ne $m.index_sha256){throw 'index'}
$selected=@();$lineNumber=0
foreach($line in [IO.File]::ReadLines("$recording/index.jsonl")){
 $row=$line|ConvertFrom-Json
 if($line.Length -gt 2097152 -or ++$lineNumber -gt 36000){throw 'index cap'}
 $isSelected=$false;foreach($w in $m.windows){if($row.ordinal -ge $w.first -and $row.ordinal -le $w.last){$isSelected=$true}}
 if($isSelected){$p="$recording/$($row.path)";if((Hash $p) -ne $row.png_sha256){throw "png $p"};$selected+=@{ordinal=$row.ordinal;source_frame=$row.source_frame;capture_complete_ns=$row.capture_complete_ns;pixels_ready_ns=$row.pixels_ready_ns;source_timestamp_us=$row.source_timestamp_us;clock_domain='host_qpc_ns';path=$p;png_sha256=$row.png_sha256}}
}
if($selected.Count -ne 196){throw "packet count $($selected.Count)"}
Save 'rgb-packet.json' @{schema=1;human_gold=0;recording=$recording;index_sha256=$m.index_sha256;index_rows=$lineNumber;frames=$selected;windows=$m.windows;trace_path="$prior/baseline-on-1/trace.jsonl";trace_sha256=Hash "$prior/baseline-on-1/trace.jsonl";witness_path="$prior/baseline-on-1/fallback-witness.jsonl";witness_sha256=Hash "$prior/baseline-on-1/fallback-witness.jsonl";physical_ownership='unknown';prefix='Read original verified full-prefix selected trace; no new window observer/owner warm start'}
Save 'read-command-failures.json' @{failures=@('Read absent out/x10c/baseline/CMakeLists.txt and provenance.json; correct files in apps/frame_review/x10c_offline and prior batch','Read absent baseline-on-1/frames.jsonl; actual selected file trace.jsonl','Read absent full-recording/round-1; actual PNG root full-recording/frames');impact='read-only path discovery errors; no build/replay/policy mutation'}
Write-Output "prepared protected=$($files.Count) png=$($selected.Count) aggregate=$existing free=$free"
