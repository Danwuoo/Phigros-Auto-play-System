$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$batchPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi'
$outPath=Join-Path $repoPath 'out/x10d-o-bvi'
if(Test-Path -LiteralPath $outPath){throw 'new out already exists'}
[IO.Directory]::CreateDirectory($outPath)|Out-Null
$cmakePath=(Get-Command cmake.exe).Source;$ninjaPath=(Get-Command ninja.exe).Source
$vcvars='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
foreach($config in @('release','debug','asan')){
 $type=if($config -eq 'release'){'Release'}else{'Debug'};$asan=if($config -eq 'asan'){'ON'}else{'OFF'}
 foreach($stage in @('configure','build')){
  $body=@('@echo off','set VSCMD_SKIP_SENDTELEMETRY=1','set VC_DISABLE_SQM=1','set VS_UNICODE_OUTPUT=',('call "'+$vcvars+'" -vcvars_ver=14.50 >nul'),'if errorlevel 1 exit /b 1')
  if($stage -eq 'configure'){$body+=('"'+$cmakePath+'" -S "'+$PSScriptRoot+'" -B "'+$outPath+'\'+$config+'" -G Ninja -DCMAKE_BUILD_TYPE='+$type+' -DBVI_ASAN='+$asan+' "-DCMAKE_MAKE_PROGRAM='+$ninjaPath+'" "-DCMAKE_CXX_FLAGS=/showIncludes"')}
  else{$body+=('"'+$cmakePath+'" --build "'+$outPath+'\'+$config+'" --parallel 2')}
  $body+='exit /b %errorlevel%'
  [IO.File]::WriteAllText((Join-Path $PSScriptRoot "$stage-$config.cmd"),($body -join "`r`n")+"`r`n",[Text.ASCIIEncoding]::new())
 }
}
[IO.Directory]::CreateDirectory("$batchPath/pre-build-source")|Out-Null
Copy-Item -LiteralPath $PSScriptRoot -Destination "$batchPath/pre-build-source/research" -Recurse
$files=@(Get-ChildItem -LiteralPath $PSScriptRoot -File|ForEach-Object{@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant();bytes=$_.Length}})
$inputs=@(foreach($name in @('normalized.json','oracle.json','typed-inputs.json','normalized-execution.json','typed-execution.json','specification-binding.json','author-corrections.json')){$p="$batchPath/$name";@{path=$p;sha256=(Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant();bytes=(Get-Item -LiteralPath $p).Length}})
$v=@{phase='all actual source/input/tool before first build';source=$files;inputs=$inputs;compiler_toolset='Community14.50.35717';cmake=(Get-FileHash -LiteralPath $cmakePath).Hash.ToLowerInvariant();ninja=(Get-FileHash -LiteralPath $ninjaPath).Hash.ToLowerInvariant();jobs=2;dependencies='installed nlohmann headers, MSVC CRT/STL and Windows SDK; no vcpkg install or product core'}
[IO.File]::WriteAllText("$batchPath/pre-build-binding.json",($v|ConvertTo-Json -Depth 15)+"`n",[Text.UTF8Encoding]::new($false))
Write-Output "standalone wrappers/source bound; cmake=$cmakePath ninja=$ninjaPath"
