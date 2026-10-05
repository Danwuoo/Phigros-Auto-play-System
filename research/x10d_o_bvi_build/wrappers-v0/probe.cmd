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
echo TRACE L11 argv-configure START
"C:\Users\wurre\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe" -NoProfile -File "C:\Users\wurre\Desktop\Phigros-Auto-play-System\research\x10d_o_bvi_build\argv-fixture.ps1" -Mode configure -S "C:\Users\wurre\Desktop\Phigros-Auto-play-System\research\x10d_o_bvi_build" -B "C:\Users\wurre\Desktop\Phigros-Auto-play-System\out\x10d-o-bvi-build\release" -G Ninja -BuildType Release -Asan OFF -MakeProgram "C:\Users\wurre\AppData\Local\Programs\Python\Python310\Scripts\ninja.exe"
set "BVI_EXIT=%errorlevel%"
echo TRACE L12 argv-configure END exit=%BVI_EXIT%
if not "%BVI_EXIT%"=="0" exit /b %BVI_EXIT%
echo TRACE L16 argv-build START
"C:\Users\wurre\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe" -NoProfile -File "C:\Users\wurre\Desktop\Phigros-Auto-play-System\research\x10d_o_bvi_build\argv-fixture.ps1" -Mode build -B "C:\Users\wurre\Desktop\Phigros-Auto-play-System\out\x10d-o-bvi-build\release" -Parallel 2 -ReturnCode 7
set "BVI_EXIT=%errorlevel%"
echo TRACE L17 argv-build END exit=%BVI_EXIT%
if not "%BVI_EXIT%"=="7" exit /b 1
where cl.exe
if errorlevel 1 exit /b 1
"C:/Users/wurre/AppData/Local/Programs/Python/Python310/Lib/site-packages/cmake/data/bin/cmake.exe" --version
if errorlevel 1 exit /b 1
"C:/Users/wurre/AppData/Local/Programs/Python/Python310/Scripts/ninja.exe" --version
if errorlevel 1 exit /b 1
echo TRACE DIAGNOSTIC_POSITIVE wrapper-v0
exit /b 0
