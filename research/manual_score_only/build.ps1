param([ValidateSet('baseline','lazy')][string]$Variant='baseline',[string]$OutputRoot='')
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
if(-not $OutputRoot){$OutputRoot='out/manual-score-main80-'+$Variant+'-review-20261013'}
$manualSource=if($Variant -eq 'lazy'){'research/manual_score_only/variants/lazy/manual_session.cpp'}else{'src/manual_session.cpp'}
$output=[IO.Path]::GetFullPath((Join-Path $repo $OutputRoot))
if(-not $output.StartsWith($repo+'\out\',[StringComparison]::OrdinalIgnoreCase)){throw 'output must be inside workspace out'}
if(Test-Path -LiteralPath (Join-Path $output 'pas.exe')){throw 'use a fresh output root; preserve existing binary'}
[IO.Directory]::CreateDirectory($output)|Out-Null
$binding=Get-Content -LiteralPath (Join-Path $repo 'out/prelive-producer-eligibility-evaluation-main80-e1/build-binding.json') -Raw|ConvertFrom-Json
if($binding.source_commit -cne '3ee69edc85acb6b9b968424374a376f351d0e2dd'){throw 'main80 binding required'}
$environmentBinding=Get-Content -LiteralPath $binding.compiler_environment.path -Raw|ConvertFrom-Json
$environment=Get-Content -LiteralPath $environmentBinding.environment.path -Raw|ConvertFrom-Json
foreach($p in $environment.variables.PSObject.Properties){[Environment]::SetEnvironmentVariable($p.Name,[string]$p.Value,'Process')}
$base=Join-Path $repo 'out/prelive-producer-eligibility-main80-e1'
$ninja=Get-Content -LiteralPath (Join-Path $base 'build.ninja') -Raw
$match=[regex]::Match($ninja,'(?m)^build pas[.]exe: [^\r\n]*\r?\n(?<body>(?:[ \t]+[^\r\n]*\r?\n)*)')
if(-not $match.Success){throw 'main80 link row missing'}
$libraries=[regex]::Match($match.Groups['body'].Value,'(?m)^  LINK_LIBRARIES = (.*)$').Groups[1].Value.Replace('$:',':').Replace('$ ',' ')
$motion=@(Get-ChildItem -LiteralPath (Join-Path $base 'CMakeFiles/game_motion_delta.dir') -Recurse -File -Filter game_motion.cpp.obj)
$admission=@(Get-ChildItem -LiteralPath (Join-Path $base 'CMakeFiles/candidate_admission_delta.dir') -Recurse -File -Filter candidate_admission.cpp.obj)
if($motion.Count -ne 1 -or $admission.Count -ne 1){throw 'exact main80 overlay objects required'}
foreach($library in $binding.imported_static_libraries | Where-Object role -match '^(production_|verification_)'){
 if((Get-FileHash -LiteralPath $library.binary.path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $library.binary.sha256){throw 'preserved library SHA differs'}
}
foreach($object in @($motion[0],$admission[0])){
 $pin=@($binding.compiled_outputs|Where-Object path -eq $object.FullName)
 if($pin.Count -ne 1 -or (Get-FileHash -LiteralPath $object.FullName -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pin[0].sha256){throw 'main80 overlay object SHA differs'}
}
$pins=[ordered]@{}
$pinnedSources=@('apps/pas/main.cpp',$manualSource,'include/pas/runtime.hpp','include/pas/manual_score_budget.hpp')
if($Variant -eq 'lazy'){$pinnedSources+='include/pas/manual_score_diagnostics.hpp'}
foreach($file in $pinnedSources){$pins[$file]=(Get-FileHash -LiteralPath (Join-Path $repo $file) -Algorithm SHA256).Hash.ToLowerInvariant()}
$head=(& git -C $repo rev-parse HEAD).Trim()
$hashJson=$pins|ConvertTo-Json -Compress
$header="#pragma once`ninline constexpr auto pas_session_build_commit=`"$head`";`ninline constexpr bool pas_session_build_dirty=true;`ninline constexpr auto pas_session_source_hashes=R`"json($hashJson)json`";`n"
[IO.File]::WriteAllText((Join-Path $output 'session_build_provenance.hpp'),$header)
$tool=Join-Path $repo 'out/windows-handoff/tool-main05-01/bin'
$includes=@($output,(Join-Path $repo 'include'),$repo,(Join-Path $repo 'research/prelive_current'),(Join-Path $repo 'research/bvi_cold_v3'),(Join-Path $repo 'research/hold_front_current'),(Join-Path $repo 'out/prelive-pre-emulator-release-main55/generated'),(Join-Path $repo 'out/vcpkg_installed/x64-windows/include'))|ForEach-Object {'/I"'+$_+'"'}
$objects=@()
foreach($source in @('apps/pas/main.cpp',$manualSource)){
 $object=Join-Path $output (([IO.Path]::GetFileNameWithoutExtension($source))+'.obj')
 $rsp=Join-Path $output (([IO.Path]::GetFileNameWithoutExtension($source))+'.compile.rsp')
 $compilerArgs=@('/nologo','/c','/std:c++20','/MD','/O2','/Ob2','/DNDEBUG','/W4','/permissive-','/EHsc')+$includes+@(('/Fo"'+$object+'"'),('"'+(Join-Path $repo $source)+'"'))
 [IO.File]::WriteAllLines($rsp,$compilerArgs)
 $compilerOutput=& (Join-Path $tool 'cl.exe') ('@'+$rsp) 2>&1
 $compilerExit=$LASTEXITCODE
 [IO.File]::WriteAllLines((Join-Path $output (([IO.Path]::GetFileNameWithoutExtension($source))+'.compile.log')),[string[]]$compilerOutput)
 if($compilerExit -ne 0){$compilerOutput|Select-Object -Last 8|Write-Output;throw 'entry compilation failed'}
 Write-Output ('Compiled '+$source)
 $objects+=$object
}
$exe=Join-Path $output 'pas.exe'
$link=@('/nologo','/INCREMENTAL:NO','/STACK:8388608',('/OUT:"'+$exe+'"'))+@($objects+$motion[0].FullName+$admission[0].FullName|ForEach-Object {'"'+$_+'"'})+@($libraries)
$rsp=Join-Path $output 'link.rsp'
[IO.File]::WriteAllLines($rsp,$link)
& (Join-Path $tool 'link.exe') ('@'+$rsp)
if($LASTEXITCODE -ne 0){throw 'entry link failed'}
foreach($dll in Get-ChildItem -LiteralPath $base -Filter '*.dll' -File){Copy-Item -LiteralPath $dll.FullName -Destination $output}
$report=[ordered]@{schema='pas.main80-manual-score-entry.v1';variant=$Variant;manual_source=$manualSource;base_binary_source=$binding.source_commit;entry_source_head=$head;entry_source_dirty=$true;modified_source_sha256=$pins;fresh_translation_units=2;reused_main80_overlay_objects=@($motion[0].FullName,$admission[0].FullName);exe_path=$exe;exe_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant();gameplay_persistence='none';cold_readiness_rerun=$false;cost_qualification_issued=$false;live_qualification_issued=$false}
[IO.File]::WriteAllText((Join-Path $output 'build.json'),($report|ConvertTo-Json -Depth 6))
Write-Output ('Built '+$exe)
