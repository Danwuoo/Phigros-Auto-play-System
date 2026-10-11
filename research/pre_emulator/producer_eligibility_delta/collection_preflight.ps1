param([Parameter(Mandatory)][string]$NoiseRepairBinding,
      [Parameter(Mandatory)][string]$NormalStatus,
      [Parameter(Mandatory)][string]$ColdInput,
      [Parameter(Mandatory)][string]$ColdReport,
      [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedScriptSha256,
      [ValidatePattern('^[a-z0-9-]{1,16}$')][string]$Attempt='main69-e1')
$ErrorActionPreference='Stop';$taskCollectionPreflight=$PSCommandPath
. (Join-Path $PSScriptRoot '../common.ps1')
. (Join-Path $PSScriptRoot '../noise_repair_build.ps1')
$script:maintenancePath=$taskCollectionPreflight
$own=Binding $taskCollectionPreflight
if($own.sha256 -cne $ExpectedScriptSha256){throw 'externally pinned cold collection recipe changed'}
LoadNoiseRepairBuild $NoiseRepairBinding
$deltaPin=Binding ([IO.Path]::GetFullPath($NoiseRepairBinding));$delta=NoiseRepairReadBinding $deltaPin
if(-not $delta.cold_harness_delta){throw 'explicit current cold harness required'}
$base=NoiseRepairReadBinding $delta.base_production_build
$normalPin=Binding ([IO.Path]::GetFullPath($NormalStatus));$normal=NoiseRepairReadBinding $normalPin
$coldPin=Binding ([IO.Path]::GetFullPath($ColdInput));$cold=NoiseRepairReadBinding $coldPin
$coldReportPin=Binding ([IO.Path]::GetFullPath($ColdReport));$coldReportValue=NoiseRepairReadBinding $coldReportPin
if($normal.stopped -or @($normal.rows).Count -ne 3 -or $normal.source_commit -cne $delta.base_source_commit -or $normal.execution_source_commit -cne $commit -or -not $normal.cold_harness_delta){throw 'complete current Normal3 required'}
NoiseRepairSameBinding $normal.delta_build $deltaPin 'current normal harness'
if(-not $coldReportValue.pass -or $coldReportValue.status -cne 'COLD_BUNDLE_PASS' -or $coldReportValue.qualification_issued -or $coldReportValue.live_ready){throw 'complete cold bundle required; no admission'}
NoiseRepairSameBinding $cold.narrow_build $deltaPin 'current cold harness'
$profile=Binding (Join-Path $repo 'configs/phigros-hd-assist-five-lead35.json')
$capability=Binding (Join-Path $repo 'measurements/game-semantics-20260927/touch-five/summary.json')
if($profile.bytes -ne 917 -or $profile.sha256 -cne 'd2b5cfdc6be023c715fe402a7124ccd1977f5956b8d9fa74466643a575dbd0e0' -or $capability.bytes -ne 130219 -or $capability.sha256 -cne '73dc5c94f1418a1e75bf4488871d27c849e0d6105adc79ec03c167ff672dbcd3'){throw 'preserved original profile/capability pins differ; current device still unknown'}
$candidate=@($delta.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'pas.exe'})
if($candidate.Count -ne 1){throw 'one actual relinked verification pas required'}
$candidate=$candidate[0]
$artifact=Join-Path $repo ('out/prelive-cold-collection-preflight-'+$Attempt);Fresh $artifact;$null=[IO.Directory]::CreateDirectory($artifact)
$identity=@{evidence_kind='actual';source_commit=$delta.base_source_commit;candidate_binary_sha256=$candidate.sha256;profile_sha256=$profile.sha256;capability_sha256=$capability.sha256}
$production=@{};foreach($entry in $identity.GetEnumerator()){$production[$entry.Key]=$entry.Value}
$production.schema='pas.current-production-chain-qualification.v1';$production.status=$normalPin;$production.cold_preflight_input=$coldPin
$normalScenes=@();$failed=0;$unknown=0
foreach($row in $normal.rows){
 $audit=NoiseRepairReadBinding $row.audit;$report=NoiseRepairReadBinding $row.report
 if($audit.schema -cne 'pas.windows-production-pipeline-audit.v5' -or -not $row.normal_cost_valid -or -not $audit.integrity_verified -or -not $audit.normal_cost_valid -or @($audit.normal_cost_failures).Count){throw 'original numerical gates and full raw audit required'}
 $normalScenes+=@($report.scene);$failed+=@($report.round_summary.release_failed_ids).Count;$unknown+=@($report.round_summary.release_unknown_ids).Count
}
if(($normalScenes -join ',') -cne 'tap,hold,dense' -or $failed -or $unknown){throw 'actual ordered Normal3 clean release required'}
$production.normal_scenes=$normalScenes;$production.failed=$failed;$production.unknown=$unknown
# Full-raw v5 integrity verifies every backend success and ContactLedger.finish()
# (no active contacts). These zeros derive from that actual audit contract.
$production.final_active_contacts=0;$production.full_raw_v5_zero_proofs=@($normal.rows.audit)
foreach($field in @('pass','production_packet_action_wake_pool_verified','sole_touch_owner_verified','bounded_all_attempts_verified')){$production[$field]=$true}
$production.qualification_issued=$false;$production.live_ready=$false;$production.device_operations=0
$productionPath=Join-Path $artifact 'production-cost.json';NewJson $productionPath $production
$authorization=@{};foreach($entry in $identity.GetEnumerator()){$authorization[$entry.Key]=$entry.Value}
$authorization.schema='pas.current-score-only-authorization.v1';$authorization.scope='chapter_legacy_available_in_single_pass_scores_only';$authorization.persistence='none'
foreach($field in @('one_round','no_unlock','no_retry','stop_after_pass','device_operations_authorized')){$authorization[$field]=$true}
$authorization.user_scope=(Binding (Join-Path $repo 'docs/research/live-readiness-20261007/SINGLE_PASS_SCOPE_20261009.md'))
$authorization.current_task_device_operations=0;$authorization.new_thread_now_authorized=$false;$authorization.qualification_issued=$false;$authorization.live_ready=$false
$authorizationPath=Join-Path $artifact 'authorization.json';NewJson $authorizationPath $authorization
# Import the preserved Native8 metadata helper, without executing its recipe.
$proofRecipe=Join-Path $repo 'research/windows_production_pipeline_v3/wait_diagnostic_component/build.ps1'
$tok=$null;$err=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($proofRecipe,[ref]$tok,[ref]$err)
if($err.Count){throw 'preserved Native8 helper AST'}
foreach($name in @('DRequire','DRead','DBool','DInt','DSame','DProof')){
 $functions=@($ast.FindAll({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name)
 if($functions.Count -ne 1){throw ('one preserved helper required: '+$name)};. ([scriptblock]::Create($functions[0].Extent.Text))
}
$catalogue=@{};foreach($row in $delta.base_original_build_processes){$catalogue[$row.Native8.verification.path]=$row.Native8}
foreach($row in $base.controls){$v=NoiseRepairReadBinding $row.receipt;$proof=DProof $v.stage;$catalogue[$proof.verification.path]=$proof}
foreach($row in $normal.rows){foreach($proof in @($row.process,$row.auditor_process)){$catalogue[$proof.verification.path]=$proof}}
if($delta.motion_source_delta){
 $proof=$delta.motion_source_delta.regression.process
 if(-not $proof.verification.path){throw 'actual motion regression Native8 required'}
 $catalogue[$proof.verification.path]=$proof
}
$inputPath=Join-Path $artifact 'collection-input.json'
NewJson $inputPath @{schema='pas.current-candidate-collection-input.v1';evidence_kind='actual';source_commit=$delta.base_source_commit;candidate_binary=$candidate;source_manifest=$base.source_manifest;production_build=$delta.base_production_build;cold_preflight_input=$coldPin;runtime_profile=$profile;capability=$capability;production_cost=(Binding $productionPath);authorization=(Binding $authorizationPath);cold_harness_delta=$deltaPin;verification_delta=$delta.verification_delta;native_processes=@($catalogue.Values);qualification_processes=@{};device_operations=0;qualification_issued=$false;live_ready=$false} 4194304
$inputPin=Binding $inputPath
# Reuse the actual final readonly closure, then add current Normal/control
# metadata. The C++ factory independently checks every transitive receipt.
$stage='readonly-eligibility-'+$Attempt+'-full';$readerProof=ProcessEvidence $stage
$closure=NoiseRepairReadBinding $readerProof.input_closure;$root=NoiseRepairReadBinding $closure.root
$actualPaths=@();foreach($leaf in $root.leaves){$value=NoiseRepairReadBinding $leaf;$actualPaths+=@($value.files.path)}
foreach($proof in $catalogue.Values){
 foreach($role in @('specification','input_closure','result','verification','freeze','state','contract','command')){$actualPaths+=@($proof.$role.path)}
 $c=NoiseRepairReadBinding $proof.input_closure;$actualPaths+=@($c.root.path);$r=NoiseRepairReadBinding $c.root;$actualPaths+=@($r.leaves.path)
}
$inputs+=@($actualPaths)+@($inputPath,$productionPath,$authorizationPath,$normalPin.path,$normal.plan.path,$coldPin.path,$coldReportPin.path,$profile.path,$capability.path,$own.path,$proofRecipe)+@($normal.rows.report.path)+@($normal.rows.audit.path)
$stage='cold-collection-preflight-'+$Attempt;$Operation='tests';$Campaign=''
Native $stage $candidate.path @('candidate-preflight','--workspace',$repo,'--config',$profile.path,'--capability',$capability.path,'--admission-input',$inputPath,'--admission-sha256',$inputPin.sha256) $inputs 900 2
$stdout=Join-Path $out ($stage+'/'+$stage+'.stdout.log');$report=Get-Content -LiteralPath $stdout -Raw|ConvertFrom-Json
if($report.pass -or $report.qualification_issued -or $report.live_ready -or -not $report.cold_collection_inputs_valid -or @($report.issues).Count -ne 1 -or $report.issues[0].code -cne 'CURRENT_DEVICE_COLLECTION_REQUIRED'){throw 'actual factory did not stop solely at the current device collection boundary'}
NewJson (Join-Path $artifact 'result.json') @{schema='pas.cold-collection-preflight-boundary.v1';source_commit=$commit;base_formal_source_commit=$delta.base_source_commit;input=$inputPin;report=(Binding $stdout);Native8=(ProcessEvidence $stage);cold_collection_inputs_valid=$true;qualification_issued=$false;cost_qualification=$false;live_ready=$false;device_operations=0;new_thread_created=$false;new_etw_recordings=0}
Write-Output 'actual new factory cold joins PASS; stopped before current device collection'
