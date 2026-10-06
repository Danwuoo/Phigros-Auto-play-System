param([Parameter(Mandatory)][string]$Report,[Parameter(Mandatory)][ValidatePattern('^[a-z0-9-]{1,24}$')][string]$Attempt,[int]$ExpectedNative=0)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh='C:/Users/wurre/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/pwsh.exe'
$audit=Join-Path $repo 'out/prelive-audit-release-01/prelive_audit.exe'
$r=Get-Content -LiteralPath $Report -Raw|ConvertFrom-Json
$suffix=if($r.schema -eq 'pas.prelive-current-pixels.v1'){@('.events.jsonl','.rows.jsonl')}else{@('.journal.jsonl','.attempts.json')}
$inputs=@($Report,$audit)+@($suffix|ForEach-Object {$Report+$_})+@(Get-ChildItem $PSScriptRoot -File|ForEach-Object FullName)
$name='audit-data-prelive-release-'+$Attempt
$params=Join-Path $pkg ($name+'-params.json')
if(Test-Path -LiteralPath $params){throw 'fresh audit params'}
$arguments=@{StageName=$name;Executable=$audit;Arguments=@($Report,(Join-Path $pkg ('audit-'+$Attempt+'.json')));GateReceipt=(Join-Path $repo 'out/windows-handoff/native-qualification-01/native-gate.json');Inputs=$inputs;TotalSeconds=300;ExpectedNative=$ExpectedNative}
[IO.File]::WriteAllText($params,(@{script=(Join-Path $repo 'tools/zero_miss_windows/native-stage.ps1');arguments=$arguments}|ConvertTo-Json -Depth 16),[Text.UTF8Encoding]::new($false))
& $pwsh -NoProfile -File (Join-Path $repo 'out/windows-handoff/withdrawal-review-01/stage.ps1') -Parameters $params
if($LASTEXITCODE -ne 0){throw 'audit stage unverified'}
