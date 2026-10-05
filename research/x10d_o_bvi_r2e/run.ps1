param([Parameter(Mandatory)][string]$Stage,[Parameter(Mandatory)][string]$AttemptId)
. "$PSScriptRoot/common.ps1"
$ExpectedContract='4588595d453555004b76c7aed98221d8f7eb63539a1d185d4c54f80405cdf7e2'
$lock=$null;$handles=@{};$result=$null;$state=$null;$binding=$null
function DurableState($v){$next="$Evidence/state-$($v.revision).pending";$h=NewHandle $next;try{PutHandle $h $v}finally{$h.Dispose()};[IO.File]::Move($next,"$Evidence/state.json",$true);$read=Json "$Evidence/state.json";if($read.status -cne $v.status -or $read.revision -ne $v.revision){throw 'state-readback'}}
function Reserve($leaf,[bool]$shared=$false){$p=Join-Path $binding.evidence $leaf;NoAlias $p;$share=if($shared){[IO.FileShare]::ReadWrite}else{[IO.FileShare]::Read};$h=[IO.FileStream]::new($p,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,$share);$handles[$leaf]=$h;PutHandle $h @{pending=$true;attempt=$Attempt;stage=$Stage};$h}
function VerifyResult($r,$s){
 if($r.launch -cne 'launched' -or $r.created -ne $true -or $r.resumed -ne $true -or $r.root_exited -ne $true -or $null -eq $r.exit_code -or $r.identity_trusted -ne $true -or $r.active_final -ne 0 -or $r.streams_completed -ne $true -or $r.errors.Count -or $r.errors_overflow -or $r.stdout_overflow -or $r.stderr_overflow -or $r.elapsed_s -gt $s.total_s){throw 'process-integrity'}
 ExpectedExit $r $s.native_exit $s.runner_exit
 if($Stage -eq 'owned-child'){
  if($r.reason -cne 'descendants-nonquiescent' -or $r.natural_quiescence -ne $false -or $r.quiescence_s -lt 15 -or $r.cleanup -cne 'cleanup_verified'){throw 'owned-child-rejection'}
  $child=Json "$Evidence/child-identity.json";$parent=Json "$Evidence/parent-identity.json"
  if($child.pid -notin $r.held_members_exited){throw 'child-handle-not-exited'}
  foreach($a in @($child,$parent)){if($a.attempt -cne $Attempt -or $a.pending -or $a.pid -ne $child.pid -or $a.creation_filetime -ne $child.creation_filetime -or $a.image -cne $child.image){throw 'child-receipt'}}
  foreach($name in @('root-exit','quiescence-end')){$sn=@($r.snapshots|Where-Object stage -eq $name);$member=@($sn[0].members|Where-Object pid -eq $child.pid);if($sn.Count -ne 1 -or -not $sn[0].complete -or $member.Count -ne 1 -or $member[0].creation_filetime -ne $child.creation_filetime -or $member[0].image -cne $child.image -or -not $member[0].membership_before -or -not $member[0].membership_after -or $member[0].exited){throw 'child-membership'}}
 }elseif($r.reason -or $r.natural_quiescence -ne $true -or $r.cleanup -cne 'not_needed_verified_zero'){throw 'natural-predicate'}
 if($s.report){$p=Join-Path $Evidence $s.report;$j=Json $p;if($j.pending){throw 'report-missing'}
  if($Stage -eq 'wrong-contact-release'){
   if($j.status -cne 'cold_fail_requires_classification' -or $j.failed_assertions -ne 2 -or -not $j.consumer_rejects -or $j.adopted -ne $false -or $j.negative_rows.Count -ne 2){throw 'negative-consumer'}
   foreach($row in $j.negative_rows){if($row.expected -ne 4 -or $row.actual -ne 1 -or $row.pass -ne $false){throw 'negative-row'}}
   if($j.negative_rows[0].field -cne 'contact_id_transfer@0' -or $j.negative_rows[1].field -cne 'renaming_output_contact'){throw 'negative-chain'}
  }elseif($Stage -like 'suite-*'){
   if($j.status -cne 'cold_contract_only' -or $j.failed_assertions -ne 0 -or $j.unverified_oracle_fields.Count -ne 0 -or $j.schema_errors.Count -ne 0 -or $j.supplemental.Count -ne 22 -or $j.schema_negative_controls.Count -ne 4){throw 'cold-suite'}
   foreach($layer in @('rgb','typed','lifecycle','e2e')){if($j.layers.$layer.cases_enumerated -ne 89 -or $j.layers.$layer.cases_with_assertions -ne 89 -or $j.layers.$layer.failed_assertions -ne 0){throw 'cold-layer'}}
  }elseif($Stage -eq 'png-current'){if($j.rows.Count -ne 52 -or $j.integrity_failures -ne 0 -or $j.core_counterexamples -ne 0){throw 'png-integrity'}}
 }
}
try{
 if($AttemptId -cne $Attempt){throw 'attempt-id'};$binding=Binding
 if((Sha "$Evidence/contract.json") -cne $ExpectedContract){throw 'contract-sha'}
 $contract=Json "$Evidence/contract.json";$stageSpec=@($contract.stages|Where-Object name -ceq $Stage);if($stageSpec.Count -ne 1){throw 'stage-allowlist'};$stageSpec=$stageSpec[0]
 $lock=[IO.FileStream]::new("$Evidence/attempt.lock",[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
 $state=Json "$Evidence/state.json";CheckState $state $Stage $ExpectedContract (Sha "$Evidence/freeze.json")
 $freeze=Json "$Evidence/freeze.json";foreach($e in @($freeze.files)+@($contract.inputs)+@($contract.dependencies)){CheckEntry $e}
 $capacity=Capacity $stageSpec.reserve_development $stageSpec.reserve_out
 # Recheck protected sources and original Git state immediately before each real stage.
 $protection=Protection
 Add-Type -Path "$Source/owned.cs"
 foreach($leaf in @('command','result','verification')){$null=Reserve "$Stage-$leaf.json"}
 for($i=0;$i -lt 8;$i++){$null=Reserve "$Stage-checkpoint-$i.json"}
 foreach($leaf in @('stdout','stderr')){$h=Reserve "$Stage.$leaf.log";$h.SetLength(0);$h.Flush($true)}
 if($stageSpec.report){$null=Reserve $stageSpec.report $true}
 if($Stage -eq 'owned-child'){$null=Reserve 'child-identity.json' $true;$null=Reserve 'parent-identity.json' $true}
 $command=@{attempt=$Attempt;stage=$Stage;binding=$binding;spec=$stageSpec;executable=(Entry $stageSpec.exe);capacity=$capacity;protection=$protection;pending=$false;launch='not_launched'};PutHandle $handles["$Stage-command.json"] $command
 $state.status='RUNNING';$state.running=$Stage;$state.revision++;$state.native_stages_consumed++;DurableState $state
 $result=[R2EOwned+Result]::new();$script:checkpointIndex=0
 $callback=[Action[string,R2EOwned+Result]]{param($phase,$facts)$idx=$script:checkpointIndex;if($idx -ge 8){throw 'checkpoint-cap'};PutHandle $handles["$Stage-checkpoint-$idx.json"] @{attempt=$Attempt;stage=$Stage;phase=$phase;facts=$facts;pending=$false} 16384;$script:checkpointIndex++}
 [R2EOwned]::Run($result,$stageSpec.exe,[string[]]$stageSpec.argv,$binding.repo,$handles["$Stage.stdout.log"],$handles["$Stage.stderr.log"],$stageSpec.total_s,$stageSpec.stream_cap,$callback)
 if($result.created -eq $true){$state.native_launches_known++}
 PutHandle $handles["$Stage-result.json"] @{attempt=$Attempt;stage=$Stage;facts=$result;pending=$false}
 VerifyResult $result $stageSpec
 $entries=@();foreach($p in @("$Stage-command.json","$Stage-result.json","$Stage.stdout.log","$Stage.stderr.log")){$entries+=Entry "$Evidence/$p"};if($stageSpec.report){$entries+=Entry "$Evidence/$($stageSpec.report)"}
 $verification=@{attempt=$Attempt;stage=$Stage;verification_exit=0;native_exit=$result.exit_code;runner_exit=$result.runner_exit;pending=$false;entries=$entries;capacity_after=(Capacity);adopted=$false}
 PutHandle $handles["$Stage-verification.json"] $verification
 $actual=Json "$Evidence/$Stage-verification.json";if($actual.verification_exit -ne 0){throw 'verification-readback'}
 $state.receipts+=Entry "$Evidence/$Stage-verification.json";$state.status='READY';$state.next=$stageSpec.next;$state.running=$null;$state.revision++;if(-not $state.next){$state.status='COMPLETE'};DurableState $state
 @{stage=$Stage;native_exit=$result.exit_code;runner_exit=$result.runner_exit;verification_exit=0;state=$state.status;next=$state.next;elapsed_s=$result.elapsed_s}|ConvertTo-Json -Compress
 exit 0
}catch{
 $reason=$_.Exception.Message
 if($handles.ContainsKey("$Stage-verification.json")){try{PutHandle $handles["$Stage-verification.json"] @{attempt=$Attempt;stage=$Stage;verification_exit=1;pending=$false;reason=$reason;facts=$result}}catch{}}
 if($state){try{$state.status='STOP';$state.reason=$reason;$state.revision++;DurableState $state}catch{}}
 [Console]::Error.WriteLine("R2E STOP: $reason");exit 1
}finally{foreach($h in $handles.Values){try{$h.Dispose()}catch{}};if($lock){$lock.Dispose()}}
