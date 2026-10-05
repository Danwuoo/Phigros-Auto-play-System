param([Parameter(Mandatory)][string]$Stage,[Parameter(Mandatory)][string]$AttemptId)
. "$PSScriptRoot/common.ps1"
. "$PSScriptRoot/transaction.ps1"
. "$PSScriptRoot/diagnostic.ps1"
$ExpectedRunnerContract='REPLACE_CONTRACT_SHA'
$lock=$null;$tx=$null;$state=$null
try{
 if($AttemptId -cne $Attempt){throw 'attempt-id'};$binding=Binding
 if((Sha "$Evidence/runner-contract.json") -cne $ExpectedRunnerContract){throw 'runner-contract-sha'}
 $lock=[IO.FileStream]::new("$Evidence/attempt.lock",[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
 $state=Json "$Evidence/state.json"
 $product=$Stage -notin @('natural','nonzero','owned-child','probe-1','probe-2','probe-3','probe-4')
 $contract=Json "$Evidence/runner-contract.json";$contractSha=$ExpectedRunnerContract;$freezePath="$Evidence/runner-freeze.json"
 if($product){DiagnosticGate $state;if(-not $state.product_frozen){throw 'product-freeze-required'};$freezePath="$Evidence/product-freeze.json";$pf=Json $freezePath;CheckEntry $pf.contract;$contract=Json $pf.contract.path;$contractSha=$pf.contract.sha256;if($state.product_contract_sha -cne $contractSha){throw 'product-contract-sha'}}
 $spec=@($contract.stages|Where-Object name -ceq $Stage);if($spec.Count -ne 1){throw 'stage-allowlist'};$spec=$spec[0]
 CheckLaunchState $Evidence $state $Stage $contractSha (Sha $freezePath)
 foreach($e in @((Json "$Evidence/runner-freeze.json").files)+@((Json $freezePath).files)+@($contract.inputs)+@($contract.dependencies)){CheckEntry $e}
 if($Stage -like 'probe-*'){DiagnosticSlot $state $spec;foreach($e in (Json "$Evidence/wrapper-v$($state.wrapper_revision).json").files){CheckEntry $e.entry};CheckEntry (Json "$Evidence/wrapper-v$($state.wrapper_revision).json").immutable_generator}
 $protection=Protection
 try{$capacity=Capacity $spec.reserve_development $spec.reserve_out}catch{
  if($_.Exception.Message -cne 'PREFLIGHT_REJECTED-estimate'){throw}
  NewJson "$Evidence/$Stage-preflight-rejected.json" @{attempt=$Attempt;stage=$Stage;status='PREFLIGHT_REJECTED';reason=$_.Exception.Message;native_launched=$false;spec=$spec;actual=(Capacity)}
  [Console]::Error.WriteLine('PREFLIGHT_REJECTED; no launch consumed');exit 2
 }
 Add-Type -Path "$Source/owned.cs"
 $q=[R2FOwned].GetMethod('Quote',[Reflection.BindingFlags]'NonPublic,Static')
 $line=@((& { $q.Invoke($null,@([string]$spec.exe)) }));foreach($a in $spec.argv){$line+=$q.Invoke($null,@([string]$a))};$commandline=$line -join ' '
 if($Stage -like 'probe-*'){$state.diagnostic_slots_consumed++}
 $tx=NewTransaction $Evidence $spec $state
 BeginTransaction $tx @{attempt=$Attempt;stage=$Stage;binding=$binding;spec=$spec;commandline_to_CreateProcess=$commandline;executable=(Entry $spec.exe);capacity=$capacity;protection=$protection;pending=$false;launch='not_launched'}
 $facts=[R2FOwned+Result]::new();$tx.facts=$facts
 $callback=[Action[string,R2FOwned+Result]]{param($phase,$r)CheckpointTransaction $tx $phase $r}
 [R2FOwned]::Run($facts,$spec.exe,[string[]]$spec.argv,$binding.repo,$tx.handles["$Stage.stdout.log"],$tx.handles["$Stage.stderr.log"],$spec.total_s,$spec.stream_cap,$callback)
 $verification=if($Stage -like 'probe-*'){FinishDiagnostic $tx $facts (Capacity)}else{FinishTransaction $tx $facts (Capacity)}
 @{stage=$Stage;native_exit=$facts.exit_code;runner_exit=$facts.runner_exit;verification_exit=$verification.verification_exit;classification=$verification.classification;state=$tx.state.status;next=$tx.state.next;elapsed_s=$facts.elapsed_s}|ConvertTo-Json -Compress
 if($tx.state.status -eq 'STOP'){exit 1};exit 0
}catch{
 $reason=$_.Exception.Message
 if($tx){StopTransaction $tx $reason}elseif($state){try{$state.status='STOP';$state.reason=$reason;$state.revision++;DurableState $Evidence $state}catch{}}
 [Console]::Error.WriteLine("BVI build STOP: $reason");exit 1
}finally{if($tx){CloseTransaction $tx};if($lock){$lock.Dispose()}}
