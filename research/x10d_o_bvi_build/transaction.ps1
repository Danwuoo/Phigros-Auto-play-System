# File-only shared transaction. No process creation or native interop in this module.
function DurableState($dir,$v,[scriptblock]$fault={param($phase)}){
 & $fault 'state-before-write'
 $next=Join-Path $dir "state-$($v.revision).pending";$h=NewHandle $next
 try{PutHandle $h $v}finally{$h.Dispose()}
 & $fault 'state-before-move'
 [IO.File]::Move($next,(Join-Path $dir 'state.json'),$true)
 $got=Json (Join-Path $dir 'state.json')
 if($got.status -cne $v.status -or $got.revision -ne $v.revision){throw 'state-readback'}
}
function NewTransaction($dir,$spec,$state,[scriptblock]$fault={param($phase)}){
 $allowed=$dir -eq $Evidence -or $dir.StartsWith((Join-Path $Evidence 'scratch-') ,[StringComparison]::OrdinalIgnoreCase)
 if(-not $allowed){throw 'transaction-root'};NoAlias $dir
 @{dir=$dir;spec=$spec;stage=$spec.name;state=$state;handles=@{};externals=@();sealed=$false;facts=$null;fault=$fault;begun=$false;checkpoint=0}
}
function CheckLaunchState($dir,$state,$stage,$contractSha,$freezeSha){
 CheckState $state $stage $contractSha $freezeSha
 # If RUNNING persistence failed, preopened output reservations still prevent reuse of READY.
 if(Test-Path -LiteralPath (Join-Path $dir "$stage-command.json")){throw 'consumed-output-reservation'}
}
function WriteTransaction($tx,$leaf,$value,$cap=65536){if($tx.sealed){throw 'transaction-sealed'};& $tx.fault "write:$leaf";if(-not $tx.handles[$leaf].CanWrite){throw 'disposed-writer'};PutHandle $tx.handles[$leaf] $value $cap}
function ReserveTransaction($tx,$leaf,[bool]$external=$false){
 $path=Join-Path $tx.dir $leaf;NoAlias $path
 $share=if($external){[IO.FileShare]::ReadWrite}else{[IO.FileShare]::Read}
 $h=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,$share)
 $tx.handles[$leaf]=$h;if($external){$tx.externals+= $leaf}
 WriteTransaction $tx $leaf @{pending=$true;attempt=$Attempt;stage=$tx.stage}
}
function BeginTransaction($tx,$command){
 $stage=$tx.stage
 foreach($leaf in @('command','result','verification')){ReserveTransaction $tx "$stage-$leaf.json"}
 for($i=0;$i -lt 8;$i++){ReserveTransaction $tx "$stage-checkpoint-$i.json"}
 foreach($leaf in @('stdout','stderr')){ReserveTransaction $tx "$stage.$leaf.log";$tx.handles["$stage.$leaf.log"].SetLength(0);$tx.handles["$stage.$leaf.log"].Flush($true)}
 if($tx.spec.report){ReserveTransaction $tx $tx.spec.report $true}
 if($stage -eq 'owned-child'){foreach($leaf in @('child-identity.json','parent-identity.json','parent-poll.json')){ReserveTransaction $tx $leaf $true}}
 WriteTransaction $tx "$stage-command.json" $command
 $tx.state.status='RUNNING';$tx.state.running=$stage;$tx.state.revision++;$tx.state.native_stages_consumed++
 DurableState $tx.dir $tx.state $tx.fault
 $tx.begun=$true
}
function CheckpointTransaction($tx,$phase,$facts){
 if($tx.checkpoint -ge 8){throw 'checkpoint-cap'}
 WriteTransaction $tx "$($tx.stage)-checkpoint-$($tx.checkpoint).json" @{attempt=$Attempt;stage=$tx.stage;phase=$phase;facts=$facts;pending=$false} 16384
 $tx.checkpoint++
}
function VerifyProcess($r,$spec){
 if($r.launch -cne 'launched' -or $r.created -ne $true -or $r.resumed -ne $true -or $r.root_exited -ne $true -or $null -eq $r.exit_code -or $r.identity_trusted -ne $true -or $r.active_final -ne 0 -or $r.held_all_signaled -ne $true -or $r.streams_completed -ne $true -or $r.errors.Count -or $r.errors_overflow -or $r.stdout_overflow -or $r.stderr_overflow -or $r.elapsed_s -gt $spec.total_s){throw 'process-integrity'}
 ExpectedExit $r $spec.native_exit $spec.runner_exit
 if($spec.name -eq 'owned-child'){
  if($r.reason -cne 'descendants-nonquiescent' -or $r.natural_quiescence -ne $false -or $r.quiescence_s -lt 15 -or $r.cleanup -cne 'cleanup_verified'){throw 'owned-child-rejection'}
 }elseif($r.reason -or $r.natural_quiescence -ne $true -or $r.cleanup -cne 'not_needed_verified_zero'){throw 'natural-predicate'}
}
function GuardExternalOutputs($tx){
 # Called only after process/root/job/pumps all verified finished. External producer is gone.
 # Exchange shared reservations for read-only guards that deny any additional writer.
 foreach($leaf in $tx.externals){$tx.handles[$leaf].Dispose();$tx.handles.Remove($leaf);$tx.handles[$leaf]=[IO.FileStream]::new((Join-Path $tx.dir $leaf),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)}
}
function VerifyConsumers($tx,$r){
 if($tx.stage -eq 'owned-child'){
  $child=Json (Join-Path $tx.dir 'child-identity.json');$parent=Json (Join-Path $tx.dir 'parent-identity.json')
  if($child.pid -notin $r.held_members_exited){throw 'child-handle-not-exited'}
  foreach($a in @($child,$parent)){if($a.schema -cne 'r2f.identity.v1' -or $a.status -cne 'published' -or $a.attempt -cne $Attempt -or $a.pending -ne $false -or -not $a.publish_qpc -or $a.qpc_frequency -ne [Diagnostics.Stopwatch]::Frequency -or $a.pid -ne $child.pid -or $a.creation_filetime -ne $child.creation_filetime -or $a.image -cne $child.image){throw 'child-receipt'}}
  $poll=Json (Join-Path $tx.dir 'parent-poll.json')
  if($poll.schema -ne 1 -or $poll.attempt -cne $Attempt -or $poll.accepted -ne $true -or $poll.last -cne 'ready' -or $poll.end_qpc -gt $poll.deadline_qpc -or $poll.polls -lt 1){throw 'identity-poll-receipt'}
  foreach($name in @('root-exit','quiescence-end')){
   $sn=@($r.snapshots|Where-Object stage -eq $name);if($sn.Count -ne 1){throw 'child-snapshot'}
   $member=@($sn[0].members|Where-Object pid -eq $child.pid)
   if(-not $sn[0].complete -or $member.Count -ne 1 -or $member[0].creation_filetime -ne $child.creation_filetime -or $member[0].image -cne $child.image -or -not $member[0].membership_before -or -not $member[0].membership_after -or $member[0].exited){throw 'child-membership'}
  }
 }
 if($tx.spec.report){
  $j=Json (Join-Path $tx.dir $tx.spec.report);if($j.pending){throw 'report-missing'}
  if($tx.stage -eq 'wrong-contact-release'){
   if($j.status -cne 'cold_fail_requires_classification' -or $j.failed_assertions -ne 2 -or -not $j.consumer_rejects -or $j.adopted -ne $false -or $j.negative_rows.Count -ne 2){throw 'negative-consumer'}
   foreach($row in $j.negative_rows){if($row.expected -ne 4 -or $row.actual -ne 1 -or $row.pass -ne $false){throw 'negative-row'}}
   if($j.negative_rows[0].field -cne 'contact_id_transfer@0' -or $j.negative_rows[1].field -cne 'renaming_output_contact'){throw 'negative-chain'}
  }elseif($tx.stage -like 'suite-*'){
   if($j.status -cne 'cold_contract_only' -or $j.failed_assertions -ne 0 -or $j.unverified_oracle_fields.Count -ne 0 -or $j.schema_errors.Count -ne 0 -or $j.supplemental.Count -ne 22 -or $j.schema_negative_controls.Count -ne 4){throw 'cold-suite'}
   foreach($layer in @('rgb','typed','lifecycle','e2e')){if($j.layers.$layer.cases_enumerated -ne 89 -or $j.layers.$layer.cases_with_assertions -ne 89 -or $j.layers.$layer.failed_assertions -ne 0){throw 'cold-layer'}}
  }else{throw 'report-stage-allowlist'}
 }
}
function FinishTransaction($tx,$facts,$capacity){
 $tx.facts=$facts;if($facts.created -eq $true){$tx.state.native_launches_known++}
 WriteTransaction $tx "$($tx.stage)-result.json" @{attempt=$Attempt;stage=$tx.stage;facts=$facts;pending=$false}
 VerifyProcess $facts $tx.spec
 foreach($leaf in @("$($tx.stage).stdout.log","$($tx.stage).stderr.log")){$tx.handles[$leaf].Flush($true)}
 GuardExternalOutputs $tx
 VerifyConsumers $tx $facts
 $entries=@();foreach($leaf in @($tx.handles.Keys|Sort-Object)){if($leaf -cne "$($tx.stage)-verification.json"){$entries+=Entry (Join-Path $tx.dir $leaf)}}
 $verification=@{attempt=$Attempt;stage=$tx.stage;verification_exit=0;native_exit=$facts.exit_code;runner_exit=$facts.runner_exit;pending=$false;entries=$entries;capacity_after=$capacity;adopted=$false;producer_complete=$true;checkpoint_count=$tx.checkpoint}
 WriteTransaction $tx "$($tx.stage)-verification.json" $verification
 $tx.sealed=$true
 $anchor=Entry (Join-Path $tx.dir "$($tx.stage)-verification.json")
 $got=Json $anchor.path;if($got.verification_exit -ne 0 -or $got.pending){throw 'verification-readback'}
 foreach($e in $got.entries){CheckEntry $e}
 $tx.state.receipts+= $anchor;$tx.state.status='READY';$tx.state.next=$tx.spec.next;$tx.state.running=$null;$tx.state.revision++
 if(-not $tx.state.next){$tx.state.status='COMPLETE'}
 DurableState $tx.dir $tx.state $tx.fault
 if($tx.spec.next){CheckState (Json (Join-Path $tx.dir 'state.json')) $tx.spec.next $tx.state.contract_sha $tx.state.freeze_sha}
 $verification
}
function StopTransaction($tx,$reason){
 $tx.sealed=$false
 if($tx.handles.ContainsKey("$($tx.stage)-verification.json")){try{PutHandle $tx.handles["$($tx.stage)-verification.json"] @{attempt=$Attempt;stage=$tx.stage;verification_exit=1;pending=$false;reason=$reason;facts=$tx.facts}}catch{}}
 try{$tx.state.status='STOP';$tx.state.reason=$reason;$tx.state.revision++;DurableState $tx.dir $tx.state $tx.fault}catch{}
 $tx.sealed=$true
}
function CloseTransaction($tx){foreach($h in $tx.handles.Values){try{$h.Dispose()}catch{}}}
function DependentStep($result,[scriptblock]$next){if($result.exit_code -ne 0 -or $result.isError){throw 'dependent-step-rejected'};& $next}
