param([Parameter(Mandatory)][string]$ExpectedScriptSha256,
      [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$FrozenHead,
      [Parameter(Mandatory)][string]$ReaderDelta,
      [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedReaderDeltaSha256,
      [ValidatePattern('^[a-z0-9-]{1,40}$')][string]$Attempt='main64-r03')
$ErrorActionPreference='Stop'
$taskScript=$PSCommandPath
$taskWorkspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $taskWorkspace 'research/pre_emulator/common.ps1')
. (Join-Path $taskWorkspace 'research/cold_scheduler_diagnostic/capacity.ps1')
$script:maintenancePath=$taskScript;$Operation='pipeline'
$execution=$FrozenHead;$toolSource='6e3755dcabcb5f77280802657c0bbe6f9f103fb1'
function RRequire([bool]$ok,[string]$reason){if(-not $ok){throw ('scheduler offline: '+$reason)}}
function RRead($pin,[long]$cap=4194304){CheckEntry $pin;RRequire ($pin.bytes -le $cap) 'bounded input';Get-Content -LiteralPath $pin.path -Raw|ConvertFrom-Json}
function RBool($obj,[string]$key,[bool]$expected){RRequire ($obj.$key -is [bool] -and $obj.$key -eq $expected) ('typed '+$key)}
function RPin([string]$path,[string]$sha){$p=Binding $path;RRequire ($p.sha256 -ceq $sha) 'external exact pin';return $p}
$own=RPin $taskScript $ExpectedScriptSha256
RRequire ((& git -C $repo rev-parse HEAD) -ceq $execution -and (& git -C $repo branch --show-current) -ceq 'main' -and -not (& git -C $repo status --porcelain)) 'unchanged clean execution'
$toolPin=RPin (Join-Path $repo 'out/prelive-cold-scheduler-evaluation-main62-etw-tools-r01/build-binding.json') '0c330035f22d3d6313130990dbdfc52242630b26610795bd16b21af3cef7bb17'
$t=RRead $toolPin;RRequire ($t.source_commit -ceq $toolSource -and $t.translation_unit_count -eq 3 -and $t.actual_native_stage_count -eq 4) 'original tools62'
$bPin=$t.original_component57;$b=RRead $bPin;$base=RRead $b.base_production_build
RRequire ($b.delta_source_commit -ceq '5dce75d12c63ed51de5d996cedcd44e29d729841' -and $b.base_source_commit -ceq '12ec0550e882e3b06751d67fa67c43dda3e9312b') 'original component57/base55'
$deltaPin=RPin ([IO.Path]::GetFullPath($ReaderDelta)) $ExpectedReaderDeltaSha256; $delta=RRead $deltaPin;
RRequire ($delta.schema -ceq 'pas.cold-scheduler-reader-delta-build.v1' -and $delta.translation_unit_count -eq 1 -and $delta.actual_native_stage_count -eq 3 -and $delta.base_tool_build.sha256 -ceq $toolPin.sha256 -and $delta.base_tool_source_commit -ceq $toolSource -and $delta.reader_controls_count -eq 20 -and $delta.reader_schema_controls_count -eq 8) 'one reader delta and original base joins'
foreach($field in @('diagnostic_only')){RBool $delta $field $true}
foreach($field in @('controller_rebuilt','producer_rebuilt','formal_rebuilt','recording_started','qualification_issued','whole_candidate_qualification','cost_qualification','physical_qualification','live_ready')){RBool $delta $field $false}
RRequire ($delta.device_operations -eq 0 -and $delta.measurement_runs -eq 0) 'reader delta scope'
$controls=RRead $delta.reader_controls_report;RBool $controls 'pass' $true;RBool $controls.schema_controls 'pass' $true
RRequire ($controls.source_commit -ceq $delta.source_commit -and $controls.assertions -eq 20 -and $controls.schema_controls.assertions -eq 8) 'compiled reader controls'
foreach($proof in @($delta.configure_process,$delta.build_process,$delta.reader_controls_process)){foreach($pin in $proof.PSObject.Properties.Value){CheckEntry $pin};$verification=RRead $proof.verification;Receipt $verification.stage}
$reader=@($delta.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'cold_scheduler_trace_reader.exe'})
$auditor=@($base.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'production_pipeline_audit.exe'})
RRequire ($reader.Count -eq 1 -and $auditor.Count -eq 1) 'unique sealed offline tools'
foreach($pin in @($reader)+@($auditor)){CheckEntry $pin}
$script:seal=@{files=@($reader)+@($auditor)}
$original=Join-Path $repo 'out/prelive-cold-scheduler-main62-r03';$root=Join-Path $original 'tap'
$artifact=Join-Path $repo ('out/prelive-cold-scheduler-schema-'+$Attempt);Fresh $artifact;$null=[IO.Directory]::CreateDirectory($artifact)
$originalFiles=@(Get-ChildItem -LiteralPath $original -File -Recurse|ForEach-Object {Binding $_.FullName})
$requestPin=Binding (Join-Path $original 'trace-request.json');$request=RRead $requestPin 65536
$ready=RRead (Binding (Join-Path $original 'ready.json')) 65536
$controller=RRead (Binding (Join-Path $original 'controller-report.json')) 65536
$watch=RRead (Binding (Join-Path $original 'watchdog-report.json')) 65536
$launch=RRead (Binding (Join-Path $original 'elevated-launch-result.json')) 65536
RRequire ($request.execution_source_commit -ceq $toolSource -and $ready.request_sha256 -ceq $requestPin.sha256 -and $ready.session_guid -ceq $request.session_guid) 'request and original ready join'
foreach($record in @($controller,$watch)){
 RBool $record 'cleanup_confirmed' $true;RBool $record 'final_statistics_verified' $false
 RRequire ($record.request_sha256 -ceq $requestPin.sha256 -and $record.session_guid -ceq $request.session_guid -and $record.tool_source_commit -ceq $toolSource -and $record.instance.handle -eq $ready.instance.handle -and $record.instance.logger_thread_id -eq $ready.instance.logger_thread_id -and $record.instance.file -ceq $ready.instance.file) 'same original stopped session'
 RBool $record 'ttl_overrun' $false
}
RRequire ($controller.reason -ceq 'requested_stop' -and $controller.stop_status -eq 0 -and $launch.controller_pid -eq $controller.process_id -and $launch.watchdog_pid -eq $watch.process_id -and $launch.controller_exit -eq 2 -and $launch.watchdog_exit -eq 0) 'original abnormal exit retained'
RBool $launch 'controller_exited' $true;RBool $launch 'watchdog_exited' $true
$producerStage='producer-cold-scheduler-main62-r03';Receipt $producerStage;$producerEvidence=ProcessEvidence $producerStage
$closure=RRead $producerEvidence.input_closure;RBool $closure 'pass' $false;RBool $closure 'before_verified' $true;RBool $closure 'after_verified' $true
RRequire ($closure.status -ceq 'STOP') 'original maintenance STOP retained'
$native=RRead $producerEvidence.result
$reportPin=Binding (Join-Path $root 'report.json');$r=RRead $reportPin
RRequire ($r.source_build_commit -ceq $b.base_source_commit -and $r.diagnostic_component_identity.source_commit -ceq $b.delta_source_commit -and $r.actual_manual_translation_unit_sha256 -ceq $b.base_manual_source.sha256 -and $r.requested -eq 2560 -and $r.cadence_ms -eq 20 -and $r.scene -ceq 'tap' -and $r.device_operations -eq 0 -and $r.process_qos.process_id -eq $native.facts.pid) 'actual producer workload/source/PID'
$etl=Binding (Join-Path $original 'scheduler.etl');RRequire ($etl.bytes -gt 0 -and $etl.bytes -le 134217728) 'original ETL bound'
$inputs=@($deltaPin.path,$own.path,$toolPin.path,$bPin.path,$b.base_production_build.path,$b.base_manual_source.path)+@($originalFiles.path)+@($producerEvidence.Values.path)+(GateInputs)
$guard=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json;$inputs+=@($guard.guard_source.path,$guard.process_source.path,$guard.cached_identity_controls.path,$guard.identity_wait_controls.path)
$phase='audit';$auditPin=$null;$auditEvidence=$null;$readerPin=$null;$readerEvidence=$null;$selectionPin=$null
try {
 $auditPath=Join-Path $repo 'out/prelive-cold-scheduler-read-main62-r03/tap-audit.json'
 Receipt 'audit-scheduler-offline-main62-r03'
 $auditEvidence=ProcessEvidence 'audit-scheduler-offline-main62-r03';$auditPin=Binding $auditPath;$a=RRead $auditPin;RBool $a 'integrity_verified' $true
 RRequire ($a.source_commit -ceq $b.base_source_commit -and $a.report.sha256 -ceq $reportPin.sha256) 'original full raw audit'
 $indexPin=$r.producer_wait_diagnostic.index;$index=RRead $indexPin
 RRequire ($index.clock_domain -ceq 'host_qpc_ns' -and $index.host_clock_frequency_hz -eq 10000000 -and $index.thread_id -gt 0 -and $index.diagnostic_component_identity.source_commit -ceq $b.delta_source_commit) 'actual TID/clock'
 $gaps=@();foreach($segment in $index.segments){$s=RRead $segment.file;foreach($row in $s.poll_gap_rows){RRequire ($row.Count -eq 9) 'columns';if($row[7] -is [bool] -and $row[7] -and $row[8] -is [bool] -and -not $row[8]){$gaps+=,@($row)}}}
 RRequire ($gaps.Count -gt 0) 'gap evidence';$g=@($gaps|Sort-Object @{Expression={$_[1]};Descending=$true},@{Expression={$_[0]};Descending=$false})[0]
 RRequire ($g[2]%100 -eq 0 -and $g[3]%100 -eq 0 -and $g[3]-$g[2] -eq $g[1]) 'exact QPC ns inverse'
 $begin=[long]([decimal]$g[2]/100);$end=[long]([decimal]$g[3]/100)
 RRequire ($begin -ge $request.start_qpc -and $end -lt $controller.observed_qpc) 'selected gap inside original observed trace span'
 $selection=Join-Path $artifact 'selected-gap.json'
 NewJson $selection @{schema='pas.cold-scheduler-selected-gap.v1';rule='largest valid adjacent QPC gap, earliest attempt tie, all attempted slots included';component_build=$bPin;producer_process=$producerEvidence;report=$reportPin;diagnostic_index=$indexPin;attempt=$g[0];max_gap_ns=$g[1];gap_begin_ns=$g[2];gap_end_ns=$g[3];begin_qpc=$begin;end_qpc=$end;target_tid=$index.thread_id;qpc_frequency=10000000;exact_inversion=$true;qualification_issued=$false;live_ready=$false;device_operations=0}
 $selectionPin=Binding $selection;$phase='reader';$readerPath=Join-Path $artifact 'scheduler-summary.json'
 Native ('read-scheduler-schema-'+$Attempt) $reader[0].path @('--etl',$etl.path,'--target-tid',[string]$index.thread_id,'--begin-qpc',[string]$begin,'--end-qpc',[string]$end,'--qpc-frequency','10000000','--output',$readerPath) @($inputs+$selection+$auditPin.path+$auditEvidence.Values.path) 120 2
 $readerEvidence=ProcessEvidence ('read-scheduler-schema-'+$Attempt);$readerPin=Binding $readerPath;$summary=RRead $readerPin 1048576
 RRequire ($summary.source_commit -ceq $delta.source_commit -and $summary.status -ceq 'UNKNOWN' -and $summary.target_tid -eq $index.thread_id -and $summary.begin_qpc -eq $begin -and $summary.end_qpc -eq $end) 'actual new reader identity and unchanged target'
 RBool $summary 'process_trace_reached_eof' $true;RBool $summary 'file_loss_observations_complete' $true
 RRequire ($summary.process_trace_status -eq 0 -and $summary.schema_catalog.schema -ceq 'pas.cold-scheduler-trace-schema-catalog.v1' -and @($summary.schema_catalog.entries).Count -gt 0) 'actual complete metadata catalog'
 foreach($pin in $originalFiles){CheckEntry $pin};RRequire ((& git -C $repo rev-parse HEAD) -ceq $execution -and -not (& git -C $repo status --porcelain)) 'unchanged originals/source'
 NewJson (Join-Path $artifact 'status.json') @{schema='pas.cold-scheduler-offline-status.v1';status='SCHEMA_CATALOG_COMPLETE_SCHEDULER_UNKNOWN';maintenance_script=$own;execution_source_commit=$execution;reader_delta=$deltaPin;tool_build=$toolPin;component_build=$bPin;original_stop=(Binding (Join-Path $original 'stop-status.json'));original_controller_exit=2;original_controller_final_statistics_verified=$false;original_trace_final_loss_verification='UNKNOWN';original_producer_closure_status='STOP';original_producer_native_exit=0;ETL=$etl;report=$reportPin;audit=$auditPin;selection=$selectionPin;scheduler_summary=$readerPin;producer_process=$producerEvidence;audit_process=$auditEvidence;reader_process=$readerEvidence;raw_integrity_verified=$true;original_numerical_cost_valid=$a.normal_cost_valid;original_numerical_failures=$a.normal_cost_failures;new_recording_attempts=0;new_producer_runs=0;normal3_runs=0;AA12_runs=0;noise18_runs=0;ABBA36_runs=0;qualification_issued=$false;cost_ready=$false;physical_qualification=$false;live_ready=$false;device_operations=0}
 Write-Output 'offline schema catalog complete; scheduler UNKNOWN and original STOP retained; no new recording'
} catch {
 $readerPath=Join-Path $artifact 'scheduler-summary.json';if(Test-Path -LiteralPath $readerPath){$readerPin=Binding $readerPath}
 NewJson (Join-Path $artifact 'stop-status.json') @{schema='pas.cold-scheduler-offline-stop.v1';phase=$phase;reason=$_.Exception.Message;maintenance_script=$own;execution_source_commit=$execution;tool_build=$toolPin;ETL=$etl;report=$reportPin;audit=$auditPin;selection=$selectionPin;scheduler_summary=$readerPin;original_stop=(Binding (Join-Path $original 'stop-status.json'));original_controller_final_statistics_verified=$false;original_trace_final_loss_verification='UNKNOWN';new_recording_attempts=0;new_producer_runs=0;qualification_issued=$false;cost_ready=$false;live_ready=$false;device_operations=0}
 throw
}
