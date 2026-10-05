param([ValidateSet('36','50')][string]$Role)
. "$PSScriptRoot/x2-budget.ps1"
$source=if($Role -eq '36'){Join-Path $script:X2Repo 'out/x1/c36h-v3'}else{Join-Path $script:X2Repo 'out/x2/main50-winner-only'}
$build=Join-Path $script:X2Repo "out/x2/build$Role"
if(Test-Path -LiteralPath $build){throw 'Build root must be new'}
$args=@('-S',"$script:X2Repo/apps/frame_review/offline",'-B',$build,'-G','Visual Studio 18 2026','-A','x64','-T','v145',
 '-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275',
 "-DX1_SOURCE=$source","-DX1_REPO=$script:X2Repo","-DCMAKE_PREFIX_PATH=$script:X2Repo/out/vcpkg_installed/x64-windows",'-DX2_OFFLINE=ON',
 '-DX1_SANITIZER_SUPPORT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')
if($Role -eq '50'){$args+='-DX2_MAIN50=ON'}
$code=X2-Invoke "configure$Role" 'cmake' $args;if($code){throw 'configure failed'}
$code=X2-Invoke "build$Role" 'cmake' @('--build',$build,'--config','Release','--parallel','2');if($code){throw 'build failed'}
Copy-Item -LiteralPath "$script:X2Repo/out/vcpkg_installed/x64-windows/bin/z.dll","$script:X2Repo/out/vcpkg_installed/x64-windows/bin/gtest.dll","$script:X2Repo/out/vcpkg_installed/x64-windows/bin/gtest_main.dll" -Destination "$build/Release"
Write-Output "X2 Release $Role built."
