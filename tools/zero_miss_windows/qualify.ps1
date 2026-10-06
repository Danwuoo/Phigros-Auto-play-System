param([Parameter(Mandatory)][string]$FreshRoot,
      [Parameter(Mandatory)][string]$DiagnosticReceipt)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$qualifyRoot=[IO.Path]::GetFullPath($FreshRoot)
if(Test-Path -LiteralPath $qualifyRoot){throw 'qualification-root-exists'}
$localSource=Join-Path $qualifyRoot 'source'
. (Join-Path $PSScriptRoot 'process.ps1') -TaskRepo $taskRoot -TaskAttempt 'windows-round4-20261006' -TaskEvidence $qualifyRoot -TaskSource $localSource
$diagnostic=Json $DiagnosticReceipt
if($diagnostic.failed -ne 0 -or $diagnostic.cases.Count -ne 20 -or $diagnostic.actual_CreateProcess -ne 0){throw 'file-only-gate'}
[IO.Directory]::CreateDirectory($Source)|Out-Null
$pwsh=(Get-Command pwsh -ErrorAction Stop).Source
$cmake=(Get-Command cmake -ErrorAction Stop).Source
$vcvars='C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$cl='C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe'
foreach($p in @($pwsh,$cmake,$vcvars,$cl)){if(-not(Test-Path -LiteralPath $p -PathType Leaf)){throw "tool-missing:$p"}}
$processScript=Join-Path $PSScriptRoot 'process.ps1'
$loader=". '$processScript' -TaskRepo '$Repo' -TaskAttempt '$Attempt' -TaskEvidence '$qualifyRoot' -TaskSource '$Source'"
[IO.File]::WriteAllText((Join-Path $Source 'common.ps1'),$loader,[Text.UTF8Encoding]::new($false))
foreach($leaf in @('control-parent.ps1','control-child.ps1','identity.ps1')){
 [IO.File]::WriteAllBytes((Join-Path $Source $leaf),[IO.File]::ReadAllBytes((Join-Path $historical $leaf)))
}
$fixture=@'
param([int]$ReturnCode=0,[string]$Value='space token',[string]$Trailing='C:\argv-test\')
if($ReturnCode -notin @(0,7) -or $Value -cne 'space token' -or $Trailing -cne 'C:\argv-test\'){throw 'argv-fixture'}
[Console]::WriteLine('TRACE fixture entry '+[Environment]::CommandLine)
[Console]::WriteLine('TRACE fixture end exit='+$ReturnCode)
exit $ReturnCode
'@
$fixturePath=Join-Path $Source 'argv-fixture.ps1'
[IO.File]::WriteAllText($fixturePath,$fixture,[Text.UTF8Encoding]::new($false))
$commonInputs=@($DiagnosticReceipt,$processScript,(Join-Path $Source 'common.ps1'),$fixturePath)
$receipts=@()
foreach($code in @(0,7)){
 $stage=if($code -eq 0){'natural'}else{'nonzero'}
 $receipts+=InvokeLocalOwnedStage -StageRoot (Join-Path $qualifyRoot $stage) -Stage $stage -Executable $pwsh -Arguments @('-NoProfile','-File',$fixturePath,'-ReturnCode',"$code",'-Value','space token','-Trailing','C:\argv-test\') -NativeExit $code -RunnerExit $code -TotalSeconds 45 -Inputs $commonInputs
}
$bindingPath=Join-Path $qualifyRoot 'child-binding-template.json'
NewJson $bindingPath @{attempt=$Attempt;pwsh=$pwsh;child_script=(Join-Path $Source 'control-child.ps1');child_identity=$null;parent_identity=$null;parent_poll=$null}
$parent=Join-Path $Source 'control-parent.ps1'
$childInputs=$commonInputs+@($parent,(Join-Path $Source 'control-child.ps1'),(Join-Path $Source 'identity.ps1'))
$receipts+=InvokeLocalOwnedStage -StageRoot (Join-Path $qualifyRoot 'owned-child') -Stage 'owned-child' -Executable $pwsh -Arguments @('-NoProfile','-File',$parent,'-Binding',$bindingPath,'-AttemptId',$Attempt) -NativeExit 0 -RunnerExit 125 -TotalSeconds 60 -Inputs $childInputs -ChildBinding $bindingPath
$wrapperDir=Join-Path $Source 'wrappers-v0'
[IO.Directory]::CreateDirectory($wrapperDir)|Out-Null
$wrapperPath=Join-Path $wrapperDir 'probe.cmd'
$lines=@('@echo off','echo TRACE wrapper-v0 entry cmdcmdline=[%cmdcmdline%]',
 'set VSCMD_SKIP_SENDTELEMETRY=1','set VC_DISABLE_SQM=1','set VS_UNICODE_OUTPUT=',
 'echo TRACE L06 vcvars START',('call "'+$vcvars+'" -vcvars_ver=14.51 >nul'),
 'set "PAS_LOCAL_EXIT=%errorlevel%"','echo TRACE L07 vcvars END exit=%PAS_LOCAL_EXIT%',
 'if not "%PAS_LOCAL_EXIT%"=="0" exit /b %PAS_LOCAL_EXIT%',
 'echo TRACE L11 argv-configure START',('"'+$pwsh+'" -NoProfile -File "'+$fixturePath+'" -ReturnCode 0'),
 'set "PAS_LOCAL_EXIT=%errorlevel%"','echo TRACE L12 argv-configure END exit=%PAS_LOCAL_EXIT%',
 'if not "%PAS_LOCAL_EXIT%"=="0" exit /b %PAS_LOCAL_EXIT%',
 'echo TRACE L16 argv-build START',('"'+$pwsh+'" -NoProfile -File "'+$fixturePath+'" -ReturnCode 7'),
 'set "PAS_LOCAL_EXIT=%errorlevel%"','echo TRACE L17 argv-build END exit=%PAS_LOCAL_EXIT%',
 'if not "%PAS_LOCAL_EXIT%"=="7" exit /b 1',('"'+$cl+'" /Bv'),
 ('"'+$cmake+'" --version'),'if errorlevel 1 exit /b 1',
 'echo TRACE DIAGNOSTIC_POSITIVE wrapper-v0','exit /b 0')
[IO.File]::WriteAllText($wrapperPath,($lines -join "`r`n")+"`r`n",[Text.UTF8Encoding]::new($false))
$probeRoot=Join-Path $qualifyRoot 'probe-1'
$receipts+=InvokeLocalOwnedStage -StageRoot $probeRoot -Stage 'probe-1' -Executable 'C:\Windows\System32\cmd.exe' -Arguments @('/d','/s','/c',$wrapperPath) -TotalSeconds 90 -Inputs ($commonInputs+@($wrapperPath,$vcvars,$cl,$cmake))
$trace=[IO.File]::ReadAllText((Join-Path $probeRoot 'probe-1.stdout.log'))
foreach($marker in @('TRACE wrapper-v0 entry cmdcmdline=','TRACE L07 vcvars END exit=0','TRACE L12 argv-configure END exit=0','TRACE L17 argv-build END exit=7','TRACE DIAGNOSTIC_POSITIVE wrapper-v0')){if(-not $trace.Contains($marker)){throw "wrapper-marker:$marker"}}
foreach($receipt in $receipts){CheckEntry $receipt.receipt}
NewJson (Join-Path $qualifyRoot 'native-gate.json') @{schema='pas.windows-offline-process-gate.v1';attempt=$Attempt;
 diagnostic=(Entry $DiagnosticReceipt);file_only_cases=20;file_only_failed=0;
 native_controls=$receipts;native_controls_verified=4;wrapper_positive=$true;
 device_operations=0;historical_stop_resumed=$false}
$receipts|ConvertTo-Json -Depth 6
