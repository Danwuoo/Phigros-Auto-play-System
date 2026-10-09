param([Parameter(Mandatory)][string]$Report)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Report){throw 'fresh initial acquisition controls'}
if(-not ('PasOwnedV3' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'owned.cs')}
$taskRows=@()
foreach($taskCase in @(@('resume',0.0,1),@('before-boundary',0.049999,1),@('boundary',0.05,25),@('after-boundary',0.050001,25),@('steady',1.0,25))){
 $taskActual=[PasOwnedV3]::AcquisitionSleep($taskCase[1]);$taskRows+=@{id=$taskCase[0];pass=($taskActual -eq $taskCase[2]);actual=$taskActual;expected=$taskCase[2]}
}
foreach($taskCase in @(@('negative',-0.001),@('nan',[double]::NaN),@('positive-infinity',[double]::PositiveInfinity),@('negative-infinity',[double]::NegativeInfinity))){
 $taskRejected=$false;try{[void][PasOwnedV3]::AcquisitionSleep($taskCase[1])}catch{$taskRejected=$_.Exception.ToString().Contains('acquisition-clock')}
 $taskRows+=@{id=$taskCase[0];pass=$taskRejected;invalid_clock_rejected=$taskRejected}
}
# Fake root exits at 35 ms; a child image is readable only at 6..12 ms.
# A 25 ms cadence misses it. New cadence acquires it while it is readable.
$taskOldHit=$false;for($taskMs=0;$taskMs -lt 35;$taskMs+=25){if($taskMs -ge 6 -and $taskMs -le 12){$taskOldHit=$true}}
$taskNewHit=$false;for($taskMs=0;$taskMs -lt 35;$taskMs+=[PasOwnedV3]::AcquisitionSleep($taskMs/1000.0)){if($taskMs -ge 6 -and $taskMs -le 12){$taskNewHit=$true}}
$taskRows+=@{id='short-root-acquisition-gap';pass=(-not $taskOldHit -and $taskNewHit);old_25ms_acquired=$taskOldHit;new_acquired=$taskNewHit}
$taskRows+=@{id='unknown-image-still-rejected';pass=(-not [PasOwnedV3]::CachedImageAllowed($null,123,456,[IntPtr]789,$true))}
$taskSaved=[PasOwnedV3+PinnedIdentity]::new();$taskSaved.pid=123;$taskSaved.creation_filetime=456;$taskSaved.handle=[IntPtr]789;$taskSaved.image='FAKE-validated-image';$taskSaved.image_validation_qpc=100
$taskRows+=@{id='known-live-image-failure-still-rejected';pass=(-not [PasOwnedV3]::CachedImageAllowed($taskSaved,123,456,[IntPtr]789,$false))}
$taskPins=@('owned.cs','initial-controls.ps1'|ForEach-Object {$taskPath=Join-Path $PSScriptRoot $_;@{path=$taskPath;bytes=(Get-Item -LiteralPath $taskPath).Length;sha256=(Get-FileHash -LiteralPath $taskPath).Hash.ToLowerInvariant()}})
$taskFailed=@($taskRows|Where-Object {-not $_.pass}).Count
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Report)))|Out-Null
$taskValue=@{schema='pas.initial-identity-acquisition-controls.v1';cases=$taskRows;failed=$taskFailed;actual_CreateProcess=0;shared_source=$taskPins;initial_window_ms=50;initial_sleep_ms=1;steady_sleep_ms=25;fake_facts_not_platform_evidence=$true}
[IO.File]::WriteAllText($Report,($taskValue|ConvertTo-Json -Depth 8)+"`n",[Text.UTF8Encoding]::new($false))
@{cases=$taskRows.Count;failed=$taskFailed;actual_CreateProcess=0}|ConvertTo-Json -Compress
if($taskFailed){exit 1};exit 0
