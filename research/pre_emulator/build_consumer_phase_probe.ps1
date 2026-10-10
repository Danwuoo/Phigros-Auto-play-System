param([Parameter(Mandatory)][string]$BaseBinding,
      [ValidatePattern('^main[0-9]{2}$')][string]$Attempt='main37',
      [string]$Profile='configs/phigros-hd-assist-five-lead35.json')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
$script:maintenancePath=$PSCommandPath
$Operation='build'
$baseCommit='aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de'
$head=(& git -C $repo rev-parse HEAD)
if($LASTEXITCODE -or -not $head -or (& git -C $repo branch --show-current) -cne 'main' -or (& git -C $repo status --porcelain)){throw 'exact clean main diagnostic source required'}

function ProbeRead($entry){CheckEntry $entry;return Get-Content -LiteralPath $entry.path -Raw|ConvertFrom-Json}
function ProbeRelative([string]$path){
 $absolute=[IO.Path]::GetFullPath($path)
 if(-not $absolute.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'probe input outside main'}
 return [IO.Path]::GetRelativePath($repo,$absolute).Replace('\','/')
}
function ProbeClosure([string]$stage){
 $pin=Binding (Join-Path $pkg ($stage+'-input-closure-verification.json'))
 $value=ProbeRead $pin
 if($value.schema -cne 'pas.current-input-closure-verification.v1' -or $value.status -cne 'COMPLETE' -or $value.pass -ne $true -or $value.before_verified -ne $true -or $value.after_verified -ne $true){throw 'native probe build input closure incomplete'}
 return $pin
}
function VerifyProbeInputs {
 if((& git -C $repo rev-parse HEAD) -cne $head -or (& git -C $repo branch --show-current) -cne 'main' -or (& git -C $repo status --porcelain)){throw 'diagnostic source changed during build'}
 foreach($entry in @($sourceEntries)+@($staticInputs)+@($selectedDependencies)+@($profilePin,$basePin,$baseBuildPin,$baseBuild.source_manifest,$baseBuild.dependency_manifest,$baseZPin,$environmentPin)+@($environmentProof.environment,$environmentProof.verification,$environmentProof.state)){CheckEntry $entry}
}
function ProbeCandidateBytes {
 $roots=@(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -Directory|Where-Object { $_.Name -match '^prelive-pre-emulator-(?:(?:release|debug|asan)-main[0-9]{2}|evaluation-main[0-9]{2}-(?:release|debug|asan)|fixture-repo-main[0-9]{2}(?:-(?:release|debug|asan))?|planning)$' -or $_.Name -match '^prelive-(?:noise-repair|consumer-phase-probe|narrow-reader)-(?:evaluation-)?main[0-9]{2}$' })
 $bytes=[long](($roots|ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -File -Recurse}|Measure-Object Length -Sum).Sum)
 if($bytes -gt 6442450944){throw 'unchanged aggregate candidate cap'}
 return $bytes
}

$basePin=Binding ([IO.Path]::GetFullPath($BaseBinding))
$base=ProbeRead $basePin
if($base.schema -cne 'pas.narrow-noise-repair-build.v1' -or $base.configuration -cne 'release' -or $base.base_source_commit -cne $baseCommit -or $base.scope -cne 'noise_and_unrun_abba_only' -or $base.whole_candidate_qualification -ne $false -or $base.live_ready -ne $false -or $base.device_operations -ne 0){throw 'original narrow repair provenance required, without qualification'}
$baseBuildPin=$base.base_release_build
$baseBuild=ProbeRead $baseBuildPin
if($baseBuild.schema -cne 'pas.cooperative-build.v1' -or $baseBuild.source_commit -cne $baseCommit -or $baseBuild.configuration -cne 'release'){throw 'preserved main32 Release static provenance required'}
$baseRoot=[IO.Path]::GetFullPath($baseBuild.build_root)
if($baseRoot -cne [IO.Path]::GetFullPath((Join-Path $repo 'out/prelive-pre-emulator-release-main32'))){throw 'original Release root identity'}
$staticInputs=@()
foreach($relative in @('cold/current_rails.lib','cold/current_front.lib','cold/own_bridge.lib','cold/exact_bvi_v3.lib','production/pas_core.lib')){
 $path=[IO.Path]::GetFullPath((Join-Path $baseRoot $relative))
 $entries=@($baseBuild.files|Where-Object {[IO.Path]::GetFullPath($_.path) -ceq $path})
 $repairEntries=@($base.base_static_inputs|Where-Object {[IO.Path]::GetFullPath($_.path) -ceq $path})
 if($entries.Count -ne 1 -or $repairEntries.Count -ne 1 -or $entries[0].bytes -ne $repairEntries[0].bytes -or $entries[0].sha256 -cne $repairEntries[0].sha256){throw 'five original imported library SHA joins required'}
 CheckEntry $entries[0];$staticInputs+=@($entries[0])
}
$baseZRows=@($baseBuild.files|Where-Object {[IO.Path]::GetFullPath($_.path) -ceq [IO.Path]::GetFullPath((Join-Path $baseRoot 'cold/z.dll'))})
if($baseZRows.Count -ne 1){throw 'one original copied zlib runtime required'}
$baseZPin=$baseZRows[0];CheckEntry $baseZPin
$baseSource=ProbeRead $baseBuild.source_manifest
if($baseSource.source_commit -cne $baseCommit){throw 'imported static source identity'}
# These are the source/interfaces compiled into the imported libraries. The
# changed diagnostic Profile holder is outside this closure, entirely inline.
$staticSourceEntries=@($baseSource.files|Where-Object {
 $relative=ProbeRelative $_.path
 $relative -cmatch '^(?:src/|include/|research/bvi_cold_v3/|research/bvi_owner_cooperative/|research/hold_front_current/)' -or $relative -in @('CMakeLists.txt','research/prelive_current/current.cpp','research/prelive_current/current.hpp','research/prelive_current/tail_current.hpp','research/prelive_current/own_bridge.cpp','research/prelive_current/own_bridge.hpp','research/prelive_current/receipt_stream.hpp')
})
foreach($entry in $staticSourceEntries){CheckEntry $entry}

$dependencyManifest=ProbeRead $baseBuild.dependency_manifest
if($dependencyManifest.source_commit -cne $baseCommit -or $dependencyManifest.release_and_debug -ne $true -or $dependencyManifest.protoc_and_plugin -ne $true){throw 'original dependency manifest provenance'}
$dependencyRoot=[IO.Path]::GetFullPath($dependencyManifest.root)
$selectedPaths=@(Get-ChildItem -LiteralPath (Join-Path $dependencyRoot 'include/nlohmann') -File -Recurse|ForEach-Object FullName)
foreach($relative in @('share/nlohmann_json','share/zlib')){$selectedPaths+=@(Get-ChildItem -LiteralPath (Join-Path $dependencyRoot $relative) -File -Recurse|Where-Object {$_.Extension -ceq '.cmake' -or $_.Extension -ceq '.natvis'}|ForEach-Object FullName)}
$selectedPaths+=@('include/zlib.h','include/zconf.h','lib/z.lib','debug/lib/zd.lib','bin/z.dll'|ForEach-Object {Join-Path $dependencyRoot $_})
$selectedDependencies=@()
foreach($path in $selectedPaths|Sort-Object -Unique){
 $rows=@($dependencyManifest.files|Where-Object {[IO.Path]::GetFullPath($_.path) -ceq [IO.Path]::GetFullPath($path)})
 if($rows.Count -ne 1){throw 'one selected json/zlib original dependency pin required'}
 CheckEntry $rows[0];$selectedDependencies+=@($rows[0])
}
# No grpc/protobuf libraries or unrelated installed files enter native inputs.
$environmentBinding=Join-Path $repo 'out/prelive-pre-emulator-evaluation-main32-release/compiler-environment-binding.json'
$environmentPin=Binding $environmentBinding
$environmentProof=ProbeRead $environmentPin
if($environmentProof.schema -cne 'pas.msvc-environment-binding.v1' -or $environmentProof.source_commit -cne $baseCommit -or $environmentProof.device_operations -ne 0 -or $environmentProof.live_ready -ne $false){throw 'original compiler environment provenance'}
foreach($entry in @($environmentProof.environment,$environmentProof.verification,$environmentProof.state)){CheckEntry $entry}
$environmentReceipt=ProbeRead $environmentProof.verification;Receipt $environmentReceipt.stage
$environment=ProbeRead $environmentProof.environment
if($environment.schema -cne 'pas.msvc-environment.v1' -or $environment.variables.VCToolsVersion -cne '14.51.36231' -or $environment.variables.VisualStudioVersion -cne '18.0'){throw 'same pinned Release MSVC environment required'}
$profilePath=if([IO.Path]::IsPathRooted($Profile)){[IO.Path]::GetFullPath($Profile)}else{Join-Path $repo $Profile}
$profilePin=Binding $profilePath
$null=ProbeRelative $profilePath
$sourcePaths=@(& git -c core.quotePath=false -C $repo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -cmatch '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'})
if($LASTEXITCODE){throw 'current diagnostic source inventory'}
$sourceEntries=@($sourcePaths|ForEach-Object {Binding (Join-Path $repo $_)})
$authorization=Join-Path $repo 'docs/research/live-readiness-20261007/NOISE_REPAIR_AUTHORIZATION_20261011.json'
$authorizationPin=Binding $authorization
$authorizationValue=ProbeRead $authorizationPin
if($authorizationValue.status -cne 'HUMAN_AUTHORIZED' -or $authorizationValue.scope -cne 'noise_and_unrun_abba_only' -or $authorizationValue.device_operations -ne 0 -or $authorizationValue.live_ready -ne $false -or $authorizationValue.create_new_thread){throw 'unchanged cold-only authorization'}
$scopeDocument=Join-Path $repo 'docs/research/live-readiness-20261007/NOISE_REPAIR_SCOPE_20261011.md'
$approval=Join-Path $repo 'docs/research/live-readiness-20261007/CAPACITY_APPROVAL_20261008.json'
$approvalValue=Get-Content -LiteralPath $approval -Raw|ConvertFrom-Json
if($approvalValue.status -cne 'HUMAN_APPROVED' -or $approvalValue.retained_prelive_cap_bytes -ne 51539607552){throw 'unchanged 48 GiB capacity authorization'}
$artifact=Join-Path $repo ('out/prelive-consumer-phase-probe-evaluation-'+$Attempt)
$root=Join-Path $repo ('out/prelive-consumer-phase-probe-'+$Attempt)
Fresh $artifact;Fresh $root
[IO.Directory]::CreateDirectory($artifact)|Out-Null
$sourceManifest=Join-Path $artifact 'source-manifest.json'
NewJson $sourceManifest @{schema='pas.cooperative-source.v1';source_commit=$head;dirty=$false;files=$sourceEntries;diagnostic_only=$true;cost_qualification=$false;whole_candidate_qualification=$false;device_operations=0;live_ready=$false}
$dependencySelection=Join-Path $artifact 'selected-dependency-manifest.json'
NewJson $dependencySelection @{schema='pas.consumer-phase-probe-dependencies.v1';source_commit=$head;original_dependency_manifest=$baseBuild.dependency_manifest;original_dependency_source_commit=$baseCommit;files=$selectedDependencies;selection='Actual nlohmann-json and zlib headers/configuration/import/runtime files only';diagnostic_only=$true;cost_qualification=$false;device_operations=0;live_ready=$false}
$corePlan=Join-Path $artifact 'diagnostic-core-build-plan.json'
$declaredTUs=@(@{target='current_chain_consumer_phase_probe';source=(Join-Path $repo 'research/prelive_current/chain.cpp')},@{target='consumer_phase_summary';source=(Join-Path $repo 'research/prelive_current/diagnostic_phase_summary.cpp')})
NewJson $corePlan @{schema='pas.consumer-phase-probe-core-build-plan.v1';source_commit=$head;base_source_commit=$baseCommit;configuration='release';diagnostic_only=$true;formal_rebuilt=$false;imported_static_libraries=$staticInputs;imported_static_source_files=$staticSourceEntries;targets=@('current_chain_consumer_phase_probe','consumer_phase_summary');translation_units=$declaredTUs;compiler_environment=$environmentPin;compiler_snapshot=(Binding $tool);profile=$profilePin;compile_definition='PAS_DIAGNOSTIC_PHASE_PROFILE=1';phase_row_cap=9;topology_raw_byte_cap=65536;topology_logical_processor_cap=256;changed_profile_holder_freshly_instantiated_in_chain_TU_only=$true;pas_emulator_and_old_chain_objects_excluded=$true;cost_qualification=$false;whole_candidate_qualification=$false;device_operations=0;live_ready=$false}
$adapter=Join-Path $repo 'research/main_consolidation/cmake-stage.ps1'
$toolchain='C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake'
$guard=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$inputs=@($PSCommandPath,(Join-Path $PSScriptRoot 'common.ps1'),$recipe,$adapter,(Join-Path $repo 'research/main_consolidation/stage.ps1'),$sourceManifest,$dependencySelection,$corePlan,$basePin.path,$baseBuildPin.path,$baseBuild.source_manifest.path,$baseBuild.dependency_manifest.path,$baseZPin.path,$profilePath,$authorization,$scopeDocument,$approval,$toolchain,$environmentBinding,$environmentProof.environment.path,$environmentProof.verification.path,$environmentProof.state.path)+@($sourceEntries.path)+@($staticInputs.path)+@($selectedDependencies.path)+(GateInputs)+@($guard.guard_source.path,$guard.process_source.path,$guard.cached_identity_controls.path,$guard.identity_wait_controls.path)
$options=@(('-DCMAKE_TOOLCHAIN_FILE='+$toolchain),('-DVCPKG_INSTALLED_DIR='+(Join-Path $repo 'out/vcpkg_installed')),'-DVCPKG_TARGET_TRIPLET=x64-windows','-DVCPKG_MANIFEST_MODE=OFF','-DCMAKE_CXX_SCAN_FOR_MODULES=OFF','-DCMAKE_HAVE_LIB_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=','-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL',('-DPAS_WORKSPACE='+$repo),('-DPAS_ORIGINAL_BUILD='+$baseRoot))
$configure='configure-prelive-consumer-phase-probe-'+$Attempt
VerifyProbeInputs;$null=ProbeCandidateBytes
Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $PSScriptRoot 'consumer_phase_probe');BuildRoot=$root;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentBinding;Operation='Configure';Configuration='Release';Inputs=$inputs;Options=$options}
$configureReceipt=Binding (Join-Path $out ($configure+'/'+$configure+'-verification.json'))
$configureClosure=ProbeClosure $configure
$ninja=(Get-Content -LiteralPath (Join-Path $root 'build.ninja') -Raw).Replace('\','/')
$compileRows=@([regex]::Matches($ninja,'(?m)^build ([^\r\n:]+[.]obj): CXX_COMPILER[^\r\n]*')|ForEach-Object {$_.Groups[1].Value})
if($compileRows.Count -ne 2 -or @($compileRows|Where-Object {$_ -cmatch '^CMakeFiles/current_chain_consumer_phase_probe[.]dir/.*chain[.]cpp[.]obj$'}).Count -ne 1 -or @($compileRows|Where-Object {$_ -cmatch '^CMakeFiles/consumer_phase_summary[.]dir/.*diagnostic_phase_summary[.]cpp[.]obj$'}).Count -ne 1 -or $ninja -notmatch 'PAS_DIAGNOSTIC_PHASE_PROFILE=1' -or $ninja -notmatch '/O2 /Ob2 /DNDEBUG' -or $ninja -notmatch '(?:/|-)MD(?: |\r?\n)'){throw 'exact two fresh diagnostic TUs and original Release flags required'}
$build='build-prelive-consumer-phase-probe-'+$Attempt
VerifyProbeInputs;$null=ProbeCandidateBytes
Stage $build $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $PSScriptRoot 'consumer_phase_probe');BuildRoot=$root;StageName=$build;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentBinding;Operation='Build';Configuration='Release';Inputs=@($inputs+(Join-Path $root 'CMakeCache.txt')+(Join-Path $root 'build.ninja'));Options=@('--target','consumer_phase_probe')}
$buildReceipt=Binding (Join-Path $out ($build+'/'+$build+'-verification.json'))
$buildClosure=ProbeClosure $build
VerifyProbeInputs
$outputs=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object {Binding $_.FullName})
$objects=@($outputs|Where-Object {[IO.Path]::GetExtension($_.path) -ceq '.obj'})
$targetObjects=@($objects|Where-Object {$_.path.Replace('\','/') -cmatch '/CMakeFiles/(?:current_chain_consumer_phase_probe|consumer_phase_summary)[.]dir/'})
if($targetObjects.Count -ne 2 -or @($targetObjects|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'chain.cpp.obj'}).Count -ne 1 -or @($targetObjects|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'diagnostic_phase_summary.cpp.obj'}).Count -ne 1){throw 'only declared diagnostic target objects permitted'}
foreach($object in $objects|Where-Object {$_ -notin $targetObjects}){if($object.path.Replace('\','/') -cnotmatch '/(?:CompilerIdCXX/CMakeCXXCompilerId[.]obj|CMakeFiles/ShowIncludes/main[.]obj)$'){throw 'unexpected formal or unrelated object in diagnostic root'}}
$files=@(Binding (Join-Path $root 'current_chain_consumer_phase_probe.exe');Binding (Join-Path $root 'consumer_phase_summary.exe');Binding (Join-Path $root 'z.dll'))
$z=@($selectedDependencies|Where-Object {$_.path -ceq (Join-Path $dependencyRoot 'bin/z.dll')})
if($z.Count -ne 1 -or $files[2].bytes -ne $z[0].bytes -or $files[2].sha256 -cne $z[0].sha256 -or $files[2].bytes -ne $baseZPin.bytes -or $files[2].sha256 -cne $baseZPin.sha256){throw 'original zlib runtime bytes required'}
$bindingPath=Join-Path $artifact 'build-binding.json'
NewJson $bindingPath @{schema='pas.consumer-phase-probe-build.v1';source_commit=$head;configuration='release';diagnostic_only=$true;cost_qualification=$false;whole_candidate_qualification=$false;device_operations=0;live_ready=$false;source_manifest=(Binding $sourceManifest);dependency_manifest=(Binding $dependencySelection);original_dependency_manifest=$baseBuild.dependency_manifest;core_build_plan=(Binding $corePlan);base_repair_build=$basePin;base_release_build=$baseBuildPin;base_source_commit=$baseCommit;profile=$profilePin;build_root=$root;files=$files;compiled_outputs=$outputs;declared_translation_units=$declaredTUs;base_static_inputs=$staticInputs;base_runtime_dll=$baseZPin;compiler_environment=$environmentPin;compiler_environment_reused=$true;configure_receipt=$configureReceipt;configure_input_closure=$configureClosure;build_receipts=@($buildReceipt);build_input_closures=@($buildClosure);tests_run=0;measurements_run=0;candidate_bytes=(ProbeCandidateBytes);candidate_cap=6442450944;tests_scope_authorization=$authorizationPin}
Write-Output $bindingPath
