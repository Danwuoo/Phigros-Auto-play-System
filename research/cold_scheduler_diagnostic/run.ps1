param([Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$FrozenHead,
      [Parameter(Mandatory)][string]$ToolBinding,
      [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedToolBindingSha256,
      [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedRunnerSha256,
      [ValidatePattern('^[a-z0-9-]{1,40}$')][string]$Attempt='main62-r03')
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $workspace 'research/pre_emulator/common.ps1')
. (Join-Path $workspace 'research/cold_scheduler_diagnostic/capacity.ps1')
$script:maintenancePath=$PSCommandPath;$Operation='pipeline'
function Require([bool]$ok,[string]$why){if(-not $ok){throw ('cold scheduler diagnostic: '+$why)}}
function ReadPin($pin,[long]$cap=4194304){CheckEntry $pin;Require ($pin.bytes -le $cap) 'metadata cap';Get-Content -LiteralPath $pin.path -Raw|ConvertFrom-Json}
function Bool($value,[string]$key,[bool]$expected){Require ($value.$key -is [bool] -and $value.$key -eq $expected) ('typed '+$key)}
function AtomicJson([string]$path,$value){Require (-not(Test-Path -LiteralPath $path)) 'fresh atomic output';$temp=$path+'.publish-tmp';NewJson $temp $value;[IO.File]::Move($temp,$path,$false)}
function PrepareNative([string]$stage,[string]$executable,$arguments,$stageInputs,[int]$seconds=900){
 Require ($executable -in $seal.files.path) 'sealed native executable'
 $spec=Join-Path $pkg ($stage+'-direct-spec.json')
 $closureInputs=InputClosure $stage @($stageInputs+$script:maintenancePath+$pwsh+$gate+$tool+(Join-Path $repo 'research/main_consolidation/stage.ps1'))
 $direct=@($closureInputs)+@($script:maintenancePath,$pwsh,$gate,$tool,(Join-Path $repo 'research/main_consolidation/stage.ps1'),(Join-Path $pkg ($stage+'-params.json')))
 NewJson $spec @{stage_root=(Join-Path $out $stage);stage=$stage;exe=$executable;argv=@($arguments);native_exit=0;runner_exit=0;total_s=$seconds;stream_cap=4194304;inputs=$direct}
 return $spec
}
$runnerPin=Binding $script:maintenancePath;Require ($runnerPin.sha256 -ceq $ExpectedRunnerSha256) 'external runner SHA'
$toolPin=Binding ([IO.Path]::GetFullPath($ToolBinding));Require ($toolPin.sha256 -ceq $ExpectedToolBindingSha256) 'fresh tools binding SHA'
$t=ReadPin $toolPin
Require ($t.schema -ceq 'pas.cold-scheduler-diagnostic-build.v1' -and $t.source_commit -ceq $FrozenHead -and $t.translation_unit_count -eq 3 -and $t.actual_native_stage_count -eq 4) 'fresh standalone three-TU tools'
foreach($field in @('diagnostic_only','original_negatives_preserved')){Bool $t $field $true}
foreach($field in @('qualification_issued','whole_candidate_qualification','cost_qualification','physical_qualification','live_ready','recording_started','formal_rebuilt')){Bool $t $field $false}
Require ($t.device_operations -eq 0 -and $t.measurement_runs -eq 0) 'tool build scope'
foreach($reportPin in @($t.controller_controls_report,$t.reader_controls_report)){$controlReport=ReadPin $reportPin;Bool $controlReport 'pass' $true;Require ($controlReport.failed -eq 0) 'tool controls'}
$controller=@($t.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'cold_scheduler_trace_controller.exe'})
$reader=@($t.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'cold_scheduler_trace_reader.exe'})
Require ($controller.Count -eq 1 -and $reader.Count -eq 1) 'unique sealed tools'
$bindingPin=$t.original_component57;$b=ReadPin $bindingPin
Require ($b.schema -ceq 'pas.production-wait-diagnostic-component-build.v1' -and $b.delta_source_commit -ceq '5dce75d12c63ed51de5d996cedcd44e29d729841' -and $b.base_source_commit -ceq '12ec0550e882e3b06751d67fa67c43dda3e9312b') 'original component57/base55 identity'
foreach($field in @('qualification_issued','cost_qualification','physical_qualification','live_ready','formal_rebuilt')){Bool $b $field $false};Bool $b 'diagnostic_only' $true
$manifest=ReadPin $b.source_manifest;$base=ReadPin $b.base_production_build
$producer=@($b.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'production_wait_diagnostic_component.exe'})
$auditor=@($base.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'production_pipeline_audit.exe'})
Require ($producer.Count -eq 1 -and $auditor.Count -eq 1) 'component producer and original auditor'
$launcherPin=Binding (Join-Path $PSScriptRoot 'elevated-launch.ps1')
$pins=@($runnerPin,$toolPin,$launcherPin,$t.source_manifest,$t.source_snapshot,$t.core_build_plan,$t.controller_controls_report,$t.reader_controls_report,$t.original_normal55_negative,$t.original_AA_r03_negative,$bindingPin,$b.source_manifest,$b.core_build_plan,$b.base_production_build,$b.base_cpu_build,$b.base_source_snapshot,$b.base_manual_source,$b.controls_report)+@($t.files)+@($manifest.files)+@($b.files)+@($auditor)
foreach($proof in @($t.configure_process,$t.build_process,$t.controller_controls_process,$t.reader_controls_process,$b.configure_process,$b.build_process,$b.controls_process)){$pins+=@($proof.PSObject.Properties.Value);$v=ReadPin $proof.verification;Receipt $v.stage}
foreach($pin in $pins){CheckEntry $pin}
$script:seal=@{files=@($t.files)+@($b.files)+@($auditor)}
function AssertSource {Require ((& git -C $repo rev-parse HEAD) -ceq $FrozenHead -and (& git -C $repo branch --show-current) -ceq 'main' -and -not (& git -C $repo status --porcelain)) 'clean new execution identity';foreach($pin in $pins){CheckEntry $pin}}
AssertSource
$artifact=Join-Path $repo ('out/prelive-cold-scheduler-'+$Attempt);Fresh $artifact;$null=[IO.Directory]::CreateDirectory($artifact)
$root=Join-Path $artifact 'tap';$planPath=Join-Path $artifact 'plan.json'
NewJson $planPath @{schema='pas.cold-scheduler-measurement-plan.v1';execution_source_commit=$FrozenHead;tool_build=$toolPin;component_build=$bindingPin;compiled_component_source_commit=$b.delta_source_commit;base_source_commit=$b.base_source_commit;scene='tap';requested=2560;cadence_ms=20;diagnostic_only=$true;single_attempt=$true;logging_mode='sequential';log_mode=37748737;hybrid_shutdown_policy='stop';enable_flags=268437520;trace_duration_limit_s=75;trace_buffer_limit_bytes=67108864;trace_raw_limit_bytes=134217728;qualification_issued=$false;normal3_runs=0;AA12_runs=0;noise18_runs=0;ABBA36_runs=0;device_operations=0;live_ready=$false}
$launchPath=Join-Path $artifact 'launch-plan.json'
NewJson $launchPath @{schema='pas.cold-scheduler-launch-plan.v1';workspace=$repo;artifact_root=$artifact;execution_source_commit=$FrozenHead;launcher=$launcherPin;controller=$controller[0];component_build=$bindingPin;producer=$producer[0];session_name=('PAS-ColdScheduler-'+$Attempt);session_guid=([guid]::NewGuid().ToString());diagnostic_only=$true;live_ready=$false;device_operations=0}
$launchPin=Binding $launchPath;$guard=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$inputs=@($pins.path)+@($planPath,$launchPath)+(GateInputs)+@($guard.guard_source.path,$guard.process_source.path,$guard.cached_identity_controls.path,$guard.identity_wait_controls.path)
$producerStage='producer-cold-scheduler-'+$Attempt;$auditorStage='audit-cold-scheduler-'+$Attempt;$readerStage='read-cold-scheduler-'+$Attempt
$producerSpec=PrepareNative $producerStage $producer[0].path @('tap','2560','20',$root) $inputs
 $producerParams=Join-Path $pkg ($producerStage+'-params.json')
 Capacity $producerStage 'before'
 NewJson $producerParams @{script=(Join-Path $repo 'research/windows_process_guard_v3/invoke.ps1');arguments=@{Specification=$producerSpec;GateReceipt=$gate}}
 CheckClosure $producerStage 'before'
Capacity ('scheduler-'+$Attempt) 'before'
$phase='elevation';$launch=$null;$ready=$null;$producerEvidence=$null;$auditEvidence=$null;$readerEvidence=$null;$reportPin=$null;$auditPin=$null;$readerPin=$null;$diagnosticIndex=$null;$selectedGap=$null
try {
 AssertSource
 $launch=Start-Process -FilePath $pwsh -ArgumentList @('-NoProfile','-NonInteractive','-File',('"'+$launcherPin.path+'"'),'-LaunchPlan',('"'+$launchPath+'"'),'-ExpectedPlanSha256',$launchPin.sha256) -Verb RunAs -WindowStyle Hidden -PassThru
 $phase='trace_start'
 $readyPath=Join-Path $artifact 'ready.json';$awaitDeadline=[Diagnostics.Stopwatch]::GetTimestamp()+90*[Diagnostics.Stopwatch]::Frequency
 while(-not(Test-Path -LiteralPath $readyPath) -and -not $launch.HasExited -and [Diagnostics.Stopwatch]::GetTimestamp() -lt $awaitDeadline){[Threading.Thread]::Sleep(25)}
 Require (Test-Path -LiteralPath $readyPath) 'trace did not become ready; original failure retained'
 $ready=ReadPin (Binding $readyPath) 65536
 Require ($ready.schema -ceq 'pas.cold-scheduler-trace-ready.v1' -and $ready.tool_source_commit -ceq $FrozenHead -and $ready.session_name -ceq ('PAS-ColdScheduler-'+$Attempt)) 'current trace ready identity';Bool $ready 'successful_start' $true
 $requestPin=Binding (Join-Path $artifact 'trace-request.json');$request=ReadPin $requestPin 65536
 Require ($ready.request_sha256 -ceq $requestPin.sha256 -and $ready.session_guid -ceq $request.session_guid -and $ready.start_qpc -eq $request.start_qpc -and $ready.deadline_qpc -eq $request.deadline_qpc -and $ready.qpc_frequency -eq $request.qpc_frequency -and $request.qpc_frequency -eq [Diagnostics.Stopwatch]::Frequency -and $request.execution_source_commit -ceq $FrozenHead) 'ready and actual request identity/time join'
 Bool $ready 'ttl_overrun' $false
 Require ($ready.observed_qpc -lt $ready.deadline_qpc -and [Diagnostics.Stopwatch]::GetTimestamp() -lt $ready.deadline_qpc) 'original shared deadline still active before producer'
 Require ($ready.instance.handle -gt 0 -and $ready.instance.clock -eq 1 -and $ready.instance.mode -eq 37748737 -and $ready.instance.enable_flags -eq 268437520 -and $ready.instance.maximum_file_mb -eq 128 -and $ready.instance.maximum_buffers*$ready.instance.buffer_kb*1024 -le 67108864) 'actual trace flags and normalized bounds'
 $phase='producer'
 try {& $pwsh -NoProfile -File (Join-Path $repo 'research/main_consolidation/stage.ps1') -Parameters $producerParams;Require ($LASTEXITCODE -eq 0) 'original owned producer wrapper exit';Receipt $producerStage;$producerEvidence=ProcessEvidence $producerStage}
 finally {if(-not(Test-Path -LiteralPath (Join-Path $artifact 'stop.json'))){AtomicJson (Join-Path $artifact 'stop.json') @{schema='pas.cold-scheduler-trace-stop.v1';request_sha256=$ready.request_sha256;session_guid=$ready.session_guid;handle=$ready.instance.handle}}}
 $phase='trace_stop'
 $waitUntil=[Diagnostics.Stopwatch]::GetTimestamp()+10*[Diagnostics.Stopwatch]::Frequency
 while((-not(Test-Path -LiteralPath (Join-Path $artifact 'controller-report.json')) -or -not(Test-Path -LiteralPath (Join-Path $artifact 'watchdog-report.json'))) -and [Diagnostics.Stopwatch]::GetTimestamp() -lt $waitUntil){[Threading.Thread]::Sleep(25)}
 $controllerReport=ReadPin (Binding (Join-Path $artifact 'controller-report.json')) 65536
 $watchdogReport=ReadPin (Binding (Join-Path $artifact 'watchdog-report.json')) 65536
 foreach($result in @($controllerReport,$watchdogReport)){Bool $result 'cleanup_confirmed' $true;Require ($result.request_sha256 -ceq $ready.request_sha256 -and $result.session_guid -ceq $ready.session_guid) 'same exact stopped session'}
 Require ($controllerReport.schema -ceq 'pas.cold-scheduler-trace-controller-report.v1' -and $watchdogReport.schema -ceq 'pas.cold-scheduler-trace-watchdog-report.v1') 'actual stopped report schemas'
 foreach($result in @($controllerReport,$watchdogReport)){Require ($result.tool_source_commit -ceq $FrozenHead -and $result.instance.handle -eq $ready.instance.handle -and $result.instance.guid -ceq $ready.instance.guid -and $result.instance.logger_thread_id -eq $ready.instance.logger_thread_id -and $result.instance.file -ceq $ready.instance.file -and $result.deadline_qpc -eq $ready.deadline_qpc -and $result.final_statistics_verified -is [bool]) 'stopped exact instance and typed final statistics'}
 $finalStats=@(@($controllerReport,$watchdogReport)|Where-Object {$_.final_statistics_verified})
 Require ($finalStats.Count -ge 1) 'no verified final flush statistics; UNKNOWN'
 foreach($result in $finalStats){Require ($result.instance.events_lost -eq 0 -and $result.instance.log_buffers_lost -eq 0 -and $result.instance.realtime_buffers_lost -eq 0) 'final trace loss preserved as negative'}
 Bool $controllerReport 'raw_file_within_cap' $true
 $launchResultPath=Join-Path $artifact 'elevated-launch-result.json'
 while(-not(Test-Path -LiteralPath $launchResultPath) -and [Diagnostics.Stopwatch]::GetTimestamp() -lt $waitUntil){[Threading.Thread]::Sleep(25)}
 $launchResult=ReadPin (Binding $launchResultPath) 65536
 Require ($launchResult.schema -ceq 'pas.cold-scheduler-elevated-launch-result.v1' -and $launchResult.execution_source_commit -ceq $FrozenHead -and $launchResult.controller_pid -eq $controllerReport.process_id -and $launchResult.watchdog_pid -eq $watchdogReport.process_id -and $launchResult.controller_exit -is [int] -and $launchResult.watchdog_exit -is [int] -and $launchResult.controller_exit -eq 0 -and $launchResult.watchdog_exit -eq 0) 'actual independently exited diagnostic processes'
 Bool $launchResult 'controller_exited' $true;Bool $launchResult 'watchdog_exited' $true
 Require ($controllerReport.reason -ceq 'requested_stop') 'original deadline/abnormal stop preserved as negative'
 CheckClosure $producerStage 'after';Capacity $producerStage 'after';ClosureReceipt $producerStage $true
 $reportPin=Binding (Join-Path $root 'report.json');$r=ReadPin $reportPin
 Require ($r.schema -ceq 'pas.windows-production-pipeline-cold.v1' -and $r.source_build_commit -ceq $b.base_source_commit -and $r.diagnostic_component_identity.source_commit -ceq $b.delta_source_commit -and $r.actual_manual_translation_unit_sha256 -ceq $b.base_manual_source.sha256 -and $r.requested -eq 2560 -and $r.cadence_ms -eq 20 -and $r.scene -ceq 'tap' -and $r.device_operations -eq 0) 'unchanged actual component workload and provenance'
 $native=ReadPin $producerEvidence.result;Require ($r.process_qos.process_id -eq $native.facts.pid) 'actual own producer PID'
 $phase='auditor';$auditPath=Join-Path $artifact 'tap-audit.json';$raw=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object FullName)
 AssertSource
 Native $auditorStage $auditor[0].path @('--root',$root,'--expected-report-sha256',$reportPin.sha256,'--source-commit',$b.base_source_commit,'--output',$auditPath) @($inputs+$raw+$producerEvidence.Values.path) 900
 $auditEvidence=ProcessEvidence $auditorStage;$auditPin=Binding $auditPath;$a=ReadPin $auditPin
 Require ($a.schema -ceq 'pas.windows-production-pipeline-audit.v5' -and $a.source_commit -ceq $b.base_source_commit -and $a.report.sha256 -ceq $reportPin.sha256) 'original full raw auditor';Bool $a 'integrity_verified' $true
 $diagnosticIndex=$r.producer_wait_diagnostic.index;$index=ReadPin $diagnosticIndex
 Require ($index.clock_domain -ceq 'host_qpc_ns' -and $index.host_clock_frequency_hz -eq 10000000 -and $index.thread_id -gt 0 -and $index.diagnostic_component_identity.source_commit -ceq $b.delta_source_commit -and $index.segments.Count -le 5) 'current producer TID and exact clock domain'
 $gaps=@();foreach($segment in $index.segments){$s=ReadPin $segment.file;foreach($row in $s.poll_gap_rows){Require ($row.Count -eq 9) 'gap columns';if($row[7] -is [bool] -and $row[7] -and $row[8] -is [bool] -and -not $row[8]){$gaps+=,@($row)}}}
 Require ($gaps.Count -gt 0) 'available diagnostic maximum'
 $selectedGap=@($gaps|Sort-Object @{Expression={$_[1]};Descending=$true},@{Expression={$_[0]};Descending=$false})[0]
 Require ($selectedGap[2]%100 -eq 0 -and $selectedGap[3]%100 -eq 0 -and $selectedGap[3]-$selectedGap[2] -eq $selectedGap[1]) 'exact HostClock QPC-ns inversion; no origin guess'
 $begin=[long]([decimal]$selectedGap[2]/[decimal]100);$end=[long]([decimal]$selectedGap[3]/[decimal]100)
 $selectionPath=Join-Path $artifact 'selected-gap.json'
 NewJson $selectionPath @{schema='pas.cold-scheduler-selected-gap.v1';rule='largest valid adjacent QPC gap, earliest attempt tie, all attempted slots included';component_build=$bindingPin;producer_process=$producerEvidence;report=$reportPin;diagnostic_index=$diagnosticIndex;attempt=$selectedGap[0];max_gap_ns=$selectedGap[1];gap_begin_ns=$selectedGap[2];gap_end_ns=$selectedGap[3];begin_qpc=$begin;end_qpc=$end;target_tid=$index.thread_id;qpc_frequency=10000000;exact_inversion=$true;qualification_issued=$false;live_ready=$false;device_operations=0}
 $phase='reader';$etl=Binding (Join-Path $artifact 'scheduler.etl');Require ($etl.bytes -gt 0 -and $etl.bytes -le 134217728) 'single ETL hard cap'
 $readerPath=Join-Path $artifact 'scheduler-summary.json'
 Native $readerStage $reader[0].path @('--etl',$etl.path,'--target-tid',[string]$index.thread_id,'--begin-qpc',[string]$begin,'--end-qpc',[string]$end,'--qpc-frequency','10000000','--output',$readerPath) @($inputs+$etl.path+$selectionPath+$diagnosticIndex.path+$reportPin.path+$auditPin.path+$producerEvidence.Values.path+$auditEvidence.Values.path+(Join-Path $artifact 'controller-report.json')+(Join-Path $artifact 'watchdog-report.json')) 120
 $readerEvidence=ProcessEvidence $readerStage;$readerPin=Binding $readerPath;$summary=ReadPin $readerPin 1048576
 AssertSource
 NewJson (Join-Path $artifact 'status.json') @{schema='pas.cold-scheduler-measurement-status.v1';status='DIAGNOSTIC_COMPLETE';execution_source_commit=$FrozenHead;tool_build=$toolPin;component_build=$bindingPin;plan=(Binding $planPath);trace_request=(Binding (Join-Path $artifact 'trace-request.json'));ETL=$etl;report=$reportPin;audit=$auditPin;diagnostic_index=$diagnosticIndex;selection=(Binding $selectionPath);scheduler_summary=$readerPin;producer_process=$producerEvidence;audit_process=$auditEvidence;reader_process=$readerEvidence;controller_report=(Binding (Join-Path $artifact 'controller-report.json'));watchdog_report=(Binding (Join-Path $artifact 'watchdog-report.json'));raw_integrity_verified=$true;original_numerical_cost_valid=$a.normal_cost_valid;original_numerical_failures=$a.normal_cost_failures;diagnostic_measurement_runs=1;normal3_runs=0;AA12_runs=0;noise18_runs=0;ABBA36_runs=0;qualification_issued=$false;whole_candidate_qualification=$false;cost_ready=$false;physical_qualification=$false;live_ready=$false;device_operations=0}
 Write-Output 'one cold scheduler diagnostic complete; original numerical results preserved; no qualification'
} catch {
 $reason=$_.Exception.Message
 if($ready -and -not(Test-Path -LiteralPath (Join-Path $artifact 'stop.json'))){try{AtomicJson (Join-Path $artifact 'stop.json') @{schema='pas.cold-scheduler-trace-stop.v1';request_sha256=$ready.request_sha256;session_guid=$ready.session_guid;handle=$ready.instance.handle}}catch{}}
 if(Test-Path -LiteralPath (Join-Path $out ($producerStage+'/'+$producerStage+'-verification.json'))){try{CheckClosure $producerStage 'after';Capacity $producerStage 'after';ClosureReceipt $producerStage $false $reason}catch{}}
 NewJson (Join-Path $artifact 'stop-status.json') @{schema='pas.cold-scheduler-measurement-stop.v1';execution_source_commit=$FrozenHead;phase=$phase;reason=$reason;tool_build=$toolPin;component_build=$bindingPin;plan=(Binding $planPath);producer_process=$producerEvidence;audit_process=$auditEvidence;reader_process=$readerEvidence;report=$reportPin;audit=$auditPin;scheduler_summary=$readerPin;diagnostic_index=$diagnosticIndex;selected_gap=$selectedGap;qualification_issued=$false;cost_ready=$false;live_ready=$false;device_operations=0}
 throw
}
