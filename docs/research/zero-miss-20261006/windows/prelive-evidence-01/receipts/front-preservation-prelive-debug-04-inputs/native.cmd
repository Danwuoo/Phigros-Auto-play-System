@echo off
echo TRACE native START
"C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-current-debug-04\front_preservation.exe" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-20261006\front_preservation-debug-04.json"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
