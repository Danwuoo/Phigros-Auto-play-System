param([ValidateRange(1,2)][int]$Round=1)
. "$PSScriptRoot/common.ps1"
. "$PSScriptRoot/transaction.ps1"
if((Json "$Evidence/state.json").status -cne 'INIT_PENDING'){throw 'pretest-before-freeze-only'}
$definitions=@(
 @('T01','natural-zero','pass'),@('T02','natural-log','pass'),@('T03','nonzero','pass'),@('T04','owned-child','pass'),
 @('T05','wrong-contact','pass'),@('T06','suite-release','pass'),@('T07','suite-debug','pass'),@('T08','suite-asan','pass'),
 @('T09','pending-report','report-missing'),@('T10','closed-result','*disposed*'),@('T11','mutate-after-hash','source-input-sha'),
 @('T12','missing-receipt','*Could not find*'),@('T13','pending-verification','receipt-unverified'),@('T14','mutate-log','referenced-receipt-sha'),
 @('T15','stop-state','state-blocked'),@('T16','running-state','state-blocked'),@('T17','ready-state-fault','injected-state'),
 @('T18','running-state-fault','injected-state'),@('T19','exit-save','injected-exit-save'),@('T20','bad-native','exit-predicate'),
 @('T21','pending-stream','process-integrity'),@('T22','wrong-contact-fake0','exit-predicate'),
 @('T23','ordinary-writer-guard','*being used by another process*'),@('T24','external-writer-guard','*being used by another process*'),
 @('T25','duplicate-stage','state-blocked'),@('T26','sealed-write','transaction-sealed'),
 @('T27','retained-arithmetic','pass'),@('T28','cap-equality','pass'),@('T29','cap-over1','PREFLIGHT_REJECTED-estimate'),
 @('T30','units','pass'),@('T31','outer-exit1','dependent-step-rejected'),@('T32','outer-isError','dependent-step-rejected')
)
if(-not (Test-Path "$Evidence/transaction-oracle.json")){
 NewJson "$Evidence/transaction-oracle.json" @{schema=1;cases=@($definitions|ForEach-Object{@{id=$_[0];mode=$_[1];expected=$_[2]}});round_limit=1;cases_per_round=32;native_allowed=$false;success_criteria='same transaction held-writer and closed SHA/JSON, durable READY, next-stage consumer; negative shared consumer exact error plus STOP/RUNNING/reservation blocks; no real identity claim'}
}
$oracle=Json "$Evidence/transaction-oracle.json"
$scratch=Join-Path $Evidence "scratch-transaction-round-$Round";[IO.Directory]::CreateDirectory($scratch)|Out-Null
if(Test-Path "$Evidence/transaction-round-$Round.json"){throw 'round-once'}
$rows=[Collections.Generic.List[object]]::new()
function Facts($native=0,$runner=0){[pscustomobject]@{launch='launched';created=$true;resumed=$true;root_exited=$true;exit_code=$native;runner_exit=$runner;identity_trusted=$true;active_final=0;held_all_signaled=$true;streams_completed=$true;errors=@();errors_overflow=$false;stdout_overflow=$false;stderr_overflow=$false;elapsed_s=1.0;reason='';natural_quiescence=$true;cleanup='not_needed_verified_zero';quiescence_s=0.0;snapshots=@();held_members_exited=@();identity_kind='FAKE-not-platform-validation'}}
function ExternalWrite($tx,$leaf,$v){
 $path=Join-Path $tx.dir $leaf;$pending=Json $path;if(-not $pending.pending -or $pending.attempt -cne $Attempt){throw 'fake-producer-reservation'}
 $h=[IO.FileStream]::new($path,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite)
 try{PutHandle $h $v}finally{$h.Dispose()}
}
foreach($case in $oracle.cases){
 $id=$case.id;$mode=$case.mode;$expected=$case.expected;$error=$null;$tx=$null;$held=@();$closed=@();$facts=$null;$details=@{};$script:mutations=0;$script:stateWrites=0;$script:verificationWrites=0;$script:currentTx=$null
 try{
  if($mode -eq 'retained-arithmetic'){CapacityMath 156209 0 (40249475-156209) 0 99999999999;$details=@{retained=156209;pending=40093266;sum=40249475}}
  elseif($mode -eq 'cap-equality'){CapacityMath 0 0 40249475 0 99999999999;$details=@{sum=40249475;cap=40249475}}
  elseif($mode -eq 'cap-over1'){CapacityMath 1 0 40249475 0 99999999999}
  elseif($mode -eq 'units'){if(40*1MB -ne 41943040 -or 40*1000000 -eq 41943040 -or 41943040-1693565 -ne 40249475){throw 'unit-oracle'};CapacityMath 0 0 (40*1000000) 0 99999999999;$details=@{MiB=41943040;decimal_MB=40000000;remaining=40249475}}
  elseif($mode -like 'outer-*'){
   $result=if($mode -eq 'outer-exit1'){@{exit_code=1;isError=$false}}else{@{exit_code=0;isError=$true}}
   DependentStep $result {$script:mutations++};throw 'outer-did-not-stop'
  }else{
   $dir=Join-Path $scratch $id;[IO.Directory]::CreateDirectory($dir)|Out-Null
   $stage='natural';$native=0;$runner=0;$report=$null
   if($mode -eq 'nonzero'){$stage='nonzero';$native=$runner=7}
   elseif($mode -eq 'owned-child'){$stage='owned-child';$runner=125}
   elseif($mode -in @('wrong-contact','wrong-contact-fake0')){$stage='wrong-contact-release';$native=$runner=1;$report='report.json'}
   elseif($mode -like 'suite-*' -or $mode -in @('pending-report','external-writer-guard')){$stage=if($mode -like 'suite-*'){$mode}else{'suite-release'};$report='report.json'}
   $spec=[pscustomobject]@{name=$stage;native_exit=$native;runner_exit=$runner;report=$report;total_s=30;next='next-stage'}
   $state=@{attempt=$Attempt;status='READY';next=$stage;revision=1;native_stages_consumed=0;native_launches_known=0;contract_sha='mock-contract';freeze_sha='mock-freeze';receipts=@();running=$null;reason=$null}
   NewJson "$dir/state.json" $state
   $fault={param($phase)
    if($phase -eq 'state-before-write'){$script:stateWrites++;if(($mode -eq 'ready-state-fault' -and $script:stateWrites -ge 2) -or $mode -eq 'running-state-fault'){throw 'injected-state'}}
    if($mode -eq 'exit-save' -and $phase -eq 'write:natural-result.json'){throw 'injected-exit-save'}
    if($mode -eq 'mutate-after-hash' -and $phase -eq 'write:natural-verification.json'){$script:verificationWrites++;if($script:verificationWrites -eq 2){$h=$script:currentTx.handles['natural-command.json'];$h.Position=$h.Length;$h.WriteByte(32);$h.Flush($true)}}
   }
   $tx=NewTransaction $dir $spec $state $fault;$script:currentTx=$tx
   CheckLaunchState $dir (Json "$dir/state.json") $stage mock-contract mock-freeze
   BeginTransaction $tx @{attempt=$Attempt;stage=$stage;pending=$false;launch='fake-no-native-authority';fake=$true}
   $facts=Facts $native $runner;$tx.facts=$facts
   CheckpointTransaction $tx 'fake-native-exited' $facts
   if($mode -eq 'natural-log'){
    foreach($stream in @('stdout','stderr')){$b=[Text.Encoding]::UTF8.GetBytes("real file log $stream`n");$tx.handles["$stage.$stream.log"].Write($b);$tx.handles["$stage.$stream.log"].Flush($true)}
   }
   if($stage -eq 'owned-child'){
    $identity=@{schema='r2f.identity.v1';status='published';attempt=$Attempt;pending=$false;pid=1234;creation_filetime=555;image='FAKE-image';publish_qpc=[Diagnostics.Stopwatch]::GetTimestamp();qpc_frequency=[Diagnostics.Stopwatch]::Frequency;fake=$true}
    ExternalWrite $tx 'child-identity.json' $identity;ExternalWrite $tx 'parent-identity.json' $identity
    ExternalWrite $tx 'parent-poll.json' @{schema=1;attempt=$Attempt;accepted=$true;last='ready';end_qpc=1;deadline_qpc=2;polls=1;fake=$true}
    $member=@{pid=1234;creation_filetime=555;image='FAKE-image';membership_before=$true;membership_after=$true;exited=$false}
    $facts.reason='descendants-nonquiescent';$facts.natural_quiescence=$false;$facts.quiescence_s=15.0;$facts.cleanup='cleanup_verified';$facts.held_members_exited=@(1234);$facts.snapshots=@(@{stage='root-exit';complete=$true;members=@($member)},@{stage='quiescence-end';complete=$true;members=@($member)})
   }
   if($report -and $mode -ne 'pending-report'){
    $j=@{pending=$false;status='cold_contract_only';failed_assertions=0;unverified_oracle_fields=@();schema_errors=@();supplemental=@(1..22);schema_negative_controls=@(1..4);layers=@{};fake=$true}
    foreach($layer in @('rgb','typed','lifecycle','e2e')){$j.layers[$layer]=@{cases_enumerated=89;cases_with_assertions=89;failed_assertions=0}}
    if($stage -eq 'wrong-contact-release'){$j=@{status='cold_fail_requires_classification';failed_assertions=2;consumer_rejects=$true;adopted=$false;negative_rows=@(@{field='contact_id_transfer@0';expected=4;actual=1;pass=$false},@{field='renaming_output_contact';expected=4;actual=1;pass=$false});fake=$true}}
    ExternalWrite $tx $report $j
    # Check external producer's final bytes with compatible real readers while shared holder remains open.
    $e=Entry "$dir/$report";$read=Json $e.path;if($read.pending -or -not $e.sha256){throw 'external-held-readback'}
   }
   if($mode -eq 'ordinary-writer-guard'){$h=[IO.FileStream]::new("$dir/$stage-command.json",[IO.FileMode]::Open,[IO.FileAccess]::Write,[IO.FileShare]::ReadWrite);$h.Dispose();throw 'bypass-writer-allowed'}
   if($mode -eq 'closed-result'){$tx.handles["$stage-result.json"].Dispose()}
   if($mode -eq 'bad-native'){$facts.exit_code=$facts.runner_exit=7}
   if($mode -eq 'pending-stream'){$facts.streams_completed=$false}
   if($mode -eq 'wrong-contact-fake0'){$facts.exit_code=$facts.runner_exit=0}
   $v=FinishTransaction $tx $facts @{fake=$true;reserved_bytes=65536}
   $held=@($tx.handles.Keys|Sort-Object|ForEach-Object{Entry (Join-Path $dir $_)})
   foreach($e in $held){CheckEntry $e};$resultJson=Json "$dir/$stage-result.json";if(-not $resultJson.facts.root_exited -or $v.verification_exit -ne 0){throw 'complete-transaction-oracle'}
   if($mode -eq 'missing-receipt'){$s=Json "$dir/state.json";$s.receipts=@(@{path="$dir/missing.json";bytes=1;sha256='missing'});CheckState $s next-stage mock-contract mock-freeze}
   elseif($mode -eq 'pending-verification'){$leaf="$stage-verification.json";PutHandle $tx.handles[$leaf] @{attempt=$Attempt;pending=$true;verification_exit=0;entries=@()};$s=Json "$dir/state.json";$s.receipts=@(Entry "$dir/$leaf");CheckState $s next-stage mock-contract mock-freeze}
   elseif($mode -eq 'mutate-log'){$h=$tx.handles["$stage.stdout.log"];$h.WriteByte(32);$h.Flush($true);CheckState (Json "$dir/state.json") next-stage mock-contract mock-freeze}
   elseif($mode -in @('stop-state','running-state')){$s=Json "$dir/state.json";$s.status=if($mode -eq 'stop-state'){'STOP'}else{'RUNNING'};$s.revision++;DurableState $dir $s;CheckState (Json "$dir/state.json") next-stage mock-contract mock-freeze}
   elseif($mode -eq 'external-writer-guard'){$h=[IO.FileStream]::new("$dir/report.json",[IO.FileMode]::Open,[IO.FileAccess]::Write,[IO.FileShare]::ReadWrite);$h.Dispose();throw 'external-guard-failed'}
   elseif($mode -eq 'duplicate-stage'){CheckState (Json "$dir/state.json") $stage mock-contract mock-freeze}
   elseif($mode -eq 'sealed-write'){WriteTransaction $tx "$stage-result.json" @{incorrect='later write'}}
   $details=@{held_references=$held.Count;ready_next=(Json "$dir/state.json").next;shared_transaction=$true;fake_native=$true}
  }
 }catch{
  $error=$_.Exception.Message
  if($tx){StopTransaction $tx $error
   $disk=Json (Join-Path $tx.dir 'state.json');$blocked=$null
   try{CheckLaunchState $tx.dir $disk $disk.next mock-contract mock-freeze}catch{$blocked=$_.Exception.Message}
   $details=@{durable_status=$disk.status;next_launch_rejection=$blocked;shared_facts_root_exited=if($facts){$facts.root_exited}else{$null};shared_native_exit=if($facts){$facts.exit_code}else{$null};state_writes=$script:stateWrites;fake_native=$true}
   if(-not $blocked){$error+=';FAIL-next-launch-not-blocked'}
  }
 }finally{
  if($tx){CloseTransaction $tx}
 }
 if($held.Count -and -not $error){foreach($e in $held){CheckEntry $e;if($e.path -like '*.json'){$null=Json $e.path};$closed+=Entry $e.path};$details.closed_references=$closed.Count;$details.held_closed_SHA_equal=$true}
 $pass=if($expected -eq 'pass'){$null -eq $error}else{$error -like $expected -and $error -notlike '*FAIL-*'}
 if($mode -like 'outer-*' -and $script:mutations -ne 0){$pass=$false}
 $rows.Add(@{id=$id;mode=$mode;expected=$expected;observed_error=$error;pass=[bool]$pass;facts=$details;outer_mutations=$script:mutations})
}
$failed=@($rows|Where-Object pass -ne $true)
NewJson "$Evidence/transaction-round-$Round.json" @{schema=1;round=$Round;oracle=(Entry "$Evidence/transaction-oracle.json");shared_source=@(Entry "$Source/common.ps1";Entry "$Source/transaction.ps1");cases=$rows;failed=$failed.Count;actual_CreateProcess=0;native_interop_loaded=$false;real_file_IO=$true;scratch_namespace=$scratch;platform_identity_cleanup_verified=$false}
@{round=$Round;cases=$rows.Count;failed=$failed.Count;failures=$failed;actual_CreateProcess=0}|ConvertTo-Json -Depth 7 -Compress
if($failed.Count){exit 1}
