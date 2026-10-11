param([Parameter(Mandatory)][string]$ProductionBuildBinding,
      [Parameter(Mandatory)][string]$NoiseRepairBinding,
      [ValidatePattern('^main[0-9]{2}$')][string]$Attempt='main47')
. (Join-Path $PSScriptRoot 'common.ps1')
# Keep the complete existing receipt/helper set in this script's scope. A
# dot-source only inside LoadBuilds would lose its local helpers on return.
. (Join-Path $PSScriptRoot 'noise_repair_build.ps1')
$script:maintenancePath=$PSCommandPath
LoadBuilds 'main32' $NoiseRepairBinding
$noise=$builds[0];$noisePin=$script:noiseRepairBindingPin
$commit=$noise.source_commit
$productionPin=Binding ([IO.Path]::GetFullPath($ProductionBuildBinding))
$production=NoiseRepairReadBinding $productionPin
NoiseRepairRequire ($production.schema -ceq 'pas.cooperative-build.v1' -and $production.configuration -ceq 'release' -and $production.source_commit -ceq $commit) 'current same-source Release production binding'
foreach($field in @('component_cpu_delta','formal_rebuilt','old_configuration_prerequisite_only')){NoiseRepairRequire ($production.$field -is [bool] -and $production.$field) ('explicit current production '+$field)}
foreach($field in @('whole_candidate_qualification','qualification_issued','cost_ready','live_ready')){NoiseRepairRequire ($production.$field -is [bool] -and -not $production.$field) ('production build cannot claim '+$field)}
NoiseRepairRequire ($production.device_operations -eq 0) 'endpoint-free production build'
NoiseRepairSameBinding $production.noise_repair_build $noisePin 'current production/narrow build'
NoiseRepairSameBinding $production.source_manifest $noise.source_manifest 'same current whole source manifest'
NoiseRepairSameBinding $production.dependency_manifest $noise.dependency_manifest 'same complete installed dependency manifest'
$manifest=NoiseRepairReadBinding $production.source_manifest
$dependencies=NoiseRepairReadBinding $production.dependency_manifest
$core=NoiseRepairReadBinding $production.core_build_plan
NoiseRepairRequire ($manifest.schema -ceq 'pas.cooperative-source.v1' -and $manifest.source_commit -ceq $commit -and $manifest.dirty -is [bool] -and -not $manifest.dirty) 'current clean production source'
NoiseRepairRequire ($core.schema -ceq 'pas.fresh-core-build-plan.v1' -and $core.source_commit -ceq $commit -and $core.configuration -ceq 'release' -and $core.translation_units -eq 22 -and $core.old_donor_objects_used -is [bool] -and -not $core.old_donor_objects_used -and $core.component_cpu_delta -is [bool] -and $core.component_cpu_delta -and $core.device_operations -eq 0 -and $core.live_ready -eq $false) 'fresh current formal22 production core plan'
NoiseRepairSameBinding $core.compiler_snapshot (Binding $tool) 'unchanged production compiler snapshot'
NoiseRepairRequire (@(Compare-Object (@($production.formal_changed_paths)|Sort-Object) (@($noise.formal_changed_paths)|Sort-Object)).Count -eq 0 -and @(Compare-Object (@($core.formal_changed_paths)|Sort-Object) (@($noise.formal_changed_paths)|Sort-Object)).Count -eq 0) 'explicit current formal delta join'
NoiseRepairRequire ($dependencies.source_commit -ceq $noise.base_source_commit -and $dependencies.release_and_debug -eq $true -and $dependencies.protoc_and_plugin -eq $true) 'original installed dependencies retain their original provenance'
$buildRoot=[IO.Path]::GetFullPath($production.build_root)
NoiseRepairRequire ([IO.Path]::GetDirectoryName($buildRoot) -ceq [IO.Path]::GetFullPath((Join-Path $repo 'out')) -and [IO.Path]::GetFileName($buildRoot) -cmatch '^prelive-pre-emulator-release-main[0-9]{2}$' -and [IO.Path]::GetFileName($buildRoot) -cne 'prelive-pre-emulator-release-main32') 'fresh current production root, separate from preserved base'

$sourceByPath=@{}
$tracked=@(& git -c core.quotePath=false -C $repo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -cmatch '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'})
NoiseRepairRequire (-not $LASTEXITCODE -and $tracked.Count -eq @($manifest.files).Count) 'exact full tracked current production source denominator'
foreach($file in $manifest.files){
 $relative=NoiseRepairRelative $file.path;$path=[IO.Path]::GetFullPath($file.path)
 NoiseRepairRequire ($relative -cin $tracked -and -not $sourceByPath.ContainsKey($path)) 'unique current production source member'
 CheckEntry $file;$sourceByPath[$path]=$file
}
$manualKey=[IO.Path]::GetFullPath((Join-Path $repo 'src/manual_session.cpp'))
NoiseRepairRequire ($sourceByPath.ContainsKey($manualKey)) 'actual manual-session source is bound'
$manual=$sourceByPath[$manualKey]
foreach($file in $dependencies.files){CheckEntry $file}
$filesByPath=@{}
foreach($file in $production.files){
 $path=[IO.Path]::GetFullPath($file.path)
 NoiseRepairRequire ($path.StartsWith($buildRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and -not $filesByPath.ContainsKey($path)) 'unique fresh production output within build root'
 CheckEntry $file;$filesByPath[$path]=$file
}
NoiseRepairRequire ($filesByPath.Count -gt 0) 'sealed fresh production outputs'
$actualFiles=@(Get-ChildItem -LiteralPath $buildRoot -File -Recurse|ForEach-Object {[IO.Path]::GetFullPath($_.FullName)})
NoiseRepairRequire (@(Compare-Object ($actualFiles|Sort-Object) (@($filesByPath.Keys)|Sort-Object)).Count -eq 0) 'complete concrete fresh production output set'
$objects=@($production.files|Where-Object {[IO.Path]::GetExtension($_.path) -ceq '.obj' -and $_.path.Replace('\','/') -cnotmatch '/(?:CompilerIdCXX/|ShowIncludes/)'} )
NoiseRepairRequire ($objects.Count -eq 52) 'exact actual production/control52 object denominator'
foreach($role in @(@{name='pas_core';count=22},@{name='pas_current_candidate';count=8},@{name='pas_emulator';count=6},@{name='pas_proto';count=2},@{name='pas_bench';count=5})){
 $library=[IO.Path]::GetFullPath((Join-Path $buildRoot ('production/'+$role.name+'.lib')))
 NoiseRepairRequire ($filesByPath.ContainsKey($library) -and @($objects|Where-Object {$_.path.Replace('\','/').Contains('/production/CMakeFiles/'+$role.name+'.dir/',[StringComparison]::Ordinal)}).Count -eq $role.count) 'fresh production library and exact actual object role'
}

function AssertCurrentProduction {
 NoiseRepairRequire ((& git -C $repo rev-parse HEAD) -ceq $commit -and (& git -C $repo branch --show-current) -ceq 'main' -and -not (& git -C $repo status --porcelain)) 'same exact clean main throughout normal3 production'
 foreach($entry in @($manifest.files)+@($production.files)+@($productionPin,$production.source_manifest,$production.dependency_manifest,$production.core_build_plan,$noisePin)){CheckEntry $entry}
}
function VerifyProductionReceipt($receipt,$closure,$required){
 NoiseRepairCheckNativeReceipt $receipt
 $verification=NoiseRepairReadBinding $receipt
 $value=NoiseRepairReadBinding $closure
 $actual=Binding (Join-Path $pkg ($verification.stage+'-input-closure-verification.json'))
 NoiseRepairSameBinding $closure $actual 'actual production native input closure'
 NoiseRepairRequire ($value.schema -ceq 'pas.current-input-closure-verification.v1' -and $value.stage -ceq $verification.stage -and $value.status -ceq 'COMPLETE' -and $value.pass -eq $true -and $value.before_verified -eq $true -and $value.after_verified -eq $true) 'production native before/after COMPLETE closure'
 NoiseRepairSameBinding $value.verification $receipt 'production closure/native verification'
 NoiseRepairRequireNativeInputs $receipt $required
 return $script:noiseRepairSpecs[$verification.stage]
}
$compileInputs=@($production.source_manifest,$production.dependency_manifest,$production.core_build_plan,$noise.compiler_environment)+@($manifest.files)+@($dependencies.files)
AssertCurrentProduction
$null=VerifyProductionReceipt $production.configure_receipt $production.configure_input_closure $compileInputs
$expectedBuildGroups=@(@{id='core';targets=@('pas_core')},@{id='action';targets=@('pas_current_candidate')},@{id='emulator';targets=@('pas_emulator')},@{id='bench';targets=@('pas_bench')},@{id='entry';targets=@('pas','candidate_admission_controls','candidate_receipt_collector_controls','production_entry_controls')},@{id='pipeline';targets=@('score_only_controls','production_pipeline_cold','production_pipeline_audit')})
NoiseRepairRequire (@($production.build_receipts).Count -eq 6 -and @($production.build_input_closures).Count -eq 6 -and @($production.build_target_groups).Count -eq 6 -and @($core.build_target_groups).Count -eq 6) 'exact six complete bounded fresh production build receipts/closures'
for($index=0;$index -lt 6;$index++){
 $expected=$expectedBuildGroups[$index]
 foreach($groups in @($production.build_target_groups,$core.build_target_groups)){
  NoiseRepairRequire ($groups[$index].id -ceq $expected.id -and @($groups[$index].targets).Count -eq @($expected.targets).Count -and @(Compare-Object (@($groups[$index].targets)|Sort-Object) (@($expected.targets)|Sort-Object)).Count -eq 0) 'exact ordered six production target groups'
 }
 $null=VerifyProductionReceipt $production.build_receipts[$index] $production.build_input_closures[$index] $compileInputs
 $proof=NoiseRepairReadBinding $production.build_receipts[$index]
 NoiseRepairRequire ($proof.stage -ceq ('build-production-delta-'+$Attempt+'-'+$expected.id)) 'actual ordered bounded production stage join'
}
$buildProofs=@($production.configure_receipt,$production.configure_input_closure)+@($production.build_receipts)+@($production.build_input_closures)
$expectedControls=@('candidate_admission_controls','candidate_receipt_collector_controls','production_entry_controls','score_only_invalid','score_only_tap','score_only_hold','score_only_dense')
NoiseRepairRequire (@($production.controls).Count -eq $expectedControls.Count -and @(Compare-Object (@($production.controls.target)|Sort-Object) ($expectedControls|Sort-Object)).Count -eq 0) 'exact affected production admission/entry/None controls'
foreach($target in $expectedControls){
 $rows=@($production.controls|Where-Object target -ceq $target);NoiseRepairRequire ($rows.Count -eq 1) 'unique affected production control role'
 $row=$rows[0];$binaryKey=[IO.Path]::GetFullPath($row.binary.path)
 NoiseRepairRequire ($filesByPath.ContainsKey($binaryKey)) 'affected control image is in fresh production outputs'
 NoiseRepairSameBinding $row.binary $filesByPath[$binaryKey] 'affected production control binary'
 $spec=VerifyProductionReceipt $row.receipt $row.input_closure (@($compileInputs)+@($production.files)+$buildProofs)
 NoiseRepairRequire ([string]::Equals($spec.exe,$row.binary.path,[StringComparison]::OrdinalIgnoreCase)) 'actual affected control image/native join'
 $value=NoiseRepairReadBinding $row.report
 NoiseRepairRequire ($value.failed -eq 0 -and $value.device_operations -eq 0 -and $value.live_ready -eq $false) 'endpoint-free successful affected production controls'
 if($target -cin @('candidate_admission_controls','candidate_receipt_collector_controls','production_entry_controls')){
  NoiseRepairRequire (@($spec.argv).Count -eq 1 -and $spec.argv[0] -ceq $row.report.path -and $value.qualification_issued -is [bool] -and -not $value.qualification_issued) 'closed production entry/admission output and scope'
  if($target -ceq 'candidate_admission_controls'){
   NoiseRepairRequire ($value.schema -ceq 'pas.candidate-admission-controls.v1' -and $value.assertions -eq 82 -and @($value.checks).Count -eq 82 -and $value.pass -is [bool] -and $value.pass -and $value.synthetic_fixture_only -is [bool] -and $value.synthetic_fixture_only -and $value.gameplay_files_created -eq 0) 'all82 bounded admission controls'
   foreach($check in $value.checks){NoiseRepairRequire ($check.pass -is [bool] -and $check.pass) 'actual admission control PASS'}
  }elseif($target -ceq 'candidate_receipt_collector_controls'){
   NoiseRepairRequire ($value.schema -ceq 'pas.candidate-receipt-collector-controls.v1' -and $value.pass -is [bool] -and $value.pass -and ($value.assertions -is [int] -or $value.assertions -is [long]) -and $value.assertions -eq 77 -and @($value.checks).Count -eq 77 -and $value.synthetic_fixture_only -is [bool] -and $value.synthetic_fixture_only -and $value.device_endpoints -eq 0 -and $value.gameplay_files_created -eq 0) 'all77 bounded synthetic-only current candidate collector controls'
   $ids=@{};foreach($check in $value.checks){NoiseRepairRequire ($check.id -is [string] -and $check.id -and -not $ids.ContainsKey($check.id) -and $check.pass -is [bool] -and $check.pass) 'unique actual collector control PASS';$ids[$check.id]=$true}
  }else{
   NoiseRepairRequire ($value.schema -ceq 'pas.production-entry-closed-controls.v1' -and $value.assertions -eq 13 -and @($value.checks).Count -eq 13 -and $value.actual_production_function_called -is [bool] -and $value.actual_production_function_called) 'all13 actual closed production entry controls'
   for($index=0;$index -lt 13;$index++){$field=if($index -lt 9){'actual_production_entry_rejects_UNQUALIFIED'}else{'pass'};NoiseRepairRequire ($value.checks[$index].$field -is [bool] -and $value.checks[$index].$field) 'actual entry/geometry control PASS'}
  }
 }else{
  $mode=$target.Substring('score_only_'.Length)
  NoiseRepairRequire (@($spec.argv).Count -eq 2 -and $spec.argv[0] -ceq $mode -and [IO.Path]::GetFullPath((Join-Path $spec.argv[1] 'control-report.json')) -ceq [IO.Path]::GetFullPath($row.report.path)) 'actual score-only mode/fresh output join'
  NoiseRepairRequire ($value.schema -ceq 'pas.score-only-literal-controls.v1' -and $value.pass -is [bool] -and $value.pass -and $value.cost_qualification -eq $false -and $value.physical_qualification -eq $false -and $value.whole_candidate_qualification_issued -eq $false -and $value.no_gameplay_files_created -eq $true -and $value.base_component_source_commit -ceq $commit -and $value.base_component_source_dirty -eq $false -and $value.actual_manual_translation_unit_sha256 -ceq $manual.sha256) 'actual current score-only None source/lifecycle scope'
  if($mode -ceq 'invalid'){NoiseRepairRequire ($value.invalid_requests_rejected_before_io -eq 8 -and $value.controlled_stop_lifecycle_cases -eq 2 -and $value.result_tail_fake_clock_cases -eq 4) 'affected score-only invalid/Stop/fake-clock coverage'}
  else{NoiseRepairRequire ($value.scene -ceq $mode -and $value.literal_production_source_executed -eq $true -and $value.all_downs_terminalized -eq $true -and $value.contacts_final -eq 0 -and $value.down -eq $value.up+$value.contacts_released_by_cancel -and $value.release_calls -gt 0 -and ($mode -ceq 'dense' -or $value.down -gt 0)) 'actual score-only scene/contact balance'}
 }
}

# Switch only after every current build/affected-control byte and native join.
$script:builds=@($production);$script:seal=@{files=@($production.files)};$Mode='release'
$artifact=Join-Path $repo ('out/prelive-pre-emulator-production-'+$Attempt)
Fresh $artifact;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$plans=@();foreach($scene in @('tap','hold','dense')){$plans+=@(@{id=('normal-'+$scene);mode='release';scene=$scene;n=2560;warmup=256;measurement=2304;cadence=20})}
$planPath=Join-Path $artifact 'plan.json'
NewJson $planPath @{schema='pas.pre-emulator-production-plan.v1';source_commit=$commit;plans=$plans;production_build=$productionPin;noise_repair_build=$noisePin;production_binary_executed=$false;scope='Necessary current endpoint-free normal3 only; original75/prod9 remain historical prerequisites';qualification_issued=$false;device_operations=0;live_ready=$false}
$inputs=@($script:noiseRepairActiveInputs.Keys)+@($productionPin.path,$noisePin.path,$planPath,$production.source_manifest.path,$production.dependency_manifest.path,$production.core_build_plan.path)+@($manifest.files.path)+@($dependencies.files.path)+@($production.files.path)+$buildProofs.path+@($production.controls.binary.path)+@($production.controls.report.path)+@($production.controls.receipt.path)+@($production.controls.input_closure.path)
$inputs=@($inputs|Sort-Object -Unique)
$Operation='pipeline';$rows=@();$stopped=$false;$reason='';$activePlan=$null;$activeStage='';$activePhase=''
try{foreach($p in $plans){
 AssertCurrentProduction
 $id=$Attempt+'-'+$p.id;$root=Join-Path $artifact $p.id;$stage='production-pre-emulator-'+$id
 $activePlan=$p;$activeStage=$stage;$activePhase='producer'
 Native $stage (Exe 'production_pipeline_cold') @($p.scene,'2560','20',$root) $inputs 900
 $report=Join-Path $root 'report.json';$reportPin=Binding $report;$r=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
 NoiseRepairRequire ($r.schema -ceq 'pas.windows-production-pipeline-cold.v1' -and $r.scene -ceq $p.scene -and $r.source_build_commit -ceq $commit -and $r.source_build_dirty -eq $false -and $r.actual_manual_translation_unit_sha256 -ceq $manual.sha256 -and $r.endpoint_free -eq $true -and $r.live_ready -eq $false -and $r.device_operations -eq 0 -and $r.production_packet_action_wake_archive_executed -eq $true -and $r.public_entry_rejected -eq $true -and $r.real_capture_rpc_measured -eq $false -and $r.requested -eq 2560 -and $r.warmup -eq 256 -and $r.measurement_attempts -eq 2304 -and $r.cadence_ms -eq 20) 'literal current endpoint-free normal2560/warm256/meas2304/cadence20 report'
 $process=ProcessEvidence $stage
 $audit=Join-Path $artifact ($p.id+'-audit.json');$raw=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object FullName)
 AssertCurrentProduction
 $activePhase='auditor';$activeStage='audit-production-pre-emulator-'+$id
 Native $activeStage (Exe 'production_pipeline_audit') @('--root',$root,'--expected-report-sha256',$reportPin.sha256,'--source-commit',$commit,'--output',$audit) @($inputs+$raw+$process.Values.path) 900
 $auditPin=Binding $audit;$a=Get-Content -LiteralPath $audit -Raw|ConvertFrom-Json
 NoiseRepairRequire ($a.schema -ceq 'pas.windows-production-pipeline-audit.v5' -and $a.integrity_verified -eq $true -and $a.source_commit -ceq $commit -and $a.live_ready -eq $false -and $a.device_operations -eq 0 -and $a.physical_qualification -eq $false -and $a.windows_cost_qualification -eq $false) 'independent full raw v5 production audit scope/integrity'
 NoiseRepairSameBinding $a.report $reportPin 'auditor exact current report SHA'
 $auditProcess=ProcessEvidence $activeStage
 $rows+=@(@{id=$p.id;report=$reportPin;audit=$auditPin;process=$process;audit_process=$auditProcess;normal_cost_valid=$a.normal_cost_valid;qualification_issued=$false;live_ready=$false;device_operations=0})
 Write-Output ($p.id+' raw integrity PASS; numerical cost '+$a.normal_cost_valid)
 NoiseRepairRequire ($a.normal_cost_valid -is [bool] -and $a.normal_cost_valid -and @($a.normal_cost_failures).Count -eq 0) ('production necessary original numerical gate failed '+$p.id)
 AssertCurrentProduction
 $bytes=[long]((Get-ChildItem -LiteralPath $artifact -File -Recurse|Measure-Object Length -Sum).Sum)
 if($bytes+1073741824 -gt 12884901888){throw 'production raw capacity headroom'}
}}catch{$stopped=$true;$reason=$_.Exception.Message}
$failure=$null
if($stopped -and $activePlan -and $activePlan.id -notin $rows.id){
 $resultPath=Join-Path $out ($activeStage+'/'+$activeStage+'-result.json')
 $failure=@{id=$activePlan.id;phase=$activePhase;stage=$activeStage;status='FAILED_STAGE';reason=$reason;result=$null}
 if(Test-Path -LiteralPath $resultPath){$failure.result=Binding $resultPath;$native=Get-Content -LiteralPath $resultPath -Raw|ConvertFrom-Json;if($null -ne $native.facts.exit_code){$failure.status='FAILED_EXECUTION';$failure.native_exit=$native.facts.exit_code}}
}
$status=Join-Path $artifact 'status.json'
NewJson $status @{schema='pas.pre-emulator-production-status.v1';plan=(Binding $planPath);source_commit=$commit;production_build=$productionPin;noise_repair_build=$noisePin;rows=$rows;failure=$failure;stopped=$stopped;reason=$reason;remaining=@($plans|Where-Object {$_.id -notin $rows.id -and ($null -eq $failure -or $_.id -cne $failure.id)}|ForEach-Object {@{id=$_.id;status='NOT_RUN'}});qualification_issued=$false;whole_candidate_qualification=$false;capture_rpc_measured=$false;physical_qualification=$false;live_ready=$false;device_operations=0}
Write-Output $status
if($stopped){throw $reason}
