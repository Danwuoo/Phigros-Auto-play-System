param([string]$Mode,[string]$S,[string]$B,[string]$G,[string]$MakeProgram,[string]$BuildType,[string]$Asan,[int]$Parallel,[int]$ReturnCode=0)
$ErrorActionPreference='Stop'
$repo='C:\Users\wurre\Desktop\Phigros-Auto-play-System'
$source=Join-Path $repo 'research/x10d_o_bvi_build'
$out=Join-Path $repo 'out/x10d-o-bvi-build'
if($Mode -notin @('configure','build') -or $ReturnCode -notin @(0,7)){throw 'fixture-mode'}
if($Mode -eq 'configure'){
 if($S -cne $source -or $B -cne "$out\release" -or $G -cne 'Ninja' -or $BuildType -cne 'Release' -or $Asan -cne 'OFF' -or $MakeProgram -cne 'C:\Users\wurre\AppData\Local\Programs\Python\Python310\Scripts\ninja.exe'){throw 'fixture-configure-argv'}
}elseif($B -cne "$out\release" -or $Parallel -ne 2){throw 'fixture-build-argv'}
[Console]::WriteLine('TRACE fixture entry mode='+$Mode+' commandline='+[Environment]::CommandLine)
foreach($config in @(@('release','Release','OFF'),@('debug','Debug','OFF'),@('asan','Debug','ON'))){@{configuration=$config[0];type=$config[1];asan=$config[2];source=$S;out="$out\$($config[0])";mode=$Mode;parallel=$Parallel;pure_argv_fixture=$true}|ConvertTo-Json -Compress}
foreach($p in @('C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe','C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe')){$f=Get-Item -LiteralPath $p;[Console]::WriteLine('TOOL '+$f.FullName+' version='+$f.VersionInfo.FileVersion)}
[Console]::WriteLine('TRACE fixture end exit='+$ReturnCode)
exit $ReturnCode
