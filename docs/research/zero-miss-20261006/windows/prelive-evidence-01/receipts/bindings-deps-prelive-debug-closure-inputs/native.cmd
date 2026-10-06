@echo off
echo TRACE native START
"C:\Users\wurre\AppData\Local\Programs\Python\Python310\Scripts\ninja.exe" "-C" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-current-debug-05" "-t" "deps"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
