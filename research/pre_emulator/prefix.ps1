param([ValidatePattern('^main[0-9]{2}$')][string]$BuildAttempt='main12',[ValidatePattern('^[a-z0-9-]{1,16}$')][string]$Attempt='main12')
. (Join-Path $PSScriptRoot 'common.ps1');$script:maintenancePath=$PSCommandPath
LoadBuilds $BuildAttempt
$artifact=Join-Path $repo ('out/prelive-pre-emulator-prefix-'+$Attempt);Fresh $artifact;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$manifest=Join-Path $artifact 'source-manifest.json';$inputs+=@(SourceManifest $manifest)+@($manifest)
$selection=Join-Path $repo 'out/retained/consolidation-20261008/worktrees/PAS-windows-readiness-20261007/out/readiness-20261007-01/full-prefix-selection.json'
$selected=Get-Content -LiteralPath $selection -Raw|ConvertFrom-Json;if($selected.frames.Count -ne 3063){throw 'exact 3063 prefix denominator'}
foreach($f in $selected.frames){CheckEntry $f};$inputs+=@($selection)+$selected.frames.path
$Operation='pixels';$report=Join-Path $artifact 'full-prefix.json';$stage='pixels-pre-emulator-'+$Attempt
Native $stage (Exe 'current_chain') @('pixels',$selection,$report) $inputs 900
$r=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json;if($r.frames -ne 3063 -or $r.processed -ne 3063 -or -not $r.release_verified -or $r.stop -or $r.device_endpoints -ne 0){throw 'full prefix safety'}
$audit=Join-Path $artifact 'full-prefix-audit.json';$auditStage='audit-pixels-pre-emulator-'+$Attempt
Native $auditStage (Exe 'prelive_audit') @($report,$audit) @($inputs+$report+($report+'.rows.jsonl')+($report+'.events.jsonl')) 900
$a=Get-Content -LiteralPath $audit -Raw|ConvertFrom-Json;if(-not $a.integrity -or -not $a.complete_prefix_evidence -or -not $a.all_input_processed -or -not $a.plan_join_complete -or -not $a.coordinate_join_complete -or $a.release_uncertainty_seen){throw 'full raw prefix joins'}
NewJson (Join-Path $artifact 'prefix-receipt.json') @{schema='pas.cooperative-prefix-receipt.v1';source_commit=$commit;source_manifest=(Binding $manifest);binary=(Binding (Exe 'current_chain'));selection=(Binding $selection);report=(Binding $report);audit=(Binding $audit);producer_receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')));auditor_receipt=(Binding (Join-Path $out ($auditStage+'/'+$auditStage+'-verification.json')));frames=3063;pass=$true;live_ready=$false;device_operations=0;physical_gold=0}
