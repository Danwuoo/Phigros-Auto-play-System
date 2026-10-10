param([Parameter(Mandatory)][string]$BaseBinding,
      [ValidatePattern('^main[0-9]{2}$')][string]$Attempt='main36')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
$script:maintenancePath=$PSCommandPath
$Operation='maintenance'
$compiledCommit='42a450c30170faa0fc6014ecc4e8aba893c153e6'
$baseCommit='aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de'
$head=(& git -C $repo rev-parse HEAD)
if($LASTEXITCODE -or -not $head -or (& git -C $repo branch --show-current) -cne 'main' -or (& git -C $repo status --porcelain)){throw 'exact clean main maintenance source required'}
& git -C $repo merge-base --is-ancestor $compiledCommit $head
if($LASTEXITCODE){throw 'main35 compiled source must be an ancestor'}

function ReadAmendBound($entry){CheckEntry $entry;return (Get-Content -LiteralPath $entry.path -Raw|ConvertFrom-Json -AsHashtable)}
function AmendRelative([string]$path){
 $absolute=[IO.Path]::GetFullPath($path)
 if(-not $absolute.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'amendment path outside main'}
 return [IO.Path]::GetRelativePath($repo,$absolute).Replace('\','/')
}
function MaintenancePath([string]$path){
 return ($path -in @('AGENTS.md','README.md','research/pre_emulator/PLAN.md') -or $path -cmatch '^research/pre_emulator/[^/]+[.]ps1$' -or ($path -cmatch '^docs/.*[.](?:md|json)$' -and $path -cne 'docs/research/zero-miss-20261006/windows/PRELIVE_COST_PROTOCOL_R2_20261007.md'))
}
function OriginalNoisePath([string]$path){
 return ((MaintenancePath $path) -or $path -in @('research/prelive_current/chain.cpp','research/prelive_current/consumer_priority.hpp','research/prelive_current/tests/consumer_priority_tests.cpp','research/prelive_current/CMakeLists.txt','research/pre_emulator/noise_repair/CMakeLists.txt'))
}
function VerifyAmendPreserved {
 CheckEntry $parentPin
 foreach($entry in $preservedPins.Values){CheckEntry $entry}
 foreach($entry in $currentEntries){CheckEntry $entry}
 if((& git -C $repo rev-parse HEAD) -cne $head -or (& git -C $repo branch --show-current) -cne 'main' -or (& git -C $repo status --porcelain)){throw 'maintenance source changed during amendment'}
}
function AddAmendPreserved($entry){
 if(-not $entry.path -or $entry.sha256 -cnotmatch '^[0-9a-f]{64}$' -or $entry.bytes -lt 0){throw 'complete preservation pin required'}
 $path=[IO.Path]::GetFullPath($entry.path)
 if($preservedPins.ContainsKey($path)){
  $old=$preservedPins[$path]
  if($old.bytes -ne $entry.bytes -or $old.sha256 -cne $entry.sha256){throw 'conflicting preservation identity'}
 }else{$preservedPins[$path]=$entry}
}
function AmendCandidateBytes {
 $roots=@(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -Directory|Where-Object { $_.Name -match '^prelive-pre-emulator-(?:(?:release|debug|asan)-main[0-9]{2}|evaluation-main[0-9]{2}-(?:release|debug|asan)|fixture-repo-main[0-9]{2}(?:-(?:release|debug|asan))?|planning)$' -or $_.Name -match '^prelive-noise-repair-(?:evaluation-)?main[0-9]{2}$' })
 $bytes=[long](($roots|ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -File -Recurse}|Measure-Object Length -Sum).Sum)
 if($bytes -gt 6442450944){throw 'unchanged aggregate candidate cap'}
 return $bytes
}

$parentPin=Binding ([IO.Path]::GetFullPath($BaseBinding))
$original=ReadAmendBound $parentPin
if($original.schema -cne 'pas.narrow-noise-repair-build.v1' -or $original.source_commit -cne $compiledCommit -or $original.configuration -cne 'release' -or $original.scope -cne 'noise_and_unrun_abba_only' -or $original.base_source_commit -cne $baseCommit -or $original.whole_candidate_qualification -ne $false -or $original.live_ready -ne $false -or $original.device_operations -ne 0 -or $original.maintenance_only_amendment){throw 'original non-amended main35 narrow build required'}
$oldManifest=ReadAmendBound $original.source_manifest
$oldPlan=ReadAmendBound $original.core_build_plan
if($oldManifest.schema -cne 'pas.cooperative-source.v1' -or $oldManifest.source_commit -cne $compiledCommit -or $oldManifest.dirty -ne $false -or $oldManifest.device_operations -ne 0 -or $oldManifest.live_ready -ne $false -or $oldPlan.source_commit -cne $compiledCommit -or $oldPlan.base_source_commit -cne $baseCommit -or $oldPlan.formal_rebuilt -ne $false -or $oldPlan.configuration -cne 'release' -or $oldPlan.whole_candidate_qualification -ne $false -or $oldPlan.live_ready -ne $false -or $oldPlan.device_operations -ne 0 -or @(Compare-Object (@($oldPlan.delta_targets)|Sort-Object) (@('current_chain','consumer_priority_controls')|Sort-Object)).Count){throw 'actual compiled source/plan identities required'}
if(@($original.base_static_inputs).Count -ne 5 -or @($oldPlan.base_static_inputs).Count -ne 5){throw 'five unchanged imported static library identities required'}
foreach($entry in $original.base_static_inputs){$planEntries=@($oldPlan.base_static_inputs|Where-Object {$_.path -ceq $entry.path});if($planEntries.Count -ne 1 -or $planEntries[0].bytes -ne $entry.bytes -or $planEntries[0].sha256 -cne $entry.sha256){throw 'original plan/static library SHA join'}}
foreach($name in @('current_rails.lib','current_front.lib','own_bridge.lib','exact_bvi_v3.lib','pas_core.lib')){if(@($original.base_static_inputs|Where-Object {[IO.Path]::GetFileName($_.path) -ceq $name}).Count -ne 1){throw 'original five static library set'}}
if(-not $original.build_receipts.Count -or -not $original.compiled_outputs.Count -or $original.candidate_cap -ne 6442450944){throw 'original compiled outputs/build receipts and cap required'}

$maintenanceChanges=@(& git -c core.quotePath=false -C $repo diff --name-only $compiledCommit $head)
if($LASTEXITCODE -or -not $maintenanceChanges.Count){throw 'explicit maintenance-only correction required'}
foreach($path in $maintenanceChanges){if(-not (MaintenancePath $path) -or -not (Test-Path -LiteralPath (Join-Path $repo $path) -PathType Leaf)){throw ('change outside maintenance-only correction: '+$path)}}
$changedPaths=@(& git -c core.quotePath=false -C $repo diff --name-only $baseCommit $head)
if($LASTEXITCODE){throw 'original noise source delta listing'}
foreach($path in $changedPaths){if(-not (OriginalNoisePath $path)){throw ('change outside original narrow scope: '+$path)}}
$trackedPaths=@(& git -c core.quotePath=false -C $repo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -cmatch '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'})
if($LASTEXITCODE){throw 'current tracked source inventory'}
$currentEntries=@($trackedPaths|ForEach-Object {Binding (Join-Path $repo $_)})
$currentByPath=@{};foreach($entry in $currentEntries){$relative=AmendRelative $entry.path;if($currentByPath.ContainsKey($relative)){throw 'duplicate current source path'};$currentByPath[$relative]=$entry}
$compiledByPath=@{}
foreach($entry in $oldManifest.files){
 $relative=AmendRelative $entry.path
 if($compiledByPath.ContainsKey($relative)){throw 'duplicate compiled source path'}
 $compiledByPath[$relative]=$entry
 if(-not $currentByPath.ContainsKey($relative)){throw ('compiled code removal outside correction: '+$relative)}
 $current=$currentByPath[$relative]
 if(-not (MaintenancePath $relative)){
  CheckEntry $entry
  if($current.bytes -ne $entry.bytes -or $current.sha256 -cne $entry.sha256){throw ('compiled C++/header/CMake/formal source changed: '+$relative)}
 }elseif($current.bytes -ne $entry.bytes -or $current.sha256 -cne $entry.sha256){if($relative -cnotin $maintenanceChanges){throw 'undeclared maintenance source byte change'}}
}
foreach($relative in $currentByPath.Keys|Where-Object {-not $compiledByPath.ContainsKey($_)}){if(-not (MaintenancePath $relative) -or $relative -cnotin $maintenanceChanges){throw ('added non-maintenance compiled source: '+$relative)}}

# Preserve exact original runtime, object, library, native and compiler pins.
# Installed dependency contents and historical ZIPs are not amendment inputs.
$preservedPins=@{}
$maintenanceInputs=@()
foreach($entry in $original.additional_inputs){
 $relative=AmendRelative $entry.path
 $current=Binding $entry.path
 if($current.bytes -ne $entry.bytes -or $current.sha256 -cne $entry.sha256){
  if(-not (MaintenancePath $relative) -or $relative -cnotin $maintenanceChanges){throw 'undeclared maintenance document binding change'}
 }
 $maintenanceInputs+=@($current)
}
foreach($entry in @($original.base_static_inputs)+@($original.files)+@($original.compiled_outputs)+@($original.configure_receipt)+@($original.build_receipts)+@($original.controls_receipt)+@($original.controls_report)+@($original.configure_input_closure)+@($original.build_input_closures)+@($original.controls_input_closure)+@($original.compiler_environment)+@($oldPlan.compiler_snapshot)+@($oldPlan.compiler_environment)+@($original.source_manifest)+@($original.core_build_plan)+@($original.dependency_manifest)+@($original.base_release_build)+@($original.base_release_evaluation)+@($original.original_main32_inventory)+@($original.tests_scope_authorization)+$maintenanceInputs){AddAmendPreserved $entry}
foreach($proof in $original.old_configuration_proofs){AddAmendPreserved $proof.evaluation;AddAmendPreserved $proof.build;if($proof.archive_proof){AddAmendPreserved $proof.archive_proof}}
$report=ReadAmendBound $original.controls_report
if($report.schema -cne 'pas.consumer-priority-tests.v1' -or $report.pass -ne $true -or $report.failed -ne 0 -or $report.assertions -ne 36 -or $report.checks.Count -ne 36 -or @($report.checks|Where-Object {$_.pass -ne $true}).Count -or $report.device_operations -ne 0 -or $report.device_endpoints -ne 0 -or $report.cost_qualification -ne $false -or $report.live_ready -ne $false){throw 'original 36 passing controls retained as compiled-source prerequisite'}
foreach($entry in @($original.configure_receipt)+@($original.build_receipts)+@($original.controls_receipt)){$verification=ReadAmendBound $entry;Receipt $verification.stage}
foreach($entry in @($original.configure_input_closure)+@($original.build_input_closures)+@($original.controls_input_closure)){$closure=ReadAmendBound $entry;if($closure.schema -cne 'pas.current-input-closure-verification.v1' -or $closure.status -cne 'COMPLETE' -or $closure.pass -ne $true -or $closure.before_verified -ne $true -or $closure.after_verified -ne $true){throw 'original verified compile/control input closure required'}}
VerifyAmendPreserved

$artifact=Join-Path $repo ('out/prelive-noise-repair-evaluation-'+$Attempt)
Fresh $artifact
[IO.Directory]::CreateDirectory($artifact)|Out-Null
$capacityStage='amend-prelive-noise-repair-'+$Attempt
Capacity $capacityStage 'before'
$null=AmendCandidateBytes
$manifestPath=Join-Path $artifact 'source-manifest.json'
NewJson $manifestPath @{schema='pas.cooperative-source.v1';source_commit=$head;compiled_source_commit=$compiledCommit;base_source_commit=$baseCommit;dirty=$false;files=$currentEntries;original_compiled_source_manifest=$original.source_manifest;maintenance_only_amendment=$true;component_rebuilt=$false;formal_rebuilt=$false;device_operations=0;live_ready=$false}
$planPath=Join-Path $artifact 'narrow-core-build-plan.json'
$newPlan=($oldPlan|ConvertTo-Json -Depth 40|ConvertFrom-Json -AsHashtable)
$newPlan.source_commit=$head
$newPlan.compiled_source_commit=$compiledCommit
$newPlan.formal_rebuilt=$false
$newPlan.component_rebuilt=$false
NewJson $planPath $newPlan
VerifyAmendPreserved
Capacity $capacityStage 'after'
$proofPath=Join-Path $artifact 'maintenance-amendment-proof.json'
NewJson $proofPath @{schema='pas.narrow-noise-maintenance-amendment.v1';source_commit=$head;compiled_source_commit=$compiledCommit;parent_repair_build=$parentPin;maintenance_changed_paths=@($maintenanceChanges|Sort-Object -Unique);before_verified=$true;after_verified=$true;preserved_assets=@($preservedPins.Values|Sort-Object path);source_manifest=(Binding $manifestPath);core_build_plan=(Binding $planPath);component_rebuilt=$false;formal_rebuilt=$false;native_operations=0;controls_rerun=$false;whole_candidate_qualification=$false;device_operations=0;live_ready=$false}
$amended=($original|ConvertTo-Json -Depth 40|ConvertFrom-Json -AsHashtable)
$amended.source_commit=$head
$amended.compiled_source_commit=$compiledCommit
$amended.maintenance_only_amendment=$true
$amended.parent_repair_build=$parentPin
$amended.compiled_source_manifest=$original.source_manifest
$amended.source_manifest=Binding $manifestPath
$amended.core_build_plan=Binding $planPath
$amended.maintenance_changed_paths=@($maintenanceChanges|Sort-Object -Unique)
$amended.changed_paths=@($changedPaths|Sort-Object -Unique)
$amended.formal_rebuilt=$false
$amended.component_rebuilt=$false
$amended.amendment_proof=Binding $proofPath
$amended.additional_inputs=$maintenanceInputs
$amended.candidate_bytes=AmendCandidateBytes
VerifyAmendPreserved
$bindingPath=Join-Path $artifact 'build-binding.json'
NewJson $bindingPath $amended
Write-Output $bindingPath
