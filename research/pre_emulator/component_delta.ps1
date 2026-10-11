# Explicit CPU component repair branch. Original qualification is preserved,
# never converted into qualification of the rebuilt formal/action components.
if(-not (Get-Variable -Name noiseRepairBuild -Scope Script -ErrorAction SilentlyContinue)){$script:noiseRepairBuild=$null}
function ComponentDeltaTargets { return @('current_chain','consumer_priority_controls','worker_affinity_controls','producer_wait_tests','pas_core','own_bridge','owner_checkpoint_tests','owner_expiry_tests','component_mask_tests','uniform_fnv_tests') }
function ComponentDeltaExecutables { return @(ComponentDeltaTargets|Where-Object {$_ -notin @('pas_core','own_bridge')}) }
function ComponentDeltaUnits {
 $core=@('core','config','analysis','adb','journal','touch_evidence','preview','game','game_clip_replay','game_cold_pipeline','game_cold_corpus','game_motion','game_tracking','game_tracking_methods','game_tracking_analysis','game_tracking_shadow','game_dataset','game_dataset_tools','game_session','session_archive','session_pixel_clips','session_recording')
 return @($core|ForEach-Object {'src/'+$_+'.cpp'})+@('research/prelive_current/own_bridge.cpp','research/bvi_owner_cooperative/extract.cpp','research/prelive_current/chain.cpp','research/prelive_current/tests/consumer_priority_tests.cpp','research/prelive_current/tests/worker_affinity_tests.cpp','research/prelive_current/tests/producer_wait_tests.cpp','research/prelive_current/tests/owner_checkpoint_tests.cpp','research/prelive_current/tests/owner_expiry_tests.cpp','research/prelive_current/tests/component_mask_tests.cpp','research/bvi_owner_cooperative/cold_verification/main.cpp')
}
function ComponentDeltaAllowed([string]$path) {
 if($path -cin @('research/windows_production_pipeline_v3/main.cpp','research/windows_production_pipeline_v3/producer_placement.hpp','research/pre_emulator/score_only_controls.cpp')){return $true}
 return $path -cin @('src/game.cpp','src/manual_session.cpp','src/candidate_admission.cpp','src/candidate_receipts.cpp','include/pas/candidate_receipts.hpp','include/pas/candidate_admission.hpp','apps/pas/main.cpp','tests/candidate_admission_tests.cpp','tests/candidate_receipt_collector_tests.cpp','tests/score_session_tests.cpp','CMakeLists.txt','research/bvi_owner_cooperative/extract.cpp','research/bvi_owner_cooperative/cold_verification/main.cpp','research/bvi_owner_cooperative/cold_verification/observation_equal.hpp','research/bvi_owner_cooperative/cold_verification/CMakeLists.txt','research/bvi_owner_cooperative/cold_verification/run.ps1','research/prelive_current/tests/owner_checkpoint_tests.cpp','research/prelive_current/tests/component_mask_tests.cpp','research/pre_emulator/component_delta.ps1','research/windows_production_build/CMakeLists.txt') -or $path -cmatch '^research/pre_emulator/qualification_reader/reader/component_delta[.]hpp$'
}
function script:ComponentDeltaColdProducerRequire([bool]$ok,[string]$why) {
 if(-not $ok){throw ('cold producer metadata: '+$why)}
}
function script:ComponentDeltaColdProducerField($value,[string]$name) {
 ComponentDeltaColdProducerRequire ($null -ne $value) ('missing object for '+$name)
 if($value -is [Collections.IDictionary]){
  ComponentDeltaColdProducerRequire (@($value.Keys) -ccontains $name) ('missing '+$name)
  $field=$value[$name]
 }else{
  $properties=@($value.PSObject.Properties|Where-Object Name -CEQ $name)
  ComponentDeltaColdProducerRequire ($properties.Count -eq 1) ('missing '+$name)
  $field=$properties[0].Value
 }
 ComponentDeltaColdProducerRequire ($null -ne $field) ('null '+$name)
 return ,$field
}
function script:ComponentDeltaColdProducerBool($value,[string]$name,[bool]$expected) {
 $field=ComponentDeltaColdProducerField $value $name
 ComponentDeltaColdProducerRequire ($field -is [bool] -and $field -eq $expected) ('typed '+$name+' must equal '+$expected)
}
function script:ComponentDeltaColdProducerInteger($value,[string]$name,[bigint]$minimum,[bigint]$maximum) {
 $field=ComponentDeltaColdProducerField $value $name
 $integer=$field -is [byte] -or $field -is [sbyte] -or $field -is [int16] -or $field -is [uint16] -or $field -is [int32] -or $field -is [uint32] -or $field -is [int64] -or $field -is [uint64] -or $field -is [bigint]
 ComponentDeltaColdProducerRequire $integer ('typed integer '+$name)
 ComponentDeltaColdProducerRequire ([bigint]$field -ge $minimum -and [bigint]$field -le $maximum) ('integer '+$name+' outside declared bounds')
 return $field
}
function script:ComponentDeltaColdProducerText($value,[string]$name,[string]$expected) {
 $field=ComponentDeltaColdProducerField $value $name
 ComponentDeltaColdProducerRequire ($field -is [string] -and $field -ceq $expected) ('exact '+$name)
}
function script:ComponentDeltaColdProducerMask($value,[string]$name) {
 return [uint64](ComponentDeltaColdProducerInteger $value $name 1 ([bigint]::Parse('18446744073709551615')))
}
function script:ComponentDeltaValidateColdProducerWait($report) {
 if($report.PSObject.Properties.Name -ccontains 'producer_wait_policy' -or ($report -is [System.Collections.IDictionary] -and $report.Contains('producer_wait_policy'))){
  ComponentDeltaColdProducerText $report 'producer_wait_policy' 'full_period_active_qpc'
  $null=ComponentDeltaColdProducerInteger $report 'producer_active_tail_ns' 0 0
  $null=ComponentDeltaColdProducerInteger $report 'producer_active_wait_horizon_ns' 100000000 100000000
  $null=ComponentDeltaColdProducerInteger $report 'producer_active_wait_poll_cap' 16777216 16777216
  ComponentDeltaColdProducerBool $report 'producer_no_kernel_wait' $true
 }else{$null=ComponentDeltaColdProducerInteger $report 'producer_active_tail_ns' 2000000 2000000}
}
function script:ComponentDeltaValidateProcessQos($snapshot) {
 ComponentDeltaColdProducerText $snapshot 'schema' 'pas.cold-process-qos.v1'
 ComponentDeltaColdProducerText $snapshot 'policy' 'own_cold_process_execution_speed_high_qos'
 foreach($pair in @(@('version',1),@('execution_speed_mask',1),@('supported_mask',5),@('process_class_before',32),@('process_class_after_close',32),@('device_operations',0))){$null=ComponentDeltaColdProducerInteger $snapshot $pair[0] $pair[1] $pair[1]}
 foreach($name in @('constructor_attempted','previous_available','requested_available','set_attempted','set_succeeded','applied_available','applied','close_attempted','restore_set_attempted','restore_set_succeeded','restored_available','restored','verified')){ComponentDeltaColdProducerBool $snapshot $name $true}
 foreach($name in @('rollback_attempted','rollback_available','rollback_restored','fallback_attempted','fallback_available','fallback_restored','cost_qualification','live_ready')){ComponentDeltaColdProducerBool $snapshot $name $false}
 foreach($name in @('failure','rollback_failure','fallback_failure')){ComponentDeltaColdProducerText $snapshot $name 'none'}
 $pidBefore=ComponentDeltaColdProducerInteger $snapshot 'process_id' 1 4294967295
 $threadBefore=ComponentDeltaColdProducerInteger $snapshot 'owner_thread_id' 1 4294967295
 ComponentDeltaColdProducerRequire ((ComponentDeltaColdProducerInteger $snapshot 'close_process_id' 1 4294967295) -eq $pidBefore -and (ComponentDeltaColdProducerInteger $snapshot 'close_thread_id' 1 4294967295) -eq $threadBefore) 'QoS own process and same main-thread explicit close'
 $states=@{}
 foreach($name in @('previous','requested','applied_readback','restored_readback')){
  $state=ComponentDeltaColdProducerField $snapshot $name
  $null=ComponentDeltaColdProducerInteger $state 'version' 1 1
  $control=[uint32](ComponentDeltaColdProducerInteger $state 'control_mask' 0 5)
  $stateMask=[uint32](ComponentDeltaColdProducerInteger $state 'state_mask' 0 5)
  ComponentDeltaColdProducerRequire (($control -band 2) -eq 0 -and ($stateMask -band 2) -eq 0) 'QoS only documented execution-speed/timer-resolution bits'
  $states[$name]=@{control=$control;state=$stateMask}
 }
 ComponentDeltaColdProducerRequire ($states.requested.control -eq ($states.previous.control -bor 1) -and $states.requested.state -eq ($states.previous.state -band 4)) 'QoS only execution-speed bit changes; all other supported bits preserved'
 ComponentDeltaColdProducerRequire ($states.applied_readback.control -eq $states.requested.control -and $states.applied_readback.state -eq $states.requested.state -and $states.restored_readback.control -eq $states.previous.control -and $states.restored_readback.state -eq $states.previous.state) 'QoS exact applied readback and full prior restoration'
 foreach($name in @('rollback_readback','fallback_readback')){
  $state=ComponentDeltaColdProducerField $snapshot $name
  foreach($field in @('version','control_mask','state_mask')){$null=ComponentDeltaColdProducerInteger $state $field 0 0}
 }
 foreach($name in @('query_previous','set_requested','query_applied','set_restore','query_restored')){
  $api=ComponentDeltaColdProducerField $snapshot $name
  ComponentDeltaColdProducerBool $api 'sampled' $true
  $null=ComponentDeltaColdProducerInteger $api 'status' 1 4294967295
  $null=ComponentDeltaColdProducerInteger $api 'error' 0 0
 }
 foreach($name in @('set_rollback','query_rollback','set_fallback','query_fallback')){
  $api=ComponentDeltaColdProducerField $snapshot $name
  ComponentDeltaColdProducerBool $api 'sampled' $false
  foreach($field in @('status','error')){$null=ComponentDeltaColdProducerInteger $api $field 0 0}
 }
}
function script:ComponentDeltaValidateProcessQosControls($controls) {
 ComponentDeltaColdProducerText $controls 'schema' 'pas.cold-process-qos-controls.v1'
 ComponentDeltaColdProducerText $controls 'policy' 'own_cold_process_execution_speed_high_qos'
 foreach($pair in @(@('assertions',24),@('failed',0),@('version',1),@('execution_speed_mask',1),@('supported_mask',5),@('device_operations',0))){$null=ComponentDeltaColdProducerInteger $controls $pair[0] $pair[1] $pair[1]}
 foreach($name in @('pass','synthetic_api_controls','actual_windows_restore_control')){ComponentDeltaColdProducerBool $controls $name $true}
 foreach($name in @('cost_qualification','live_ready')){ComponentDeltaColdProducerBool $controls $name $false}
 $checks=ComponentDeltaColdProducerField $controls 'checks'
 ComponentDeltaColdProducerRequire ($checks -is [array] -and $checks.Count -eq 24) 'QoS separate exact controls24'
 $ids=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 foreach($case in $checks){$id=ComponentDeltaColdProducerField $case 'id';ComponentDeltaColdProducerRequire ($id -is [string] -and $id.Length -gt 0 -and $ids.Add($id)) 'QoS controls unique nonempty ids';ComponentDeltaColdProducerBool $case 'pass' $true}
 ComponentDeltaValidateProcessQos (ComponentDeltaColdProducerField $controls 'actual_windows_snapshot')
}
function script:ComponentDeltaValidateColdProducer($report) {
 $null=ComponentDeltaColdProducerInteger $report 'producer_priority_requested' 2 2
 ComponentDeltaValidateColdProducerWait $report
 if(($null -ne $script:noiseRepairBuild -and $script:noiseRepairBuild.process_qos_required) -or $report.PSObject.Properties.Name -ccontains 'process_qos' -or ($report -is [Collections.IDictionary] -and $report.Contains('process_qos'))){ComponentDeltaValidateProcessQos (ComponentDeltaColdProducerField $report 'process_qos')}
 $plan=ComponentDeltaColdProducerField $report 'worker_placement'
 ComponentDeltaColdProducerText $plan 'schema' 'pas.cold-worker-placement.v1'
 ComponentDeltaColdProducerBool $plan 'valid' $true;ComponentDeltaColdProducerText $plan 'failure' 'none'
 foreach($name in @('active_groups','process_class','group','topology_error','device_operations')){
  $expected=@{active_groups=1;process_class=32;group=0;topology_error=0;device_operations=0}[$name]
  $null=ComponentDeltaColdProducerInteger $plan $name $expected $expected
 }
 ComponentDeltaColdProducerBool $plan 'cost_qualification' $false
 $null=ComponentDeltaColdProducerInteger $plan 'topology_bytes' 1 65536
 $cores=ComponentDeltaColdProducerInteger $plan 'topology_cores' 2 256
 $null=ComponentDeltaColdProducerInteger $plan 'topology_logical_processors' 2 256
 $null=ComponentDeltaColdProducerInteger $plan 'efficiency_class' 0 255
 $process=ComponentDeltaColdProducerMask $plan 'process_mask';$system=ComponentDeltaColdProducerMask $plan 'system_mask'
 $consumer=ComponentDeltaColdProducerMask $plan 'consumer_mask';$producer=ComponentDeltaColdProducerMask $plan 'producer_mask'
 $consumerCore=ComponentDeltaColdProducerInteger $plan 'consumer_core' 0 ($cores-1);$producerCore=ComponentDeltaColdProducerInteger $plan 'producer_core' 0 ($cores-1)
 $consumerNumber=ComponentDeltaColdProducerInteger $plan 'consumer_number' 0 63;$producerNumber=ComponentDeltaColdProducerInteger $plan 'producer_number' 0 63
 ComponentDeltaColdProducerRequire (($process -band $system) -eq $process -and ($consumer -band $process) -eq $consumer -and ($producer -band $process) -eq $producer -and $consumerCore -ne $producerCore -and $consumerNumber -lt $producerNumber -and $consumer -eq ([uint64]1 -shl $consumerNumber) -and $producer -eq ([uint64]1 -shl $producerNumber)) 'two distinct physical-core LP masks in the unchanged process/system mask'
 $affinity=ComponentDeltaColdProducerField $report 'producer_affinity'
 foreach($name in @('verified','previous_available','applied_available','restored_available','set_succeeded','applied','close_attempted','restored')){ComponentDeltaColdProducerBool $affinity $name $true}
 foreach($name in @('rollback_attempted','rollback_restored','rollback_available','fallback_attempted','fallback_available')){ComponentDeltaColdProducerBool $affinity $name $false}
 foreach($name in @('failure','rollback_failure','fallback_failure')){ComponentDeltaColdProducerText $affinity $name 'none'}
 $affinityThread=ComponentDeltaColdProducerInteger $affinity 'thread_id' 1 4294967295
 $null=ComponentDeltaColdProducerInteger $affinity 'active_groups' 1 1;$null=ComponentDeltaColdProducerInteger $affinity 'process_class' 32 32
 ComponentDeltaColdProducerRequire ((ComponentDeltaColdProducerMask $affinity 'process_mask') -eq $process -and (ComponentDeltaColdProducerMask $affinity 'system_mask') -eq $system) 'affinity process/system mask joins'
 foreach($name in @('requested_group','applied_group','previous_group','restored_group')){$null=ComponentDeltaColdProducerInteger $affinity $name 0 0}
 ComponentDeltaColdProducerRequire ((ComponentDeltaColdProducerMask $affinity 'requested_mask') -eq $producer -and (ComponentDeltaColdProducerMask $affinity 'applied_mask') -eq $producer) 'affinity requested/applied producer LP joins'
 ComponentDeltaColdProducerRequire ((ComponentDeltaColdProducerMask $affinity 'previous_mask') -eq (ComponentDeltaColdProducerMask $affinity 'restored_mask')) 'affinity exact explicit restoration'
 $priority=ComponentDeltaColdProducerField $report 'producer_priority'
 ComponentDeltaColdProducerText $priority 'schema' 'pas.cold-producer-priority-snapshot.v1'
 foreach($name in @('verified','previous_available','constructor_attempted','applied_available','close_attempted','restored_available','explicit_restored','body_entered','body_completed')){ComponentDeltaColdProducerBool $priority $name $true}
 foreach($name in @('rollback_attempted','rollback_restored')){ComponentDeltaColdProducerBool $priority $name $false}
 foreach($name in @('failure','cleanup_failure')){ComponentDeltaColdProducerText $priority $name 'none'}
 $null=ComponentDeltaColdProducerInteger $priority 'requested' 2 2;$null=ComponentDeltaColdProducerInteger $priority 'applied' 2 2
 $null=ComponentDeltaColdProducerInteger $priority 'process_class_after_close' 32 32
 $priorityThread=ComponentDeltaColdProducerInteger $priority 'thread_id' 1 4294967295
 $previous=ComponentDeltaColdProducerInteger $priority 'previous' -2147483648 2147483647;$restored=ComponentDeltaColdProducerInteger $priority 'restored' -2147483648 2147483647
 ComponentDeltaColdProducerRequire ($priorityThread -eq $affinityThread -and $previous -eq $restored) 'priority same owner thread and exact explicit restoration'
}
function script:ComponentDeltaValidateColdProducerControls($report) {
 $controls=ComponentDeltaColdProducerField $report 'producer_placement_controls'
 ComponentDeltaColdProducerText $controls 'schema' 'pas.cold-producer-placement-controls.v1'
 foreach($name in @('checks','passed')){$null=ComponentDeltaColdProducerInteger $controls $name 27 27}
 foreach($name in @('failed','device_operations')){$null=ComponentDeltaColdProducerInteger $controls $name 0 0}
 $priority=ComponentDeltaColdProducerInteger $controls 'producer_priority_requested' 2 2;ComponentDeltaValidateColdProducerWait $controls
 $tail=ComponentDeltaColdProducerField $controls 'producer_active_tail_ns'
 ComponentDeltaColdProducerBool $controls 'synthetic_api_controls' $true
 foreach($name in @('cost_qualification','live_ready')){ComponentDeltaColdProducerBool $controls $name $false}
 $cases=ComponentDeltaColdProducerField $controls 'cases'
 ComponentDeltaColdProducerRequire ($cases -is [array] -and $cases.Count -eq 27) 'placement controls exact cases27'
 $ids=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 foreach($case in $cases){$id=ComponentDeltaColdProducerField $case 'id';ComponentDeltaColdProducerRequire ($id -is [string] -and $id.Length -gt 0 -and $ids.Add($id)) 'placement controls unique nonempty id';ComponentDeltaColdProducerBool $case 'pass' $true}
 $null=ComponentDeltaColdProducerInteger $report 'invalid_requests_rejected_before_io' 8 8
 $null=ComponentDeltaColdProducerInteger $report 'controlled_stop_lifecycle_cases' 2 2
 $null=ComponentDeltaColdProducerInteger $report 'result_tail_fake_clock_cases' 4 4
 $stops=ComponentDeltaColdProducerField $report 'controlled_stop_producer_snapshots'
 ComponentDeltaColdProducerRequire ($stops -is [array] -and $stops.Count -eq 2) 'controlled stop exact snapshots2'
 $modes=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 foreach($stop in $stops){
  $mode=ComponentDeltaColdProducerField $stop 'mode';ComponentDeltaColdProducerRequire ($mode -is [string] -and $mode -cin @('escape','console') -and $modes.Add($mode)) 'one escape and one console producer snapshot'
  $joined=@{producer_priority_requested=$priority;producer_active_tail_ns=$tail;worker_placement=(ComponentDeltaColdProducerField $stop 'worker_placement');producer_affinity=(ComponentDeltaColdProducerField $stop 'producer_affinity');producer_priority=(ComponentDeltaColdProducerField $stop 'producer_priority')}
  if($tail -eq 0){foreach($name in @('producer_wait_policy','producer_active_wait_horizon_ns','producer_active_wait_poll_cap','producer_no_kernel_wait')){$joined[$name]=ComponentDeltaColdProducerField $controls $name}}
  if(($null -ne $script:noiseRepairBuild -and $script:noiseRepairBuild.process_qos_required) -or $report.PSObject.Properties.Name -ccontains 'process_qos'){$joined.process_qos=ComponentDeltaColdProducerField $report 'process_qos'}
  ComponentDeltaValidateColdProducer $joined
 }
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
