param([ValidateRange(0,2)][int]$Revision)
. "$PSScriptRoot/common.ps1"
. "$Source/transaction.ps1"
$s=Json "$Evidence/state.json"
if($Revision -eq 0){if($s.status -cne 'INIT_PENDING'){throw 'initial-wrapper-before-freeze'}}else{
 $lock=[IO.FileStream]::new("$Evidence/attempt.lock",[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
 try{
  if($s.status -cne 'READY' -or $s.next -cne "probe-$($Revision+1)" -or $s.wrapper_repairs -ne $Revision-1 -or $s.diagnostic_slots_consumed -ne $Revision){throw 'wrapper-repair-state'}
  $v=Json $s.diagnostic_receipts[-1].path;if($v.classification -cne 'DIAGNOSTIC_REJECTED'){throw 'wrapper-repair-without-rejection'}
  foreach($e in (Json "$Evidence/runner-freeze.json").files){CheckEntry $e};CheckEntry (Json "$Evidence/runner-freeze.json").contract
 }catch{$lock.Dispose();throw}
}
try{
 $null=Capacity 131072 0
 $dir=Join-Path $Source "wrappers-v$Revision";if(Test-Path $dir){throw 'wrapper-version-once'};[IO.Directory]::CreateDirectory($dir)|Out-Null
 $vc='C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat'
 $cmake='C:/Users/wurre/AppData/Local/Programs/Python/Python310/Lib/site-packages/cmake/data/bin/cmake.exe'
 $ninja='C:/Users/wurre/AppData/Local/Programs/Python/Python310/Scripts/ninja.exe'
 if($Revision -gt 0){$vc=$vc.Replace('/','\');$cmake=$cmake.Replace('/','\');$ninja=$ninja.Replace('/','\')}
 $pwsh=Join-Path $PSHOME 'pwsh.exe'
 $head=@('@echo off',("echo TRACE wrapper-v$Revision entry cmdcmdline=[%cmdcmdline%]"),'set VSCMD_SKIP_SENDTELEMETRY=1','set VC_DISABLE_SQM=1','set VS_UNICODE_OUTPUT=','echo TRACE L06 vcvars START',('call "'+$vc+'" -vcvars_ver=14.50 >nul'),'set "BVI_EXIT=%errorlevel%"','echo TRACE L07 vcvars END exit=%BVI_EXIT%','if not "%BVI_EXIT%"=="0" exit /b %BVI_EXIT%')
 $fixture=@('echo TRACE L11 argv-configure START',('"'+$pwsh+'" -NoProfile -File "'+$Source+'\argv-fixture.ps1" -Mode configure -S "'+$Source+'" -B "'+$Out+'\release" -G Ninja -BuildType Release -Asan OFF -MakeProgram "'+$ninja.Replace('/','\')+'"'),'set "BVI_EXIT=%errorlevel%"','echo TRACE L12 argv-configure END exit=%BVI_EXIT%','if not "%BVI_EXIT%"=="0" exit /b %BVI_EXIT%','echo TRACE L16 argv-build START',('"'+$pwsh+'" -NoProfile -File "'+$Source+'\argv-fixture.ps1" -Mode build -B "'+$Out+'\release" -Parallel 2 -ReturnCode 7'),'set "BVI_EXIT=%errorlevel%"','echo TRACE L17 argv-build END exit=%BVI_EXIT%','if not "%BVI_EXIT%"=="7" exit /b 1','where cl.exe','if errorlevel 1 exit /b 1',('"'+$cmake+'" --version'),'if errorlevel 1 exit /b 1',('"'+$ninja+'" --version'),'if errorlevel 1 exit /b 1',("echo TRACE DIAGNOSTIC_POSITIVE wrapper-v$Revision"),'exit /b 0')
 function BatchFile($path,$lines){$data=[Text.Encoding]::ASCII.GetBytes(($lines -join "`r`n")+"`r`n");$h=NewHandle $path;try{$h.Write($data);$h.Flush($true)}finally{$h.Dispose()}}
 BatchFile "$dir/probe.cmd" ($head+$fixture)
 foreach($c in @(@('release','Release','OFF'),@('debug','Debug','OFF'),@('asan','Debug','ON'))){
  $configure=@(('echo TRACE CMAKE configure-'+$c[0]+' ENTRY'),('"'+$cmake+'" -S "'+$Source+'" -B "'+$Out+'\'+$c[0]+'" -G Ninja -DCMAKE_BUILD_TYPE='+$c[1]+' -DBVI_ASAN='+$c[2]+' "-DCMAKE_MAKE_PROGRAM='+$ninja+'"'),'set "BVI_EXIT=%errorlevel%"',('echo TRACE CMAKE configure-'+$c[0]+' END exit=%BVI_EXIT%'),'exit /b %BVI_EXIT%')
  $build=@(('echo TRACE CMAKE build-'+$c[0]+' ENTRY'),('"'+$cmake+'" --build "'+$Out+'\'+$c[0]+'" --parallel 2'),'set "BVI_EXIT=%errorlevel%"',('echo TRACE CMAKE build-'+$c[0]+' END exit=%BVI_EXIT%'),'exit /b %BVI_EXIT%')
  BatchFile "$dir/configure-$($c[0]).cmd" ($head+$configure);BatchFile "$dir/build-$($c[0]).cmd" ($head+$build)
 }
 $files=@(Get-ChildItem -LiteralPath $dir -File|Sort-Object Name|ForEach-Object{$e=Entry $_.FullName;$b=[IO.File]::ReadAllBytes($_.FullName);@{entry=$e;encoding='ASCII/UTF8-no-BOM';NUL=($b -contains 0);CRLF=([Text.Encoding]::ASCII.GetString($b).Replace("`r`n",'').Contains("`n") -eq $false)}})
 NewJson "$Evidence/wrapper-v$Revision.json" @{revision=$Revision;files=$files;vcvars=$vc;cmake=$cmake;ninja=$ninja;repair=if($Revision){'Normalize literal executable/batch Windows paths to backslashes; preserve args and exit propagation'}else{'Marked original forward-slash wrapper copy; no product executed'};immutable_generator=(Entry "$Source/wrappers.ps1")}
 if($Revision){$s.wrapper_repairs=$Revision;$s.wrapper_revision=$Revision;$s.revision++;DurableState $Evidence $s}
 @{wrapper_revision=$Revision;files=$files.Count;capacity=(Capacity)}|ConvertTo-Json -Depth 3 -Compress
}finally{if($lock){$lock.Dispose()}}
