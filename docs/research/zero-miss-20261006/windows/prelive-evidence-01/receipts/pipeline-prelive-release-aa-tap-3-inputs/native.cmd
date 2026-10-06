@echo off
echo TRACE native START
"C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-current-release-13\current_chain.exe" "pipeline" "A" "tap" "2560" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-20261006\pipeline-release-aa-tap-3.json"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
