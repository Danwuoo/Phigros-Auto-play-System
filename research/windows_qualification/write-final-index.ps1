param([Parameter(Mandatory)][string]$Prerequisites,
 [Parameter(Mandatory)][string]$CampaignStatus,
 [Parameter(Mandatory)][string]$Output,
 [string]$RawAudit='')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$pkg=Join-Path $repo 'out/prelive-20261006';$owned=Join-Path $repo 'out/windows-handoff'
function Bind($path){$item=Get-Item -LiteralPath $path;@{path=$item.FullName;bytes=[long]$item.Length;sha256=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()}}
function Check($entry){$actual=Bind $entry.path;if($actual.bytes -ne $entry.bytes -or $actual.sha256 -cne $entry.sha256){throw ('retained-evidence-changed '+$entry.path)}}
if(Test-Path -LiteralPath $Output){throw 'fresh-final-index-required'}
$pre=Get-Content -LiteralPath $Prerequisites -Raw|ConvertFrom-Json
if(-not $pre.pass -or $pre.stages.Count -ne 83 -or $pre.device_operations -ne 0){throw 'complete-cold-prerequisites'}
foreach($entry in $pre.bindings){Check $entry}
$stages=@()
foreach($stage in $pre.stages){
 $verification=Join-Path $owned ($stage+'/'+$stage+'-verification.json')
 $result=Join-Path $owned ($stage+'/'+$stage+'-result.json')
 $state=Get-Content -LiteralPath (Join-Path $owned ($stage+'/state.json')) -Raw|ConvertFrom-Json
 $v=Get-Content -LiteralPath $verification -Raw|ConvertFrom-Json
 $facts=(Get-Content -LiteralPath $result -Raw|ConvertFrom-Json).facts
 if($state.status -cne 'COMPLETE' -or $v.native_exit -ne 0 -or $v.runner_exit -ne 0 -or $v.verification_exit -ne 0 -or
  -not $facts.identity_trusted -or -not $facts.held_all_signaled -or -not $facts.streams_completed -or $facts.active_final -ne 0){throw ('stage-not-complete '+$stage)}
 $stages+=@(@{stage=$stage;verification=(Bind $verification);result=(Bind $result);elapsed_s=$facts.elapsed_s;
  native_exit=0;runner_exit=0;verification_exit=0;identity_trusted=$true;held_all_signaled=$true;streams_completed=$true;active_final=0})
}
$status=Get-Content -LiteralPath $CampaignStatus -Raw|ConvertFrom-Json
if($status.schema -cne 'pas.windows-aa-batch-status.v1' -or $status.source_commit -cne $pre.source_commit -or $status.device_operations -ne 0){throw 'same-source-cost-batch'}
Check $status.campaign
$campaign=Get-Content -LiteralPath $status.campaign.path -Raw|ConvertFrom-Json
foreach($entry in $campaign.bindings){Check $entry}
foreach($run in $status.runs){
 Check $run
 if((Bind $run.audit_path).sha256 -cne $run.audit_sha256){throw 'raw-audit-binding-changed'}
 foreach($entry in $run.process_evidence.PSObject.Properties.Value){Check $entry}
}
$cost=@()
foreach($plan in $campaign.planned_runs){
 $row=@{run_id=$plan.run_id;scene=$plan.scene;produced=$false}
 if(Test-Path -LiteralPath $plan.report_path){
  $raw=Get-Content -LiteralPath $plan.report_path -Raw|ConvertFrom-Json
  $row.produced=$true;$row.report=Bind $plan.report_path
  $row.attempts=Bind ($plan.report_path+'.attempts.json');$row.journal=Bind ($plan.report_path+'.journal.jsonl')
  $row.raw_measurement=@{};foreach($name in @('requested','attempts','warmup_attempts','cadence_ms','owner_seen','owner_seen_fraction','consumed','pool_drops',
   'writer_drops','writer_fault','journal_admission_failed','safe','supervisor_stopped','injection_failed','release_verified','own_down','own_move','own_up',
   'all_phase_lateness_ms','capture_to_owner_ms','capture_to_owner_journal_ms','measurement','producer_render_ms','producer_schedule_lateness_ms',
   'producer_deadline_overruns','publication_deadline_overruns','rss_peak_os','source_absolute_age','device_endpoints')){
   if($raw.PSObject.Properties.Name -contains $name){$row.raw_measurement[$name]=$raw.$name}
  }
  $stage='pipeline-prelive-release-'+$plan.run_id
  foreach($name in @('state.json',($stage+'-result.json'),($stage+'-verification.json'))){
   $path=Join-Path $owned ($stage+'/'+$name);if(Test-Path -LiteralPath $path){$row[$name]=Bind $path}
  }
 }
 $cost+=@($row)
}
$stops=@();foreach($folder in Get-ChildItem -LiteralPath $owned -Directory){
 $path=Join-Path $folder.FullName 'state.json';if(-not (Test-Path -LiteralPath $path)){continue}
 $state=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json
 if($state.status -ceq 'STOP'){$stops+=@(@{stage=$folder.Name;state=(Bind $path);reason=$state.reason})}
}
$inputs=@((Bind $Prerequisites),(Bind $CampaignStatus),$status.campaign,(Bind (Join-Path $repo 'out/readiness-20261007-01/compact-archive-verification.json')),
 (Bind (Join-Path $repo 'out/readiness-20261007-01/protected-input-restoration.json')),
 (Bind (Join-Path $repo 'out/readiness-20261007-01/reader-controls-02/report.json')))
if($RawAudit){$inputs+=@(Bind $RawAudit)}
foreach($run in $status.runs){$inputs+=@(Bind $run.audit_path)}
foreach($entry in $status.statuses){$inputs+=@($entry.verdict);Check $entry.verdict}
$failure=Join-Path $owned 'gate-prelive-release-aa01-tap3/gate-prelive-release-aa01-tap3-result.json'
if(Test-Path -LiteralPath $failure){$inputs+=@(Bind $failure);$failedFacts=(Get-Content -LiteralPath $failure -Raw|ConvertFrom-Json).facts}else{$failedFacts=$null}
$archives=@(Get-ChildItem -LiteralPath 'C:/Users/wurre/Downloads/PAS-readiness-retained-builds-20261007' -File -Filter '*.verified.json'|ForEach-Object {Bind $_.FullName})
$files=@(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -Recurse -File)
$value=@{schema='pas.windows-readiness-result-index.v1';time_utc=[DateTime]::UtcNow.ToString('o');measured_source_commit=$pre.source_commit;
 cold_functional_verified=$true;cold_stage_count=83;stages=$stages;source_build_modes=@('release','debug','asan');WIC_prefix_frames=3063;
 reader_positive_and_negative_controls=15;cost_status=(Bind $CampaignStatus);cost_runs=$cost;cost_statuses=$status.statuses;cost_remaining=$status.remaining;
 cost_failed_process_facts=$failedFacts;retained_STOPs=$stops;inputs=$inputs;retained_build_archives=$archives;
 out_capacity=@{files=$files.Count;bytes=[long](($files|Measure-Object Length -Sum).Sum);cap_bytes=12884901888;free_bytes=[IO.DriveInfo]::new('C:\').AvailableFreeSpace};
 admission=@{state='UNQUALIFIED';enabled=$false;full_live_validator_implemented=$false;production_packet_cost_qualified=$false;physical_qualified=$false;
  device_commands=0;touches=0;emulator_started_by_this_task=$false;selected_device='emulator (historical profile emulator-5554, current serial unverified)';play_owner='user'};
 all_songs_IN_miss_zero=@{complete=$false;current_roster='unknown';current_IN_unlocks='unknown'};
 scope='Evidence index and cold qualification only. Raw numerical distributions do not override failed process/cost gates. No live admission token.'}
$bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 40)+"`n")
if($bytes.Length -ge 67108864){throw 'final-index-metadata-cap'}
[IO.File]::WriteAllBytes($Output,$bytes)
Bind $Output|ConvertTo-Json -Compress
