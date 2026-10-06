@echo off
echo TRACE native START
"C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-current-debug-05\formal_affected.exe" "--gtest_output=json:C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-20261006\formal_affected-debug-05.json"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
