@echo off
echo TRACE wrapper-v0 entry cmdcmdline=[%cmdcmdline%]
set VSCMD_SKIP_SENDTELEMETRY=1
set VC_DISABLE_SQM=1
set VS_UNICODE_OUTPUT=
echo TRACE L06 vcvars START
call "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" -vcvars_ver=14.50 >nul
set "BVI_EXIT=%errorlevel%"
echo TRACE L07 vcvars END exit=%BVI_EXIT%
if not "%BVI_EXIT%"=="0" exit /b %BVI_EXIT%
echo TRACE CMAKE configure-debug ENTRY
"C:/Users/wurre/AppData/Local/Programs/Python/Python310/Lib/site-packages/cmake/data/bin/cmake.exe" -S "C:\Users\wurre\Desktop\Phigros-Auto-play-System\research\x10d_o_bvi_build" -B "C:\Users\wurre\Desktop\Phigros-Auto-play-System\out\x10d-o-bvi-build\debug" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBVI_ASAN=OFF "-DCMAKE_MAKE_PROGRAM=C:/Users/wurre/AppData/Local/Programs/Python/Python310/Scripts/ninja.exe"
set "BVI_EXIT=%errorlevel%"
echo TRACE CMAKE configure-debug END exit=%BVI_EXIT%
exit /b %BVI_EXIT%
