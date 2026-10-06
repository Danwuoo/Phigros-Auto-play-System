@echo off
echo TRACE native START
"C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\hold-front-release-controller-01\prefix_preservation.exe" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\hold-front-package-01\prefix-release-controller-01.json"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
