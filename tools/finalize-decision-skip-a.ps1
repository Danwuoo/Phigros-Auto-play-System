param([ValidateSet('freeze','freeze-final','finalize')][string]$Phase='freeze')
$ErrorActionPreference='Stop'
$taskRepo=(Resolve-Path "$PSScriptRoot/..").Path
$taskBatch="$taskRepo/measurements/runtime-decision-skip-a"
$taskBuild="$taskRepo/out/decision-skip-a"
function HashFile($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function SaveNew($p,$j){if(Test-Path -LiteralPath $p){throw "existing $p"};[IO.File]::WriteAllText($p,($j|ConvertTo-Json -Depth 95)+"`n",[Text.UTF8Encoding]::new($false))}
function Entry($p){@{path=[IO.Path]::GetFullPath($p);bytes=(Get-Item -LiteralPath $p).Length;sha256=HashFile $p}}
if($Phase -like 'freeze*'){
 $suffix=if($Phase -eq 'freeze-final'){'-v2'}else{''}
 New-Item -ItemType Directory -Path "$taskBatch/source-snapshot$suffix"|Out-Null
 foreach($f in Get-ChildItem -LiteralPath "$taskRepo/apps/decision_skip_a" -File){Copy-Item -LiteralPath $f.FullName -Destination "$taskBatch/source-snapshot$suffix/$($f.Name)"}
 foreach($n in @('decision-skip-a-maintenance.ps1','finalize-decision-skip-a.ps1')){Copy-Item -LiteralPath "$taskRepo/tools/$n" -Destination "$taskBatch/source-snapshot$suffix/$n"}
 $deps=[Collections.Generic.SortedSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
 $tlogs=@(Get-ChildItem -LiteralPath $taskBuild -Recurse -File | Where-Object { $_.Name -in @('CL.read.1.tlog','Microsoft.Build.CPPTasks.CL.read.1.tlog','link.read.1.tlog','CL.command.1.tlog','link.command.1.tlog') -and $_.FullName -match 'decision_skip_a.dir'})
 foreach($t in $tlogs){foreach($line in Get-Content -LiteralPath $t.FullName){if($line -match '^[A-Za-z]:[\\/]'){$p=$line.Trim();if(Test-Path -LiteralPath $p -PathType Leaf){[void]$deps.Add($p)}}}}
 foreach($p in @('C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe','C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/link.exe')){[void]$deps.Add($p)}
 if(@($deps|Where-Object{$_ -match 'pas_core\.lib'}).Count -ne 0){throw 'unexpected runtime core link'}
 SaveNew "$taskBatch/compiled-dependency-freeze$suffix.json" @{scope='actual new analyzer CL/link read tlogs and compiler; independent C++20 JSON/BCrypt; no runtime core';files=@($deps|ForEach-Object{Entry $_});tlogs=@($tlogs|ForEach-Object{Entry $_.FullName})}
 $bindings=@(Get-ChildItem -LiteralPath "$taskRepo/apps/decision_skip_a" -File|ForEach-Object{Entry $_.FullName})
 $bindings+=@(foreach($n in @('decision-skip-a-maintenance.ps1','finalize-decision-skip-a.ps1')){Entry "$taskRepo/tools/$n"})
 $bindings+=@(Get-ChildItem -LiteralPath $taskBuild -File -Recurse|Where-Object {$_.Name -eq 'decision_skip_a.exe' -or $_.Extension -eq '.map' -or $_.Name -eq 'clang_rt.asan_dynamic-x86_64.dll'}|ForEach-Object{Entry $_.FullName})
 $bindings+=@(Entry "$taskBatch/input-manifest-v2.json";Entry "$taskBatch/protocol-before-analysis.md";Entry "$taskBatch/compiled-dependency-freeze$suffix.json")
 SaveNew "$taskBatch/source-binary-binding$suffix.json" @{files=$bindings;new_runtime_exe=$false;new_tool_only=$true;strategy_modified=$false;frozen_workspace_documents='batch document snapshots, never mutable workspace status as permanently immutable'}
 $direct=@('build-release-fix','build-release-final','build-reader-diagnostic','build-reader-fix','build-final-release','build-delivery-release','build-delivery-asan')
 foreach($n in $direct){if(Test-Path -LiteralPath "$taskBatch/$n-command.json"){continue};$asan=$n -like '*asan';$dir=if($asan){'asan'}else{'release'};$cfg=if($asan){'RelWithDebInfo'}else{'Release'};SaveNew "$taskBatch/$n-command.json" @{exe='cmake';arguments=@('--build',"$taskBuild/$dir",'--config',$cfg,'--parallel','2');log="$taskBatch/$n.log";note='exact direct command captured from executed maintenance invocation; historical exit file retained'}}
 Write-Host "source/binary freeze; dependencies=$($deps.Count)"
 exit
}
$freeze=Get-Content "$taskBatch/source-binary-binding-v2.json" -Raw|ConvertFrom-Json
foreach($f in $freeze.files){if((HashFile $f.path) -ne $f.sha256 -or (Get-Item -LiteralPath $f.path).Length -ne $f.bytes){throw "tool freeze changed $($f.path)"}}
$before=Get-Content "$taskBatch/workspace-before.json" -Raw|ConvertFrom-Json
foreach($f in $before.formal_files){if((HashFile $f.path) -ne $f.sha256){throw "formal changed $($f.path)"}}
$equal=@();foreach($f in Get-ChildItem -LiteralPath "$taskBatch/deterministic-final-1" -File){$p="$taskBatch/deterministic-final-2/$($f.Name)";if(!(Test-Path -LiteralPath $p) -or (HashFile $f.FullName) -ne (HashFile $p) -or $f.Length -ne (Get-Item -LiteralPath $p).Length){throw "non-deterministic $p"};$equal+=Entry $f.FullName}
if(@(Get-ChildItem -LiteralPath "$taskBatch/deterministic-final-2" -File).Count -ne $equal.Count){throw 'extra output'}
SaveNew "$taskBatch/determinism.json" @{equal=$true;file_count=$equal.Count;files=$equal}
New-Item -ItemType Directory -Path "$taskBatch/documents-delivery"|Out-Null
$prefix=@();foreach($n in @('PROJECT_STATUS_NEXT_STEPS_20261001.md','C36H_FORWARD_EXECUTION_PLAN_20261003.md')){
 $old=[IO.File]::ReadAllBytes("$taskBatch/documents-before/$n");$now=[IO.File]::ReadAllBytes("$taskRepo/docs/$n");if($now.Length -lt $old.Length){throw 'document shortened'}
 for($i=0;$i -lt $old.Length;$i++){if($old[$i] -ne $now[$i]){throw "prefix changed $n"}}
 $prefix+=@{document=$n;original_bytes=$old.Length;current_bytes=$now.Length;prefix_equal=$true};Copy-Item -LiteralPath "$taskRepo/docs/$n" -Destination "$taskBatch/documents-delivery/$n"
}
foreach($n in @('RUNTIME_DECISION_SKIP_A_PROTOCOL_20261003.md','RUNTIME_DECISION_SKIP_A_RESULT_20261003.md','RUNTIME_DECISION_SKIP_A_HANDOFF_20261003.md')){Copy-Item -LiteralPath "$taskRepo/docs/$n" -Destination "$taskBatch/documents-delivery/$n"}
SaveNew "$taskBatch/workspace-after.json" @{head=git -C $taskRepo rev-parse HEAD;branch=git -C $taskRepo branch --show-current;worktrees=git -C $taskRepo worktree list;status=git -C $taskRepo status --porcelain;formal_unchanged=$before.formal_files.Count;document_prefix=$prefix}
$old=(Bytes "$taskRepo/measurements/game-assist/2026-09-30-m0-manual-continue")+(Bytes "$taskRepo/measurements/research-next-20261001");$r3=Bytes "$taskRepo/measurements/runtime-cost-x11-p-r3";$batch=Bytes $taskBatch;$build=Bytes $taskBuild;$free=(Get-PSDrive C).Free
if($old -ne (Get-Content "$taskBatch/capacity-before.json" -Raw|ConvertFrom-Json).old_campaign_plus_prior_bytes){throw 'old capacity changed'}
if($old -gt 8GB -or $r3 -gt 2GB -or $batch+2MB -gt 48MB -or $build -gt 1GB -or $old+$r3+$batch+2MB+16MB -gt 10GB -or $free -lt 5GB){throw 'final capacity'}
SaveNew "$taskBatch/artifact-ledger.json" @{files=@(Get-ChildItem -LiteralPath $taskBatch -File -Recurse|Sort-Object FullName|ForEach-Object{Entry $_.FullName});exclude='this ledger and final-summary self hashes are separately recorded';documents='immutable batch snapshots; mutable workspace status/forward not artifact bindings'}
$summary=@{status='選項A交付完成，待總控獨立驗收；成本資格仍not-ready';head=$before.head;new_runtime_cost_stress_OS_live_runs=0;input_manifest=Entry "$taskBatch/input-manifest-v2.json";source_binary_binding=Entry "$taskBatch/source-binary-binding-v2.json";compiled_dependency_freeze=Entry "$taskBatch/compiled-dependency-freeze-v2.json";artifact_ledger=Entry "$taskBatch/artifact-ledger.json";determinism=Entry "$taskBatch/determinism.json";report=Entry "$taskBatch/deterministic-final-1/report.json";capacity=@{old_campaign_plus_prior_bytes=$old;r3_bytes=$r3;batch_bytes=0;build_bytes=$build;combined_bytes=0;disk_free_bytes=$free;analysis_limit_bytes=48MB;controller_reserve_bytes=16MB;batch_limit_bytes=64MB;build_limit_bytes=1GB;free_reserve_bytes=5GB;NTFS_allocation='Unknown'};actual_commands=@(Get-ChildItem -LiteralPath $taskBatch -Filter '*-command.json'|Sort-Object Name|ForEach-Object{Entry $_.FullName});failures_retained=@('build-release','build-asan','deterministic-1','reader-check-1');actual_tests=@('tests-final-release','tests-final-asan','verify-asan');only_referenced='R3 original runs/tests/audits/bridge, original R1/R2 runtime and stress; no rerun';notes='analysis-1 exploratory and both rejected reader outputs retained; no old raw copied or mutated'}
$p="$taskBatch/final-summary.json";if(Test-Path -LiteralPath $p){throw 'existing final-summary'}
for($i=0;$i -lt 6;$i++){$summary.capacity.batch_bytes=(Bytes $taskBatch);$summary.capacity.combined_bytes=$old+$r3+$summary.capacity.batch_bytes;[IO.File]::WriteAllText($p,($summary|ConvertTo-Json -Depth 95)+"`n",[Text.UTF8Encoding]::new($false))}
if((Bytes $taskBatch) -ne $summary.capacity.batch_bytes){throw 'self length unstable'}
Write-Host "final batch=$($summary.capacity.batch_bytes) build=$build; ledger and summary SHA below"
Write-Host "ledger $(HashFile "$taskBatch/artifact-ledger.json")"
Write-Host "summary $(HashFile $p)"
