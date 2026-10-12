param([Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$FrozenHead,
      [Parameter(Mandatory)][string]$BaseProductionBinding,
      [Parameter(Mandatory)][string]$BaseCpuBinding,
      [Parameter(Mandatory)][string]$BaseSourceSnapshot,
      [string]$BaseSourceProjection='out/planning/main57-base55-aa-r03-short-source-copy-proof.json',
      [ValidatePattern('^[a-z0-9-]{1,12}$')][string]$ShortSourceId='m69e1',
      [ValidatePattern('^[a-z0-9-]{1,32}$')][string]$Attempt='main78-eligibility-r01',
      [switch]$MotionSourceDelta,[switch]$DenseSourceDelta,[switch]$FrameMetadataDelta)
$ErrorActionPreference='Stop'
$taskBuildScript=$PSCommandPath
. (Join-Path $PSScriptRoot '../common.ps1')
. (Join-Path $PSScriptRoot '../component_delta.ps1')
. (Join-Path $PSScriptRoot '../source_performance_deltas.ps1')
$script:maintenancePath=$taskBuildScript
$Operation='build'
$baseCommit='12ec0550e882e3b06751d67fa67c43dda3e9312b'
$originalCommit='aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de'
$deltaPolicy='highest_raw_efficiency_class_disjoint_balanced_core_lowest_lps'
# These nested controls have separate denominators; original controls retain
# their own existing counts and meanings.
$eligibilityControlsCount=12
$eligibilityReaderControlsCount=12
$verificationControlsCount=12
if($DenseSourceDelta -or $FrameMetadataDelta){if(-not $MotionSourceDelta){throw 'performance deltas require preserved motion branch'}}
function DRequire([bool]$ok,[string]$reason){if(-not $ok){throw ('producer eligibility delta: '+$reason)}}
function DRead($pin){DRequire ($pin.bytes -le 4194304) 'small metadata cap';CheckEntry $pin;return Get-Content -LiteralPath $pin.path -Raw|ConvertFrom-Json}
function DBool($value,[string]$key,[bool]$expected){$v=$value.$key;DRequire ($v -is [bool] -and $v -eq $expected) ('typed boolean '+$key)}
function DInt($value,[string]$key,[long]$expected){$v=$value.$key;DRequire (($v -is [int] -or $v -is [long]) -and $v -eq $expected) ('typed integer '+$key)}
function DSame($a,$b){return [string]::Equals([IO.Path]::GetFullPath($a.path),[IO.Path]::GetFullPath($b.path),[StringComparison]::OrdinalIgnoreCase) -and $a.bytes -eq $b.bytes -and $a.sha256 -ceq $b.sha256}
function DPath([string]$path){$full=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($path)){$path}else{Join-Path $repo $path}));DRequire ($full.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'workspace-contained path';$part=$full;while($part){if(Test-Path -LiteralPath $part){DRequire (-not ((Get-Item -LiteralPath $part).Attributes -band [IO.FileAttributes]::ReparsePoint)) 'no reparse path'};$part=[IO.Path]::GetDirectoryName($part)};return $full}
function DRelative([string]$path){return [IO.Path]::GetRelativePath($repo,(DPath $path)).Replace('\','/')}
function AssertHead{DRequire ((& git -C $repo rev-parse HEAD) -ceq $FrozenHead -and (& git -C $repo branch --show-current) -ceq 'main' -and -not (& git -C $repo status --porcelain)) 'exact frozen clean main'}
function AssertInputs{AssertHead;foreach($pin in @($sourcePins)+@($sourceCopies)+@($fixedPins)){CheckEntry $pin}}
function AssertControlInputs{AssertHead;foreach($pin in @($sourcePins)+@($sourceCopies)+@($fixedPins)){if(-not $controlCompileOnlyPaths.Contains([IO.Path]::GetFullPath($pin.path))){CheckEntry $pin}}}
function DOutput($binding,[string]$relative){$expected=[IO.Path]::GetFullPath((Join-Path $binding.build_root $relative));$found=@((@($binding.files)+@($binding.compiled_outputs))|Where-Object {$null -ne $_ -and $_.path -and [IO.Path]::GetFullPath($_.path) -ceq $expected}|Group-Object path|ForEach-Object {$_.Group[0]});DRequire ($found.Count -eq 1) ('one sealed original output '+$relative);return $found[0]}
function DSourceMap($manifest){$map=@{};foreach($pin in $manifest.files){$relative=DRelative $pin.path;DRequire (-not $map.ContainsKey($relative)) 'unique original source';$map[$relative]=$pin};return $map}
function DControls($report,[string]$schema,[int]$count){DRequire ($report.schema -ceq $schema) 'separate controls schema';DInt $report 'assertions' $count;DInt $report 'failed' 0;DBool $report 'pass' $true;DRequire (@($report.checks).Count -eq $count) 'separate controls denominator';$ids=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($case in $report.checks){DRequire ($case.id -is [string] -and $case.id.Length -gt 0 -and $ids.Add($case.id)) 'unique nonempty control id';DBool $case 'pass' $true};DInt $report 'device_operations' 0;foreach($field in @('qualification_issued','cost_qualification','live_ready')){if($report.PSObject.Properties.Name -ccontains $field){DBool $report $field $false}}}
function DEligibilityIdentity($report){$identity=$report.producer_eligibility_delta_identity;DRequire ($identity.schema -ceq 'pas.cold-producer-eligibility-identity.v1' -and $identity.source_commit -ceq $FrozenHead -and $identity.base_source_commit -ceq $baseCommit -and $identity.worker_affinity_sha256 -ceq $sourceByRelative['research/prelive_current/worker_affinity.hpp'].sha256 -and $identity.producer_placement_sha256 -ceq $sourceByRelative['research/windows_production_pipeline_v3/producer_placement.hpp'].sha256) 'actual compiled eligibility identity';DBool $identity 'source_dirty' $false;foreach($field in @('qualification_issued','cost_qualification','live_ready')){DBool $identity $field $false};DInt $identity 'device_operations' 0}
function DCmakeLiteral([string]$text){$v=$text.Replace('\','/');DRequire ($v -notmatch '[";$\r\n]') 'literal CMake input';return '"'+$v+'"'}
function DLinkRoles([string]$text,$required,$all,[string]$root){
 $required=@($required|Where-Object {$null -ne $_})
 $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 foreach($token in [regex]::Matches($text,'"[^"]*"|\S+')){$path=$token.Value.Trim('"');$name=[IO.Path]::GetFileName($path);$known=@($all|Where-Object {[IO.Path]::GetFileName($_.binary.path) -ieq $name});if($known.Count -eq 0){continue}
  $resolved=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($path)){$path}else{Join-Path $root $path}));$exact=@($required|Where-Object {[string]::Equals([IO.Path]::GetFullPath($_.binary.path),$resolved,[StringComparison]::OrdinalIgnoreCase)})
  DRequire ($exact.Count -eq 1 -and $seen.Add($exact[0].role)) 'actual linker exact historical role/path once; no shadow/duplicate/cross-core archive'
 }
 DRequire ($seen.Count -eq @($required).Count) 'all exact historical roles in actual LINK_LIBRARIES'
}
function DProjection($projection,$projectionSnapshotPin,$projectionManifestPin,$originalMap,$snapshotMap){
 DRequire ($projection.schema -ceq 'pas.lossless-source-path-projection.v1' -and $projection.source_commit -ceq $baseCommit -and (DSame $projection.original_snapshot $projectionSnapshotPin) -and (DSame $projection.original_source_manifest $projectionManifestPin)) 'actual base55 short projection provenance'
 foreach($field in @('all_originals_before_after_verified','all_copies_after_verified','original_snapshot_unchanged','relative_paths_unchanged','source_path_projection_only')){DBool $projection $field $true}
 foreach($field in @('current_source_qualification','whole_candidate_qualification','qualification_issued','cost_ready','live_ready')){DBool $projection $field $false};DInt $projection 'device_operations' 0;DInt $projection 'native_launches' 0;DInt $projection 'legacy_max_path_characters' 259;DInt $projection 'file_count' $originalMap.Count
 DRequire (@($projection.files).Count -eq $originalMap.Count) 'complete base55 short projection denominator'
 $copies=@{};$bytes=0L;$maximum=0
 foreach($row in $projection.files){$relative=$row.relative_path;DRequire ($relative -is [string] -and $originalMap.ContainsKey($relative) -and $snapshotMap.ContainsKey($relative) -and -not $copies.ContainsKey($relative) -and (DSame $row.original_copy $snapshotMap[$relative].copy)) 'unique exact original snapshot projection row'
  $expected=DPath (Join-Path $projection.repo_root $relative);DRequire ([string]::Equals($expected,[IO.Path]::GetFullPath($row.copy.path),[StringComparison]::OrdinalIgnoreCase) -and $expected.Length -le 259 -and $row.copy.bytes -eq $originalMap[$relative].bytes -and $row.copy.sha256 -ceq $originalMap[$relative].sha256) 'complete lossless short base source copy'
  CheckEntry $row.copy;$copies[$relative]=$row.copy;$bytes+=$row.copy.bytes;$maximum=[Math]::Max($maximum,$expected.Length)
 }
 DInt $projection 'total_bytes' $bytes
 # The preserved proof writes 205.0. This historical descriptive field is
 # compared to the actual integer path maximum without rewriting that proof.
 DRequire (($projection.max_path_characters -is [int] -or $projection.max_path_characters -is [long] -or $projection.max_path_characters -is [double]) -and $projection.max_path_characters -eq $maximum) 'actual historical projection path maximum'
 return $copies
}
function DDependencySelection($original,$baseSelection,$runtime,[string]$originalGraph){
 $installed=DPath 'out/vcpkg_installed/x64-windows';$byPath=@{};foreach($pin in $original.files){$key=[IO.Path]::GetFullPath($pin.path);DRequire (-not $byPath.ContainsKey($key)) 'unique full original installed metadata';$byPath[$key]=$pin}
 $chosen=@{};$reasons=@{};$add={param([string]$path,[string]$reason) $key=[IO.Path]::GetFullPath($path);DRequire ($byPath.ContainsKey($key) -and $key.StartsWith($installed+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'actual dependency is exact original installed member';$chosen[$key]=$byPath[$key];if(-not $reasons.ContainsKey($key)){$reasons[$key]=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)};$null=$reasons[$key].Add($reason)}
 DRequire ($baseSelection.schema -ceq 'pas.narrow-noise-repair-dependency-selection.v1' -and (DSame $baseSelection.original_dependency_manifest $base.dependency_manifest)) 'preserved original CPU55 selected dependency closure'
 foreach($pin in $baseSelection.files){$key=[IO.Path]::GetFullPath($pin.path);DRequire ($byPath.ContainsKey($key) -and (DSame $byPath[$key] $pin)) 'CPU55 selected pin belongs to full original manifest';& $add $key 'preserved_cpu55_selection'}
 foreach($pin in $runtime){$key=[IO.Path]::GetFullPath($pin.path);DRequire ($byPath.ContainsKey($key) -and (DSame $byPath[$key] $pin)) 'runtime source belongs to full original manifest';& $add $key 'actual_runtime18_source'}
 # The explicit old app-object relink uses the same CLI/grpc/protobuf/FFmpeg
 # target library set as the actual base55 pas link. Every new configured link
 # is checked against this selection before its build can launch.
 $oldAppLink=@([regex]::Matches($originalGraph,'(?m)^build production/pas[.]exe: CXX_EXECUTABLE_LINKER[^\r\n]*\r?\n(?<body>(?:[ \t]+[^\r\n]*\r?\n)*)'));DRequire ($oldAppLink.Count -eq 1) 'one actual base55 app relink dependency edge';$linkRows=@([regex]::Matches($oldAppLink[0].Groups['body'].Value,'(?m)^  LINK_LIBRARIES = (.*)$'));DRequire ($linkRows.Count -eq 1) 'actual base55 app linker dependency field';foreach($row in $linkRows){foreach($token in [regex]::Matches($row.Groups[1].Value.Replace('$:',':').Replace('$ ',' '),'"[^"]*"|\S+')){$path=$token.Value.Trim('"');if([IO.Path]::IsPathRooted($path) -and [IO.Path]::GetFullPath($path).Replace('\','/').StartsWith($installed.Replace('\','/')+'/lib/',[StringComparison]::OrdinalIgnoreCase)){& $add $path 'actual_base55_release_link_library'}}}
 # These package configuration files are read by the declared find_package
 # calls, including their transitive imported targets. They do not authorize
 # unused Debug archive payloads as Native inputs.
 foreach($pin in $original.files){$relative=[IO.Path]::GetRelativePath($installed,$pin.path).Replace('\','/');if($relative -cmatch '^share/(nlohmann_json|zlib|cli11|protobuf|grpc|absl|utf8_range|re2|c-ares|openssl|ffmpeg)/.*[.]cmake$'){& $add $pin.path 'declared_package_configuration'}}
 if($MotionSourceDelta){foreach($pin in $original.files){$relative=[IO.Path]::GetRelativePath($installed,$pin.path).Replace('\','/');if($relative -cmatch '^share/gtest/.*[.]cmake$' -or $relative -cin @('lib/gtest.lib','lib/manual-link/gtest_main.lib')){& $add $pin.path 'actual_motion_regression_GTest_release'}}}
 $roots=@($repo,(Join-Path $repo 'include'),(Join-Path $repo 'research/prelive_current'),(Join-Path $repo 'research/bvi_cold_v3'),(Join-Path $repo 'research/hold_front_current'),(Join-Path $base.build_root 'generated'),(Join-Path $base.build_root 'production/generated'),(Join-Path $installed 'include'))
 $queue=[Collections.Generic.Queue[string]]::new();foreach($relative in @('research/prelive_current/chain.cpp','research/prelive_current/tests/worker_affinity_tests.cpp','research/windows_production_pipeline_v3/main.cpp','src/grpc_transport.cpp','src/manual_session.cpp','research/pre_emulator/score_only_controls.cpp','research/pre_emulator/qualification_reader/main.cpp','research/pre_emulator/qualification_reader/controls.cpp','src/candidate_admission.cpp','tests/candidate_admission_tests.cpp')){$queue.Enqueue((DPath $relative))}
 if($MotionSourceDelta){foreach($relative in @('src/game_motion.cpp','src/runtime.cpp','tests/game_tests.cpp','tests/cold_scenario_tests.cpp')){$queue.Enqueue((DPath $relative))}}
 $visited=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
 while($queue.Count){$path=$queue.Dequeue();if(-not $visited.Add($path)){continue};DRequire ($visited.Count -le 4096 -and (Get-Item -LiteralPath $path).Length -le 4194304) 'bounded direct/transitive include search';$text=[IO.File]::ReadAllText($path)
  foreach($match in [regex]::Matches($text,'(?m)^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]')){$name=$match.Groups[1].Value;$resolved=$null;foreach($root in @([IO.Path]::GetDirectoryName($path))+$roots){$candidate=[IO.Path]::GetFullPath((Join-Path $root $name));if(Test-Path -LiteralPath $candidate -PathType Leaf){$resolved=$candidate;break}};if(-not $resolved){continue};if($resolved.StartsWith($installed+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){& $add $resolved 'fresh_TU_transitive_include_search'};if($resolved.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){$queue.Enqueue($resolved)}}
 }
 $files=@($chosen.Values|Sort-Object path);return @{files=$files;reasons=@($files|ForEach-Object {@{input=$_;reasons=@($reasons[$_.path]|Sort-Object)}});fixed_installed_root=$installed;include_search_files=$visited.Count}
}

# Original helper includes natural completion, all Native8 seals and exact
# closure verification_output == input_closure.path. It is imported as source,
# never executed as an old build recipe and never replaced by caller flags.
$proofRecipe=Join-Path $repo 'research/windows_production_pipeline_v3/wait_diagnostic_component/build.ps1'
$tok=$null;$err=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($proofRecipe,[ref]$tok,[ref]$err)
DRequire ($err.Count -eq 0) 'existing Native8 helper AST'
$functions=@($ast.FindAll({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq 'DProof')
DRequire ($functions.Count -eq 1) 'one existing Native8 verifier';. ([scriptblock]::Create($functions[0].Extent.Text))
AssertHead;DRequire ($FrozenHead -cne $baseCommit -and $eligibilityControlsCount -gt 0 -and $eligibilityReaderControlsCount -gt 0) 'distinct frozen delta and finalized separate control counts'
$basePin=Binding (DPath $BaseProductionBinding);$cpuPin=Binding (DPath $BaseCpuBinding);$snapshotPin=Binding (DPath $BaseSourceSnapshot);$baseProjectionPin=Binding (DPath $BaseSourceProjection)
$base=DRead $basePin;$cpu=DRead $cpuPin;$baseSnapshot=DRead $snapshotPin
DRequire ($base.schema -ceq 'pas.cooperative-build.v1' -and $cpu.schema -ceq 'pas.narrow-noise-repair-build.v1' -and $base.source_commit -ceq $baseCommit -and $cpu.source_commit -ceq $baseCommit -and $base.configuration -ceq 'release' -and $cpu.configuration -ceq 'release' -and (DSame $base.noise_repair_build $cpuPin) -and (DSame $base.source_manifest $cpu.source_manifest) -and (DSame $base.dependency_manifest $cpu.dependency_manifest)) 'actual base55 production/CPU/source/dependency join'
$baseSource=DRead $base.source_manifest;$baseMap=DSourceMap $baseSource
DRequire ($baseSource.schema -ceq 'pas.cooperative-source.v1' -and $baseSource.source_commit -ceq $baseCommit -and $baseSnapshot.schema -ceq 'pas.lossless-diagnostic-source-snapshot.v1' -and $baseSnapshot.source_commit -ceq $baseCommit -and (DSame $baseSnapshot.source_manifest $base.source_manifest)) 'actual original55 source snapshot'
DBool $baseSource 'dirty' $false;DBool $baseSnapshot 'all_copy_SHAs_verified' $true;DBool $baseSnapshot 'originals_rechecked' $true
DInt $baseSnapshot 'full_current_tracked_source_count' 939;DInt $baseSnapshot 'file_count' 946;DRequire (@($baseSource.files).Count -eq 939 -and @($baseSnapshot.files).Count -eq 946) 'actual original55 source/snapshot denominators'
$snapshotByRelative=@{};foreach($row in $baseSnapshot.files){$relative=DRelative $row.original.path;DRequire (-not $snapshotByRelative.ContainsKey($relative) -and $row.original.bytes -eq $row.copy.bytes -and $row.original.sha256 -ceq $row.copy.sha256) 'exact original/copy snapshot pair';$snapshotByRelative[$relative]=$row}
foreach($relative in $baseMap.Keys){DRequire ($snapshotByRelative.ContainsKey($relative) -and (DSame $snapshotByRelative[$relative].original $baseMap[$relative])) 'complete original source snapshot mapping'}
$baseProjection=DRead $baseProjectionPin;$baseProjectionCopies=DProjection $baseProjection $snapshotPin $base.source_manifest $baseMap $snapshotByRelative
$old32Pin=$cpu.base_release_build;$old32=DRead $old32Pin;$old32Source=DRead $old32.source_manifest;$old32Map=DSourceMap $old32Source
DRequire ($old32.schema -ceq 'pas.cooperative-build.v1' -and $old32.source_commit -ceq $originalCommit -and $old32Source.source_commit -ceq $originalCommit) 'actual main32 library provenance'
$cpuPlan=DRead $cpu.core_build_plan;$productionPlan=DRead $base.core_build_plan
DRequire ($cpuPlan.source_commit -ceq $baseCommit -and $productionPlan.source_commit -ceq $baseCommit) 'actual55 base plans'
$dependencies=DRead $base.dependency_manifest
DRequire ($dependencies.source_commit -ceq $originalCommit -and $dependencies.release_and_debug -eq $true -and $dependencies.protoc_and_plugin -eq $true) 'complete original installed dependency provenance'
$environmentPin=$cpu.compiler_environment;$environment=DRead $environmentPin
DRequire ($environment.schema -ceq 'pas.msvc-environment-binding.v1' -and $environment.source_commit -ceq $originalCommit) 'original compiler environment'
$authorization=DRead $cpu.tests_scope_authorization
DRequire ($authorization.status -ceq 'HUMAN_AUTHORIZED' -and $authorization.scope -ceq 'noise_and_unrun_abba_only') 'preserved cold scope';DInt $authorization 'device_operations' 0;DBool $authorization 'live_ready' $false
$baseNative=@();foreach($entry in @(@{role='cpu';binding=$cpu},@{role='production';binding=$base})){
 $b=$entry.binding;$proof=DProof (DRead $b.configure_receipt).stage;DRequire (DSame $proof.verification $b.configure_receipt) 'original configure Native8 join';$baseNative+=@(@{role=$entry.role;kind='configure';Native8=$proof})
 foreach($pin in $b.build_receipts){$proof=DProof (DRead $pin).stage;DRequire (DSame $proof.verification $pin) 'original build Native8 join';$baseNative+=@(@{role=$entry.role;kind='build';Native8=$proof})}
}

# Derive each imported archive's actual compile members from its sealed Ninja
# graph. Each row records both historical object pin and the source manifest
# owning the compile; this does not treat an old object as freshly compiled.
function DLibrary([string]$role,$binding,$bindingPin,[string]$commit,$manifestPin,$map,[string]$archiveRelative){
 $archive=DOutput $binding $archiveRelative;CheckEntry $archive
 $graph=DOutput $binding 'build.ninja';$graphText=(DReadText $graph).Replace('\','/')
 $compileRows=@([regex]::Matches($graphText,'(?m)^build ([^\r\n:]+[.]obj): CXX_COMPILER[^ \r\n]+[ \t]+([^|\r\n]+)'))
 $escaped=[regex]::Escape($archiveRelative.Replace('\','/'));$archiveRows=@([regex]::Matches($graphText,'(?m)^build '+$escaped+': CXX_STATIC_LIBRARY_LINKER[^ \r\n]+[ \t]+([^\r\n]+)'))
 DRequire ($archiveRows.Count -eq 1) ('one actual static archive row '+$role)
 $objects=@(($archiveRows[0].Groups[1].Value -split '[|]')[0].Trim() -split '\s+'|Where-Object {$_});DRequire ($objects.Count -gt 0 -and @($objects|Sort-Object -Unique).Count -eq $objects.Count) 'unique actual archive members'
 $tuples=@();foreach($object in $objects){$memberCompileRows=@($compileRows|Where-Object {$_.Groups[1].Value -ceq $object});DRequire ($memberCompileRows.Count -eq 1) 'archive object has exact compile edge'
  $path=[IO.Path]::GetFullPath($memberCompileRows[0].Groups[2].Value.Trim().Replace('$:',':').Replace('$ ',' '));$normalized=$path.Replace('\','/')
  $rel=@($map.Keys|Where-Object {$normalized.EndsWith('/'+$_,[StringComparison]::OrdinalIgnoreCase)})
  if($rel.Count -eq 1){$source=$map[$rel[0]];$relative=$rel[0]}else{DRequire ($rel.Count -eq 0 -and $normalized -match '/generated/emulator_controller[.](?:grpc[.])?pb[.]cc$') 'only declared generated proto source outside tracked manifest';$source=DOutput $binding ('generated/'+[IO.Path]::GetFileName($path));$relative='generated/'+[IO.Path]::GetFileName($path)}
  $objectPin=DOutput $binding $object
  $tuples+=@(@{relative_source=$relative;original_source=$source;actual_compile_source_path=$path;historical_object=$objectPin;archive_member=$object})
 }
 return @{role=$role;binary=$archive;compiled_source_commit=$commit;build_binding=$bindingPin;source_manifest=$manifestPin;compile_graph=$graph;translation_unit_count=$tuples.Count;translation_units=$tuples;historical_import=$true;freshly_rebuilt=$false}
}
function DReadText($pin){DRequire ($pin.bytes -le 4194304) 'bounded source/graph text';CheckEntry $pin;return Get-Content -LiteralPath $pin.path -Raw}
$libraries=@()
foreach($role in @('current_rails','current_front','exact_bvi_v3')){$row=DLibrary ('cpu_'+$role) $old32 $old32Pin $originalCommit $old32.source_manifest $old32Map ('cold/'+$role+'.lib');$expected=@($cpu.linked_static_inputs|Where-Object {[IO.Path]::GetFileName($_.path) -ceq ($role+'.lib')});DRequire ($expected.Count -eq 1 -and (DSame $row.binary $expected[0])) 'actual original32 CPU effective role';$libraries+=@($row)}
foreach($role in @('own_bridge','pas_core')){$row=DLibrary ('cpu_'+$role) $cpu $cpuPin $baseCommit $cpu.source_manifest $baseMap ($role+'.lib');$expected=@($cpu.linked_static_inputs|Where-Object {[IO.Path]::GetFileName($_.path) -ceq ($role+'.lib')});DRequire ($expected.Count -eq 1 -and (DSame $row.binary $expected[0])) 'actual CPU55 effective role';$libraries+=@($row)}
foreach($role in @('pas_core','pas_current_candidate','pas_proto')){$libraries+=@(DLibrary ('production_'+$role) $base $basePin $baseCommit $base.source_manifest $baseMap ('production/'+$role+'.lib'))}
foreach($role in @('pas_emulator','pas_bench')){$row=DLibrary ('verification_'+$role) $base $basePin $baseCommit $base.source_manifest $baseMap ('production/'+$role+'.lib');if($role -ceq 'pas_emulator'){$row.replaced_archive_sources=@('src/candidate_admission.cpp');$row.replacement_target='candidate_admission_delta'};$libraries+=@($row)}
if($MotionSourceDelta){
 foreach($row in $libraries){if($row.role -cin @('cpu_pas_core','production_pas_core')){
  $row.replaced_archive_sources=@('src/game_motion.cpp');$row.replacement_target='game_motion_delta'
 }}
}
if($DenseSourceDelta){foreach($row in $libraries){if($row.role -cin @('cpu_pas_core','production_pas_core')){
 $row.replaced_archive_sources=@('src/game_motion.cpp','src/game.cpp');$row.Remove('replacement_target')
 $row.replacement_targets=@(@{source='src/game_motion.cpp';target='game_motion_delta'},@{source='src/game.cpp';target='game_delta'})
}}}
$baseGraph=DOutput $base 'build.ninja';$baseGraphText=(DReadText $baseGraph).Replace('\','/');$appRows=@([regex]::Matches($baseGraphText,'(?m)^build (production/CMakeFiles/pas[.]dir/[^\r\n:]+[.]obj): CXX_COMPILER[^ \r\n]+[ \t]+([^|\r\n]+)')|Where-Object {$_.Groups[2].Value.Trim().Replace('$:',':').Replace('$ ',' ').EndsWith('/apps/pas/main.cpp',[StringComparison]::OrdinalIgnoreCase)})
DRequire ($appRows.Count -eq 1) 'one original base55 app object compile edge';$baseAppObject=DOutput $base $appRows[0].Groups[1].Value;CheckEntry $baseAppObject
$tracked=@(& git -c core.quotePath=false -C $repo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -cmatch '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'})
DRequire ($LASTEXITCODE -eq 0 -and $tracked.Count -gt 0 -and @($tracked|Sort-Object -Unique).Count -eq $tracked.Count) 'dynamic complete source denominator'
$sourcePins=@();$sourceByRelative=@{};foreach($relative in $tracked){$pin=Binding (DPath $relative);$sourcePins+=@($pin);$sourceByRelative[$relative]=$pin}
# Headers and actual unchanged source members of imported libraries must match
# their recorded base. Research wait/placement/harness changes are fresh TUs.
if($MotionSourceDelta){$motionText=DReadText $sourceByRelative['src/game_motion.cpp'];ComponentDeltaMotionSourceText (DReadText $baseProjectionCopies['src/game_motion.cpp']) $motionText;DRequire ($sourceByRelative['src/game_motion.cpp'].bytes -eq [Text.Encoding]::UTF8.GetByteCount($motionText)) 'motion source UTF8 without BOM'}
if($DenseSourceDelta){ComponentDeltaDenseSourceText $baseProjectionCopies['src/game.cpp'] $sourceByRelative['src/game.cpp']}
DRequire ((DReadText $sourceByRelative['research/prelive_current/chain.cpp']).Contains('#include "frame_metadata.hpp"') -eq [bool]$FrameMetadataDelta) 'fixed metadata source and explicit branch agree'
if($FrameMetadataDelta){
 $frameBaseChain=Binding (DPath 'out/prelive-esrc-m80e1/files/research/prelive_current/chain.cpp')
 ComponentDeltaFrameSourceText $frameBaseChain $sourceByRelative['research/prelive_current/chain.cpp']
 $fixedPins+=@($frameBaseChain)
}
$importedSources=@{};foreach($row in $libraries){foreach($tuple in $row.translation_units){if(-not $tuple.relative_source.StartsWith('generated/') -and $tuple.relative_source -cnotin $row.replaced_archive_sources -and -not ($MotionSourceDelta -and $tuple.relative_source -ceq 'src/game_motion.cpp' -and $row.role -cin @('cpu_pas_core','production_pas_core'))){$relative=$tuple.relative_source;DRequire ($sourceByRelative.ContainsKey($relative) -and $sourceByRelative[$relative].bytes -eq $tuple.original_source.bytes -and $sourceByRelative[$relative].sha256 -ceq $tuple.original_source.sha256) ('imported effective compiled source changed '+$relative);$importedSources[$relative]=$tuple.original_source}}}
foreach($relative in $baseMap.Keys|Where-Object {$_ -cmatch '^include/' -or $_ -ceq 'CMakeLists.txt' -or $_ -ceq 'src/manual_session.cpp' -or $_ -ceq 'src/grpc_transport.cpp'}){DRequire ($sourceByRelative.ContainsKey($relative) -and $sourceByRelative[$relative].bytes -eq $baseMap[$relative].bytes -and $sourceByRelative[$relative].sha256 -ceq $baseMap[$relative].sha256) ('unchanged formal ABI/manual/grpc source '+$relative)}
DRequire ($sourceByRelative['apps/pas/main.cpp'].bytes -eq $baseMap['apps/pas/main.cpp'].bytes -and $sourceByRelative['apps/pas/main.cpp'].sha256 -ceq $baseMap['apps/pas/main.cpp'].sha256) 'original app TU source preserved for explicit imported object'
$manual=$sourceByRelative['src/manual_session.cpp'];$generated=@(DOutput $base 'production/generated/session_build_provenance.hpp';DOutput $base 'generated/emulator_controller.pb.h';DOutput $base 'generated/emulator_controller.grpc.pb.h')
$sessionText=DReadText $generated[0];DRequire ($sessionText.Contains('pas_session_build_commit="'+$baseCommit+'"') -and $sessionText.Contains('pas_session_build_dirty=false')) 'base55 manual/session compiled identity retained'
$runtimePlan=DRead $base.runtime_copy_plan;$runtimeProof=DRead $base.runtime_copy_verification
DRequire ($runtimePlan.schema -ceq 'pas.bound-release-runtime-copy-plan.v1' -and $runtimeProof.schema -ceq 'pas.bound-release-runtime-copy-verification.v1' -and $runtimePlan.source_commit -ceq $baseCommit -and $runtimeProof.source_commit -ceq $baseCommit -and (DSame $runtimePlan.dependency_manifest $base.dependency_manifest) -and (DSame $runtimeProof.plan $base.runtime_copy_plan)) 'base55 original runtime copy joins'
DBool $runtimePlan 'applocal_enabled' $false;DBool $runtimeProof 'pass' $true;DInt $runtimePlan 'source_file_count' 18;DInt $runtimePlan 'copy_file_count' 36;DInt $runtimeProof 'file_count' 36
$runtimeSources=@($runtimePlan.source_files);DRequire ($runtimeSources.Count -eq 18 -and @($runtimeSources.path|Sort-Object -Unique).Count -eq 18) 'unique original runtime18'
$baseSelectionPin=$cpu.selected_dependency_manifest;$baseSelection=DRead $baseSelectionPin
$dependencySelection=DDependencySelection $dependencies $baseSelection $runtimeSources $baseGraphText
$fixedPins=@($basePin,$cpuPin,$snapshotPin,$baseProjectionPin,$baseSelectionPin,$base.source_manifest,$base.dependency_manifest,$base.core_build_plan,$base.runtime_copy_plan,$base.runtime_copy_verification,$cpu.core_build_plan,$cpu.compiler_environment,$cpu.tests_scope_authorization,$old32Pin,$old32.source_manifest,$old32.core_build_plan,$environment.environment,$environment.verification,$environment.state,$baseAppObject)+(Binding $proofRecipe)+@($libraries.binary)+@($libraries.compile_graph)+$generated+$runtimeSources+@($dependencySelection.files)+@($baseProjectionCopies.Values)+@($baseNative|ForEach-Object {$_.Native8.Values})
foreach($pin in $fixedPins){CheckEntry $pin}
$artifact=DPath ('out/prelive-producer-eligibility-evaluation-'+$Attempt);$buildRoot=DPath ('out/prelive-producer-eligibility-'+$Attempt);$shortRoot=DPath ('out/prelive-esrc-'+$ShortSourceId);$shortFiles=Join-Path $shortRoot 'files'
foreach($path in @($artifact,$buildRoot,$shortRoot)){Fresh $path};$null=[IO.Directory]::CreateDirectory($artifact);$null=[IO.Directory]::CreateDirectory($shortFiles)
$sourceCopies=@();$copyByRelative=@{};$snapshotRows=@();foreach($relative in $tracked){$original=$sourceByRelative[$relative];$path=DPath (Join-Path $shortFiles $relative);DRequire ($path.Length -le 259) 'complete short source max259';$null=[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path));[IO.File]::Copy($original.path,$path,$false);$copy=Binding $path;DRequire ($copy.bytes -eq $original.bytes -and $copy.sha256 -ceq $original.sha256) 'lossless new source copy';$sourceCopies+=@($copy);$copyByRelative[$relative]=$copy;$snapshotRows+=@(@{relative_path=$relative;original=$original;copy=$copy})}
AssertInputs
$manifestPath=Join-Path $artifact 'source-manifest.json';NewJson $manifestPath @{schema='pas.cooperative-source.v1';source_commit=$FrozenHead;dirty=$false;files=$sourcePins;full_current_tracked_source_count=$sourcePins.Count;base_source_commit=$baseCommit;cold_harness_delta=$true;formal_rebuilt=$false;qualification_issued=$false;live_ready=$false;device_operations=0} 4194304
$selectedDependencyPath=Join-Path $artifact 'selected-dependency-manifest.json';NewJson $selectedDependencyPath @{schema='pas.cold-producer-eligibility-dependency-selection.v1';source_commit=$FrozenHead;original_dependency_source_commit=$originalCommit;original_dependency_manifest=$base.dependency_manifest;base_selected_dependency_manifest=$baseSelectionPin;files=$dependencySelection.files;file_count=$dependencySelection.files.Count;fixed_installed_root=$dependencySelection.fixed_installed_root;selection_reasons=$dependencySelection.reasons;include_search_files=$dependencySelection.include_search_files;selection='Preserved CPU55 selection, actual base55 app Release link libraries, declared find-package configuration, fresh TU lexical transitive include search and runtime18; configured actual links must be covered before build';debug_payloads_added=$false;complete_original_preflight_files=$dependencies.files.Count;qualification_issued=$false;cost_qualification=$false;live_ready=$false;device_operations=0} 4194304
$snapshotPath=Join-Path $shortRoot 'snapshot.json';NewJson $snapshotPath @{schema='pas.lossless-diagnostic-source-snapshot.v1';source_commit=$FrozenHead;source_manifest=(Binding $manifestPath);file_count=$sourceCopies.Count;files=$snapshotRows;all_copy_SHAs_verified=$true;originals_rechecked=$true;relative_paths_unchanged=$true;legacy_max_path_characters=259;qualification_issued=$false;live_ready=$false;device_operations=0} 4194304
$libraryCmake=Join-Path $artifact 'library-inputs.cmake';$text='';foreach($row in $libraries){$text+='set(PAS_'+$row.role+'_PATH '+(DCmakeLiteral $row.binary.path)+')'+[Environment]::NewLine;$text+='set(PAS_'+$row.role+'_SHA256 '+(DCmakeLiteral $row.binary.sha256)+')'+[Environment]::NewLine};[IO.File]::WriteAllText($libraryCmake,$text,[Text.UTF8Encoding]::new($false))
$runtimeCmake=Join-Path $artifact 'runtime-inputs.cmake';$text='set(PAS_ELIGIBILITY_RUNTIME_SOURCES'+[Environment]::NewLine;foreach($pin in $runtimeSources){$text+=' '+(DCmakeLiteral $pin.path)+[Environment]::NewLine};$text+=')'+[Environment]::NewLine;[IO.File]::WriteAllText($runtimeCmake,$text,[Text.UTF8Encoding]::new($false))
$targets=@('current_chain','worker_affinity_controls','production_pipeline_cold','score_only_controls','qualification_reader','qualification_reader_controls','candidate_admission_delta','pas','candidate_admission_controls')
$unitSources=@(@{target='current_chain';path='research/prelive_current/chain.cpp'},@{target='worker_affinity_controls';path='research/prelive_current/tests/worker_affinity_tests.cpp'},@{target='production_pipeline_cold';path='research/windows_production_pipeline_v3/main.cpp'},@{target='production_pipeline_cold';path='src/grpc_transport.cpp'},@{target='score_only_controls';path='research/pre_emulator/score_only_controls.cpp'},@{target='score_only_controls';path='src/grpc_transport.cpp'},@{target='qualification_reader';path='research/pre_emulator/qualification_reader/main.cpp'},@{target='qualification_reader_controls';path='research/pre_emulator/qualification_reader/controls.cpp'},@{target='candidate_admission_delta';path='src/candidate_admission.cpp'},@{target='candidate_admission_controls';path='tests/candidate_admission_tests.cpp'})
if($MotionSourceDelta){
 $targets+=@('game_motion_delta','motion_source_regression')
 $unitSources+=@(@{target='game_motion_delta';path='src/game_motion.cpp'},@{target='motion_source_regression';path='tests/game_tests.cpp'},@{target='motion_source_regression';path='tests/cold_scenario_tests.cpp'})
}
if($DenseSourceDelta){$targets+=@('game_delta');$unitSources+=@(@{target='game_delta';path='src/game.cpp'})}
if($FrameMetadataDelta){$targets+=@('frame_metadata_controls');$unitSources+=@(@{target='frame_metadata_controls';path='research/prelive_current/tests/frame_metadata_tests.cpp'})}
$units=@($unitSources|ForEach-Object {@{target=$_.target;relative_source=$_.path;original_source=$sourceByRelative[$_.path];source=$copyByRelative[$_.path]}})
if($MotionSourceDelta){
 foreach($relative in @('tests/game_tests.cpp','tests/cold_scenario_tests.cpp')){DRequire ($sourceByRelative[$relative].bytes -eq $baseMap[$relative].bytes -and $sourceByRelative[$relative].sha256 -ceq $baseMap[$relative].sha256) 'unchanged original motion regression test TU'}
 foreach($name in @('gtest.dll','gtest_main.dll')){DRequire (@($runtimeSources|Where-Object {[IO.Path]::GetFileName($_.path) -ceq $name}).Count -eq 1) 'original runtime18 supplies actual GTest DLL'}
 $runtimePrefix=$sourceByRelative['src/runtime.cpp'];DRequire ($runtimePrefix.bytes -eq $baseMap['src/runtime.cpp'].bytes -and $runtimePrefix.sha256 -ceq $baseMap['src/runtime.cpp'].sha256) 'unchanged original pure runtime source'
 $runtimeContractPath=Join-Path $artifact 'runtime_contract.cpp';[IO.File]::WriteAllText($runtimeContractPath,(ComponentDeltaMotionRuntimeText (DReadText $runtimePrefix)),[Text.UTF8Encoding]::new($false));$runtimeContract=Binding $runtimeContractPath
 $fixedPins+=@($runtimeContract)
 $extraUnits=@($units|Where-Object {$_.target -cin @('game_delta','frame_metadata_controls')})
 $units=@($units|Where-Object {$_.target -cnotin @('game_delta','frame_metadata_controls')})+@(@{target='motion_source_regression';relative_source='generated/runtime_contract.cpp';original_source=$runtimeContract;source=$runtimeContract})+$extraUnits
 $members=@();foreach($role in @('cpu_pas_core','production_pas_core')){$library=@($libraries|Where-Object role -ceq $role);$member=@($library[0].translation_units|Where-Object relative_source -ceq 'src/game_motion.cpp');DRequire ($member.Count -eq 1) 'one original motion archive member per preserved core';$members+=@(@{role=$role;archive=$library[0].binary;member=$member[0].historical_object;source=$member[0].original_source})}
 $fixedPins+=@($members|ForEach-Object {$_.member})
 $motionStatic=@{schema='pas.cold-motion-source-delta.v1';source_commit=$FrozenHead;base_source_commit=$baseCommit;source_path='src/game_motion.cpp';literal_transform='ridge_guarded_gray_short_circuit.v1';base_source=$baseProjectionCopies['src/game_motion.cpp'];current_source=$sourceByRelative['src/game_motion.cpp'];compile_source=$copyByRelative['src/game_motion.cpp'];original_archive_members=$members;consumer_targets=@(ComponentDeltaMotionTargets);regression=@{target='motion_source_regression';test_sources=@($sourceByRelative['tests/game_tests.cpp'],$sourceByRelative['tests/cold_scenario_tests.cpp']);runtime_contract_source=$runtimeContract;runtime_contract_prefix=$runtimePrefix;cases=26;case_names=@(ComponentDeltaMotionCases);filter=(ComponentDeltaMotionFilter)};formal_libraries_rebuilt=$false;fresh_formal_translation_units=1;qualification_issued=$false;cost_qualification=$false;live_ready=$false;device_operations=0}
}
if($DenseSourceDelta){
 $denseMembers=@();foreach($role in @('cpu_pas_core','production_pas_core')){
  $library=@($libraries|Where-Object role -ceq $role);$member=@($library[0].translation_units|Where-Object relative_source -ceq 'src/game.cpp')
  DRequire ($member.Count -eq 1) 'one original game member per core';CheckEntry $member[0].historical_object
  $denseMembers+=@(@{role=$role;archive=$library[0].binary;member=$member[0].historical_object;source=$member[0].original_source})
 }
 $denseStatic=@{schema='pas.cold-dense-source-delta.v1';source_commit=$FrozenHead;base_source_commit=$baseCommit;source_path='src/game.cpp';literal_transform='white_note_decode_pca_short_circuit.v1';base_source=$baseProjectionCopies['src/game.cpp'];current_source=$sourceByRelative['src/game.cpp'];compile_source=$copyByRelative['src/game.cpp'];original_archive_members=$denseMembers;consumer_targets=@(ComponentDeltaDenseTargets);regression=@{target='motion_source_regression';test_sources=@($sourceByRelative['tests/game_tests.cpp'],$sourceByRelative['tests/cold_scenario_tests.cpp']);runtime_contract_source=$runtimeContract;runtime_contract_prefix=$runtimePrefix;cases=12;case_names=@(ComponentDeltaDenseCases);filter=(ComponentDeltaDenseFilter)};formal_libraries_rebuilt=$false;fresh_formal_translation_units=1;qualification_issued=$false;cost_qualification=$false;live_ready=$false;device_operations=0}
 $fixedPins+=@($denseMembers|ForEach-Object {$_.member})+@($denseStatic.base_source)
}
if($FrameMetadataDelta){
 $frameStatic=@{schema='pas.cold-frame-metadata-delta.v1';policy='fixed_metadata_before_epoch.v1';source_commit=$FrozenHead;base_source_commit='3ee69edc85acb6b9b968424374a376f351d0e2dd';base_chain_source=$frameBaseChain;current_chain_source=$sourceByRelative['research/prelive_current/chain.cpp'];helper_source=$sourceByRelative['research/prelive_current/frame_metadata.hpp'];tests_source=$sourceByRelative['research/prelive_current/tests/frame_metadata_tests.cpp'];frame_controls=@{target='frame_metadata_controls';schema='pas.cold-frame-metadata-reuse-controls.v1';assertions=8};source_dirty=$false;qualification_issued=$false;cost_qualification=$false;live_ready=$false;device_operations=0}
}
$freshCount=$units.Count
$planPath=Join-Path $artifact 'core-build-plan.json';$planData=@{schema='pas.cold-producer-eligibility-delta-plan.v1';source_commit=$FrozenHead;source_dirty=$false;base_source_commit=$baseCommit;original_source_commit=$originalCommit;configuration='release';base_cpu_build=$cpuPin;base_production_build=$basePin;base_source_snapshot=$snapshotPin;base_source_projection=$baseProjectionPin;selected_dependency_manifest=(Binding $selectedDependencyPath);source_manifest=(Binding $manifestPath);source_snapshot=(Binding $snapshotPath);short_source_id=$ShortSourceId;compiler_environment=$environmentPin;targets=$targets;translation_unit_count=$freshCount;translation_units=$units;imported_static_libraries=$libraries;base_generated_inputs=$generated;base_manual_source=$manual;base_app_object=$baseAppObject;verification_only_replacement_source='src/candidate_admission.cpp';producer_eligibility_policy=$deltaPolicy;eligibility_controls_count=$eligibilityControlsCount;eligibility_reader_controls_count=$eligibilityReaderControlsCount;verification_controls_count=$verificationControlsCount;original_affinity_controls_count=65;original_producer_placement_controls_count=27;original_reader_controls_count=81;formal_rebuilt=$false;formal_libraries_preserved=$true;old_objects_reused=$true;base_app_object_reused=$true;cold_harness_delta=$true;new_etw_recordings=0;measurement_runs=0;normal3_runs=0;AA12_runs=0;noise18_runs=0;ABBA36_runs=0;qualification_issued=$false;whole_candidate_qualification=$false;cost_qualification=$false;live_ready=$false;device_operations=0}
if($MotionSourceDelta){$planData.motion_source_delta=$motionStatic;$planData.formal_libraries_rebuilt=$false;$planData.fresh_formal_translation_units=1}
if($DenseSourceDelta){$planData.dense_source_delta=$denseStatic;$planData.fresh_formal_translation_units=2}
if($FrameMetadataDelta){$planData.frame_metadata_delta=$frameStatic}
NewJson $planPath $planData 4194304
$fixedPins+=@(Binding $selectedDependencyPath;Binding $manifestPath;Binding $snapshotPath;Binding $planPath;Binding $libraryCmake;Binding $runtimeCmake)
$adapter=Join-Path $repo 'research/main_consolidation/cmake-stage.ps1';$toolchain='C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake';$guard=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$recipeRoot=[IO.Path]::TrimEndingDirectorySeparator((Join-Path $shortFiles 'research/pre_emulator/producer_eligibility_delta'))
$inputs=@($taskBuildScript,$adapter,(Join-Path $repo 'research/main_consolidation/stage.ps1'),(Join-Path $repo 'research/pre_emulator/common.ps1'),$toolchain)+@($sourcePins.path)+@($sourceCopies.path)+@($fixedPins.path)+(GateInputs)+@($guard.guard_source.path,$guard.process_source.path,$guard.cached_identity_controls.path,$guard.identity_wait_controls.path)
$options=@(('-DCMAKE_TOOLCHAIN_FILE='+$toolchain),('-DVCPKG_INSTALLED_DIR='+(Join-Path $repo 'out/vcpkg_installed')),'-DVCPKG_TARGET_TRIPLET=x64-windows','-DVCPKG_MANIFEST_MODE=OFF','-DVCPKG_APPLOCAL_DEPS=OFF','-DCMAKE_CXX_SCAN_FOR_MODULES=OFF','-DCMAKE_HAVE_LIB_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=','-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL',('-DPAS_SOURCE_WORKSPACE='+$shortFiles),('-DPAS_BASE_PRODUCTION_ROOT='+$base.build_root),('-DPAS_DELTA_SOURCE_COMMIT='+$FrozenHead),('-DPAS_LIBRARY_INPUTS_FILE='+$libraryCmake),('-DPAS_RUNTIME_INPUTS_FILE='+$runtimeCmake),('-DPAS_BASE_MANUAL_SHA256='+$manual.sha256),('-DPAS_WORKER_AFFINITY_SHA256='+$sourceByRelative['research/prelive_current/worker_affinity.hpp'].sha256),('-DPAS_PRODUCER_PLACEMENT_SHA256='+$sourceByRelative['research/windows_production_pipeline_v3/producer_placement.hpp'].sha256),('-DPAS_BASE_APP_OBJECT='+$baseAppObject.path),('-DPAS_BASE_APP_OBJECT_SHA256='+$baseAppObject.sha256),('-DPAS_CANDIDATE_ADMISSION_SHA256='+$sourceByRelative['src/candidate_admission.cpp'].sha256),('-DPAS_CANDIDATE_ADMISSION_TESTS_SHA256='+$sourceByRelative['tests/candidate_admission_tests.cpp'].sha256))
if($MotionSourceDelta){$options+=@('-DPAS_MOTION_SOURCE_DELTA=ON',('-DPAS_MOTION_SOURCE_SHA256='+$motionStatic.current_source.sha256),('-DPAS_MOTION_RUNTIME_CONTRACT_SOURCE='+$runtimeContract.path),('-DPAS_MOTION_RUNTIME_CONTRACT_SHA256='+$runtimeContract.sha256),('-DPAS_TEST_SOURCE_ROOT='+$repo))}
if($DenseSourceDelta){$options+=@('-DPAS_DENSE_SOURCE_DELTA=ON',('-DPAS_DENSE_SOURCE_SHA256='+$denseStatic.current_source.sha256))}
if($FrameMetadataDelta){$options+=@('-DPAS_FRAME_METADATA_DELTA=ON',('-DPAS_FRAME_METADATA_HEADER_SHA256='+$frameStatic.helper_source.sha256),('-DPAS_FRAME_METADATA_TESTS_SHA256='+$frameStatic.tests_source.sha256))}
$configure='configure-producer-eligibility-'+$Attempt;AssertInputs
Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=$recipeRoot;BuildRoot=$buildRoot;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentPin.path;Operation='Configure';Configuration='Release';Inputs=$inputs;Options=$options}
$configureProof=DProof $configure
$ninjaPath=Join-Path $buildRoot 'build.ninja';$ninja=(Get-Content -LiteralPath $ninjaPath -Raw).Replace('\','/');$compileRows=@([regex]::Matches($ninja,'(?m)^build ([^\r\n:]+[.]obj): CXX_COMPILER[^ \r\n]+[ \t]+([^|\r\n]+)'))
DRequire ($compileRows.Count -eq $freshCount -and $ninja -notmatch 'applocal[.]ps1' -and $ninja -match '/O2 /Ob2 /DNDEBUG' -and $ninja -match '(?:/|-)MD(?: |\r?\n)' -and $ninja -notmatch 'CXX_STATIC_LIBRARY_LINKER') 'exact declared fresh Release compile occurrences without rebuilt archive/app-local'
foreach($unit in $units){$rows=@($compileRows|Where-Object {$_.Groups[1].Value.StartsWith('CMakeFiles/'+$unit.target+'.dir/') -and [string]::Equals([IO.Path]::GetFullPath($_.Groups[2].Value.Trim().Replace('$:',':').Replace('$ ',' ')),[IO.Path]::GetFullPath($unit.source.path),[StringComparison]::OrdinalIgnoreCase)});DRequire ($rows.Count -eq 1) 'each exact source compile target'}
$exeTargets=@($targets|Where-Object {$_ -cnotin @('candidate_admission_delta','game_motion_delta','game_delta')})
$selectedInstalled=@{};foreach($pin in $dependencySelection.files){$selectedInstalled[[IO.Path]::GetFullPath($pin.path)]=$pin}
foreach($field in [regex]::Matches($ninja,'(?m)^  LINK_LIBRARIES = (.*)$')){foreach($token in [regex]::Matches($field.Groups[1].Value.Replace('$:',':').Replace('$ ',' '),'"[^"]*"|\S+')){$path=$token.Value.Trim('"');if([IO.Path]::IsPathRooted($path)){$key=[IO.Path]::GetFullPath($path);if($key.StartsWith($dependencySelection.fixed_installed_root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){DRequire ($selectedInstalled.ContainsKey($key)) 'configured actual installed linker input belongs to original-pinned selected closure'}}}}
foreach($target in $exeTargets){$row=@([regex]::Matches($ninja,'(?m)^build '+[regex]::Escape($target)+ '[.]exe: CXX_EXECUTABLE_LINKER[^\r\n]*\r?\n(?<body>(?:[ \t]+[^\r\n]*\r?\n)*)'));DRequire ($row.Count -eq 1) 'one effective executable link row';$link=@([regex]::Matches($row[0].Groups['body'].Value,'(?m)^  LINK_LIBRARIES = (.*)$'));DRequire ($link.Count -eq 1) 'actual LINK_LIBRARIES required';$required=if($target -ceq 'current_chain'){@($libraries|Where-Object role -cmatch '^cpu_')}elseif($target -cin @('production_pipeline_cold','score_only_controls')){@($libraries|Where-Object role -cmatch '^production_')}elseif($target -ceq 'motion_source_regression'){@($libraries|Where-Object role -ceq 'production_pas_core')}elseif($target -ceq 'frame_metadata_controls'){@($libraries|Where-Object role -ceq 'cpu_pas_core')}elseif($target -ceq 'pas'){@($libraries|Where-Object {$_.role -cmatch '^(production_|verification_)'})}elseif($target -ceq 'candidate_admission_controls'){@($libraries|Where-Object {$_.role -cmatch '^production_' -or $_.role -ceq 'verification_pas_emulator'})}else{@()};$linkText=$link[0].Groups[1].Value.Replace('$:',':').Replace('$ ',' ');DLinkRoles $linkText $required $libraries $buildRoot
 if($MotionSourceDelta){ComponentDeltaMotionLinkObjects $row[0].Value $target $compileRows $baseAppObject ([bool]$DenseSourceDelta)}
 if($MotionSourceDelta -and ($target -cin @(ComponentDeltaMotionTargets) -or $target -ceq 'motion_source_regression')){$explicit=($row[0].Value -split '\|')[0];DRequire (@([regex]::Matches($explicit,'CMakeFiles/game_motion_delta[.]dir/[^ \r\n]+[.]obj')).Count -eq 1 -and $linkText -notmatch 'game_motion[.]cpp[.]obj') 'one shared motion object explicit before archive arguments'}
 if($target -cin @('pas','candidate_admission_controls')){DRequire ($row[0].Value -match 'CMakeFiles/candidate_admission_delta[.]dir/[^\r\n]+[.]obj') 'new verification object explicitly precedes archive field';if($target -ceq 'pas'){DRequire ($row[0].Value.Contains($baseAppObject.path.Replace('\','/').Replace(':','$:')) -or $row[0].Value.Contains($baseAppObject.path.Replace('\','/'))) 'actual old app object explicit link'};DRequire ($linkText -notmatch 'candidate_admission[.]cpp[.]obj') 'no old candidate object explicitly linked'}
}
$generatedDelta=@(Binding (Join-Path $buildRoot 'producer_eligibility_delta_identity.hpp');Binding (Join-Path $buildRoot 'verification_delta_identity.hpp');Binding (Join-Path $buildRoot 'frozen_manual_session.cpp'))
if($MotionSourceDelta){$generatedDelta+=@($runtimeContract)}
$runtimeCopies=@();foreach($pin in $runtimeSources){$copy=Binding (Join-Path $buildRoot ([IO.Path]::GetFileName($pin.path)));DRequire ($copy.bytes -eq $pin.bytes -and $copy.sha256 -ceq $pin.sha256) 'exact runtime18 copy';$runtimeCopies+=@($copy)}
$inputs+=@($generatedDelta.path)+@($runtimeCopies.path)+(Join-Path $buildRoot 'CMakeCache.txt')+$ninjaPath+@($configureProof.Values.path)
$build='build-producer-eligibility-'+$Attempt;AssertInputs
Stage $build $adapter @{ReceiptRepo=$repo;SourceRoot=$recipeRoot;BuildRoot=$buildRoot;StageName=$build;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentPin.path;Operation='Build';Configuration='Release';Inputs=$inputs;Options=@('--target')+$targets}
$buildProof=DProof $build;AssertInputs
$outputs=@(Get-ChildItem -LiteralPath $buildRoot -File -Recurse|ForEach-Object {Binding $_.FullName});$objects=@($outputs|Where-Object {$_.path -match '[.]obj$' -and $_.path.Replace('\','/') -cmatch '/CMakeFiles/(current_chain|worker_affinity_controls|production_pipeline_cold|score_only_controls|qualification_reader|qualification_reader_controls|candidate_admission_delta|candidate_admission_controls|game_motion_delta|motion_source_regression|game_delta|frame_metadata_controls)[.]dir/'})
DRequire ($objects.Count -eq $freshCount) 'actual declared fresh objects';foreach($pin in $outputs|Where-Object {$_.path -match '[.]obj$' -and $_ -notin $objects}){DRequire ($pin.path.Replace('\','/') -cmatch '/(?:CompilerIdCXX/CMakeCXXCompilerId[.]obj|CMakeFiles/ShowIncludes/main[.]obj)$') 'only standard CMake probe objects additional'}
$exes=@{};foreach($target in $exeTargets){$rows=@($outputs|Where-Object {[IO.Path]::GetFileName($_.path) -ceq ($target+'.exe')});DRequire ($rows.Count -eq 1) 'one fresh executable for each declared link target';$exes[$target]=$rows[0]}
$script:seal=@{files=@($exes.Values)+$runtimeCopies}
# Native controls use the original tests reservation after build outputs are
# sealed. Configure/build retain their original build reservation and gates.
$Operation='tests'
# Compile-only installed archives remain fully frozen in configure/build.
# Executed controls consume the sealed images, runtime copies, sources,
# objects and imported project libraries rather than rereading these archives.
$controlCompileOnlyLargeDependencies=@($dependencySelection.files|Where-Object {$_.bytes -gt 268435456 -and [IO.Path]::GetExtension($_.path) -ieq '.lib'})
$controlCompileOnlyPaths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($pin in $controlCompileOnlyLargeDependencies){$null=$controlCompileOnlyPaths.Add([IO.Path]::GetFullPath($pin.path))}
$controlPolicyPath=Join-Path $artifact 'control-input-policy.json'
NewJson $controlPolicyPath @{schema='pas.cold-controls-input-policy.v1';source_commit=$FrozenHead;selected_dependency_manifest=(Binding $selectedDependencyPath);compile_only_large_installed_archives=$controlCompileOnlyLargeDependencies;configure_and_build_keep_all_selected_dependencies=$true;final_full_dependency_recheck_preserved=$true;control_assertions_use_same_compile_only_exclusion=$true;actual_images_runtime_sources_objects_imported_libraries_preserved=$true;controls_counts_unchanged=$true;qualification_issued=$false;device_operations=0} 4194304
$controlInputs=@($inputs+$outputs.path+$buildProof.Values.path|Where-Object {-not $controlCompileOnlyPaths.Contains([IO.Path]::GetFullPath($_))})+@($controlPolicyPath)
$affinityReport=Join-Path $artifact 'worker-affinity-controls.json';$test='test-producer-eligibility-'+$Attempt+'-affinity';AssertControlInputs
Native $test $exes.worker_affinity_controls.path @($affinityReport) $controlInputs 300
$affinityProof=DProof $test;$affinity=DRead (Binding $affinityReport);DControls $affinity 'pas.worker-affinity-tests.v1' 65
DControls $affinity.eligibility_controls 'pas.cold-producer-eligibility-controls.v1' $eligibilityControlsCount
DEligibilityIdentity $affinity;DRequire ($affinity.producer_eligibility_policy_id -ceq $deltaPolicy) 'actual affinity policy id'
$script:noiseRepairBuild=@{process_qos_required=$true;producer_wait_required=$true;producer_active_tail_ns=0;worker_affinity_required=$true}
$scoreRows=@();foreach($scene in @('invalid','tap','hold','dense')){$root=Join-Path $artifact ('score-only-'+$scene);Fresh $root;$test='test-producer-eligibility-'+$Attempt+'-score-'+$scene;AssertControlInputs
 Native $test $exes.score_only_controls.path @($scene,$root) $controlInputs 300
 $proof=DProof $test;$reportPin=Binding (Join-Path $root 'control-report.json');$report=DRead $reportPin
 DRequire ($report.schema -ceq 'pas.score-only-literal-controls.v1' -and $report.base_component_source_commit -ceq $baseCommit -and $report.actual_manual_translation_unit_sha256 -ceq $manual.sha256) 'literal score-only base55 manual source';DBool $report 'pass' $true;DInt $report 'failed' 0;DInt $report 'device_operations' 0;foreach($flag in @('cost_qualification','physical_qualification','whole_candidate_qualification_issued')){DBool $report $flag $false};DBool $report 'no_gameplay_files_created' $true
 DEligibilityIdentity $report;ComponentDeltaValidateColdProducer $report 'pas.cold-worker-placement.v3'
 $result=DRead $proof.result;DRequire ($report.process_qos.process_id -eq $result.facts.pid) 'actual score QoS bound to its Native PID'
 if($scene -ceq 'invalid'){DInt $report 'invalid_requests_rejected_before_io' 8;DInt $report 'controlled_stop_lifecycle_cases' 2;DInt $report 'result_tail_fake_clock_cases' 4;DRequire ($report.producer_placement_controls.checks -eq 27 -and $report.producer_placement_controls.passed -eq 27 -and $report.producer_placement_controls.failed -eq 0 -and @($report.producer_placement_controls.cases).Count -eq 27) 'original placement27 separate'}
 if($scene -ceq 'invalid'){ComponentDeltaValidateColdProducerControls $report 'pas.cold-worker-placement.v3'}
 $scoreRows+=@(@{target=('score_only_'+$scene);binary=$exes.score_only_controls;report=$reportPin;Native8=$proof})
}
$readerReport=Join-Path $artifact 'qualification-reader-controls.json';$test='test-producer-eligibility-'+$Attempt+'-reader';AssertControlInputs
Native $test $exes.qualification_reader_controls.path @($readerReport) $controlInputs 300
$readerProof=DProof $test;$reader=DRead (Binding $readerReport)
DRequire ($reader.schema -ceq 'pas.qualification-reader-controls.v1' -and @($reader.cases).Count -eq 81) 'original reader81 schema/array';DInt $reader 'cases_count' 81;DInt $reader 'failed' 0;DInt $reader 'device_operations' 0;DBool $reader 'qualification_issued' $false;DBool $reader 'live_ready' $false
$readerIds=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($case in $reader.cases){DRequire ($case.id -is [string] -and $case.id.Length -gt 0 -and $readerIds.Add($case.id)) 'original reader unique id';DBool $case 'pass' $true}
DControls $reader.active_wait_policy_controls 'pas.active-wait-policy-reader-controls.v1' 12
DControls $reader.process_qos_policy_controls 'pas.process-qos-policy-reader-controls.v1' 12
DControls $reader.cold_harness_delta_controls 'pas.cold-harness-delta-reader-controls.v1' $eligibilityReaderControlsCount
if($DenseSourceDelta){DControls $reader.dense_source_delta_controls 'pas.cold-dense-reader-controls.v1' 8}
if($FrameMetadataDelta){DControls $reader.frame_metadata_delta_controls 'pas.cold-frame-metadata-reader-controls.v1' 8}
$verificationReport=Join-Path $artifact 'candidate-verification-controls.json';$test='test-producer-eligibility-'+$Attempt+'-verification';AssertControlInputs
Native $test $exes.candidate_admission_controls.path @($verificationReport) $controlInputs 300
$verificationProof=DProof $test;$verificationReportPin=Binding $verificationReport;$verificationControls=DRead $verificationReportPin
DControls $verificationControls 'pas.candidate-admission-controls.v1' 82
DControls $verificationControls.verification_delta_controls 'pas.cold-candidate-verification-controls.v1' $verificationControlsCount
if($MotionSourceDelta){
 $motionReport=Join-Path $artifact 'motion-source-regression.json';$test='test-producer-eligibility-'+$Attempt+'-motion';AssertControlInputs
 Native $test $exes.motion_source_regression.path @(('--gtest_filter='+(ComponentDeltaMotionFilter)),('--gtest_output=json:'+$motionReport)) $controlInputs 300
 $motionProof=DProof $test;ComponentDeltaMotionGTest (DRead (Binding $motionReport))
 $newMotionObjects=@($objects|Where-Object {$_.path.Replace('\','/').Contains('/CMakeFiles/game_motion_delta.dir/')});DRequire ($newMotionObjects.Count -eq 1) 'one shared actual motion object'
 $motionActual=@{};foreach($key in $motionStatic.Keys){$motionActual[$key]=$motionStatic[$key]};$motionActual.new_object=$newMotionObjects[0];$motionActual.consumer_images=@(ComponentDeltaMotionTargets|ForEach-Object {$exes[$_]})
 $regression=@{};foreach($key in $motionStatic.regression.Keys){$regression[$key]=$motionStatic.regression[$key]};$regression.binary=$exes.motion_source_regression;$regression.process=$motionProof;$regression.report=Binding $motionReport;$motionActual.regression=$regression
}
if($DenseSourceDelta){
 $denseReport=Join-Path $artifact 'dense-source-regression.json';$test='test-producer-eligibility-'+$Attempt+'-dense';AssertControlInputs
 Native $test $exes.motion_source_regression.path @(('--gtest_filter='+(ComponentDeltaDenseFilter)),('--gtest_output=json:'+$denseReport)) $controlInputs 300
 $denseProof=DProof $test;ComponentDeltaDenseGTest (DRead (Binding $denseReport))
 $newGameObjects=@($objects|Where-Object {$_.path.Replace('\','/').Contains('/CMakeFiles/game_delta.dir/')});DRequire ($newGameObjects.Count -eq 1) 'one actual shared game object'
 $denseActual=@{};foreach($key in $denseStatic.Keys){$denseActual[$key]=$denseStatic[$key]};$denseActual.new_object=$newGameObjects[0];$denseActual.consumer_images=@(ComponentDeltaDenseTargets|ForEach-Object {$exes[$_]})
 $denseRegression=@{};foreach($key in $denseStatic.regression.Keys){$denseRegression[$key]=$denseStatic.regression[$key]};$denseRegression.binary=$exes.motion_source_regression;$denseRegression.process=$denseProof;$denseRegression.report=Binding $denseReport;$denseActual.regression=$denseRegression
}
if($FrameMetadataDelta){
 $frameReport=Join-Path $artifact 'frame-metadata-controls.json';$test='test-producer-eligibility-'+$Attempt+'-frame';AssertControlInputs
 Native $test $exes.frame_metadata_controls.path @($frameReport) $controlInputs 300
 $frameProof=DProof $test;$frameReportPin=Binding $frameReport;$frameReportValue=DRead $frameReportPin
 DControls $frameReportValue 'pas.cold-frame-metadata-reuse-controls.v1' 8
 DRequire ($frameReportValue.source_build_commit -ceq $FrozenHead) 'actual fixed metadata controls source';DBool $frameReportValue 'source_build_dirty' $false
 $frameActual=@{};foreach($key in $frameStatic.Keys){$frameActual[$key]=$frameStatic[$key]}
 $frameActual.frame_controls=@{target='frame_metadata_controls';schema='pas.cold-frame-metadata-reuse-controls.v1';assertions=8;binary=$exes.frame_metadata_controls;report=$frameReportPin;process=$frameProof}
}
AssertInputs;foreach($pin in $outputs){CheckEntry $pin}
$newCandidateObjects=@($objects|Where-Object {$_.path.Replace('\','/').Contains('/CMakeFiles/candidate_admission_delta.dir/')} );DRequire ($newCandidateObjects.Count -eq 1) 'one actual new verification object'
$verificationLibraries=@();foreach($name in @('pas_core','pas_current_candidate','pas_proto','pas_emulator','pas_bench')){$role=if($name -cin @('pas_emulator','pas_bench')){'verification_'+$name}else{'production_'+$name};$row=@($libraries|Where-Object role -ceq $role);DRequire ($row.Count -eq 1) 'exact verification link role';$verificationLibraries+=@(@{role=$name;binary=$row[0].binary;compiled_source_commit=$baseCommit;build_binding=$basePin})}
$verificationDeltaPath=Join-Path $artifact 'verification-delta.json';$verificationData=@{schema='pas.cold-candidate-verification-delta.v1';source_commit=$FrozenHead;source_dirty=$false;base_source_commit=$baseCommit;candidate_binary=$exes.pas;candidate_source=$sourceByRelative['src/candidate_admission.cpp'];tests_source=$sourceByRelative['tests/candidate_admission_tests.cpp'];source_manifest=(Binding $manifestPath);source_snapshot=(Binding $snapshotPath);core_build_plan=(Binding $planPath);base_production_build=$basePin;base_source_snapshot=$snapshotPin;base_source_projection=$baseProjectionPin;selected_dependency_manifest=(Binding $selectedDependencyPath);base_generated_provenance=$generated[0];compile_process=$buildProof;link_process=$buildProof;linked_static_libraries=$verificationLibraries;base_app_object=$baseAppObject;new_candidate_object=$newCandidateObjects[0];verification_controls_report=$verificationReportPin;verification_controls_process=$verificationProof;verification_controls_count=$verificationControlsCount;formal_rebuilt=$false;verification_only_delta=$true;qualification_issued=$false;cost_qualification=$false;live_ready=$false;device_operations=0}
if($MotionSourceDelta){$verificationData.motion_source_delta=$motionActual;$verificationData.verification_only_delta=$false;$verificationData.verification_and_motion_delta=$true}
if($DenseSourceDelta){$verificationData.dense_source_delta=$denseActual;$verificationData.verification_and_motion_and_dense_delta=$true}
if($FrameMetadataDelta){$verificationData.frame_metadata_delta=$frameActual}
NewJson $verificationDeltaPath $verificationData 4194304
$nativeProcesses=@($configureProof,$buildProof,$affinityProof,$readerProof,$verificationProof)+@($scoreRows|ForEach-Object {$_.Native8})
if($MotionSourceDelta){$nativeProcesses+=@($motionProof)}
if($DenseSourceDelta){$nativeProcesses+=@($denseProof)}
if($FrameMetadataDelta){$nativeProcesses+=@($frameProof)}
$bindingPath=Join-Path $artifact 'build-binding.json';$bindingData=@{schema='pas.cold-producer-eligibility-delta-build.v1';source_commit=$FrozenHead;source_dirty=$false;base_source_commit=$baseCommit;original_source_commit=$originalCommit;configuration='release';base_cpu_build=$cpuPin;base_production_build=$basePin;base_source_snapshot=$snapshotPin;base_source_projection=$baseProjectionPin;selected_dependency_manifest=(Binding $selectedDependencyPath);source_manifest=(Binding $manifestPath);source_snapshot=(Binding $snapshotPath);dependency_manifest=$base.dependency_manifest;compiler_environment=$environmentPin;core_build_plan=(Binding $planPath);compile_graph=(Binding $ninjaPath);build_root=$buildRoot;targets=$targets;translation_unit_count=$freshCount;translation_units=$units;imported_static_libraries=$libraries;base_generated_inputs=$generated;generated_delta_inputs=$generatedDelta;base_manual_source=$manual;base_app_object=$baseAppObject;base_session_provenance_preserved=$true;runtime18_sources=$runtimeSources;runtime18_copies=$runtimeCopies;files=@($exes.Values)+$runtimeCopies;compiled_outputs=$outputs;configure_process=$configureProof;build_process=$buildProof;affinity_controls_process=$affinityProof;affinity_controls_report=(Binding $affinityReport);score_control_processes=$scoreRows;reader_controls_process=$readerProof;reader_controls_report=(Binding $readerReport);verification_controls_process=$verificationProof;verification_controls_report=$verificationReportPin;verification_delta=(Binding $verificationDeltaPath);verification_controls_count=$verificationControlsCount;eligibility_controls_count=$eligibilityControlsCount;eligibility_reader_controls_count=$eligibilityReaderControlsCount;producer_eligibility_policy=$deltaPolicy;original_affinity_controls_count=65;original_producer_placement_controls_count=27;original_reader_controls_count=81;base_original_build_processes=$baseNative;actual_native_stage_count=$nativeProcesses.Count;actual_native8_pin_count=8*$nativeProcesses.Count;formal_rebuilt=$false;formal_libraries_preserved=$true;old_objects_reused=$true;base_app_object_reused=$true;cold_harness_delta=$true;qualification_issued=$false;whole_candidate_qualification=$false;cost_qualification=$false;physical_qualification=$false;live_ready=$false;device_endpoints=0;device_operations=0;measurement_runs=0;normal3_runs=0;AA12_runs=0;noise18_runs=0;ABBA36_runs=0;new_etw_recordings=0;cost_status='NOT_READY';base_STOP_preserved=$true;scope='Only10 fresh cold harness/controls/reader/verification compile occurrences and explicit base55 app-object relink; actual main32/main55 library roles remain imported historical source identities; no fresh22/32/52 or whole qualification claim.'}
if($MotionSourceDelta){$bindingData.motion_source_delta=$motionActual;$bindingData.formal_libraries_rebuilt=$false;$bindingData.fresh_formal_translation_units=1;$bindingData.scope='14 fresh compile occurrences: 10 cold harness/reader/factory, one exact formal motion object and three regression TUs; original formal archives remain sealed; no whole qualification.'}
if($DenseSourceDelta){$bindingData.dense_source_delta=$denseActual;$bindingData.fresh_formal_translation_units=2}
if($FrameMetadataDelta){$bindingData.frame_metadata_delta=$frameActual}
if($DenseSourceDelta -and $FrameMetadataDelta){$bindingData.scope='16 fresh compile occurrences with explicit motion/game member overlays and fixed metadata controls; original archives/qualification stay historical; necessary current costs remain pending.'}
NewJson $bindingPath $bindingData 4194304
@{binding=(Binding $bindingPath);fresh_translation_units=$freshCount;native_stages=$nativeProcesses.Count;measurement_runs=0;new_etw_recordings=0;qualification_issued=$false;device_operations=0;live_ready=$false}|ConvertTo-Json -Depth 4
