@echo off
echo TRACE Build entry [%cmdcmdline%]
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.51 >nul
set "PAS_LOCAL_EXIT=%errorlevel%"
echo TRACE vcvars END exit=%PAS_LOCAL_EXIT%
if not "%PAS_LOCAL_EXIT%"=="0" exit /b %PAS_LOCAL_EXIT%
set "PATH=C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\msvc-isolated-probe-01\bin;%PATH%"
"C:\Users\wurre\AppData\Local\Programs\Python\Python310\Lib\site-packages\cmake\data\bin\cmake.exe" "--build" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-current-release-05" "--parallel" "4"
set "PAS_LOCAL_EXIT=%errorlevel%"
echo TRACE Build END exit=%PAS_LOCAL_EXIT%
"C:\Users\wurre\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe" -NoProfile -Command "[Threading.Thread]::Sleep(750)"
exit /b %PAS_LOCAL_EXIT%
