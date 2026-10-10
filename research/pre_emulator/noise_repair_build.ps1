# Dot-source after common.ps1. This loader validates a declared Release harness
# delta; it never builds or runs tests/metrics, and issues no candidate admission.
function NoiseRepairRequire([bool]$condition,[string]$message){if(-not $condition){throw ('noise-repair '+$message)}}
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
function NoiseRepairAllowedChange([string]$relative){
 if($relative -in @('AGENTS.md','README.md','research/pre_emulator/PLAN.md','research/prelive_current/chain.cpp','research/prelive_current/consumer_priority.hpp','research/prelive_current/tests/consumer_priority_controls.cpp','research/prelive_current/tests/consumer_priority_tests.cpp','research/prelive_current/CMakeLists.txt')){return $true}
 if($relative -cmatch '^research/pre_emulator/(?:[^/]+[.]ps1|(?:[^/]+/)*CMakeLists[.]txt)$'){return $true}
 if($relative -cmatch '^docs/.*[.](?:md|json)$' -and $relative -cne 'docs/research/zero-miss-20261006/windows/PRELIVE_COST_PROTOCOL_R2_20261007.md'){return $true}
 return $false
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
 $changed=@($delta.changed_paths)
 NoiseRepairRequire ($changed.Count -gt 0 -and @($changed|Sort-Object -Unique).Count -eq $changed.Count) 'exact nonduplicate changed_paths required'
 foreach($relative in $changed){NoiseRepairRequire (NoiseRepairAllowedChange $relative) ('change outside authorized harness/maintenance/docs '+$relative)}
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
   NoiseRepairSameBinding $currentSources[$relative] $file ('unchanged formal source '+$relative)
  }
 }
 NoiseRepairSameBinding $delta.dependency_manifest $base.dependency_manifest 'unchanged installed dependency manifest'
 $dependencies=NoiseRepairReadBinding $delta.dependency_manifest
 NoiseRepairRequire ($dependencies.source_commit -ceq $baseCommit -and $dependencies.release_and_debug -eq $true -and $dependencies.protoc_and_plugin -eq $true) 'old dependency provenance must remain old'
 $dependencyFiles=@{}
 foreach($file in $dependencies.files){NoiseRepairCheckBinding $file;$dependencyFiles[[IO.Path]::GetFullPath($file.path)]=$file}
 NoiseRepairRequire (@($delta.base_static_inputs).Count -gt 0) 'base static inputs required'
 foreach($file in $delta.base_static_inputs){
  NoiseRepairAddInputs @($file);$key=[IO.Path]::GetFullPath($file.path)
  if($baseFiles.ContainsKey($key)){NoiseRepairSameBinding $file $baseFiles[$key] 'base static file'}
  elseif($dependencyFiles.ContainsKey($key)){NoiseRepairSameBinding $file $dependencyFiles[$key] 'base installed dependency'}
  else{throw ('noise-repair static input not in base build/dependency closure '+$file.path)}
 }
 foreach($name in @('current_rails.lib','current_front.lib','own_bridge.lib','exact_bvi_v3.lib','pas_core.lib')){NoiseRepairRequire (@($delta.base_static_inputs|Where-Object {[IO.Path]::GetFileName($_.path) -ceq $name}).Count -eq 1) ('one unchanged static library '+$name)}
 $corePlan=NoiseRepairReadBinding $delta.core_build_plan
 NoiseRepairRequire ($corePlan.source_commit -ceq $head -and $corePlan.base_source_commit -ceq $baseCommit -and $corePlan.formal_rebuilt -eq $false) 'delta/core provenance'
 NoiseRepairRequire (@(Compare-Object (@($corePlan.delta_targets)|Sort-Object) (@('current_chain','consumer_priority_controls')|Sort-Object)).Count -eq 0) 'only the two declared harness targets'
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
 $compilerEnvironment=NoiseRepairReadBinding $delta.compiler_environment
 NoiseRepairRequire ($delta.compiler_environment_reused -eq $true -and $compilerEnvironment.source_commit -ceq $baseCommit -and $compilerEnvironment.live_ready -eq $false -and $compilerEnvironment.device_operations -eq 0) 'compiler environment retained with original provenance'
 NoiseRepairAddInputs @($compilerEnvironment.environment,$compilerEnvironment.verification,$compilerEnvironment.state)
 NoiseRepairCheckNativeReceipt $compilerEnvironment.verification
 $declaredClosures=@($delta.configure_input_closure)+@($delta.build_input_closures)+@($delta.controls_input_closure)
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
   NoiseRepairRequire ($name -in @('current_chain.exe','consumer_priority_controls.exe') -or [IO.Path]::GetExtension($key) -ceq '.dll') 'new runtime output outside declared harness/DLL set'
   if([IO.Path]::GetExtension($key) -ceq '.dll'){
    $origins=@(@($base.files)+@($dependencies.files)|Where-Object {[IO.Path]::GetFileName($_.path) -ceq $name -and $_.bytes -eq $file.bytes -and $_.sha256 -ceq $file.sha256})
    NoiseRepairRequire ($origins.Count -gt 0) ('byte-identical original runtime DLL required '+$name)
   }
  }
 }
 foreach($name in @('current_chain.exe','consumer_priority_controls.exe','cost_analysis.exe','prelive_audit.exe','cost_comparison.exe','batch_check.exe')){
  $files=@($delta.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq $name})
  NoiseRepairRequire ($files.Count -eq 1) ('one sealed executable '+$name)
  $key=[IO.Path]::GetFullPath($files[0].path)
  if($name -in @('current_chain.exe','consumer_priority_controls.exe')){NoiseRepairRequire (-not $baseFiles.ContainsKey($key)) ('new harness output required '+$name)}
  else{NoiseRepairRequire ($baseFiles.ContainsKey($key)) ('byte-identical original tool required '+$name)}
 }
 NoiseRepairRequire (@($delta.compiled_outputs).Count -gt 0) 'complete actual delta output bindings'
 NoiseRepairAddInputs @($delta.compiled_outputs)
 # Runtime DLLs beside the reused tools are actual inputs; unrelated legacy
 # base compiler/cache products and the installed dependency tree are not.
 $runtimeDirectories=@($delta.files|Where-Object {[IO.Path]::GetExtension($_.path) -ceq '.exe'}|ForEach-Object {[IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($_.path))}|Sort-Object -Unique)
 foreach($file in $base.files|Where-Object {[IO.Path]::GetExtension($_.path) -ceq '.dll' -and [IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($_.path)) -in $runtimeDirectories}){NoiseRepairAddInputs @($file)}
 foreach($name in @('current_chain.exe','consumer_priority_controls.exe')){
  $sealed=@($delta.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq $name})[0]
  $compiled=@($delta.compiled_outputs|Where-Object {[string]::Equals($_.path,$sealed.path,[StringComparison]::OrdinalIgnoreCase)})
  NoiseRepairRequire ($compiled.Count -eq 1) ('one actual new compiled output '+$name)
  NoiseRepairSameBinding $sealed $compiled[0] ('compiled/sealed output '+$name)
 }
 $controlVerification=NoiseRepairReadBinding $delta.controls_receipt
 $controlSpec=$script:noiseRepairSpecs[$controlVerification.stage]
 $controlBinary=@($delta.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'consumer_priority_controls.exe'})[0]
 NoiseRepairRequire ([string]::Equals($controlSpec.exe,$controlBinary.path,[StringComparison]::OrdinalIgnoreCase) -and @($controlSpec.argv|Where-Object {[string]::Equals($_,$delta.controls_report.path,[StringComparison]::OrdinalIgnoreCase)}).Count -eq 1) 'new controls binary/report native join'
 $allObjects=@($delta.compiled_outputs|Where-Object {[IO.Path]::GetExtension($_.path) -ceq '.obj'})
 $deltaObjects=@($allObjects|Where-Object {$_.path.Replace('\','/') -cmatch '/CMakeFiles/(?:current_chain|consumer_priority_controls)[.]dir/'})
 NoiseRepairRequire ($deltaObjects.Count -eq 2 -and @($deltaObjects|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'chain.cpp.obj'}).Count -eq 1 -and @($deltaObjects|Where-Object {[IO.Path]::GetFileName($_.path) -in @('consumer_priority_tests.cpp.obj','consumer_priority_controls.cpp.obj')}).Count -eq 1) 'only the two actual harness translation units'
 foreach($object in $allObjects|Where-Object {$_ -notin $deltaObjects}){
  NoiseRepairRequire ($object.path.Replace('\','/') -cmatch '/(?:CompilerIdCXX/CMakeCXXCompilerId[.]obj|CMakeFiles/ShowIncludes/main[.]obj)$') 'unexpected new target/formal object'
 }
 $ninjaRows=@($delta.compiled_outputs|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'build.ninja'})
 NoiseRepairRequire ($ninjaRows.Count -eq 1) 'one actual delta Ninja plan'
 $compilerRows=@(Get-Content -LiteralPath $ninjaRows[0].path|ForEach-Object {$_.Replace('\','/')}|Where-Object {$_ -cmatch '^build .+: CXX_COMPILER'})
 NoiseRepairRequire ($compilerRows.Count -eq 2 -and @($compilerRows|Where-Object {$_ -cmatch '^build CMakeFiles/current_chain[.]dir/'}).Count -eq 1 -and @($compilerRows|Where-Object {$_ -cmatch '^build CMakeFiles/consumer_priority_controls[.]dir/'}).Count -eq 1) 'actual Ninja plan must compile only the two harness targets'
 if(NoiseRepairProperty $delta 'additional_inputs'){NoiseRepairAddInputs @($delta.additional_inputs)}
 $oldProofs=@($inventory.cold)
 NoiseRepairRequire ($oldProofs.Count -eq 3 -and @(Compare-Object (@($oldProofs.mode)|Sort-Object) (@('release','debug','asan')|Sort-Object)).Count -eq 0) 'three preserved old configuration proofs'
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
 $script:noiseRepairBuild=@{original_inventory=$inventory;base_build=$base;compiled_source_commit=$compiledCommit;maintenance_only_amendment=$isAmendment}
 $script:noiseRepairPrerequisites=@{source_commit=$baseCommit;scope='Previously passed scopes only; no new-source fresh/full qualification';release_tests=$oldTests;old_configuration_proofs=$oldProofs;compiler_archive_links=$archiveLinks;original_main32_inventory=$delta.original_main32_inventory;formal_source_byte_identical=$true;formal_rebuilt=$false;compiled_source_commit=$compiledCommit;controls_prerequisite_only=$isAmendment;component_rebuilt=(-not $isAmendment);maintenance_only_amendment=$isAmendment;whole_candidate_qualification=$false;live_ready=$false;device_operations=0}
}
