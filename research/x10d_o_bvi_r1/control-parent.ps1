$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$target=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/control-child-identity.json'
if(Test-Path -LiteralPath $target){throw 'control already ran'}
$childPath=Join-Path $PSScriptRoot 'control-child.ps1'
$child=Start-Process -FilePath (Join-Path $PSHOME 'pwsh.exe') -ArgumentList @('-NoProfile','-File',('"'+$childPath+'"')) -WindowStyle Hidden -PassThru
$timer=[Diagnostics.Stopwatch]::StartNew()
while(-not (Test-Path -LiteralPath $target)){if($timer.Elapsed.TotalSeconds -ge 3){throw 'control child identity not written'};Start-Sleep -Milliseconds 25}
$identity=Get-Content -LiteralPath $target -Raw|ConvertFrom-Json
if($identity.pid -ne $child.Id){throw 'wrong child identity'}
$v=@{child_pid=$child.Id;creation_filetime=$child.StartTime.ToUniversalTime().ToFileTimeUtc();image=$child.MainModule.FileName;child_identity_seen=$true;utc=[DateTime]::UtcNow.ToString('o')}
[IO.File]::WriteAllText((Join-Path (Split-Path $target) 'control-parent-expected-child.json'),($v|ConvertTo-Json)+"`n",[Text.UTF8Encoding]::new($false))
exit 0
