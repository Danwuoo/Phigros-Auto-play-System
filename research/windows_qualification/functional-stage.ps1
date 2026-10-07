param([Parameter(Mandatory)][string]$StageName,[Parameter(Mandatory)][string]$Executable,
 [string[]]$Arguments=@(),[Parameter(Mandatory)][string]$GateReceipt,
 [string[]]$Inputs=@(),[int]$ExpectedNative=0,[int]$TotalSeconds=300)
$ErrorActionPreference='Stop'
$taskRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if($StageName -notmatch '^[a-z0-9-]{1,90}$'){throw 'functional-stage-name'}
$bundle=Join-Path $taskRepo ('out/windows-handoff/'+$StageName+'-inputs')
if(Test-Path -LiteralPath $bundle){throw 'fresh-functional-inputs'}
[IO.Directory]::CreateDirectory($bundle)|Out-Null
$pwsh=(Get-Command pwsh -ErrorAction Stop).Source
foreach($value in @($Executable,$pwsh)+$Arguments){if($value -match '["%\r\n]'){throw 'literal-functional-argument'}}
$command='"'+$Executable+'" '+(($Arguments|ForEach-Object {'"'+$_+'"'}) -join ' ')
$cmd=Join-Path $bundle 'functional.cmd'
$lines=@('@echo off','echo TRACE functional START',$command,'set "PAS_FUNCTIONAL_EXIT=%errorlevel%"',
 'echo TRACE functional END exit=%PAS_FUNCTIONAL_EXIT%',
 ('"'+$pwsh+'" -NoProfile -Command "[Threading.Thread]::Sleep(750)"'),'exit /b %PAS_FUNCTIONAL_EXIT%')
[IO.File]::WriteAllText($cmd,($lines -join "`r`n")+"`r`n",[Text.UTF8Encoding]::new($false))
$spec=Join-Path $bundle 'spec.json'
[IO.File]::WriteAllText($spec,(@{stage_root=(Join-Path $taskRepo ('out/windows-handoff/'+$StageName));stage=$StageName;
 exe=$env:ComSpec;argv=@('/d','/s','/c',$cmd);native_exit=$ExpectedNative;runner_exit=$ExpectedNative;
 total_s=$TotalSeconds;stream_cap=4194304;inputs=@($PSCommandPath,$cmd,$Executable,$pwsh)+$Inputs}|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
& (Join-Path $taskRepo 'tools/zero_miss_windows/invoke.ps1') -Specification $spec -GateReceipt $GateReceipt
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
$trace=[IO.File]::ReadAllText((Join-Path $taskRepo ('out/windows-handoff/'+$StageName+'/'+$StageName+'.stdout.log')))
if(-not $trace.Contains('TRACE functional END exit='+$ExpectedNative)){throw 'functional-native-exit-trace'}
# This command envelope is functional verification only. Its cmd.exe Job root
# cannot qualify direct-producer identity or cold/production cost admission.
