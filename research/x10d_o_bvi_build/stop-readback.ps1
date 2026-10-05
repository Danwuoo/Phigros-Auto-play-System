. "$PSScriptRoot/common.ps1"
. "$Source/transaction.ps1"
$s=Json "$Evidence/state.json";if($s.status -cne 'STOP'){throw 'stop-readback-status'}
$rows=@()
foreach($name in @('natural','nonzero','owned-child','probe-1','probe-2','probe-3','probe-4','configure-release','build-release','wrong-contact-release','suite-release','configure-debug','build-debug','suite-debug','configure-asan','build-asan','suite-asan')){
 $reason=$null;try{CheckLaunchState $Evidence $s $name uncreated-contract uncreated-freeze}catch{$reason=$_.Exception.Message}
 if($reason -cne 'state-blocked'){throw 'STOP-cross-shell-gap'}
 $rows+=@{stage=$name;rejected=$true;reason=$reason;native_calls=0}
}
NewJson "$Evidence/STOP-readback.json" @{state=(Entry "$Evidence/state.json");rows=$rows;new_shell=$true;read_only_consumer=$true;actual_CreateProcess=0;contract_or_freeze_created=$false}
@{status=$s.status;rejections=$rows.Count;native_calls=0}|ConvertTo-Json -Compress
