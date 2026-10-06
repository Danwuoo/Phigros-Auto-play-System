@echo off
echo TRACE native START
"C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-current-asan-03\current_tests.exe" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-20261006\current_tests-asan-03.json"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
