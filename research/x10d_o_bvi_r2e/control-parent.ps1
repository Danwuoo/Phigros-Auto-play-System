param([Parameter(Mandatory)][string]$Binding,[Parameter(Mandatory)][string]$Attempt)
$ErrorActionPreference='Stop'
$b=Get-Content -LiteralPath $Binding -Raw|ConvertFrom-Json
if($Attempt -cne 'bvi-r2e-20261005-01' -or $b.attempt -cne $Attempt -or $b.child_script -cne (Join-Path $PSScriptRoot 'control-child.ps1')){throw 'parent-binding'}
$child=Start-Process -FilePath $b.pwsh -ArgumentList @('-NoProfile','-File',('"'+$b.child_script+'"'),'-Binding',('"'+$Binding+'"'),'-Attempt',$Attempt) -WindowStyle Hidden -PassThru
$timer=[Diagnostics.Stopwatch]::StartNew();$v=$null
while($timer.Elapsed.TotalSeconds -lt 3){try{$v=Get-Content -LiteralPath $b.child_identity -Raw|ConvertFrom-Json;if(-not $v.pending){break}}catch{};Start-Sleep -Milliseconds 25}
if(-not $v -or $v.pending -or $v.pid -ne $child.Id -or $v.attempt -cne $Attempt){throw 'child-identity-missing'}
$r=@{attempt=$Attempt;pid=$child.Id;creation_filetime=$child.StartTime.ToUniversalTime().ToFileTimeUtc();image=$child.MainModule.FileName;qpc_ticks=[Diagnostics.Stopwatch]::GetTimestamp();qpc_frequency=[Diagnostics.Stopwatch]::Frequency;utc=[DateTime]::UtcNow.ToString('o')}
if($r.creation_filetime -ne $v.creation_filetime -or $r.image -cne $v.image){throw 'child-identity-mismatch'}
$pending=Get-Content -LiteralPath $b.parent_identity -Raw|ConvertFrom-Json;if($pending.attempt -cne $Attempt -or -not $pending.pending){throw 'parent-reservation'}
$h=[IO.FileStream]::new($b.parent_identity,[IO.FileMode]::Open,[IO.FileAccess]::Write,[IO.FileShare]::ReadWrite)
try{$data=[Text.Encoding]::UTF8.GetBytes(($r|ConvertTo-Json -Compress)+"`n");$h.SetLength(0);$h.Write($data);$h.Flush($true)}finally{$h.Dispose()}
exit 0
