param([Parameter(Mandatory)][string]$Attempt,[Parameter(Mandatory)][string]$BuildAttempt,
 [Parameter(Mandatory)][string]$PixelAttempt,[Parameter(Mandatory)][string]$AuditStage,
 [Parameter(Mandatory)][string]$AuditReport,[Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'));$pkg=Join-Path $repo 'out/prelive-20261006';$out=Join-Path $repo 'out/windows-handoff'
$tokens=$null;$errors=$null;$recipe=Join-Path $repo 'research/prelive_current/run.ps1'
$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'shared-parser'}
foreach($name in @('NewJson','Fresh','Binding','CheckEntry','Receipt')){$f=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name);if($f.Count -ne 1){throw 'shared-function'};. ([scriptblock]::Create($f[0].Extent.Text))}
$commit=(& git -C $repo rev-parse HEAD);if((& git -C $repo status --porcelain)){throw 'clean-prerequisite-source'}
$bindings=@((Binding $PSCommandPath),(Binding (Join-Path $repo 'research/windows_qualification/write-prerequisites.ps1')));$stages=@();$modes=@()
foreach($mode in @('release','debug','asan')){
 $selectedBuild=if($mode -eq 'release'){'win04'}else{$BuildAttempt}
 $binding=Join-Path $pkg ('current-build-'+$mode+'-'+$selectedBuild+'.json');$build=Get-Content -LiteralPath $binding -Raw|ConvertFrom-Json
 if($build.target_commit -cne $commit){throw 'same-current-source-build'}
 foreach($b in @($build.files)+@($build.source_files)+@($build.migration_targets)+@($build.input_closure_receipts)+@($build.build_receipts)){CheckEntry $b}
 $bindings+=@(Binding $binding)
 foreach($kind in @('prefix_preservation','front_preservation','own_prefix','current_tests','formal_affected','active_body_tests','tail_guard_tests','runtime_finish_tests','runtime_finish_pixels','cost_workload_tests','tail_only_owner_review','tail_only_pixels_review','disabled_live_seam_tests','live_seam_packet_review','live_seam_runtime_review','streaming_workload_tests','producer_timing_tests','offline_output_tests','cost_unit','cost_review','input_closure_review','producer_review')){
  $selectedAttempt=if($mode -eq 'asan' -and $kind -notin @('prefix_preservation','front_preservation','own_prefix','current_tests','formal_affected','active_body_tests','tail_guard_tests','runtime_finish_tests','runtime_finish_pixels')){'regress05'}else{$Attempt}
  $stages+=@($kind.Replace('_','-')+'-prelive-'+$mode+'-'+$selectedAttempt)
 }
 foreach($kind in @('family-v1','verify-family-v1','family-v2','verify-family-v2','normal-tail-family')){
  $selectedAttempt=if($mode -eq 'asan'){'regress05'}elseif($mode -eq 'debug' -and $kind -in @('family-v2','verify-family-v2','normal-tail-family')){'regress06'}else{$Attempt}
  $stages+=@($kind+'-prelive-'+$mode+'-'+$selectedAttempt)
 }
 $affected=Get-Content -LiteralPath (Join-Path $pkg ('formal_affected-'+$mode+'-'+$Attempt+'.json')) -Raw|ConvertFrom-Json
 if($affected.tests -ne 186 -or $affected.failures -ne 0 -or $affected.disabled -ne 0){throw 'affected-tests-exact-denominator'}
 $reviewAttempt=if($mode -eq 'asan'){'regress05'}else{$Attempt}
 foreach($folder in @('cost-review','input-closure-review','producer-review')){$path=Join-Path $pkg ($folder+'-'+$mode+'-'+$reviewAttempt+'/review_result.json');$result=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json;if(-not $result.pass){throw 'independent-controls'};$bindings+=@(Binding $path)}
 $modes+=@($mode)
}
$stages+=@(('pixels-prelive-release-'+$PixelAttempt),$AuditStage)
foreach($stage in $stages){Receipt $stage;$path=Join-Path $pkg ($stage+'-input-closure-verification.json');$closure=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json;
 if($closure.status -cne 'COMPLETE' -or -not $closure.pass -or -not $closure.before_verified -or -not $closure.after_verified){throw 'complete-before-after-closure'}
 $bindings+=@(Binding $path)
}
$pixelPath=Join-Path $pkg ('pixels-release-'+$PixelAttempt+'.json');$pixel=Get-Content -LiteralPath $pixelPath -Raw|ConvertFrom-Json
if($pixel.frames -ne 3063 -or $pixel.processed -ne 3063 -or -not $pixel.release_verified -or $pixel.device_endpoints -ne 0){throw 'complete-current-WIC-prefix'}
$audit=Get-Content -LiteralPath $AuditReport -Raw|ConvertFrom-Json
foreach($field in @('integrity','complete_prefix_evidence','all_input_processed','metrics_verified','plan_join_complete','coordinate_join_complete')){if(-not $audit.$field){throw ('independent-prefix-'+$field)}}
if($audit.release_uncertainty_seen -or $audit.errors.Count){throw 'prefix-audit-errors'}
$bindings+=@((Binding $pixelPath),(Binding $AuditReport));Fresh $Output
$reader=Join-Path $repo 'out/readiness-20261007-01/reader-controls-02/report.json';$r=Get-Content -LiteralPath $reader -Raw|ConvertFrom-Json
if(-not $r.pass -or $r.cases.Count -ne 15 -or $r.device_operations -ne 0){throw 'actual-reader-controls'}
$bindings+=@((Binding $reader),(Binding (Join-Path $repo 'out/functional-gui-build-03/pas_functional_envelope.exe')),
 (Binding (Join-Path $repo 'out/readiness-20261007-01/functional-gui-source/runner.cpp')),
 (Binding (Join-Path $repo 'out/readiness-20261007-01/functional-gui-stage.ps1')),
 (Binding (Join-Path $repo 'out/readiness-20261007-01/run-functional-gui.ps1')),
 (Binding (Join-Path $repo 'out/readiness-20261007-01/run-functional-gui-asan-remaining.ps1')),
 (Binding (Join-Path $repo 'out/readiness-20261007-01/run-functional-gui-debug-family2.ps1')),
 (Binding (Join-Path $repo 'out/readiness-20261007-01/pixels-gui.ps1')))
NewJson $Output @{schema='pas.windows-cold-prerequisites.v1';source_commit=$commit;pass=$true;modes=$modes;stages=$stages;bindings=$bindings;
 WIC_prefix_frames=3063;physical_gold=0;production_packet_cost_qualified=$false;live_ready=$false;device_operations=0}
Binding $Output|ConvertTo-Json -Compress
