# Explicit CPU component repair branch. Original qualification is preserved,
# never converted into qualification of the rebuilt formal/action components.
function ComponentDeltaTargets { return @('current_chain','consumer_priority_controls','worker_affinity_controls','producer_wait_tests','pas_core','own_bridge','owner_checkpoint_tests','owner_expiry_tests','component_mask_tests','uniform_fnv_tests') }
function ComponentDeltaExecutables { return @(ComponentDeltaTargets|Where-Object {$_ -notin @('pas_core','own_bridge')}) }
function ComponentDeltaUnits {
 $core=@('core','config','analysis','adb','journal','touch_evidence','preview','game','game_clip_replay','game_cold_pipeline','game_cold_corpus','game_motion','game_tracking','game_tracking_methods','game_tracking_analysis','game_tracking_shadow','game_dataset','game_dataset_tools','game_session','session_archive','session_pixel_clips','session_recording')
 return @($core|ForEach-Object {'src/'+$_+'.cpp'})+@('research/prelive_current/own_bridge.cpp','research/bvi_owner_cooperative/extract.cpp','research/prelive_current/chain.cpp','research/prelive_current/tests/consumer_priority_tests.cpp','research/prelive_current/tests/worker_affinity_tests.cpp','research/prelive_current/tests/producer_wait_tests.cpp','research/prelive_current/tests/owner_checkpoint_tests.cpp','research/prelive_current/tests/owner_expiry_tests.cpp','research/prelive_current/tests/component_mask_tests.cpp','research/bvi_owner_cooperative/cold_verification/main.cpp')
}
function ComponentDeltaAllowed([string]$path) {
 return $path -cin @('src/game.cpp','src/manual_session.cpp','src/candidate_admission.cpp','src/candidate_receipts.cpp','include/pas/candidate_receipts.hpp','include/pas/candidate_admission.hpp','apps/pas/main.cpp','tests/candidate_admission_tests.cpp','tests/candidate_receipt_collector_tests.cpp','tests/score_session_tests.cpp','CMakeLists.txt','research/bvi_owner_cooperative/extract.cpp','research/bvi_owner_cooperative/cold_verification/main.cpp','research/bvi_owner_cooperative/cold_verification/observation_equal.hpp','research/bvi_owner_cooperative/cold_verification/CMakeLists.txt','research/bvi_owner_cooperative/cold_verification/run.ps1','research/prelive_current/tests/owner_checkpoint_tests.cpp','research/prelive_current/tests/component_mask_tests.cpp','research/pre_emulator/component_delta.ps1','research/windows_production_build/CMakeLists.txt') -or $path -cmatch '^research/pre_emulator/qualification_reader/reader/component_delta[.]hpp$'
}
function ComponentDeltaFormalPaths($changed) {return @($changed|Where-Object {$_ -cmatch '^(?:src/|include/|apps/pas/)' -or $_ -ceq 'CMakeLists.txt'}|Sort-Object -Unique)}
function ComponentDeltaControls {
 return @(@{target='owner_checkpoint_tests';schema='pas.owner-cooperative-checkpoint-tests.v1';assertions=36},@{target='owner_expiry_tests';schema='pas.owner-expiry-regression.v1';assertions=26},@{target='component_mask_tests';schema='pas.component-mask-tests.v1';assertions=359},@{target='uniform_fnv_tests';schema='pas.uniform-rgb-fnv-controls.v1';assertions=19})
}
function ComponentDeltaAssertControl($value,$item) {
 if($value.schema -cne $item.schema -or $value.assertions -ne $item.assertions -or @($value.checks).Count -ne $item.assertions -or $value.failed -ne 0 -or @($value.checks|Where-Object {$_.pass -ne $true}).Count -or $value.device_endpoints -ne 0 -or $value.live_ready -eq $true -or $value.cost_qualification -eq $true -or $value.device_operations -gt 0){throw ('CPU repair semantic controls failed '+$item.target)}
 if($value.assertions -isnot [int] -and $value.assertions -isnot [long]){throw 'integer control count required'}
 foreach($check in $value.checks){if($check.pass -isnot [bool] -or -not $check.pass){throw 'typed true semantic checks required'}}
 if($item.target -cin @('component_mask_tests','uniform_fnv_tests')){
  foreach($field in @('pass','cost_qualification','live_ready')){if($value.$field -isnot [bool] -or $value.$field -ne ($field -ceq 'pass')){throw 'explicit typed CPU control scope/pass'}}
  if(($value.device_operations -isnot [int] -and $value.device_operations -isnot [long]) -or $value.device_operations -ne 0){throw 'explicit zero CPU control device operations'}
 }
 if($item.target -ceq 'uniform_fnv_tests' -and ($value.table_bytes -ne 524288 -or $value.frame_cache_bytes -ne 0 -or $value.uniform_state_comparisons -ne 262144)){throw 'bounded exact FNV control coverage'}
 if($item.target -ceq 'component_mask_tests' -and ($value.dark_block_sample_count -ne 8 -or $value.dark_block_source_bytes -ne 48 -or $value.dark_block_max_byte -ne 165 -or $value.SSE2_fast_path -isnot [bool] -or -not $value.SSE2_fast_path)){throw 'exact conservative Windows SSE2 component mask policy'}
}
function ComponentDeltaControlSources([string]$target) {
 if($target -ceq 'uniform_fnv_tests'){return @('research/bvi_owner_cooperative/cold_verification/main.cpp','research/bvi_owner_cooperative/cold_verification/observation_equal.hpp','research/bvi_owner_cooperative/extract.cpp','research/bvi_owner_cooperative/extract.hpp')}
 if($target -ceq 'component_mask_tests'){return @('research/prelive_current/tests/component_mask_tests.cpp','src/game.cpp','include/pas/game.hpp')}
 return @(('research/prelive_current/tests/'+$target+'.cpp'),'research/prelive_current/tests/owner_checkpoint_tests.cpp','research/prelive_current/own_bridge.cpp','research/prelive_current/owner_checkpoint.hpp','research/bvi_owner_cooperative/extract.cpp','research/bvi_owner_cooperative/extract.hpp')
}
function ComponentDeltaAssertGraph([string]$buildRoot,[string]$workspace,$expectedSources) {
 $ninja=(Get-Content -LiteralPath (Join-Path $buildRoot 'build.ninja') -Raw).Replace('\','/').Replace('$:',':').Replace('$ ',' ')
 $compileRows=@([regex]::Matches($ninja,'(?m)^build ([^\r\n:]+[.]obj): CXX_COMPILER[^ ]+ ([^\r\n]+?) \|\|'))
 if($compileRows.Count -ne @($expectedSources).Count){throw 'CPU delta exact fresh translation-unit denominator'}
 $actual=@($compileRows|ForEach-Object {[IO.Path]::GetRelativePath($workspace,$_.Groups[2].Value).Replace('\','/')})
 if(@(Compare-Object (@($expectedSources)|Sort-Object) ($actual|Sort-Object)).Count -or @($actual|Sort-Object -Unique).Count -ne $actual.Count){throw 'CPU delta exact source/object graph'}
 $exeSources=@{current_chain='research/prelive_current/chain.cpp';consumer_priority_controls='research/prelive_current/tests/consumer_priority_tests.cpp';worker_affinity_controls='research/prelive_current/tests/worker_affinity_tests.cpp';producer_wait_tests='research/prelive_current/tests/producer_wait_tests.cpp';owner_checkpoint_tests='research/prelive_current/tests/owner_checkpoint_tests.cpp';owner_expiry_tests='research/prelive_current/tests/owner_expiry_tests.cpp';component_mask_tests='research/prelive_current/tests/component_mask_tests.cpp';uniform_fnv_tests='research/bvi_owner_cooperative/cold_verification/main.cpp'}
 foreach($target in $exeSources.Keys){
  $rows=@($compileRows|Where-Object {$_.Groups[1].Value.StartsWith('CMakeFiles/'+$target+'.dir/',[StringComparison]::Ordinal)})
  if($rows.Count -ne 1 -or [IO.Path]::GetRelativePath($workspace,$rows[0].Groups[2].Value).Replace('\','/') -cne $exeSources[$target]){throw 'each CPU executable must compile its exact sole source'}
 }
 if(([regex]::Matches($ninja,'(?m)^build [^\r\n:]+[.]lib: CXX_STATIC_LIBRARY_LINKER')).Count -ne 2 -or ([regex]::Matches($ninja,'(?m)^build [^\r\n:]+[.]exe: CXX_EXECUTABLE_LINKER')).Count -ne 8){throw 'exact two CPU archives/eight executable link targets'}
 foreach($role in @(@{name='pas_core';count=22},@{name='own_bridge';count=2})){
  $archive=[regex]::Match($ninja,'(?m)^build '+$role.name+'[.]lib: CXX_STATIC_LIBRARY_LINKER[^\r\n]+').Value
  if(-not $archive -or ([regex]::Matches($archive,'[.]obj(?: |$)')).Count -ne $role.count){throw ('CPU delta exact fresh archive members '+$role.name)}
  $members=@([regex]::Matches($archive,'[^ ]+[.]obj(?= |$)')|ForEach-Object {$_.Value})
  $compiled=@($compileRows|Where-Object {$_.Groups[1].Value.StartsWith('CMakeFiles/'+$role.name+'.dir/',[StringComparison]::Ordinal)}|ForEach-Object {$_.Groups[1].Value})
  if($compiled.Count -ne $role.count -or @(Compare-Object ($members|Sort-Object) ($compiled|Sort-Object)).Count){throw 'archive must contain exactly its declared fresh component objects'}
 }
 if($ninja -match '/prelive-pre-emulator-release-main32/(?:cold/own_bridge|production/pas_core)[.]lib'){throw 'original rebuilt-role library must never be linked'}
 foreach($target in @('current_chain','owner_checkpoint_tests','owner_expiry_tests')){
  $link=[regex]::Match($ninja,'(?m)^build '+$target+'[.]exe: CXX_EXECUTABLE_LINKER[^\r\n]+').Value
  foreach($name in @('own_bridge.lib','pas_core.lib')){if($link -notmatch ('(?: |\|)'+[regex]::Escape($name)+'(?: |$)')){throw ('fresh CPU role missing from '+$target+' link '+$name)}}
  $block=[regex]::Match($ninja,'(?ms)^build '+$target+'[.]exe: CXX_EXECUTABLE_LINKER.*?(?=^\s*$)').Value
  $arguments=[regex]::Match($block,'(?m)^  LINK_LIBRARIES = ([^\r\n]+)').Groups[1].Value
  $tokens=@([regex]::Matches($arguments,'"[^"]+"|[^ ]+')|ForEach-Object {$_.Value.Trim('"')})
  foreach($name in @('own_bridge','pas_core','current_rails','current_front','exact_bvi_v3')){
   $roleArgs=@($tokens|Where-Object {[IO.Path]::GetFileName($_) -eq ($name+'.lib')})
   $expected=if($name -cin @('own_bridge','pas_core')){$name+'.lib'}else{(Join-Path $workspace ('out/prelive-pre-emulator-release-main32/cold/'+$name+'.lib')).Replace('\','/')}
   if($roleArgs.Count -ne 1 -or $roleArgs[0] -cne $expected){throw ('actual linker arguments require one exact path for '+$name)}
  }
 }
 if($ninja -notmatch '/O2 /Ob2 /DNDEBUG' -or $ninja -notmatch '(?:/|-)MD(?: |\r?\n)'){throw 'CPU delta original Release/MD flags'}
 return @($compileRows|ForEach-Object {$_.Groups[1].Value})
}
function ComponentDeltaVerifyBindings($delta,$plan,$baseFiles) {
 foreach($value in @($delta,$plan)){foreach($field in @('component_cpu_delta','formal_rebuilt')){if($value.$field -isnot [bool] -or $value.$field -ne $true){throw 'typed true CPU component declarations'}}}
 if(($plan.translation_units -isnot [int] -and $plan.translation_units -isnot [long]) -or @($delta.base_static_inputs).Count -ne 5){throw 'typed fresh TU count and exact preserved five-role prerequisite'}
 if($delta.component_cpu_delta -ne $true -or $delta.formal_rebuilt -ne $true -or $plan.component_cpu_delta -ne $true -or $plan.formal_rebuilt -ne $true -or $plan.translation_units -ne 32 -or @($plan.delta_targets).Count -ne 10 -or @(Compare-Object (@($plan.delta_targets)|Sort-Object) (ComponentDeltaTargets|Sort-Object)).Count -or @(Compare-Object (@($plan.translation_unit_sources)|Sort-Object) (ComponentDeltaUnits|Sort-Object)).Count){throw 'exact declared CPU repair branch'}
 if(@($delta.rebuilt_static_inputs).Count -ne 2 -or @($delta.linked_static_inputs).Count -ne 5 -or @($plan.rebuilt_static_targets).Count -ne 2 -or @(Compare-Object (@($plan.rebuilt_static_targets)|Sort-Object) @('own_bridge','pas_core')).Count){throw 'exact two rebuilt/five effective static roles'}
 if(@(Compare-Object (@($delta.formal_changed_paths)|Sort-Object) (ComponentDeltaFormalPaths $delta.changed_paths)).Count -or @(Compare-Object (@($plan.formal_changed_paths)|Sort-Object) (@($delta.formal_changed_paths)|Sort-Object)).Count){throw 'explicit formal source changes required'}
 foreach($name in @('current_rails','current_front','exact_bvi_v3','own_bridge','pas_core')){
  $rows=@($delta.linked_static_inputs|Where-Object {[IO.Path]::GetFileName($_.path) -ceq ($name+'.lib')})
  if($rows.Count -ne 1){throw 'one effective static input per logical role'}
  CheckEntry $rows[0]
  if($name -cin @('own_bridge','pas_core')){
   $fresh=[IO.Path]::GetFullPath((Join-Path $delta.build_root ($name+'.lib')))
   if([IO.Path]::GetFullPath($rows[0].path) -cne $fresh){throw 'rebuilt role must use fresh candidate library path'}
   foreach($set in @('rebuilt_static_inputs','compiled_outputs')){
    $pins=@($delta.$set|Where-Object {[IO.Path]::GetFullPath($_.path) -ceq $fresh})
    if($pins.Count -ne 1 -or $pins[0].sha256 -cne $rows[0].sha256 -or $pins[0].bytes -ne $rows[0].bytes){throw 'fresh library output/effective/rebuilt joins'}
   }
  }else{
   $key=[IO.Path]::GetFullPath($rows[0].path)
   if(-not $baseFiles.ContainsKey($key) -or $baseFiles[$key].sha256 -cne $rows[0].sha256 -or $baseFiles[$key].bytes -ne $rows[0].bytes){throw 'three exact preserved effective static roles'}
  }
 }
 $objects=ComponentDeltaAssertGraph $delta.build_root $repo $plan.translation_unit_sources
 foreach($relative in @($objects)+@('build.ninja','CMakeCache.txt')){
  $path=[IO.Path]::GetFullPath((Join-Path $delta.build_root $relative))
  $pins=@($delta.compiled_outputs|Where-Object {[IO.Path]::GetFullPath($_.path) -ceq $path})
  if($pins.Count -ne 1){throw 'every concrete CPU compile graph/object must be sealed'}
  CheckEntry $pins[0]
 }
}
