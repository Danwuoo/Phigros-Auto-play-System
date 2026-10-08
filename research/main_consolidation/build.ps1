param([ValidateSet('release','debug','asan')][string]$Mode='release')
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
$artifact=Join-Path $repo ('out/prelive-main-integration-evaluation-main05-'+$Mode);Fresh $artifact;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$approval=Join-Path $repo 'docs/research/live-readiness-20261007/CAPACITY_APPROVAL_20261008.json';$a=Get-Content -Raw -LiteralPath $approval|ConvertFrom-Json;if($a.status -cne 'HUMAN_APPROVED' -or $a.retained_prelive_cap_bytes -ne 51539607552){throw '48 GiB approval required'};
$plan=Join-Path $sourceRepo 'docs/research/live-readiness-20261007/DIAGNOSTIC_WRITER_AND_ARCHIVE_20261008.md'
$coreProof=Join-Path $artifact 'fresh-core-build-plan.json';NewJson $coreProof @{schema='pas.fresh-core-build-plan.v1';source_commit=$commit;configuration=$Mode;translation_units=22;old_donor_objects_used=$false;compiler_snapshot=(Binding $tool);device_operations=0;live_ready=$false}
$sourceFiles=@(& git -C $sourceRepo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -match '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'}|ForEach-Object {Join-Path $sourceRepo $_})
$sourceManifest=Join-Path $artifact 'source-manifest.json';NewJson $sourceManifest @{schema='pas.cooperative-source.v1';source_commit=$commit;dirty=$false;files=@($sourceFiles|ForEach-Object {Binding $_});device_operations=0;live_ready=$false}
$adapter=Join-Path $oldRepo 'research/windows_owner_wake/cmake-stage-v2.ps1';$g=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$deps=@(Get-ChildItem -LiteralPath 'C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed/x64-windows/include' -File -Recurse|Where-Object {$_.FullName -match '\\(nlohmann|zlib|google|grpc|gtest)\\' -or $_.Name -in @('zlib.h','zconf.h')}|ForEach-Object FullName)
$inputs=@($PSCommandPath,$recipe,$adapter,$plan,$approval,(Join-Path $repo 'research/main_consolidation/stage.ps1'),$sourceManifest,$coreProof)+$sourceFiles+$deps+(GateInputs)+@($g.guard_source.path,$g.process_source.path,$g.cached_identity_controls.path,$g.identity_wait_controls.path)
$root=Join-Path $repo ('out/prelive-main-integration-'+$Mode+'-main05');$cfg=if($Mode -eq 'release'){'Release'}else{'Debug'}
$options=@('-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake','-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed','-DVCPKG_TARGET_TRIPLET=x64-windows','-DVCPKG_MANIFEST_MODE=OFF','-DCMAKE_CXX_SCAN_FOR_MODULES=OFF','-DCMAKE_HAVE_LIB_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=')
if($Mode -eq 'asan'){$options+=@('-DPAS_PRELIVE_ASAN=ON','-DPAS_ASAN_SUPPORT_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')}
$configure='configure-prelive-main-integration-'+$Mode+'-main05'
Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $sourceRepo 'research/prelive_current');BuildRoot=$root;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;Operation='Configure';Configuration=$cfg;Inputs=$inputs;Options=$options}
$targets=@('diagnostic_writer_tests','owner_checkpoint_tests','owner_expiry_tests','producer_wait_tests','tap_reset_workload_tests','receipt_join_diagnose','owner_wait_tests','current_tests','active_body_tests','tail_guard_tests','tail_only_owner_review','tail_only_pixels_review','disabled_live_seam_tests','live_seam_packet_review','live_seam_runtime_review','runtime_finish_tests','current_chain','current_chain_phase_profile','diagnostic_phase_summary','prelive_audit')
$buildReceipts=@()
$taskNinja=Get-Content -LiteralPath (Join-Path $root 'build.ninja') -Raw
$usedLibs=@(Get-ChildItem -LiteralPath 'C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed/x64-windows/lib' -File -Recurse|Where-Object {$taskNinja.Contains($_.Name)}|ForEach-Object FullName)
$inputs+=@($usedLibs)+@(Get-ChildItem -LiteralPath 'C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed/x64-windows/bin' -File -Recurse|ForEach-Object FullName)
$objects=@(Get-Content -LiteralPath (Join-Path $root 'build.ninja')|ForEach-Object {$line=$_.Replace('\','/');if($line -match '^build (CMakeFiles/formal_core[.]dir/[^:]+[.]obj): CXX_COMPILER'){$Matches[1]}})
$decl=Get-Content -LiteralPath (Join-Path $sourceRepo 'CMakeLists.txt')|Where-Object {$_ -match '^add_library\(pas_core STATIC '};$expected=@([regex]::Matches($decl,'src/[^ )]+[.]cpp')|ForEach-Object {$_.Value})
$actual=@($objects|ForEach-Object {if($_ -match '/(src/[^/]+[.]cpp)[.]obj$'){$Matches[1]}else{throw 'unexpected formal core object'}})
if($objects.Count -ne 22 -or @(Compare-Object ($actual|Sort-Object) ($expected|Sort-Object)).Count){throw 'exact fresh 22 core object set'}
$groups=@();for($i=0;$i -lt $objects.Count;$i+=5){$groups+=@(@{name=('core-'+[int]($i/5+1));targets=@($objects|Select-Object -Skip $i -First 5)})};$groups+=@(@{name='libraries';targets=@('current_rails')});foreach($target in $targets){$groups+=@(@{name=$target.Replace('_','-');targets=@($target)})}
foreach($group in $groups){
 $build='build-prelive-main-integration-'+$Mode+'-main05-'+$group.name
 Stage $build $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $sourceRepo 'research/prelive_current');BuildRoot=$root;StageName=$build;ToolSnapshot=$tool;GateReceipt=$gate;Operation='Build';Configuration=$cfg;Inputs=@($inputs+(Join-Path $root 'CMakeCache.txt')+(Join-Path $root 'build.ninja'));Options=@('--target')+$group.targets}
 $buildReceipts+=@(Binding (Join-Path $out ($build+'/'+$build+'-verification.json')))
 if($group.name -ceq 'diagnostic-writer-tests'){
  $testExe=Join-Path $root 'diagnostic_writer_tests.exe';$seal=@{files=@(Binding $testExe)};$Operation='tests';$report=Join-Path $artifact 'diagnostic_writer_tests.json';$stage='test-prelive-main-integration-'+$Mode+'-main05-diagnostic-writer-tests'
  $runtimeDlls=@(Get-ChildItem -LiteralPath $root -File -Filter '*.dll'|ForEach-Object FullName)
  Native $stage $testExe @($report) @($inputs+$seal.files.path+$buildReceipts.path+$runtimeDlls) 900
  $value=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
  if($value.schema -cne 'pas.diagnostic-writer-tests.v1' -or $value.failed -ne 0 -or $value.tests_planned -ne 15 -or $value.tests_run -ne 15 -or $value.cases.Count -ne 15 -or @($value.cases|Where-Object {-not $_.pass}).Count){throw 'all 15 real writer and encoding cases required'}
  $earlyTest=@{test='diagnostic_writer_tests';report=(Binding $report);receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))};$Operation='build'
 }
}
$seal=@{files=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object {Binding $_.FullName})}
$buildSeal=Join-Path $artifact 'build-binding.json';NewJson $buildSeal @{schema='pas.cooperative-build.v1';source_commit=$commit;configuration=$Mode;source_manifest=(Binding $sourceManifest);core_build_plan=(Binding $coreProof);build_root=$root;files=$seal.files;configure_receipt=(Binding (Join-Path $out ($configure+'/'+$configure+'-verification.json')));build_receipts=$buildReceipts;device_operations=0;live_ready=$false}
$Operation='tests';$tests=@($earlyTest);foreach($target in $targets|Where-Object {$_ -notin @('diagnostic_writer_tests','receipt_join_diagnose','current_chain','current_chain_phase_profile','diagnostic_phase_summary','prelive_audit')}){
 $report=Join-Path $artifact ($target+'.json');$stage='test-prelive-main-integration-'+$Mode+'-main05-'+$target.Replace('_','-');Native $stage (Join-Path $root ($target+'.exe')) @($report) @($inputs+$buildSeal+$seal.files.path) 900
 $r=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json;if($r.failed -ne 0){throw ('cold regression failed '+$target)}
 $tests+=@(@{test=$target;report=(Binding $report);receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))})
}
$newRoots=@(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -Directory|Where-Object Name -like 'prelive-main-integration*');$candidateBytes=[long](($newRoots|ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -File -Recurse}|Measure-Object Length -Sum).Sum);if($candidateBytes -gt 6442450944){throw 'separate candidate cap'}
NewJson (Join-Path $artifact 'evaluation.json') @{schema='pas.cooperative-cold-evaluation.v1';source_commit=$commit;build=(Binding $buildSeal);tests=$tests;affected_cold_pass=$true;candidate_bytes=$candidateBytes;candidate_cap=6442450944;live_ready=$false;device_operations=0}
