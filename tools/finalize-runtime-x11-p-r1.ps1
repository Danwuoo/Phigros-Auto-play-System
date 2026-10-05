$ErrorActionPreference='Stop';$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue";$batchPath="$campaignPath/runtime-x11-p-r1";$outPath="$repoPath/out/x11-p-r1"
. "$PSScriptRoot/collect-runtime-x11-p-r1-tests.ps1"
function Hash($p){(Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($n,$v){[IO.File]::WriteAllText("$batchPath/$n",($v|ConvertTo-Json -Depth 80)+"`n",[Text.UTF8Encoding]::new($false))}
if(Test-Path "$batchPath/final-summary.json"){throw 'already frozen'}
$counts=@();foreach($n in @('source-binding-before-cost','compiled-dependency-freeze','negative-tool-freeze')){
 $b=Get-Content "$batchPath/$n.json" -Raw|ConvertFrom-Json
 foreach($f in $b.files){if((Hash $f.path) -ne $f.sha256){throw "freeze altered $($f.path)"}}
 $counts+=@{name=$n;entries=$b.files.Count;all_SHA_match=$true}
}
$tests=@();foreach($f in Get-ChildItem $batchPath -Filter 'tests-*.xml'){$tests+=@{name=$f.BaseName;result=Get-R1TestResult $f.FullName}}
foreach($t in $tests){if($t.result.skipped -or $t.result.errors -or $t.result.disabled){throw 'unexpected skip/error/disabled'}}
$expected=@{'tests-B0-original'=0;'tests-B1-original'=1;'tests-B0-pending'=5;'tests-B1-pending'=0;'tests-B0-r1'=0;'tests-B1-r1'=0;'tests-asan-r1'=0;'tests-asan-pending'=0}
foreach($t in $tests){if($t.result.failures -ne $expected[$t.name]){throw "test failures $($t.name)"}}
if(($tests|Where-Object name -eq 'tests-B1-original').result.failed_names[0] -ne 'GameOwner.SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt'){throw 'regression classification'}
$runs=@();foreach($dir in Get-ChildItem $batchPath -Directory|Where-Object Name -match '^(aa-|ab-|stress-)'){$runs+=@{name=$dir.Name;summary=Get-Content "$($dir.FullName)/summary.json" -Raw|ConvertFrom-Json;summary_sha256=Hash "$($dir.FullName)/summary.json";bytes=Bytes $dir.FullName}}
$normal=@($runs|Where-Object name -Like 'aa-*');$stress=@($runs|Where-Object name -Like 'stress-*');$ab=@($runs|Where-Object name -Like 'ab-*')
if($normal.Count -ne 8 -or $stress.Count -ne 4 -or $ab.Count -ne 0){throw 'negative stop run counts'}
foreach($run in $normal){if($run.bytes -gt 4MB){throw 'normal output capacity'}};foreach($run in $stress){if($run.bytes -gt 7MB){throw 'stress output capacity'}}
$noise=Get-Content "$batchPath/noise-frozen.json" -Raw|ConvertFrom-Json;if($noise.noise_gate){throw 'review this finalizer if successful; no invented ABBA result'}
Save 'all-cost-runs.json' $runs
$workspace=Get-Content "$batchPath/workspace-before.json" -Raw|ConvertFrom-Json
$dirty=@();foreach($f in $workspace.dirty_files){
 if($f.path -match 'runtime.x11.p.r1|RUNTIME_X11_P_R1|PROJECT_STATUS_NEXT_STEPS|C36H_FORWARD_EXECUTION'){continue}
 $same=(Hash "$repoPath/$($f.path)") -eq $f.sha256;if(!$same){throw "prior dirty changed $($f.path)"};$dirty+=@{path=$f.path;sha256=$f.sha256;retained=$true}
}
if(@(git diff --name-only HEAD -- src include CMakeLists.txt).Count){throw 'formal checkout altered'}
if((git rev-parse HEAD) -ne $workspace.head -or (git branch --show-current) -ne $workspace.branch){throw 'git head changed'}
Save 'workspace-after.json' @{head=(git rev-parse HEAD);branch=(git branch --show-current);worktrees=@(git worktree list);status=@(git status --short);prior_dirty=$dirty;formal_source_unchanged=$true;strategy=(Get-Content "$repoPath/include/pas/strategy_version.hpp" -Raw)}
$oldLedger=Get-Content "$campaignPath/runtime-x11-p/artifact-ledger.json" -Raw|ConvertFrom-Json;$oldChecks=0
foreach($f in $oldLedger.files){if((Hash $f.path) -ne $f.sha256 -or (Get-Item -LiteralPath $f.path).Length -ne $f.bytes){throw "old artifact altered $($f.path)"};++$oldChecks}
$original="$campaignPath/acceptance36h-01/candidate36h-tint1-runtime/pas.exe";if((Hash $original) -ne '61ecfacd2cc48ed270506719a1089be646d915c64e9026706ac1654e0d4bbce9'){throw 'old live SHA'}
Save 'old-artifacts-after.json' @{checked=$oldChecks;all_equal=$true;original_live_sha256=Hash $original;scope='old final artifact ledger, no original unlisted compiled SHA claim'}
$sourceSnapshot="$batchPath/source-snapshot";New-Item -ItemType Directory $sourceSnapshot|Out-Null
foreach($f in Get-ChildItem "$repoPath/tools" -File|Where-Object Name -Match 'runtime-x11-p-r1'){Copy-Item $f.FullName $sourceSnapshot}
Copy-Item "$repoPath/apps/runtime_x11_p_r1" "$sourceSnapshot/apps" -Recurse
Copy-Item "$repoPath/docs/RUNTIME_X11_P_R1_PROTOCOL_20261003.md","$repoPath/docs/RUNTIME_X11_P_R1_RESULT_20261003.md","$repoPath/docs/RUNTIME_X11_P_R1_HANDOFF_20261003.md" $sourceSnapshot
foreach($r in @('B0','B1')) {
 $archive="$outPath/$r-source.tar";& tar -cf $archive -C "$outPath/$r" src include apps cmake proto tests configs third_party CMakeLists.txt x11.cmake vcpkg.json vcpkg-configuration.json
 if($LASTEXITCODE){throw 'source archive'}
 Save "source-archive-$r.json" @{command=@('tar','-cf',$archive,'-C',"$outPath/$r",'src','include','apps','cmake','proto','tests','configs','third_party','CMakeLists.txt','x11.cmake','vcpkg.json','vcpkg-configuration.json');sha256=Hash $archive;bytes=(Get-Item $archive).Length;historical_PNG='junction references excluded'}
}
$artifactRows=@();foreach($root in @($batchPath,$outPath)){foreach($f in Get-ChildItem $root -File -Recurse){if($f.Name -in @('artifact-ledger.json','final-summary.json')){continue};$artifactRows+=@{path=$f.FullName;bytes=$f.Length;sha256=Hash $f.FullName}}}
Save 'artifact-ledger.json' @{files=$artifactRows;excluded_self=@('artifact-ledger.json','final-summary.json');PNG_references_not_traversed=$true}
$inverse=Get-Content "$batchPath/source-inverse-audit.json" -Raw|ConvertFrom-Json
$runtime=Get-Content "$batchPath/runtime-metadata-verified.json" -Raw|ConvertFrom-Json
$summary=@{schema=1;state='not-ready';delivery='交付完成，待總控獨立驗收';controller_accepted=$false;ready_for_live=$false;head=$workspace.head;branch=$workspace.branch;strategy='main50/27/11 donor; C36h behavioral baseline';only_decision_delta='8-line pending hook';suppression=$false;cost_runs=8;stress_runs=4;ABBA_runs=0;additional_cost_runs=0;cost_gate='not-evaluable: A/A gate failed, ABBA forbidden';noise_gate=$false;
 blockers=@('A/A noise inadequate: RGB recognition p95 and capture-to-owner p99; owner accept p99 and capture-to-owner p95','aa-owner-1 owner coverage114/128=89.0625% below90%','ASan stress-owner-B1 all-phase receipt n0; timing/active dispatch coverage Unknown, not passed latency','no controlled B1 normal gate costs; pending-only cost qualification Unknown');
 corrected_contracts=@('shared original Wake/high-resolution timer','same compiled full SessionArchive and event JSON','lead35','testcase skip regression','empty/missing metric rejects','complete serialized-row verification','each fake release call/result/raw negatives','native512MiB capacity plan arithmetic+scaled hard bound');
 source_inverses=$inverse.core_files.Count;freeze_verification=$counts;old_artifacts_rehashed=$oldChecks;tests=$tests;normal_runs=@($normal|ForEach-Object name);stress_runs_detail=@($stress|ForEach-Object name);full_recorded_replays=0;synthetic_bridge_inputs_per_role=512;bridge_byte_equal=$true;new_runtime=$runtime;profile_parent_sha256=Hash "$batchPath/profile-parent.json";profile_preview_sha256=Hash "$batchPath/x12-profile-preview.json";capability_reference_sha256=Hash "$batchPath/capability-reference.json";live_preflight='not performed';emulator_started=$false;ADB_probe=$false;manual_session=$false;real_touch=$false;live_rounds=0;models=$false;commit_push=$false;new_chat_goal=$false;
 engineering_repairs=1;separate_negative_helper_copy_failure='negative-copy-admin-failure.json; corrected existing z.dll packaging, no meter change/run';unknown=@('original live unlisted compiled SHA','physical identity/body ownership','true capture/session lifecycle/RPC/render age/adoption','14 lost opportunities /77 Miss /cross-song effects','full manual-session performance equality; optional archive metering and extra admission dump overhead');
 X12_capacity=@{round_journal_bytes=512MB;segments=32;segment_bytes=16MB;max_attempts=6;bounded_plan_bytes=3277848576;future_live_root=12GB;reserve=5GB;prepared_only=$true};
 all_cost_runs='all-cost-runs.json';raw_release_negatives='release-negatives/releases-and-receipts.jsonl';artifact_ledger='artifact-ledger.json';artifact_ledger_sha256=Hash "$batchPath/artifact-ledger.json";capacity=@{batch_bytes=0;out_bytes=0;campaign_plus_prior_bytes=0;free_bytes=0;batch_limit=128MB;out_limit=3GB;campaign_limit=8GB;reserve=5GB};self_accounting='all outputs including logs/failures/bridges/tests/source/ledger/summary; logical entries; reparse references not duplicated; excludes recursive self SHA'}
foreach($i in 1..3){Save 'final-summary.json' $summary;$summary.capacity.batch_bytes=Bytes $batchPath;$summary.capacity.out_bytes=Bytes $outPath;$summary.capacity.campaign_plus_prior_bytes=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001");$summary.capacity.free_bytes=(Get-PSDrive C).Free}
Save 'final-summary.json' $summary
if($summary.capacity.batch_bytes -gt 128MB -or $summary.capacity.out_bytes -gt 3GB -or $summary.capacity.campaign_plus_prior_bytes -gt 8GB -or $summary.capacity.free_bytes -lt 5GB){throw 'final capacity gate'}
Write-Output ($summary.capacity|ConvertTo-Json);Write-Output "not-ready; old=$oldChecks new artifacts=$($artifactRows.Count)"
