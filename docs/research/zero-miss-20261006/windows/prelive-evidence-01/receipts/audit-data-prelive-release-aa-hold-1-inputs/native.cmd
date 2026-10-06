@echo off
echo TRACE native START
"C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-audit-release-01\prelive_audit.exe" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-20261006\pipeline-release-aa-hold-1.json" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-20261006\audit-aa-hold-1.json"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
