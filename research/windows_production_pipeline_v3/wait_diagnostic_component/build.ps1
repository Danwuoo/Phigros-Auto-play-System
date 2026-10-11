param([Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$FrozenHead,
      [Parameter(Mandatory)][string]$BaseProductionBinding,
      [Parameter(Mandatory)][string]$BaseCpuBinding,
      [Parameter(Mandatory)][string]$BaseSourceSnapshot,
      [ValidatePattern('^[a-z0-9-]{1,40}$')][string]$Attempt='main56-waitgap')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../../pre_emulator/common.ps1')
$script:maintenancePath=$PSCommandPath
$Operation='build'
$baseCommit='12ec0550e882e3b06751d67fa67c43dda3e9312b'
$diagnosticRoot='research/windows_production_pipeline_v3/wait_diagnostic_component/'
$deltaMain='research/windows_production_pipeline_v3/main.cpp'
$deltaNew=@('research/windows_production_pipeline_v3/poll_gap_diagnostic.hpp',
 'research/windows_production_pipeline_v3/tests/poll_gap_diagnostic_tests.cpp',
 ($diagnosticRoot+'CMakeLists.txt'),($diagnosticRoot+'build.ps1'))
function DRequire([bool]$value,[string]$reason){if(-not $value){throw ('wait diagnostic component: '+$reason)}}
function DRead($pin){CheckEntry $pin;DRequire ($pin.bytes -le 4194304) 'small metadata read';return Get-Content -LiteralPath $pin.path -Raw|ConvertFrom-Json}
function DSame($a,$b){return [string]::Equals([IO.Path]::GetFullPath($a.path),[IO.Path]::GetFullPath($b.path),[StringComparison]::OrdinalIgnoreCase) -and $a.bytes -eq $b.bytes -and $a.sha256 -ceq $b.sha256}
function DRelative([string]$path){$p=[IO.Path]::GetFullPath($path);DRequire ($p.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'source containment';return [IO.Path]::GetRelativePath($repo,$p).Replace('\','/')}
function DBool($value,[string]$key,[bool]$expected){$v=$value.$key;DRequire ($v -is [bool] -and $v -eq $expected) ('typed bool '+$key)}
function DInt($value,[string]$key,[long]$expected){$v=$value.$key;DRequire (($v -is [int] -or $v -is [long]) -and $v -eq $expected) ('typed integer '+$key)}
function DProof([string]$stage){
 Receipt $stage
 $proof=@{verification=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')));input_closure=(Binding (Join-Path $pkg ($stage+'-input-closure-verification.json')))}
 $spec=Join-Path $pkg ($stage+'-direct-spec.json');if(-not(Test-Path -LiteralPath $spec -PathType Leaf)){$spec=Join-Path $out ($stage+'-inputs/spec.json')};$proof.specification=Binding $spec
 foreach($role in @('result','freeze','state','contract','command')){$name=if($role -in @('result','command')){$stage+'-'+$role+'.json'}else{$role+'.json'};$proof[$role]=Binding (Join-Path $out ($stage+'/'+$name))}
 foreach($pin in $proof.Values){CheckEntry $pin}
 $v=DRead $proof.verification;$r=DRead $proof.result;$s=DRead $proof.state;$c=DRead $proof.input_closure
 DBool $v 'pending' $false;DBool $v 'producer_complete' $true;foreach($field in @('native_exit','runner_exit','verification_exit')){DInt $v $field 0}
 DBool $r 'pending' $false;foreach($field in @('created','assigned','resumed','identity_trusted','held_all_signaled','streams_completed','natural_quiescence')){DBool $r.facts $field $true}
 DBool $r.facts 'errors_overflow' $false;foreach($field in @('active_final','exit_code','runner_exit')){DInt $r.facts $field 0}
 DRequire ($r.stage -ceq $stage -and @($r.facts.errors).Count -eq 0 -and $null -eq $r.facts.termination_returned -and $r.facts.guard_version -ceq 'pas.owned-held-identity.v3') 'natural Native completion'
 DRequire ($s.status -ceq 'COMPLETE' -and $null -eq $s.running -and $s.freeze_sha -ceq $proof.freeze.sha256 -and $s.contract_sha -ceq $proof.contract.sha256) 'Native state seal'
 DRequire ($c.schema -ceq 'pas.current-input-closure-verification.v1' -and $c.status -ceq 'COMPLETE' -and $c.stage -ceq $stage -and (DSame $c.verification $proof.verification) -and (DSame $c.state $proof.state)) 'Native closure provenance'
 foreach($field in @('pass','before_verified','after_verified')){DBool $c $field $true}
 $root=DRead $c.root;DRequire ($root.schema -ceq 'pas.current-input-closure.v1' -and $root.stage -ceq $stage -and $root.leaf_count -eq $c.leaf_count -and $root.files_count -eq $c.files_count -and [string]::Equals($root.verification_output,$proof.input_closure.path,[StringComparison]::OrdinalIgnoreCase)) 'Native closure root join'
 DBool (DRead $proof.contract) 'device_allowed' $false
 return $proof
}
function AssertSource {
 DRequire ((& git -C $repo rev-parse HEAD) -ceq $FrozenHead -and (& git -C $repo branch --show-current) -ceq 'main' -and -not (& git -C $repo status --porcelain)) 'exact clean diagnostic main source'
 foreach($pin in $currentSourcePins){CheckEntry $pin}
}
function AssertPinnedInputs {AssertSource;foreach($pin in $fixedInputs){CheckEntry $pin}}
function DFile([string]$relative){$expected=[IO.Path]::GetFullPath((Join-Path $base.build_root $relative));$rows=@($base.files|Where-Object {[IO.Path]::GetFullPath($_.path) -ceq $expected});DRequire ($rows.Count -eq 1) ('one base output '+$relative);CheckEntry $rows[0];return $rows[0]}

DRequire ($FrozenHead -cne $baseCommit) 'base and diagnostic delta commits are distinct'
$basePin=Binding ([IO.Path]::GetFullPath($BaseProductionBinding));$cpuPin=Binding ([IO.Path]::GetFullPath($BaseCpuBinding));$snapshotPin=Binding ([IO.Path]::GetFullPath($BaseSourceSnapshot))
$base=DRead $basePin;$cpu=DRead $cpuPin;$snapshot=DRead $snapshotPin
DRequire ($base.schema -ceq 'pas.cooperative-build.v1' -and $base.source_commit -ceq $baseCommit -and $base.configuration -ceq 'release' -and $cpu.schema -ceq 'pas.narrow-noise-repair-build.v1' -and $cpu.source_commit -ceq $baseCommit -and (DSame $base.noise_repair_build $cpuPin) -and (DSame $base.source_manifest $cpu.source_manifest) -and (DSame $base.dependency_manifest $cpu.dependency_manifest)) 'frozen main55 production/CPU/source/dependency joins'
foreach($doc in @($base,$cpu)){foreach($field in @('qualification_issued','live_ready','device_operations')){if($field -eq 'device_operations'){DInt $doc $field 0}elseif($doc.PSObject.Properties.Name -ccontains $field){DBool $doc $field $false}}}
$baseSource=DRead $base.source_manifest
DRequire ($baseSource.schema -ceq 'pas.cooperative-source.v1' -and $baseSource.source_commit -ceq $baseCommit -and $snapshot.schema -ceq 'pas.lossless-diagnostic-source-snapshot.v1' -and $snapshot.source_commit -ceq $baseCommit -and (DSame $snapshot.source_manifest $base.source_manifest)) 'immutable base source snapshot joins'
DBool $baseSource 'dirty' $false;DBool $snapshot 'all_copy_SHAs_verified' $true;DBool $snapshot 'originals_rechecked' $true
$copies=@{};foreach($row in $snapshot.files){$p=DRelative $row.original.path;DRequire (-not $copies.ContainsKey($p) -and $row.original.bytes -eq $row.copy.bytes -and $row.original.sha256 -ceq $row.copy.sha256) 'unique unchanged snapshot copy';$copies[$p]=$row}
$baseSources=@{};foreach($pin in $baseSource.files){$p=DRelative $pin.path;DRequire (-not $baseSources.ContainsKey($p) -and $copies.ContainsKey($p) -and (DSame $copies[$p].original $pin)) 'base source complete snapshot projection';CheckEntry $copies[$p].copy;$baseSources[$p]=$pin}
$tracked=@(& git -c core.quotePath=false -C $repo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -cmatch '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'})
DRequire ($LASTEXITCODE -eq 0 -and @($tracked|Sort-Object -Unique).Count -eq $tracked.Count) 'current full source denominator'
$currentSourcePins=@();$currentByPath=@{};$codeChanges=@();$newPaths=@()
foreach($p in $tracked){$pin=Binding (Join-Path $repo $p);$currentSourcePins+=@($pin);$currentByPath[$p]=$pin;if($baseSources.ContainsKey($p)){if($pin.bytes -ne $baseSources[$p].bytes -or $pin.sha256 -cne $baseSources[$p].sha256){DRequire ($p -ceq $deltaMain) ('unexpected base compiled-source change '+$p);$codeChanges+=@($p)}}else{DRequire ($p -cin $deltaNew) ('unexpected new compiled-source path '+$p);$newPaths+=@($p)}}
DRequire ($codeChanges.Count -eq 1 -and $codeChanges[0] -ceq $deltaMain -and @(Compare-Object ($newPaths|Sort-Object) ($deltaNew|Sort-Object)).Count -eq 0 -and @($baseSources.Keys|Where-Object {$_ -cnotin $tracked}).Count -eq 0) 'only production main delta and exact four appended source files'
AssertSource
# Formal src/include, root CMake, shared producer_wait and the old reader stay
# byte-identical to base55. The changed old main is checked against its copy,
# never by applying the old main SHA to the new current path.
$baseProofs=@(DProof (DRead $base.configure_receipt).stage)
DRequire (@($base.build_receipts).Count -eq 6 -and @($base.build_input_closures).Count -eq 6) 'original production six-group build receipts'
for($i=0;$i -lt 6;$i++){$proof=DProof (DRead $base.build_receipts[$i]).stage;DRequire ((DSame $proof.verification $base.build_receipts[$i]) -and (DSame $proof.input_closure $base.build_input_closures[$i])) 'original build Native8 joins';$baseProofs+=@($proof)}
$libraries=@(DFile 'production/pas_core.lib';DFile 'production/pas_current_candidate.lib';DFile 'production/pas_proto.lib')
$generated=@(DFile 'production/generated/session_build_provenance.hpp';DFile 'generated/emulator_controller.pb.h';DFile 'generated/emulator_controller.grpc.pb.h')
$manual=$currentByPath['src/manual_session.cpp'];DRequire ($manual.sha256 -ceq $baseSources['src/manual_session.cpp'].sha256) 'exact base manual source'
$provenance=Get-Content -LiteralPath $generated[0].path -Raw
DRequire ($provenance.Contains('pas_session_build_commit="'+$baseCommit+'"') -and $provenance.Contains('pas_session_build_dirty=false')) 'unchanged base55 generated manual/session provenance'
$runtimePlan=DRead $base.runtime_copy_plan;$runtimeProof=DRead $base.runtime_copy_verification
DRequire ($runtimePlan.schema -ceq 'pas.bound-release-runtime-copy-plan.v1' -and $runtimePlan.source_commit -ceq $baseCommit -and (DSame $runtimePlan.dependency_manifest $base.dependency_manifest) -and $runtimeProof.schema -ceq 'pas.bound-release-runtime-copy-verification.v1' -and $runtimeProof.source_commit -ceq $baseCommit -and (DSame $runtimeProof.plan $base.runtime_copy_plan)) 'frozen Release runtime deployment'
DBool $runtimePlan 'applocal_enabled' $false;DBool $runtimeProof 'pass' $true;DInt $runtimePlan 'source_file_count' 18;DInt $runtimePlan 'copy_file_count' 36;DInt $runtimeProof 'file_count' 36
$runtimeSources=@($runtimePlan.source_files);DRequire ($runtimeSources.Count -eq 18 -and @($runtimeSources.path|Sort-Object -Unique).Count -eq 18 -and @($runtimeProof.copies).Count -eq 36) 'complete runtime18/36 denominator'
$runtimeBase=@();foreach($pin in $runtimeSources){CheckEntry $pin;$name=[IO.Path]::GetFileName($pin.path);$copy=DFile $name;DRequire ($copy.bytes -eq $pin.bytes -and $copy.sha256 -ceq $pin.sha256) 'base runtime exact installed bytes';$runtimeBase+=@($copy)}
$dependencies=DRead $base.dependency_manifest;DRequire ($dependencies.source_commit -ceq 'aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de' -and @($dependencies.files).Count -eq 2154) 'original dependency provenance'
# Compiler closures bind the original installed dependencies, including actual
# Release linker inputs. These are build inputs, never cost/admission operands.
foreach($pin in $dependencies.files){CheckEntry $pin}
$environmentPin=$cpu.compiler_environment;$environmentProof=DRead $environmentPin
DRequire ($environmentProof.schema -ceq 'pas.msvc-environment-binding.v1' -and $environmentProof.source_commit -ceq 'aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de') 'original compiler environment'
foreach($pin in @($environmentProof.environment,$environmentProof.verification,$environmentProof.state)){CheckEntry $pin}
$authorization=DRead $cpu.tests_scope_authorization;DRequire ($authorization.status -ceq 'HUMAN_AUTHORIZED' -and $authorization.scope -ceq 'noise_and_unrun_abba_only') 'existing cold authorization';DInt $authorization 'device_operations' 0;DBool $authorization 'live_ready' $false
$fixedInputs=@($basePin,$cpuPin,$snapshotPin,$base.source_manifest,$base.dependency_manifest,$base.core_build_plan,$base.runtime_copy_plan,$base.runtime_copy_verification,$cpu.core_build_plan,$environmentPin,$environmentProof.environment,$environmentProof.verification,$environmentProof.state,$cpu.tests_scope_authorization)+$libraries+$generated+$runtimeSources+$runtimeBase+@($dependencies.files)+@($baseProofs|ForEach-Object {$_.Values})
foreach($pin in $fixedInputs){CheckEntry $pin}
$artifact=Join-Path $repo ('out/prelive-wait-diagnostic-evaluation-'+$Attempt);$buildRoot=Join-Path $repo ('out/prelive-wait-diagnostic-'+$Attempt)
DRequire (-not [string]::Equals($buildRoot,$base.build_root,[StringComparison]::OrdinalIgnoreCase)) 'independent build root';Fresh $artifact;Fresh $buildRoot;$null=[IO.Directory]::CreateDirectory($artifact)
$sourceManifest=Join-Path $artifact 'source-manifest.json'
NewJson $sourceManifest @{schema='pas.cooperative-source.v1';source_commit=$FrozenHead;dirty=$false;files=$currentSourcePins;diagnostic_only=$true;base_source_commit=$baseCommit;base_source_manifest=$base.source_manifest;changed_compiled_paths=$codeChanges;added_compiled_paths=$newPaths;formal_rebuilt=$false;qualification_issued=$false;cost_ready=$false;live_ready=$false;device_operations=0}
$runtimeCmake=Join-Path $artifact 'runtime-sources.cmake';$runtimeText='set(PAS_COMPONENT_RUNTIME_SOURCES'+[Environment]::NewLine
foreach($pin in $runtimeBase){$path=$pin.path.Replace('\','/');DRequire ($path -notmatch '[";$\r\n]') 'literal runtime path';$runtimeText+=' "'+$path+'"'+[Environment]::NewLine};$runtimeText+=')'+[Environment]::NewLine;[IO.File]::WriteAllText($runtimeCmake,$runtimeText,[Text.UTF8Encoding]::new($false))
$targets=@('production_wait_diagnostic_component','producer_poll_gap_controls')
$units=@(@{target=$targets[0];source=$currentByPath[$deltaMain]},@{target=$targets[0];source=$currentByPath['src/grpc_transport.cpp']},@{target=$targets[1];source=$currentByPath[$deltaNew[1]]})
$corePlan=Join-Path $artifact 'component-core-build-plan.json'
NewJson $corePlan @{schema='pas.production-wait-diagnostic-component-plan.v1';delta_source_commit=$FrozenHead;base_source_commit=$baseCommit;base_production_build=$basePin;base_cpu_build=$cpuPin;base_source_snapshot=$snapshotPin;source_manifest=(Binding $sourceManifest);configuration='release';targets=$targets;translation_units=$units;translation_unit_count=3;imported_static_libraries=$libraries;base_generated_inputs=$generated;base_manual_source=$manual;base_session_provenance_preserved=$true;runtime_sources=$runtimeBase;runtime18_count=18;formal_rebuilt=$false;old_objects_reused=$false;diagnostic_only=$true;controls_count=16;qualification_issued=$false;whole_candidate_qualification=$false;cost_qualification=$false;physical_qualification=$false;live_ready=$false;device_operations=0}
$adapter=Join-Path $repo 'research/main_consolidation/cmake-stage.ps1';$toolchain='C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake';$guard=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$inputs=@($PSCommandPath,(Join-Path $PSScriptRoot 'CMakeLists.txt'),(Join-Path $repo 'research/pre_emulator/common.ps1'),$recipe,$adapter,(Join-Path $repo 'research/main_consolidation/stage.ps1'),$sourceManifest,$corePlan,$runtimeCmake,$toolchain)+@($currentSourcePins.path)+@($fixedInputs.path)+(GateInputs)+@($guard.guard_source.path,$guard.process_source.path,$guard.cached_identity_controls.path,$guard.identity_wait_controls.path)
$options=@(('-DCMAKE_TOOLCHAIN_FILE='+$toolchain),('-DVCPKG_INSTALLED_DIR='+(Join-Path $repo 'out/vcpkg_installed')),'-DVCPKG_TARGET_TRIPLET=x64-windows','-DVCPKG_MANIFEST_MODE=OFF','-DVCPKG_APPLOCAL_DEPS=OFF','-DCMAKE_CXX_SCAN_FOR_MODULES=OFF','-DCMAKE_HAVE_LIB_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=','-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL',('-DPAS_WORKSPACE='+$repo),('-DPAS_BASE_BUILD_ROOT='+$base.build_root),('-DPAS_DELTA_SOURCE_COMMIT='+$FrozenHead),('-DPAS_DELTA_MAIN_SHA256='+$currentByPath[$deltaMain].sha256),('-DPAS_DELTA_HEADER_SHA256='+$currentByPath[$deltaNew[0]].sha256),('-DPAS_DELTA_TEST_SHA256='+$currentByPath[$deltaNew[1]].sha256),('-DPAS_BASE_MANUAL_SHA256='+$manual.sha256),('-DPAS_BASE_SESSION_PROVENANCE_SHA256='+$generated[0].sha256),('-DPAS_BASE_CORE_SHA256='+$libraries[0].sha256),('-DPAS_BASE_CURRENT_SHA256='+$libraries[1].sha256),('-DPAS_BASE_PROTO_SHA256='+$libraries[2].sha256),('-DPAS_RUNTIME_SOURCE_FILE='+$runtimeCmake))
$configure='configure-wait-diagnostic-'+$Attempt;AssertPinnedInputs
Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=$PSScriptRoot;BuildRoot=$buildRoot;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentPin.path;Operation='Configure';Configuration='Release';Inputs=$inputs;Options=$options}
$configureProof=DProof $configure
$ninja=(Get-Content -LiteralPath (Join-Path $buildRoot 'build.ninja') -Raw).Replace('\','/')
$rows=@([regex]::Matches($ninja,'(?m)^build ([^\r\n:]+[.]obj): CXX_COMPILER[^ \r\n]+[ \t]+([^|\r\n]+)'))
DRequire ($rows.Count -eq 3 -and $ninja -notmatch 'applocal[.]ps1' -and $ninja -match '/O2 /Ob2 /DNDEBUG' -and $ninja -match '(?:/|-)MD(?: |\r?\n)') 'exact3 diagnostic compile edges and Release flags'
for($i=0;$i -lt 3;$i++){$unit=$units[$i];$matches=@($rows|Where-Object {$_.Groups[1].Value.StartsWith('CMakeFiles/'+$unit.target+'.dir/') -and [string]::Equals([IO.Path]::GetFullPath($_.Groups[2].Value.Trim().Replace('$:',':').Replace('$ ',' ')),[IO.Path]::GetFullPath($unit.source.path),[StringComparison]::OrdinalIgnoreCase)});DRequire ($matches.Count -eq 1) 'each exact source belongs to its diagnostic target'}
foreach($pin in $libraries){DRequire ($ninja.Contains($pin.path.Replace('\','/')) -or $ninja.Contains($pin.path.Replace('\','/').Replace(':','$:'))) 'exact imported library is present in link graph'}
$generatedDelta=@(Binding (Join-Path $buildRoot 'wait_diagnostic_component_identity.hpp');Binding (Join-Path $buildRoot 'frozen_manual_session.cpp'))
$runtimeCopies=@();foreach($pin in $runtimeBase){$copy=Binding (Join-Path $buildRoot ([IO.Path]::GetFileName($pin.path)));DRequire ($copy.bytes -eq $pin.bytes -and $copy.sha256 -ceq $pin.sha256) 'exact runtime18 component copy';$runtimeCopies+=@($copy)}
$inputs+=@($generatedDelta.path)+@($runtimeCopies.path)+(Join-Path $buildRoot 'CMakeCache.txt')+(Join-Path $buildRoot 'build.ninja')+@($configureProof.Values.path)
$build='build-wait-diagnostic-'+$Attempt;AssertPinnedInputs
Stage $build $adapter @{ReceiptRepo=$repo;SourceRoot=$PSScriptRoot;BuildRoot=$buildRoot;StageName=$build;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentPin.path;Operation='Build';Configuration='Release';Inputs=$inputs;Options=@('--target')+$targets}
$buildProof=DProof $build;AssertPinnedInputs
$outputs=@(Get-ChildItem -LiteralPath $buildRoot -File -Recurse|ForEach-Object {Binding $_.FullName})
$objects=@($outputs|Where-Object {$_.path -match '[.]obj$' -and $_.path.Replace('\','/') -cmatch '/CMakeFiles/(production_wait_diagnostic_component|producer_poll_gap_controls)[.]dir/'})
DRequire ($objects.Count -eq 3) 'three actual component objects';foreach($pin in $outputs|Where-Object {$_.path -match '[.]obj$' -and $_ -notin $objects}){DRequire ($pin.path.Replace('\','/') -cmatch '/(?:CompilerIdCXX/CMakeCXXCompilerId[.]obj|CMakeFiles/ShowIncludes/main[.]obj)$') 'CMake probes are the only extra objects'}
$exes=@();foreach($target in $targets){$found=@($outputs|Where-Object {[IO.Path]::GetFileName($_.path) -ceq ($target+'.exe')});DRequire ($found.Count -eq 1) 'one fresh component executable';$exes+=@($found[0])}
$script:seal=@{files=@($exes)+$runtimeCopies};$controlReport=Join-Path $artifact 'poll-gap-controls.json';$controlStage='test-wait-diagnostic-'+$Attempt+'-poll-gap-controls'
Native $controlStage $exes[1].path @($controlReport) @($inputs+$outputs.path+$buildProof.Values.path) 300
$controlProof=DProof $controlStage;$report=DRead (Binding $controlReport)
DRequire ($report.schema -ceq 'pas.producer-poll-gap-controls.v1') 'poll-gap diagnostic controls schema';DInt $report 'assertions' 16;DInt $report 'failed' 0;DInt $report 'device_endpoints' 0;DInt $report 'device_operations' 0;DInt $report 'sample_cap' 16777217;DInt $report 'interval_cap' 16777216;DInt $report 'extra_clock_reads' 0
foreach($field in @('pass','synthetic_clock_controls','diagnostic_only')){DBool $report $field $true};foreach($field in @('cost_qualification','live_ready','instrumentation_overhead_measured')){DBool $report $field $false}
$ids=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);DRequire (@($report.checks).Count -eq 16) 'separate16 control denominator';foreach($check in $report.checks){DRequire ($check.id -is [string] -and $check.id.Length -gt 0 -and $ids.Add($check.id)) 'unique control id';DBool $check 'pass' $true}
AssertPinnedInputs;foreach($pin in $outputs){CheckEntry $pin}
$bindingPath=Join-Path $artifact 'build-binding.json'
NewJson $bindingPath @{schema='pas.production-wait-diagnostic-component-build.v1';delta_source_commit=$FrozenHead;base_source_commit=$baseCommit;base_production_build=$basePin;base_cpu_build=$cpuPin;base_source_snapshot=$snapshotPin;source_manifest=(Binding $sourceManifest);dependency_manifest=$base.dependency_manifest;compiler_environment=$environmentPin;core_build_plan=(Binding $corePlan);build_root=$buildRoot;translation_unit_count=3;targets=$targets;imported_static_libraries=$libraries;base_generated_inputs=$generated;generated_delta_inputs=$generatedDelta;base_manual_source=$manual;base_session_provenance_preserved=$true;runtime18_sources=$runtimeBase;runtime18_copies=$runtimeCopies;files=@($exes)+$runtimeCopies;compiled_outputs=$outputs;configure_process=$configureProof;build_process=$buildProof;controls_process=$controlProof;controls_report=(Binding $controlReport);controls_count=16;base_original_build_processes=$baseProofs;formal_rebuilt=$false;old_objects_reused=$false;diagnostic_only=$true;qualification_issued=$false;whole_candidate_qualification=$false;cost_qualification=$false;physical_qualification=$false;live_ready=$false;device_endpoints=0;device_operations=0;normal3_runs=0;AA12_runs=0;noise18_runs=0;ABBA36_runs=0;measurement_runs=0;cost_status='NOT_READY';base_STOP_preserved=$true;scope='Three-TU endpoint-free wait diagnostic only; frozen main55 libraries/provenance remain base identity; never accepted by qualification LoadBuilds/AA/admission.'}
@{binding=(Binding $bindingPath);translation_units=3;controls=16;measurement_runs=0;qualification_issued=$false;live_ready=$false;device_operations=0}|ConvertTo-Json -Depth 4
