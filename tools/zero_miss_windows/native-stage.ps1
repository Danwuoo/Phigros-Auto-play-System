param([Parameter(Mandatory)][string]$StageName,[Parameter(Mandatory)][string]$Executable,
      [string[]]$Arguments=@(),[Parameter(Mandatory)][string]$GateReceipt,
      [string[]]$Inputs=@(),[ValidateRange(30,1800)][int]$TotalSeconds=120,
      [int]$ExpectedNative=0)
$ErrorActionPreference='Stop'
$taskRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if($StageName -notmatch '^[a-z0-9-]{1,90}$'){throw 'stage-name'}
$Executable=[IO.Path]::GetFullPath($Executable)
$bundle=Join-Path $taskRepo ('out/windows-handoff/'+$StageName+'-inputs')
if(Test-Path -LiteralPath $bundle){throw 'fresh-native-inputs'}
[IO.Directory]::CreateDirectory($bundle)|Out-Null
foreach($v in @($Executable)+$Arguments){if($v -match '["%\r\n]'){throw 'literal-cmd-argument'}}
$command='"'+$Executable+'" '+(($Arguments|ForEach-Object {'"'+$_+'"'}) -join ' ')
$cmdPath=Join-Path $bundle 'native.cmd'
$lines=@('@echo off','echo TRACE native START',$command,'set "PAS_NATIVE_EXIT=%errorlevel%"',
 'echo TRACE native END exit=%PAS_NATIVE_EXIT%','exit /b %PAS_NATIVE_EXIT%')
[IO.File]::WriteAllText($cmdPath,($lines -join "`r`n")+"`r`n",[Text.UTF8Encoding]::new($false))
$spec=@{stage_root=(Join-Path $taskRepo ('out/windows-handoff/'+$StageName));stage=$StageName;
 exe=$env:ComSpec;argv=@('/d','/s','/c',$cmdPath);native_exit=$ExpectedNative;runner_exit=$ExpectedNative;
 total_s=$TotalSeconds;stream_cap=4194304;inputs=(@($PSCommandPath,$cmdPath,$Executable)+$Inputs)}
$specPath=Join-Path $bundle 'spec.json'
[IO.File]::WriteAllText($specPath,($spec|ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'invoke.ps1') -Specification $specPath -GateReceipt $GateReceipt
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
$trace=[IO.File]::ReadAllText((Join-Path $spec.stage_root ($StageName+'.stdout.log')))
if($trace -notmatch ('TRACE native END exit='+[regex]::Escape([string]$ExpectedNative))){throw 'native-exit-trace'}
exit 0
