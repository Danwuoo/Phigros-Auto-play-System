param([Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$FrozenHead,
      [ValidatePattern('^[a-z0-9-]{1,32}$')][string]$Attempt='main58-etw-tools',
      [string]$ComponentBinding='out/prelive-wait-diagnostic-evaluation-main57-waitgap/build-binding.json',
      [ValidatePattern('^[0-9a-f]{64}$')][string]$ComponentBindingSha256='e30ce09fb65661e8e09b85b99ecf1e42b1750161f272adbda61eeaf19873e4b3',
      [string]$Original55Negative='out/planning/main55-normal-r01-negative-original.json',
      [ValidatePattern('^[0-9a-f]{64}$')][string]$Original55NegativeSha256='7050fb0c0947deb6f6b455187f1d174dc1937d2c67adcc8b597e4afce23c1deb',
      [string]$OriginalAaR03Negative='out/planning/main57-base55-aa-r03-cost-negative-original.json',
      [ValidatePattern('^[0-9a-f]{64}$')][string]$OriginalAaR03NegativeSha256='fb3757132f270d4d8d98e9dd35cf59264d7974997d4c0fb6e52237b2e01e71e3')
$ErrorActionPreference='Stop'
$taskBuildScript=$PSCommandPath
. (Join-Path $PSScriptRoot '../pre_emulator/common.ps1')
. (Join-Path $PSScriptRoot 'capacity.ps1')
$script:maintenancePath=$taskBuildScript
$Operation='build'
$base57='5dce75d12c63ed51de5d996cedcd44e29d729841'
$base55='12ec0550e882e3b06751d67fa67c43dda3e9312b'
function DRequire([bool]$value,[string]$reason){if(-not $value){throw ('cold scheduler diagnostic build: '+$reason)}}
function DRead($pin){CheckEntry $pin;DRequire ($pin.bytes -le 4194304) 'small metadata';return Get-Content -LiteralPath $pin.path -Raw|ConvertFrom-Json}
function DBool($value,[string]$key,[bool]$expected){$v=$value.$key;DRequire ($v -is [bool] -and $v -eq $expected) ('typed boolean '+$key)}
function DInt($value,[string]$key,[long]$expected){$v=$value.$key;DRequire (($v -is [int] -or $v -is [long]) -and $v -eq $expected) ('typed integer '+$key)}
function DSame($a,$b){return [string]::Equals([IO.Path]::GetFullPath($a.path),[IO.Path]::GetFullPath($b.path),[StringComparison]::OrdinalIgnoreCase) -and $a.bytes -eq $b.bytes -and $a.sha256 -ceq $b.sha256}
function DPath([string]$path){$full=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($path)){$path}else{Join-Path $repo $path}));DRequire ($full.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'workspace-contained input';$part=$full;while($part){if(Test-Path -LiteralPath $part){DRequire (-not ((Get-Item -LiteralPath $part).Attributes -band [IO.FileAttributes]::ReparsePoint)) 'no reparse input'};$part=[IO.Path]::GetDirectoryName($part)};return $full}
function DExternal([string]$path,[string]$sha){$pin=Binding (DPath $path);DRequire ($pin.bytes -le 4194304 -and $pin.sha256 -ceq $sha) 'externally frozen small historical metadata';return $pin}
function AssertHead{DRequire ((& git -C $repo rev-parse HEAD) -ceq $FrozenHead -and (& git -C $repo branch --show-current) -ceq 'main' -and -not (& git -C $repo status --porcelain)) 'exact clean future diagnostic source'}
function AssertInputs{AssertHead;foreach($pin in @($sourcePins)+@($copies)+@($fixedPins)){CheckEntry $pin}}

# Import the existing diagnostic Native8 verifier verbatim. It checks the same
# original guard/state/closure controls and does not create a new gate family.
$proofRecipe=Join-Path $repo 'research/windows_production_pipeline_v3/wait_diagnostic_component/build.ps1'
$proofTokens=$null;$proofErrors=$null;$proofAst=[Management.Automation.Language.Parser]::ParseFile($proofRecipe,[ref]$proofTokens,[ref]$proofErrors)
DRequire ($proofErrors.Count -eq 0) 'existing proof recipe AST'
$proofFunctions=@($proofAst.FindAll({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq 'DProof')
DRequire ($proofFunctions.Count -eq 1) 'one existing Native8 proof helper'
. ([scriptblock]::Create($proofFunctions[0].Extent.Text))
AssertHead
$componentPin=DExternal $ComponentBinding $ComponentBindingSha256;$component=DRead $componentPin
DRequire ($component.schema -ceq 'pas.production-wait-diagnostic-component-build.v1' -and $component.delta_source_commit -ceq $base57 -and $component.base_source_commit -ceq $base55) 'original component57/base55 identities'
DBool $component 'diagnostic_only' $true;DBool $component 'qualification_issued' $false;DBool $component 'live_ready' $false;DInt $component 'device_operations' 0
$negative55Pin=DExternal $Original55Negative $Original55NegativeSha256;$negativeR03Pin=DExternal $OriginalAaR03Negative $OriginalAaR03NegativeSha256
$negative55=DRead $negative55Pin;$negativeR03=DRead $negativeR03Pin
DRequire ($negativeR03.compiled_source_commit -ceq $base55 -and $negativeR03.execution_source_commit -ceq $base57 -and $negativeR03.original_numerical_validity_pass -is [bool] -and -not $negativeR03.original_numerical_validity_pass -and $negativeR03.noise18 -ceq 'NOT_RUN' -and $negativeR03.ABBA36 -ceq 'NOT_RUN') 'original numerical negative remains negative'
$cpuPin=$component.base_cpu_build;$cpu=DRead $cpuPin
DRequire ($cpu.schema -ceq 'pas.narrow-noise-repair-build.v1' -and $cpu.source_commit -ceq $base55) 'frozen base compiler identity'
$environmentPin=$cpu.compiler_environment;$environment=DRead $environmentPin
DRequire ($environment.schema -ceq 'pas.msvc-environment-binding.v1' -and $environment.source_commit -ceq 'aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de') 'actual original main32 compiler environment'
$environmentInputs=@($environment.environment,$environment.verification,$environment.state);foreach($pin in $environmentInputs){CheckEntry $pin}
$installedInclude=DPath 'out/vcpkg_installed/x64-windows/include'
$headers=@(Get-ChildItem -LiteralPath (Join-Path $installedInclude 'nlohmann') -File -Recurse|ForEach-Object {Binding (DPath $_.FullName)})
DRequire ($headers.Count -gt 0) 'existing installed JSON header closure'
# Same full tracked source definition used by the preserved source manifests;
# the reserved Windows aux path remains the previously documented exclusion.
$tracked=@(& git -c core.quotePath=false -C $repo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -cmatch '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'})
DRequire ($LASTEXITCODE -eq 0 -and $tracked.Count -gt 0 -and @($tracked|Sort-Object -Unique).Count -eq $tracked.Count) 'complete dynamic tracked source denominator'
$sourcePins=@();$sourceByRelative=@{}
foreach($relative in $tracked){$pin=Binding (DPath $relative);$sourcePins+=@($pin);$sourceByRelative[$relative]=$pin}
$unitRelatives=@('tools/cold_scheduler_trace_controller.cpp','tests/cold_scheduler_trace_controller_tests.cpp','tools/cold_scheduler_trace_reader.cpp')
$targets=@('cold_scheduler_trace_controller','cold_scheduler_trace_controller_tests','cold_scheduler_trace_reader')
foreach($relative in @($unitRelatives)+@('research/cold_scheduler_diagnostic/build.ps1','research/cold_scheduler_diagnostic/CMakeLists.txt','research/cold_scheduler_diagnostic/run.ps1','research/cold_scheduler_diagnostic/elevated-launch.ps1')){DRequire ($sourceByRelative.ContainsKey($relative)) ('current frozen source member '+$relative)}
$artifact=DPath ('out/prelive-cold-scheduler-evaluation-'+$Attempt);$buildRoot=DPath ('out/prelive-cold-scheduler-tools-'+$Attempt);$shortRoot=DPath ('out/prelive-scheduler-src-'+$Attempt);$shortFiles=Join-Path $shortRoot 'files'
foreach($path in @($artifact,$buildRoot,$shortRoot)){Fresh $path}
$fixedPins=@($componentPin,$negative55Pin,$negativeR03Pin,$cpuPin,$component.base_source_snapshot,$component.source_manifest,$component.base_production_build,$component.controls_report,$environmentPin)+$environmentInputs+$headers+@(Binding $proofRecipe)
foreach($pin in $fixedPins){CheckEntry $pin}
$null=[IO.Directory]::CreateDirectory($artifact);$null=[IO.Directory]::CreateDirectory($shortFiles)
$copies=@();$copyByRelative=@{};$snapshotRows=@()
foreach($relative in $tracked){$original=$sourceByRelative[$relative];$copyPath=DPath (Join-Path $shortFiles $relative);DRequire ($copyPath.Length -le 259) 'short frozen source transport max259';$null=[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($copyPath));[IO.File]::Copy($original.path,$copyPath,$false);$copy=Binding $copyPath;DRequire ($original.bytes -eq $copy.bytes -and $original.sha256 -ceq $copy.sha256) 'lossless short source copy';$copies+=@($copy);$copyByRelative[$relative]=$copy;$snapshotRows+=@(@{relative_path=$relative;original=$original;copy=$copy})}
foreach($pin in @($sourcePins)+@($copies)){CheckEntry $pin}
AssertHead
$manifestPath=Join-Path $artifact 'source-manifest.json'
NewJson $manifestPath @{schema='pas.cooperative-source.v1';source_commit=$FrozenHead;dirty=$false;files=$sourcePins;full_current_tracked_source_count=$sourcePins.Count;source_filter='same original source manifest extensions; reserved aux path excluded';diagnostic_only=$true;formal_rebuilt=$false;qualification_issued=$false;cost_ready=$false;live_ready=$false;device_operations=0} 4194304
$snapshotPath=Join-Path $shortRoot 'snapshot.json'
NewJson $snapshotPath @{schema='pas.lossless-diagnostic-source-snapshot.v1';source_commit=$FrozenHead;source_manifest=(Binding $manifestPath);full_current_tracked_source_count=$sourcePins.Count;file_count=$copies.Count;files=$snapshotRows;all_copy_SHAs_verified=$true;originals_rechecked=$true;relative_paths_unchanged=$true;max_path_characters=(@($copies.path|ForEach-Object {$_.Length})|Measure-Object -Maximum).Maximum;legacy_max_path_characters=259;diagnostic_only=$true;qualification_issued=$false;device_operations=0;live_ready=$false} 4194304
$units=@();for($i=0;$i -lt 3;$i++){$units+=@(@{target=$targets[$i];original_source=$sourceByRelative[$unitRelatives[$i]];source=$copyByRelative[$unitRelatives[$i]]})}
$planPath=Join-Path $artifact 'core-build-plan.json'
NewJson $planPath @{schema='pas.cold-scheduler-diagnostic-build-plan.v1';source_commit=$FrozenHead;source_manifest=(Binding $manifestPath);source_snapshot=(Binding $snapshotPath);configuration='release';translation_units=$units;translation_unit_count=3;targets=$targets;dependency_include=$installedInclude;installed_header_inputs=$headers;linked_windows_libraries=@('advapi32','bcrypt','ole32','tdh');compiler_environment=$environmentPin;controller_controls_count=32;reader_controls_count=20;original_component57=$componentPin;original_component_source_commit=$base57;original_manual_library_source_commit=$base55;original_normal55_negative=$negative55Pin;original_AA_r03_negative=$negativeR03Pin;no_original_libraries_imported=$true;original_negative_payloads_rehashed=$false;recording_started=$false;diagnostic_only=$true;qualification_issued=$false;normal3_runs=0;AA12_runs=0;noise18_runs=0;ABBA36_runs=0;device_operations=0;live_ready=$false} 4194304
$fixedPins+=@(Binding $manifestPath;Binding $snapshotPath;Binding $planPath)
$adapter=Join-Path $repo 'research/main_consolidation/cmake-stage.ps1';$guard=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$inputs=@($taskBuildScript,$adapter,(Join-Path $repo 'research/main_consolidation/stage.ps1'),(Join-Path $repo 'research/pre_emulator/common.ps1'),$recipe)+@($sourcePins.path)+@($copies.path)+@($fixedPins.path)+(GateInputs)+@($guard.guard_source.path,$guard.process_source.path,$guard.cached_identity_controls.path,$guard.identity_wait_controls.path)
$options=@('-DVCPKG_APPLOCAL_DEPS=OFF','-DCMAKE_CXX_SCAN_FOR_MODULES=OFF','-DCMAKE_HAVE_LIB_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=','-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL',('-DPAS_SOURCE_WORKSPACE='+$shortFiles),('-DPAS_DEPENDENCY_INCLUDE='+$installedInclude),('-DPAS_TOOL_SOURCE_COMMIT='+$FrozenHead),('-DPAS_CONTROLLER_SOURCE_SHA256='+$units[0].source.sha256),('-DPAS_CONTROLLER_TEST_SOURCE_SHA256='+$units[1].source.sha256),('-DPAS_READER_SOURCE_SHA256='+$units[2].source.sha256))
$configure='configure-cold-scheduler-'+$Attempt;AssertInputs
Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $shortFiles 'research/cold_scheduler_diagnostic');BuildRoot=$buildRoot;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentPin.path;Operation='Configure';Configuration='Release';Inputs=$inputs;Options=$options}
$configureProof=DProof $configure
$ninjaPath=Join-Path $buildRoot 'build.ninja';$ninja=(Get-Content -LiteralPath $ninjaPath -Raw).Replace('\','/')
$compileRows=@([regex]::Matches($ninja,'(?m)^build ([^\r\n:]+[.]obj): CXX_COMPILER[^ \r\n]+[ \t]+([^|\r\n]+)'))
DRequire ($compileRows.Count -eq 3 -and $ninja -notmatch 'applocal[.]ps1' -and $ninja -match '/O2 /Ob2 /DNDEBUG' -and $ninja -match '(?:/|-)MD(?: |\r?\n)') 'actual exact3 Release compile edges without applocal'
foreach($unit in $units){$unitRows=@($compileRows|Where-Object {$_.Groups[1].Value.StartsWith('CMakeFiles/'+$unit.target+'.dir/') -and [string]::Equals([IO.Path]::GetFullPath($_.Groups[2].Value.Trim().Replace('$:',':').Replace('$ ',' ')),[IO.Path]::GetFullPath($unit.source.path),[StringComparison]::OrdinalIgnoreCase)});DRequire ($unitRows.Count -eq 1) 'each exact frozen source belongs to its one tool target'}
foreach($lib in @('advapi32','bcrypt','ole32','tdh')){DRequire ($ninja -match ('(?i)\b'+$lib+'[.]lib\b')) ('explicit Windows library '+$lib)}
DRequire ($ninja -notmatch '(?i)(pas_core|pas_current_candidate|pas_proto|own_bridge)[.]lib') 'no original candidate library import'
$graphPin=Binding $ninjaPath;$cachePin=Binding (Join-Path $buildRoot 'CMakeCache.txt');$inputs+=@($graphPin.path,$cachePin.path)+@($configureProof.Values.path)
$build='build-cold-scheduler-'+$Attempt;AssertInputs
Stage $build $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $shortFiles 'research/cold_scheduler_diagnostic');BuildRoot=$buildRoot;StageName=$build;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentPin.path;Operation='Build';Configuration='Release';Inputs=$inputs;Options=@('--target')+$targets}
$buildProof=DProof $build;AssertInputs
$outputs=@(Get-ChildItem -LiteralPath $buildRoot -File -Recurse|ForEach-Object {Binding $_.FullName})
$objects=@($outputs|Where-Object {$_.path -match '[.]obj$' -and $_.path.Replace('\','/') -cmatch '/CMakeFiles/cold_scheduler_trace_(controller|controller_tests|reader)[.]dir/'})
DRequire ($objects.Count -eq 3) 'three actual tool objects'
foreach($pin in $outputs|Where-Object {$_.path -match '[.]obj$' -and $_ -notin $objects}){DRequire ($pin.path.Replace('\','/') -cmatch '/(?:CompilerIdCXX/CMakeCXXCompilerId[.]obj|CMakeFiles/ShowIncludes/main[.]obj)$') 'only original CMake probe objects are additional'}
$exes=@();foreach($target in $targets){$found=@($outputs|Where-Object {[IO.Path]::GetFileName($_.path) -ceq ($target+'.exe')});DRequire ($found.Count -eq 1) 'one fresh tool executable';$exes+=@($found[0])}
$script:seal=@{files=$exes}
# Control CLI/schema/count constants are fixed to the companion source contract.
$controllerReportPath=Join-Path $artifact 'controller-controls.json';$readerReportPath=Join-Path $artifact 'reader-self-test.json'
$controllerControlStage='test-cold-scheduler-'+$Attempt+'-controller';AssertInputs
Native $controllerControlStage $exes[1].path @($controllerReportPath) @($inputs+$outputs.path+$buildProof.Values.path) 300
$controllerProof=DProof $controllerControlStage;$controllerReport=DRead (Binding $controllerReportPath)
$readerControlStage='test-cold-scheduler-'+$Attempt+'-reader';AssertInputs
Native $readerControlStage $exes[2].path @('--self-test','--output',$readerReportPath) @($inputs+$outputs.path+$buildProof.Values.path+$controllerProof.Values.path+$controllerReportPath) 300
$readerProof=DProof $readerControlStage;$readerReport=DRead (Binding $readerReportPath)
DRequire ($controllerReport.schema -ceq 'pas.cold-scheduler-controller-controls.v1' -and $readerReport.schema -ceq 'pas.cold-scheduler-trace-reader-controls.v1') 'exact endpoint-free controls schemas'
foreach($row in @(@{report=$controllerReport;count=32},@{report=$readerReport;count=20})){$report=$row.report;DInt $report 'assertions' $row.count;DInt $report 'failed' 0;DRequire (@($report.checks).Count -eq $row.count) 'fixed separate controls denominator';$ids=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($check in $report.checks){DRequire ($check.id -is [string] -and $check.id.Length -gt 0 -and $ids.Add($check.id)) 'unique controls id';DBool $check 'pass' $true};DBool $report 'pass' $true;DInt $report 'device_operations' 0;DBool $report 'qualification_issued' $false;DBool $report 'live_ready' $false}
DRequire ($controllerReport.tool_source_commit -ceq $FrozenHead -and $readerReport.source_commit -ceq $FrozenHead) 'actual compiled control tool source identities'
DBool $controllerReport 'tool_source_dirty' $false;DBool $readerReport 'source_dirty' $false
foreach($field in @('synthetic_api_controls','diagnostic_only','sequential_file_mode')){DBool $controllerReport $field $true};foreach($field in @('actual_recording_tested','cost_qualification')){DBool $controllerReport $field $false};foreach($field in @('etw_start_operations','etw_stop_operations','device_endpoints')){DInt $controllerReport $field 0}
DBool $readerReport 'diagnostic_only' $true;DBool $readerReport 'qualification' $false;foreach($field in @('trace_files_read','trace_sessions_started')){DInt $readerReport $field 0}
AssertInputs;foreach($pin in $outputs){CheckEntry $pin}
$bindingPath=Join-Path $artifact 'build-binding.json'
NewJson $bindingPath @{schema='pas.cold-scheduler-diagnostic-build.v1';source_commit=$FrozenHead;source_manifest=(Binding $manifestPath);source_snapshot=(Binding $snapshotPath);core_build_plan=(Binding $planPath);configuration='release';build_root=$buildRoot;translation_unit_count=3;targets=$targets;compiler_environment=$environmentPin;installed_header_inputs=$headers;files=$exes;compiled_outputs=$outputs;configure_process=$configureProof;build_process=$buildProof;controller_controls_process=$controllerProof;reader_controls_process=$readerProof;controller_controls_report=(Binding $controllerReportPath);reader_controls_report=(Binding $readerReportPath);controller_controls_count=$controllerReport.assertions;reader_controls_count=$readerReport.assertions;actual_native_stage_count=4;actual_native8_pin_count=32;original_component57=$componentPin;original_component_source_commit=$base57;original_manual_library_source_commit=$base55;original_normal55_negative=$negative55Pin;original_AA_r03_negative=$negativeR03Pin;original_libraries_imported=$false;large_original_library_hashes_performed=0;original_raw_payload_hashes_performed=0;original_negatives_preserved=$true;formal_rebuilt=$false;diagnostic_only=$true;recording_started=$false;measurement_runs=0;qualification_issued=$false;whole_candidate_qualification=$false;cost_qualification=$false;physical_qualification=$false;normal3_runs=0;AA12_runs=0;noise18_runs=0;ABBA36_runs=0;device_endpoints=0;device_operations=0;live_ready=$false;scope='Fresh three-TU standalone Windows scheduling diagnostic tools and endpoint-free controls only; original component57/base55 identities and numerical negatives remain separate; no trace or producer run.'} 4194304
@{binding=(Binding $bindingPath);translation_units=3;native_stages=4;controller_controls=$controllerReport.assertions;reader_controls=$readerReport.assertions;recording_started=$false;qualification_issued=$false;device_operations=0;live_ready=$false}|ConvertTo-Json -Depth 4
