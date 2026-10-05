param([ValidateRange(1,2)][int]$Round=1)
. "$PSScriptRoot/common.ps1"
. "$PSScriptRoot/identity.ps1"
if((Json "$Evidence/state.json").status -cne 'INIT_PENDING'){throw 'pretest-before-freeze-only'}
$defs=@(
 @('I01','empty','empty'),@('I02','null','null'),@('I03','half','parse-incomplete'),@('I04','pending','pending'),
 @('I05','schema','incomplete-schema'),@('I06','status','incomplete-schema'),@('I07','missing-pid','incomplete-schema'),
 @('I08','attempt','identity-mismatch'),@('I09','pid','identity-mismatch'),@('I10','creation','identity-mismatch'),@('I11','image','identity-mismatch'),
 @('I12','stale','stale-publication'),@('I13','future','stale-publication'),@('I14','unpublished-deadline','timeout'),
 @('I15','missing','read-incomplete'),@('I16','over-cap','over-cap'),@('I17','valid','ready'),
 @('I18','empty-valid','empty,ready'),@('I19','null-valid','null,ready'),@('I20','half-valid','parse-incomplete,ready'),
 @('I21','pending-valid','pending,ready'),@('I22','incomplete-valid','incomplete-schema,ready'),
 @('W01','active-zero-delayed','signal'),@('W02','immediate','signal'),@('W03','near-deadline','signal'),
 @('W04','timeout','held-WAIT_TIMEOUT'),@('W05','failed','held-WAIT_FAILED'),@('W06','unknown','held-wait-unknown'),
 @('W07','expired','held-WAIT_TIMEOUT'),@('W08','late','held-after-deadline'),@('W09','two-shared-pass','signal'),
 @('W10','two-shared-timeout','held-WAIT_TIMEOUT'),@('W11','many-shared-timeout','held-WAIT_TIMEOUT'),@('W12','remaining-budget','signal'),
 @('B01','root-control','7'),@('B02','root-product','277'),@('B03','cleanup-full','25'),@('B04','cleanup-remaining','27'),@('B05','invalid-total','budget-total')
)
if(Test-Path "$Evidence/control-round-$Round.json"){throw 'round-once'}
if(-not (Test-Path "$Evidence/control-oracle.json")){NewJson "$Evidence/control-oracle.json" @{schema=1;round_limit=2;cases_per_round=39;cases=@($defs|ForEach-Object{@{id=$_[0];mode=$_[1];expected=$_[2]}});actual_CreateProcess_allowed=$false;criteria='same InspectIdentity/PublishIdentity and R2FOwned.AwaitHeld/RootDeadline/CleanupDeadline; fake I/O phases, QPC and waits are not platform evidence; shared five-second deadline, no deadline per handle'}}
$oracle=Json "$Evidence/control-oracle.json"
Add-Type -Path "$Source/owned.cs"
$scratch=Join-Path $Evidence "scratch-control-round-$Round";[IO.Directory]::CreateDirectory($scratch)|Out-Null
$rows=[Collections.Generic.List[object]]::new()
function Raw($path,$s){$h=[IO.FileStream]::new($path,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite);try{$h.SetLength(0);$b=[Text.Encoding]::UTF8.GetBytes($s);$h.Write($b);$h.Flush($true)}finally{$h.Dispose()}}
function Value($path,$v){$h=[IO.FileStream]::new($path,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite);try{PutHandle $h $v}finally{$h.Dispose()}}
foreach($case in $oracle.cases){
 $mode=$case.mode;$observed=$null;$error=$null;$facts=@{};$pass=$false
 try{
  if($case.id.StartsWith('I')){
   $path=Join-Path $scratch "$($case.id).json";NewJson $path @{attempt=$Attempt;pending=$true}
   $expected=@{attempt=$Attempt;pid=1234;creation_filetime=555;image='FAKE-image'}
   $v=$expected.Clone();$v.schema='r2f.identity.v1';$v.status='published';$v.pending=$false;$v.publish_qpc=[Diagnostics.Stopwatch]::GetTimestamp();$v.qpc_frequency=[Diagnostics.Stopwatch]::Frequency
   $phase=$mode.Replace('-valid','');$now=$v.publish_qpc;$deadline=$now+3*[Diagnostics.Stopwatch]::Frequency
   switch($phase){
    'empty'{Raw $path ''};'null'{Raw $path 'null'};'half'{Raw $path '{"schema":'};'pending'{Value $path @{attempt=$Attempt;pending=$true}}
    'schema'{$v.schema='wrong';Value $path $v};'status'{$v.status='';Value $path $v};'missing-pid'{$v.Remove('pid');Value $path $v}
    'attempt'{$v.attempt='old';Value $path $v};'pid'{$v.pid=999;Value $path $v};'creation'{$v.creation_filetime=999;Value $path $v};'image'{$v.image='wrong';Value $path $v}
    'stale'{$v.publish_qpc=$now-4*[Diagnostics.Stopwatch]::Frequency;Value $path $v};'future'{$v.publish_qpc=$now+1;Value $path $v}
    'unpublished-deadline'{$now=$deadline};'missing'{$path=Join-Path $scratch 'absent.json'};'over-cap'{Raw $path ('x'*4097)}
    'incomplete'{$v.Remove('creation_filetime');Value $path $v};'valid'{Value $path $v}
    default{throw 'case-mode'}
   }
   $r=InspectIdentity $path $expected $now $deadline;$observed=$r.state;$facts.first_ready=$r.ready
   if($mode.EndsWith('-valid')){
    if($r.ready){throw 'partial-ready'}
    Value $path @{attempt=$Attempt;pending=$true};PublishIdentity $path ($expected.Clone())
    $finalNow=[Diagnostics.Stopwatch]::GetTimestamp();$r=InspectIdentity $path $expected $finalNow ($finalNow+3*[Diagnostics.Stopwatch]::Frequency)
    $observed+=','+$r.state;$facts.published=(Entry $path);$facts.final_ready=$r.ready
   }
   $pass=$observed -ceq $case.expected
   if($case.expected -eq 'ready' -and -not $r.ready){$pass=$false}
  }elseif($case.id.StartsWith('W')){
   $script:fakeNow=0.0;$script:signalAt=2.0;$script:waitCode=[uint32]258;$script:late=$false;$deadline=5.0;$ids=@(1)
   switch($mode){
    'active-zero-delayed'{$facts.job_active=0};'immediate'{$script:signalAt=0.0};'near-deadline'{$script:signalAt=4.95}
    'timeout'{$script:signalAt=6.0};'failed'{$script:waitCode=[uint32]4294967295};'unknown'{$script:waitCode=[uint32]128}
    'expired'{$deadline=0.0};'late'{$script:late=$true};'two-shared-pass'{$ids=@(1,2);$script:signalAt=2.0}
    'two-shared-timeout'{$ids=@(1,2);$script:signalAt=3.0};'many-shared-timeout'{$ids=@(1,2,3);$script:signalAt=2.0}
    'remaining-budget'{$script:fakeNow=26.0;$deadline=[R2FOwned]::CleanupDeadline($script:fakeNow,30);$script:signalAt=26.9}
   }
   $records=@();$caught=$null
   foreach($pidValue in $ids){
    if($pidValue -gt 1){$script:signalAt=$script:fakeNow+$(if($mode -eq 'two-shared-timeout'){3.0}else{2.0})}
    $record=[R2FOwned+HeldWait]::new();$records+= $record
    try{[R2FOwned]::AwaitHeld([uint32]$pidValue,$deadline,[Func[double]]{return $script:fakeNow},[Func[uint32,uint32]]{param($ms)
      if($script:late){$script:fakeNow+=6.0;return [uint32]0}
      if($script:waitCode -ne 258){return $script:waitCode}
      if($script:fakeNow -ge $script:signalAt){return [uint32]0}
      $script:fakeNow+=[double]$ms/1000.0
      if($script:fakeNow -ge $script:signalAt){return [uint32]0};return [uint32]258
     },[Func[int]]{return 87},$record)}catch{$caught=$_.Exception.Message;break}
   }
   $observed=if($caught){$caught}else{'signal'};$facts.records=$records;$facts.end_s=$script:fakeNow;$facts.shared_deadline_s=$deadline
   $pass=if($case.expected -eq 'signal'){$observed -ceq 'signal' -and @($records|Where-Object signaled -ne $true).Count -eq 0}else{$observed.Contains($case.expected)}
   if($mode -eq 'failed' -and $record.last_error -ne 87){$pass=$false}
   if($mode -in @('two-shared-timeout','many-shared-timeout') -and ($script:fakeNow -gt 5.001 -or @($records|Where-Object deadline_s -ne 5).Count)){ $pass=$false }
  }else{
   $observed=switch($mode){'root-control'{[R2FOwned]::RootDeadline(30)};'root-product'{[R2FOwned]::RootDeadline(300)};'cleanup-full'{[R2FOwned]::CleanupDeadline(20,30)};'cleanup-remaining'{[R2FOwned]::CleanupDeadline(26,30)};'invalid-total'{try{[R2FOwned]::RootDeadline(29)}catch{$_.Exception.Message}}}
   $pass=if($mode -eq 'invalid-total'){[string]$observed -like '*budget-total*'}else{[string]$observed -ceq $case.expected}
  }
 }catch{$error=$_.Exception.Message}
 $rows.Add(@{id=$case.id;mode=$mode;expected=$case.expected;observed=$observed;error=$error;pass=([bool]$pass -and -not $error);facts=$facts})
}
$failed=@($rows|Where-Object pass -ne $true)
NewJson "$Evidence/control-round-$Round.json" @{schema=1;round=$Round;oracle=(Entry "$Evidence/control-oracle.json");cases=$rows;failed=$failed.Count;shared_source=@(Entry "$Source/common.ps1";Entry "$Source/transaction.ps1";Entry "$Source/identity.ps1";Entry "$Source/owned.cs");actual_CreateProcess=0;fake_identity_not_platform_evidence=$true;fake_wait_no_native_API=$true}
@{round=$Round;cases=$rows.Count;failed=$failed.Count;failures=$failed;actual_CreateProcess=0}|ConvertTo-Json -Depth 6 -Compress
if($failed.Count){exit 1}
