param([ValidatePattern('^main[0-9]{2}$')][string]$Attempt='main47')
$ErrorActionPreference='Stop'
# The source and process helpers are shared; LoadBuilds is deliberately unused.
# main32's Debug/ASan compiler roots are archived, and none is a delta donor.
. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $PSScriptRoot 'component_delta.ps1')
$script:maintenancePath=$PSCommandPath
$Operation='build'
$baseCommit='aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de'
$scope='noise_and_unrun_abba_only'
$commit=(& git -C $repo rev-parse HEAD)
if($LASTEXITCODE -or -not $commit -or (& git -C $repo branch --show-current) -cne 'main' -or (& git -C $repo status --porcelain)){throw 'exact clean main source required'}
& git -C $repo merge-base --is-ancestor $baseCommit $commit
if($LASTEXITCODE){throw 'main32 must be a source ancestor'}

function ReadBound($entry){CheckEntry $entry;return (Get-Content -LiteralPath $entry.path -Raw|ConvertFrom-Json)}
function VerifyReceiptPin($entry){$value=ReadBound $entry;Receipt $value.stage;return $value}
function BaseFile([string]$relative){
 $wanted=[IO.Path]::GetFullPath((Join-Path $baseRoot $relative))
 $found=@($baseBuild.files|Where-Object {[IO.Path]::GetFullPath($_.path) -ceq $wanted})
 if($found.Count -ne 1){throw ('one preserved main32 file required: '+$relative)}
 CheckEntry $found[0];return $found[0]
}
function AssertCurrentSource {
 if((& git -C $repo rev-parse HEAD) -cne $commit -or (& git -C $repo branch --show-current) -cne 'main' -or (& git -C $repo status --porcelain)){throw 'source changed during narrow build'}
 foreach($entry in $sourceEntries){CheckEntry $entry}
}
function AssertArchivePreserved($entry){
 # Historical compiler ZIPs are preserved evidence, not new compiler inputs.
 # Their small verified proof anchors the original SHA; do not rehash large ZIPs.
 if(-not $entry.path -or $entry.sha256 -cnotmatch '^[0-9a-f]{64}$' -or $entry.bytes -le 0 -or -not (Test-Path -LiteralPath $entry.path -PathType Leaf) -or (Get-Item -LiteralPath $entry.path).Length -ne $entry.bytes){throw 'preserved compiler archive presence/length required'}
}
function AssertBaseUnchanged {
 CheckEntry $inventoryPin
 foreach($entry in @($staticInputs)+@($baseExecutables)+@($baseRuntimeDlls)){CheckEntry $entry}
 foreach($entry in $snapshot.files.copy){CheckEntry $entry}
 foreach($entry in $selectedDependencies){CheckEntry $entry}
 foreach($proof in $oldProofs){
  CheckEntry $proof.evaluation;CheckEntry $proof.build
  if($proof.archive_proof){
   $archiveProof=ReadBound $proof.archive_proof
   if($archiveProof.archive.path -cne $proof.archive.path -or $archiveProof.archive.bytes -ne $proof.archive.bytes -or $archiveProof.archive.sha256 -cne $proof.archive.sha256){throw 'preserved archive/proof SHA join'}
   AssertArchivePreserved $proof.archive
  }
 }
 CheckEntry $baseBuild.source_manifest;CheckEntry $baseBuild.dependency_manifest;CheckEntry $baseBuild.core_build_plan
}
function ClosurePin([string]$stage){
 $pin=Binding (Join-Path $pkg ($stage+'-input-closure-verification.json'))
 $value=ReadBound $pin
 if($value.schema -cne 'pas.current-input-closure-verification.v1' -or -not $value.pass -or -not $value.before_verified -or -not $value.after_verified -or $value.status -cne 'COMPLETE'){throw 'verified before/after native input closure required'}
 return $pin
}
function BuildProcessEvidence([string]$stage){
 $e=@{}
 foreach($pair in @(@('specification',(Join-Path $out ($stage+'-inputs/spec.json'))),@('input_closure',(Join-Path $pkg ($stage+'-input-closure-verification.json'))),@('result',(Join-Path $out ($stage+'/'+$stage+'-result.json'))),@('verification',(Join-Path $out ($stage+'/'+$stage+'-verification.json'))),@('freeze',(Join-Path $out ($stage+'/freeze.json'))),@('state',(Join-Path $out ($stage+'/state.json'))),@('contract',(Join-Path $out ($stage+'/contract.json'))),@('command',(Join-Path $out ($stage+'/'+$stage+'-command.json'))))){$e[$pair[0]]=Binding $pair[1]}
 return $e
}
function CheckCandidateCapacity {
 # Retain the existing aggregate candidate ceiling, including the new delta roots.
 $roots=@(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -Directory|Where-Object { $_.Name -match '^prelive-pre-emulator-(?:(?:release|debug|asan)-main[0-9]{2}|evaluation-main[0-9]{2}-(?:release|debug|asan)|fixture-repo-main[0-9]{2}(?:-(?:release|debug|asan))?|planning)$' -or $_.Name -match '^prelive-(?:noise-repair|consumer-phase-probe|narrow-reader)-(?:evaluation-)?main[0-9]{2}$' })
 $bytes=[long](($roots|ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -File -Recurse}|Measure-Object Length -Sum).Sum)
 if($bytes -gt 6442450944){throw 'separate candidate cap'}
 return $bytes
}

$inventoryPath=Join-Path $repo 'out/prelive-pre-emulator-planning/main32-stage-inventory.json'
$inventoryPin=Binding $inventoryPath
$inventory=ReadBound $inventoryPin
if($inventory.schema -cne 'pas.main32-stage-inventory.v1' -or $inventory.source_commit -cne $baseCommit -or $inventory.device_operations -ne 0 -or $inventory.live_ready -or $inventory.cost_ready -or $inventory.qualification_issued -or -not $inventory.original_STOPs_preserved -or $inventory.cold.Count -ne 3 -or $inventory.prefix3063 -cne 'PASS' -or $inventory.faults4 -cne 'PASS' -or $inventory.production.integrity_rows -ne 9 -or $inventory.abba36 -cne 'NOT_RUN'){throw 'original main32 prerequisite inventory required'}
$snapshot=ReadBound $inventory.source_snapshot
if($snapshot.source_commit -cne $baseCommit -or $snapshot.files.Count -ne 913 -or $snapshot.qualification_issued -or $snapshot.device_operations -ne 0){throw 'preserved main32 source snapshot required'}
foreach($row in $snapshot.files){CheckEntry $row.copy;if($row.copy.bytes -ne $row.original.bytes -or $row.copy.sha256 -cne $row.original.sha256){throw 'lossless main32 source snapshot required'}}

$oldProofs=@();$oldInputs=@($inventoryPath,$inventory.source_snapshot.path)+@($snapshot.files.copy.path)
foreach($mode in @('release','debug','asan')){
 $row=@($inventory.cold|Where-Object mode -ceq $mode)
 if($row.Count -ne 1 -or $row[0].groups -ne 25){throw 'original three 25-group proof identities required'}
 $evaluation=ReadBound $row[0].evaluation
 $oldBuild=ReadBound $row[0].build
 CheckEntry $evaluation.build
 if($evaluation.source_commit -cne $baseCommit -or $oldBuild.source_commit -cne $baseCommit -or $oldBuild.configuration -cne $mode -or $evaluation.build.sha256 -cne $row[0].build.sha256 -or $evaluation.tests.Count -ne 25 -or -not $evaluation.affected_cold_pass -or $evaluation.device_operations -ne 0 -or $evaluation.live_ready){throw 'original main32 cold evaluation identity'}
 foreach($test in $evaluation.tests){CheckEntry $test.report;$null=VerifyReceiptPin $test.receipt}
 $oldProof=@{mode=$mode;evaluation=$row[0].evaluation;build=$row[0].build;source_commit=$baseCommit;groups=25;new_configuration_qualification=$false}
 if($mode -cne 'release'){
  $archiveProofPin=Binding (Join-Path $repo ('out/retained/consolidation-20261008/source-aa9de-debug-asan-compiler-archives/'+$mode+'.zip.verified.json'))
  $archiveProof=ReadBound $archiveProofPin
  if($archiveProof.schema -cne 'pas.recoverable-main32-compiler-archive.v1' -or $archiveProof.source_commit -cne $baseCommit -or $archiveProof.configuration -cne $mode -or -not $archiveProof.all_entry_SHAs_readback_verified -or -not $archiveProof.originals_rechecked -or $archiveProof.whole_candidate_qualified -or $archiveProof.live_ready -or $archiveProof.device_operations -ne 0 -or $archiveProof.build_binding.path -cne $row[0].build.path -or $archiveProof.build_binding.bytes -ne $row[0].build.bytes -or $archiveProof.build_binding.sha256 -cne $row[0].build.sha256 -or $archiveProof.original_source_snapshot -cne $inventory.source_snapshot.path){throw 'original lossless Debug/ASan archive proof required'}
  $archivePath=Join-Path $repo ('out/retained/consolidation-20261008/source-aa9de-debug-asan-compiler-archives/'+$mode+'.zip')
  if([IO.Path]::GetFullPath($archiveProof.archive.path) -cne [IO.Path]::GetFullPath($archivePath)){throw 'original compiler archive path identity'}
  AssertArchivePreserved $archiveProof.archive
  $oldProof.archive_proof=$archiveProofPin;$oldProof.archive=$archiveProof.archive
  $oldInputs+=@($archiveProofPin.path)
 }
 $oldProofs+=@($oldProof)
 $oldInputs+=@($row[0].evaluation.path,$row[0].build.path)+@($evaluation.tests.report.path)+@($evaluation.tests.receipt.path)
 if($mode -ceq 'release'){$baseBuild=$oldBuild;$baseEvaluation=$evaluation;$baseBuildPin=$row[0].build;$baseEvaluationPin=$row[0].evaluation}
}
$baseRoot=[IO.Path]::GetFullPath($baseBuild.build_root)
if($baseRoot -cne [IO.Path]::GetFullPath((Join-Path $repo 'out/prelive-pre-emulator-release-main32')) -or $baseBuild.schema -cne 'pas.cooperative-build.v1'){throw 'preserved main32 Release root required'}
foreach($entry in $baseBuild.files){CheckEntry $entry}
$null=VerifyReceiptPin $baseBuild.configure_receipt
foreach($entry in $baseBuild.build_receipts){$null=VerifyReceiptPin $entry}
$baseManifest=ReadBound $baseBuild.source_manifest
$baseCore=ReadBound $baseBuild.core_build_plan
$dependencies=ReadBound $baseBuild.dependency_manifest
if($baseManifest.source_commit -cne $baseCommit -or $baseManifest.files.Count -ne 909 -or $snapshot.formal_manifest.sha256 -cne $baseBuild.source_manifest.sha256 -or $baseCore.source_commit -cne $baseCommit -or $baseCore.translation_units -ne 22 -or $dependencies.source_commit -cne $baseCommit -or $dependencies.files.Count -ne 2154 -or -not $dependencies.release_and_debug -or -not $dependencies.protoc_and_plugin){throw 'base formal/source/dependency identities required'}
# Verify the complete original installed closure once before building. Only
# files actually used by these four targets enter subsequent native closures.
foreach($entry in $dependencies.files){CheckEntry $entry}
$dependencyPreflightVerified=$true
$dependencyRoot=[IO.Path]::GetFullPath($dependencies.root)
$selectedPaths=@(Get-ChildItem -LiteralPath (Join-Path $dependencyRoot 'include/nlohmann') -File -Recurse|ForEach-Object FullName)
foreach($relative in @('share/nlohmann_json','share/zlib')){$selectedPaths+=@(Get-ChildItem -LiteralPath (Join-Path $dependencyRoot $relative) -File -Recurse|Where-Object {$_.Extension -ceq '.cmake' -or $_.Extension -ceq '.natvis'}|ForEach-Object FullName)}
$selectedPaths+=@('include/zlib.h','include/zconf.h','lib/z.lib','debug/lib/zd.lib','bin/z.dll'|ForEach-Object {Join-Path $dependencyRoot $_})
$selectedDependencies=@()
foreach($path in $selectedPaths|Sort-Object -Unique){
 $rows=@($dependencies.files|Where-Object {[IO.Path]::GetFullPath($_.path) -ceq [IO.Path]::GetFullPath($path)})
 if($rows.Count -ne 1){throw 'one selected json/zlib original dependency pin required'}
 CheckEntry $rows[0];$selectedDependencies+=@($rows[0])
}
$snapshotByOriginal=@{}
foreach($row in $snapshot.files){$snapshotByOriginal[[IO.Path]::GetFullPath($row.original.path)]=$row}
foreach($entry in $baseManifest.files){$row=$snapshotByOriginal[[IO.Path]::GetFullPath($entry.path)];if(-not $row -or $row.original.sha256 -cne $entry.sha256 -or $row.original.bytes -ne $entry.bytes){throw 'base source manifest must join lossless snapshot'}}

$staticInputs=@('cold/current_rails.lib','cold/current_front.lib','cold/own_bridge.lib','cold/exact_bvi_v3.lib','production/pas_core.lib'|ForEach-Object {BaseFile $_})
$baseExecutables=@('cold/cost_analysis.exe','cold/prelive_audit.exe','cost_comparison.exe','batch_check.exe'|ForEach-Object {BaseFile $_})
$baseRuntimeDlls=@(BaseFile 'cold/z.dll';BaseFile 'z.dll')
$environmentBinding=Join-Path $repo 'out/prelive-pre-emulator-evaluation-main32-release/compiler-environment-binding.json'
$environmentPin=Binding $environmentBinding
$environmentProof=ReadBound $environmentPin
if($environmentProof.schema -cne 'pas.msvc-environment-binding.v1' -or $environmentProof.source_commit -cne $baseCommit -or $environmentProof.device_operations -ne 0 -or $environmentProof.live_ready){throw 'original compiler environment provenance required'}
foreach($entry in @($environmentProof.environment,$environmentProof.verification,$environmentProof.state)){CheckEntry $entry}
$null=VerifyReceiptPin $environmentProof.verification
$environment=ReadBound $environmentProof.environment
if($environment.schema -cne 'pas.msvc-environment.v1' -or $environment.variables.VCToolsVersion -cne '14.51.36231' -or $environment.variables.VisualStudioVersion -cne '18.0'){throw 'same pinned MSVC 14.51 environment required'}

$authorization=Join-Path $repo 'docs/research/live-readiness-20261007/NOISE_REPAIR_AUTHORIZATION_20261011.json'
$scopePin=Binding $authorization
$authorizationValue=ReadBound $scopePin
if($authorizationValue.schema -cne 'pas.user-directed-narrow-noise-repair-scope.v1' -or $authorizationValue.status -cne 'HUMAN_AUTHORIZED' -or $authorizationValue.scope -cne $scope -or $authorizationValue.full_75_rerun_required -or $authorizationValue.production_pass_rerun_required -or @($authorizationValue.allowed_operations).Count -ne 2 -or @($authorizationValue.allowed_operations|Where-Object {$_ -notin @('aa','abba')}).Count -or $authorizationValue.create_new_thread -or $authorizationValue.emulator_authorized_in_this_cold_stage -or $authorizationValue.device_operations -ne 0 -or $authorizationValue.live_ready){throw 'user directed narrow scope required'}
$scopeDocument=Join-Path $repo 'docs/research/live-readiness-20261007/NOISE_REPAIR_SCOPE_20261011.md'
$approval=Join-Path $repo 'docs/research/live-readiness-20261007/CAPACITY_APPROVAL_20261008.json'
$approvalValue=Get-Content -LiteralPath $approval -Raw|ConvertFrom-Json
if($approvalValue.status -cne 'HUMAN_APPROVED' -or $approvalValue.retained_prelive_cap_bytes -ne 51539607552){throw 'original 48 GiB capacity authorization required'}

$changedPaths=@(& git -c core.quotePath=false -C $repo diff --name-only $baseCommit $commit)
if($LASTEXITCODE -or -not $changedPaths.Count){throw 'explicit narrow source delta required'}
foreach($path in $changedPaths){
 $allowed=$path -in @('AGENTS.md','README.md','research/pre_emulator/PLAN.md','research/prelive_current/chain.cpp','research/prelive_current/consumer_priority.hpp','research/prelive_current/tests/consumer_priority_tests.cpp','research/prelive_current/worker_affinity.hpp','research/prelive_current/tests/worker_affinity_tests.cpp','research/prelive_current/producer_wait.hpp','research/prelive_current/tests/producer_wait_tests.cpp','research/prelive_current/diagnostic_processor.hpp','research/prelive_current/diagnostic_phase_profile.hpp','research/prelive_current/diagnostic_phase_summary.cpp','research/prelive_current/owner_candidate_step.hpp','research/prelive_current/runtime.hpp','research/prelive_current/CMakeLists.txt') -or $path -cmatch '^research/pre_emulator/qualification_reader/(?:main[.]cpp|controls[.]cpp|reader/narrow_preflight[.]hpp)$' -or $path -match '^research/pre_emulator/(?:[^/]+[.]ps1|(?:[^/]+/)*CMakeLists[.]txt)$' -or ($path -match '^docs/.*[.](md|json)$' -and $path -cne 'docs/research/zero-miss-20261006/windows/PRELIVE_COST_PROTOCOL_R2_20261007.md')
 if(-not ($allowed -or (ComponentDeltaAllowed $path))){throw ('delta outside cold harness/maintenance scope: '+$path)}
}
$trackedSourcePaths=@(& git -c core.quotePath=false -C $repo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -match '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'})
if($LASTEXITCODE){throw 'tracked source inventory'}
$sourceEntries=@($trackedSourcePaths|ForEach-Object {Binding (Join-Path $repo $_)})
$currentSourceByPath=@{};foreach($entry in $sourceEntries){$currentSourceByPath[[IO.Path]::GetFullPath($entry.path)]=$entry}
foreach($entry in $baseManifest.files){
 $current=$currentSourceByPath[[IO.Path]::GetFullPath($entry.path)]
 if(-not $current){throw 'base code removal is outside narrow repair scope'}
 if($current.sha256 -cne $entry.sha256 -or $current.bytes -ne $entry.bytes){$relative=[IO.Path]::GetRelativePath($repo,$entry.path).Replace('\','/');if($relative -cnotin $changedPaths){throw 'uncommitted source byte difference from base'}}
}

$artifact=Join-Path $repo ('out/prelive-noise-repair-evaluation-'+$Attempt)
$root=Join-Path $repo ('out/prelive-noise-repair-'+$Attempt)
Fresh $artifact;Fresh $root
[IO.Directory]::CreateDirectory($artifact)|Out-Null
$sourceManifest=Join-Path $artifact 'source-manifest.json'
NewJson $sourceManifest @{schema='pas.cooperative-source.v1';source_commit=$commit;dirty=$false;files=$sourceEntries;base_source_commit=$baseCommit;device_operations=0;live_ready=$false}
$dependencySelection=Join-Path $artifact 'selected-dependency-manifest.json'
NewJson $dependencySelection @{schema='pas.narrow-noise-repair-dependency-selection.v1';source_commit=$commit;original_dependency_manifest=$baseBuild.dependency_manifest;original_dependency_source_commit=$baseCommit;complete_original_preflight_verified=$dependencyPreflightVerified;complete_original_preflight_files=$dependencies.files.Count;files=$selectedDependencies;selection='Actual nlohmann-json and zlib headers/configuration/import/runtime files only';device_operations=0;live_ready=$false}
$targets=ComponentDeltaTargets
$buildTargetGroups=@(@{targets=@('pas_core')},@{targets=@('own_bridge','current_chain','consumer_priority_controls','worker_affinity_controls','producer_wait_tests')},@{targets=@('owner_checkpoint_tests','owner_expiry_tests','component_mask_tests','uniform_fnv_tests')})
$translationUnitSources=ComponentDeltaUnits
$formalChanged=ComponentDeltaFormalPaths $changedPaths
$workerPlacementPolicy='single-group highest raw EfficiencyClass; last two distinct physical cores by ascending lowest LP per core; consumer lower, producer higher; exact restore'
$corePlan=Join-Path $artifact 'narrow-core-build-plan.json'
NewJson $corePlan @{schema='pas.narrow-noise-repair-core-build-plan.v1';source_commit=$commit;base_source_commit=$baseCommit;configuration='release';component_cpu_delta=$true;formal_rebuilt=$true;formal_changed_paths=$formalChanged;rebuilt_static_targets=@('own_bridge','pas_core');delta_targets=$targets;build_target_groups=$buildTargetGroups;translation_units=32;translation_unit_sources=$translationUnitSources;base_static_inputs=$staticInputs;worker_placement_policy=$workerPlacementPolicy;producer_priority_policy='synthetic producer thread only; HIGHEST(2); consumer HIGHEST(2); NORMAL process class(32); scoped query/set/readback/restore; same A/B policy';consumer_priority_requested=2;producer_active_tail_ns=2000000;producer_wait_controls_count=43;compiler_snapshot=(Binding $tool);compiler_environment=$environmentPin;original_configuration_proofs=$oldProofs;prerequisite_groups_each=25;whole_candidate_qualification=$false;device_operations=0;live_ready=$false}
$adapter=Join-Path $repo 'research/main_consolidation/cmake-stage.ps1'
$toolchain='C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake'
$guard=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$inputs=@((Join-Path $PSScriptRoot 'component_delta.ps1'),$PSCommandPath,(Join-Path $PSScriptRoot 'common.ps1'),$recipe,$adapter,(Join-Path $repo 'research/main_consolidation/stage.ps1'),$sourceManifest,$corePlan,$dependencySelection,$authorization,$scopeDocument,$approval,$toolchain,$environmentBinding,$environmentProof.environment.path,$environmentProof.verification.path,$environmentProof.state.path,$baseBuild.source_manifest.path,$baseBuild.dependency_manifest.path,$baseBuild.core_build_plan.path,$baseBuild.configure_receipt.path)+@($sourceEntries.path)+@($selectedDependencies.path)+@($staticInputs.path)+@($baseExecutables.path)+@($baseRuntimeDlls.path)+@($baseBuild.build_receipts.path)+$oldInputs+(GateInputs)+@($guard.guard_source.path,$guard.process_source.path,$guard.cached_identity_controls.path,$guard.identity_wait_controls.path)
$options=@(('-DCMAKE_TOOLCHAIN_FILE='+$toolchain),('-DVCPKG_INSTALLED_DIR='+(Join-Path $repo 'out/vcpkg_installed')),'-DVCPKG_TARGET_TRIPLET=x64-windows','-DVCPKG_MANIFEST_MODE=OFF','-DCMAKE_CXX_SCAN_FOR_MODULES=OFF','-DCMAKE_HAVE_LIB_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=','-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL',('-DPAS_WORKSPACE='+$repo),('-DPAS_ORIGINAL_BUILD='+$baseRoot))
$configure='configure-prelive-noise-repair-'+$Attempt
AssertCurrentSource;AssertBaseUnchanged;$null=CheckCandidateCapacity
Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $PSScriptRoot 'noise_repair');BuildRoot=$root;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentBinding;Operation='Configure';Configuration='Release';Inputs=$inputs;Options=$options}
$configureReceipt=Binding (Join-Path $out ($configure+'/'+$configure+'-verification.json'))
$configureClosure=ClosurePin $configure
$configureProcess=BuildProcessEvidence $configure

# Exact fresh32 graph; original three libraries are effective immutable inputs.
$objects=ComponentDeltaAssertGraph $root $repo $translationUnitSources
$buildReceipts=@();$buildClosures=@();$buildProcesses=@();$priorOutputs=@()
for($groupIndex=0;$groupIndex -lt $buildTargetGroups.Count;$groupIndex++){
 $stage='build-prelive-noise-repair-'+$Attempt+'-cpu-group'+($groupIndex+1).ToString('00')
 AssertCurrentSource;AssertBaseUnchanged;$null=CheckCandidateCapacity
 foreach($pin in $priorOutputs){CheckEntry $pin}
 $priorProofs=@($configureProcess.Values|ForEach-Object {$_.path})+@($buildProcesses|ForEach-Object {$_.Values|ForEach-Object {$_.path}})
 $priorOutputPaths=@($priorOutputs|ForEach-Object {$_.path})
 Stage $stage $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $PSScriptRoot 'noise_repair');BuildRoot=$root;StageName=$stage;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentBinding;Operation='Build';Configuration='Release';Inputs=@($inputs+(Join-Path $root 'CMakeCache.txt')+(Join-Path $root 'build.ninja')+$priorOutputPaths+$priorProofs);Options=@('--target')+@($buildTargetGroups[$groupIndex].targets)}
 $buildReceipts+=@(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))
 $buildClosures+=@(ClosurePin $stage);$buildProcesses+=@(BuildProcessEvidence $stage)
 foreach($pin in $priorOutputs){CheckEntry $pin}
 $priorOutputs=@(Get-ChildItem -LiteralPath $root -File -Recurse|Where-Object {$_.Extension -cin @('.obj','.lib','.exe','.dll')}|ForEach-Object {Binding $_.FullName})
}
$inputs+=@($configureProcess.Values.path)+@($buildProcesses|ForEach-Object {$_.Values.path})
AssertCurrentSource;AssertBaseUnchanged
$chain=Join-Path $root 'current_chain.exe';$controls=Join-Path $root 'consumer_priority_controls.exe';$affinityControls=Join-Path $root 'worker_affinity_controls.exe';$waitControls=Join-Path $root 'producer_wait_tests.exe';$zDll=Join-Path $root 'z.dll'
$newRuntime=@(Binding $chain;Binding $controls;Binding $affinityControls;Binding $waitControls;Binding $zDll)+@(ComponentDeltaExecutables|Where-Object {$_ -notin @('current_chain','consumer_priority_controls','worker_affinity_controls','producer_wait_tests')}|ForEach-Object {Binding (Join-Path $root ($_+'.exe'))})
$rebuiltStatic=@(Binding (Join-Path $root 'own_bridge.lib');Binding (Join-Path $root 'pas_core.lib'))
$linkedStatic=@($staticInputs|Where-Object {[IO.Path]::GetFileName($_.path) -cnotin @('own_bridge.lib','pas_core.lib')})+$rebuiltStatic
if($newRuntime[4].sha256 -cne $baseRuntimeDlls[0].sha256 -or $newRuntime[4].bytes -ne $baseRuntimeDlls[0].bytes){throw 'new runtime z.dll must match original base bytes'}
$runtimeFiles=@($newRuntime)+$baseExecutables+$baseRuntimeDlls
if(@($runtimeFiles|Where-Object {[IO.Path]::GetExtension($_.path) -ceq '.exe'}).Count -ne 12 -or @($runtimeFiles.path|Sort-Object -Unique).Count -ne $runtimeFiles.Count){throw 'unique twelve executable runtime seal required'}
$compiledOutputs=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object {Binding $_.FullName})
$compiledObjects=@($compiledOutputs|Where-Object {[IO.Path]::GetExtension($_.path) -ceq '.obj'})
$expectedObjects=@($objects|ForEach-Object {[IO.Path]::GetFullPath((Join-Path $root $_))})
$targetObjects=@($compiledObjects|Where-Object {[IO.Path]::GetFullPath($_.path) -cin $expectedObjects})
if($targetObjects.Count -ne 32){throw 'exact fresh32 CPU target objects'}
foreach($object in $compiledObjects|Where-Object {$_ -notin $targetObjects}){if($object.path.Replace('\','/') -cnotmatch '/(?:CompilerIdCXX/CMakeCXXCompilerId[.]obj|CMakeFiles/ShowIncludes/main[.]obj)$'){throw 'unexpected object outside CPU delta'}}
$seal=@{files=$runtimeFiles}
$controlsReport=Join-Path $artifact 'consumer-priority-controls.json'
$controlsStage='test-prelive-noise-repair-'+$Attempt+'-consumer-priority-controls'
$Operation='tests'
Native $controlsStage $controls @($controlsReport) @($inputs+$runtimeFiles.path+$linkedStatic.path+$compiledOutputs.path+$configureReceipt.path+$buildReceipts.path+$configureClosure.path+$buildClosures.path) 900
$report=Get-Content -LiteralPath $controlsReport -Raw|ConvertFrom-Json
if($report.schema -cne 'pas.consumer-priority-tests.v1' -or -not $report.pass -or $report.failed -ne 0 -or $report.assertions -ne 36 -or $report.checks.Count -ne 36 -or @($report.checks|Where-Object {-not $_.pass}).Count -or ($report.consumer_priority_requested -isnot [long] -and $report.consumer_priority_requested -isnot [int]) -or $report.consumer_priority_requested -ne 2 -or $report.device_operations -ne 0 -or $report.device_endpoints -ne 0 -or $report.cost_qualification -or $report.live_ready){throw 'all 36 consumer HIGHEST(2) priority controls required without qualification'}
$controlsReceipt=Binding (Join-Path $out ($controlsStage+'/'+$controlsStage+'-verification.json'))
$controlsClosure=ClosurePin $controlsStage
$affinityReport=Join-Path $artifact 'worker-affinity-controls.json'
$affinityStage='test-prelive-noise-repair-'+$Attempt+'-worker-affinity-controls'
AssertCurrentSource;AssertBaseUnchanged
Native $affinityStage $affinityControls @($affinityReport) @($inputs+$runtimeFiles.path+$linkedStatic.path+$compiledOutputs.path+$configureReceipt.path+$buildReceipts.path+$configureClosure.path+$buildClosures.path+$controlsReport+$controlsReceipt.path+$controlsClosure.path) 900
$affinityValue=Get-Content -LiteralPath $affinityReport -Raw|ConvertFrom-Json
if($affinityValue.schema -cne 'pas.worker-affinity-tests.v1' -or -not $affinityValue.pass -or $affinityValue.failed -ne 0 -or $affinityValue.assertions -ne 65 -or $affinityValue.checks.Count -ne 65 -or @($affinityValue.checks|Where-Object {-not $_.pass}).Count -or $affinityValue.device_operations -ne 0 -or $affinityValue.device_endpoints -ne 0 -or $affinityValue.cost_qualification -or $affinityValue.live_ready){throw 'all 65 worker affinity controls required without qualification'}
$affinityReceipt=Binding (Join-Path $out ($affinityStage+'/'+$affinityStage+'-verification.json'))
$affinityClosure=ClosurePin $affinityStage
$waitReport=Join-Path $artifact 'producer-wait-controls.json'
$waitStage='test-prelive-noise-repair-'+$Attempt+'-producer-wait-tests'
AssertCurrentSource;AssertBaseUnchanged
Native $waitStage $waitControls @($waitReport) @($inputs+$runtimeFiles.path+$linkedStatic.path+$compiledOutputs.path+$configureReceipt.path+$buildReceipts.path+$configureClosure.path+$buildClosures.path+$controlsReport+$controlsReceipt.path+$controlsClosure.path+$affinityReport+$affinityReceipt.path+$affinityClosure.path) 900
$waitValue=Get-Content -LiteralPath $waitReport -Raw|ConvertFrom-Json
if($waitValue.schema -cne 'pas.producer-deadline-wait-tests.v1' -or $waitValue.pass -ne $true -or $waitValue.failed -ne 0 -or $waitValue.assertions -ne 43 -or $waitValue.checks.Count -ne 43 -or @($waitValue.checks|Where-Object {$_.pass -ne $true}).Count -or ($waitValue.active_tail_ns -isnot [long] -and $waitValue.active_tail_ns -isnot [int]) -or $waitValue.active_tail_ns -ne 2000000 -or $waitValue.active_tail_poll_cap -ne 16777216 -or $waitValue.device_operations -ne 0 -or $waitValue.device_endpoints -ne 0 -or $waitValue.cost_qualification -ne $false -or $waitValue.live_ready -ne $false){throw 'all 43 bounded 2ms producer wait controls required without qualification'}
$waitReceipt=Binding (Join-Path $out ($waitStage+'/'+$waitStage+'-verification.json'))
$waitClosure=ClosurePin $waitStage
$componentControls=@()
foreach($item in ComponentDeltaControls){
 $componentReport=Join-Path $artifact ($item.target+'-controls.json')
 $componentStage='test-prelive-noise-repair-'+$Attempt+'-'+$item.target.Replace('_','-')
 $componentBinary=Join-Path $root ($item.target+'.exe')
 AssertCurrentSource;AssertBaseUnchanged
 Native $componentStage $componentBinary @($componentReport) @($inputs+$runtimeFiles.path+$linkedStatic.path+$compiledOutputs.path+$configureReceipt.path+$buildReceipts.path+$configureClosure.path+$buildClosures.path+$controlsReport+$controlsReceipt.path+$affinityReport+$affinityReceipt.path+$waitReport+$waitReceipt.path) 900
 $value=Get-Content -LiteralPath $componentReport -Raw|ConvertFrom-Json
 ComponentDeltaAssertControl $value $item
 $componentControls+=@(@{target=$item.target;schema=$item.schema;assertions=$item.assertions;binary=(Binding $componentBinary);report=(Binding $componentReport);receipt=(Binding (Join-Path $out ($componentStage+'/'+$componentStage+'-verification.json')));input_closure=(ClosurePin $componentStage)})
}
foreach($entry in $compiledOutputs){CheckEntry $entry}
AssertCurrentSource;AssertBaseUnchanged
$candidateBytes=CheckCandidateCapacity
$bindingPath=Join-Path $artifact 'build-binding.json'
NewJson $bindingPath @{schema='pas.narrow-noise-repair-build.v1';source_commit=$commit;configuration='release';scope=$scope;whole_candidate_qualification=$false;live_ready=$false;device_operations=0;base_source_commit=$baseCommit;base_release_build=$baseBuildPin;base_release_evaluation=$baseEvaluationPin;original_main32_inventory=$inventoryPin;source_manifest=(Binding $sourceManifest);dependency_manifest=$baseBuild.dependency_manifest;selected_dependency_manifest=(Binding $dependencySelection);core_build_plan=(Binding $corePlan);component_cpu_delta=$true;formal_rebuilt=$true;formal_changed_paths=$formalChanged;component_controls=$componentControls;rebuilt_static_inputs=$rebuiltStatic;linked_static_inputs=$linkedStatic;base_static_inputs=$staticInputs;build_root=$root;files=$runtimeFiles;compiled_outputs=$compiledOutputs;configure_receipt=$configureReceipt;configure_input_closure=$configureClosure;configure_process=$configureProcess;build_target_groups=$buildTargetGroups;build_receipts=$buildReceipts;build_input_closures=$buildClosures;build_processes=$buildProcesses;controls_receipt=$controlsReceipt;controls_report=(Binding $controlsReport);controls_input_closure=$controlsClosure;affinity_controls_receipt=$affinityReceipt;affinity_controls_report=(Binding $affinityReport);affinity_controls_input_closure=$affinityClosure;wait_controls_binary=$newRuntime[3];wait_controls_receipt=$waitReceipt;wait_controls_report=(Binding $waitReport);wait_controls_input_closure=$waitClosure;compiler_environment=$environmentPin;compiler_environment_reused=$true;tests_scope_authorization=$scopePin;old_configuration_proofs=$oldProofs;old_core_tests_prerequisite_only=$true;changed_paths=@($changedPaths|Sort-Object -Unique);additional_inputs=@(Binding $scopeDocument;Binding $approval);candidate_bytes=$candidateBytes;candidate_cap=6442450944}
Write-Output $bindingPath
