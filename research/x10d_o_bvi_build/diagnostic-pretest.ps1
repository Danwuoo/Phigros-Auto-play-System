. "$PSScriptRoot/common.ps1"
. "$Source/transaction.ps1"
. "$Source/diagnostic.ps1"
if((Json "$Evidence/state.json").status -cne 'INIT_PENDING'){throw 'diagnostic-pretest-once'}
$defs=@(@('D01','slot-valid','pass'),@('D02','slot-reused','diagnostic-slot'),@('D03','slot-five','diagnostic-slot'),@('D04','repair-three','diagnostic-revision'),@('D05','product-frozen','diagnostic-slot'),@('D06','wrong-exe','diagnostic-command-allowlist'),@('D07','wrong-argv','diagnostic-command-allowlist'),@('D08','old-path','diagnostic-command-allowlist'),@('D09','wrong-revision','diagnostic-revision'),@('D10','missing-controls','diagnostic-controls'),@('D11','positive','pass'),@('D12','nonzero','pass'),@('D13','missing-markers','pass'),@('D14','bad-cleanup','process-integrity'),@('D15','bad-runner','exit-predicate'),@('D16','no-positive','diagnostic-positive-required'),@('D17','pending-receipt','diagnostic-receipt-untrusted'),@('D18','wrong-positive-version','diagnostic-positive-version'),@('D19','full-rejection-transaction','pass'),@('D20','full-positive-transaction','pass'))
NewJson "$Evidence/diagnostic-oracle.json" @{cases=@($defs|ForEach-Object{@{id=$_[0];mode=$_[1];expected=$_[2]}});rounds=1;max_cases=20;actual_CreateProcess_allowed=$false;frozen_before_cases=$true}
$scratch="$Evidence/scratch-diagnostic";[IO.Directory]::CreateDirectory($scratch)|Out-Null
function FakeFacts($exitCode=0){[pscustomobject]@{launch='launched';created=$true;resumed=$true;root_exited=$true;exit_code=$exitCode;runner_exit=$exitCode;identity_trusted=$true;active_final=0;held_all_signaled=$true;streams_completed=$true;errors=@();errors_overflow=$false;stdout_overflow=$false;stderr_overflow=$false;elapsed_s=1.0;reason='';natural_quiescence=$true;cleanup='not_needed_verified_zero'}}
function State($stage='probe-1'){@{attempt=$Attempt;status='READY';next=$stage;revision=1;contract_sha='fake-contract';freeze_sha='fake-freeze';receipts=@();diagnostic_receipts=@();diagnostic_slots_consumed=0;wrapper_revision=0;wrapper_repairs=0;diagnostic_positive=$false;product_frozen=$false;native_stages_consumed=0;native_launches_known=0;running=$null;reason=$null}}
$rows=@()
foreach($c in (Json "$Evidence/diagnostic-oracle.json").cases){
 $s=State;$s.receipts=@(1,2,3);$spec=[pscustomobject]@{name='probe-1';exe='C:\Windows\System32\cmd.exe';argv=@('/d','/s','/c',"$Source\wrappers-v0\probe.cmd");wrapper_revision=0;native_exit=0;runner_exit=0;total_s=30;next='probe-2';report=$null}
 $f=FakeFacts;$text='TRACE DIAGNOSTIC_POSITIVE wrapper-v0 TRACE L07 vcvars END exit=0 TRACE L12 argv-configure END exit=0 TRACE L17 argv-build END exit=7';$error=$null;$tx=$null;$facts=@{}
 try{
  switch($c.mode){
   'slot-reused'{$s.diagnostic_slots_consumed=1};'slot-five'{$spec.name='probe-5'};'repair-three'{$s.wrapper_repairs=3};'product-frozen'{$s.product_frozen=$true};'wrong-exe'{$spec.exe='unapproved'};'wrong-argv'{$spec.argv[1]='/k'};'old-path'{$spec.argv[3]="$Repo\research\x10d_o_bvi_r2f_control\configure-release.cmd"};'wrong-revision'{$spec.wrapper_revision=1};'missing-controls'{$s.receipts=@(1,2)}
  }
  if($c.mode -in @('slot-valid','slot-reused','slot-five','repair-three','product-frozen','wrong-exe','wrong-argv','old-path','wrong-revision','missing-controls')){DiagnosticSlot $s $spec}
  elseif($c.mode -in @('positive','nonzero','missing-markers','bad-cleanup','bad-runner')){
   if($c.mode -eq 'nonzero'){$f=FakeFacts 1};if($c.mode -eq 'missing-markers'){$text=''};if($c.mode -eq 'bad-cleanup'){$f.held_all_signaled=$false};if($c.mode -eq 'bad-runner'){$f.runner_exit=125}
   $o=DiagnosticOutcome $f $spec $text;$expected=if($c.mode -eq 'positive'){'DIAGNOSTIC_POSITIVE'}else{'DIAGNOSTIC_REJECTED'};if($o -cne $expected){throw 'diagnostic-outcome'};$facts=@{classification=$o;native_exit=$f.exit_code;verification_pass=($o -eq 'DIAGNOSTIC_POSITIVE')}
  }elseif($c.mode -in @('no-positive','pending-receipt','wrong-positive-version')){
   $s.diagnostic_positive=$c.mode -ne 'no-positive';$p="$scratch/$($c.id).json";NewJson $p @{attempt=$Attempt;pending=($c.mode -eq 'pending-receipt');integrity_verified=$true;classification='DIAGNOSTIC_POSITIVE';verification_exit=0;wrapper_revision=1;entries=@()};$s.diagnostic_receipts=@(Entry $p);DiagnosticGate $s
  }else{
   $dir="$scratch/$($c.id)";[IO.Directory]::CreateDirectory($dir)|Out-Null;$s=State;$s.diagnostic_slots_consumed=1
   if($c.mode -eq 'full-rejection-transaction'){$f=FakeFacts 1;$text='TRACE L06 vcvars START'}
   NewJson "$dir/state.json" $s;$tx=NewTransaction $dir $spec $s;BeginTransaction $tx @{fake=$true;pending=$false};CheckpointTransaction $tx 'fake-exit' $f
   $b=[Text.Encoding]::UTF8.GetBytes($text);$tx.handles['probe-1.stdout.log'].Write($b)
   $v=FinishDiagnostic $tx $f @{out_new=0;fake=$true};CheckState (Json "$dir/state.json") $tx.state.next fake-contract fake-freeze
   if($s.native_stages_consumed -ne 1 -or $s.native_launches_known -ne 1 -or $s.diagnostic_receipts.Count -ne 1 -or $v.verification_exit -ne $f.exit_code){throw 'diagnostic-transaction'}
   if($c.mode -eq 'full-positive-transaction'){DiagnosticGate $s}else{if($s.diagnostic_positive){throw 'rejection-promoted'}}
   $facts=@{state=(Entry "$dir/state.json");verification=(Entry "$dir/probe-1-verification.json");classification=$v.classification;native_exit=$v.native_exit;shared_transaction=$true;fake=$true}
  }
 }catch{$error=$_.Exception.Message;if($tx){StopTransaction $tx $error}}finally{if($tx){CloseTransaction $tx}}
 $pass=if($c.expected -eq 'pass'){-not $error}else{$error -ceq $c.expected}
 $rows+=@{id=$c.id;mode=$c.mode;expected=$c.expected;error=$error;pass=[bool]$pass;facts=$facts}
}
$failed=@($rows|Where-Object pass -ne $true)
NewJson "$Evidence/diagnostic-pretest.json" @{cases=$rows;failed=$failed.Count;actual_CreateProcess=0;shared_source=@(Entry "$Source/diagnostic.ps1";Entry "$Source/common.ps1";Entry "$Source/transaction.ps1");fake_facts_not_native_verification=$true}
@{cases=$rows.Count;failed=$failed.Count;failures=$failed;actual_CreateProcess=0}|ConvertTo-Json -Depth 6 -Compress
if($failed.Count){exit 1}
