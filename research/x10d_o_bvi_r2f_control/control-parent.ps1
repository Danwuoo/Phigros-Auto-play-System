param([Parameter(Mandatory)][string]$Binding,[Parameter(Mandatory)][Alias('Attempt')][string]$AttemptId)
. "$PSScriptRoot/common.ps1"
. "$PSScriptRoot/identity.ps1"
$b=Json $Binding;if($AttemptId -cne $Attempt -or $b.attempt -cne $Attempt -or $b.child_script -cne (Join-Path $PSScriptRoot 'control-child.ps1')){throw 'parent-binding'}
$child=Start-Process -FilePath $b.pwsh -ArgumentList @('-NoProfile','-File',('"'+$b.child_script+'"'),'-Binding',('"'+$Binding+'"'),'-AttemptId',$Attempt) -WindowStyle Hidden -PassThru
$expected=@{attempt=$Attempt;pid=$child.Id;creation_filetime=$child.StartTime.ToUniversalTime().ToFileTimeUtc();image=$child.MainModule.FileName}
$start=[Diagnostics.Stopwatch]::GetTimestamp();$deadline=$start+3*[Diagnostics.Stopwatch]::Frequency
$poll=@{schema=1;attempt=$Attempt;start_qpc=$start;deadline_qpc=$deadline;frequency=[Diagnostics.Stopwatch]::Frequency;polls=0;counts=@{};transitions=@();last=$null}
try{
 do{
  $r=InspectIdentity $b.child_identity $expected ([Diagnostics.Stopwatch]::GetTimestamp()) $deadline
  $poll.polls++;if(-not $poll.counts.ContainsKey($r.state)){$poll.counts[$r.state]=0};$poll.counts[$r.state]++
  if($poll.last -cne $r.state -and $poll.transitions.Count -lt 16){$poll.transitions+=@{state=$r.state;qpc=$r.qpc}};$poll.last=$r.state
  if($r.ready){break};if($r.state -eq 'timeout'){throw 'identity-publication-timeout'}
  if($r.state -in @('identity-mismatch','stale-publication','over-cap')){throw "identity-publication-$($r.state)"}
  Start-Sleep -Milliseconds 10
 }while($true)
 PublishIdentity $b.parent_identity $expected
 $poll.accepted=$true
}finally{
 $poll.end_qpc=[Diagnostics.Stopwatch]::GetTimestamp()
 $h=[IO.FileStream]::new($b.parent_poll,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite);try{PutHandle $h $poll 8192}finally{$h.Dispose()}
}
exit 0
