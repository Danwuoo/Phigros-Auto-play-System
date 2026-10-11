param([Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$FrozenHead,
      [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$Attempt='main63-reader-r01',
      [string]$BaseToolsBinding='out/prelive-cold-scheduler-evaluation-main62-etw-tools-r01/build-binding.json',
      [ValidatePattern('^[0-9a-f]{64}$')][string]$BaseToolsBindingSha256='0c330035f22d3d6313130990dbdfc52242630b26610795bd16b21af3cef7bb17',
      [string]$OriginalMeasurementStop='out/planning/main62-scheduler-measurement-stop-original.json',
      [ValidatePattern('^[0-9a-f]{64}$')][string]$OriginalMeasurementStopSha256='025ffb8112b7a43237b48c4935f8bcda3ee78e70ff8c69045589993bbd7014a9')
$ErrorActionPreference='Stop'
$taskBuildScript=$PSCommandPath
. (Join-Path $PSScriptRoot '../../pre_emulator/common.ps1')
. (Join-Path $PSScriptRoot '../capacity.ps1')
$script:maintenancePath=$taskBuildScript
$Operation='build'
$baseCommit='6e3755dcabcb5f77280802657c0bbe6f9f103fb1'
function DRequire([bool]$ok,[string]$reason){if(-not $ok){throw ('cold scheduler reader delta: '+$reason)}}
function DRead($pin){DRequire ($pin.bytes -le 4194304) 'small metadata cap';CheckEntry $pin;return Get-Content -LiteralPath $pin.path -Raw|ConvertFrom-Json}
function DBool($value,[string]$key,[bool]$expected){$v=$value.$key;DRequire ($v -is [bool] -and $v -eq $expected) ('typed boolean '+$key)}
function DInt($value,[string]$key,[long]$expected){$v=$value.$key;DRequire (($v -is [int] -or $v -is [long]) -and $v -eq $expected) ('typed integer '+$key)}
function DSame($a,$b){return [string]::Equals([IO.Path]::GetFullPath($a.path),[IO.Path]::GetFullPath($b.path),[StringComparison]::OrdinalIgnoreCase) -and $a.bytes -eq $b.bytes -and $a.sha256 -ceq $b.sha256}
function DPath([string]$path){$full=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($path)){$path}else{Join-Path $repo $path}));DRequire ($full.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'workspace-contained path';$part=$full;while($part){if(Test-Path -LiteralPath $part){DRequire (-not ((Get-Item -LiteralPath $part).Attributes -band [IO.FileAttributes]::ReparsePoint)) 'no reparse path'};$part=[IO.Path]::GetDirectoryName($part)};return $full}
function DExternal([string]$path,[string]$sha){$path=DPath $path;DRequire ((Get-Item -LiteralPath $path).Length -le 4194304) 'small external pin';$pin=Binding $path;DRequire ($pin.sha256 -ceq $sha) 'external metadata SHA';return $pin}
function AssertHead{DRequire ((& git -C $repo rev-parse HEAD) -ceq $FrozenHead -and (& git -C $repo branch --show-current) -ceq 'main' -and -not (& git -C $repo status --porcelain)) 'exact clean frozen reader source'}
function AssertInputs{AssertHead;foreach($pin in @($sourcePins)+@($copies)+@($fixedPins)){CheckEntry $pin}}
function DControls($report,[string]$schema,[int]$count){DRequire ($report.schema -ceq $schema) 'controls schema';DInt $report 'assertions' $count;DInt $report 'failed' 0;DBool $report 'pass' $true;DRequire (@($report.checks).Count -eq $count) 'separate controls denominator';$ids=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($check in $report.checks){DRequire ($check.id -is [string] -and $check.id.Length -gt 0 -and $ids.Add($check.id)) 'unique controls id';DBool $check 'pass' $true};DBool $report 'diagnostic_only' $true;DBool $report 'qualification_issued' $false;DBool $report 'live_ready' $false;DInt $report 'device_operations' 0}

# Reuse the existing Native8 verifier verbatim, including state seals and the
# original closure verification-output join. No ETW or producer is executed.
$proofRecipe=Join-Path $repo 'research/windows_production_pipeline_v3/wait_diagnostic_component/build.ps1'
$proofTokens=$null;$proofErrors=$null;$proofAst=[Management.Automation.Language.Parser]::ParseFile($proofRecipe,[ref]$proofTokens,[ref]$proofErrors)
DRequire ($proofErrors.Count -eq 0) 'existing proof recipe AST'
$proofFunctions=@($proofAst.FindAll({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq 'DProof')
DRequire ($proofFunctions.Count -eq 1) 'one original Native8 helper'
. ([scriptblock]::Create($proofFunctions[0].Extent.Text))
AssertHead
DRequire ($FrozenHead -cne $baseCommit) 'fresh reader identity distinct from base62 tools'
$basePin=DExternal $BaseToolsBinding $BaseToolsBindingSha256;$base=DRead $basePin
DRequire ($basePin.bytes -eq 35379 -and $base.schema -ceq 'pas.cold-scheduler-diagnostic-build.v1' -and $base.source_commit -ceq $baseCommit) 'original main62 tools binding'
DInt $base 'translation_unit_count' 3;DInt $base 'actual_native_stage_count' 4;DInt $base 'actual_native8_pin_count' 32;DInt $base 'controller_controls_count' 32;DInt $base 'reader_controls_count' 20
DBool $base 'diagnostic_only' $true;foreach($field in @('qualification_issued','whole_candidate_qualification','cost_qualification','physical_qualification','live_ready','recording_started','formal_rebuilt')){DBool $base $field $false};DInt $base 'device_operations' 0
$baseSource=DRead $base.source_manifest;DRequire ($baseSource.source_commit -ceq $baseCommit) 'base source manifest identity';DBool $baseSource 'dirty' $false
$oldController=DRead $base.controller_controls_report;$oldReader=DRead $base.reader_controls_report
DControls $oldController 'pas.cold-scheduler-controller-controls.v1' 32;DControls $oldReader 'pas.cold-scheduler-trace-reader-controls.v1' 20
DRequire ($oldController.tool_source_commit -ceq $baseCommit -and $oldReader.source_commit -ceq $baseCommit) 'old52 controls stay with base62';DBool $oldController 'tool_source_dirty' $false;DBool $oldReader 'source_dirty' $false
$oldProcesses=@();foreach($role in @('configure_process','build_process','controller_controls_process','reader_controls_process')){$original=$base.$role;$stage=(DRead $original.verification).stage;$proof=DProof $stage;foreach($key in @('specification','input_closure','result','verification','freeze','state','contract','command')){DRequire (DSame $proof[$key] $original.$key) 'exact original Native8 binding'};$oldProcesses+=@(@{role=$role;Native8=$proof})}
$stopPin=DExternal $OriginalMeasurementStop $OriginalMeasurementStopSha256;$originalStop=DRead $stopPin
DRequire ($originalStop.status -ceq 'MEASUREMENT_WRAPPER_STOP' -and $originalStop.actual_execution_source_commit -ceq $baseCommit -and $originalStop.recording_attempts -eq 1 -and $originalStop.auditor_status_at_original_STOP -ceq 'NOT_RUN' -and $originalStop.decoder_status_at_original_STOP -ceq 'NOT_RUN') 'original actual record STOP retained'
DBool $originalStop 'qualification_issued' $false;DBool $originalStop 'live_ready' $false
$environmentPin=$base.compiler_environment;$environment=DRead $environmentPin
DRequire ($environment.schema -ceq 'pas.msvc-environment-binding.v1' -and $environment.source_commit -ceq 'aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de') 'original compiler environment'
$environmentInputs=@($environment.environment,$environment.verification,$environment.state);foreach($pin in $environmentInputs){CheckEntry $pin}
$tracked=@(& git -c core.quotePath=false -C $repo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -cmatch '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'})
DRequire ($LASTEXITCODE -eq 0 -and $tracked.Count -gt 0 -and @($tracked|Sort-Object -Unique).Count -eq $tracked.Count) 'dynamic full current source denominator'
$sourcePins=@();$sourceByRelative=@{};foreach($relative in $tracked){$pin=Binding (DPath $relative);$sourcePins+=@($pin);$sourceByRelative[$relative]=$pin}
$readerRelative='tools/cold_scheduler_trace_reader.cpp';$recipeRoot='research/cold_scheduler_diagnostic/reader_delta/'
foreach($relative in @($readerRelative,($recipeRoot+'build.ps1'),($recipeRoot+'CMakeLists.txt'))){DRequire ($sourceByRelative.ContainsKey($relative)) 'frozen reader recipe/source present'}
$artifact=DPath ('out/prelive-cold-scheduler-reader-evaluation-'+$Attempt);$buildRoot=DPath ('out/prelive-cold-scheduler-reader-'+$Attempt);$shortRoot=DPath ('out/prelive-rsrc-'+$Attempt);$shortFiles=Join-Path $shortRoot 'files'
foreach($path in @($artifact,$buildRoot,$shortRoot)){Fresh $path}
$fixedPins=@($basePin,$base.source_manifest,$base.source_snapshot,$base.core_build_plan,$base.controller_controls_report,$base.reader_controls_report,$base.original_component57,$base.original_normal55_negative,$base.original_AA_r03_negative,$stopPin,$environmentPin)+$environmentInputs+@(Binding $proofRecipe)+@($oldProcesses|ForEach-Object {$_.Native8.Values})
foreach($pin in $fixedPins){CheckEntry $pin}
$null=[IO.Directory]::CreateDirectory($artifact);$null=[IO.Directory]::CreateDirectory($shortFiles)
$copies=@();$copyByRelative=@{};$snapshotRows=@();foreach($relative in $tracked){$original=$sourceByRelative[$relative];$copyPath=DPath (Join-Path $shortFiles $relative);DRequire ($copyPath.Length -le 259) 'short source max259';$null=[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($copyPath));[IO.File]::Copy($original.path,$copyPath,$false);$copy=Binding $copyPath;DRequire ($original.bytes -eq $copy.bytes -and $original.sha256 -ceq $copy.sha256) 'lossless current source copy';$copies+=@($copy);$copyByRelative[$relative]=$copy;$snapshotRows+=@(@{relative_path=$relative;original=$original;copy=$copy})}
AssertInputs
$manifestPath=Join-Path $artifact 'source-manifest.json';NewJson $manifestPath @{schema='pas.cooperative-source.v1';source_commit=$FrozenHead;dirty=$false;files=$sourcePins;full_current_tracked_source_count=$sourcePins.Count;diagnostic_only=$true;formal_rebuilt=$false;qualification_issued=$false;live_ready=$false;device_operations=0} 4194304
$snapshotPath=Join-Path $shortRoot 'snapshot.json';NewJson $snapshotPath @{schema='pas.lossless-diagnostic-source-snapshot.v1';source_commit=$FrozenHead;source_manifest=(Binding $manifestPath);file_count=$copies.Count;files=$snapshotRows;all_copy_SHAs_verified=$true;originals_rechecked=$true;relative_paths_unchanged=$true;legacy_max_path_characters=259;diagnostic_only=$true;qualification_issued=$false;live_ready=$false;device_operations=0} 4194304
$target='cold_scheduler_trace_reader';$unit=@{target=$target;original_source=$sourceByRelative[$readerRelative];source=$copyByRelative[$readerRelative]}
$readerCmakeRoot=[IO.Path]::TrimEndingDirectorySeparator((Join-Path $shortFiles $recipeRoot))
$planPath=Join-Path $artifact 'core-build-plan.json';NewJson $planPath @{schema='pas.cold-scheduler-reader-delta-plan.v1';source_commit=$FrozenHead;source_manifest=(Binding $manifestPath);source_snapshot=(Binding $snapshotPath);base_tool_build=$basePin;base_tool_source_commit=$baseCommit;original_controller_controls=$base.controller_controls_report;original_reader_controls=$base.reader_controls_report;original_measurement_stop=$stopPin;compiler_environment=$environmentPin;configuration='release';translation_unit_count=1;translation_units=@($unit);targets=@($target);linked_windows_libraries=@('advapi32','tdh');reader_controls_count=20;reader_schema_controls_count=8;reader_prefix_controls_count=12;controller_rebuilt=$false;producer_rebuilt=$false;candidate_libraries_imported=$false;diagnostic_only=$true;recording_started=$false;qualification_issued=$false;device_operations=0;live_ready=$false} 4194304
$fixedPins+=@(Binding $manifestPath;Binding $snapshotPath;Binding $planPath)
$adapter=Join-Path $repo 'research/main_consolidation/cmake-stage.ps1';$guard=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$inputs=@($taskBuildScript,$adapter,(Join-Path $repo 'research/main_consolidation/stage.ps1'),(Join-Path $repo 'research/pre_emulator/common.ps1'))+@($sourcePins.path)+@($copies.path)+@($fixedPins.path)+(GateInputs)+@($guard.guard_source.path,$guard.process_source.path,$guard.cached_identity_controls.path,$guard.identity_wait_controls.path)
$options=@('-DVCPKG_APPLOCAL_DEPS=OFF','-DCMAKE_CXX_SCAN_FOR_MODULES=OFF','-DCMAKE_HAVE_LIB_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=','-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL',('-DPAS_SOURCE_WORKSPACE='+$shortFiles),('-DPAS_TOOL_SOURCE_COMMIT='+$FrozenHead),('-DPAS_READER_SOURCE_SHA256='+$unit.source.sha256))
$configure='configure-scheduler-reader-'+$Attempt;AssertInputs
Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=$readerCmakeRoot;BuildRoot=$buildRoot;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentPin.path;Operation='Configure';Configuration='Release';Inputs=$inputs;Options=$options}
$configureProof=DProof $configure
$ninjaPath=Join-Path $buildRoot 'build.ninja';$ninja=(Get-Content -LiteralPath $ninjaPath -Raw).Replace('\','/')
$compileRows=@([regex]::Matches($ninja,'(?m)^build ([^\r\n:]+[.]obj): CXX_COMPILER[^ \r\n]+[ \t]+([^|\r\n]+)'))
DRequire ($compileRows.Count -eq 1 -and $ninja -notmatch 'applocal[.]ps1' -and $ninja -match '/O2 /Ob2 /DNDEBUG' -and $ninja -match '(?:/|-)MD(?: |\r?\n)') 'exact1 fresh Release reader compile edge'
DRequire ($compileRows[0].Groups[1].Value.StartsWith('CMakeFiles/'+$target+'.dir/') -and [string]::Equals([IO.Path]::GetFullPath($compileRows[0].Groups[2].Value.Trim().Replace('$:',':').Replace('$ ',' ')),[IO.Path]::GetFullPath($unit.source.path),[StringComparison]::OrdinalIgnoreCase)) 'exact copied reader source target'
foreach($lib in @('advapi32','tdh')){DRequire ($ninja -match ('(?i)\b'+$lib+'[.]lib\b')) ('Windows SDK library '+$lib)}
DRequire ($ninja -notmatch '(?i)(pas_core|pas_current_candidate|pas_proto|own_bridge)[.]lib' -and $ninja -notmatch 'cold_scheduler_trace_controller[.]exe') 'standalone reader graph without old objects/libraries/controller'
$graphPin=Binding $ninjaPath;$cachePin=Binding (Join-Path $buildRoot 'CMakeCache.txt');$inputs+=@($graphPin.path,$cachePin.path)+@($configureProof.Values.path)
$build='build-scheduler-reader-'+$Attempt;AssertInputs
Stage $build $adapter @{ReceiptRepo=$repo;SourceRoot=$readerCmakeRoot;BuildRoot=$buildRoot;StageName=$build;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentPin.path;Operation='Build';Configuration='Release';Inputs=$inputs;Options=@('--target',$target)}
$buildProof=DProof $build;AssertInputs
$outputs=@(Get-ChildItem -LiteralPath $buildRoot -File -Recurse|ForEach-Object {Binding $_.FullName});$objects=@($outputs|Where-Object {$_.path.Replace('\','/') -cmatch '/CMakeFiles/cold_scheduler_trace_reader[.]dir/.*[.]obj$'})
DRequire ($objects.Count -eq 1) 'one actual fresh reader object';foreach($pin in $outputs|Where-Object {$_.path -match '[.]obj$' -and $_ -notin $objects}){DRequire ($pin.path.Replace('\','/') -cmatch '/(?:CompilerIdCXX/CMakeCXXCompilerId[.]obj|CMakeFiles/ShowIncludes/main[.]obj)$') 'only standard CMake probe objects additional'}
$exes=@($outputs|Where-Object {[IO.Path]::GetFileName($_.path) -ceq ($target+'.exe')});DRequire ($exes.Count -eq 1) 'one fresh reader executable';$script:seal=@{files=$exes}
$reportPath=Join-Path $artifact 'reader-self-test.json';$test='test-scheduler-reader-'+$Attempt;AssertInputs
Native $test $exes[0].path @('--self-test','--output',$reportPath) @($inputs+$outputs.path+$buildProof.Values.path) 300
$readerProof=DProof $test;$report=DRead (Binding $reportPath)
DControls $report 'pas.cold-scheduler-trace-reader-controls.v1' 20;DControls $report.schema_controls 'pas.cold-scheduler-trace-reader-schema-controls.v1' 8
DControls $report.prefix_controls 'pas.cold-scheduler-trace-reader-prefix-controls.v1' 12
DRequire ($report.source_commit -ceq $FrozenHead) 'actual compiled new reader identity';DBool $report 'source_dirty' $false;DBool $report 'qualification' $false;foreach($field in @('trace_files_read','trace_sessions_started')){DInt $report $field 0}
AssertInputs;foreach($pin in $outputs){CheckEntry $pin}
$bindingPath=Join-Path $artifact 'build-binding.json';NewJson $bindingPath @{schema='pas.cold-scheduler-reader-delta-build.v1';source_commit=$FrozenHead;source_manifest=(Binding $manifestPath);source_snapshot=(Binding $snapshotPath);core_build_plan=(Binding $planPath);base_tool_build=$basePin;base_tool_source_commit=$baseCommit;base_tool_native_processes=$oldProcesses;original_controller_controls=$base.controller_controls_report;original_reader_controls=$base.reader_controls_report;original_measurement_stop=$stopPin;original_component57=$base.original_component57;original_manual_library_source_commit=$base.original_manual_library_source_commit;configuration='release';build_root=$buildRoot;translation_unit_count=1;targets=@($target);compiler_environment=$environmentPin;files=$exes;compiled_outputs=$outputs;compile_graph=$graphPin;configure_process=$configureProof;build_process=$buildProof;reader_controls_process=$readerProof;reader_controls_report=(Binding $reportPath);reader_controls_count=$report.assertions;reader_schema_controls_count=$report.schema_controls.assertions;reader_prefix_controls_count=$report.prefix_controls.assertions;actual_native_stage_count=3;actual_native8_pin_count=24;controller_rebuilt=$false;producer_rebuilt=$false;original_candidate_libraries_imported=$false;original_raw_payload_hashes_performed=0;original_large_library_hashes_performed=0;formal_rebuilt=$false;diagnostic_only=$true;recording_started=$false;measurement_runs=0;qualification_issued=$false;whole_candidate_qualification=$false;cost_qualification=$false;physical_qualification=$false;normal3_runs=0;AA12_runs=0;noise18_runs=0;ABBA36_runs=0;device_endpoints=0;device_operations=0;live_ready=$false;scope='Only one fresh offline reader TU and its separate20 original/8 schema/12 prefix controls. Base62 controller/tools52 controls, component57/base55 producer and all numerical negatives remain separate historical inputs; no new recording or workload.'} 4194304
@{binding=(Binding $bindingPath);translation_units=1;native_stages=3;original_reader_controls=20;schema_controls=8;prefix_controls=12;recording_started=$false;qualification_issued=$false;device_operations=0;live_ready=$false}|ConvertTo-Json -Depth 4
