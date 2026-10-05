@echo off
set VSCMD_SKIP_SENDTELEMETRY=1
set VC_DISABLE_SQM=1
set VS_UNICODE_OUTPUT=
call "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" -vcvars_ver=14.50 >nul
if errorlevel 1 exit /b 1
"C:/Users/wurre/AppData/Local/Programs/Python/Python310/Lib/site-packages/cmake/data/bin/cmake.exe" -S "C:\Users\wurre\Desktop\Phigros-Auto-play-System\research\x10d_o_bvi_r2f_resume" -B "C:\Users\wurre\Desktop\Phigros-Auto-play-System\out\x10d-o-bvi-r2f-resume/release" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBVI_ASAN=OFF "-DCMAKE_MAKE_PROGRAM=C:/Users/wurre/AppData/Local/Programs/Python/Python310/Scripts/ninja.exe"
exit /b %errorlevel%
