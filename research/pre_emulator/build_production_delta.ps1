param([ValidatePattern('^main[0-9]{2}$')][string]$Attempt='main50',[Parameter(Mandatory)][string]$NoiseRepairBinding)
. (Join-Path $PSScriptRoot 'common.ps1')
$script:maintenancePath=$PSCommandPath
LoadBuilds 'main32' $NoiseRepairBinding
$candidate=$builds[0];$commit=$candidate.source_commit;$Operation='build'
if(-not $candidate.component_cpu_delta -or -not $candidate.formal_rebuilt){throw 'explicit current CPU source/component build required'}
$artifact=Join-Path $repo ('out/prelive-pre-emulator-evaluation-'+$Attempt+'-release')
$root=Join-Path $repo ('out/prelive-pre-emulator-release-'+$Attempt)
Fresh $artifact;Fresh $root;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$environmentBinding=$candidate.compiler_environment.path
$sourceManifest=$candidate.source_manifest
$dependencyManifest=$candidate.dependency_manifest
$manifest=Get-Content -LiteralPath $sourceManifest.path -Raw|ConvertFrom-Json
$dependencies=Get-Content -LiteralPath $dependencyManifest.path -Raw|ConvertFrom-Json
foreach($entry in @($manifest.files)+@($dependencies.files)){CheckEntry $entry}
$corePlan=Join-Path $artifact 'fresh-core-build-plan.json'
$buildGroups=@(@{id='core';targets=@('pas_core')},@{id='action';targets=@('pas_current_candidate')},@{id='emulator';targets=@('pas_emulator')},@{id='bench';targets=@('pas_bench')},@{id='entry';targets=@('pas','candidate_admission_controls','candidate_receipt_collector_controls','production_entry_controls')},@{id='pipeline';targets=@('score_only_controls','production_pipeline_cold','production_pipeline_audit')})
NewJson $corePlan @{schema='pas.fresh-core-build-plan.v1';source_commit=$commit;configuration='release';translation_units=22;old_donor_objects_used=$false;component_cpu_delta=$true;formal_changed_paths=$candidate.formal_changed_paths;compiler_snapshot=(Binding $tool);build_target_groups=$buildGroups;device_operations=0;live_ready=$false;scope='Fresh production binary and affected controls; six bounded build groups retain original held-identity-cap64; no repeat of original75/prod9'}
$adapter=Join-Path $repo 'research/main_consolidation/cmake-stage.ps1'
$toolchain='C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake'
$guard=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$stageInputs=@($inputs)+@($PSCommandPath,$sourceManifest.path,$dependencyManifest.path,$corePlan,$adapter,$toolchain,$environmentBinding)+@($manifest.files.path)+@($dependencies.files.path)+(GateInputs)+@($guard.guard_source.path,$guard.process_source.path,$guard.cached_identity_controls.path,$guard.identity_wait_controls.path)
function AssertProductionSource {
 if((& git -C $repo rev-parse HEAD) -cne $commit -or (& git -C $repo branch --show-current) -cne 'main' -or (& git -C $repo status --porcelain)){throw 'production source must remain exact clean main'}
 foreach($entry in $manifest.files){CheckEntry $entry}
}
function ProductionClosure([string]$stage){
 $pin=Binding (Join-Path $pkg ($stage+'-input-closure-verification.json'))
 $value=Get-Content -LiteralPath $pin.path -Raw|ConvertFrom-Json
 if($value.stage -cne $stage -or $value.status -cne 'COMPLETE' -or $value.pass -ne $true -or $value.before_verified -ne $true -or $value.after_verified -ne $true){throw 'production actual native before/after closure required'}
 return $pin
}
$options=@(('-DCMAKE_TOOLCHAIN_FILE='+$toolchain),('-DVCPKG_INSTALLED_DIR='+(Join-Path $repo 'out/vcpkg_installed')),'-DVCPKG_TARGET_TRIPLET=x64-windows','-DVCPKG_MANIFEST_MODE=OFF','-DCMAKE_CXX_SCAN_FOR_MODULES=OFF','-DCMAKE_HAVE_LIB_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=','-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL')
# Validate only the selected source graph before compiling; actual Native builds and physical member checks still follow.
function Production52Require([bool]$ok,[string]$why){if(-not $ok){throw ('production52 prebuild: '+$why)}}
function Production52Path([string]$p){return [IO.Path]::GetFullPath($p).Replace([char]92,[char]47).ToLowerInvariant()}
function Production52Tokens([string]$text){
 $out=[Collections.Generic.List[string]]::new();$token=''
 for($i=0;$i -lt $text.Length;$i++){
  $c=$text[$i]
  if($c -eq '$'){
   Production52Require ($i+1 -lt $text.Length) 'trailing Ninja escape'
   $i++;Production52Require ($text[$i] -cin @('$',' ',':')) 'unsupported Ninja variable/escape in concrete edge'
   $token+=$text[$i]
  }elseif([char]::IsWhiteSpace($c)){
   if($token.Length){$out.Add($token);$token=''}
  }else{$token+=$c}
 }
 if($token.Length){$out.Add($token)}
 return $out.ToArray()
}
function Production52RootSources([string]$RootText,[string]$Target,[int]$Count){
 $pattern='add_library\('+[regex]::Escape($Target)+'[ \t\r\n]+STATIC[ \t\r\n]+([^)]*)\)'
 $matches=[regex]::Matches($RootText,$pattern)
 Production52Require ($matches.Count -eq 1) ('one literal root declaration '+$Target)
 $files=@($matches[0].Groups[1].Value.Trim() -split '\s+')
 Production52Require ($files.Count -eq $Count -and @($files|Sort-Object -Unique).Count -eq $Count) ('complete unique root sources '+$Target)
 foreach($f in $files){Production52Require ($f -cmatch '^[A-Za-z0-9_/-]+[.]cpp$' -and $f -notmatch '(^|/)\.\.?(/|$)') ('literal safe root source '+$f)}
 return $files
}
function Production52Plan([string]$SourceRepo,[string]$BuildRoot){
 $root=Get-Content -LiteralPath (Join-Path $SourceRepo 'CMakeLists.txt') -Raw
 $plan=@{}
 foreach($r in @(@('pas_core',22),@('pas_current_candidate',8),@('pas_emulator',6),@('pas_bench',5))){
  $plan[$r[0]]=@(Production52RootSources $root $r[0] $r[1]|ForEach-Object {Production52Path (Join-Path $SourceRepo $_)})
 }
 $plan.pas_proto=@('emulator_controller.pb.cc','emulator_controller.grpc.pb.cc')|ForEach-Object {Production52Path (Join-Path $BuildRoot ('generated/'+$_))}
 $exe=@{
  pas=@('apps/pas/main.cpp');
  candidate_admission_controls=@('tests/candidate_admission_tests.cpp');
  candidate_receipt_collector_controls=@('tests/candidate_receipt_collector_tests.cpp');
  production_entry_controls=@('research/windows_production_entry_controls/main.cpp');
  score_only_controls=@('research/pre_emulator/score_only_controls.cpp','src/grpc_transport.cpp');
  production_pipeline_cold=@('research/windows_production_pipeline_v3/main.cpp','src/grpc_transport.cpp');
  production_pipeline_audit=@('research/windows_production_pipeline_audit_v5/main.cpp')
 }
 foreach($target in $exe.Keys){$plan[$target]=@($exe[$target]|ForEach-Object {Production52Path (Join-Path $SourceRepo $_)})}
 Production52Require ((($plan.Values|ForEach-Object {$_}|Measure-Object).Count) -eq 52) 'exact selected52 source occurrences'
 return $plan
}
function Production52AssertGraph([string]$NinjaText,[string]$SourceRepo,[string]$BuildRoot,$SourcePins){
 $plan=Production52Plan $SourceRepo $BuildRoot
 $pins=@{}
 foreach($pin in $SourcePins){$key=Production52Path $pin.path;Production52Require (-not $pins.ContainsKey($key)) 'unique source manifest pin';$pins[$key]=$pin}
 $required=@('CMakeLists.txt','research/windows_production_build/CMakeLists.txt','research/pre_emulator/CMakeLists.txt','proto/emulator_controller.proto')
 foreach($path in $required){Production52Require ($pins.ContainsKey((Production52Path (Join-Path $SourceRepo $path)))) ('bound build source '+$path)}
 foreach($target in $plan.Keys|Where-Object {$_ -cne 'pas_proto'}){
  foreach($source in $plan[$target]){Production52Require ($pins.ContainsKey($source)) ('actual source manifest member '+$source)}
 }
 # This is a concrete Ninja graph check after configure. Generated .cc files need
 # not exist yet. No object/library/executable is required or built by this check.
 $text=$NinjaText.Replace([char]92,[char]47) -replace '\r',''
 $rows=@{}
 $outputOwners=@{}
 foreach($m in [regex]::Matches($text,'(?m)^build ([^\n:]+[.]obj): CXX_COMPILER__([^ ]+)_unscanned_Release ([^\n]+)\n((?:  [^\n]*\n)*)')){
  $target=$m.Groups[2].Value
  if(-not $plan.ContainsKey($target)){continue}
  $output=@(Production52Tokens $m.Groups[1].Value)
  $sources=@(Production52Tokens (($m.Groups[3].Value -split ' \|\|? ',2)[0]))
  Production52Require ($output.Count -eq 1 -and $sources.Count -eq 1) ('one compile output/source '+$target)
  $prefix=if($target -cin @('pas_core','pas_current_candidate','pas_emulator','pas_proto','pas_bench','pas','candidate_admission_controls','candidate_receipt_collector_controls')){'production/'}else{''}
  Production52Require ($output[0] -cmatch ('^'+[regex]::Escape($prefix+'CMakeFiles/'+$target+'.dir/')+'.+[.]obj$') -and $output[0] -notmatch '(^|/)\.\.?(/|$)') ('contained exact selected object owner '+$target)
  Production52Require (-not $outputOwners.ContainsKey($output[0])) 'unique selected object'
  $outputOwners[$output[0]]=$target
  $absolute=if([IO.Path]::IsPathFullyQualified($sources[0])){$sources[0]}else{Join-Path $BuildRoot $sources[0]}
  if(-not $rows.ContainsKey($target)){$rows[$target]=[Collections.Generic.List[object]]::new()}
  $rows[$target].Add(@{output=$output[0];source=(Production52Path $absolute)})
  $flags=$m.Groups[4].Value
  Production52Require ($flags -match '(?m)^  FLAGS = .* -std:c\+\+20(?: |$)' -and $flags -match '(?m)^  FLAGS = .* -MD(?: |$)' -and $flags -match '(?m)^  FLAGS = .* /O2(?: |$)' -and $flags -notmatch '(?m)^  FLAGS = .* -MDd(?: |$)') ('formal C++20 Release MD/O2 '+$target)
 }
 foreach($target in $plan.Keys){
  Production52Require ($rows.ContainsKey($target)) ('target has actual compile edges '+$target)
  $actual=@($rows[$target]|ForEach-Object source|Sort-Object)
  $expected=@($plan[$target]|Sort-Object)
  Production52Require ($actual.Count -eq $expected.Count -and @(Compare-Object $actual $expected -CaseSensitive).Count -eq 0 -and @($actual|Sort-Object -Unique).Count -eq $actual.Count) ('exact target/source/object membership '+$target)
 }
 Production52Require ($outputOwners.Count -eq 52) 'selected52 actual object edges'
 $libraries=@('pas_core','pas_current_candidate','pas_emulator','pas_proto','pas_bench')
 $requiredLinks=@{
  # CMake repeats core/emulator for the direct and transitive pas/bench links.
  # These are linker arguments, distinct from forbidden duplicate archive members.
  pas=@('pas_core','pas_emulator','pas_bench','pas_emulator','pas_proto','pas_current_candidate','pas_core');
  candidate_admission_controls=@('pas_core','pas_current_candidate','pas_emulator','pas_proto');
  candidate_receipt_collector_controls=@('pas_core','pas_current_candidate','pas_emulator','pas_proto');
  production_entry_controls=@('pas_core','pas_current_candidate','pas_emulator','pas_proto');
  score_only_controls=@('pas_core','pas_current_candidate','pas_proto');
  production_pipeline_cold=@('pas_core','pas_current_candidate','pas_proto');
  production_pipeline_audit=@()
 }
 foreach($target in $plan.Keys){
  $library=$target -cin $libraries
  $output=if($library){'production/'+$target+'.lib'}else{if($target -cin @('pas','candidate_admission_controls','candidate_receipt_collector_controls')){'production/'+$target+'.exe'}else{$target+'.exe'}}
  $kind=if($library){'CXX_STATIC_LIBRARY_LINKER'}else{'CXX_EXECUTABLE_LINKER'}
  $matches=[regex]::Matches($text,('(?m)^build '+[regex]::Escape($output)+': '+$kind+'__'+[regex]::Escape($target)+'_Release ([^\n]*)\n((?:  [^\n]*\n)*)'))
  Production52Require ($matches.Count -eq 1) ('exact archive/executable edge '+$target)
  $edge=$matches[0]
  $members=@(Production52Tokens (($edge.Groups[1].Value -split ' \|\|? ',2)[0]))
  $expected=@($rows[$target]|ForEach-Object output)
  Production52Require ($members.Count -eq $expected.Count -and @(Compare-Object ($members|Sort-Object) ($expected|Sort-Object) -CaseSensitive).Count -eq 0 -and @($members|Sort-Object -Unique).Count -eq $members.Count) ('archive/executable exact member ownership '+$target)
  if(-not $library){
   $lm=[regex]::Match($edge.Groups[2].Value,'(?m)^  LINK_LIBRARIES = ([^\n]*)')
   Production52Require $lm.Success ('actual executable linker arguments '+$target)
   $args=@(Production52Tokens $lm.Groups[1].Value)
   $owned=@($args|Where-Object {$_ -cmatch '^production/pas_[A-Za-z_]+[.]lib$'})
   $wanted=@($requiredLinks[$target]|ForEach-Object {'production/'+$_+'.lib'})
   Production52Require ($owned.Count -eq $wanted.Count -and ($wanted.Count -eq 0 -or @(Compare-Object ($owned|Sort-Object) ($wanted|Sort-Object) -CaseSensitive).Count -eq 0)) ('exact actual production static link closure '+$target)
   if($target -ceq 'production_pipeline_audit'){Production52Require (@($args|Where-Object {$_ -match '(?:^|/)z[.]lib$'}).Count -ge 1) 'auditor actual ZLIB link'}
  }
 }
 $collector=Production52Path (Join-Path $SourceRepo 'src/candidate_receipts.cpp')
 Production52Require (@($rows.pas_emulator|Where-Object source -CEQ $collector).Count -eq 1) 'collector TU exists exactly once in actual pas_emulator graph'
 # Source bytes must remain exactly what current source manifest says. Generated
 # files are bound by the original Proto source/dependency generation contract.
 $requiredSourceKeys=@($required|ForEach-Object {Production52Path (Join-Path $SourceRepo $_)})+@($plan.Keys|Where-Object {$_ -cne 'pas_proto'}|ForEach-Object {$plan[$_]})
 foreach($key in $requiredSourceKeys|Sort-Object -Unique){
  $p=$pins[$key];$f=Get-Item -LiteralPath $p.path
  Production52Require ($f.Length -eq $p.bytes -and (Get-FileHash -LiteralPath $p.path -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $p.sha256) ('actual source pin bytes/SHA '+$key)
 }
 return @{schema='pas.production52-prebuild-graph-review.v1';graph_verified=$true;target_objects=52;targets=12;collector_source=$collector;qualification_issued=$false;live_ready=$false;native_launches=0;device_operations=0}
}
$configure='configure-production-delta-'+$Attempt
AssertProductionSource
Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=$PSScriptRoot;BuildRoot=$root;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentBinding;Operation='Configure';Configuration='Release';Inputs=$stageInputs;Options=$options}
$configureReceipt=Binding (Join-Path $out ($configure+'/'+$configure+'-verification.json'))
$configureClosure=ProductionClosure $configure
$ninja=(Get-Content -LiteralPath (Join-Path $root 'build.ninja') -Raw).Replace('\','/')
$productionGraph=Production52AssertGraph $ninja $repo $root $manifest.files
$graphPath=Join-Path $artifact 'prebuild-source-graph.json'
$productionGraph.source_commit=$commit
$productionGraph.configure_receipt=$configureReceipt
$productionGraph.configure_input_closure=$configureClosure
$productionGraph.source_manifest=$sourceManifest
$productionGraph.ninja=(Binding (Join-Path $root 'build.ninja'))
NewJson $graphPath $productionGraph
$stageInputs+=@($graphPath)
$coreRows=@([regex]::Matches($ninja,'(?m)^build (production/CMakeFiles/pas_core[.]dir/[^\r\n:]+[.]obj): CXX_COMPILER'))
if($coreRows.Count -ne 22 -or $ninja -match 'IMPORTED.*pas_core'){throw 'fresh complete production formal22 graph required'}
# Compiler and link children count towards the same original held-identity cap.
# Separate owned transactions retain every actual native/source proof.
$buildReceipts=@();$buildClosures=@()
foreach($group in $buildGroups){
 $stage='build-production-delta-'+$Attempt+'-'+$group.id
 AssertProductionSource
 $priorProducts=@(Get-ChildItem -LiteralPath $root -File -Recurse|Where-Object {[IO.Path]::GetExtension($_.FullName) -cin @('.obj','.lib','.exe') -and $_.FullName.Replace('\','/') -cnotmatch '/(?:CompilerIdCXX/|ShowIncludes/)'}|Select-Object -ExpandProperty FullName)
 $priorEvidencePaths=@($buildReceipts|ForEach-Object {$_.path})+@($buildClosures|ForEach-Object {$_.path})
 Stage $stage $adapter @{ReceiptRepo=$repo;SourceRoot=$PSScriptRoot;BuildRoot=$root;StageName=$stage;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$environmentBinding;Operation='Build';Configuration='Release';Inputs=@($stageInputs+(Join-Path $root 'CMakeCache.txt')+(Join-Path $root 'build.ninja')+$priorEvidencePaths+$priorProducts);Options=@('--target')+$group.targets}
 $buildReceipts+=@(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))
 $buildClosures+=@(ProductionClosure $stage)
}
$files=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object {Binding $_.FullName})
$builtObjects=@($files|Where-Object {[IO.Path]::GetExtension($_.path) -ceq '.obj' -and $_.path.Replace('\','/') -notmatch '/(?:CompilerIdCXX/|ShowIncludes/)'} )
if($builtObjects.Count -ne 52){throw 'exact52 actual fresh production/control TUs required; configured unused suites are not built'}
foreach($role in @(@{name='pas_core';count=22},@{name='pas_current_candidate';count=8},@{name='pas_emulator';count=6},@{name='pas_proto';count=2},@{name='pas_bench';count=5})){
 $members=@($builtObjects|Where-Object {$_.path.Replace('\','/').Contains('/production/CMakeFiles/'+$role.name+'.dir/',[StringComparison]::Ordinal)})
 $archive=[regex]::Match($ninja,'(?m)^build production/'+$role.name+'[.]lib: CXX_STATIC_LIBRARY_LINKER[^\r\n]+').Value
 $graphMembers=@([regex]::Matches($archive,'[^ ]+[.]obj(?= |$)')|ForEach-Object {$_.Value})
 $expectedMembers=@($members|ForEach-Object {[IO.Path]::GetRelativePath($root,$_.path).Replace('\','/')})
 if($members.Count -ne $role.count -or @(Compare-Object ($graphMembers|Sort-Object) ($expectedMembers|Sort-Object)).Count){throw 'fresh production archive/member/object closure'}
}
$manualPin=@($manifest.files|Where-Object {[IO.Path]::GetFullPath($_.path) -ceq [IO.Path]::GetFullPath((Join-Path $repo 'src/manual_session.cpp'))})
if($manualPin.Count -ne 1){throw 'one actual production manual source pin'}
foreach($object in $coreRows){CheckEntry (Binding (Join-Path $root $object.Groups[1].Value))}
foreach($name in @('pas_core','pas_current_candidate','pas_emulator','pas_proto','pas_bench')){CheckEntry (Binding (Join-Path $root ('production/'+$name+'.lib')))}
$seal=@{files=$files};$Operation='tests'
$controlRows=@()
foreach($name in @('candidate_admission_controls','candidate_receipt_collector_controls','production_entry_controls')){
 $binary=@($files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq ($name+'.exe')})
 if($binary.Count -ne 1){throw 'one actual production control executable'}
 $report=Join-Path $artifact ($name+'.json');$test='test-production-delta-'+$Attempt+'-'+$name.Replace('_','-')
 AssertProductionSource
 Native $test $binary[0].path @($report) @($stageInputs+$files.path+$configureReceipt.path+$configureClosure.path+$buildReceipts.path+$buildClosures.path) 900
 $value=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
 if($value.failed -ne 0 -or $value.device_operations -ne 0 -or $value.live_ready -ne $false -or $value.qualification_issued -eq $true){throw 'production affected control failure or qualification'}
 if($name -ceq 'candidate_admission_controls' -and ($value.schema -cne 'pas.candidate-admission-controls.v1' -or $value.assertions -ne 82 -or @($value.checks).Count -ne 82 -or $value.pass -ne $true)){throw 'all82 affected candidate receipt/admission controls required'}
 if($name -ceq 'candidate_admission_controls'){
  if($value.synthetic_fixture_only -isnot [bool] -or -not $value.synthetic_fixture_only -or $value.qualification_issued -isnot [bool] -or $value.qualification_issued -or $value.gameplay_files_created -ne 0){throw 'synthetic admission controls cannot issue qualification or gameplay files'}
  foreach($check in $value.checks){if($check.pass -isnot [bool] -or -not $check.pass){throw 'every admission control must pass'}}
 }
 if($name -ceq 'candidate_receipt_collector_controls'){
  if($value.schema -cne 'pas.candidate-receipt-collector-controls.v1' -or $value.assertions -ne 85 -or $value.pass -isnot [bool] -or -not $value.pass -or $value.qualification_issued -isnot [bool] -or $value.qualification_issued -or @($value.checks).Count -ne 85 -or $value.synthetic_fixture_only -isnot [bool] -or -not $value.synthetic_fixture_only -or $value.device_endpoints -ne 0 -or $value.gameplay_files_created -ne 0){throw 'all85 bounded synthetic collector controls without qualification required'}
  $ids=@();foreach($check in $value.checks){if($check.pass -isnot [bool] -or -not $check.pass -or $check.id -in $ids){throw 'every collector control must pass with a unique id'};$ids+=@($check.id)}
 }
 if($name -ceq 'production_entry_controls' -and ($value.schema -cne 'pas.production-entry-closed-controls.v1' -or $value.assertions -ne 13 -or -not $value.actual_production_function_called)){throw 'actual production closed entry13 required'}
 if($name -ceq 'production_entry_controls'){
  if(@($value.checks).Count -ne 13){throw 'all13 actual production closed-entry cases'}
  for($index=0;$index -lt 13;$index++){$field=if($index -lt 9){'actual_production_entry_rejects_UNQUALIFIED'}else{'pass'};if($value.checks[$index].$field -isnot [bool] -or -not $value.checks[$index].$field){throw 'every actual production closed-entry/geometry control must pass'}}
 }
 $controlRows+=@(@{target=$name;binary=$binary[0];report=(Binding $report);receipt=(Binding (Join-Path $out ($test+'/'+$test+'-verification.json')));input_closure=(ProductionClosure $test)})
}
$score=@($files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'score_only_controls.exe'})
if($score.Count -ne 1){throw 'one actual score-only control executable'}
foreach($mode in @('invalid','tap','hold','dense')){
 $fixture=Join-Path $artifact ('score-only-'+$mode);$test='test-production-delta-'+$Attempt+'-score-only-'+$mode
 AssertProductionSource
 Native $test $score[0].path @($mode,$fixture) @($stageInputs+$files.path+$configureReceipt.path+$configureClosure.path+$buildReceipts.path+$buildClosures.path) 900
 $report=Join-Path $fixture 'control-report.json';$value=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
 if($value.schema -cne 'pas.score-only-literal-controls.v1' -or $value.pass -ne $true -or $value.failed -ne 0 -or $value.device_operations -ne 0 -or $value.live_ready -ne $false -or $value.cost_qualification -ne $false -or $value.no_gameplay_files_created -ne $true){throw 'score-only literal lifecycle/None/Stop control failure'}
 if($value.base_component_source_commit -cne $commit -or $value.base_component_source_dirty -ne $false -or $value.actual_manual_translation_unit_sha256 -cne $manualPin[0].sha256 -or $value.whole_candidate_qualification_issued -ne $false -or $value.physical_qualification -ne $false){throw 'score-only current compiled source/manual identity and scope'}
 if($mode -ceq 'invalid'){
  if($value.invalid_requests_rejected_before_io -ne 8 -or $value.controlled_stop_lifecycle_cases -ne 2 -or $value.result_tail_fake_clock_cases -ne 4){throw 'all affected pre-I/O/Stop/result-tail fake-clock controls'}
 }else{
  if($value.scene -cne $mode -or -not $value.literal_production_source_executed -or -not $value.all_downs_terminalized -or $value.contacts_final -ne 0 -or $value.down -ne $value.up+$value.contacts_released_by_cancel -or $value.release_calls -le 0 -or ($mode -cne 'dense' -and $value.down -le 0)){throw 'actual score-only complete contact balance/release/scene coverage'}
 }
 $controlRows+=@(@{target=('score_only_'+$mode);binary=$score[0];report=(Binding $report);receipt=(Binding (Join-Path $out ($test+'/'+$test+'-verification.json')));input_closure=(ProductionClosure $test)})
}
AssertProductionSource;foreach($entry in $files){CheckEntry $entry}
$binding=Join-Path $artifact 'build-binding.json'
NewJson $binding @{schema='pas.cooperative-build.v1';source_commit=$commit;configuration='release';source_manifest=$sourceManifest;dependency_manifest=$dependencyManifest;core_build_plan=(Binding $corePlan);build_root=$root;files=$files;configure_receipt=$configureReceipt;configure_input_closure=$configureClosure;build_receipts=$buildReceipts;build_input_closures=$buildClosures;build_target_groups=$buildGroups;controls=$controlRows;component_cpu_delta=$true;formal_rebuilt=$true;formal_changed_paths=$candidate.formal_changed_paths;old_configuration_prerequisite_only=$true;noise_repair_build=(Binding ([IO.Path]::GetFullPath($NoiseRepairBinding)));whole_candidate_qualification=$false;qualification_issued=$false;cost_ready=$false;device_operations=0;live_ready=$false}
Write-Output $binding
