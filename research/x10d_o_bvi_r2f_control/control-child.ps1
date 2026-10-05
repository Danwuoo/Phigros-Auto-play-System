param([Parameter(Mandatory)][string]$Binding,[Parameter(Mandatory)][Alias('Attempt')][string]$AttemptId)
. "$PSScriptRoot/common.ps1"
. "$PSScriptRoot/identity.ps1"
$b=Json $Binding;if($AttemptId -cne $Attempt -or $b.attempt -cne $Attempt){throw 'child-binding'}
$p=[Diagnostics.Process]::GetCurrentProcess()
PublishIdentity $b.child_identity @{attempt=$Attempt;pid=$p.Id;creation_filetime=$p.StartTime.ToUniversalTime().ToFileTimeUtc();image=$p.MainModule.FileName;publish_start_qpc=[Diagnostics.Stopwatch]::GetTimestamp();sleep_ms=25000}
Start-Sleep -Milliseconds 25000
