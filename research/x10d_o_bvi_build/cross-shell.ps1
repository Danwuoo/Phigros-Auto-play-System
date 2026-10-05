. "$PSScriptRoot/common.ps1"
. "$Source/transaction.ps1"
if((Json "$Evidence/state.json").status -cne 'INIT_PENDING'){throw 'cross-before-freeze'}
$path="$Evidence/scratch-mock-v1/stop-state.json";$reason=$null
try{CheckState (Json $path) nonzero contract freeze}catch{$reason=$_.Exception.Message}
if($reason -cne 'state-blocked'){throw 'S06-cross-shell'}
NewJson "$Evidence/mock-S06-cross-shell.json" @{rejected=$true;reason=$reason;state=(Entry $path);native_calls=0}
$rows=@()
foreach($id in @('T15','T16','T17','T18')){
 $dir="$Evidence/scratch-transaction-round-1/$id";$s=Json "$dir/state.json";$reason=$null
 try{CheckLaunchState $dir $s $s.next mock-contract mock-freeze}catch{$reason=$_.Exception.Message}
 if($reason -notin @('state-blocked','consumed-output-reservation')){throw 'cross-shell-transaction'}
 $rows+=@{case=$id;status=$s.status;reason=$reason;rejected=$true;state=(Entry "$dir/state.json")}
}
NewJson "$Evidence/transaction-cross-shell.json" @{rows=$rows;native_calls=0;shared_source=(Entry "$Source/transaction.ps1")}
@{rejected=5;native_calls=0}|ConvertTo-Json -Compress
