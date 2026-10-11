param([ValidatePattern('^component[0-9]{2}$')][string]$Attempt='component01')
$ErrorActionPreference='Stop'
$verificationSource=$PSScriptRoot
. (Join-Path $PSScriptRoot '../../pre_emulator/common.ps1')
$script:maintenancePath=$PSCommandPath
$Operation='build'
$head=(& git -C $repo rev-parse HEAD)
$baseRoot=Join-Path $repo 'out/prelive-pre-emulator-release-main32'
$basePath=Join-Path $repo 'out/prelive-pre-emulator-evaluation-main32-release/build-binding.json'
$basePin=Binding $basePath
$base=Get-Content -Raw -LiteralPath $basePath|ConvertFrom-Json
CheckEntry $base.dependency_manifest
$dependencies=Get-Content -Raw -LiteralPath $base.dependency_manifest.path|ConvertFrom-Json
$donor=@($base.files|Where-Object {$_.path -ceq (Join-Path $baseRoot 'cold/exact_bvi_v3.lib')})
if($donor.Count -ne 1){throw 'one preserved exact cold-v3 donor library required'}
CheckEntry $donor[0]
$environmentPath=Join-Path $repo 'out/prelive-pre-emulator-evaluation-main32-release/compiler-environment-binding.json'
$environmentPin=Binding $environmentPath
$environment=Get-Content -Raw -LiteralPath $environmentPath|ConvertFrom-Json
foreach($pin in @($environment.environment,$environment.verification,$environment.state)){CheckEntry $pin}
$actualDependencies=@($dependencies.files|Where-Object {$_.path.Replace('\','/') -cmatch '/x64-windows/(?:include/nlohmann/|share/nlohmann_json/)'} )
if(-not $actualDependencies.Count){throw 'bounded actual json dependencies required'}
foreach($pin in $actualDependencies){CheckEntry $pin}
$sourcePaths=@('research/bvi_owner_cooperative/extract.cpp','research/bvi_owner_cooperative/extract.hpp','research/bvi_owner_cooperative/cold_verification/main.cpp','research/bvi_owner_cooperative/cold_verification/observation_equal.hpp','research/bvi_owner_cooperative/cold_verification/CMakeLists.txt','research/bvi_owner_cooperative/cold_verification/run.ps1')+@(Get-ChildItem -LiteralPath (Join-Path $repo 'research/bvi_cold_v3') -Filter '*.hpp'|ForEach-Object {[IO.Path]::GetRelativePath($repo,$_.FullName)})
$sourcePins=@($sourcePaths|ForEach-Object {Binding (Join-Path $repo $_)})
function VerifyActualInputs{foreach($pin in $sourcePins+$actualDependencies+$donor+@($basePin,$environmentPin,$environment.environment,$environment.verification,$environment.state)){CheckEntry $pin}}
function ComponentClosure([string]$stage){$pin=Binding (Join-Path $pkg ($stage+'-input-closure-verification.json'));$value=Get-Content -Raw -LiteralPath $pin.path|ConvertFrom-Json;if($value.status -cne 'COMPLETE' -or $value.pass -ne $true -or $value.before_verified -ne $true -or $value.after_verified -ne $true){throw 'complete before/after component input closure required'};return $pin}
$root=Join-Path $repo ('out/prelive-uniform-fnv-'+$Attempt)
$artifact=Join-Path $repo ('out/prelive-uniform-fnv-evaluation-'+$Attempt)
Fresh $root;Fresh $artifact;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$manifestPath=Join-Path $artifact 'actual-source-inputs.json'
NewJson $manifestPath @{schema='pas.uniform-rgb-fnv-component-inputs.v1';head_context=$head;source_dirty=$true;scope='Standalone algorithm actual inputs only; not a final candidate source manifest';files=$sourcePins;donor_library=$donor[0];actual_dependencies=$actualDependencies;device_operations=0;live_ready=$false;cost_qualification=$false}
$adapter=Join-Path $repo 'research/main_consolidation/cmake-stage.ps1'
$toolchain='C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake'
$guard=Get-Content -Raw -LiteralPath $gate|ConvertFrom-Json
$inputs=@($PSCommandPath,(Join-Path $repo 'research/pre_emulator/common.ps1'),$recipe,$adapter,(Join-Path $repo 'research/main_consolidation/stage.ps1'),$manifestPath,$basePath,$base.dependency_manifest.path,$environmentPath,$environment.environment.path,$environment.verification.path,$environment.state.path,$toolchain)+@($sourcePins.path)+@($actualDependencies.path)+@($donor.path)+(GateInputs)+@($guard.guard_source.path,$guard.process_source.path,$guard.cached_identity_controls.path,$guard.identity_wait_controls.path)
$options=@(('-DCMAKE_TOOLCHAIN_FILE='+$toolchain),('-DVCPKG_INSTALLED_DIR='+(Join-Path $repo 'out/vcpkg_installed')),'-DVCPKG_TARGET_TRIPLET=x64-windows','-DVCPKG_MANIFEST_MODE=OFF','-DCMAKE_CXX_SCAN_FOR_MODULES=OFF','-DCMAKE_HAVE_LIB_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=','-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL',('-DPAS_WORKSPACE='+$repo),('-DPAS_ORIGINAL_BUILD='+$baseRoot))
$configure='configure-prelive-uniform-fnv-'+$Attempt
VerifyActualInputs
Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=$verificationSource;BuildRoot=$root;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentPath;Operation='Configure';Configuration='Release';Inputs=$inputs;Options=$options}
$configureReceipt=Binding (Join-Path $out ($configure+'/'+$configure+'-verification.json'));$configureClosure=ComponentClosure $configure
$ninja=(Get-Content -Raw -LiteralPath (Join-Path $root 'build.ninja')).Replace('\','/')
if(@([regex]::Matches($ninja,'(?m)^build .+[.]obj: CXX_COMPILER')).Count -ne 1 -or $ninja -notmatch 'CMakeFiles/uniform_rgb_fnv_controls[.]dir/main[.]cpp[.]obj' -or $ninja -notmatch '/O2 /Ob2 /DNDEBUG' -or $ninja -notmatch '(?:/|-)MD(?: |\r?\n)'){throw 'one exact standalone TU and original Release flags required'}
$build='build-prelive-uniform-fnv-'+$Attempt
VerifyActualInputs
Stage $build $adapter @{ReceiptRepo=$repo;SourceRoot=$verificationSource;BuildRoot=$root;StageName=$build;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentPath;Operation='Build';Configuration='Release';Inputs=@($inputs+(Join-Path $root 'CMakeCache.txt')+(Join-Path $root 'build.ninja'));Options=@('--target','uniform_rgb_fnv_controls')}
$buildReceipt=Binding (Join-Path $out ($build+'/'+$build+'-verification.json'));$buildClosure=ComponentClosure $build
VerifyActualInputs
$outputs=@(Get-ChildItem -LiteralPath $root -Recurse -File|ForEach-Object {Binding $_.FullName})
$binary=Binding (Join-Path $root 'uniform_rgb_fnv_controls.exe')
$seal=@{files=@($binary)}
$reportPath=Join-Path $artifact 'controls.json';$test='test-prelive-uniform-fnv-'+$Attempt;$Operation='tests'
Native $test $binary.path @($reportPath) @($inputs+$outputs.path+$configureReceipt.path+$configureClosure.path+$buildReceipt.path+$buildClosure.path) 900
$report=Get-Content -Raw -LiteralPath $reportPath|ConvertFrom-Json
if($report.schema -cne 'pas.uniform-rgb-fnv-controls.v1' -or $report.pass -ne $true -or $report.failed -ne 0 -or $report.assertions -ne 19 -or $report.checks.Count -ne 19 -or @($report.checks|Where-Object {$_.pass -ne $true}).Count -or $report.table_bytes -ne 524288 -or $report.frame_cache_bytes -ne 0 -or $report.uniform_state_comparisons -ne 262144 -or $report.device_operations -ne 0 -or $report.device_endpoints -ne 0 -or $report.cost_qualification -ne $false -or $report.live_ready -ne $false){throw 'all 19 isolated algorithm controls without qualification required'}
VerifyActualInputs
$testReceipt=Binding (Join-Path $out ($test+'/'+$test+'-verification.json'));$testClosure=ComponentClosure $test
$proofPath=Join-Path $artifact 'component-proof.json'
NewJson $proofPath @{schema='pas.uniform-rgb-fnv-component-proof.v1';head_context=$head;source_dirty=$true;candidate_qualification_issued=$false;cost_qualification=$false;live_ready=$false;device_operations=0;actual_source_inputs=(Binding $manifestPath);compiler_environment=$environmentPin;donor_library=$donor[0];binary=$binary;compiled_outputs=$outputs;configure_receipt=$configureReceipt;configure_input_closure=$configureClosure;build_receipt=$buildReceipt;build_input_closure=$buildClosure;controls_report=(Binding $reportPath);controls_receipt=$testReceipt;controls_input_closure=$testClosure;before_verified=$true;after_verified=$true}
Write-Output $proofPath
