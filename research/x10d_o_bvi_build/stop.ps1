. "$PSScriptRoot/common.ps1"
. "$Source/transaction.ps1"
$s=Json "$Evidence/state.json"
if($s.status -cne 'INIT_PENDING' -or $s.native_stages_consumed -ne 0 -or $s.native_launches_known -ne 0){throw 'preformal-stop-state'}
$j=Json "$Evidence/diagnostic-pretest.json"
if($j.cases.Count -ne 20 -or $j.failed -ne 2 -or @($j.cases|Where-Object{$_.id -in @('D19','D20') -and $_.error -ceq 'transaction-root' -and -not $_.pass}).Count -ne 2){throw 'failure-readback'}
if(-not (Test-Path "$Evidence/failure-classification.json")){throw 'preserved-classification-required'}
foreach($name in @('running','reason')){if(-not $s.PSObject.Properties[$name]){$s|Add-Member -NotePropertyName $name -NotePropertyValue $null}}
$s.status='STOP';$s.next=$null;$s.running=$null;$s.reason='diagnostic-pretest-18-of-20; one authorized round exhausted';$s.revision++
DurableState $Evidence $s
NewJson "$Evidence/capacity-stop.json" (Capacity 1048576 0)
@{status=$s.status;revision=$s.revision;known_launches=$s.native_launches_known;state=(Entry "$Evidence/state.json");failure=(Entry "$Evidence/failure-classification.json")}|ConvertTo-Json -Depth 4 -Compress
