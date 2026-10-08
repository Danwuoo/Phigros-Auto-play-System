param([ValidateSet('release','debug','asan')][string]$Mode='release',
      [ValidatePattern('^main[0-9]{2}$')][string]$BuildAttempt='main19')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'));$sourceRepo=$repo;$oldRepo=$repo
$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006';$pwsh=(Get-Command pwsh).Source;$Operation='build';$Campaign='';$script:inputClosures=@{};$script:maintenancePath=$PSCommandPath
[IO.Directory]::CreateDirectory($pkg)|Out-Null
$gate=Join-Path $out 'guard-main05-native-01/native-gate.json';$tool=Join-Path $out 'tool-main05-01/tool-snapshot.json';$recipe=Join-Path $repo 'research/prelive_current/run.ps1';$tok=$null;$err=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tok,[ref]$err)
if($err.Count){throw 'shared recipe parse'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage','Native')){
 $d=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name);if($d.Count -ne 1){throw 'shared function'};$t=$d[0].Extent.Text
 if($name -in @('Native','Stage')){$t=$t.Replace('Join-Path $out ''withdrawal-review-01/stage.ps1''','Join-Path $repo ''research/main_consolidation/stage.ps1''')};if($name -ceq 'Native'){$t=$t.Replace('$PSCommandPath','$script:maintenancePath').Replace('tools/zero_miss_windows/invoke.ps1','research/windows_process_guard_v3/invoke.ps1')};if($name -ceq 'GateInputs'){$t=$t.Replace('pas.windows-offline-process-gate.v1','pas.windows-offline-process-gate.v3')};. ([scriptblock]::Create($t))
}
$commit=(& git -C $sourceRepo rev-parse HEAD);if($LASTEXITCODE -or -not $commit -or (& git -C $sourceRepo branch --show-current) -cne 'main'){throw 'main source identity required'};if((& git -C $sourceRepo rev-parse HEAD) -cne $commit -or (& git -C $sourceRepo status --porcelain)){throw 'exact clean candidate required'}
$artifact=Join-Path $repo ('out/prelive-pre-emulator-evaluation-'+$BuildAttempt+'-'+$Mode);Fresh $artifact;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$approval=Join-Path $repo 'docs/research/live-readiness-20261007/CAPACITY_APPROVAL_20261008.json';$a=Get-Content -Raw -LiteralPath $approval|ConvertFrom-Json;if($a.status -cne 'HUMAN_APPROVED' -or $a.retained_prelive_cap_bytes -ne 51539607552){throw '48 GiB approval required'};
$plan=Join-Path $sourceRepo 'research/pre_emulator/PLAN.md'
$coreProof=Join-Path $artifact 'fresh-core-build-plan.json';NewJson $coreProof @{schema='pas.fresh-core-build-plan.v1';source_commit=$commit;configuration=$Mode;translation_units=22;old_donor_objects_used=$false;compiler_snapshot=(Binding $tool);device_operations=0;live_ready=$false}
$sourceFiles=@(& git -C $sourceRepo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -match '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'}|ForEach-Object {Join-Path $sourceRepo $_})
$sourceManifest=Join-Path $artifact 'source-manifest.json';NewJson $sourceManifest @{schema='pas.cooperative-source.v1';source_commit=$commit;dirty=$false;files=@($sourceFiles|ForEach-Object {Binding $_});device_operations=0;live_ready=$false}
$adapter=Join-Path $repo 'research/main_consolidation/cmake-stage.ps1';$g=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$dependencyRoot='C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed/x64-windows'
# Capture both configurations, package metadata, protoc/plugin and runtime DLLs
# before configure; Release/lib basename matches miss Debug-specific inputs.
$deps=@(Get-ChildItem -LiteralPath $dependencyRoot -File -Recurse|ForEach-Object FullName)
$dependencyManifest=Join-Path $artifact 'dependency-manifest.json'
NewJson $dependencyManifest @{schema='pas.pre-emulator-dependency-closure.v1';source_commit=$commit;root=$dependencyRoot;files=@($deps|ForEach-Object {Binding $_});release_and_debug=$true;protoc_and_plugin=$true;device_operations=0;live_ready=$false} 4194304
$deps+=@($dependencyManifest,'C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake')
$inputs=@($PSCommandPath,$recipe,$adapter,$plan,$approval,(Join-Path $repo 'research/main_consolidation/stage.ps1'),$sourceManifest,$coreProof)+$sourceFiles+$deps+(GateInputs)+@($g.guard_source.path,$g.process_source.path,$g.cached_identity_controls.path,$g.identity_wait_controls.path)
$environmentStage='environment-prelive-pre-emulator-'+$Mode+'-'+$BuildAttempt
$environmentRoot=Join-Path $out ($environmentStage+'-inputs');Fresh $environmentRoot;[IO.Directory]::CreateDirectory($environmentRoot)|Out-Null
$environmentOutput=Join-Path $environmentRoot 'compiler-environment.json'
$environmentCommand=Join-Path $environmentRoot 'capture.cmd'
$vcvars='C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
$capture=Join-Path $repo 'research/main_consolidation/capture-environment.ps1'
foreach($value in @($vcvars,$pwsh,$capture,$environmentOutput)){if($value -match '["%\r\n]'){throw 'literal environment capture path'}}
$lines=@('@echo off','setlocal DisableDelayedExpansion',('call "'+$vcvars+'" -vcvars_ver=14.51 >nul'),'if not "%errorlevel%"=="0" exit /b %errorlevel%',('"'+$pwsh+'" -NoProfile -File "'+$capture+'" -Output "'+$environmentOutput+'"'),'exit /b %errorlevel%')
[IO.File]::WriteAllText($environmentCommand,($lines -join "`r`n")+"`r`n",[Text.UTF8Encoding]::new($false))
$environmentSpec=Join-Path $environmentRoot 'spec.json'
$environmentLeaves=InputClosure $environmentStage @($inputs+$capture+$environmentCommand+$vcvars+$pwsh)
NewJson $environmentSpec @{stage_root=(Join-Path $out $environmentStage);stage=$environmentStage;exe=$env:ComSpec;argv=@('/d','/s','/c',$environmentCommand);native_exit=0;runner_exit=0;total_s=120;stream_cap=65536;inputs=@($environmentLeaves+$environmentCommand+$capture+$vcvars+$pwsh+(Join-Path $pkg ($environmentStage+'-params.json')));environment=@{VSCMD_SKIP_SENDTELEMETRY='1';VC_DISABLE_SQM='1';MSBUILDDISABLENODEREUSE='1'}}
Stage $environmentStage (Join-Path $repo 'research/windows_process_guard_v3/invoke.ps1') @{Specification=$environmentSpec;GateReceipt=$gate}
$environmentBinding=Join-Path $artifact 'compiler-environment-binding.json';NewJson $environmentBinding @{schema='pas.msvc-environment-binding.v1';environment=(Binding $environmentOutput);verification=(Binding (Join-Path $out ($environmentStage+'/'+$environmentStage+'-verification.json')));state=(Binding (Join-Path $out ($environmentStage+'/state.json')));source_commit=$commit;device_operations=0;live_ready=$false} 131072
$inputs+=@($environmentBinding,$environmentOutput)
$root=Join-Path $repo ('out/prelive-pre-emulator-'+$Mode+'-'+$BuildAttempt);$cfg=if($Mode -eq 'release'){'Release'}else{'Debug'}
$options=@('-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake','-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed','-DVCPKG_TARGET_TRIPLET=x64-windows','-DVCPKG_MANIFEST_MODE=OFF','-DCMAKE_CXX_SCAN_FOR_MODULES=OFF','-DCMAKE_HAVE_LIB_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=')
if($Mode -eq 'asan'){$options+=@('-DPAS_PRELIVE_ASAN=ON','-DPAS_ASAN_SUPPORT_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')}
$configure='configure-prelive-pre-emulator-'+$Mode+'-'+$BuildAttempt
Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $sourceRepo 'research/pre_emulator');BuildRoot=$root;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentBinding;Operation='Configure';Configuration=$cfg;Inputs=$inputs;Options=$options}
$targets=@('diagnostic_writer_tests','owner_checkpoint_tests','owner_expiry_tests','producer_wait_tests','tap_reset_workload_tests','receipt_join_diagnose','owner_wait_tests','current_tests','active_body_tests','tail_guard_tests','tail_only_owner_review','tail_only_pixels_review','disabled_live_seam_tests','live_seam_packet_review','live_seam_runtime_review','runtime_finish_tests','current_chain','current_chain_phase_profile','diagnostic_phase_summary','prelive_audit','cost_analysis','cost_analysis_tests','cost_analysis_review_tests','cost_receipt','cost_comparison','cost_comparison_tests','production_pipeline_cold','production_pipeline_audit','production_pipeline_audit_tests','production_entry_controls','fault_controls','fault_audit','batch_check','pas')
$buildReceipts=@()
$taskNinja=Get-Content -LiteralPath (Join-Path $root 'build.ninja') -Raw
$objects=@(Get-Content -LiteralPath (Join-Path $root 'build.ninja')|ForEach-Object {$line=$_.Replace('\','/');if($line -match '^build (production/CMakeFiles/pas_core[.]dir/[^:]+[.]obj): CXX_COMPILER'){$Matches[1]}})
$decl=Get-Content -LiteralPath (Join-Path $sourceRepo 'CMakeLists.txt')|Where-Object {$_ -match '^add_library\(pas_core STATIC '};$expected=@([regex]::Matches($decl,'src/[^ )]+[.]cpp')|ForEach-Object {$_.Value})
$actual=@($objects|ForEach-Object {if($_ -match '/(src/[^/]+[.]cpp)[.]obj$'){$Matches[1]}else{throw 'unexpected formal core object'}})
if($objects.Count -ne 22 -or @(Compare-Object ($actual|Sort-Object) ($expected|Sort-Object)).Count){throw 'exact fresh 22 core object set'}
$groups=@(@{name='fault-audit';targets=@('fault_audit')},@{name='batch-check';targets=@('batch_check')})
for($i=0;$i -lt $objects.Count;$i+=5){$groups+=@(@{name=('core-'+[int]($i/5+1));targets=@($objects|Select-Object -Skip $i -First 5)})}
# main05's combined library stage reached the fixed 64 held-identity cap.
# Build each dependency first so the next stage has a small, known closure;
# never increase the guard limit or reuse the interrupted output as a donor.
foreach($library in @('pas_core','pas_proto','pas_current_candidate','pas_emulator','pas_bench','exact_bvi_v3','current_front','own_bridge','current_rails')){$groups+=@(@{name=('library-'+$library.Replace('_','-'));targets=@($library)})}
$groups+=@(@{name='diagnostic-writer-tests';targets=@('diagnostic_writer_tests')})
$largeTargets=@('production_entry_controls','fault_controls','pas')
$remainingTargets=@($targets|Where-Object {$_ -notin (@('fault_audit','batch_check','diagnostic_writer_tests')+$largeTargets)})
for($i=0;$i -lt $remainingTargets.Count;$i+=3){$groups+=@(@{name=('targets-'+([int]($i/3+1)).ToString('00'));targets=@($remainingTargets|Select-Object -Skip $i -First 3)})}
foreach($target in $largeTargets){$groups+=@(@{name=$target.Replace('_','-');targets=@($target)})}
foreach($group in $groups){
 $build='build-prelive-pre-emulator-'+$Mode+'-'+$BuildAttempt+'-'+$group.name
 Stage $build $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $sourceRepo 'research/pre_emulator');BuildRoot=$root;StageName=$build;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentBinding;Operation='Build';Configuration=$cfg;Inputs=@($inputs+(Join-Path $root 'CMakeCache.txt')+(Join-Path $root 'build.ninja'));Options=@('--target')+$group.targets}
 $buildReceipts+=@(Binding (Join-Path $out ($build+'/'+$build+'-verification.json')))
 & (Join-Path $PSScriptRoot 'archive-linker-caches.ps1') -Mode $Mode -BuildAttempt $BuildAttempt -Stage $build -Targets $group.targets
 if($group.name -ceq 'diagnostic-writer-tests'){
  $testExe=Join-Path $root 'cold/diagnostic_writer_tests.exe';$seal=@{files=@(Binding $testExe)};$Operation='tests';$report=Join-Path $artifact 'diagnostic_writer_tests.json';$stage='test-prelive-pre-emulator-'+$Mode+'-'+$BuildAttempt+'-diagnostic-writer-tests'
  $runtimeDlls=@(Get-ChildItem -LiteralPath $root -File -Recurse -Filter '*.dll'|ForEach-Object FullName)
  Native $stage $testExe @($report) @($inputs+$seal.files.path+$buildReceipts.path+$runtimeDlls) 900
  $value=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
  if($value.schema -cne 'pas.diagnostic-writer-tests.v1' -or $value.failed -ne 0 -or $value.tests_planned -ne 15 -or $value.tests_run -ne 15 -or $value.cases.Count -ne 15 -or @($value.cases|Where-Object {-not $_.pass}).Count){throw 'all 15 real writer and encoding cases required'}
  $earlyTest=@{test='diagnostic_writer_tests';report=(Binding $report);receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))};$Operation='build'
 }
}
& (Join-Path $PSScriptRoot 'archive-final-symbols.ps1') -Mode $Mode -BuildAttempt $BuildAttempt -FinalStage $build
$symbolProof=Join-Path $repo ('out/retained/consolidation-20261008/'+$BuildAttempt+'-'+$Mode+'-final-symbols/proof.json')
$symbols=if(Test-Path -LiteralPath $symbolProof){Binding $symbolProof}else{$null}
$seal=@{files=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object {Binding $_.FullName})}
$buildSeal=Join-Path $artifact 'build-binding.json';NewJson $buildSeal @{schema='pas.cooperative-build.v1';source_commit=$commit;configuration=$Mode;source_manifest=(Binding $sourceManifest);dependency_manifest=(Binding $dependencyManifest);core_build_plan=(Binding $coreProof);build_root=$root;files=$seal.files;configure_receipt=(Binding (Join-Path $out ($configure+'/'+$configure+'-verification.json')));build_receipts=$buildReceipts;serial_symbol_archive=$symbols;device_operations=0;live_ready=$false}
function FindExecutable($target){$found=@(Get-ChildItem -LiteralPath $root -File -Recurse -Filter ($target+'.exe'));if($found.Count -ne 1){throw 'one exact executable required'};return $found[0].FullName}
$Operation='tests';$tests=@($earlyTest);foreach($target in $targets|Where-Object {$_ -notin @('diagnostic_writer_tests','receipt_join_diagnose','current_chain','current_chain_phase_profile','diagnostic_phase_summary','prelive_audit','cost_analysis','cost_analysis_tests','cost_analysis_review_tests','cost_receipt','cost_comparison','production_pipeline_cold','production_pipeline_audit','fault_controls','fault_audit','batch_check','pas')}){
 $report=Join-Path $artifact ($target+'.json');$stage='test-prelive-pre-emulator-'+$Mode+'-'+$BuildAttempt+'-'+$target.Replace('_','-');Native $stage (FindExecutable $target) @($report) @($inputs+$buildSeal+$seal.files.path) 900
 $r=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json;if($r.failed -ne 0){throw ('cold regression failed '+$target)}
 $tests+=@(@{test=$target;report=(Binding $report);receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))})
}
$fixtureRoot=Join-Path $artifact 'cost-unit-fixture';$reviewRoot=Join-Path $artifact 'cost-review-fixture'
$fixtureRepo=Join-Path $repo ('out/prelive-pre-emulator-fixture-repo-'+$BuildAttempt+'-'+$Mode)
& (Join-Path $PSScriptRoot 'prepare-fixture.ps1') -SourceManifest $sourceManifest -Destination $fixtureRepo
$fixtureProof=Join-Path $fixtureRepo 'fixture-repository-proof.json';$fixtureInputs=@(Get-ChildItem -LiteralPath $fixtureRepo -File -Recurse|ForEach-Object FullName)
foreach($unit in @(@{name='cost_analysis_tests';args=@($fixtureRoot,$fixtureRepo);report=(Join-Path $fixtureRoot 'result.json')},@{name='cost_analysis_review_tests';args=@($fixtureRoot,$reviewRoot);report=(Join-Path $reviewRoot 'review_result.json')})){
 $stage='test-prelive-pre-emulator-'+$Mode+'-'+$BuildAttempt+'-'+$unit.name.Replace('_','-')
 Native $stage (FindExecutable $unit.name) $unit.args @($inputs+$buildSeal+$seal.files.path+$fixtureInputs) 900
 $r=Get-Content -LiteralPath $unit.report -Raw|ConvertFrom-Json;if(-not $r.pass){throw 'cost tool unit review failed'}
 $tests+=@(@{test=$unit.name;report=(Binding $unit.report);receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))})
}
$newRoots=@(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -Directory|Where-Object Name -match '^prelive-pre-emulator-(?:(?:release|debug|asan)-main[0-9]{2}|evaluation-main[0-9]{2}-(?:release|debug|asan)|fixture-repo-main[0-9]{2}(?:-(?:release|debug|asan))?|planning)$');$candidateBytes=[long](($newRoots|ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -File -Recurse}|Measure-Object Length -Sum).Sum);if($candidateBytes -gt 6442450944){throw 'separate candidate cap'}
NewJson (Join-Path $artifact 'evaluation.json') @{schema='pas.cooperative-cold-evaluation.v1';source_commit=$commit;build=(Binding $buildSeal);tests=$tests;affected_cold_pass=$true;candidate_bytes=$candidateBytes;candidate_cap=6442450944;live_ready=$false;device_operations=0}
