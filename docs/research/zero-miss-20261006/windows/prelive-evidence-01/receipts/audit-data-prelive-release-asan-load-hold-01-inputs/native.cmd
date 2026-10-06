@echo off
echo TRACE native START
"C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-audit-release-01\prelive_audit.exe" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-20261006\pipeline-asan-load-hold-01.json" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-20261006\audit-asan-load-hold-01.json"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
