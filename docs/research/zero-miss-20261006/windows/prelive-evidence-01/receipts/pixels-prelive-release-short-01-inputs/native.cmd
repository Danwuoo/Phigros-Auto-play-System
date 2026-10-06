@echo off
echo TRACE native START
"C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-current-release-10\current_chain.exe" "pixels" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\real-pixels-selection-02\selection.json" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\prelive-20261006\pixels-release-short-01.json"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
