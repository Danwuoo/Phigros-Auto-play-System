param([string]$BuildRoot='out/manual-score-main80-lazy-20261013',[ValidatePattern('^r[1-9][0-9]*$')][string]$ValidationRun='r2')
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$build=[IO.Path]::GetFullPath((Join-Path $repo $BuildRoot))
if(-not $build.StartsWith($repo+'\out\',[StringComparison]::OrdinalIgnoreCase)){throw 'build must be inside workspace out'}
$output=Join-Path $build ('lazy-validation-'+$ValidationRun)
if(Test-Path -LiteralPath $output){throw 'preserve existing validation; choose a fresh build root'}
[IO.Directory]::CreateDirectory($output)|Out-Null
$environment=Get-Content -LiteralPath (Join-Path $repo 'out/windows-handoff/environment-prelive-pre-emulator-release-main32-inputs/compiler-environment.json') -Raw|ConvertFrom-Json
foreach($p in $environment.variables.PSObject.Properties){[Environment]::SetEnvironmentVariable($p.Name,[string]$p.Value,'Process')}
$tool=Join-Path $repo 'out/windows-handoff/tool-main05-01/bin'
$source=Join-Path $PSScriptRoot 'lazy_archive_tests.cpp'
$object=Join-Path $output 'lazy_archive_tests.obj'
$exe=Join-Path $output 'lazy_archive_tests.exe'
$compile=Join-Path $output 'compile.rsp'
$compilerArgs=@('/nologo','/c','/std:c++20','/MD','/O2','/DNDEBUG','/W4','/permissive-','/EHsc',('/I"'+(Join-Path $repo 'include')+'"'),('/I"'+(Join-Path $repo 'out/vcpkg_installed/x64-windows/include')+'"'),('/Fo"'+$object+'"'),('"'+$source+'"'))
[IO.File]::WriteAllLines($compile,$compilerArgs)
$compilerOutput=& (Join-Path $tool 'cl.exe') ('@'+$compile) 2>&1
$compilerExit=$LASTEXITCODE
[IO.File]::WriteAllLines((Join-Path $output 'compile.log'),[string[]]$compilerOutput)
if($compilerExit -ne 0){$compilerOutput|Select-Object -Last 8|Write-Output;throw 'lazy validation compile failed'}
$linkLines=Get-Content -LiteralPath (Join-Path $build 'link.rsp')
$libraryLines=@($linkLines|Where-Object {$_ -match 'pas_core[.]lib'})
if($libraryLines.Count -ne 1){throw 'exact preserved production library row required'}
$link=Join-Path $output 'link.rsp'
[IO.File]::WriteAllLines($link,@('/nologo','/INCREMENTAL:NO','/SUBSYSTEM:CONSOLE',('/OUT:"'+$exe+'"'),('"'+$object+'"'),$libraryLines[0]))
& (Join-Path $tool 'link.exe') ('@'+$link)
if($LASTEXITCODE -ne 0){throw 'lazy validation link failed'}
foreach($dll in Get-ChildItem -LiteralPath $build -Filter '*.dll' -File){Copy-Item -LiteralPath $dll.FullName -Destination $output}
$validationOutput=& $exe (Join-Path $output 'synthetic-fixtures') 2>&1
$validationExit=$LASTEXITCODE
[IO.File]::WriteAllLines((Join-Path $output 'test.log'),[string[]]$validationOutput)
$validationOutput|Write-Output
if($validationExit -ne 0){throw 'lazy diagnostic validation failed'}
