param([Parameter(Mandatory)][string]$Stage,[Parameter(Mandatory)][string]$AttemptId)
. "$PSScriptRoot/common.ps1"
. "$PSScriptRoot/transaction.ps1"
$ExpectedContract='140c52a35a01f8e9488e49a269be053b8b03403431428115b5c874e541eb3175'
$lock=$null;$tx=$null;$state=$null
try{
 if($AttemptId -cne $Attempt){throw 'attempt-id'};$binding=Binding
 if((Sha "$Evidence/contract.json") -cne $ExpectedContract){throw 'contract-sha'}
 $contract=Json "$Evidence/contract.json";$spec=@($contract.stages|Where-Object name -ceq $Stage);if($spec.Count -ne 1){throw 'stage-allowlist'};$spec=$spec[0]
 $lock=[IO.FileStream]::new("$Evidence/attempt.lock",[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
 $state=Json "$Evidence/state.json";CheckLaunchState $Evidence $state $Stage $ExpectedContract (Sha "$Evidence/freeze.json")
 $freeze=Json "$Evidence/freeze.json";foreach($e in @($freeze.files)+@($contract.inputs)+@($contract.dependencies)){CheckEntry $e}
 $protection=Protection
 # Estimate rejection occurs before RUNNING or output reservation and does not consume launch.
 try{$capacity=Capacity $spec.reserve_development $spec.reserve_out}catch{
  if($_.Exception.Message -cne 'PREFLIGHT_REJECTED-estimate'){throw}
  NewJson "$Evidence/$Stage-preflight-rejected.json" @{attempt=$Attempt;stage=$Stage;status='PREFLIGHT_REJECTED';reason=$_.Exception.Message;native_launched=$false;spec=$spec;actual=(Capacity)}
  [Console]::Error.WriteLine('PREFLIGHT_REJECTED; no launch consumed');exit 2
 }
 Add-Type -Path "$Source/owned.cs"
 $tx=NewTransaction $Evidence $spec $state
 BeginTransaction $tx @{attempt=$Attempt;stage=$Stage;binding=$binding;spec=$spec;executable=(Entry $spec.exe);capacity=$capacity;protection=$protection;pending=$false;launch='not_launched'}
 $facts=[R2FOwned+Result]::new();$tx.facts=$facts
 $callback=[Action[string,R2FOwned+Result]]{param($phase,$r)CheckpointTransaction $tx $phase $r}
 [R2FOwned]::Run($facts,$spec.exe,[string[]]$spec.argv,$binding.repo,$tx.handles["$Stage.stdout.log"],$tx.handles["$Stage.stderr.log"],$spec.total_s,$spec.stream_cap,$callback)
 $verification=FinishTransaction $tx $facts (Capacity)
 @{stage=$Stage;native_exit=$facts.exit_code;runner_exit=$facts.runner_exit;verification_exit=$verification.verification_exit;state=$tx.state.status;next=$tx.state.next;elapsed_s=$facts.elapsed_s}|ConvertTo-Json -Compress
 exit 0
}catch{
 $reason=$_.Exception.Message
 if($tx){StopTransaction $tx $reason}elseif($state){try{$state.status='STOP';$state.reason=$reason;$state.revision++;DurableState $Evidence $state}catch{}}
 [Console]::Error.WriteLine("R2F resume STOP: $reason");exit 1
}finally{if($tx){CloseTransaction $tx};if($lock){$lock.Dispose()}}
