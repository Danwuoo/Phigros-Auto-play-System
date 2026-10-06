@echo off
echo TRACE Configure entry [%cmdcmdline%]
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.51 >nul
set "PAS_LOCAL_EXIT=%errorlevel%"
echo TRACE vcvars END exit=%PAS_LOCAL_EXIT%
if not "%PAS_LOCAL_EXIT%"=="0" exit /b %PAS_LOCAL_EXIT%
set "PATH=C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\msvc-isolated-probe-01\bin;%PATH%"
"C:\Users\wurre\AppData\Local\Programs\Python\Python310\Lib\site-packages\cmake\data\bin\cmake.exe" "-S" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\research\prelive_current" "-B" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-current-release-05" "-G" "Ninja" "-DCMAKE_MAKE_PROGRAM=C:\Users\wurre\AppData\Local\Programs\Python\Python310\Scripts\ninja.exe" "-DCMAKE_CXX_COMPILER=C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\msvc-isolated-probe-01\bin\cl.exe" "-DCMAKE_LINKER=C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\msvc-isolated-probe-01\bin\link.exe" "-DCMAKE_AR=C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\msvc-isolated-probe-01\bin\lib.exe" "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded" "-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" "-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed" "-DVCPKG_MANIFEST_INSTALL=OFF" "-DVCPKG_TARGET_TRIPLET=x64-windows"
set "PAS_LOCAL_EXIT=%errorlevel%"
echo TRACE Configure END exit=%PAS_LOCAL_EXIT%
"C:\Users\wurre\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe" -NoProfile -Command "[Threading.Thread]::Sleep(750)"
exit /b %PAS_LOCAL_EXIT%
