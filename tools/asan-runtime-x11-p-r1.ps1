param([ValidateSet('Build','Tests')][string]$Mode)
$ErrorActionPreference='Stop';$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$outPath="$repoPath/out/x11-p-r1";$batchPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-r1";$buildPath="$outPath/build-asan"
function Run($name,$exe,[string[]]$a){$p="$batchPath/$name-command.json";if(Test-Path $p){throw 'existing command'}
 [IO.File]::WriteAllText($p,(@{exe=$exe;arguments=$a}|ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
 & $exe @a *> "$batchPath/$name.log";$code=$LASTEXITCODE
 [IO.File]::WriteAllText("$batchPath/$name-exit.json",(@{exit=$code;log_sha256=(Get-FileHash "$batchPath/$name.log").Hash.ToLowerInvariant()}|ConvertTo-Json),[Text.UTF8Encoding]::new($false));Write-Output "$name exit=$code";return $code}
if($Mode -eq 'Build') {
 if(Test-Path "$batchPath/source-binding-before-cost.json"){throw 'freeze prohibits rebuild'}
 $options=@('-S',"$outPath/B1",'-B',$buildPath,'-G','Visual Studio 18 2026','-A','x64','-T','v145',
 '-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275',
 '-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake','-DVCPKG_MANIFEST_MODE=OFF',
 "-DVCPKG_INSTALLED_DIR=$repoPath/out/vcpkg_installed",'-DVCPKG_TARGET_TRIPLET=x64-windows',"-DX11_REPO=$repoPath",'-DR1_ASAN=ON',
 '-DPAS_ASAN_SUPPORT_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')
 $c=Run 'configure-asan' cmake $options;if($c[-1]){throw 'configure'}
 $c=Run 'build-asan' cmake @('--build',$buildPath,'--config','Debug','--parallel','2','--target','x11_cost','x11_r1_tests','x11_pending_tests');if($c[-1]){throw 'build'}
 Copy-Item "$repoPath/out/vcpkg_installed/x64-windows/debug/bin/zd.dll" "$buildPath/Debug"
}else{
 $env:R1_TEST_ROOT="$batchPath/test-artifacts-asan"
 Run 'tests-asan-r1' "$buildPath/Debug/x11_r1_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/tests-asan-r1.xml")
 Run 'tests-asan-pending' "$buildPath/Debug/x11_pending_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/tests-asan-pending.xml")
}
