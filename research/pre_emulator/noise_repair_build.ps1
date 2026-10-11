. (Join-Path $PSScriptRoot 'component_delta.ps1')
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
 if($script:noiseRepairBuild.producer_wait_required){
  if($report.PSObject.Properties.Name -ccontains 'producer_active_tail_ns'){
   NoiseRepairRequire (($report.producer_active_tail_ns -is [int] -or $report.producer_active_tail_ns -is [long]) -and $report.producer_active_tail_ns -eq $script:noiseRepairBuild.producer_active_tail_ns) 'actual producer active-tail must equal compiled integer policy'
  }else{NoiseRepairRequire ($script:noiseRepairBuild.producer_active_tail_ns -eq 20000000) 'missing runtime producer active-tail only permitted for identified legacy 20ms build'}
 }
 if(-not $script:noiseRepairBuild.worker_affinity_required){return}
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
function LoadNoiseRepairBuild([string]$bindingPath){
 $script:noiseRepairChecked=@{}
 $script:noiseRepairSpecs=@{}
 $script:noiseRepairActiveInputs=@{}
 $bindingPin=Binding $bindingPath
 $delta=NoiseRepairReadBinding $bindingPin
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
 $script:noiseRepairBuild=@{original_inventory=$inventory;base_build=$base;compiled_source_commit=$compiledCommit;maintenance_only_amendment=$isAmendment;worker_affinity_required=$hasAffinity;consumer_priority_requested=$consumerPriorityRequested;producer_wait_required=$hasWait;producer_active_tail_ns=$producerActiveTailNs}
 $script:noiseRepairPrerequisites=@{source_commit=$baseCommit;scope='Previously passed scopes only; no new-source fresh/full qualification';release_tests=$oldTests;old_configuration_proofs=$oldProofs;compiler_archive_links=$archiveLinks;original_main32_inventory=$delta.original_main32_inventory;formal_source_byte_identical=(-not $cpuDelta);formal_rebuilt=$cpuDelta;formal_changed_paths=@($delta.formal_changed_paths);effective_linked_static_inputs=@($delta.linked_static_inputs);compiled_source_commit=$compiledCommit;controls_prerequisite_only=$isAmendment;component_rebuilt=(-not $isAmendment);maintenance_only_amendment=$isAmendment;whole_candidate_qualification=$false;live_ready=$false;device_operations=0}
}
