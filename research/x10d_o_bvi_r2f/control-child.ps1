param([Parameter(Mandatory)][string]$Binding,[Parameter(Mandatory)][string]$Attempt)
$ErrorActionPreference='Stop'
$b=Get-Content -LiteralPath $Binding -Raw|ConvertFrom-Json
if($Attempt -cne 'bvi-r2f-20261005-01' -or $b.attempt -cne $Attempt){throw 'child-binding'}
$p=[Diagnostics.Process]::GetCurrentProcess()
$v=@{attempt=$Attempt;pid=$p.Id;creation_filetime=$p.StartTime.ToUniversalTime().ToFileTimeUtc();image=$p.MainModule.FileName;qpc_ticks=[Diagnostics.Stopwatch]::GetTimestamp();qpc_frequency=[Diagnostics.Stopwatch]::Frequency;utc=[DateTime]::UtcNow.ToString('o');sleep_ms=25000}
$pending=Get-Content -LiteralPath $b.child_identity -Raw|ConvertFrom-Json;if($pending.attempt -cne $Attempt -or -not $pending.pending){throw 'child-reservation'}
$h=[IO.FileStream]::new($b.child_identity,[IO.FileMode]::Open,[IO.FileAccess]::Write,[IO.FileShare]::ReadWrite)
try{$data=[Text.Encoding]::UTF8.GetBytes(($v|ConvertTo-Json -Compress)+"`n");$h.SetLength(0);$h.Write($data);$h.Flush($true)}finally{$h.Dispose()}
Start-Sleep -Milliseconds 25000
