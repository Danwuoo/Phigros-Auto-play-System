$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$batchPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p"
$outPath="$repoPath/out/x11-p"
if(Test-Path "$batchPath/final-summary.json"){throw 'already frozen'}
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($p,$v){[IO.File]::WriteAllText($p,($v|ConvertTo-Json -Depth 70)+"`n",[Text.UTF8Encoding]::new($false))}
$binding=Get-Content "$batchPath/source-binding-before-cost.json" -Raw|ConvertFrom-Json
foreach($f in $binding.files){if((Hash $f.path) -ne $f.sha256){throw "binding altered $($f.path)"}}
$dep=Get-Content "$batchPath/compiled-dependency-freeze.json" -Raw|ConvertFrom-Json
foreach($f in $dep.files){if((Hash $f.path) -ne $f.sha256){throw "dependency altered $($f.path)"}}
$w=Get-Content "$batchPath/workspace-before.json" -Raw|ConvertFrom-Json
if((git rev-parse HEAD) -ne $w.head -or (git branch --show-current) -ne $w.branch){throw 'git state altered'}
if(@(git diff --name-only HEAD -- src include CMakeLists.txt).Count){throw 'formal checkout changed'}
$tests=@()
foreach($name in @('tests-B0-original','tests-B0-historical','tests-B0-pending','tests-B1-original','tests-B1-pending')){
 [xml]$x=Get-Content "$batchPath/$name.xml" -Raw
 $fail=@($x.SelectNodes('//testcase[failure]')|ForEach-Object {"$($_.classname).$($_.name)"})
 $tests+=@{name=$name;tests=[int]$x.testsuites.tests;failures=[int]$x.testsuites.failures;errors=[int]$x.testsuites.errors;disabled=[int]$x.testsuites.disabled;skipped=[int]$x.testsuites.skipped;failed_names=$fail;xml_sha256=(Hash "$batchPath/$name.xml")}
}
$bad=@($tests|Where-Object {$_.errors -ne 0 -or $_.disabled -ne 0})
if($bad.Count){throw 'unexpected test errors'}
if(($tests|Where-Object name -eq 'tests-B0-original').failures -ne 0 -or ($tests|Where-Object name -eq 'tests-B0-historical').failures -ne 0 -or ($tests|Where-Object name -eq 'tests-B1-original').failures -ne 1 -or ($tests|Where-Object name -eq 'tests-B1-pending').failures -ne 0 -or ($tests|Where-Object name -eq 'tests-B0-pending').failures -ne 5){throw 'test contract counts'}
$candidate=($tests|Where-Object name -eq 'tests-B1-original')
if($candidate.failed_names[0] -ne 'GameOwner.SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt'){throw 'unexpected regression'}
$commands=@(Get-ChildItem $batchPath -Filter '*-cost-command.json')
$normal=@($commands|Where-Object Name -NotLike 'stress-*');$stress=@($commands|Where-Object Name -Like 'stress-*')
if($normal.Count -ne 24 -or $stress.Count -ne 4){throw 'run budget'}
foreach($c in $commands){$exit=Get-Content ($c.FullName.Replace('-command.json','-exit.json')) -Raw|ConvertFrom-Json;if($exit.exit -ne 0){throw 'unexpected cost run failure'}}
$gate=Get-Content "$batchPath/cost-evaluation.json" -Raw|ConvertFrom-Json
$inverse=Get-Content "$batchPath/source-inverse-audit.json" -Raw|ConvertFrom-Json
if(!$inverse.baseline_public_bridge_equal -or !$inverse.candidate_public_bridge_equal){throw 'bridge'}
$original="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/candidate36h-tint1-runtime/pas.exe"
if((Hash $original) -ne '61ecfacd2cc48ed270506719a1089be646d915c64e9026706ac1654e0d4bbce9'){throw 'original live binary'}
$summary=@{schema=1;state='not-ready';review_status='待總控獨立驗收';X11_engineering_delivery='complete; negative cost result';ready_for_controlled_live=$false;cost_gate=[bool]$gate.cost_gate;noise_gate=[bool]$gate.noise_gate;
 blockers=@('RGB recognition p99 and dense owner p99 A/A noise insufficient under predeclared formula','dense owner p99 ABBA failed both batches +.46655/.45685ms > .298ms','baseline ab-owner-1-4-B0 lateness p99 15.0906ms >15ms','production SessionArchive full serialization/disk and high-resolution Wake not equivalently metered','original live unlisted compiled SHA provenance unknown','future X12 256MiB journal guard missing; native C36h hard cap512MiB');
 head=$w.head;branch=$w.branch;formal_source_unchanged=$true;core_inverse_files=$inverse.core_files.Count;source_binding_items=$binding.files.Count;dependency_items=$dep.files.Count;tests=$tests;
 cost_runs=24;stress_runs=4;full_replay_runs=0;bridge_inputs_per_role=512;bridge_old_new_events_equal=$true;stress_B1_owner='new uninstrumented B1 core+new concurrent harness ASan; excluded from cost';emulator_started=$false;adb_probe=$false;live_rounds=0;models=$false;commit_push=$false;new_chat_goal=$false;suppression=$false;
 original_live_binary=@{path=$original;sha256=(Hash $original)};
 new_runtime=@(foreach($r in @('B0','B1')){@{role=$r;path="$outPath/runtime-$r/pas.exe";sha256=(Hash "$outPath/runtime-$r/pas.exe");source_archive="$outPath/$r-final-source.tar";source_archive_sha256=(Hash "$outPath/$r-final-source.tar")}});
 profile_parent_sha256=(Hash "$batchPath/profile-parent.json");profile_preview_sha256=(Hash "$batchPath/x12-profile-preview.json");profile_change='log_dir only';capability_reference_sha256=(Hash "$batchPath/capability-reference.json");device_preflight='not performed; static prior reference only';
 unknown=@('14 lost opportunities/gameplay/Miss/cross-song/physical gold','real RPC/render age/game adoption','per-call fake release denominator not persisted; release_all fixed success only');
 artifact_ledger='artifact-ledger.json';ledger_sha256='';capacity=@{batch_bytes=0;out_bytes=0;campaign_plus_prior_bytes=0;free_bytes=0;batch_limit=128MB;out_limit=3GB;campaign_limit=8GB;reserve=5GB};
 exact_commands='all *-command.json, CMakeCache/vcxproj/tlogs, source-archive-commands.json and maintenance scripts in source-snapshot; build-B0-2/aux/ASan commands also in manual-executions.json';
 self_accounting='final-summary and artifact-ledger bytes included in measured totals, no recursive self SHA'}
Save "$batchPath/workspace-after.json" @{head=(git rev-parse HEAD);branch=(git branch --show-current);worktrees=@(git worktree list);status=@(git status --short);no_formal_source_diff=$true;old_dirty_retained='old-dirty-retained.json/status-prefix-proof.json';no_new_live=$true}
$artifacts=@()
foreach($root in @($batchPath,$outPath)){
 foreach($f in Get-ChildItem -LiteralPath $root -File -Recurse){if($f.Name -in @('artifact-ledger.json','final-summary.json')){continue};$artifacts+=@{path=$f.FullName;bytes=$f.Length;sha256=(Hash $f.FullName)}}
}
Save "$batchPath/artifact-ledger.json" @{files=$artifacts;capacity_scope='batch plus out products and all failures; junction test raw references are not copied/traversed';excluded_self=@('artifact-ledger.json','final-summary.json')}
$summary.ledger_sha256=Hash "$batchPath/artifact-ledger.json"
foreach($i in 1..3){
 Save "$batchPath/final-summary.json" $summary
 $summary.capacity.batch_bytes=Bytes $batchPath;$summary.capacity.out_bytes=Bytes $outPath
 $summary.capacity.campaign_plus_prior_bytes=(Bytes "$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue")+(Bytes "$repoPath/measurements/research-next-20261001")
 $summary.capacity.free_bytes=(Get-PSDrive C).Free
}
Save "$batchPath/final-summary.json" $summary
if($summary.capacity.batch_bytes -gt 128MB -or $summary.capacity.out_bytes -gt 3GB -or $summary.capacity.campaign_plus_prior_bytes -gt 8GB -or $summary.capacity.free_bytes -lt 5GB){throw 'capacity gate'}
Write-Output ($summary.capacity|ConvertTo-Json)
Write-Output "Frozen not-ready; cost=$($summary.cost_gate) noise=$($summary.noise_gate) files=$($artifacts.Count)"
