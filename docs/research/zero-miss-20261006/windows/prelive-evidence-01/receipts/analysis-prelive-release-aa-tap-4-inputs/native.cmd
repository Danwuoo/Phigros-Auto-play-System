@echo off
echo TRACE native START
"C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-current-release-13\cost_analysis.exe" "run" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-20261006\aa-tap-4-gate-input.json" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-20261006\analysis-release-aa-tap-4.json"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
