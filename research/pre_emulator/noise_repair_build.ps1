. (Join-Path $PSScriptRoot 'component_delta.ps1')
. (Join-Path $PSScriptRoot 'source_performance_deltas.ps1')
# Dot-source after common.ps1. This loader validates a declared Release harness
# delta; it never builds or runs tests/metrics, and issues no candidate admission.
function script:NoiseRepairRequire([bool]$condition,[string]$message){if(-not $condition){throw ('noise-repair '+$message)}}
function NoiseRepairProperty($value,[string]$name){return $value.PSObject.Properties.Name -contains $name}
function NoiseRepairRelative([string]$path){
 $absolute=[IO.Path]::GetFullPath($path)
 NoiseRepairRequire ($absolute.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) ('path outside main '+$path)
 return [IO.Path]::GetRelativePath($repo,$absolute).Replace('\','/')
}
function NoiseRepairSameBinding($actual,$expected,[string]$label){
 NoiseRepairRequire ([string]::Equals([IO.Path]::GetFullPath($actual.path),[IO.Path]::GetFullPath($expected.path),[StringComparison]::OrdinalIgnoreCase)) ($label+' path')
 NoiseRepairRequire ($actual.bytes -eq $expected.bytes -and $actual.sha256 -ceq $expected.sha256) ($label+' bytes/SHA')
}
function NoiseRepairCheckBinding($entry){
 NoiseRepairRequire ($null -ne $entry -and $entry.path -and $entry.sha256 -cmatch '^[0-9a-f]{64}$' -and $entry.bytes -ge 0) 'complete binding required'
 $absolute=[IO.Path]::GetFullPath($entry.path)
 NoiseRepairRequire ([IO.Path]::GetExtension($absolute) -cne '.zip') 'historical archive ZIP cannot be a new native input'
 if($script:noiseRepairChecked.ContainsKey($absolute)){
  NoiseRepairSameBinding $entry $script:noiseRepairChecked[$absolute] 'duplicate binding'
 }else{
  CheckEntry $entry
  $script:noiseRepairChecked[$absolute]=$entry
 }
}
function NoiseRepairReadBinding($entry){NoiseRepairCheckBinding $entry;return Get-Content -LiteralPath $entry.path -Raw|ConvertFrom-Json}
function NoiseRepairAddInputs($entries){
 foreach($entry in @($entries)){
  # The existing native reader accepts an actual input only through 256 MiB.
  # Checked historical dependencies are not automatically execution inputs.
  NoiseRepairRequire ($null -ne $entry -and $entry.bytes -le 268435456) ('actual native input exceeds existing bound '+$entry.path)
  NoiseRepairCheckBinding $entry
  $script:noiseRepairActiveInputs[[IO.Path]::GetFullPath($entry.path)]=$entry
 }
}
function NoiseRepairSameValue($actual,$expected,[string]$label){
 if($null -eq $actual -or $null -eq $expected){NoiseRepairRequire ($null -eq $actual -and $null -eq $expected) ($label+' null identity');return}
 if($expected -is [System.Collections.IList]){
  NoiseRepairRequire ($actual -is [System.Collections.IList] -and $actual.Count -eq $expected.Count) ($label+' array identity')
  for($index=0;$index -lt $expected.Count;$index++){NoiseRepairSameValue $actual[$index] $expected[$index] ($label+'['+$index+']')}
 }elseif($expected -is [pscustomobject]){
  NoiseRepairRequire ($actual -is [pscustomobject]) ($label+' object identity')
  $names=@($expected.PSObject.Properties.Name);$actualNames=@($actual.PSObject.Properties.Name)
  NoiseRepairRequire (@(Compare-Object ($names|Sort-Object) ($actualNames|Sort-Object)).Count -eq 0) ($label+' property set')
  foreach($name in $names){NoiseRepairSameValue $actual.$name $expected.$name ($label+'.'+$name)}
 }else{NoiseRepairRequire ($actual -ceq $expected) ($label+' value identity')}
}
function NoiseRepairMaintenanceChange([string]$relative){
 return ($relative -in @('AGENTS.md','README.md','research/pre_emulator/PLAN.md') -or $relative -cmatch '^research/pre_emulator/[^/]+[.]ps1$' -or ($relative -cmatch '^docs/.*[.](?:md|json)$' -and $relative -cne 'docs/research/zero-miss-20261006/windows/PRELIVE_COST_PROTOCOL_R2_20261007.md'))
}
function NoiseRepairConsumerPriorityPolicy($plan,$controls,$header,$tests){
 $hasPlan=NoiseRepairProperty $plan 'consumer_priority_requested'
 $hasControls=NoiseRepairProperty $controls 'consumer_priority_requested'
 $legacyHeader=$header.bytes -eq 9155 -and $header.sha256 -ceq '22e2ae774920f223a4f358d800912d86c6fe1976c4452edd8f22d95a1c024cd8'
 $legacyTests=$tests.bytes -eq 11370 -and $tests.sha256 -ceq '083e294be6fe79d249bf8f89c65c3adedab28cc66beab09b85c5c2406a06a413'
 if(-not $hasPlan -and -not $hasControls){
  NoiseRepairRequire ($legacyHeader -and $legacyTests) 'missing consumer policy requires exact legacy compiled header/tests'
  return 1
 }
 NoiseRepairRequire ($hasPlan -and $hasControls -and ($plan.consumer_priority_requested -is [int] -or $plan.consumer_priority_requested -is [long]) -and ($controls.consumer_priority_requested -is [int] -or $controls.consumer_priority_requested -is [long]) -and $plan.consumer_priority_requested -eq 2 -and $controls.consumer_priority_requested -eq 2 -and -not $legacyHeader -and -not $legacyTests) 'new consumer policy requires integer HIGHEST(2) plan/controls and new compiled header/tests'
 return 2
}
function NoiseRepairProducerActiveTailPolicy($plan,$controls,$header,$tests){
 $hasPlan=NoiseRepairProperty $plan 'producer_active_tail_ns'
 $legacyHeader1=$header.bytes -eq 4804 -and $header.sha256 -ceq '9e9700107d4f07301ef302351fa645cb705a57b1e049f4166eb70604a116ddc2'
 $legacyTests1=$tests.bytes -eq 12026 -and $tests.sha256 -ceq 'ee3b63b9787b21ebe27bdc3e7b32e571c9a3dd1e7532f5ac1951d8e8932ff7a6'
 $legacyHeader2=$header.bytes -eq 4998 -and $header.sha256 -ceq 'ee2f57c862d0543a5c6855cd43f4a8aeab157b96bc706acf85b6c5a004595c38'
 $legacyTests2=$tests.bytes -eq 12056 -and $tests.sha256 -ceq 'ffb5f56676b0e0ef23e92b1a1ffe071cbbe644568c5d660d2a8479ff2ddeb058'
 NoiseRepairRequire ((NoiseRepairProperty $controls 'active_tail_ns') -and ($controls.active_tail_ns -is [int] -or $controls.active_tail_ns -is [long]) -and $controls.active_tail_poll_cap -eq 16777216) 'producer controls declare integer active-tail and unchanged poll cap'
 if(NoiseRepairProperty $plan 'producer_wait_policy'){
  NoiseRepairRequire ($plan.producer_wait_policy -is [string] -and $plan.producer_wait_policy -ceq 'full_period_active_qpc' -and $hasPlan -and ($plan.producer_active_tail_ns -is [int] -or $plan.producer_active_tail_ns -is [long]) -and $plan.producer_active_tail_ns -eq 0 -and $controls.active_tail_ns -eq 2000000 -and -not $legacyHeader1 -and -not $legacyHeader2 -and -not $legacyTests1 -and -not $legacyTests2) 'explicit full-period active policy with separate original 2ms controls and new compiled sources'
  foreach($pair in @(@('producer_active_wait_horizon_ns',100000000),@('producer_active_wait_poll_cap',16777216))){$field=$plan.($pair[0]);NoiseRepairRequire (($field -is [int] -or $field -is [long]) -and $field -eq $pair[1]) 'exact bounded active wait plan integers'}
  foreach($pair in @(@('producer_wait_controls_count',43),@('producer_active_wait_controls_count',12))){$field=$plan.($pair[0]);NoiseRepairRequire (($field -is [int] -or $field -is [long]) -and $field -eq $pair[1]) 'explicit unchanged original and separate active control denominators'}
  NoiseRepairRequire ($controls.pass -is [bool] -and $controls.pass -and $controls.failed -eq 0 -and $controls.assertions -eq 43 -and $controls.checks.Count -eq 43 -and @($controls.checks|Where-Object {$_.pass -isnot [bool] -or -not $_.pass}).Count -eq 0) 'full-active build keeps original43 and aggregate control PASS'
  $diagnostic=$controls.diagnostic_controls
  NoiseRepairRequire ($diagnostic.schema -ceq 'pas.producer-wait-diagnostic-controls.v1' -and $diagnostic.assertions -eq 20 -and $diagnostic.checks.Count -eq 20 -and $diagnostic.failed -eq 0 -and @($diagnostic.checks|Where-Object {$_.pass -isnot [bool] -or -not $_.pass}).Count -eq 0 -and $diagnostic.kernel_sample_cap -eq 4 -and $diagnostic.device_operations -eq 0) 'full-active build keeps separate diagnostic20 PASS'
  foreach($pair in @(@('pass',$true),@('synthetic_api_controls',$true),@('cost_qualification',$false),@('live_ready',$false))){$field=$diagnostic.($pair[0]);NoiseRepairRequire ($field -is [bool] -and $field -eq $pair[1]) 'typed diagnostic20 flags'}
  $active=$controls.active_wait_controls
  NoiseRepairRequire ($active.schema -ceq 'pas.producer-active-qpc-controls.v1' -and $active.policy -ceq 'full_period_active_qpc' -and $active.assertions -eq 12 -and $active.checks.Count -eq 12 -and $active.failed -eq 0 -and @($active.checks|Where-Object {$_.pass -isnot [bool] -or -not $_.pass}).Count -eq 0) 'separate active QPC controls12 required'
  foreach($pair in @(@('pass',$true),@('synthetic_api_controls',$true),@('no_kernel_wait',$true),@('cost_qualification',$false),@('live_ready',$false))){$field=$active.($pair[0]);NoiseRepairRequire ($field -is [bool] -and $field -eq $pair[1]) 'typed active QPC control flags'}
  foreach($pair in @(@('assertions',12),@('failed',0),@('horizon_ns',100000000),@('poll_cap',16777216),@('device_operations',0))){$field=$active.($pair[0]);NoiseRepairRequire (($field -is [int] -or $field -is [long]) -and $field -eq $pair[1]) 'typed active QPC control limits'}
  $activeIds=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
  foreach($check in $active.checks){NoiseRepairRequire ($check.id -is [string] -and $check.id.Length -gt 0 -and $activeIds.Add($check.id)) 'unique nonempty active QPC control ids'}
  return 0
 }
 NoiseRepairRequire (-not (NoiseRepairProperty $controls 'active_wait_controls') -and -not (NoiseRepairProperty $plan 'producer_active_wait_horizon_ns') -and -not (NoiseRepairProperty $plan 'producer_active_wait_poll_cap')) 'full active fields cannot borrow legacy tail policy'
 if(-not $hasPlan){
  NoiseRepairRequire ($controls.active_tail_ns -eq 20000000 -and (($legacyHeader1 -and $legacyTests1) -or ($legacyHeader2 -and $legacyTests2))) 'missing producer tail plan requires exact paired legacy 20ms compiled header/tests'
  return 20000000
 }
 NoiseRepairRequire (($plan.producer_active_tail_ns -is [int] -or $plan.producer_active_tail_ns -is [long]) -and $plan.producer_active_tail_ns -eq 2000000 -and $controls.active_tail_ns -eq 2000000 -and -not $legacyHeader1 -and -not $legacyHeader2 -and -not $legacyTests1 -and -not $legacyTests2) 'new producer tail requires integer 2ms plan/controls and new compiled header/tests'
 return 2000000
}
function NoiseRepairRequireNativeInputs($receipt,$entries){
 $verification=NoiseRepairReadBinding $receipt
 NoiseRepairRequire ($script:noiseRepairSpecs.ContainsKey($verification.stage)) 'consumer policy native stage already verified'
 $closure=NoiseRepairReadBinding (Binding (Join-Path $pkg ($verification.stage+'-input-closure-verification.json')))
 $root=NoiseRepairReadBinding $closure.root
 NoiseRepairRequire ($root.schema -ceq 'pas.current-input-closure.v1' -and $root.stage -ceq $verification.stage -and @($root.leaves).Count -eq $closure.leaf_count -and $root.leaf_count -eq $closure.leaf_count -and $root.files_count -eq $closure.files_count) 'consumer policy native closure root join'
 $members=@{};$index=0
 foreach($leafPin in $root.leaves){
  $leaf=NoiseRepairReadBinding $leafPin
  NoiseRepairRequire ($leaf.schema -ceq 'pas.current-input-closure-leaf.v1' -and $leaf.stage -ceq $verification.stage -and $leaf.index -eq $index -and @($leaf.files).Count -gt 0 -and @($leaf.files).Count -le 64) 'consumer policy ordered native closure leaf'
  foreach($file in $leaf.files){
   NoiseRepairRequire ($file.path -and $file.bytes -ge 0 -and $file.sha256 -cmatch '^[0-9a-f]{64}$') 'consumer policy native input pin shape'
   $key=[IO.Path]::GetFullPath($file.path)
   NoiseRepairRequire (-not $members.ContainsKey($key)) 'consumer policy duplicate native input'
   $members[$key]=$file
  }
  $index++
 }
 NoiseRepairRequire ($members.Count -eq $closure.files_count) 'consumer policy complete native closure metadata'
 foreach($entry in $entries){
  $key=[IO.Path]::GetFullPath($entry.path)
  NoiseRepairRequire ($members.ContainsKey($key)) ('consumer compiled source missing from actual native inputs '+$entry.path)
  NoiseRepairSameBinding $members[$key] $entry 'consumer compiled source/native input join'
 }
 # Only small leaf metadata is added; historical leaf members are not rehashed.
 NoiseRepairAddInputs @($root.leaves)
 $script:noiseRepairLastNativeInputs=$members
}
function NoiseRepairCheckNativeReceipt($entry){
 $verification=NoiseRepairReadBinding $entry
 NoiseRepairRequire ([IO.Path]::GetFileName($entry.path) -ceq ($verification.stage+'-verification.json')) 'native verification identity'
 Receipt $verification.stage
 $stageRoot=Join-Path $out $verification.stage
 $commandPin=Binding (Join-Path $stageRoot ($verification.stage+'-command.json'))
 $command=NoiseRepairReadBinding $commandPin
 NoiseRepairRequire ($command.stage -ceq $verification.stage) 'native command stage'
 $freezePin=Binding (Join-Path $stageRoot 'freeze.json')
 $freeze=NoiseRepairReadBinding $freezePin
 $directSpec=Join-Path $pkg ($verification.stage+'-direct-spec.json')
 $adapterSpec=Join-Path $out ($verification.stage+'-inputs/spec.json')
 $specRows=@($freeze.files|Where-Object {[string]::Equals($_.path,$directSpec,[StringComparison]::OrdinalIgnoreCase) -or [string]::Equals($_.path,$adapterSpec,[StringComparison]::OrdinalIgnoreCase)})
 NoiseRepairRequire ($specRows.Count -eq 1) 'one frozen native specification'
 $spec=NoiseRepairReadBinding $specRows[0]
 NoiseRepairRequire ($spec.stage -ceq $verification.stage -and $spec.exe -ceq $command.spec.exe -and (@($spec.argv)|ConvertTo-Json -Compress) -ceq (@($command.spec.argv)|ConvertTo-Json -Compress)) 'native specification/command join'
 $script:noiseRepairSpecs[$verification.stage]=$spec
 $closurePin=Binding (Join-Path $pkg ($verification.stage+'-input-closure-verification.json'))
 $closure=NoiseRepairReadBinding $closurePin
 NoiseRepairRequire ($closure.stage -ceq $verification.stage -and $closure.status -ceq 'COMPLETE' -and $closure.pass -eq $true -and $closure.before_verified -eq $true -and $closure.after_verified -eq $true) 'native input closure incomplete'
 NoiseRepairAddInputs @($closure.root,$entry,$commandPin,$freezePin,$specRows[0],$closurePin)
 NoiseRepairAddInputs @($verification.entries)
 foreach($path in @((Join-Path $stageRoot ($verification.stage+'-result.json')),(Join-Path $stageRoot 'state.json'),(Join-Path $stageRoot 'freeze.json'),(Join-Path $stageRoot 'contract.json'))){NoiseRepairAddInputs @((Binding $path))}
}
function NoiseRepairProcessQosPolicy($plan,$controls,$header,$tests) {
 $hasPolicy=NoiseRepairProperty $plan 'process_qos_policy'
 $hasControls=NoiseRepairProperty $controls 'process_qos_controls'
 $original54Header=$header.bytes -eq 13299 -and $header.sha256 -ceq '925cbad472141db860990f349503a93a7cdb3dc042e39201988d02f36e04f1a2'
 $original54Tests=$tests.bytes -eq 32590 -and $tests.sha256 -ceq '1be55d92023609a1feb84c20aaea0ae6e0adbeebde80923839add0bac15c118f'
 if(-not $hasPolicy){
  NoiseRepairRequire (-not $hasControls -and -not (NoiseRepairProperty $plan 'process_qos_controls_count')) 'QoS controls/count cannot borrow an undeclared legacy policy'
  if(NoiseRepairProperty $plan 'producer_wait_policy') {NoiseRepairRequire ($original54Header -and $original54Tests) 'unscoped full-active policy only accepted for exact historical54 compiled source pair'}
  elseif((NoiseRepairProperty $plan 'producer_active_tail_ns') -and $plan.producer_active_tail_ns -eq 2000000){
   # Exact sealed 2ms pairs: main44/47-52 and diagnostic-only main53.
   # New QoS sources cannot downgrade by removing their policy/control fields.
   $legacy2ms=($header.bytes -eq 4994 -and $header.sha256 -ceq '60a22dadc78f71d895e2488c55ed32222af5cbd294a98c60f8b1fd0dc509dee1' -and $tests.bytes -eq 12586 -and $tests.sha256 -ceq '5054887016f4ebdc8a43681c14faabe99fb0740045df9c48b4ea49214ff723cb') -or ($header.bytes -eq 10760 -and $header.sha256 -ceq '8fc675f9d1e95a4809379e643a7dc4487e01e30a92c52e2a080e578c5e7ef0f9' -and $tests.bytes -eq 23479 -and $tests.sha256 -ceq '9efc0314f7415bed15cb70f6de2c1b3fe3242aa9fc3f83acb66ff45b0bca9adb')
   NoiseRepairRequire $legacy2ms 'missing QoS 2ms policy only accepted for exact paired historical compiled sources'
  }
  return $false
 }
 NoiseRepairRequire ($hasControls -and $plan.process_qos_policy -is [string] -and $plan.process_qos_policy -ceq 'own_cold_process_execution_speed_high_qos' -and ($plan.process_qos_controls_count -is [int] -or $plan.process_qos_controls_count -is [long]) -and $plan.process_qos_controls_count -eq 24 -and -not $original54Header -and -not $original54Tests) 'declared new own-process QoS policy/count and new compiled source pair'
 NoiseRepairRequire ($plan.producer_wait_policy -is [string] -and $plan.producer_wait_policy -ceq 'full_period_active_qpc' -and $plan.producer_active_tail_ns -eq 0 -and $controls.pass -is [bool] -and $controls.pass) 'QoS candidate keeps explicit full-active policy and all original/nested controls PASS'
 ComponentDeltaValidateProcessQosControls $controls.process_qos_controls
 return $true
}
function NoiseRepairCpuBuildGroups {
 return @([pscustomobject]@{targets=@('pas_core')},[pscustomobject]@{targets=@('own_bridge','current_chain','consumer_priority_controls','worker_affinity_controls','producer_wait_tests')},[pscustomobject]@{targets=@('owner_checkpoint_tests','owner_expiry_tests','component_mask_tests','uniform_fnv_tests')})
}
function NoiseRepairCpuProcessEvidence($process,$receipt,$closure){
 $names=@('specification','input_closure','result','verification','freeze','state','contract','command')
 NoiseRepairRequire (@($process.PSObject.Properties.Name).Count -eq 8 -and @(Compare-Object (@($process.PSObject.Properties.Name)|Sort-Object) ($names|Sort-Object)).Count -eq 0) 'exact eight-field CPU native process evidence'
 NoiseRepairCheckNativeReceipt $receipt
 $verification=NoiseRepairReadBinding $receipt;$stage=$verification.stage
 $paths=@{specification=(Join-Path $out ($stage+'-inputs/spec.json'));input_closure=(Join-Path $pkg ($stage+'-input-closure-verification.json'));result=(Join-Path $out ($stage+'/'+$stage+'-result.json'));verification=$receipt.path;freeze=(Join-Path $out ($stage+'/freeze.json'));state=(Join-Path $out ($stage+'/state.json'));contract=(Join-Path $out ($stage+'/contract.json'));command=(Join-Path $out ($stage+'/'+$stage+'-command.json'))}
 foreach($name in $names){NoiseRepairSameBinding $process.$name (Binding $paths[$name]) ('actual CPU native process '+$name);NoiseRepairAddInputs @($process.$name)}
 NoiseRepairSameBinding $process.input_closure $closure 'CPU process/declared closure'
 $value=NoiseRepairReadBinding $closure
 NoiseRepairRequire ($value.schema -ceq 'pas.current-input-closure-verification.v1' -and $value.stage -ceq $stage -and $value.status -ceq 'COMPLETE' -and $value.pass -eq $true -and $value.before_verified -eq $true -and $value.after_verified -eq $true) 'CPU stage actual COMPLETE before/after closure'
 NoiseRepairSameBinding $value.verification $receipt 'CPU closure/verification';NoiseRepairSameBinding $value.state $process.state 'CPU closure/state'
 return $script:noiseRepairSpecs[$stage]
}
function NoiseRepairVerifyCpuBuildStages($delta,$plan,$sourcePins){
 $groups=@(NoiseRepairCpuBuildGroups)
 NoiseRepairSameValue $plan.build_target_groups $groups 'CPU plan exact ordered build groups'
 NoiseRepairSameValue $delta.build_target_groups $groups 'CPU binding exact ordered build groups'
 NoiseRepairRequire (@($delta.build_receipts).Count -eq 3 -and @($delta.build_input_closures).Count -eq 3 -and @($delta.build_processes).Count -eq 3 -and @($delta.build_receipts.path|Sort-Object -Unique).Count -eq 3) 'exact three unique fresh CPU build stages'
 $null=NoiseRepairCpuProcessEvidence $delta.configure_process $delta.configure_receipt $delta.configure_input_closure
 NoiseRepairRequireNativeInputs $delta.configure_receipt (@($delta.source_manifest,$delta.core_build_plan)+@($sourcePins))
 $priorTargets=@();$priorProofs=@($delta.configure_process.PSObject.Properties|ForEach-Object Value)
 for($index=0;$index -lt 3;$index++){
  $receipt=$delta.build_receipts[$index];$spec=NoiseRepairCpuProcessEvidence $delta.build_processes[$index] $receipt $delta.build_input_closures[$index]
  NoiseRepairRequire ([string]::Equals($spec.exe,$env:ComSpec,[StringComparison]::OrdinalIgnoreCase) -and @($spec.argv).Count -eq 4 -and $spec.argv[0] -ceq '/d' -and $spec.argv[1] -ceq '/s' -and $spec.argv[2] -ceq '/c') 'CPU existing cmd adapter native launch'
  $commandPath=Join-Path $out ($spec.stage+'-inputs/build.cmd')
  NoiseRepairRequire ([string]::Equals($spec.argv[3],$commandPath,[StringComparison]::OrdinalIgnoreCase)) 'CPU current stage literal build command path'
  $freeze=NoiseRepairReadBinding $delta.build_processes[$index].freeze
  $commandPins=@($freeze.files|Where-Object {[string]::Equals($_.path,$commandPath,[StringComparison]::OrdinalIgnoreCase)})
  NoiseRepairRequire ($commandPins.Count -eq 1) 'CPU build command frozen before launch';NoiseRepairAddInputs $commandPins
  $lines=@((Get-Content -LiteralPath $commandPath)|Where-Object {$_ -cmatch '^"[^"\r\n]+cmake[.]exe" '})
  NoiseRepairRequire ($lines.Count -eq 1) 'one actual CPU CMake launch line'
  $tokens=@([regex]::Matches($lines[0],'"([^"\r\n]*)"')|ForEach-Object {$_.Groups[1].Value})
  $expected=@('--build',$delta.build_root,'--parallel','4','--target')+@($groups[$index].targets)
  NoiseRepairRequire ($tokens.Count -eq $expected.Count+1 -and (($tokens|ForEach-Object {'"'+$_+'"'}) -join ' ') -ceq $lines[0] -and (@($tokens[1..($tokens.Count-1)])|ConvertTo-Json -Compress) -ceq (@($expected)|ConvertTo-Json -Compress)) 'exact ordered CPU group CMake argv and unchanged parallel4'
  $cmakePins=@($freeze.files|Where-Object {[string]::Equals($_.path,$tokens[0],[StringComparison]::OrdinalIgnoreCase)})
  NoiseRepairRequire ($cmakePins.Count -eq 1) 'actual CPU CMake image frozen';NoiseRepairAddInputs $cmakePins
  $priorOutputs=@($delta.compiled_outputs|Where-Object {$p=$_.path.Replace('\','/');$name=[IO.Path]::GetFileName($_.path);$object=[IO.Path]::GetExtension($_.path) -ceq '.obj';@($priorTargets|Where-Object {($object -and $p.Contains('/CMakeFiles/'+$_+'.dir/',[StringComparison]::Ordinal)) -or $name -ceq ($_+'.lib') -or $name -ceq ($_+'.exe')}).Count -gt 0})
  NoiseRepairRequireNativeInputs $receipt (@($delta.source_manifest,$delta.core_build_plan)+@($sourcePins)+@($priorProofs)+@($priorOutputs)+@(Binding (Join-Path $delta.build_root 'build.ninja');Binding (Join-Path $delta.build_root 'CMakeCache.txt')))
  $priorTargets+=@($groups[$index].targets);$priorProofs+=@($delta.build_processes[$index].PSObject.Properties|ForEach-Object Value)
 }
}
function NoiseRepairAllowedChange([string]$relative,[bool]$allowCpu=$false){
 if($allowCpu -and (ComponentDeltaAllowed $relative)){return $true}
 if($relative -in @('research/prelive_current/producer_wait.hpp','research/prelive_current/tests/producer_wait_tests.cpp')){return $true}
 if($relative -in @('AGENTS.md','README.md','research/pre_emulator/PLAN.md','research/prelive_current/chain.cpp','research/prelive_current/consumer_priority.hpp','research/prelive_current/tests/consumer_priority_controls.cpp','research/prelive_current/tests/consumer_priority_tests.cpp','research/prelive_current/worker_affinity.hpp','research/prelive_current/tests/worker_affinity_tests.cpp','research/prelive_current/owner_candidate_step.hpp','research/prelive_current/diagnostic_processor.hpp','research/prelive_current/diagnostic_phase_profile.hpp','research/prelive_current/diagnostic_phase_summary.cpp','research/prelive_current/CMakeLists.txt')){return $true}
 if($relative -cmatch '^research/pre_emulator/qualification_reader/(?:main[.]cpp|controls[.]cpp|reader/narrow_preflight[.]hpp)$'){return $true}
 if($relative -cmatch '^research/pre_emulator/(?:[^/]+[.]ps1|(?:[^/]+/)*CMakeLists[.]txt)$'){return $true}
 if($relative -cmatch '^docs/.*[.](?:md|json)$' -and $relative -cne 'docs/research/zero-miss-20261006/windows/PRELIVE_COST_PROTOCOL_R2_20261007.md'){return $true}
 return $false
}
function script:NoiseRepairVerifyWorkerPlacement($report){
 if($script:noiseRepairBuild.process_qos_required){ComponentDeltaValidateProcessQos (ComponentDeltaColdProducerField $report 'process_qos')}
 if($script:noiseRepairBuild.producer_wait_required){
  if($report.PSObject.Properties.Name -ccontains 'producer_active_tail_ns'){
   NoiseRepairRequire (($report.producer_active_tail_ns -is [int] -or $report.producer_active_tail_ns -is [long]) -and $report.producer_active_tail_ns -eq $script:noiseRepairBuild.producer_active_tail_ns) 'actual producer active-tail must equal compiled integer policy'
  }else{NoiseRepairRequire ($script:noiseRepairBuild.producer_active_tail_ns -eq 20000000) 'missing runtime producer active-tail only permitted for identified legacy 20ms build'}
  if($script:noiseRepairBuild.producer_active_tail_ns -eq 0){
   NoiseRepairRequire ($report.producer_wait_policy -is [string] -and $report.producer_wait_policy -ceq 'full_period_active_qpc' -and $report.producer_no_kernel_wait -is [bool] -and $report.producer_no_kernel_wait) 'actual full-period active QPC policy and no kernel wait required'
   foreach($pair in @(@('producer_active_wait_horizon_ns',100000000),@('producer_active_wait_poll_cap',16777216))){$field=$report.($pair[0]);NoiseRepairRequire (($field -is [int] -or $field -is [long]) -and $field -eq $pair[1]) 'actual bounded active QPC limits'}
  }else{NoiseRepairRequire (-not (NoiseRepairProperty $report 'producer_wait_policy') -or $report.producer_wait_policy -cne 'full_period_active_qpc') 'legacy tail cannot declare full-period active policy'}
 }
 if(-not $script:noiseRepairBuild.worker_affinity_required){return}
 if($script:noiseRepairBuild.cold_harness_delta){
  ComponentDeltaValidateEligibilityIdentity $report
  $placement=ComponentDeltaColdProducerField $report 'worker_placement';$consumerApplied=ComponentDeltaColdProducerField $placement 'consumer_affinity_applied';NoiseRepairRequire ($consumerApplied -is [bool]) 'typed actual consumer affinity mode'
  NoiseRepairRequire ($report.schema -cin @('pas.prelive-complete-chain-cost.v1','pas.windows-production-pipeline-cold.v1') -and $consumerApplied -eq ($report.schema -ceq 'pas.prelive-complete-chain-cost.v1')) 'actual AA consumer applied / Normal consumer reserved mode'
  ComponentDeltaValidateEligibilityPlacement $placement $consumerApplied $script:noiseRepairBuild.worker_placement_schema
  if(-not $consumerApplied){ComponentDeltaValidateColdProducer $report $script:noiseRepairBuild.worker_placement_schema;return}
  foreach($role in @('consumer','producer')){$receipt=ComponentDeltaColdProducerField $report ($role+'_affinity');$mask=ComponentDeltaColdProducerMask $placement ($role+'_mask');ComponentDeltaColdProducerText $receipt 'schema' 'pas.cold-worker-affinity.v1';foreach($key in @('verified','applied','close_attempted','restored','previous_available','applied_available','restored_available')){ComponentDeltaColdProducerBool $receipt $key $true};foreach($key in @('rollback_attempted','fallback_attempted')){ComponentDeltaColdProducerBool $receipt $key $false};foreach($key in @('failure','rollback_failure','fallback_failure')){ComponentDeltaColdProducerText $receipt $key 'none'};$null=ComponentDeltaColdProducerInteger $receipt 'thread_id' 1 4294967295
   $states=@{};foreach($field in @('requested','applied_readback','previous','restored_readback')){$state=ComponentDeltaColdProducerField $receipt $field;$null=ComponentDeltaColdProducerInteger $state 'group' 0 0;$states[$field]=ComponentDeltaColdProducerMask $state 'mask'}
   ComponentDeltaColdProducerRequire ($states.requested -eq $mask -and $states.applied_readback -eq $mask -and $states.restored_readback -eq $states.previous -and (ComponentDeltaColdProducerMask $receipt 'process_mask') -eq (ComponentDeltaColdProducerMask $placement 'process_mask') -and (ComponentDeltaColdProducerMask $receipt 'system_mask') -eq (ComponentDeltaColdProducerMask $placement 'system_mask')) 'AA exact independently derived union/readback/restore'
   $null=ComponentDeltaColdProducerInteger $receipt 'active_groups' 1 1;$null=ComponentDeltaColdProducerInteger $receipt 'process_class' 32 32
  };NoiseRepairRequire ($report.consumer_affinity.thread_id -ne $report.producer_affinity.thread_id) 'independent cold worker threads';return
 }
 $p=$report.worker_placement
 NoiseRepairRequire ($p.schema -ceq 'pas.cold-worker-placement.v1' -and $p.valid -eq $true -and $p.failure -ceq 'none' -and $p.active_groups -eq 1 -and $p.process_class -eq 32 -and $p.group -eq 0 -and $p.topology_error -eq 0 -and $p.topology_bytes -gt 0 -and $p.topology_bytes -le 65536 -and $p.topology_cores -ge 2 -and $p.topology_cores -le 256 -and $p.topology_logical_processors -ge 2 -and $p.topology_logical_processors -le 256 -and $p.cost_qualification -eq $false -and $p.device_operations -eq 0) 'bounded actual cold worker placement required'
 NoiseRepairRequire ($p.consumer_mask -gt 0 -and $p.producer_mask -gt 0 -and $p.consumer_mask -ne $p.producer_mask -and $p.consumer_core -ne $p.producer_core -and $p.consumer_number -lt $p.producer_number -and $p.consumer_number -ge 0 -and $p.producer_number -lt 64 -and $p.efficiency_class -ge 0 -and $p.efficiency_class -le 255) 'two distinct deterministic highest-class physical cores required'
 foreach($role in @('consumer','producer')){
  $s=$report.($role+'_affinity');[uint64]$mask=$p.($role+'_mask')
  NoiseRepairRequire (($mask -band ($mask-1)) -eq 0 -and ($mask -band [uint64]$p.process_mask) -eq $mask) 'requested single LP in current process mask'
  NoiseRepairRequire ($s.schema -ceq 'pas.cold-worker-affinity.v1' -and $s.verified -eq $true -and $s.applied -eq $true -and $s.close_attempted -eq $true -and $s.restored -eq $true -and $s.previous_available -eq $true -and $s.applied_available -eq $true -and $s.restored_available -eq $true -and $s.thread_id -gt 0 -and $s.active_groups -eq 1 -and $s.process_class -eq 32 -and $s.process_mask -eq $p.process_mask -and $s.system_mask -eq $p.system_mask -and $s.requested.group -eq 0 -and $s.requested.mask -eq $mask -and $s.applied_readback.group -eq 0 -and $s.applied_readback.mask -eq $mask -and $s.previous.group -eq 0 -and $s.previous.mask -gt 0 -and $s.restored_readback.group -eq $s.previous.group -and $s.restored_readback.mask -eq $s.previous.mask -and $s.failure -ceq 'none' -and $s.rollback_failure -ceq 'none' -and $s.fallback_failure -ceq 'none' -and $s.rollback_attempted -eq $false -and $s.fallback_attempted -eq $false) ($role+' actual readback and exact explicit affinity restoration required')
 }
 NoiseRepairRequire ($report.consumer_affinity.thread_id -ne $report.producer_affinity.thread_id) 'independent producer and consumer threads'
}
function ColdHarnessTargets([bool]$motion=$false,[bool]$dense=$false,[bool]$frame=$false) {$targets=@('current_chain','worker_affinity_controls','production_pipeline_cold','score_only_controls','qualification_reader','qualification_reader_controls','candidate_admission_delta','pas','candidate_admission_controls');if($motion){$targets+=@('game_motion_delta','motion_source_regression')};if($dense){$targets+=@('game_delta')};if($frame){$targets+=@('frame_metadata_controls')};return $targets}
function ColdHarnessUnits([bool]$motion=$false,[bool]$dense=$false,[bool]$frame=$false) {$units=@(@{target='current_chain';relative_source='research/prelive_current/chain.cpp'},@{target='worker_affinity_controls';relative_source='research/prelive_current/tests/worker_affinity_tests.cpp'},@{target='production_pipeline_cold';relative_source='research/windows_production_pipeline_v3/main.cpp'},@{target='production_pipeline_cold';relative_source='src/grpc_transport.cpp'},@{target='score_only_controls';relative_source='research/pre_emulator/score_only_controls.cpp'},@{target='score_only_controls';relative_source='src/grpc_transport.cpp'},@{target='qualification_reader';relative_source='research/pre_emulator/qualification_reader/main.cpp'},@{target='qualification_reader_controls';relative_source='research/pre_emulator/qualification_reader/controls.cpp'},@{target='candidate_admission_delta';relative_source='src/candidate_admission.cpp'},@{target='candidate_admission_controls';relative_source='tests/candidate_admission_tests.cpp'});if($motion){$units+=@(@{target='game_motion_delta';relative_source='src/game_motion.cpp'},@{target='motion_source_regression';relative_source='tests/game_tests.cpp'},@{target='motion_source_regression';relative_source='tests/cold_scenario_tests.cpp'},@{target='motion_source_regression';relative_source='generated/runtime_contract.cpp'})};if($dense){$units+=@(@{target='game_delta';relative_source='src/game.cpp'})};if($frame){$units+=@(@{target='frame_metadata_controls';relative_source='research/prelive_current/tests/frame_metadata_tests.cpp'})};return $units}
function ColdHarnessControl($report,[string]$schema,[int]$count,[string]$array='checks',[string]$denominator='assertions') {
 ComponentDeltaColdProducerText $report 'schema' $schema;$null=ComponentDeltaColdProducerInteger $report $denominator $count $count;$null=ComponentDeltaColdProducerInteger $report 'failed' 0 0
 $rows=ComponentDeltaColdProducerField $report $array;NoiseRepairRequire ($rows -is [array] -and $rows.Count -eq $count) 'exact cold control denominator'
 $ids=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($row in $rows){NoiseRepairRequire ($row.id -is [string] -and $row.id.Length -gt 0 -and $ids.Add($row.id)) 'unique cold control id';ComponentDeltaColdProducerBool $row 'pass' $true}
 if(NoiseRepairProperty $report 'pass'){ComponentDeltaColdProducerBool $report 'pass' $true}
 $null=ComponentDeltaColdProducerInteger $report 'device_operations' 0 0
 foreach($field in @('live_ready','qualification_issued','cost_qualification')){if(NoiseRepairProperty $report $field){ComponentDeltaColdProducerBool $report $field $false}}
}
function ColdHarnessFields($value,$names,[string]$label){NoiseRepairRequire (@(Compare-Object (@($value.PSObject.Properties.Name)|Sort-Object) (@($names)|Sort-Object)).Count -eq 0) ('exact '+$label+' fields')}
function ColdHarnessDenseGTest($report){
 ComponentDeltaDenseGTest $report
 NoiseRepairRequire ($report.testsuites -is [array] -and $report.testsuites.Count -le 12) 'Dense GTest suite array'
 $total=0;foreach($suite in $report.testsuites){foreach($field in @('failures','errors','disabled')){$null=ComponentDeltaColdProducerInteger $suite $field 0 0};$count=ComponentDeltaColdProducerInteger $suite 'tests' 1 12;NoiseRepairRequire ($suite.testsuite -is [array] -and $suite.testsuite.Count -eq $count) 'Dense suite exact denominator';$total+=$count;foreach($case in $suite.testsuite){foreach($field in @('failures','errors')){if(NoiseRepairProperty $case $field){NoiseRepairRequire ($case.$field -is [array] -and $case.$field.Count -eq 0) 'Dense case has no failure/error records'}}}}
 NoiseRepairRequire ($total -eq 12) 'Dense GTest exactly twelve completed cases'
}
function ColdHarnessNative($proof,$required,$binary=$null,$argv=$null) {
 NoiseRepairRequire (@($proof.PSObject.Properties).Count -eq 8) 'complete new Native8 projection'
 foreach($field in @('specification','input_closure','result','verification','freeze','state','contract','command')){NoiseRepairCheckBinding $proof.$field}
 NoiseRepairCheckNativeReceipt $proof.verification
 $spec=NoiseRepairReadBinding $proof.specification;$stage=$spec.stage
 foreach($pair in @(@('input_closure',(Join-Path $pkg ($stage+'-input-closure-verification.json'))),@('result',(Join-Path $out ($stage+'/'+$stage+'-result.json'))),@('freeze',(Join-Path $out ($stage+'/freeze.json'))),@('state',(Join-Path $out ($stage+'/state.json'))),@('contract',(Join-Path $out ($stage+'/contract.json'))),@('command',(Join-Path $out ($stage+'/'+$stage+'-command.json'))))){NoiseRepairSameBinding $proof.($pair[0]) (Binding $pair[1]) 'actual Native8 projection path/bytes'}
 NoiseRepairRequireNativeInputs $proof.verification $required
 NoiseRepairSameValue $spec $script:noiseRepairSpecs[$stage] 'actual frozen specification content'
 $frozen=NoiseRepairReadBinding $proof.freeze;NoiseRepairRequire (@($frozen.files|Where-Object {$_.path -and [string]::Equals($_.path,$proof.specification.path,[StringComparison]::OrdinalIgnoreCase) -and $_.bytes -eq $proof.specification.bytes -and $_.sha256 -ceq $proof.specification.sha256}).Count -eq 1) 'provided Native8 spec actually frozen'
 if($null -ne $binary){NoiseRepairRequire ([string]::Equals([IO.Path]::GetFullPath($spec.exe),[IO.Path]::GetFullPath($binary.path),[StringComparison]::OrdinalIgnoreCase)) 'actual new native executable';NoiseRepairSameValue @($spec.argv) @($argv) 'exact new native argv'}
 return $spec
}
function LoadColdHarnessDeltaBuild($bindingPin,$delta) {
 $baseCommit='12ec0550e882e3b06751d67fa67c43dda3e9312b';$originalCommit='aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de'
 $head=(& git -C $repo rev-parse HEAD);NoiseRepairRequire ($LASTEXITCODE -eq 0 -and $head -ceq $delta.source_commit -and (& git -C $repo branch --show-current) -ceq 'main' -and -not (& git -C $repo status --porcelain)) 'exact clean current main cold delta'
 ComponentDeltaColdProducerText $delta 'configuration' 'release';ComponentDeltaColdProducerText $delta 'base_source_commit' $baseCommit
 foreach($key in @('cold_harness_delta','formal_libraries_preserved','base_app_object_reused')){ComponentDeltaColdProducerBool $delta $key $true};foreach($key in @('formal_rebuilt','whole_candidate_qualification','cost_qualification','live_ready','qualification_issued')){ComponentDeltaColdProducerBool $delta $key $false};$null=ComponentDeltaColdProducerInteger $delta 'device_operations' 0 0
 $plan=NoiseRepairReadBinding $delta.core_build_plan;ComponentDeltaColdProducerText $plan 'schema' 'pas.cold-producer-eligibility-delta-plan.v1'
 $placementSchema='pas.cold-worker-placement.v3';$placementPolicy='highest_raw_efficiency_class_disjoint_balanced_core_lowest_lps'
 ComponentDeltaColdProducerText $delta 'producer_eligibility_policy' $placementPolicy;ComponentDeltaColdProducerText $plan 'producer_eligibility_policy' $placementPolicy
 foreach($key in @('source_commit','base_source_commit','configuration','targets','translation_unit_count','translation_units','imported_static_libraries')){NoiseRepairSameValue $delta.$key $plan.$key ('cold plan '+$key)}
 $hasMotion=NoiseRepairProperty $delta 'motion_source_delta';NoiseRepairRequire ($hasMotion -eq (NoiseRepairProperty $plan 'motion_source_delta')) 'plan/build motion branch presence exact'
 $hasDense=NoiseRepairProperty $delta 'dense_source_delta';$hasFrame=NoiseRepairProperty $delta 'frame_metadata_delta'
 NoiseRepairRequire ($hasDense -eq (NoiseRepairProperty $plan 'dense_source_delta') -and $hasFrame -eq (NoiseRepairProperty $plan 'frame_metadata_delta')) 'plan/build Dense and Frame branch presence exact'
 NoiseRepairRequire (-not ($hasDense -or $hasFrame) -or $hasMotion) 'performance deltas require preserved motion branch'
 $freshCount=$(if($hasMotion){14}else{10})+[int]$hasDense+[int]$hasFrame;$expectedTargets=@(ColdHarnessTargets $hasMotion $hasDense $hasFrame)
 NoiseRepairSameValue @($delta.targets) $expectedTargets 'literal cold targets';$null=ComponentDeltaColdProducerInteger $delta 'translation_unit_count' $freshCount $freshCount
 $splitBuild=$hasDense -or $hasFrame;$buildProofs=@($delta.build_process);$buildGroups=@(@{targets=$expectedTargets})
 if($splitBuild){
  $buildGroups=@(ComponentDeltaPerformanceBuildGroups $hasDense $hasFrame)
  foreach($owner in @($delta,$plan)){NoiseRepairSameValue $owner.build_target_groups $buildGroups 'exact two performance build groups';$null=ComponentDeltaColdProducerInteger $owner 'expected_native_build_stages' 2 2}
  foreach($field in @('build_processes','build_receipts','build_input_closures','build_prior_outputs')){NoiseRepairRequire (@($delta.$field).Count -eq 2) ('complete two phase '+$field)}
  $buildProofs=@($delta.build_processes);NoiseRepairSameValue $delta.build_process $buildProofs[-1] 'legacy build projection is final actual phase';NoiseRepairSameBinding $delta.compile_receipt $buildProofs[-1].verification 'final actual compile receipt projection';NoiseRepairSameBinding $delta.input_closure $buildProofs[-1].input_closure 'final actual closure projection'
  NoiseRepairRequire (@($delta.build_prior_outputs[0]).Count -eq 0) 'first phase imports no fresh object or executable'
  for($phase=0;$phase -lt 2;$phase++){NoiseRepairSameBinding $delta.build_receipts[$phase] $buildProofs[$phase].verification 'ordered actual phase verification';NoiseRepairSameBinding $delta.build_input_closures[$phase] $buildProofs[$phase].input_closure 'ordered actual phase closure'}
 }else{foreach($owner in @($delta,$plan)){foreach($field in @('build_target_groups','build_processes','build_receipts','build_input_closures','build_prior_outputs','expected_native_build_stages','compile_receipt','input_closure')){NoiseRepairRequire (-not (NoiseRepairProperty $owner $field)) 'split fields cannot enter legacy cold build'}}}
 $nativeCount=$(if($hasMotion){10}else{9})+[int]$hasDense+[int]$hasFrame+[int]$splitBuild;$null=ComponentDeltaColdProducerInteger $delta 'actual_native_stage_count' $nativeCount $nativeCount;$null=ComponentDeltaColdProducerInteger $delta 'actual_native8_pin_count' (8*$nativeCount) (8*$nativeCount)
 $expected=ColdHarnessUnits $hasMotion $hasDense $hasFrame;NoiseRepairRequire (@($delta.translation_units).Count -eq $freshCount) 'literal cold occurrences';for($index=0;$index -lt $freshCount;$index++){NoiseRepairRequire ($delta.translation_units[$index].target -ceq $expected[$index].target -and $delta.translation_units[$index].relative_source -ceq $expected[$index].relative_source) 'ordered cold source/target occurrences'}
 $cpu=NoiseRepairReadBinding $delta.base_cpu_build;$production=NoiseRepairReadBinding $delta.base_production_build
 NoiseRepairRequire ($cpu.schema -ceq 'pas.narrow-noise-repair-build.v1' -and $production.schema -ceq 'pas.cooperative-build.v1' -and $cpu.source_commit -ceq $baseCommit -and $production.source_commit -ceq $baseCommit -and $cpu.configuration -ceq 'release' -and $production.configuration -ceq 'release') 'original55 formal CPU/production separate identities'
 NoiseRepairSameBinding $production.noise_repair_build $delta.base_cpu_build 'original55 production/CPU';NoiseRepairSameBinding $production.source_manifest $cpu.source_manifest 'original55 common source';NoiseRepairSameBinding $production.dependency_manifest $cpu.dependency_manifest 'original55 common dependencies'
 $formal=NoiseRepairReadBinding $cpu.source_manifest;$current=NoiseRepairReadBinding $delta.source_manifest
 NoiseRepairRequire ($formal.source_commit -ceq $baseCommit -and $formal.dirty -eq $false -and $current.source_commit -ceq $head -and $current.dirty -eq $false -and $current.schema -ceq 'pas.cooperative-source.v1') 'formal55/current clean source manifests'
 $currentMap=@{};foreach($pin in $current.files){$relative=NoiseRepairRelative $pin.path;NoiseRepairRequire (-not $currentMap.ContainsKey($relative)) 'unique current source member';$currentMap[$relative]=$pin;NoiseRepairAddInputs @($pin)}
 $tracked=@(& git -c core.quotePath=false -C $repo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -cmatch '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'})
 NoiseRepairRequire ($LASTEXITCODE -eq 0 -and @(Compare-Object ($tracked|Sort-Object) ($currentMap.Keys|Sort-Object)).Count -eq 0) 'full current tracked code source manifest'
 $snapshot=NoiseRepairReadBinding $delta.base_source_snapshot;$projection=NoiseRepairReadBinding $delta.base_source_projection
 NoiseRepairRequire ($snapshot.source_commit -ceq $baseCommit -and $projection.schema -ceq 'pas.lossless-source-path-projection.v1' -and $projection.source_commit -ceq $baseCommit) 'original55 snapshot/short projection identity';NoiseRepairSameBinding $snapshot.source_manifest $cpu.source_manifest 'snapshot/original formal source';NoiseRepairSameBinding $projection.original_snapshot $delta.base_source_snapshot 'projection/original snapshot';NoiseRepairSameBinding $projection.original_source_manifest $cpu.source_manifest 'projection/original source'
 $projected=@{};foreach($row in $projection.files){NoiseRepairRequire (-not $projected.ContainsKey($row.relative_path) -and $row.copy.bytes -eq $row.original_copy.bytes -and $row.copy.sha256 -ceq $row.original_copy.sha256 -and $row.copy.path.Length -le 259) 'unique exact short source projection';$projected[$row.relative_path]=$row;NoiseRepairAddInputs @($row.copy)}
 $baseMap=@{};foreach($row in $snapshot.files){$relative=NoiseRepairRelative $row.original.path;NoiseRepairRequire (-not $baseMap.ContainsKey($relative) -and $row.copy.bytes -eq $row.original.bytes -and $row.copy.sha256 -ceq $row.original.sha256) 'lossless original55 snapshot mapping';$baseMap[$relative]=$row.original;if($projected.ContainsKey($relative)){NoiseRepairSameBinding $projected[$relative].original_copy $row.copy 'original55 projected copy'}else{NoiseRepairAddInputs @($row.copy)}}
 NoiseRepairRequire ($projected.Count -eq @($formal.files).Count) 'complete original55 source projection denominator'
 if($hasMotion){
  $motion=$delta.motion_source_delta;$staticMotion=$plan.motion_source_delta
  ComponentDeltaColdProducerText $motion 'schema' 'pas.cold-motion-source-delta.v1';ComponentDeltaColdProducerText $motion 'source_commit' $head;ComponentDeltaColdProducerText $motion 'base_source_commit' $baseCommit;ComponentDeltaColdProducerText $motion 'source_path' 'src/game_motion.cpp';ComponentDeltaColdProducerText $motion 'literal_transform' 'ridge_guarded_gray_short_circuit.v1'
  foreach($key in @('formal_libraries_rebuilt','qualification_issued','cost_qualification','live_ready')){ComponentDeltaColdProducerBool $motion $key $false};$null=ComponentDeltaColdProducerInteger $motion 'fresh_formal_translation_units' 1 1;$null=ComponentDeltaColdProducerInteger $motion 'device_operations' 0 0
  $formalCount=1+[int]$hasDense;foreach($owner in @($delta,$plan)){ComponentDeltaColdProducerBool $owner 'formal_libraries_rebuilt' $false;$null=ComponentDeltaColdProducerInteger $owner 'fresh_formal_translation_units' $formalCount $formalCount}
  $staticNames=@('schema','source_commit','base_source_commit','source_path','literal_transform','base_source','current_source','compile_source','original_archive_members','consumer_targets','formal_libraries_rebuilt','fresh_formal_translation_units','qualification_issued','cost_qualification','live_ready','device_operations')
  NoiseRepairRequire (@(Compare-Object (@($staticMotion.PSObject.Properties.Name)|Sort-Object) (@($staticNames)+@('regression')|Sort-Object)).Count -eq 0 -and @(Compare-Object (@($motion.PSObject.Properties.Name)|Sort-Object) (@($staticNames)+@('regression','new_object','consumer_images')|Sort-Object)).Count -eq 0) 'exact static/actual motion fields'
  foreach($key in $staticNames){NoiseRepairSameValue $motion.$key $staticMotion.$key ('static/actual motion '+$key)}
  NoiseRepairSameBinding $motion.base_source $projected['src/game_motion.cpp'].copy 'preserved original motion copy';NoiseRepairSameBinding $motion.current_source $currentMap['src/game_motion.cpp'] 'actual current motion source';NoiseRepairRequire ($motion.base_source.bytes -eq $baseMap['src/game_motion.cpp'].bytes -and $motion.base_source.sha256 -ceq $baseMap['src/game_motion.cpp'].sha256) 'original motion copy SHA/source55 join'
  ComponentDeltaMotionSourceText ([IO.File]::ReadAllText($motion.base_source.path)) ([IO.File]::ReadAllText($motion.current_source.path))
  NoiseRepairRequire ($motion.current_source.bytes -eq [Text.Encoding]::UTF8.GetByteCount([IO.File]::ReadAllText($motion.current_source.path))) 'motion LF source without BOM'
  NoiseRepairSameValue @($motion.consumer_targets) @(ComponentDeltaMotionTargets) 'five exact motion consumers'
  $reg=$motion.regression;$staticReg=$staticMotion.regression;$regNames=@('target','test_sources','runtime_contract_source','runtime_contract_prefix','cases','case_names','filter')
  NoiseRepairRequire (@(Compare-Object (@($staticReg.PSObject.Properties.Name)|Sort-Object) ($regNames|Sort-Object)).Count -eq 0 -and @(Compare-Object (@($reg.PSObject.Properties.Name)|Sort-Object) (($regNames+@('binary','process','report'))|Sort-Object)).Count -eq 0) 'exact static/actual motion regression fields'
  foreach($key in $regNames){NoiseRepairSameValue $reg.$key $staticReg.$key ('static/actual motion regression '+$key)}
  ComponentDeltaColdProducerText $reg 'target' 'motion_source_regression';ComponentDeltaColdProducerText $reg 'filter' (ComponentDeltaMotionFilter);$null=ComponentDeltaColdProducerInteger $reg 'cases' 26 26;NoiseRepairSameValue @($reg.case_names) @(ComponentDeltaMotionCases) 'literal original 26 regression case names'
  NoiseRepairRequire (@($reg.test_sources).Count -eq 2) 'two exact original regression source TUs';$testIndex=0;foreach($relative in @('tests/game_tests.cpp','tests/cold_scenario_tests.cpp')){NoiseRepairSameBinding $reg.test_sources[$testIndex++] $currentMap[$relative] 'motion regression current source';NoiseRepairRequire ($currentMap[$relative].bytes -eq $baseMap[$relative].bytes -and $currentMap[$relative].sha256 -ceq $baseMap[$relative].sha256) 'motion regression original source55 bytes unchanged'}
  NoiseRepairSameBinding $reg.runtime_contract_prefix $currentMap['src/runtime.cpp'] 'unchanged full runtime source prefix';NoiseRepairRequire ($reg.runtime_contract_prefix.bytes -eq $baseMap['src/runtime.cpp'].bytes -and $reg.runtime_contract_prefix.sha256 -ceq $baseMap['src/runtime.cpp'].sha256) 'original pure runtime source55 byte identity'
  NoiseRepairAddInputs @($reg.runtime_contract_source,$reg.runtime_contract_prefix,$motion.base_source,$motion.current_source);NoiseRepairRequire ([IO.File]::ReadAllText($reg.runtime_contract_source.path) -ceq (ComponentDeltaMotionRuntimeText ([IO.File]::ReadAllText($reg.runtime_contract_prefix.path)))) 'generated runtime exact original pure prefix and namespace suffix'
 }
 if($hasDense){
  $dense=$delta.dense_source_delta;$staticDense=$plan.dense_source_delta
  $denseNames=@('schema','source_commit','base_source_commit','source_path','literal_transform','base_source','current_source','compile_source','original_archive_members','consumer_targets','formal_libraries_rebuilt','fresh_formal_translation_units','qualification_issued','cost_qualification','live_ready','device_operations')
  ColdHarnessFields $staticDense ($denseNames+@('regression')) 'static Dense';ColdHarnessFields $dense ($denseNames+@('regression','new_object','consumer_images')) 'actual Dense'
  foreach($key in $denseNames){NoiseRepairSameValue $dense.$key $staticDense.$key ('Dense static/actual '+$key)}
  ComponentDeltaColdProducerText $dense 'schema' 'pas.cold-dense-source-delta.v1';ComponentDeltaColdProducerText $dense 'source_commit' $head;ComponentDeltaColdProducerText $dense 'base_source_commit' $baseCommit;ComponentDeltaColdProducerText $dense 'source_path' 'src/game.cpp';ComponentDeltaColdProducerText $dense 'literal_transform' 'white_note_decode_pca_short_circuit.v1'
  foreach($key in @('formal_libraries_rebuilt','qualification_issued','cost_qualification','live_ready')){ComponentDeltaColdProducerBool $dense $key $false};$null=ComponentDeltaColdProducerInteger $dense 'fresh_formal_translation_units' 1 1;$null=ComponentDeltaColdProducerInteger $dense 'device_operations' 0 0
  NoiseRepairRequire ($projected.ContainsKey('src/game.cpp')) 'preserved original game payload copy'
  NoiseRepairSameBinding $dense.base_source $projected['src/game.cpp'].copy 'preserved original game copy';NoiseRepairSameBinding $dense.current_source $currentMap['src/game.cpp'] 'actual current game source';NoiseRepairRequire ($dense.base_source.bytes -eq $baseMap['src/game.cpp'].bytes -and $dense.base_source.sha256 -ceq $baseMap['src/game.cpp'].sha256) 'game copy/source55 original SHA join'
  NoiseRepairAddInputs @($dense.base_source,$dense.current_source);ComponentDeltaDenseSourceText $dense.base_source $dense.current_source
  NoiseRepairSameValue @($dense.consumer_targets) @(ComponentDeltaDenseTargets) 'six exact Dense consumers'
  $denseReg=$dense.regression;$denseStaticReg=$staticDense.regression;$denseRegNames=@('target','test_sources','runtime_contract_source','runtime_contract_prefix','cases','case_names','filter')
  ColdHarnessFields $denseStaticReg $denseRegNames 'static Dense regression';ColdHarnessFields $denseReg ($denseRegNames+@('binary','process','report')) 'actual Dense regression'
  foreach($key in $denseRegNames){NoiseRepairSameValue $denseReg.$key $denseStaticReg.$key ('Dense regression static/actual '+$key)}
  ComponentDeltaColdProducerText $denseReg 'target' 'motion_source_regression';ComponentDeltaColdProducerText $denseReg 'filter' (ComponentDeltaDenseFilter);$null=ComponentDeltaColdProducerInteger $denseReg 'cases' 12 12;NoiseRepairSameValue @($denseReg.case_names) @(ComponentDeltaDenseCases) 'separate exact original twelve observer cases'
  foreach($key in @('test_sources','runtime_contract_source','runtime_contract_prefix')){NoiseRepairSameValue $denseReg.$key $reg.$key ('same preserved motion/Dense regression '+$key)}
 }
 if($hasFrame){
  $frame=$delta.frame_metadata_delta;$staticFrame=$plan.frame_metadata_delta
  $frameNames=@('schema','policy','source_commit','base_source_commit','base_chain_source','current_chain_source','helper_source','tests_source','source_dirty','qualification_issued','cost_qualification','live_ready','device_operations')
  ColdHarnessFields $staticFrame ($frameNames+@('frame_controls')) 'static Frame';ColdHarnessFields $frame ($frameNames+@('frame_controls')) 'actual Frame'
  foreach($key in $frameNames){NoiseRepairSameValue $frame.$key $staticFrame.$key ('Frame static/actual '+$key)}
  ComponentDeltaColdProducerText $frame 'schema' 'pas.cold-frame-metadata-delta.v1';ComponentDeltaColdProducerText $frame 'policy' 'fixed_metadata_before_epoch.v1';ComponentDeltaColdProducerText $frame 'source_commit' $head;ComponentDeltaColdProducerText $frame 'base_source_commit' '3ee69edc85acb6b9b968424374a376f351d0e2dd'
  foreach($key in @('source_dirty','qualification_issued','cost_qualification','live_ready')){ComponentDeltaColdProducerBool $frame $key $false};$null=ComponentDeltaColdProducerInteger $frame 'device_operations' 0 0
  NoiseRepairSameBinding $frame.current_chain_source $currentMap['research/prelive_current/chain.cpp'] 'Frame current chain source';NoiseRepairSameBinding $frame.helper_source $currentMap['research/prelive_current/frame_metadata.hpp'] 'Frame actual helper source';NoiseRepairSameBinding $frame.tests_source $currentMap['research/prelive_current/tests/frame_metadata_tests.cpp'] 'Frame actual tests source'
  $frame80Pin=Binding (Join-Path $repo 'out/prelive-producer-eligibility-evaluation-main80-e1/build-binding.json');NoiseRepairRequire ($frame80Pin.sha256 -ceq '7478f48f0cb7386836bfddc0bffbdf83c55bdf7f32727ae95bcb107b8a2bb058') 'actual original main80 build pin for Frame source'
  $frame80=NoiseRepairReadBinding $frame80Pin;ComponentDeltaColdProducerText $frame80 'source_commit' $frame.base_source_commit
  $frame80Source=NoiseRepairReadBinding $frame80.source_manifest;$frame80Snapshot=NoiseRepairReadBinding $frame80.source_snapshot;NoiseRepairRequire ($frame80Source.source_commit -ceq $frame.base_source_commit -and $frame80Snapshot.source_commit -ceq $frame.base_source_commit) 'Frame original80 source/snapshot commit';ComponentDeltaColdProducerText $frame80Source 'schema' 'pas.cooperative-source.v1';ComponentDeltaColdProducerBool $frame80Source 'dirty' $false;ComponentDeltaColdProducerText $frame80Snapshot 'schema' 'pas.lossless-diagnostic-source-snapshot.v1';foreach($flag in @('all_copy_SHAs_verified','originals_rechecked')){ComponentDeltaColdProducerBool $frame80Snapshot $flag $true};NoiseRepairSameBinding $frame80Snapshot.source_manifest $frame80.source_manifest 'Frame original80 snapshot/manifest'
  $frame80Original=@($frame80Source.files|Where-Object {(NoiseRepairRelative $_.path) -ceq 'research/prelive_current/chain.cpp'});NoiseRepairRequire ($frame80Original.Count -eq 1) 'one original80 chain manifest member'
  $frame80Copies=@($frame80Snapshot.files|Where-Object {$_.original.path -ceq $frame80Original[0].path});NoiseRepairRequire ($frame80Copies.Count -eq 1) 'one original80 chain snapshot copy';NoiseRepairSameBinding $frame80Copies[0].original $frame80Original[0] 'original80 chain metadata pin';NoiseRepairRequire ($frame80Copies[0].copy.bytes -eq $frame80Original[0].bytes -and $frame80Copies[0].copy.sha256 -ceq $frame80Original[0].sha256) 'original80 chain copy byte identity';NoiseRepairSameBinding $frame.base_chain_source $frame80Copies[0].copy 'Frame uses exact original80 immutable short copy'
  NoiseRepairAddInputs @($frame.base_chain_source,$frame.current_chain_source,$frame.helper_source,$frame.tests_source,$frame80Pin,$frame80.source_manifest,$frame80.source_snapshot);ComponentDeltaFrameSourceText $frame.base_chain_source $frame.current_chain_source
  $frameControl=$frame.frame_controls;$staticFrameControl=$staticFrame.frame_controls;$frameControlNames=@('target','schema','assertions')
  ColdHarnessFields $staticFrameControl $frameControlNames 'static Frame controls';ColdHarnessFields $frameControl ($frameControlNames+@('binary','report','process')) 'actual Frame controls'
  foreach($key in $frameControlNames){NoiseRepairSameValue $frameControl.$key $staticFrameControl.$key ('Frame controls static/actual '+$key)}
  ComponentDeltaColdProducerText $frameControl 'target' 'frame_metadata_controls';ComponentDeltaColdProducerText $frameControl 'schema' 'pas.cold-frame-metadata-reuse-controls.v1';$null=ComponentDeltaColdProducerInteger $frameControl 'assertions' 8 8
 }else{NoiseRepairRequire ([IO.File]::ReadAllText($currentMap['research/prelive_current/chain.cpp'].path) -notmatch '(?m)^#include "frame_metadata[.]hpp"\r?$') 'Frame implementation requires explicit source/control declaration'}
 foreach($old in $formal.files){$relative=NoiseRepairRelative $old.path;NoiseRepairRequire ($baseMap.ContainsKey($relative)) 'every formal55 source preserved';NoiseRepairSameBinding $baseMap[$relative] $old 'original55 immutable source member';if(($relative -cmatch '^(src|include|apps)/' -or $relative -ceq 'CMakeLists.txt') -and $relative -cne 'src/candidate_admission.cpp' -and -not ($hasMotion -and $relative -ceq 'src/game_motion.cpp') -and -not ($hasDense -and $relative -ceq 'src/game.cpp')){NoiseRepairRequire ($currentMap.ContainsKey($relative) -and $currentMap[$relative].bytes -eq $old.bytes -and $currentMap[$relative].sha256 -ceq $old.sha256) 'formal strategy/manual/ABI bytes unchanged except validated explicit overlays'}}
 foreach($relative in $currentMap.Keys|Where-Object {$_ -cmatch '^(src|include|apps)/' -or $_ -ceq 'CMakeLists.txt'}){NoiseRepairRequire ($baseMap.ContainsKey($relative)) 'new formal source/ABI additions cannot enter cold-only overlay'}
 $freshSnapshot=NoiseRepairReadBinding $delta.source_snapshot;NoiseRepairRequire ($freshSnapshot.source_commit -ceq $head) 'current lossless compile snapshot';NoiseRepairSameBinding $freshSnapshot.source_manifest $delta.source_manifest 'current snapshot/manifest';$copies=@{};foreach($row in $freshSnapshot.files){$relative=NoiseRepairRelative $row.original.path;NoiseRepairRequire ($currentMap.ContainsKey($relative) -and -not $copies.ContainsKey($relative) -and $row.copy.bytes -eq $row.original.bytes -and $row.copy.sha256 -ceq $row.original.sha256 -and $row.copy.path.Length -le 259) 'fresh source lossless short copies';NoiseRepairSameBinding $row.original $currentMap[$relative] 'current original source';$copies[$relative]=$row.copy;NoiseRepairAddInputs @($row.copy)};NoiseRepairRequire ($copies.Count -eq $currentMap.Count) 'full current compile copy denominator'
 foreach($unit in $delta.translation_units){if($hasMotion -and $unit.relative_source -ceq 'generated/runtime_contract.cpp'){NoiseRepairSameBinding $unit.original_source $reg.runtime_contract_source 'generated runtime original pin';NoiseRepairSameBinding $unit.source $reg.runtime_contract_source 'generated runtime compile pin';continue};NoiseRepairSameBinding $unit.original_source $currentMap[$unit.relative_source] 'cold occurrence original';NoiseRepairSameBinding $unit.source $copies[$unit.relative_source] 'cold occurrence actual compiled copy'}
 if($hasMotion){NoiseRepairSameBinding $motion.compile_source $copies['src/game_motion.cpp'] 'fresh motion compile copy'}
 if($hasDense){NoiseRepairSameBinding $dense.compile_source $copies['src/game.cpp'] 'fresh exact game compile copy'}
 $inventory=NoiseRepairReadBinding $cpu.original_main32_inventory;$old=NoiseRepairReadBinding $cpu.base_release_build;$evaluation=NoiseRepairReadBinding $cpu.base_release_evaluation
 NoiseRepairRequire ($inventory.source_commit -ceq $originalCommit -and $old.source_commit -ceq $originalCommit -and $evaluation.source_commit -ceq $originalCommit -and $evaluation.affected_cold_pass -eq $true -and @($evaluation.tests).Count -eq 25) 'preserved original32 qualification prerequisites'
 $release=@($inventory.cold|Where-Object mode -ceq 'release');NoiseRepairRequire ($release.Count -eq 1) 'original Release prerequisite';NoiseRepairSameBinding $release[0].build $cpu.base_release_build 'original Release build';NoiseRepairSameBinding $release[0].evaluation $cpu.base_release_evaluation 'original Release evaluation'
 $base32Snapshot=NoiseRepairReadBinding $inventory.source_snapshot;$oldSources=@{};foreach($row in $base32Snapshot.files){NoiseRepairRequire ($row.original.bytes -eq $row.copy.bytes -and $row.original.sha256 -ceq $row.copy.sha256) 'original32 snapshot identity';$oldSources[(NoiseRepairRelative $row.original.path)]=$row.original;NoiseRepairAddInputs @($row.copy)}
 $oldManifest=NoiseRepairReadBinding $old.source_manifest;foreach($pin in $oldManifest.files){NoiseRepairSameBinding $oldSources[(NoiseRepairRelative $pin.path)] $pin 'original32 immutable source'}
 $roles=@('cpu_current_rails','cpu_current_front','cpu_exact_bvi_v3','cpu_own_bridge','cpu_pas_core','production_pas_core','production_pas_current_candidate','production_pas_proto','verification_pas_emulator','verification_pas_bench');NoiseRepairRequire (@($delta.imported_static_libraries).Count -eq 10) 'ten explicit imported roles';$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 foreach($row in $delta.imported_static_libraries){NoiseRepairRequire ($row.role -cin $roles -and $seen.Add($row.role)) 'unique exact imported library role';ComponentDeltaColdProducerBool $row 'historical_import' $true;ComponentDeltaColdProducerBool $row 'freshly_rebuilt' $false
  if($row.role -cin @('cpu_current_rails','cpu_current_front','cpu_exact_bvi_v3')){$b=$old;$bp=$cpu.base_release_build;$sm=$old.source_manifest;$map=$oldSources;$compiled=$originalCommit}elseif($row.role.StartsWith('cpu_')){$b=$cpu;$bp=$delta.base_cpu_build;$sm=$cpu.source_manifest;$map=$baseMap;$compiled=$baseCommit}else{$b=$production;$bp=$delta.base_production_build;$sm=$production.source_manifest;$map=$baseMap;$compiled=$baseCommit}
  NoiseRepairRequire ($row.compiled_source_commit -ceq $compiled) 'library actual original compile commit';NoiseRepairSameBinding $row.build_binding $bp 'library original build';NoiseRepairSameBinding $row.source_manifest $sm 'library original source manifest'
  $outputs=@($b.files)+@($b.compiled_outputs);foreach($pin in @($row.binary,$row.compile_graph)){NoiseRepairRequire (@($outputs|Where-Object {$_.path -and [string]::Equals($_.path,$pin.path,[StringComparison]::OrdinalIgnoreCase) -and $_.bytes -eq $pin.bytes -and $_.sha256 -ceq $pin.sha256}).Count -gt 0) 'library/graph sealed original output';NoiseRepairAddInputs @($pin)}
  $text=(Get-Content -LiteralPath $row.compile_graph.path -Raw).Replace('\','/').Replace('$:',':').Replace('$ ',' ');$relative=[IO.Path]::GetRelativePath($b.build_root,$row.binary.path).Replace('\','/');$edge=@([regex]::Matches($text,'(?m)^build '+[regex]::Escape($relative)+': CXX_STATIC_LIBRARY_LINKER[^ ]+ ([^\r\n]+)'));NoiseRepairRequire ($edge.Count -eq 1) 'actual original archive graph';$members=@(($edge[0].Groups[1].Value -split '\|')[0].Trim() -split '\s+');NoiseRepairRequire ($members.Count -eq $row.translation_unit_count -and @($row.translation_units).Count -eq $members.Count -and @($members|Sort-Object -Unique).Count -eq $members.Count) 'original archive exact member denominator'
  $memberSeen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($tuple in $row.translation_units){NoiseRepairRequire ($tuple.archive_member -cin $members -and $memberSeen.Add($tuple.archive_member)) 'exact member tuple';$expectedObject=Join-Path $b.build_root $tuple.archive_member;NoiseRepairRequire ([string]::Equals([IO.Path]::GetFullPath($expectedObject),[IO.Path]::GetFullPath($tuple.historical_object.path),[StringComparison]::OrdinalIgnoreCase)) 'archive/object path';NoiseRepairAddInputs @($tuple.historical_object)
   if(-not $tuple.relative_source.StartsWith('generated/')){NoiseRepairSameBinding $tuple.original_source $map[$tuple.relative_source] 'original compiled TU source manifest';if($tuple.relative_source -cne 'src/candidate_admission.cpp' -and -not ($hasMotion -and $tuple.relative_source -ceq 'src/game_motion.cpp' -and $row.role -cin @('cpu_pas_core','production_pas_core')) -and -not ($hasDense -and $tuple.relative_source -ceq 'src/game.cpp' -and $row.role -cin @('cpu_pas_core','production_pas_core'))){NoiseRepairRequire ($currentMap[$tuple.relative_source].bytes -eq $tuple.original_source.bytes -and $currentMap[$tuple.relative_source].sha256 -ceq $tuple.original_source.sha256) 'every effective imported TU unchanged'}}else{NoiseRepairAddInputs @($tuple.original_source)}
   $compile=@([regex]::Matches($text,'(?m)^build '+[regex]::Escape($tuple.archive_member)+': CXX_COMPILER[^ ]+ ([^\r\n]+?) \|\|'));NoiseRepairRequire ($compile.Count -eq 1 -and [string]::Equals([IO.Path]::GetFullPath($compile[0].Groups[1].Value),[IO.Path]::GetFullPath($tuple.actual_compile_source_path),[StringComparison]::OrdinalIgnoreCase)) 'actual historical compile-source/member tuple'
  }
 }
 if($hasMotion){
  NoiseRepairRequire (@($motion.original_archive_members).Count -eq 2) 'two original motion core members';$index=0
  foreach($role in @('cpu_pas_core','production_pas_core')){$row=@($delta.imported_static_libraries|Where-Object role -ceq $role);
   NoiseRepairRequire ($row.Count -eq 1 -and $row[0].replaced_archive_sources -is [array]) 'declared original core replacement sources'
   if($hasDense){NoiseRepairSameValue @($row[0].replaced_archive_sources) @('src/game_motion.cpp','src/game.cpp') 'exact motion/game original replacement array';NoiseRepairRequire (-not (NoiseRepairProperty $row[0] 'replacement_target')) 'Dense has no ambiguous scalar replacement target';NoiseRepairSameValue $row[0].replacement_targets @([pscustomobject]@{source='src/game_motion.cpp';target='game_motion_delta'},[pscustomobject]@{source='src/game.cpp';target='game_delta'}) 'exact motion/game per-source replacement map'}
   else{NoiseRepairSameValue @($row[0].replaced_archive_sources) @('src/game_motion.cpp') 'exact historical motion replacement source';ComponentDeltaColdProducerText $row[0] 'replacement_target' 'game_motion_delta'}
   $tuple=@($row[0].translation_units|Where-Object relative_source -ceq 'src/game_motion.cpp');NoiseRepairRequire ($tuple.Count -eq 1) 'one actual historical motion core member';$member=$motion.original_archive_members[$index++];ComponentDeltaColdProducerText $member 'role' $role;NoiseRepairSameBinding $member.archive $row[0].binary 'preserved motion archive';NoiseRepairSameBinding $member.member $tuple[0].historical_object 'preserved motion member';NoiseRepairSameBinding $member.source $tuple[0].original_source 'preserved original motion compile source'}
 }
 if($hasDense){
  NoiseRepairRequire (@($dense.original_archive_members).Count -eq 2) 'two original game core members';$index=0
  foreach($role in @('cpu_pas_core','production_pas_core')){$row=@($delta.imported_static_libraries|Where-Object role -ceq $role);$tuple=@($row[0].translation_units|Where-Object relative_source -ceq 'src/game.cpp');NoiseRepairRequire ($row.Count -eq 1 -and $tuple.Count -eq 1 -and $row[0].translation_unit_count -eq 22) 'one original game member of each core22';$member=$dense.original_archive_members[$index++];ColdHarnessFields $member @('role','archive','member','source') 'original game member';ComponentDeltaColdProducerText $member 'role' $role;NoiseRepairSameBinding $member.archive $row[0].binary 'preserved game archive';NoiseRepairSameBinding $member.member $tuple[0].historical_object 'preserved game object';NoiseRepairSameBinding $member.source $tuple[0].original_source 'preserved original game compile source';NoiseRepairSameBinding $member.source $baseMap['src/game.cpp'] 'game member original55 source';NoiseRepairAddInputs @($member.member)}
 }
 foreach($entry in $delta.base_original_build_processes){$null=ColdHarnessNative $entry.Native8 @()}
 foreach($entry in $production.controls){$report=NoiseRepairReadBinding $entry.report;NoiseRepairRequire ($report.failed -eq 0 -and $report.device_operations -eq 0 -and $report.live_ready -eq $false) 'original55 production controls preserved';NoiseRepairCheckNativeReceipt $entry.receipt}
 $oldTests=@();foreach($entry in $evaluation.tests){NoiseRepairReadBinding $entry.report|Out-Null;NoiseRepairCheckNativeReceipt $entry.receipt;$copy=@{};foreach($prop in $entry.PSObject.Properties){$copy[$prop.Name]=$prop.Value};$copy.prerequisite_only=$true;$copy.source_commit=$originalCommit;$oldTests+=@($copy)}
 $selected=NoiseRepairReadBinding $delta.selected_dependency_manifest;ComponentDeltaColdProducerText $selected 'schema' 'pas.cold-producer-eligibility-dependency-selection.v1';ComponentDeltaColdProducerText $selected 'source_commit' $head;ComponentDeltaColdProducerText $selected 'original_dependency_source_commit' $originalCommit
 NoiseRepairSameBinding $selected.original_dependency_manifest $delta.dependency_manifest 'original complete dependency metadata';NoiseRepairSameBinding $delta.dependency_manifest $cpu.dependency_manifest 'original dependency CPU provenance';NoiseRepairSameBinding $selected.base_selected_dependency_manifest $cpu.selected_dependency_manifest 'original CPU selected closure'
 NoiseRepairRequire ([string]::Equals([IO.Path]::GetFullPath($selected.fixed_installed_root),[IO.Path]::GetFullPath((Join-Path $repo 'out/vcpkg_installed/x64-windows')),[StringComparison]::OrdinalIgnoreCase)) 'fixed installed dependency root'
 $completeDependencies=NoiseRepairReadBinding $selected.original_dependency_manifest;$baseSelection=NoiseRepairReadBinding $selected.base_selected_dependency_manifest;$dependencyMap=@{};foreach($pin in $completeDependencies.files){$dependencyMap[[IO.Path]::GetFullPath($pin.path)]=$pin}
 $selectedMap=@{};foreach($pin in $selected.files){$key=[IO.Path]::GetFullPath($pin.path);NoiseRepairRequire ($dependencyMap.ContainsKey($key) -and -not $selectedMap.ContainsKey($key) -and $key.StartsWith([IO.Path]::GetFullPath($selected.fixed_installed_root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'unique actual selected dependency within original scope';NoiseRepairSameBinding $pin $dependencyMap[$key] 'actual selected original dependency';NoiseRepairCheckBinding $pin;$selectedMap[$key]=$pin;if($pin.bytes -le 268435456){NoiseRepairAddInputs @($pin)}}
 $null=ComponentDeltaColdProducerInteger $selected 'file_count' $selectedMap.Count $selectedMap.Count;foreach($pin in $baseSelection.files){$key=[IO.Path]::GetFullPath($pin.path);NoiseRepairRequire ($selectedMap.ContainsKey($key)) 'preserved CPU selected dependency';NoiseRepairSameBinding $selectedMap[$key] $pin 'CPU selected dependency original pin'}
 NoiseRepairAddInputs @($delta.selected_dependency_manifest,$delta.dependency_manifest,$selected.base_selected_dependency_manifest)
 $environment=NoiseRepairReadBinding $delta.compiler_environment;NoiseRepairSameBinding $delta.compiler_environment $cpu.compiler_environment 'original compiler environment';NoiseRepairRequire ($environment.schema -ceq 'pas.msvc-environment-binding.v1' -and $environment.source_commit -ceq $originalCommit) 'actual pinned compiler provenance'
 foreach($pin in @($environment.environment,$environment.verification,$environment.state,$delta.base_app_object)+@($delta.base_generated_inputs)){NoiseRepairAddInputs @($pin)}
 $required=@($delta.selected_dependency_manifest,$delta.source_manifest,$delta.core_build_plan,$delta.base_app_object)+@($selected.files)+@($current.files)+@($copies.Values)+@($delta.imported_static_libraries|ForEach-Object {$_.binary})+@($delta.base_generated_inputs)
 if($hasMotion){$required+=@($reg.runtime_contract_source,$reg.runtime_contract_prefix,$motion.base_source)+@($motion.original_archive_members|ForEach-Object {$_.member})}
 if($hasDense){$required+=@($dense.base_source)+@($dense.original_archive_members|ForEach-Object {$_.member})}
 if($hasFrame){$required+=@($frame.base_chain_source,$frame.current_chain_source,$frame.helper_source,$frame.tests_source)}
 $null=ColdHarnessNative $delta.configure_process $required
 $buildRequired=$required+@($delta.compile_graph)+@($delta.generated_delta_inputs)+@($delta.runtime18_copies)
 NoiseRepairCheckBinding $delta.compile_graph;$ninja=(Get-Content -LiteralPath $delta.compile_graph.path -Raw).Replace('\','/').Replace('$:',':').Replace('$ ',' ')
 $compile=@([regex]::Matches($ninja,'(?m)^build ([^\r\n:]+[.]obj): CXX_COMPILER[^ ]+ ([^\r\n]+?) \|\|'));NoiseRepairRequire ($compile.Count -eq $freshCount -and $ninja -notmatch 'CXX_STATIC_LIBRARY_LINKER') 'actual declared compiles and zero new archives'
 foreach($unit in $delta.translation_units){NoiseRepairRequire (@($compile|Where-Object {$_.Groups[1].Value.StartsWith('CMakeFiles/'+$unit.target+'.dir/') -and [string]::Equals([IO.Path]::GetFullPath($_.Groups[2].Value),[IO.Path]::GetFullPath($unit.source.path),[StringComparison]::OrdinalIgnoreCase)}).Count -eq 1) 'actual target compile exact source copy'}
 $expectedLinks=@{current_chain=@('cpu_current_rails','cpu_current_front','cpu_exact_bvi_v3','cpu_own_bridge','cpu_pas_core');production_pipeline_cold=@('production_pas_core','production_pas_current_candidate','production_pas_proto');score_only_controls=@('production_pas_core','production_pas_current_candidate','production_pas_proto');pas=@('production_pas_core','production_pas_current_candidate','production_pas_proto','verification_pas_emulator','verification_pas_bench');candidate_admission_controls=@('production_pas_core','production_pas_current_candidate','production_pas_proto','verification_pas_emulator');worker_affinity_controls=@();qualification_reader=@();qualification_reader_controls=@()}
 if($hasMotion){$expectedLinks.motion_source_regression=@('production_pas_core');NoiseRepairRequire (@($delta.generated_delta_inputs|Where-Object {$_.path -ceq $reg.runtime_contract_source.path}).Count -eq 1) 'generated runtime included in sealed delta inputs'}
 if($hasFrame){$expectedLinks.frame_metadata_controls=@('cpu_pas_core')}
 NoiseRepairRequire (@([regex]::Matches($ninja,'(?m)^build [^\r\n ]+[.]exe: CXX_EXECUTABLE_LINKER')).Count -eq $expectedLinks.Count) 'all and only declared executable link edges'
 foreach($target in $expectedLinks.Keys){$link=@([regex]::Matches($ninja,'(?m)^build '+[regex]::Escape($target)+'[.]exe: CXX_EXECUTABLE_LINKER[^\r\n]*\r?\n(?<body>(?:[ \t]+[^\r\n]*\r?\n)*)'));NoiseRepairRequire ($link.Count -eq 1) 'one actual link per cold image';$libraries=@([regex]::Matches($link[0].Groups['body'].Value,'(?m)^  LINK_LIBRARIES = (.*)$'));NoiseRepairRequire ($libraries.Count -eq 1) 'one actual linker library field';$joined=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
  foreach($token in [regex]::Matches($libraries[0].Groups[1].Value,'"[^"]*"|\S+')){$path=$token.Value.Trim('"');$name=[IO.Path]::GetFileName($path);$classified=@($delta.imported_static_libraries|Where-Object {[string]::Equals([IO.Path]::GetFileName($_.binary.path),$name,[StringComparison]::OrdinalIgnoreCase)});if($classified.Count){$match=@($classified|Where-Object {[string]::Equals([IO.Path]::GetFullPath($path),[IO.Path]::GetFullPath($_.binary.path),[StringComparison]::OrdinalIgnoreCase)});NoiseRepairRequire ($match.Count -eq 1 -and $joined.Add($match[0].role)) 'no shadowed/duplicate historical static role in actual link'}}
  NoiseRepairRequire ($joined.Count -eq @($expectedLinks[$target]).Count) 'exact effective imported library role denominator';if($joined.Count){NoiseRepairRequire (@(Compare-Object (@($joined)|Sort-Object) (@($expectedLinks[$target])|Sort-Object)).Count -eq 0) 'exact effective imported library roles per cold image'}
  if($hasMotion){ComponentDeltaMotionLinkObjects $link[0].Value $target $compile $delta.base_app_object $hasDense}
  if($hasMotion -and ($target -cin @(ComponentDeltaMotionTargets) -or $target -ceq 'motion_source_regression')){$explicit=($link[0].Value -split '\|')[0];NoiseRepairRequire (@([regex]::Matches($explicit,'CMakeFiles/game_motion_delta[.]dir/[^ \r\n]+[.]obj')).Count -eq 1 -and $libraries[0].Groups[1].Value -notmatch 'game_motion[.]cpp[.]obj') 'one shared motion object before archive arguments'}
  if($hasDense -and $target -cin @(ComponentDeltaDenseTargets)){$explicit=($link[0].Value -split '\|')[0];NoiseRepairRequire (@([regex]::Matches($explicit,'CMakeFiles/game_delta[.]dir/[^ \r\n]+[.]obj')).Count -eq 1 -and $libraries[0].Groups[1].Value -notmatch '(?:^|[/\\: ])game[.]cpp[.]obj(?:\s|$)') 'one shared game object before archives and no explicit old game member'}
  if($target -cin @('pas','candidate_admission_controls')){NoiseRepairRequire ($link[0].Value -match 'CMakeFiles/candidate_admission_delta[.]dir/[^\r\n]+[.]obj') 'actual fresh verification object explicitly linked before archive arguments';if($target -ceq 'pas'){NoiseRepairRequire ($link[0].Value.Contains($delta.base_app_object.path.Replace('\','/'))) 'actual original55 app object explicitly linked'}}
 }
 $targetObjectPattern='/CMakeFiles/('+((@($expectedTargets|Where-Object {$_ -cne 'pas'})|ForEach-Object {[regex]::Escape($_)}) -join '|')+')[.]dir/'
 $targetObjects=@($delta.compiled_outputs|Where-Object {$_.path -match '[.]obj$' -and $_.path.Replace('\','/') -cmatch $targetObjectPattern});NoiseRepairRequire ($targetObjects.Count -eq $freshCount -and @($targetObjects.path|Sort-Object -Unique).Count -eq $freshCount) 'all actual unique declared objects sealed'
 foreach($edge in $compile){$objectPath=Join-Path $delta.build_root $edge.Groups[1].Value;NoiseRepairRequire (@($targetObjects|Where-Object {[string]::Equals([IO.Path]::GetFullPath($_.path),[IO.Path]::GetFullPath($objectPath),[StringComparison]::OrdinalIgnoreCase)}).Count -eq 1) 'every exact Ninja object joins one sealed output'}
 foreach($pin in $delta.compiled_outputs){NoiseRepairCheckBinding $pin;if($pin.path -match '[.]obj$' -and $pin -notin $targetObjects){NoiseRepairRequire ($pin.path.Replace('\','/') -match '/(?:CompilerIdCXX/CMakeCXXCompilerId[.]obj|CMakeFiles/ShowIncludes/main[.]obj)$') 'no extra formal compile output'}}
 $images=@{};foreach($pin in $delta.files){NoiseRepairCheckBinding $pin;$name=[IO.Path]::GetFileName($pin.path);NoiseRepairRequire (-not $images.ContainsKey($name)) 'unique runtime image';$images[$name]=$pin}
 $exeNames=@($expectedTargets|Where-Object {$_ -cnotin @('candidate_admission_delta','game_motion_delta','game_delta')}|ForEach-Object {$_+'.exe'});NoiseRepairRequire (@($delta.files|Where-Object {$_.path -match '[.]exe$'}).Count -eq $exeNames.Count) 'exact new executable images';foreach($name in $exeNames){NoiseRepairRequire ($images.ContainsKey($name)) 'every declared linked image'}
 $priorProofPins=@();$expectedPrior=@()
 for($phase=0;$phase -lt $buildProofs.Count;$phase++){
  if($splitBuild){NoiseRepairSameValue @($delta.build_prior_outputs[$phase]|Sort-Object path) @($expectedPrior|Sort-Object path) 'all and only stable previous phase objects/images'}
  $phaseRequired=@($buildRequired)+@($priorProofPins)+@($expectedPrior);$spec=ColdHarnessNative $buildProofs[$phase] $phaseRequired
  NoiseRepairRequire ([IO.Path]::GetFileName($spec.exe) -ceq 'cmd.exe' -and @($spec.argv).Count -eq 4 -and $spec.argv[0] -ceq '/d' -and $spec.argv[1] -ceq '/s' -and $spec.argv[2] -ceq '/c' -and [IO.Path]::GetFileName($spec.argv[3]) -ceq 'build.cmd') 'exact four-argument build adapter launch'
  $buildFreeze=NoiseRepairReadBinding $buildProofs[$phase].freeze
  $buildCommand=@($buildFreeze.files|Where-Object {$_.path -and [string]::Equals([IO.Path]::GetFullPath($_.path),[IO.Path]::GetFullPath($spec.argv[3]),[StringComparison]::OrdinalIgnoreCase)});NoiseRepairRequire ($buildCommand.Count -eq 1) 'one frozen literal build adapter';NoiseRepairCheckBinding $buildCommand[0];$commandText=Get-Content -LiteralPath $buildCommand[0].path -Raw;$commandLines=@($commandText -split '\r?\n'|Where-Object {$_ -match '^"[^"]*[/\\]cmake[.]exe" '});NoiseRepairRequire ($commandLines.Count -eq 1) 'one actual CMake command';$arguments=@([regex]::Matches($commandLines[0],'"([^"]*)"')|ForEach-Object {$_.Groups[1].Value});$expectedArguments=@($arguments[0],'--build',$delta.build_root,'--parallel','4','--target')+@($buildGroups[$phase].targets);NoiseRepairSameValue $arguments $expectedArguments 'exact build root/parallel4/ordered phase targets command';NoiseRepairRequire ($commandLines[0] -ceq (($arguments|ForEach-Object {'"'+$_+'"'}) -join ' ')) 'literal fully quoted command without suffix';NoiseRepairSameValue @($spec.argv) @('/d','/s','/c',$buildCommand[0].path) 'exact build adapter launch'
  $phaseTargets=@($buildGroups[$phase].targets);foreach($pin in $targetObjects){$targetMatch=[regex]::Match($pin.path.Replace('\','/'),'/CMakeFiles/([^/]+)[.]dir/');if($targetMatch.Success -and $targetMatch.Groups[1].Value -cin $phaseTargets){$expectedPrior+=@($pin)}}
  foreach($target in $phaseTargets|Where-Object {$_ -cnotin @('candidate_admission_delta','game_motion_delta','game_delta')}){$expectedPrior+=@($images[$target+'.exe'])}
  $priorProofPins+=@($buildProofs[$phase].PSObject.Properties|ForEach-Object {$_.Value})
 }
 $controlCompileOnly=@{};foreach($pin in $selected.files){if($pin.bytes -gt 268435456 -and [IO.Path]::GetExtension($pin.path) -ieq '.lib'){$controlCompileOnly[[IO.Path]::GetFullPath($pin.path)]=$pin}}
 $controlRequired=@(foreach($pin in ($buildRequired+@($delta.compiled_outputs)+@($priorProofPins))){$key=[IO.Path]::GetFullPath($pin.path);if($controlCompileOnly.ContainsKey($key)){NoiseRepairSameBinding $pin $controlCompileOnly[$key] 'control excludes only exact selected large compile-only dependency'}else{$pin}})
 foreach($tuple in @(@('affinity_controls','worker_affinity_controls.exe','pas.worker-affinity-tests.v1',65),@('verification_controls','candidate_admission_controls.exe','pas.candidate-admission-controls.v1',82),@('reader_controls','qualification_reader_controls.exe','pas.qualification-reader-controls.v1',81))){
  $reportPin=$delta.($tuple[0]+'_report');$proof=$delta.($tuple[0]+'_process');$null=ColdHarnessNative $proof $controlRequired $images[$tuple[1]] @($reportPin.path);$report=NoiseRepairReadBinding $reportPin
  if($tuple[0] -ceq 'reader_controls'){
   ColdHarnessControl $report $tuple[2] $tuple[3] 'cases' 'cases_count';ColdHarnessControl $report.active_wait_policy_controls 'pas.active-wait-policy-reader-controls.v1' 12;ColdHarnessControl $report.process_qos_policy_controls 'pas.process-qos-policy-reader-controls.v1' 12;ColdHarnessControl $report.cold_harness_delta_controls 'pas.cold-harness-delta-reader-controls.v1' 12
   foreach($newControl in @(@($hasDense,'dense_source_delta_controls','pas.cold-dense-reader-controls.v1'),@($hasFrame,'frame_metadata_delta_controls','pas.cold-frame-metadata-reader-controls.v1'))){if($newControl[0] -or (NoiseRepairProperty $report $newControl[1])){ColdHarnessControl $report.($newControl[1]) $newControl[2] 8;ComponentDeltaColdProducerBool $report.($newControl[1]) 'pass' $true;foreach($flag in @('qualification_issued','cost_qualification','live_ready')){ComponentDeltaColdProducerBool $report.($newControl[1]) $flag $false}}}
  }elseif($tuple[0] -ceq 'affinity_controls'){ColdHarnessControl $report $tuple[2] $tuple[3];ComponentDeltaColdProducerText $report 'producer_eligibility_policy_id' $placementPolicy;ColdHarnessControl $report.eligibility_controls 'pas.cold-producer-eligibility-controls.v1' 12}
  else{ColdHarnessControl $report $tuple[2] $tuple[3];ColdHarnessControl $report.verification_delta_controls 'pas.cold-candidate-verification-controls.v1' 12}
  NoiseRepairAddInputs @($reportPin)
 }
 $scoreSeen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($row in $delta.score_control_processes){NoiseRepairRequire ($row.target -cin @('score_only_invalid','score_only_tap','score_only_hold','score_only_dense') -and $scoreSeen.Add($row.target)) 'exact unique new score control mode';$scene=$row.target.Substring(11);$null=ColdHarnessNative $row.Native8 $controlRequired $images['score_only_controls.exe'] @($scene,[IO.Path]::GetDirectoryName($row.report.path));$report=NoiseRepairReadBinding $row.report;ComponentDeltaColdProducerText $report 'schema' 'pas.score-only-literal-controls.v1';ComponentDeltaColdProducerText $report 'base_component_source_commit' $baseCommit;ComponentDeltaColdProducerBool $report 'pass' $true;ComponentDeltaColdProducerBool $report 'no_gameplay_files_created' $true;$null=ComponentDeltaColdProducerInteger $report 'failed' 0 0;ComponentDeltaValidateColdProducer $report $placementSchema;if($scene -ceq 'invalid'){ComponentDeltaValidateColdProducerControls $report $placementSchema};NoiseRepairAddInputs @($row.report)};NoiseRepairRequire ($scoreSeen.Count -eq 4) 'all four new score controls'
 if($hasMotion){
  NoiseRepairRequire (@($motion.consumer_images).Count -eq 5) 'five exact actual motion consumer images';$index=0;foreach($target in @(ComponentDeltaMotionTargets)){NoiseRepairSameBinding $motion.consumer_images[$index++] $images[$target+'.exe'] 'motion effective image seal'}
  NoiseRepairSameBinding $reg.binary $images['motion_source_regression.exe'] 'actual motion regression image';$motionObjects=@($targetObjects|Where-Object {$_.path.Replace('\','/').Contains('/CMakeFiles/game_motion_delta.dir/')});NoiseRepairRequire ($motionObjects.Count -eq 1) 'one shared actual motion object';NoiseRepairSameBinding $motion.new_object $motionObjects[0] 'motion object seal'
  $null=ColdHarnessNative $reg.process $controlRequired $reg.binary @(('--gtest_filter='+(ComponentDeltaMotionFilter)),('--gtest_output=json:'+$reg.report.path));ComponentDeltaMotionGTest (NoiseRepairReadBinding $reg.report);NoiseRepairAddInputs @($reg.report)
 }
 if($hasDense){
  NoiseRepairRequire (@($dense.consumer_images).Count -eq 6) 'six exact actual game consumer images';$index=0;foreach($target in @(ComponentDeltaDenseTargets)){NoiseRepairSameBinding $dense.consumer_images[$index++] $images[$target+'.exe'] 'game effective image seal'}
  NoiseRepairSameBinding $denseReg.binary $images['motion_source_regression.exe'] 'actual separate Dense regression image';NoiseRepairSameBinding $denseReg.binary $reg.binary 'same rebuilt regression image for original26 and separate12';NoiseRepairRequire (-not [string]::Equals($denseReg.report.path,$reg.report.path,[StringComparison]::OrdinalIgnoreCase) -and $denseReg.process.verification.path -cne $reg.process.verification.path) 'separate Dense/Motion Native and report'
  $gameObjects=@($targetObjects|Where-Object {$_.path.Replace('\','/').Contains('/CMakeFiles/game_delta.dir/')});NoiseRepairRequire ($gameObjects.Count -eq 1) 'one shared actual game object';NoiseRepairSameBinding $dense.new_object $gameObjects[0] 'game object seal'
  $null=ColdHarnessNative $denseReg.process $controlRequired $denseReg.binary @(('--gtest_filter='+(ComponentDeltaDenseFilter)),('--gtest_output=json:'+$denseReg.report.path));ColdHarnessDenseGTest (NoiseRepairReadBinding $denseReg.report);NoiseRepairAddInputs @($denseReg.report)
 }
 if($hasFrame){
  NoiseRepairSameBinding $frameControl.binary $images['frame_metadata_controls.exe'] 'actual fixed metadata control image';$frameObjects=@($targetObjects|Where-Object {$_.path.Replace('\','/').Contains('/CMakeFiles/frame_metadata_controls.dir/')});NoiseRepairRequire ($frameObjects.Count -eq 1) 'one actual fixed metadata test TU'
  $null=ColdHarnessNative $frameControl.process $controlRequired $frameControl.binary @($frameControl.report.path);$frameReport=NoiseRepairReadBinding $frameControl.report;ColdHarnessControl $frameReport 'pas.cold-frame-metadata-reuse-controls.v1' 8;ComponentDeltaColdProducerBool $frameReport 'pass' $true
  foreach($key in @('source_build_commit','source_commit')){ComponentDeltaColdProducerText $frameReport $key $head};foreach($key in @('source_build_dirty','source_dirty','qualification_issued','cost_qualification','live_ready','performance_measurement','long_gap_root_cause_claim','original_control_denominators_modified')){ComponentDeltaColdProducerBool $frameReport $key $false};foreach($key in @('synthetic_api_controls','component_memory_controls')){ComponentDeltaColdProducerBool $frameReport $key $true};foreach($key in @('device_endpoints','etw_start_operations','etw_stop_operations','pixels_written')){$null=ComponentDeltaColdProducerInteger $frameReport $key 0 0};NoiseRepairAddInputs @($frameControl.report)
 }
 $verification=NoiseRepairReadBinding $delta.verification_delta;ComponentDeltaColdProducerText $verification 'schema' 'pas.cold-candidate-verification-delta.v1';ComponentDeltaColdProducerText $verification 'source_commit' $head;ComponentDeltaColdProducerText $verification 'base_source_commit' $baseCommit;ComponentDeltaColdProducerBool $verification 'verification_only_delta' (-not $hasMotion);if($hasMotion){ComponentDeltaColdProducerBool $verification 'verification_and_motion_delta' $true;NoiseRepairSameValue $verification.motion_source_delta $motion 'verification exact effective motion declaration'};ComponentDeltaColdProducerBool $verification 'formal_rebuilt' $false
 NoiseRepairRequire ($hasDense -eq (NoiseRepairProperty $verification 'dense_source_delta') -and $hasFrame -eq (NoiseRepairProperty $verification 'frame_metadata_delta')) 'verification performance declaration presence exact'
 if($hasDense){ComponentDeltaColdProducerBool $verification 'verification_and_motion_and_dense_delta' $true;NoiseRepairSameValue $verification.dense_source_delta $dense 'verification exact game source/object/controls declaration'}else{NoiseRepairRequire (-not (NoiseRepairProperty $verification 'verification_and_motion_and_dense_delta')) 'no unbound Dense verification flag'}
 if($hasFrame){NoiseRepairSameValue $verification.frame_metadata_delta $frame 'verification exact fixed metadata source/controls declaration'}
 NoiseRepairSameBinding $verification.candidate_binary $images['pas.exe'] 'new executing verification candidate';NoiseRepairSameBinding $verification.candidate_source $currentMap['src/candidate_admission.cpp'] 'fresh candidate factory source';NoiseRepairSameBinding $verification.tests_source $currentMap['tests/candidate_admission_tests.cpp'] 'fresh candidate controls source'
 foreach($field in @('source_manifest','core_build_plan','base_production_build','base_app_object','base_source_snapshot','base_source_projection','verification_controls_report')){NoiseRepairSameBinding $verification.$field $delta.$field ('verification/harness '+$field)};NoiseRepairSameValue $verification.compile_process $delta.build_process 'verification compile Native8';NoiseRepairSameValue $verification.link_process $delta.build_process 'verification link Native8';NoiseRepairSameValue $verification.verification_controls_process $delta.verification_controls_process 'verification controls Native8'
 if($hasDense -or $hasFrame){
  foreach($field in @('build_target_groups','build_processes','build_prior_outputs')){NoiseRepairSameValue $verification.$field $delta.$field ('complete verification performance build '+$field)}
  foreach($key in @('source_dirty','qualification_issued','cost_qualification','live_ready')){ComponentDeltaColdProducerBool $verification $key $false};$null=ComponentDeltaColdProducerInteger $verification 'device_operations' 0 0
  foreach($field in @('source_snapshot','selected_dependency_manifest')){NoiseRepairSameBinding $verification.$field $delta.$field ('complete verification performance '+$field)};NoiseRepairSameBinding $verification.base_generated_provenance $delta.base_generated_inputs[0] 'verification original session-generated provenance'
  $candidateObjects=@($targetObjects|Where-Object {$_.path.Replace('\','/').Contains('/CMakeFiles/candidate_admission_delta.dir/')});NoiseRepairRequire ($candidateObjects.Count -eq 1) 'one actual new factory object';NoiseRepairSameBinding $verification.new_candidate_object $candidateObjects[0] 'verification new factory actual object seal'
  $verificationRoles=@('pas_core','pas_current_candidate','pas_proto','pas_emulator','pas_bench');NoiseRepairRequire (@($verification.linked_static_libraries).Count -eq 5) 'exact five verification static roles';$seenVerification=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
  foreach($row in $verification.linked_static_libraries){ColdHarnessFields $row @('role','binary','compiled_source_commit','build_binding') 'verification static role';NoiseRepairRequire ($row.role -cin $verificationRoles -and $seenVerification.Add($row.role)) 'unique verification static role';$importRole=if($row.role -cin @('pas_emulator','pas_bench')){'verification_'+$row.role}else{'production_'+$row.role};$originalLibrary=@($delta.imported_static_libraries|Where-Object role -ceq $importRole);NoiseRepairRequire ($originalLibrary.Count -eq 1) 'one matching original verification archive';NoiseRepairSameBinding $row.binary $originalLibrary[0].binary 'verification actual original static archive';NoiseRepairSameBinding $row.build_binding $delta.base_production_build 'verification static original55 binding';ComponentDeltaColdProducerText $row 'compiled_source_commit' $baseCommit}
  $newProofs=@($delta.configure_process)+@($buildProofs)+@($delta.affinity_controls_process,$delta.reader_controls_process,$delta.verification_controls_process)+@($delta.score_control_processes|ForEach-Object {$_.Native8});if($hasMotion){$newProofs+=@($reg.process)};if($hasDense){$newProofs+=@($denseReg.process)};if($hasFrame){$newProofs+=@($frameControl.process)}
  $nativeStages=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($proof in $newProofs){$nativeSpec=NoiseRepairReadBinding $proof.specification;NoiseRepairRequire ($nativeStages.Add($nativeSpec.stage)) 'separate unique actual performance Native stages'};NoiseRepairRequire ($nativeStages.Count -eq $nativeCount) 'complete actual performance Native catalogue'
 }
 NoiseRepairAddInputs @($bindingPin,$verification.new_candidate_object,$delta.verification_delta,$delta.source_manifest,$delta.source_snapshot,$delta.base_source_projection,$delta.base_source_snapshot,$delta.core_build_plan,$delta.base_cpu_build,$delta.base_production_build,$cpu.source_manifest,$cpu.core_build_plan,$production.core_build_plan,$cpu.original_main32_inventory,$cpu.base_release_build,$cpu.base_release_evaluation,$delta.compiler_environment,$cpu.tests_scope_authorization,$delta.compile_graph)
 $analyzerDirectories=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
 $viewFiles=@($delta.files);foreach($name in @('cost_analysis.exe','prelive_audit.exe','cost_comparison.exe','batch_check.exe')){$pin=@($cpu.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq $name});NoiseRepairRequire ($pin.Count -eq 1) 'exact original analyzer image';NoiseRepairAddInputs @($pin[0]);$viewFiles+=@($pin[0]);$null=$analyzerDirectories.Add([IO.Path]::GetDirectoryName($pin[0].path))};NoiseRepairAddInputs (@($delta.files)+@($delta.compiled_outputs))
 foreach($pin in $old.files|Where-Object {$_.path -match '[.]dll$' -and $analyzerDirectories.Contains([IO.Path]::GetDirectoryName($_.path))}){NoiseRepairAddInputs @($pin)}
 foreach($path in @(GateInputs)+@($script:maintenancePath,(Join-Path $PSScriptRoot 'common.ps1'),(Join-Path $PSScriptRoot 'noise_repair_build.ps1'),(Join-Path $PSScriptRoot 'component_delta.ps1'),(Join-Path $PSScriptRoot 'source_performance_deltas.ps1'))){NoiseRepairAddInputs @((Binding $path))}
 NoiseRepairRequire ((& git -C $repo rev-parse HEAD) -ceq $head -and -not (& git -C $repo status --porcelain)) 'current cold source unchanged while loading'
 $view=@{};foreach($prop in $delta.PSObject.Properties){$view[$prop.Name]=$prop.Value};$view.files=$viewFiles;$view.configuration='release'
 $script:builds=@($view);$script:tests=$oldTests;$script:commit=$head;$script:seal=@{files=$viewFiles};$script:inputs=@($script:noiseRepairActiveInputs.Keys|Sort-Object -Unique);$script:noiseRepairBindingPin=$bindingPin
 $script:coldHarnessDeltaBuild=$delta;$script:coldHarnessDeltaSourceMap=$currentMap
 $script:noiseRepairBuild=@{original_inventory=$inventory;base_build=$old;compiled_source_commit=$head;cold_harness_delta=$true;worker_affinity_required=$true;worker_placement_schema=$placementSchema;consumer_priority_requested=2;producer_wait_required=$true;producer_active_tail_ns=0;process_qos_required=$true}
 $script:noiseRepairPrerequisites=@{source_commit=$originalCommit;base_formal_source_commit=$baseCommit;scope=('Preserved original75/production9/base55 archives; new'+$freshCount+' declared cold/factory/performance/regression occurrences require current Normal3/AA/noise/ABBA');release_tests=$oldTests;original_main32_inventory=$cpu.original_main32_inventory;base_cpu_build=$delta.base_cpu_build;base_production_build=$delta.base_production_build;formal_rebuilt=$false;cold_harness_delta=$true;verification_only_delta=(-not $hasMotion);motion_source_delta=$hasMotion;dense_source_delta=$hasDense;frame_metadata_delta=$hasFrame;fresh_formal_translation_units=$(if($hasMotion){1+[int]$hasDense}else{0});formal_libraries_rebuilt=$false;whole_candidate_qualification=$false;live_ready=$false;device_operations=0}
}
function LoadNoiseRepairBuild([string]$bindingPath){
 $script:noiseRepairChecked=@{}
 $script:noiseRepairSpecs=@{}
 $script:noiseRepairActiveInputs=@{}
 $bindingPin=Binding $bindingPath
 $delta=NoiseRepairReadBinding $bindingPin
 if($delta.schema -ceq 'pas.cold-producer-eligibility-delta-build.v1'){LoadColdHarnessDeltaBuild $bindingPin $delta;return}
 NoiseRepairRequire ($delta.schema -ceq 'pas.narrow-noise-repair-build.v1' -and $delta.configuration -ceq 'release' -and $delta.scope -ceq 'noise_and_unrun_abba_only') 'declared narrow Release scope'
 NoiseRepairRequire ($delta.whole_candidate_qualification -eq $false -and $delta.live_ready -eq $false -and $delta.device_operations -eq 0) 'no whole-candidate/live/device qualification'
 $head=(& git -C $repo rev-parse HEAD)
 NoiseRepairRequire ($LASTEXITCODE -eq 0 -and $head -ceq $delta.source_commit -and (& git -C $repo branch --show-current) -ceq 'main' -and -not (& git -C $repo status --porcelain)) 'exact clean current main required'
 $isAmendment=(NoiseRepairProperty $delta 'maintenance_only_amendment') -and $delta.maintenance_only_amendment -eq $true
 $compiledCommit=$head;$parentDelta=$null;$compiledManifest=$null;$compiledPlan=$null;$maintenanceChanged=@()
 if($isAmendment){
  $compiledCommit=$delta.compiled_source_commit
  NoiseRepairRequire ($compiledCommit -cmatch '^[0-9a-f]{40}$' -and $compiledCommit -cne $head -and $delta.component_rebuilt -eq $false -and $delta.formal_rebuilt -eq $false) 'explicit maintenance-only compiled identity'
  & git -C $repo merge-base --is-ancestor $compiledCommit $head
  NoiseRepairRequire ($LASTEXITCODE -eq 0) 'compiled source must be an ancestor of maintenance source'
  $parentDelta=NoiseRepairReadBinding $delta.parent_repair_build
  NoiseRepairRequire ($parentDelta.schema -ceq $delta.schema -and $parentDelta.source_commit -ceq $compiledCommit -and $parentDelta.configuration -ceq 'release' -and $parentDelta.scope -ceq $delta.scope -and $parentDelta.whole_candidate_qualification -eq $false -and $parentDelta.live_ready -eq $false -and $parentDelta.device_operations -eq 0 -and -not $parentDelta.maintenance_only_amendment) 'one original compiled parent repair build'
  $mutableFields=@('source_commit','source_manifest','core_build_plan','changed_paths','candidate_bytes','additional_inputs')
  $amendmentFields=@('compiled_source_commit','maintenance_only_amendment','parent_repair_build','compiled_source_manifest','maintenance_changed_paths','formal_rebuilt','component_rebuilt','amendment_proof')
  foreach($property in $parentDelta.PSObject.Properties){
   NoiseRepairRequire (NoiseRepairProperty $delta $property.Name) ('amended parent property missing '+$property.Name)
   if($property.Name -cnotin $mutableFields){NoiseRepairSameValue $delta.($property.Name) $property.Value ('unchanged compiled parent '+$property.Name)}
  }
  foreach($property in $delta.PSObject.Properties){NoiseRepairRequire ((NoiseRepairProperty $parentDelta $property.Name) -or $property.Name -cin $amendmentFields) ('undeclared amendment property '+$property.Name)}
  $maintenanceChanged=@(& git -c core.quotePath=false -C $repo diff --name-only $compiledCommit $head)
  NoiseRepairRequire ($LASTEXITCODE -eq 0 -and $maintenanceChanged.Count -gt 0 -and @(Compare-Object (@($delta.maintenance_changed_paths)|Sort-Object) ($maintenanceChanged|Sort-Object)).Count -eq 0 -and @($delta.maintenance_changed_paths|Sort-Object -Unique).Count -eq @($delta.maintenance_changed_paths).Count) 'exact maintenance-only changed paths'
  foreach($relative in $maintenanceChanged){NoiseRepairRequire (NoiseRepairMaintenanceChange $relative) ('compiled target/formal change cannot be amended '+$relative)}
  NoiseRepairSameBinding $delta.compiled_source_manifest $parentDelta.source_manifest 'original compiled source manifest'
  $compiledManifest=NoiseRepairReadBinding $parentDelta.source_manifest
  $compiledPlan=NoiseRepairReadBinding $parentDelta.core_build_plan
  NoiseRepairRequire ($compiledManifest.source_commit -ceq $compiledCommit -and $compiledManifest.dirty -eq $false -and $compiledPlan.source_commit -ceq $compiledCommit) 'original compiled source/plan identity'
  NoiseRepairRequire (@($delta.additional_inputs).Count -eq @($parentDelta.additional_inputs).Count) 'same additional input path set'
  for($index=0;$index -lt @($parentDelta.additional_inputs).Count;$index++){
   $oldInput=@($parentDelta.additional_inputs)[$index];$newInput=@($delta.additional_inputs)[$index]
   NoiseRepairRequire ([string]::Equals([IO.Path]::GetFullPath($oldInput.path),[IO.Path]::GetFullPath($newInput.path),[StringComparison]::OrdinalIgnoreCase)) 'same additional input path'
   if($oldInput.bytes -ne $newInput.bytes -or $oldInput.sha256 -cne $newInput.sha256){
    $relative=NoiseRepairRelative $newInput.path
    NoiseRepairRequire ((NoiseRepairMaintenanceChange $relative) -and $relative -cin $maintenanceChanged) ('undeclared additional maintenance input '+$relative)
   }
  }
  NoiseRepairAddInputs @($delta.parent_repair_build,$parentDelta.source_manifest,$parentDelta.core_build_plan)
 }
 NoiseRepairAddInputs @($bindingPin,$delta.original_main32_inventory,$delta.base_release_build,$delta.base_release_evaluation,$delta.source_manifest,$delta.dependency_manifest,$delta.core_build_plan,$delta.controls_report,$delta.compiler_environment,$delta.tests_scope_authorization)
 $inventory=NoiseRepairReadBinding $delta.original_main32_inventory
 $baseCommit='aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de'
 NoiseRepairRequire ($inventory.schema -ceq 'pas.main32-stage-inventory.v1' -and $inventory.source_commit -ceq $baseCommit -and $inventory.original_STOPs_preserved -eq $true) 'original main32 preservation identity'
 NoiseRepairRequire ($inventory.cost_ready -eq $false -and $inventory.live_ready -eq $false -and $inventory.qualification_issued -eq $false -and $inventory.abba36 -ceq 'NOT_RUN') 'original negative state retained'
 NoiseRepairAddInputs (@($inventory.prefix_receipt,$inventory.faults_status,$inventory.production.original_status,$inventory.aa.original_status,$inventory.noise.verdict,$inventory.noise.noise_status,$inventory.noise.native_result)+@($inventory.readonly_graphs.report))
 $releaseRows=@($inventory.cold|Where-Object mode -ceq 'release')
 NoiseRepairRequire ($releaseRows.Count -eq 1) 'one original Release prerequisite'
 NoiseRepairSameBinding $delta.base_release_build $releaseRows[0].build 'base Release build'
 NoiseRepairSameBinding $delta.base_release_evaluation $releaseRows[0].evaluation 'base Release evaluation'
 $base=NoiseRepairReadBinding $delta.base_release_build
 $baseEvaluation=NoiseRepairReadBinding $delta.base_release_evaluation
 NoiseRepairRequire ($base.source_commit -ceq $baseCommit -and $base.configuration -ceq 'release' -and $baseEvaluation.source_commit -ceq $baseCommit -and $baseEvaluation.affected_cold_pass -eq $true -and $baseEvaluation.tests.Count -eq 25 -and $baseEvaluation.live_ready -eq $false -and $baseEvaluation.device_operations -eq 0) 'original Release25 prerequisite'
 NoiseRepairSameBinding $baseEvaluation.build $delta.base_release_build 'Release evaluation/build join'
 $baseFiles=@{}
 foreach($file in $base.files){NoiseRepairCheckBinding $file;$key=[IO.Path]::GetFullPath($file.path);NoiseRepairRequire (-not $baseFiles.ContainsKey($key)) 'duplicate base file';$baseFiles[$key]=$file}
 foreach($receipt in @($base.configure_receipt)+@($base.build_receipts)){NoiseRepairCheckNativeReceipt $receipt}
 NoiseRepairAddInputs @($base.core_build_plan,$base.source_manifest,$inventory.source_snapshot)
 $baseManifest=NoiseRepairReadBinding $base.source_manifest
 NoiseRepairRequire ($baseManifest.source_commit -ceq $baseCommit -and $baseManifest.dirty -eq $false) 'original formal source identity'
 $snapshot=NoiseRepairReadBinding $inventory.source_snapshot
 NoiseRepairRequire ($snapshot.source_commit -ceq $baseCommit -and $snapshot.qualification_issued -eq $false) 'immutable main32 source snapshot'
 NoiseRepairSameBinding $snapshot.formal_manifest $base.source_manifest 'immutable source/formal manifest join'
 $baseSources=@{}
 foreach($row in $snapshot.files){
  NoiseRepairAddInputs @($row.copy)
  NoiseRepairRequire ($row.copy.bytes -eq $row.original.bytes -and $row.copy.sha256 -ceq $row.original.sha256) 'immutable source copy identity'
  $relative=NoiseRepairRelative $row.original.path
  NoiseRepairRequire (-not $baseSources.ContainsKey($relative)) 'duplicate immutable source'
  $baseSources[$relative]=$row.original
 }
 foreach($file in $baseManifest.files){
  $relative=NoiseRepairRelative $file.path
  NoiseRepairRequire ($baseSources.ContainsKey($relative)) ('base source copy missing '+$relative)
  NoiseRepairSameBinding $file $baseSources[$relative] ('base source '+$relative)
 }
 $cpuDelta=(NoiseRepairProperty $delta 'component_cpu_delta') -and $delta.component_cpu_delta -eq $true
 if(NoiseRepairProperty $delta 'component_cpu_delta'){NoiseRepairRequire ($delta.component_cpu_delta -is [bool] -and $cpuDelta) 'declared CPU branch must be true boolean'}
 if(-not $cpuDelta){foreach($field in @('build_target_groups','build_processes','configure_process')){NoiseRepairRequire (-not (NoiseRepairProperty $delta $field)) 'CPU stage fields cannot enter legacy unchanged-formal branch'}}
 $changed=@($delta.changed_paths)
 NoiseRepairRequire ($changed.Count -gt 0 -and @($changed|Sort-Object -Unique).Count -eq $changed.Count) 'exact nonduplicate changed_paths required'
 foreach($relative in $changed){NoiseRepairRequire (NoiseRepairAllowedChange $relative $cpuDelta) ('change outside authorized harness/maintenance/docs '+$relative)}
 $actualChanged=@(& git -c core.quotePath=false -C $repo diff --name-only $baseCommit $head)
 NoiseRepairRequire ($LASTEXITCODE -eq 0 -and @(Compare-Object ($changed|Sort-Object) ($actualChanged|Sort-Object)).Count -eq 0) 'changed_paths differs from actual base..HEAD changes'
 $manifest=NoiseRepairReadBinding $delta.source_manifest
 NoiseRepairRequire ($manifest.source_commit -ceq $head -and $manifest.dirty -eq $false -and $manifest.device_operations -eq 0 -and $manifest.live_ready -eq $false) 'actual new full source manifest'
 # Same cooperative source selection as the original 909-file build manifest.
 $expectedSource=@(& git -c core.quotePath=false -C $repo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -cmatch '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'})
 NoiseRepairRequire ($LASTEXITCODE -eq 0) 'tracked source listing'
 $currentSources=@{}
 foreach($file in $manifest.files){
  NoiseRepairCheckBinding $file
  $relative=NoiseRepairRelative $file.path
  NoiseRepairRequire (-not $currentSources.ContainsKey($relative)) 'duplicate current source'
  $currentSources[$relative]=$file
  if($baseSources.ContainsKey($relative)){
   $old=$baseSources[$relative]
   if($old.bytes -ne $file.bytes -or $old.sha256 -cne $file.sha256){NoiseRepairRequire ($relative -cin $changed) ('undeclared source delta '+$relative)}
  }else{NoiseRepairRequire ($relative -cin $changed) ('undeclared added source '+$relative)}
 }
 NoiseRepairRequire (@(Compare-Object ($expectedSource|Sort-Object) (@($currentSources.Keys)|Sort-Object)).Count -eq 0) 'complete tracked cooperative source set required'
 foreach($relative in $changed|Where-Object { -not $currentSources.ContainsKey($_) }){NoiseRepairAddInputs @((Binding (Join-Path $repo $relative)))}
 if($isAmendment){
  NoiseRepairRequire ($manifest.compiled_source_commit -ceq $compiledCommit -and $manifest.maintenance_only_amendment -eq $true -and $manifest.component_rebuilt -eq $false -and $manifest.formal_rebuilt -eq $false) 'current manifest must distinguish execution and compiled source'
  NoiseRepairSameBinding $manifest.original_compiled_source_manifest $parentDelta.source_manifest 'current/compiled source manifest join'
  $compiledSources=@{}
  foreach($file in $compiledManifest.files){
   $relative=NoiseRepairRelative $file.path
   NoiseRepairRequire (-not $compiledSources.ContainsKey($relative) -and $currentSources.ContainsKey($relative)) ('compiled source missing/duplicate '+$relative)
   $compiledSources[$relative]=$file
   $current=$currentSources[$relative]
   if(-not (NoiseRepairMaintenanceChange $relative)){NoiseRepairSameBinding $current $file ('unchanged compiled code '+$relative)}
   elseif($current.bytes -ne $file.bytes -or $current.sha256 -cne $file.sha256){NoiseRepairRequire ($relative -cin $maintenanceChanged) ('undeclared maintenance source bytes '+$relative)}
  }
  foreach($relative in $currentSources.Keys|Where-Object {-not $compiledSources.ContainsKey($_)}){NoiseRepairRequire ((NoiseRepairMaintenanceChange $relative) -and $relative -cin $maintenanceChanged) ('added compiled code cannot be amended '+$relative)}
 }
 foreach($file in $baseManifest.files){
  $relative=NoiseRepairRelative $file.path
  if(-not $currentSources.ContainsKey($relative)){NoiseRepairRequire ($relative -cin $changed) ('undeclared removed source '+$relative)}
  if($relative -cmatch '^(?:src/|include/)' -or $relative -ceq 'CMakeLists.txt'){
   NoiseRepairRequire ($currentSources.ContainsKey($relative)) ('formal source removed '+$relative)
   if($cpuDelta -and $relative -cin @($delta.formal_changed_paths)){NoiseRepairRequire ((ComponentDeltaAllowed $relative) -and $relative -cin $changed) 'explicit rebuilt formal change'}else{NoiseRepairSameBinding $currentSources[$relative] $file ('unchanged formal source '+$relative)}
  }
 }
 NoiseRepairSameBinding $delta.dependency_manifest $base.dependency_manifest 'unchanged installed dependency manifest'
 $dependencies=NoiseRepairReadBinding $delta.dependency_manifest
 NoiseRepairRequire ($dependencies.source_commit -ceq $baseCommit -and $dependencies.release_and_debug -eq $true -and $dependencies.protoc_and_plugin -eq $true) 'old dependency provenance must remain old'
 $dependencyFiles=@{}
 foreach($file in $dependencies.files){NoiseRepairCheckBinding $file;$dependencyFiles[[IO.Path]::GetFullPath($file.path)]=$file}
 if(NoiseRepairProperty $delta 'selected_dependency_manifest'){
  NoiseRepairAddInputs @($delta.selected_dependency_manifest)
  $selected=NoiseRepairReadBinding $delta.selected_dependency_manifest
  NoiseRepairRequire (@($selected.files).Count -gt 0 -and @($selected.files).Count -le 128) 'bounded actual dependency selection'
  foreach($file in $selected.files){$key=[IO.Path]::GetFullPath($file.path);NoiseRepairRequire ($dependencyFiles.ContainsKey($key)) 'selected dependency belongs to original installed closure';NoiseRepairSameBinding $file $dependencyFiles[$key] 'actual selected dependency bytes';NoiseRepairAddInputs @($file)}
 }
 NoiseRepairRequire (@($delta.base_static_inputs).Count -gt 0) 'base static inputs required'
 foreach($file in $delta.base_static_inputs){
  NoiseRepairAddInputs @($file);$key=[IO.Path]::GetFullPath($file.path)
  if($baseFiles.ContainsKey($key)){NoiseRepairSameBinding $file $baseFiles[$key] 'base static file'}
  elseif($dependencyFiles.ContainsKey($key)){NoiseRepairSameBinding $file $dependencyFiles[$key] 'base installed dependency'}
  else{throw ('noise-repair static input not in base build/dependency closure '+$file.path)}
 }
 foreach($name in @('current_rails.lib','current_front.lib','own_bridge.lib','exact_bvi_v3.lib','pas_core.lib')){NoiseRepairRequire (@($delta.base_static_inputs|Where-Object {[IO.Path]::GetFileName($_.path) -ceq $name}).Count -eq 1) ('one unchanged static library '+$name)}
 $corePlan=NoiseRepairReadBinding $delta.core_build_plan
 if(-not $cpuDelta){NoiseRepairRequire (-not (NoiseRepairProperty $corePlan 'build_target_groups')) 'CPU build group plan cannot enter legacy unchanged-formal branch'}
 NoiseRepairRequire ($corePlan.source_commit -ceq $head -and $corePlan.base_source_commit -ceq $baseCommit -and $corePlan.formal_rebuilt -eq $cpuDelta) 'delta/core provenance'
 $hasAffinity=@($corePlan.delta_targets) -ccontains 'worker_affinity_controls'
 $hasWait=@($corePlan.delta_targets) -ccontains 'producer_wait_tests'
 $producerActiveTailNs=$null
 NoiseRepairRequire ((-not $hasWait) -or $hasAffinity) 'active-tail repair includes unchanged affinity policy'
 $expectedTargets=@('current_chain','consumer_priority_controls');if($hasAffinity){$expectedTargets+=@('worker_affinity_controls')};if($hasWait){$expectedTargets+=@('producer_wait_tests')}
 if($cpuDelta){
  NoiseRepairRequire (-not $isAmendment) 'CPU rebuild cannot be maintenance-only'
  ComponentDeltaVerifyBindings $delta $corePlan $baseFiles
  $expectedTargets=ComponentDeltaTargets
  $newExecutables=@(ComponentDeltaExecutables|ForEach-Object {$_+'.exe'})
  NoiseRepairAddInputs @($delta.linked_static_inputs)
 }else{$newExecutables=@($expectedTargets|ForEach-Object {$_+'.exe'})}
 NoiseRepairRequire (@(Compare-Object (@($corePlan.delta_targets)|Sort-Object) ($expectedTargets|Sort-Object)).Count -eq 0) 'only exact declared repair targets'
 if($isAmendment){
  NoiseRepairRequire ($corePlan.compiled_source_commit -ceq $compiledCommit -and $corePlan.component_rebuilt -eq $false) 'maintenance plan retains compiled-source identity'
  foreach($property in $compiledPlan.PSObject.Properties){if($property.Name -cne 'source_commit'){NoiseRepairSameValue $corePlan.($property.Name) $property.Value ('unchanged compiled plan '+$property.Name)}}
  foreach($property in $corePlan.PSObject.Properties){NoiseRepairRequire ((NoiseRepairProperty $compiledPlan $property.Name) -or $property.Name -cin @('compiled_source_commit','component_rebuilt')) ('undeclared amended plan property '+$property.Name)}
  $amendment=NoiseRepairReadBinding $delta.amendment_proof
  NoiseRepairRequire ($amendment.schema -ceq 'pas.narrow-noise-maintenance-amendment.v1' -and $amendment.source_commit -ceq $head -and $amendment.compiled_source_commit -ceq $compiledCommit -and $amendment.before_verified -eq $true -and $amendment.after_verified -eq $true -and $amendment.component_rebuilt -eq $false -and $amendment.formal_rebuilt -eq $false -and $amendment.native_operations -eq 0 -and $amendment.controls_rerun -eq $false -and $amendment.whole_candidate_qualification -eq $false -and $amendment.device_operations -eq 0 -and $amendment.live_ready -eq $false) 'maintenance preservation proof only'
  NoiseRepairSameBinding $amendment.parent_repair_build $delta.parent_repair_build 'amendment/parent join'
  NoiseRepairSameBinding $amendment.source_manifest $delta.source_manifest 'amendment/current source join'
  NoiseRepairSameBinding $amendment.core_build_plan $delta.core_build_plan 'amendment/current plan join'
  NoiseRepairRequire (@(Compare-Object (@($amendment.maintenance_changed_paths)|Sort-Object) ($maintenanceChanged|Sort-Object)).Count -eq 0) 'amendment proof changed paths'
  foreach($entry in $amendment.preserved_assets){NoiseRepairAddInputs @($entry)}
  NoiseRepairAddInputs @($delta.amendment_proof)
 }
 NoiseRepairRequire (@($delta.build_receipts).Count -gt 0) 'delta build receipts required'
 foreach($receipt in $delta.build_receipts){NoiseRepairCheckNativeReceipt $receipt}
 if(NoiseRepairProperty $delta 'configure_receipt'){NoiseRepairCheckNativeReceipt $delta.configure_receipt}
 NoiseRepairCheckNativeReceipt $delta.controls_receipt
 $controls=NoiseRepairReadBinding $delta.controls_report
 NoiseRepairRequire ($controls.schema -ceq 'pas.consumer-priority-tests.v1' -and $controls.assertions -eq 36 -and @($controls.checks).Count -eq 36 -and $controls.failed -eq 0 -and $controls.pass -eq $true -and @($controls.checks|Where-Object {$_.pass -ne $true}).Count -eq 0) 'all 36 consumer priority controls required'
 NoiseRepairRequire ($controls.device_operations -eq 0 -and $controls.device_endpoints -eq 0 -and $controls.live_ready -eq $false -and $controls.cost_qualification -eq $false) 'controlled harness regression scope'
 if(NoiseRepairProperty $controls 'real_time_qualification_issued'){NoiseRepairRequire ($controls.real_time_qualification_issued -eq $false) 'controls cannot grant real-time qualification'}
 $consumerSources=$currentSources;$consumerManifestPin=$delta.source_manifest;$consumerPlanPin=$delta.core_build_plan;$consumerPlan=$corePlan
 if($isAmendment){$consumerSources=$compiledSources;$consumerManifestPin=$parentDelta.source_manifest;$consumerPlanPin=$parentDelta.core_build_plan;$consumerPlan=$compiledPlan}
 $consumerSourcePins=@()
 foreach($relative in @('research/prelive_current/chain.cpp','research/prelive_current/consumer_priority.hpp','research/prelive_current/tests/consumer_priority_tests.cpp')){
  NoiseRepairRequire ($consumerSources.ContainsKey($relative)) ('compiled consumer policy source missing '+$relative)
  $consumerSourcePins+=@($consumerSources[$relative])
 }
 $consumerPriorityRequested=NoiseRepairConsumerPriorityPolicy $consumerPlan $controls $consumerSourcePins[1] $consumerSourcePins[2]
 foreach($receipt in @($delta.build_receipts)+@($delta.controls_receipt)){
  NoiseRepairRequireNativeInputs $receipt (@($consumerManifestPin,$consumerPlanPin)+$consumerSourcePins)
 }
 if($hasAffinity){
  NoiseRepairAddInputs @($delta.affinity_controls_report)
  NoiseRepairCheckNativeReceipt $delta.affinity_controls_receipt
  $affinityControls=NoiseRepairReadBinding $delta.affinity_controls_report
  NoiseRepairRequire ($affinityControls.schema -ceq 'pas.worker-affinity-tests.v1' -and $affinityControls.assertions -eq 65 -and @($affinityControls.checks).Count -eq 65 -and $affinityControls.failed -eq 0 -and $affinityControls.pass -eq $true -and @($affinityControls.checks|Where-Object {$_.pass -ne $true}).Count -eq 0) 'all worker affinity controls required'
  NoiseRepairRequire ($affinityControls.device_operations -eq 0 -and $affinityControls.device_endpoints -eq 0 -and $affinityControls.live_ready -eq $false -and $affinityControls.cost_qualification -eq $false) 'affinity controls grant no cost or live qualification'
 }
 if($hasWait){
  NoiseRepairAddInputs @($delta.wait_controls_report)
  NoiseRepairCheckNativeReceipt $delta.wait_controls_receipt
  $waitControls=NoiseRepairReadBinding $delta.wait_controls_report
  NoiseRepairRequire ($corePlan.producer_wait_controls_count -ge 22 -and $corePlan.producer_wait_controls_count -le 128 -and $waitControls.schema -ceq 'pas.producer-deadline-wait-tests.v1' -and $waitControls.assertions -eq $corePlan.producer_wait_controls_count -and @($waitControls.checks).Count -eq $corePlan.producer_wait_controls_count -and $waitControls.failed -eq 0 -and @($waitControls.checks|Where-Object {$_.pass -ne $true}).Count -eq 0 -and $waitControls.device_endpoints -eq 0 -and $waitControls.device_operations -eq 0 -and $waitControls.live_ready -eq $false -and $waitControls.cost_qualification -eq $false) 'all declared producer deadline controls required without qualification'
  $producerSourcePins=@($consumerSourcePins[0])
  foreach($relative in @('research/prelive_current/producer_wait.hpp','research/prelive_current/tests/producer_wait_tests.cpp')){
   NoiseRepairRequire ($consumerSources.ContainsKey($relative)) ('compiled producer policy source missing '+$relative)
   $producerSourcePins+=@($consumerSources[$relative])
  }
  $producerActiveTailNs=NoiseRepairProducerActiveTailPolicy $consumerPlan $waitControls $producerSourcePins[1] $producerSourcePins[2]
  $processQosRequired=NoiseRepairProcessQosPolicy $consumerPlan $waitControls $producerSourcePins[1] $producerSourcePins[2]
  if($processQosRequired){
   $waitVerification=NoiseRepairReadBinding $delta.wait_controls_receipt
   $waitNative=NoiseRepairReadBinding (Binding (Join-Path (Join-Path $out $waitVerification.stage) ($waitVerification.stage+'-result.json')))
   NoiseRepairRequire ($waitControls.process_qos_controls.actual_windows_snapshot.process_id -eq $waitNative.facts.pid) 'actual QoS control snapshot must join the verified own Native PID'
  }
  foreach($receipt in @($delta.build_receipts)+@($delta.wait_controls_receipt)){
   NoiseRepairRequireNativeInputs $receipt (@($consumerManifestPin,$consumerPlanPin)+$producerSourcePins)
  }
 }
 $compilerEnvironment=NoiseRepairReadBinding $delta.compiler_environment
 NoiseRepairRequire ($delta.compiler_environment_reused -eq $true -and $compilerEnvironment.source_commit -ceq $baseCommit -and $compilerEnvironment.live_ready -eq $false -and $compilerEnvironment.device_operations -eq 0) 'compiler environment retained with original provenance'
 NoiseRepairAddInputs @($compilerEnvironment.environment,$compilerEnvironment.verification,$compilerEnvironment.state)
 NoiseRepairCheckNativeReceipt $compilerEnvironment.verification
 $declaredClosures=@($delta.configure_input_closure)+@($delta.build_input_closures)+@($delta.controls_input_closure)
 if($hasAffinity){$declaredClosures+=@($delta.affinity_controls_input_closure)}
 if($hasWait){$declaredClosures+=@($delta.wait_controls_input_closure)}
 foreach($entry in $declaredClosures){
  $closure=NoiseRepairReadBinding $entry
  NoiseRepairAddInputs @($entry)
  NoiseRepairRequire ($closure.status -ceq 'COMPLETE' -and $closure.pass -eq $true -and $closure.before_verified -eq $true -and $closure.after_verified -eq $true -and $script:noiseRepairSpecs.ContainsKey($closure.stage)) 'declared native closure must match a completed delta stage'
 }
 NoiseRepairRequire ([string]::Equals([IO.Path]::GetFullPath($delta.tests_scope_authorization.path),(Join-Path $repo 'docs/research/live-readiness-20261007/NOISE_REPAIR_AUTHORIZATION_20261011.json'),[StringComparison]::OrdinalIgnoreCase)) 'current explicit scope authorization path'
 $authorization=NoiseRepairReadBinding $delta.tests_scope_authorization
 NoiseRepairRequire ($authorization.status -ceq 'HUMAN_AUTHORIZED' -and $authorization.scope -ceq $delta.scope -and $authorization.full_75_rerun_required -eq $false -and $authorization.production_pass_rerun_required -eq $false -and $authorization.device_operations -eq 0 -and $authorization.live_ready -eq $false) 'specific unchanged-PASS skip/noise repair authorization'
 NoiseRepairRequire (@(Compare-Object (@($authorization.allowed_operations)|Sort-Object) (@('aa','abba')|Sort-Object)).Count -eq 0) 'only necessary noise AA and previously unrun ABBA'
 $runtimeFiles=@{}
 foreach($file in $delta.files){
  NoiseRepairAddInputs @($file);$key=[IO.Path]::GetFullPath($file.path)
  NoiseRepairRequire (-not $runtimeFiles.ContainsKey($key)) 'duplicate sealed runtime file'
  $runtimeFiles[$key]=$file
  if($baseFiles.ContainsKey($key)){NoiseRepairSameBinding $file $baseFiles[$key] 'reused runtime file'}
  else{
   $name=[IO.Path]::GetFileName($key)
   NoiseRepairRequire ($name -cin $newExecutables -or [IO.Path]::GetExtension($key) -ceq '.dll') 'new runtime output outside declared harness/DLL set'
   if([IO.Path]::GetExtension($key) -ceq '.dll'){
    $origins=@(@($base.files)+@($dependencies.files)|Where-Object {[IO.Path]::GetFileName($_.path) -ceq $name -and $_.bytes -eq $file.bytes -and $_.sha256 -ceq $file.sha256})
    NoiseRepairRequire ($origins.Count -gt 0) ('byte-identical original runtime DLL required '+$name)
   }
  }
 }
 foreach($name in @($newExecutables)+@('cost_analysis.exe','prelive_audit.exe','cost_comparison.exe','batch_check.exe')){
  $files=@($delta.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq $name})
  NoiseRepairRequire ($files.Count -eq 1) ('one sealed executable '+$name)
  $key=[IO.Path]::GetFullPath($files[0].path)
  if($name -cin $newExecutables){NoiseRepairRequire (-not $baseFiles.ContainsKey($key)) ('new harness output required '+$name)}
  else{NoiseRepairRequire ($baseFiles.ContainsKey($key)) ('byte-identical original tool required '+$name)}
 }
 NoiseRepairRequire (@($delta.compiled_outputs).Count -gt 0) 'complete actual delta output bindings'
 NoiseRepairAddInputs @($delta.compiled_outputs)
 # Runtime DLLs beside the reused tools are actual inputs; unrelated legacy
 # base compiler/cache products and the installed dependency tree are not.
 $runtimeDirectories=@($delta.files|Where-Object {[IO.Path]::GetExtension($_.path) -ceq '.exe'}|ForEach-Object {[IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($_.path))}|Sort-Object -Unique)
 foreach($file in $base.files|Where-Object {[IO.Path]::GetExtension($_.path) -ceq '.dll' -and [IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($_.path)) -in $runtimeDirectories}){NoiseRepairAddInputs @($file)}
 foreach($name in $newExecutables){
  $sealed=@($delta.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq $name})[0]
  $compiled=@($delta.compiled_outputs|Where-Object {[string]::Equals($_.path,$sealed.path,[StringComparison]::OrdinalIgnoreCase)})
  NoiseRepairRequire ($compiled.Count -eq 1) ('one actual new compiled output '+$name)
  NoiseRepairSameBinding $sealed $compiled[0] ('compiled/sealed output '+$name)
 }
 $controlVerification=NoiseRepairReadBinding $delta.controls_receipt
 $controlSpec=$script:noiseRepairSpecs[$controlVerification.stage]
 $controlBinary=@($delta.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'consumer_priority_controls.exe'})[0]
 NoiseRepairRequire ([string]::Equals($controlSpec.exe,$controlBinary.path,[StringComparison]::OrdinalIgnoreCase) -and @($controlSpec.argv|Where-Object {[string]::Equals($_,$delta.controls_report.path,[StringComparison]::OrdinalIgnoreCase)}).Count -eq 1) 'new controls binary/report native join'
 if($hasAffinity){
  $affinityVerification=NoiseRepairReadBinding $delta.affinity_controls_receipt
  $affinitySpec=$script:noiseRepairSpecs[$affinityVerification.stage]
  $affinityBinary=@($delta.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'worker_affinity_controls.exe'})[0]
  NoiseRepairRequire ([string]::Equals($affinitySpec.exe,$affinityBinary.path,[StringComparison]::OrdinalIgnoreCase) -and @($affinitySpec.argv|Where-Object {[string]::Equals($_,$delta.affinity_controls_report.path,[StringComparison]::OrdinalIgnoreCase)}).Count -eq 1) 'affinity controls binary/report native join'
 }
 if($hasWait){
  $waitVerification=NoiseRepairReadBinding $delta.wait_controls_receipt
  $waitSpec=$script:noiseRepairSpecs[$waitVerification.stage]
  $waitBinary=@($delta.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'producer_wait_tests.exe'})[0]
  NoiseRepairRequire ([string]::Equals($waitSpec.exe,$waitBinary.path,[StringComparison]::OrdinalIgnoreCase) -and @($waitSpec.argv).Count -eq 1 -and [string]::Equals($waitSpec.argv[0],$delta.wait_controls_report.path,[StringComparison]::OrdinalIgnoreCase)) 'producer controls actual binary/report native join'
 }
 $allObjects=@($delta.compiled_outputs|Where-Object {[IO.Path]::GetExtension($_.path) -ceq '.obj'})
 if($cpuDelta){
  $graph=ComponentDeltaAssertGraph $delta.build_root $repo $corePlan.translation_unit_sources
  $expectedObjects=@($graph|ForEach-Object {[IO.Path]::GetFullPath((Join-Path $delta.build_root $_))})
  $deltaObjects=@($allObjects|Where-Object {[IO.Path]::GetFullPath($_.path) -cin $expectedObjects})
  NoiseRepairRequire ($deltaObjects.Count -eq 32) 'all exact fresh CPU objects sealed'
  foreach($object in $allObjects|Where-Object {$_ -notin $deltaObjects}){NoiseRepairRequire ($object.path.Replace('\','/') -cmatch '/(?:CompilerIdCXX/CMakeCXXCompilerId[.]obj|CMakeFiles/ShowIncludes/main[.]obj)$') 'unexpected object outside CPU32'}
 }else{
 $allObjects=@($delta.compiled_outputs|Where-Object {[IO.Path]::GetExtension($_.path) -ceq '.obj'})
 $deltaObjects=@($allObjects|Where-Object {$_.path.Replace('\','/') -cmatch '/CMakeFiles/(?:current_chain|consumer_priority_controls|worker_affinity_controls|producer_wait_tests)[.]dir/'})
 NoiseRepairRequire ($deltaObjects.Count -eq $expectedTargets.Count -and @($deltaObjects|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'chain.cpp.obj'}).Count -eq 1 -and @($deltaObjects|Where-Object {[IO.Path]::GetFileName($_.path) -in @('consumer_priority_tests.cpp.obj','consumer_priority_controls.cpp.obj')}).Count -eq 1 -and ((-not $hasAffinity) -or @($deltaObjects|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'worker_affinity_tests.cpp.obj'}).Count -eq 1)) 'only declared actual harness translation units'
 if($hasWait){NoiseRepairRequire (@($deltaObjects|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'producer_wait_tests.cpp.obj'}).Count -eq 1) 'one actual producer wait controls TU'}
 foreach($object in $allObjects|Where-Object {$_ -notin $deltaObjects}){
  NoiseRepairRequire ($object.path.Replace('\','/') -cmatch '/(?:CompilerIdCXX/CMakeCXXCompilerId[.]obj|CMakeFiles/ShowIncludes/main[.]obj)$') 'unexpected new target/formal object'
 }
 $ninjaRows=@($delta.compiled_outputs|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'build.ninja'})
 NoiseRepairRequire ($ninjaRows.Count -eq 1) 'one actual delta Ninja plan'
 $compilerRows=@(Get-Content -LiteralPath $ninjaRows[0].path|ForEach-Object {$_.Replace('\','/')}|Where-Object {$_ -cmatch '^build .+: CXX_COMPILER'})
 NoiseRepairRequire ($compilerRows.Count -eq $expectedTargets.Count) 'actual Ninja TU count'
 foreach($target in $expectedTargets){$pattern='^build CMakeFiles/'+[regex]::Escape($target)+'[.]dir/';NoiseRepairRequire (@($compilerRows|Where-Object {$_ -cmatch $pattern}).Count -eq 1) ('one actual Ninja TU '+$target)}
 }
 if(NoiseRepairProperty $delta 'additional_inputs'){NoiseRepairAddInputs @($delta.additional_inputs)}
 $oldProofs=@($inventory.cold)
 NoiseRepairRequire ($oldProofs.Count -eq 3 -and @(Compare-Object (@($oldProofs.mode)|Sort-Object) (@('release','debug','asan')|Sort-Object)).Count -eq 0) 'three preserved old configuration proofs'
 if($cpuDelta){
  $requiredControls=ComponentDeltaControls
  NoiseRepairRequire (@($delta.component_controls).Count -eq $requiredControls.Count) 'all fresh CPU semantic controls required'
  foreach($expected in $requiredControls){
   $rows=@($delta.component_controls|Where-Object {$_.target -ceq $expected.target})
   NoiseRepairRequire ($rows.Count -eq 1) 'unique CPU semantic control role'
   $row=$rows[0]
   NoiseRepairRequire ($row.schema -ceq $expected.schema -and $row.assertions -eq $expected.assertions) 'exact CPU semantic control contract'
   NoiseRepairAddInputs @($row.binary,$row.report,$row.input_closure)
   NoiseRepairCheckNativeReceipt $row.receipt
   $value=NoiseRepairReadBinding $row.report
   ComponentDeltaAssertControl $value $expected
   $verification=NoiseRepairReadBinding $row.receipt
   $spec=$script:noiseRepairSpecs[$verification.stage]
   NoiseRepairRequire ([IO.Path]::GetFullPath($spec.exe) -ceq [IO.Path]::GetFullPath($row.binary.path) -and @($spec.argv).Count -eq 1 -and $spec.argv[0] -ceq $row.report.path) 'CPU controls actual binary/output command join'
   $runtime=@($delta.files|Where-Object {[IO.Path]::GetFullPath($_.path) -ceq [IO.Path]::GetFullPath($row.binary.path)})
   NoiseRepairRequire ($runtime.Count -eq 1) 'CPU controls binary in exact sealed runtime'
   NoiseRepairSameBinding $runtime[0] $row.binary 'CPU control runtime/binary'
   $controlSourcePins=@(foreach($relative in (ComponentDeltaControlSources $row.target)){
    NoiseRepairRequire ($currentSources.ContainsKey($relative)) ('exact CPU control source missing from current manifest '+$relative)
    $currentSources[$relative]
   })
   NoiseRepairRequireNativeInputs $row.receipt (@($delta.source_manifest,$delta.core_build_plan)+@($delta.linked_static_inputs)+$controlSourcePins)
   $closure=NoiseRepairReadBinding $row.input_closure
   NoiseRepairRequire ($closure.schema -ceq 'pas.current-input-closure-verification.v1' -and $closure.stage -ceq $verification.stage -and $closure.status -ceq 'COMPLETE' -and $closure.pass -eq $true -and $closure.before_verified -eq $true -and $closure.after_verified -eq $true) 'CPU controls stage/input closure before/after complete'
  }
  NoiseRepairVerifyCpuBuildStages $delta $corePlan @($currentSources.Values)
 }
 foreach($proof in $oldProofs){
  $evaluation=NoiseRepairReadBinding $proof.evaluation;NoiseRepairAddInputs @($proof.evaluation,$proof.build)
  NoiseRepairRequire ($evaluation.source_commit -ceq $baseCommit -and $evaluation.affected_cold_pass -eq $true -and $evaluation.tests.Count -eq 25 -and $evaluation.live_ready -eq $false -and $evaluation.device_operations -eq 0) 'preserved old25 scope'
  NoiseRepairSameBinding $evaluation.build $proof.build 'old evaluation/build proof'
  # Verify preserved report/native-receipt bytes, without executing a test or
  # resolving archived Debug/ASan compiler-root binaries as current files.
  foreach($test in $evaluation.tests){NoiseRepairAddInputs @($test.report);NoiseRepairCheckNativeReceipt $test.receipt}
 }
 $archiveLinks=@()
 if(NoiseRepairProperty $delta 'old_configuration_proofs'){
  NoiseRepairRequire (@($delta.old_configuration_proofs).Count -eq 3) 'complete old configuration proof links'
  foreach($proof in $delta.old_configuration_proofs){
   $original=@($oldProofs|Where-Object mode -ceq $proof.mode)
   NoiseRepairRequire ($original.Count -eq 1) 'unique old configuration proof mode'
   NoiseRepairSameBinding $proof.evaluation $original[0].evaluation 'old evaluation proof link'
   NoiseRepairSameBinding $proof.build $original[0].build 'old build proof link'
   if(NoiseRepairProperty $proof 'archive_proof'){
    $archiveProof=NoiseRepairReadBinding $proof.archive_proof
    NoiseRepairAddInputs @($proof.archive_proof)
    NoiseRepairRequire ($archiveProof.schema -ceq 'pas.recoverable-main32-compiler-archive.v1' -and $archiveProof.source_commit -ceq $baseCommit -and $archiveProof.configuration -ceq $proof.mode -and $archiveProof.all_entry_SHAs_readback_verified -eq $true -and $archiveProof.originals_rechecked -eq $true -and $archiveProof.whole_candidate_qualified -eq $false -and $archiveProof.live_ready -eq $false -and $archiveProof.device_operations -eq 0) 'original recoverable compiler archive proof'
    NoiseRepairSameBinding $archiveProof.build_binding $proof.build 'archive/original build join'
    NoiseRepairSameBinding $proof.archive $archiveProof.archive 'original archive link'
    $archiveFile=Get-Item -LiteralPath $proof.archive.path
    NoiseRepairRequire (-not $archiveFile.PSIsContainer -and $archiveFile.Length -eq $proof.archive.bytes) 'original archive existence/length'
    $archiveLinks+=@(@{mode=$proof.mode;archive_proof=$proof.archive_proof;archive=$proof.archive;scope='Historical preservation link only; ZIP excluded from new native inputs';archive_sha_rehashed=$false;archive_exists_and_length_matches=$true})
   }
  }
 }
 $oldTests=@()
 foreach($test in $baseEvaluation.tests){
  NoiseRepairAddInputs @($test.report);NoiseRepairCheckNativeReceipt $test.receipt
  $copy=@{};foreach($property in $test.PSObject.Properties){$copy[$property.Name]=$property.Value}
  $copy.source_commit=$baseCommit;$copy.prerequisite_only=$true;$copy.scope='Preserved main32 Release prerequisite; not new-source cold qualification'
  $oldTests+=@($copy)
 }
 $guardInputs=@(GateInputs)
 foreach($path in $guardInputs){NoiseRepairAddInputs @((Binding $path))}
 $guard=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
 NoiseRepairAddInputs @($guard.guard_source,$guard.process_source,$guard.cached_identity_controls,$guard.identity_wait_controls)
 foreach($path in @($script:maintenancePath,$recipe,(Join-Path $PSScriptRoot 'common.ps1'),(Join-Path $PSScriptRoot 'noise_repair_build.ps1'))){NoiseRepairAddInputs @((Binding $path))}
 # Commit globals only after every declared byte and original prerequisite joins.
 NoiseRepairRequire ((& git -C $repo rev-parse HEAD) -ceq $head -and -not (& git -C $repo status --porcelain)) 'source changed while loading delta'
 CheckEntry $bindingPin
 $script:builds=@($delta)
 $script:tests=$oldTests
 $script:commit=$head
 $script:seal=@{files=@($delta.files)}
 $script:inputs=@($script:noiseRepairActiveInputs.Keys|Sort-Object -Unique)
 $script:noiseRepairBindingPin=$bindingPin
 $script:noiseRepairBuild=@{original_inventory=$inventory;base_build=$base;compiled_source_commit=$compiledCommit;maintenance_only_amendment=$isAmendment;worker_affinity_required=$hasAffinity;consumer_priority_requested=$consumerPriorityRequested;producer_wait_required=$hasWait;producer_active_tail_ns=$producerActiveTailNs;process_qos_required=($hasWait -and $processQosRequired)}
 $script:noiseRepairPrerequisites=@{source_commit=$baseCommit;scope='Previously passed scopes only; no new-source fresh/full qualification';release_tests=$oldTests;old_configuration_proofs=$oldProofs;compiler_archive_links=$archiveLinks;original_main32_inventory=$delta.original_main32_inventory;formal_source_byte_identical=(-not $cpuDelta);formal_rebuilt=$cpuDelta;formal_changed_paths=@($delta.formal_changed_paths);effective_linked_static_inputs=@($delta.linked_static_inputs);compiled_source_commit=$compiledCommit;controls_prerequisite_only=$isAmendment;component_rebuilt=(-not $isAmendment);maintenance_only_amendment=$isAmendment;whole_candidate_qualification=$false;live_ready=$false;device_operations=0}
}
