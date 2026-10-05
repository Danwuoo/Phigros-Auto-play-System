$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$batchPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi'
$outPath=Join-Path $repoPath 'out/x10d-o-bvi'
$freezePath=Join-Path $batchPath 'freeze'
if(Test-Path -LiteralPath $freezePath){throw 'freeze exists'}
$timer=[Diagnostics.Stopwatch]::StartNew()
function Bound{if($timer.Elapsed.TotalSeconds -gt 180){throw 'freeze time bound'}}
function Entry($p,$mode='fingerprint only'){Bound;$f=Get-Item -LiteralPath $p;@{path=$f.FullName;bytes=$f.Length;sha256=(Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant();mode=$mode}}
[IO.Directory]::CreateDirectory("$freezePath/source")|Out-Null
Copy-Item -LiteralPath $PSScriptRoot -Destination "$freezePath/source/research" -Recurse
[IO.Directory]::CreateDirectory("$freezePath/docs")|Out-Null
foreach($f in Get-ChildItem -LiteralPath "$repoPath/docs" -File -Filter 'HOLD_OWNERSHIP_X10D_O_BVI*'){Copy-Item -LiteralPath $f.FullName -Destination "$freezePath/docs/$($f.Name)"}
Copy-Item -LiteralPath $outPath -Destination "$freezePath/configure" -Recurse
[IO.Directory]::CreateDirectory("$freezePath/dependencies/include")|Out-Null
Copy-Item -LiteralPath "$repoPath/out/vcpkg_installed/x64-windows/include/nlohmann" -Destination "$freezePath/dependencies/include/nlohmann" -Recurse
$copyright="$repoPath/out/vcpkg_installed/x64-windows/share/nlohmann_json/copyright"
if(Test-Path -LiteralPath $copyright){Copy-Item -LiteralPath $copyright -Destination "$freezePath/dependencies/nlohmann-copyright.txt"}
$deps=[Collections.Generic.List[object]]::new()
foreach($f in Get-ChildItem -LiteralPath "$repoPath/out/vcpkg_installed/x64-windows/include/nlohmann" -File -Recurse){$deps.Add((Entry $f.FullName 'copied header kit; not compiled by candidate'))}
$toolRoot='C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717'
foreach($p in @((Get-Command cmake.exe).Source,(Get-Command ninja.exe).Source,"$repoPath/out/x10d-o-bvi/release/CMakeCache.txt",'C:/Windows/System32/cmd.exe',(Get-Process -Id $PID).Path,"$toolRoot/bin/Hostx64/x64/cl.exe","$toolRoot/bin/Hostx64/x64/c1xx.dll","$toolRoot/bin/Hostx64/x64/link.exe","$toolRoot/lib/x64/libcmt.lib","$toolRoot/lib/x64/libcpmt.lib","$toolRoot/lib/x64/libvcruntime.lib",'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat')){if(Test-Path -LiteralPath $p){$deps.Add((Entry $p 'installed tool/CRT fingerprint; configure only, no candidate compilation'))}}
$cache=Get-Content -LiteralPath "$outPath/release/CMakeCache.txt"
foreach($row in $cache){if($row -match '^(CMAKE_COMMAND|CMAKE_LINKER|CMAKE_RC_COMPILER|CMAKE_MT):[^=]+=(.+)$'){$p=$Matches[2];if(Test-Path -LiteralPath $p){$deps.Add((Entry $p 'actual configured tool path fingerprint'))}}}
$v=@{schema=1;status='platform_partial_no_candidate_binary';dependencies=@($deps.ToArray());candidate_compiled_inputs=@();candidate_dlls=@();configure_artifacts=@(Get-ChildItem -LiteralPath $outPath -File -Recurse|ForEach-Object{Entry $_.FullName 'failed-quiescence/unverified configure artifact'});coverage_limits=@('No candidate compile occurred; no compiled source/dependency closure can be asserted','CompilerId/ABI binaries are configure probes, not BVI candidate','Copied nlohmann header kit is available build input, not evidence it was compiled','MSVC/SDK/STL/CRT and system DLL full closure not copied or verified','Runner Add-Type compiler/runtime assembly closure and individual descendant PIDs/images not enumerated; ownership is anonymous job membership accounting','ASan DLL/provider and Debug were not configured or executed');reproduction='frozen standalone source and input kit for independently authorized future reproduction; installed pinned toolchain still required'}
[IO.File]::WriteAllText("$batchPath/dependency-manifest.json",($v|ConvertTo-Json -Depth 20)+"`n",[Text.UTF8Encoding]::new($false))
$status=@{schema=1;status='platform_partial';stop='configure-release descendants-nonquiescent';candidate_builds=0;candidate_binaries=0;candidate_tests='not_run';top_vectors=20;expanded_cases=69;planned_case_layer_instances=207;supplemental_cases=22;rgb='not_run';typed='not_run';fake_lifecycle='not_run';debug='not_run';asan='not_run';png_audit_attempts=0;png_frames=0;full_replay=0;runtime_cost_stress=0;live=0;tool_engineering_repairs=0;oracle_changed=$false;family_count=1;algorithm_verdict='unverified, no executed core counterexample';source_oracle='synthetic declaration, human physical gold0';baseline_adoption=$false;next_dispatch=$false;cleanup_active_final=0;protected_original_git_paths=454}
[IO.File]::WriteAllText("$batchPath/delivery-status.json",($status|ConvertTo-Json -Depth 10)+"`n",[Text.UTF8Encoding]::new($false))
Write-Output "freeze complete: source/contracts/configure probes/header kit, candidate binary0"
