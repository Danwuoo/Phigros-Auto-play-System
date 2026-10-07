param([Parameter(Mandatory)][string]$Report)
$ErrorActionPreference='Stop'
$taskRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
function Binding($path){$file=Get-Item -LiteralPath $path;@{path=$file.FullName;bytes=[long]$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()}}
function Read($path){Get-Content -LiteralPath $path -Raw|ConvertFrom-Json}
function Check($entry){$actual=Binding $entry.path;if($actual.bytes -ne $entry.bytes -or $actual.sha256 -cne $entry.sha256){throw ('changed-binding '+$entry.path)}}
$gatePath=Join-Path $taskRepo 'out/windows-handoff/guard-v3-native-01/native-gate.json'
$gate=Read $gatePath
foreach($entry in @($gate.guard_source,$gate.process_source,$gate.diagnostic,$gate.cached_identity_controls,$gate.identity_wait_controls)){Check $entry}
$qualified=@()
foreach($row in $gate.native_controls){
 Check $row.receipt;$v=Read $row.receipt.path;foreach($entry in $v.entries){Check $entry}
 $resultPath=$row.receipt.path.Replace('-verification.json','-result.json');$facts=(Read $resultPath).facts
 if($facts.identity_trusted -ne $true -or $facts.active_final -ne 0 -or $facts.held_all_signaled -ne $true -or $facts.streams_completed -ne $true -or $facts.errors.Count -or $facts.errors_overflow -or $v.verification_exit -ne 0){throw 'native-control-integrity'}
 $qualified+=@{stage=$row.stage;verification=$row.receipt;result=(Binding $resultPath);facts=$facts}
}
$evaluationPath=Join-Path $taskRepo 'out/windows-handoff/guard-v3-evaluation-01/evaluation.json'
$evaluation=Read $evaluationPath
if($evaluation.completed -ne 27 -or $evaluation.stopped){throw 'complete-v3-controls-required'}
$fallbacks=@()
foreach($row in $evaluation.rows){Check $row.receipt;$v=Read $row.receipt.path;foreach($entry in $v.entries){Check $entry};foreach($snapshot in $row.facts.snapshots){foreach($member in $snapshot.members){if($member.image_query_error){$fallbacks+=@{stage=$row.stage;snapshot=$snapshot.stage;member=$member;verification=$row.receipt}}}}}
$campaignRoot=Join-Path $taskRepo 'out/prelive-aa-aa04'
$status=Read (Join-Path $campaignRoot 'status.json')
if(-not $status.normal_stopped -or $status.reason -cne 'necessary-cost-validity-failed aa04-tap1'){throw 'actual-numeric-stop-required'}
$stages=@()
foreach($name in @('pipeline-prelive-release-aa04-tap1','audit-prelive-release-aa04-tap1','gate-prelive-release-aa04-tap1')){
 $dir=Join-Path $taskRepo ('out/windows-handoff/'+$name)
 $state=Read (Join-Path $dir 'state.json');$vp=Join-Path $dir ($name+'-verification.json');$v=Read $vp
 if($state.status -cne 'COMPLETE' -or $v.verification_exit -ne 0 -or $v.native_exit -ne 0 -or $v.runner_exit -ne 0){throw 'actual-process-COMPLETE-required'}
 foreach($entry in @($state.receipts)+@($v.entries)){Check $entry}
 $f=(Read (Join-Path $dir ($name+'-result.json'))).facts
 $stages+=@{stage=$name;verification=(Binding $vp);state=(Binding (Join-Path $dir 'state.json'));facts=$f}
}
$rawPath=Join-Path $taskRepo 'out/prelive-20261006/pipeline-release-aa04-tap1.json';$raw=Read $rawPath
$auditPath=Join-Path $campaignRoot 'aa04-tap1-audit.json';$audit=Read $auditPath
$source=Read (Join-Path $taskRepo 'out/prelive-aa-aa01/source-manifest.json');foreach($entry in $source.files){Check $entry}
$negative=@();foreach($relative in @('out/windows-handoff/gate-prelive-release-aa01-tap3/state.json',
 'out/windows-handoff/guard-v2-native-01/probe-1/state.json','out/windows-handoff/guard-v2-attempt01-source/preservation.json',
 'out/windows-handoff/guard-v2-evaluation-01/evaluation.json','out/windows-handoff/guard-v2-evaluation-01/evaluation-reviewed.json',
 'out/windows-handoff/guard-v2-evaluation-01/review-writer-01.ps1','out/prelive-aa-aa02/status.json',
 'out/prelive-aa-aa03/status.json','out/prelive-aa-aa04/status.json')){$negative+=@(Binding (Join-Path $taskRepo $relative))}
$capacity=@{all_OUT_bytes=[long](Get-ChildItem -LiteralPath (Join-Path $taskRepo 'out') -File -Recurse|Measure-Object Length -Sum).Sum;
 cap=12884901888;free=[long](Get-PSDrive C).Free;minimum_free=21474836480}
$reportValue=@{schema='pas.windows-job-identity-repair-results.v1';time_utc=[DateTime]::UtcNow.ToString('o');
 candidate_source_commit=$source.source_commit;maintenance_commit='730de45d31813961ecdd03e5953671f300407c63';
 original_main_commit=(& git -C 'C:/Users/wurre/Desktop/Phigros-Auto-play-System' rev-parse HEAD);
 guard_version='pas.owned-held-identity.v3';guard_gate=(Binding $gatePath);native_controls=$qualified;
 file_only_identity_wait_cases=39;file_only_cached_identity_cases=9;file_only_diagnostic_cases=20;
 evaluation=(Binding $evaluationPath);actual_control_count=27;actual_fault_branch=$fallbacks;
 aa04_campaign=(Binding (Join-Path $campaignRoot 'campaign.json'));aa04_status=(Binding (Join-Path $campaignRoot 'status.json'));
 aa04_process_stages=$stages;cost_gate=(Binding (Join-Path $campaignRoot 'aa04-tap1-gate.json'));
 raw_bindings=@(Binding $rawPath;Binding ($rawPath+'.attempts.json');Binding ($rawPath+'.journal.jsonl'));audit=(Binding $auditPath);
 full=@{owner_seen=$raw.owner_seen;requested=$raw.requested;capture_to_owner_ms=$raw.capture_to_owner_ms;all_phase_lateness_ms=$raw.all_phase_lateness_ms;down=$raw.own_down;move=$raw.own_move;up=$raw.own_up};
 measurement=$raw.measurement;measurement_phase_counts=$audit.measurement_phase_counts;measurement_phase_lateness_ms=$audit.measurement_all_phase_lateness_ms;
 rss_peak_OS_bytes=$raw.rss_peak_os;pool_drops=$raw.pool_drops;writer_drops=$raw.writer_drops;writer_fault=$raw.writer_fault;injection_failed=$raw.injection_failed;release_verified=$raw.release_verified;
 retained_negative_evidence=$negative;original_index=(Binding (Join-Path $taskRepo 'docs/research/live-readiness-20261007/result-index.json'));
 source_rows_rehashed=$source.files.Count;capacity=$capacity;AA12_qualified=$false;noise_qualified=$false;ABBA='NOT_RUN';
 production_packet_cost_qualified=$false;live_admission='UNQUALIFIED';emulator_launches=0;device_commands=0;touches=0;pushes=0;all_songs_IN_miss_zero='NOT_COMPLETED';writer=(Binding $PSCommandPath)}
$bytes=[Text.Encoding]::UTF8.GetBytes(($reportValue|ConvertTo-Json -Depth 50)+"`n")
if($bytes.Length -gt 1048576){throw 'result-index-1MiB-cap'}
$handle=[IO.FileStream]::new($Report,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try{$handle.Write($bytes);$handle.Flush($true)}finally{$handle.Dispose()}
Binding $Report|ConvertTo-Json -Compress
