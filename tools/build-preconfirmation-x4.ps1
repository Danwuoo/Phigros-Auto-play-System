. "$PSScriptRoot/x4-budget.ps1"
$build=Join-Path $script:X4Repo 'out/x4/build'
if(Test-Path -LiteralPath $build){throw 'Build must be new'}
$args=@('-S',"$script:X4Repo/apps/frame_review/offline",'-B',$build,'-G','Visual Studio 18 2026','-A','x64','-T','v145','-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275',"-DX1_SOURCE=$script:X4Repo/out/x4/main50-provisional","-DX1_REPO=$script:X4Repo","-DCMAKE_PREFIX_PATH=$script:X4Repo/out/vcpkg_installed/x64-windows",'-DX4_OFFLINE=ON','-DX1_SANITIZER_SUPPORT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')
$code=X4-Invoke 'configure' 'cmake' $args;if($code){throw 'configure failed'}
$code=X4-Invoke 'build-release-1' 'cmake' @('--build',$build,'--config','Release','--parallel','2');if($code){throw 'build failed'}
Copy-Item -LiteralPath "$script:X4Repo/out/vcpkg_installed/x64-windows/bin/z.dll","$script:X4Repo/out/vcpkg_installed/x64-windows/bin/gtest.dll","$script:X4Repo/out/vcpkg_installed/x64-windows/bin/gtest_main.dll" -Destination "$build/Release"
