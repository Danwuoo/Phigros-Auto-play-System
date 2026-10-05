$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$target=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/control-child-identity.json'
if(Test-Path -LiteralPath $target){throw 'control identity exists'}
$p=[Diagnostics.Process]::GetCurrentProcess()
$v=@{pid=$p.Id;creation_filetime=$p.StartTime.ToUniversalTime().ToFileTimeUtc();image=$p.MainModule.FileName;utc=[DateTime]::UtcNow.ToString('o');qpc_ticks=[Diagnostics.Stopwatch]::GetTimestamp();qpc_frequency=[Diagnostics.Stopwatch]::Frequency;sleep_ms=25000}
[IO.File]::WriteAllText($target,($v|ConvertTo-Json)+"`n",[Text.UTF8Encoding]::new($false))
Start-Sleep -Milliseconds 25000
