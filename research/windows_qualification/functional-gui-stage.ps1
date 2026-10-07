param([Parameter(Mandatory)][string]$StageName,[Parameter(Mandatory)][string]$Executable,
 [string[]]$Arguments=@(),[Parameter(Mandatory)][string]$GateReceipt,
 [string[]]$Inputs=@(),[int]$ExpectedNative=0,[int]$TotalSeconds=300)
$ErrorActionPreference='Stop'
$taskRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if($StageName -notmatch '^[a-z0-9-]{1,90}$' -or $TotalSeconds -lt 31 -or $TotalSeconds -gt 1800){throw 'functional-stage-scope'}
$bundle=Join-Path $taskRepo ('out/windows-handoff/'+$StageName+'-inputs')
if(Test-Path -LiteralPath $bundle){throw 'fresh-functional-inputs'}
[IO.Directory]::CreateDirectory($bundle)|Out-Null
$envelope=Join-Path $taskRepo 'out/functional-gui-build-03/pas_functional_envelope.exe'
$childSpec=Join-Path $bundle 'functional-child.json'
[IO.File]::WriteAllText($childSpec,(@{schema='pas.functional-envelope.v1';executable=$Executable;argv=@($Arguments);
 child_seconds=($TotalSeconds-30);device_allowed=$false}|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
$spec=Join-Path $bundle 'spec.json'
$extra=@(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'functional-gui-source') -File|ForEach-Object FullName)
foreach($stage in @('configure-functional-gui-03','build-functional-gui-03')){$extra+=@(Join-Path $taskRepo ('out/windows-handoff/'+$stage+'/'+$stage+'-verification.json'))}
[IO.File]::WriteAllText($spec,(@{stage_root=(Join-Path $taskRepo ('out/windows-handoff/'+$StageName));stage=$StageName;
 exe=$envelope;argv=@($childSpec);native_exit=$ExpectedNative;runner_exit=$ExpectedNative;total_s=$TotalSeconds;stream_cap=4194304;
 inputs=@($PSCommandPath,$envelope,$childSpec,$Executable)+$extra+$Inputs}|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
& (Join-Path $taskRepo 'tools/zero_miss_windows/invoke.ps1') -Specification $spec -GateReceipt $GateReceipt
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
$trace=[IO.File]::ReadAllText((Join-Path $taskRepo ('out/windows-handoff/'+$StageName+'/'+$StageName+'.stdout.log')))
if(-not $trace.Contains('TRACE functional END exit='+$ExpectedNative)){throw 'functional-native-exit-trace'}
# Functional and read-only audit only. Never a direct cost-producer receipt.
