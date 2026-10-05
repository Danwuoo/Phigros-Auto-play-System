param([ValidateSet('initialize','verify','build','run','finalize')][string]$Phase='initialize',[string]$Name='')
$ErrorActionPreference='Stop'
$taskRepo=(Resolve-Path "$PSScriptRoot/..").Path
$taskBatch="$taskRepo/measurements/runtime-decision-skip-a"
$taskBuild="$taskRepo/out/decision-skip-a"
$r3Root="$taskRepo/measurements/runtime-cost-x11-p-r3"
function HashFile($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){if(!(Test-Path -LiteralPath $p)){return [long]0};[long](Get-ChildItem -LiteralPath $p -File -Recurse | Measure-Object Length -Sum).Sum}
function SaveNew($p,$v){if(Test-Path -LiteralPath $p){throw "existing $p"};[IO.File]::WriteAllText($p,($v|ConvertTo-Json -Depth 95)+"`n",[Text.UTF8Encoding]::new($false))}
function Capacity{
 $old=(Bytes "$taskRepo/measurements/game-assist/2026-09-30-m0-manual-continue")+(Bytes "$taskRepo/measurements/research-next-20261001")
 $r3=Bytes $r3Root;$batch=Bytes $taskBatch;$build=Bytes $taskBuild;$free=(Get-PSDrive C).Free
 if($old -gt 8GB -or $r3 -gt 2GB -or $batch -gt 48MB -or $build -gt 1GB -or $old+$r3+$batch+16MB -gt 10GB -or $free -lt 5GB){throw 'capacity'}
 @{old_campaign_plus_prior_bytes=$old;r3_bytes=$r3;batch_bytes=$batch;build_bytes=$build;combined_bytes=$old+$r3+$batch;disk_free_bytes=$free;analysis_limit_bytes=48MB;controller_reserve_bytes=16MB;batch_limit_bytes=64MB;build_limit_bytes=1GB;free_reserve_bytes=5GB;allocated_NTFS_bytes='Unknown'}
}
function VerifyLedger($path){
 $j=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json
 foreach($f in $j.files){$len=if($null -ne $f.bytes){$f.bytes}else{$f.physical_file_bytes};if(!(Test-Path -LiteralPath $f.path) -or (Get-Item -LiteralPath $f.path).Length -ne $len -or (HashFile $f.path) -ne $f.sha256){throw "binding mismatch $($f.path)"}}
 @{path=$path;sha256=HashFile $path;bytes=(Get-Item -LiteralPath $path).Length;entries=$j.files.Count;verified=$true}
}
function RunLogged($n,$exe,$argsList,[int[]]$expected=@(0)){
 SaveNew "$taskBatch/$n-command.json" @{exe=$exe;arguments=$argsList;expected_exit=$expected;utc=[DateTime]::UtcNow.ToString('o')}
 & $exe @argsList *> "$taskBatch/$n.log";$ec=$LASTEXITCODE
 SaveNew "$taskBatch/$n-exit.json" @{exit=$ec;log_sha256=HashFile "$taskBatch/$n.log";utc=[DateTime]::UtcNow.ToString('o')}
 Write-Host "$n exit=$ec";if($ec -notin $expected){throw "unexpected $ec"}
}
if($Phase -eq 'initialize'){
 if(Test-Path -LiteralPath $taskBatch){throw 'existing batch'}
 $cap=Capacity;New-Item -ItemType Directory -Path $taskBatch|Out-Null
 SaveNew "$taskBatch/capacity-before.json" $cap
 $preserve=@(Get-ChildItem -LiteralPath "$taskRepo/src","$taskRepo/include" -File -Recurse)+@(Get-Item -LiteralPath "$taskRepo/CMakeLists.txt")
 SaveNew "$taskBatch/workspace-before.json" @{head=git -C $taskRepo rev-parse HEAD;branch=git -C $taskRepo branch --show-current;worktrees=git -C $taskRepo worktree list;status=git -C $taskRepo status --porcelain;strategy='observer50/planner27/diagnostics11; C36h tint1 baseline; pending-only unchanged; suppression OFF';formal_files=@($preserve|ForEach-Object{@{path=$_.FullName;sha256=HashFile $_.FullName;bytes=$_.Length}})}
 New-Item -ItemType Directory -Path "$taskBatch/documents-before"|Out-Null
 foreach($d in @('PROJECT_STATUS_NEXT_STEPS_20261001.md','C36H_FORWARD_EXECUTION_PLAN_20261003.md','RUNTIME_DECISION_SKIP_A_PROTOCOL_20261003.md')){Copy-Item -LiteralPath "$taskRepo/docs/$d" -Destination "$taskBatch/documents-before/$d"}
 $ledgers=@("$r3Root/artifact-ledger.json","$r3Root/controller-review/artifact-ledger.json","$r3Root/source-binding-before-cost.json","$r3Root/compiled-dependency-freeze.json","$r3Root/input-binding-before-cost.json")
 $verified=@($ledgers|ForEach-Object{VerifyLedger $_})
 SaveNew "$taskBatch/initial-binding-verification.json" @{ledgers=$verified;two_ledgers='R3 original artifact ledger and later controller independent ledger are separate; additional controller independent file outside original ledger is intentional'}
 $paths=[Collections.Generic.SortedSet[string]]::new([StringComparer]::Ordinal)
 foreach($p in $ledgers){[void]$paths.Add($p)}
 foreach($run in @('aa-rgb-1','aa-rgb-2','aa-rgb-3','aa-rgb-4','aa-owner-1')){foreach($f in Get-ChildItem -LiteralPath "$r3Root/$run" -File -Recurse){[void]$paths.Add($f.FullName)}}
 foreach($f in Get-ChildItem -LiteralPath "$r3Root/source-snapshot" -File -Recurse){[void]$paths.Add($f.FullName)}
 foreach($p in @('protocol-before-cost.md','input-clock-options-before-cost.json','method-environment-before-cost.json','qualification.json','final-summary.json','old-freeze-verified-controller-independent.json','controller-review/final-summary.json','controller-review/controller-crosscheck.json')){[void]$paths.Add("$r3Root/$p")}
 foreach($p in @('core.cpp','game_session.cpp','game.cpp','session_archive.cpp')){[void]$paths.Add("$taskRepo/out/x11-p-r1/B0/src/$p")}
 foreach($p in @('action_wake.hpp','core.hpp','session_archive.hpp','session_events.hpp')){[void]$paths.Add("$taskRepo/out/x11-p-r1/B0/include/pas/$p")}
 $original=(Get-Content "$r3Root/artifact-ledger.json" -Raw|ConvertFrom-Json).files
 $controller=(Get-Content "$r3Root/controller-review/artifact-ledger.json" -Raw|ConvertFrom-Json).files
 $files=@(foreach($p in $paths){$origin=if($original.path -contains $p){'R3-original'}elseif($controller.path -contains $p){'controller-independent'}else{'referenced-freeze-or-frozen-source'};@{path=[IO.Path]::GetFullPath($p);bytes=(Get-Item -LiteralPath $p).Length;sha256=HashFile $p;origin=$origin}})
 SaveNew "$taskBatch/input-manifest.json" @{schema=1;root=$taskRepo;r3_root=$r3Root;protocol_sha256=HashFile "$taskBatch/documents-before/RUNTIME_DECISION_SKIP_A_PROTOCOL_20261003.md";files=$files}
 Write-Host "initialized $($files.Count) input bindings"
}
elseif($Phase -eq 'verify'){
 $v=@(VerifyLedger "$r3Root/artifact-ledger.json";VerifyLedger "$r3Root/controller-review/artifact-ledger.json";VerifyLedger "$taskBatch/input-manifest.json")
 SaveNew "$taskBatch/$Name-verification.json" @{verification=$v;capacity=Capacity}
}
elseif($Phase -eq 'build'){
 $config=@('-S',"$taskRepo/apps/decision_skip_a",'-B',"$taskBuild/$Name",'-G','Visual Studio 18 2026','-A','x64','-T','v145','-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275','-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake','-DVCPKG_MANIFEST_MODE=OFF',"-DVCPKG_INSTALLED_DIR=$taskRepo/out/vcpkg_installed",'-DVCPKG_TARGET_TRIPLET=x64-windows')
 if($Name -eq 'asan'){$config+=@('-DSKIP_A_ASAN=ON');$cfg='RelWithDebInfo'}else{$cfg='Release'}
 RunLogged "configure-$Name" 'cmake' $config
 RunLogged "build-$Name" 'cmake' @('--build',"$taskBuild/$Name",'--config',$cfg,'--parallel','2')
}
elseif($Phase -eq 'run'){
 if($Name -like 'tests-*release'){RunLogged $Name "$taskBuild/release/Release/decision_skip_a.exe" @('self-test',"$taskBatch/$Name.json")}
 elseif($Name -like 'tests-*asan'){RunLogged $Name "$taskBuild/asan/RelWithDebInfo/decision_skip_a.exe" @('self-test',"$taskBatch/$Name.json")}
 elseif($Name -eq 'verify-asan'){RunLogged $Name "$taskBuild/asan/RelWithDebInfo/decision_skip_a.exe" @('verify-input',"$taskBatch/input-manifest-v2.json","$taskBatch/$Name.json")}
 else{RunLogged $Name "$taskBuild/release/Release/decision_skip_a.exe" @('analyze',"$taskBatch/input-manifest-v2.json","$taskBatch/$Name")}
}
