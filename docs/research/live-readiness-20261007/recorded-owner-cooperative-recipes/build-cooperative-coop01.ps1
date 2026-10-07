param([ValidateSet('release','debug','asan')][string]$Mode='release')
$ErrorActionPreference='Stop'
$repo='C:\Users\wurre\Desktop\PAS-windows-readiness-20261007';$sourceRepo='C:\Users\wurre\Desktop\PAS-owner-cooperative-20261007';$oldRepo='C:\Users\wurre\Desktop\PAS-journal-cost-20261007'
$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006';$pwsh=(Get-Command pwsh).Source;$Operation='build';$Campaign='';$script:inputClosures=@{};$script:maintenancePath=$PSCommandPath
$gate=Join-Path $out 'guard-v3-native-01/native-gate.json';$tool=Join-Path $out 'tool-snapshot-01/tool-snapshot.json';$recipe=Join-Path $repo 'research/prelive_current/run.ps1';$tok=$null;$err=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tok,[ref]$err)
if($err.Count){throw 'shared recipe parse'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage','Native')){
 $d=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name);if($d.Count -ne 1){throw 'shared function'};$t=$d[0].Extent.Text
 if($name -ceq 'Native'){$t=$t.Replace('$PSCommandPath','$script:maintenancePath').Replace('tools/zero_miss_windows/invoke.ps1','research/windows_process_guard_v3/invoke.ps1')};if($name -ceq 'Capacity'){$t=$t.Replace('12884901888','34359738368')};if($name -ceq 'GateInputs'){$t=$t.Replace('pas.windows-offline-process-gate.v1','pas.windows-offline-process-gate.v3')};. ([scriptblock]::Create($t))
}
$commit='ff18b048971b2415dbdde8d6c726c422e7dab4a7';if((& git -C $sourceRepo rev-parse HEAD) -cne $commit -or (& git -C $sourceRepo status --porcelain)){throw 'exact clean candidate required'}
$artifact=Join-Path $repo ('out/prelive-cooperative-evaluation-coop01-'+$Mode);Fresh $artifact;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$plan=Join-Path $sourceRepo 'docs/research/live-readiness-20261007/OWNER_COOPERATIVE_CHECKPOINT_20261007.md';$coldSeal=Join-Path $repo 'out/prelive-journal-evaluation-journal10/candidate-builds.json';$cold=Get-Content -LiteralPath $coldSeal -Raw|ConvertFrom-Json
if($cold.source_commit -cne '3f23faf8bb91a4a974c33463e7232d3507c1dcc1' -or -not $cold.complete_current_commit_core_verified -or $cold.builds.Count -ne 3){throw 'qualified exact donor core source'}
CheckEntry $cold.source_manifest;$oldSource=Get-Content -LiteralPath $cold.source_manifest.path -Raw|ConvertFrom-Json
$pairs=@();foreach($relative in @(& git -C $sourceRepo ls-files src include)){
 $original=@($oldSource.files|Where-Object path -ceq (Join-Path $oldRepo $relative));if($original.Count -ne 1){throw ('old core source binding '+$relative)};CheckEntry $original[0];$current=Binding (Join-Path $sourceRepo $relative);if($current.sha256 -cne $original[0].sha256 -or $current.bytes -ne $original[0].bytes){throw ('core source differs '+$relative)};$pairs+=@(@{original=$original[0];current=$current})
}
$donorBuild=@($cold.builds|Where-Object configuration -ceq $Mode);if($donorBuild.Count -ne 1){throw 'configuration donor'};$donorBuild=$donorBuild[0]
$cores=@(@($donorBuild.files)+@($donorBuild.library_donors.file)|Where-Object path -like '*formal_core.lib');if($cores.Count -ne 1){throw 'exact core library'};CheckEntry $cores[0];$CoreDonor=Split-Path $cores[0].path
$donorReceipts=@($donorBuild.configure_receipt)+@($donorBuild.build_receipts)+@($donorBuild.library_donors.receipts)
foreach($r in $donorReceipts){CheckEntry $r;$v=Get-Content -LiteralPath $r.path -Raw|ConvertFrom-Json;Receipt $v.stage}
$coreProof=Join-Path $artifact 'core-object-reuse.json';NewJson $coreProof @{schema='pas.exact-byte-core-reuse.v1';original_source_commit=$cold.source_commit;candidate_source_commit=$commit;configuration=$Mode;library=$cores[0];source_header_pairs=$pairs;original_build_receipts=$donorReceipts;formal_core_translation_units=22;all_core_inputs_identical=$true;compiler_snapshot=(Binding $tool);original_build_cache=($donorBuild.files|Where-Object path -like '*CMakeCache.txt'|Select-Object -First 1);live_ready=$false;device_operations=0}
$sourceFiles=@(& git -C $sourceRepo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -match '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'}|ForEach-Object {Join-Path $sourceRepo $_})
$sourceManifest=Join-Path $artifact 'source-manifest.json';NewJson $sourceManifest @{schema='pas.cooperative-source.v1';source_commit=$commit;dirty=$false;files=@($sourceFiles|ForEach-Object {Binding $_});device_operations=0;live_ready=$false}
$adapter=Join-Path $oldRepo 'research/windows_owner_wake/cmake-stage-v2.ps1';$g=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$deps=@(Get-ChildItem -LiteralPath 'C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed/x64-windows/include' -File -Recurse|Where-Object {$_.FullName -match '\\(nlohmann|zlib|google|grpc|gtest)\\' -or $_.Name -in @('zlib.h','zconf.h')}|ForEach-Object FullName)
$inputs=@($PSCommandPath,$recipe,$adapter,$plan,$sourceManifest,$coreProof,$coldSeal,$cold.source_manifest.path,$cores[0].path)+$sourceFiles+$pairs.original.path+$donorReceipts.path+$deps+(GateInputs)+@($g.guard_source.path,$g.process_source.path,$g.cached_identity_controls.path,$g.identity_wait_controls.path)
$root=Join-Path $repo ('out/prelive-cooperative-'+$Mode+'-coop01');$cfg=if($Mode -eq 'release'){'Release'}else{'Debug'}
$options=@('-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake','-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed','-DVCPKG_TARGET_TRIPLET=x64-windows','-DVCPKG_MANIFEST_MODE=OFF','-DCMAKE_CXX_SCAN_FOR_MODULES=OFF','-DCMAKE_HAVE_LIB_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=',('-DPAS_PRELIVE_CORE_DONOR='+$CoreDonor))
if($Mode -eq 'asan'){$options+=@('-DPAS_PRELIVE_ASAN=ON','-DPAS_ASAN_SUPPORT_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')}
$configure='configure-prelive-cooperative-'+$Mode+'-coop01'
Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $sourceRepo 'research/prelive_current');BuildRoot=$root;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;Operation='Configure';Configuration=$cfg;Inputs=$inputs;Options=$options}
$targets=@('owner_checkpoint_tests','receipt_join_diagnose','owner_wait_tests','current_tests','active_body_tests','tail_guard_tests','tail_only_owner_review','tail_only_pixels_review','disabled_live_seam_tests','live_seam_packet_review','live_seam_runtime_review','runtime_finish_tests','current_chain','prelive_audit')
$build='build-prelive-cooperative-'+$Mode+'-coop01'
Stage $build $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $sourceRepo 'research/prelive_current');BuildRoot=$root;StageName=$build;ToolSnapshot=$tool;GateReceipt=$gate;Operation='Build';Configuration=$cfg;Inputs=@($inputs+(Join-Path $root 'CMakeCache.txt')+(Join-Path $root 'build.ninja'));Options=@('--target')+$targets}
$seal=@{files=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object {Binding $_.FullName})}
$buildSeal=Join-Path $artifact 'build-binding.json';NewJson $buildSeal @{schema='pas.cooperative-build.v1';source_commit=$commit;configuration=$Mode;source_manifest=(Binding $sourceManifest);core_reuse=(Binding $coreProof);build_root=$root;files=$seal.files;configure_receipt=(Binding (Join-Path $out ($configure+'/'+$configure+'-verification.json')));build_receipts=@(Binding (Join-Path $out ($build+'/'+$build+'-verification.json')));device_operations=0;live_ready=$false}
$Operation='tests';$tests=@();foreach($target in $targets|Where-Object {$_ -notin @('receipt_join_diagnose','current_chain','prelive_audit')}){
 $report=Join-Path $artifact ($target+'.json');$stage='test-prelive-cooperative-'+$Mode+'-coop01-'+$target.Replace('_','-');Native $stage (Join-Path $root ($target+'.exe')) @($report) @($inputs+$buildSeal+$seal.files.path) 900
 $r=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json;if($r.failed -ne 0){throw ('cold regression failed '+$target)}
 $tests+=@(@{test=$target;report=(Binding $report);receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))})
}
if($Mode -eq 'release'){
 $raw=Join-Path $repo 'out/prelive-production-journal-normal03/hold';$report=Join-Path $artifact 'hold-receipt-join-diagnosis.json';$stage='diagnose-prelive-cooperative-coop01-hold';Native $stage (Join-Path $root 'receipt_join_diagnose.exe') @($raw,$report) @($inputs+$buildSeal+$seal.files.path+@(Get-ChildItem -LiteralPath $raw -File -Recurse|ForEach-Object FullName)) 900
}
$newRoots=@(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -Directory|Where-Object Name -like 'prelive-cooperative*');$candidateBytes=[long](($newRoots|ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -File -Recurse}|Measure-Object Length -Sum).Sum);if($candidateBytes -gt 6442450944){throw 'separate candidate cap'}
NewJson (Join-Path $artifact 'evaluation.json') @{schema='pas.cooperative-cold-evaluation.v1';source_commit=$commit;build=(Binding $buildSeal);tests=$tests;affected_cold_pass=$true;candidate_bytes=$candidateBytes;candidate_cap=6442450944;live_ready=$false;device_operations=0}
