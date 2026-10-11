param([Parameter(Mandatory)][string]$BaseBinding,
      [ValidatePattern('^main[0-9]{2}$')][string]$Attempt='main45')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $PSScriptRoot 'component_delta.ps1')
$script:maintenancePath=$PSCommandPath
$Operation='build'
$baseCommit='aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de'
$head=(& git -C $repo rev-parse HEAD)
if($LASTEXITCODE -or -not $head -or (& git -C $repo branch --show-current) -cne 'main' -or (& git -C $repo status --porcelain)){throw 'exact clean main reader source required'}

function ReaderRead($entry){CheckEntry $entry;return Get-Content -LiteralPath $entry.path -Raw|ConvertFrom-Json}
function ReaderRelative([string]$path){
 $absolute=[IO.Path]::GetFullPath($path)
 if(-not $absolute.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'reader source outside main'}
 return [IO.Path]::GetRelativePath($repo,$absolute).Replace('\','/')
}
function ReaderClosure([string]$stage){
 $pin=Binding (Join-Path $pkg ($stage+'-input-closure-verification.json'))
 $value=ReaderRead $pin
 if($value.schema -cne 'pas.current-input-closure-verification.v1' -or $value.status -cne 'COMPLETE' -or $value.pass -ne $true -or $value.before_verified -ne $true -or $value.after_verified -ne $true){throw 'reader native input closure incomplete'}
 return $pin
}
function ReaderReceipt($entry){$value=ReaderRead $entry;Receipt $value.stage;return $value}
function AssertReaderInputs {
 if((& git -C $repo rev-parse HEAD) -cne $head -or (& git -C $repo branch --show-current) -cne 'main' -or (& git -C $repo status --porcelain)){throw 'reader source changed during build'}
 foreach($entry in @($readerSourceEntries)+@($selectedDependencies)+@($candidateMetadata)+@($environmentPin,$environmentProof.environment,$environmentProof.verification,$environmentProof.state)){CheckEntry $entry}
}
function ReaderCandidateBytes {
 $roots=@(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -Directory|Where-Object { $_.Name -match '^prelive-pre-emulator-(?:(?:release|debug|asan)-main[0-9]{2}|evaluation-main[0-9]{2}-(?:release|debug|asan)|fixture-repo-main[0-9]{2}(?:-(?:release|debug|asan))?|planning)$' -or $_.Name -match '^prelive-(?:noise-repair|consumer-phase-probe|narrow-reader)-(?:evaluation-)?main[0-9]{2}$' })
 $bytes=[long](($roots|ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -File -Recurse}|Measure-Object Length -Sum).Sum)
 if($bytes -gt 6442450944){throw 'unchanged aggregate candidate cap'}
 return $bytes
}

$candidatePin=Binding ([IO.Path]::GetFullPath($BaseBinding))
$candidate=ReaderRead $candidatePin
if($candidate.schema -cne 'pas.narrow-noise-repair-build.v1' -or $candidate.source_commit -cne $head -or $candidate.configuration -cne 'release' -or $candidate.scope -cne 'noise_and_unrun_abba_only' -or $candidate.base_source_commit -cne $baseCommit -or $candidate.maintenance_only_amendment -or $candidate.whole_candidate_qualification -ne $false -or $candidate.live_ready -ne $false -or $candidate.device_operations -ne 0){throw 'same clean source narrow candidate required without qualification'}
$candidateSource=ReaderRead $candidate.source_manifest
$candidateCore=ReaderRead $candidate.core_build_plan
$cpuDelta=$candidate.component_cpu_delta -eq $true
$candidateTargets=if($cpuDelta){ComponentDeltaTargets}else{@('current_chain','consumer_priority_controls','worker_affinity_controls','producer_wait_tests')}
$candidateTUs=if($cpuDelta){32}else{4}
if($candidateSource.schema -cne 'pas.cooperative-source.v1' -or $candidateSource.source_commit -cne $head -or $candidateSource.dirty -ne $false -or $candidateCore.schema -cne 'pas.narrow-noise-repair-core-build-plan.v1' -or $candidateCore.source_commit -cne $head -or $candidateCore.formal_rebuilt -ne $cpuDelta -or $candidateCore.translation_units -ne $candidateTUs -or $candidateCore.producer_wait_controls_count -ne 43 -or @($candidateCore.delta_targets).Count -ne $candidateTargets.Count -or @($candidateCore.delta_targets|Sort-Object -Unique).Count -ne $candidateTargets.Count -or @(Compare-Object ($candidateTargets|Sort-Object) (@($candidateCore.delta_targets)|Sort-Object)).Count){throw 'same source explicit same-source narrow candidate metadata required'}
# The existing whole-source manifest is the source identity of both builds.
# Candidate binaries and its five imported libraries are not reader inputs.
$tracked=@(& git -c core.quotePath=false -C $repo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -cmatch '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'})
if($LASTEXITCODE -or $tracked.Count -ne $candidateSource.files.Count){throw 'same full tracked source denominator required'}
$sourceByPath=@{}
foreach($entry in $candidateSource.files){
 $relative=ReaderRelative $entry.path
 if($relative -cnotin $tracked -or $sourceByPath.ContainsKey([IO.Path]::GetFullPath($entry.path))){throw 'candidate source set must be exact tracked current code'}
 CheckEntry $entry;$sourceByPath[[IO.Path]::GetFullPath($entry.path)]=$entry
}
$targets=@('qualification_reader','qualification_reader_controls')
$translationUnits=@(@{target='qualification_reader';source=(Join-Path $repo 'research/pre_emulator/qualification_reader/main.cpp')},@{target='qualification_reader_controls';source=(Join-Path $repo 'research/pre_emulator/qualification_reader/controls.cpp')})
# Follow actual quoted includes, including the existing inline cost predicates.
# Textual inclusion does not add an independently compiled translation unit.
$pending=[Collections.Generic.Queue[string]]::new()
foreach($unit in $translationUnits){$pending.Enqueue([IO.Path]::GetFullPath($unit.source))}
$seen=@{};$readerSourceEntries=@()
while($pending.Count){
 $path=$pending.Dequeue()
 if($seen.ContainsKey($path)){continue};$seen[$path]=$true
 $null=ReaderRelative $path
 if(-not $sourceByPath.ContainsKey($path)){throw ('reader include absent from candidate source manifest: '+$path)}
 $entry=$sourceByPath[$path];CheckEntry $entry;$readerSourceEntries+=@($entry)
 $text=Get-Content -LiteralPath $path -Raw
 foreach($match in [regex]::Matches($text,'(?m)^\s*#\s*include\s*"([^"\r\n]+)"')){
  $include=$match.Groups[1].Value
  $local=[IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetDirectoryName($path)) $include))
  $workspace=[IO.Path]::GetFullPath((Join-Path $repo $include))
  if(Test-Path -LiteralPath $local -PathType Leaf){$pending.Enqueue($local)}elseif(Test-Path -LiteralPath $workspace -PathType Leaf){$pending.Enqueue($workspace)}else{throw ('unresolved reader include: '+$include)}
 }
}
$candidateMetadata=@($candidatePin,$candidate.source_manifest,$candidate.core_build_plan,$candidate.dependency_manifest,$candidate.configure_receipt,$candidate.configure_input_closure)+@($candidate.build_receipts)+@($candidate.build_input_closures)+@($candidate.controls_report,$candidate.controls_receipt,$candidate.controls_input_closure,$candidate.affinity_controls_report,$candidate.affinity_controls_receipt,$candidate.affinity_controls_input_closure,$candidate.wait_controls_report,$candidate.wait_controls_receipt,$candidate.wait_controls_input_closure)
if($cpuDelta){foreach($row in $candidate.component_controls){$candidateMetadata+=@($row.report,$row.receipt,$row.input_closure)}}
foreach($entry in $candidateMetadata){CheckEntry $entry}
$null=ReaderReceipt $candidate.configure_receipt
foreach($entry in $candidate.build_receipts){$null=ReaderReceipt $entry}
if($cpuDelta){foreach($row in $candidate.component_controls){$null=ReaderReceipt $row.receipt}}
$null=ReaderReceipt $candidate.controls_receipt;$null=ReaderReceipt $candidate.affinity_controls_receipt;$null=ReaderReceipt $candidate.wait_controls_receipt

$originalDependencies=ReaderRead $candidate.dependency_manifest
if($originalDependencies.source_commit -cne $baseCommit -or $originalDependencies.files.Count -ne 2154){throw 'original dependency manifest provenance required'}
$dependencyRoot=[IO.Path]::GetFullPath($originalDependencies.root)
$selectedPaths=@(Get-ChildItem -LiteralPath (Join-Path $dependencyRoot 'include/nlohmann') -File -Recurse|ForEach-Object FullName)
$selectedPaths+=@(Get-ChildItem -LiteralPath (Join-Path $dependencyRoot 'share/nlohmann_json') -File -Recurse|Where-Object {$_.Extension -ceq '.cmake' -or $_.Extension -ceq '.natvis'}|ForEach-Object FullName)
$selectedDependencies=@()
foreach($path in $selectedPaths|Sort-Object -Unique){
 $rows=@($originalDependencies.files|Where-Object {[IO.Path]::GetFullPath($_.path) -ceq [IO.Path]::GetFullPath($path)})
 if($rows.Count -ne 1){throw 'one selected nlohmann original dependency pin required'}
 CheckEntry $rows[0];$selectedDependencies+=@($rows[0])
}
# No zlib, grpc/protobuf, archived ZIP or candidate static library is rehashed.
$environmentPin=$candidate.compiler_environment
$environmentProof=ReaderRead $environmentPin
if($environmentProof.schema -cne 'pas.msvc-environment-binding.v1' -or $environmentProof.source_commit -cne $baseCommit -or $environmentProof.device_operations -ne 0 -or $environmentProof.live_ready -ne $false){throw 'original compiler environment provenance required'}
foreach($entry in @($environmentProof.environment,$environmentProof.verification,$environmentProof.state)){CheckEntry $entry}
$null=ReaderReceipt $environmentProof.verification
$environment=ReaderRead $environmentProof.environment
if($environment.schema -cne 'pas.msvc-environment.v1' -or $environment.variables.VCToolsVersion -cne '14.51.36231' -or $environment.variables.VisualStudioVersion -cne '18.0'){throw 'same pinned Release MSVC environment required'}
$approval=Join-Path $repo 'docs/research/live-readiness-20261007/CAPACITY_APPROVAL_20261008.json'
$approvalValue=Get-Content -LiteralPath $approval -Raw|ConvertFrom-Json
if($approvalValue.status -cne 'HUMAN_APPROVED' -or $approvalValue.retained_prelive_cap_bytes -ne 51539607552){throw 'unchanged 48 GiB capacity authorization'}
$authorizationPin=$candidate.tests_scope_authorization
$authorization=ReaderRead $authorizationPin
if($authorization.status -cne 'HUMAN_AUTHORIZED' -or $authorization.scope -cne 'noise_and_unrun_abba_only' -or $authorization.device_operations -ne 0 -or $authorization.live_ready -ne $false -or $authorization.create_new_thread){throw 'unchanged endpoint-free cold scope required'}

$artifact=Join-Path $repo ('out/prelive-narrow-reader-evaluation-'+$Attempt)
$root=Join-Path $repo ('out/prelive-narrow-reader-'+$Attempt)
Fresh $artifact;Fresh $root
[IO.Directory]::CreateDirectory($artifact)|Out-Null
$dependencySelection=Join-Path $artifact 'selected-dependency-manifest.json'
NewJson $dependencySelection @{schema='pas.narrow-preflight-reader-dependencies.v1';source_commit=$head;original_dependency_manifest=$candidate.dependency_manifest;original_dependency_source_commit=$baseCommit;files=$selectedDependencies;selection='Actual nlohmann-json headers/configuration files only; bcrypt is a system import';read_only=$true;diagnostic_only=$true;qualified=$false;qualification_issued=$false;device_endpoints=0;device_operations=0;live_ready=$false}
$corePlan=Join-Path $artifact 'narrow-reader-core-build-plan.json'
NewJson $corePlan @{schema='pas.narrow-preflight-reader-core-build-plan.v1';source_commit=$head;configuration='release';read_only=$true;diagnostic_only=$true;formal_rebuilt=$false;targets=$targets;translation_units=$translationUnits;source_inputs=$readerSourceEntries;candidate_build=$candidatePin;candidate_source_manifest=$candidate.source_manifest;candidate_core_build_plan=$candidate.core_build_plan;linked_dependencies=@('nlohmann_json::nlohmann_json','bcrypt');imported_static_libraries=@();compiler_environment=$environmentPin;compiler_snapshot=(Binding $tool);qualified=$false;qualification_issued=$false;cost_qualification=$false;whole_candidate_qualification=$false;device_endpoints=0;device_operations=0;live_ready=$false}
$adapter=Join-Path $repo 'research/main_consolidation/cmake-stage.ps1'
$toolchain='C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake'
$guard=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$inputs=@($PSCommandPath,(Join-Path $PSScriptRoot 'common.ps1'),$recipe,$adapter,(Join-Path $repo 'research/main_consolidation/stage.ps1'),(Join-Path $PSScriptRoot 'narrow_reader/CMakeLists.txt'),$dependencySelection,$corePlan,$toolchain,$environmentPin.path,$environmentProof.environment.path,$environmentProof.verification.path,$environmentProof.state.path,$authorizationPin.path,$approval)+@($readerSourceEntries.path)+@($selectedDependencies.path)+@($candidateMetadata.path)+(GateInputs)+@($guard.guard_source.path,$guard.process_source.path,$guard.cached_identity_controls.path,$guard.identity_wait_controls.path)
$options=@(('-DCMAKE_TOOLCHAIN_FILE='+$toolchain),('-DVCPKG_INSTALLED_DIR='+(Join-Path $repo 'out/vcpkg_installed')),'-DVCPKG_TARGET_TRIPLET=x64-windows','-DVCPKG_MANIFEST_MODE=OFF','-DCMAKE_CXX_SCAN_FOR_MODULES=OFF','-DCMAKE_HAVE_LIB_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=','-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL',('-DPAS_WORKSPACE='+$repo))
$configure='configure-prelive-narrow-reader-'+$Attempt
AssertReaderInputs;$null=ReaderCandidateBytes
Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $PSScriptRoot 'narrow_reader');BuildRoot=$root;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentPin.path;Operation='Configure';Configuration='Release';Inputs=$inputs;Options=$options}
$configureReceipt=Binding (Join-Path $out ($configure+'/'+$configure+'-verification.json'))
$configureClosure=ReaderClosure $configure
$ninja=(Get-Content -LiteralPath (Join-Path $root 'build.ninja') -Raw).Replace('\','/')
$compileRows=@([regex]::Matches($ninja,'(?m)^build ([^\r\n:]+[.]obj): CXX_COMPILER[^\r\n]*')|ForEach-Object {$_.Groups[1].Value})
if($compileRows.Count -ne 2 -or @($compileRows|Where-Object {$_ -cmatch '^CMakeFiles/qualification_reader[.]dir/.*main[.]cpp[.]obj$'}).Count -ne 1 -or @($compileRows|Where-Object {$_ -cmatch '^CMakeFiles/qualification_reader_controls[.]dir/.*controls[.]cpp[.]obj$'}).Count -ne 1 -or $ninja -notmatch '/O2 /Ob2 /DNDEBUG' -or $ninja -notmatch '(?:/|-)MD(?: |\r?\n)'){throw 'exact two fresh reader TUs and original Release flags required'}
$build='build-prelive-narrow-reader-'+$Attempt
AssertReaderInputs;$null=ReaderCandidateBytes
Stage $build $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $PSScriptRoot 'narrow_reader');BuildRoot=$root;StageName=$build;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentPin.path;Operation='Build';Configuration='Release';Inputs=@($inputs+(Join-Path $root 'CMakeCache.txt')+(Join-Path $root 'build.ninja'));Options=@('--target','qualification_reader','qualification_reader_controls')}
$buildReceipt=Binding (Join-Path $out ($build+'/'+$build+'-verification.json'))
$buildClosure=ReaderClosure $build
AssertReaderInputs
$outputs=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object {Binding $_.FullName})
$objects=@($outputs|Where-Object {[IO.Path]::GetExtension($_.path) -ceq '.obj'})
$targetObjects=@($objects|Where-Object {$_.path.Replace('\','/') -cmatch '/CMakeFiles/(?:qualification_reader|qualification_reader_controls)[.]dir/'})
if($targetObjects.Count -ne 2 -or @($targetObjects|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'main.cpp.obj'}).Count -ne 1 -or @($targetObjects|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'controls.cpp.obj'}).Count -ne 1){throw 'only declared reader target objects permitted'}
foreach($object in $objects|Where-Object {$_ -notin $targetObjects}){if($object.path.Replace('\','/') -cnotmatch '/(?:CompilerIdCXX/CMakeCXXCompilerId[.]obj|CMakeFiles/ShowIncludes/main[.]obj)$'){throw 'unexpected formal or unrelated object in reader root'}}
$readerPin=Binding (Join-Path $root 'qualification_reader.exe')
$controlsPin=Binding (Join-Path $root 'qualification_reader_controls.exe')
$files=@($readerPin,$controlsPin)
$seal=@{files=$files}
$controlsReport=Join-Path $artifact 'qualification-reader-controls.json'
$controlsStage='test-prelive-narrow-reader-'+$Attempt+'-controls'
$Operation='tests'
Native $controlsStage $controlsPin.path @($controlsReport) @($inputs+$files.path+$outputs.path+$configureReceipt.path+$configureClosure.path+$buildReceipt.path+$buildClosure.path) 900
$report=Get-Content -LiteralPath $controlsReport -Raw|ConvertFrom-Json
if($report.schema -cne 'pas.qualification-reader-controls.v1' -or $report.failed -ne 0 -or $report.cases_count -ne 71 -or $report.cases.Count -ne 71 -or @($report.cases|Where-Object {$_.pass -ne $true}).Count -or $report.qualification_issued -ne $false -or $report.live_ready -ne $false -or $report.device_operations -ne 0){throw 'all 71 original and narrow preflight reader controls required without qualification'}
$controlsReceipt=Binding (Join-Path $out ($controlsStage+'/'+$controlsStage+'-verification.json'))
$controlsClosure=ReaderClosure $controlsStage
foreach($entry in $outputs){CheckEntry $entry}
AssertReaderInputs
$bindingPath=Join-Path $artifact 'build-binding.json'
NewJson $bindingPath @{schema='pas.narrow-preflight-reader-build.v1';source_commit=$head;configuration='release';read_only=$true;diagnostic_only=$true;qualified=$false;qualification_issued=$false;cost_qualification=$false;whole_candidate_qualification=$false;device_endpoints=0;device_operations=0;live_ready=$false;source_manifest=$candidate.source_manifest;dependency_manifest=(Binding $dependencySelection);original_dependency_manifest=$candidate.dependency_manifest;core_build_plan=(Binding $corePlan);candidate_build=$candidatePin;candidate_source_manifest=$candidate.source_manifest;candidate_core_build_plan=$candidate.core_build_plan;candidate_metadata=$candidateMetadata;build_root=$root;reader=$readerPin;controls=$controlsPin;files=$files;compiled_outputs=$outputs;declared_translation_units=$translationUnits;compiler_environment=$environmentPin;compiler_environment_reused=$true;configure_receipt=$configureReceipt;configure_input_closure=$configureClosure;build_receipts=@($buildReceipt);build_input_closures=@($buildClosure);controls_report=(Binding $controlsReport);controls_receipt=$controlsReceipt;controls_input_closure=$controlsClosure;controls_cases=$report.cases.Count;reader_checks_run=0;measurements_run=0;candidate_bytes=(ReaderCandidateBytes);candidate_cap=6442450944;tests_scope_authorization=$authorizationPin}
Write-Output $bindingPath
