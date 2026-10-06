@echo off
echo TRACE native START
"C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\hold-front-release-controller-01\pixel_chain.exe" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\real-pixels-selection-02\selection.json" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\hold-front-package-01\pixels-release-controller-01.json"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
