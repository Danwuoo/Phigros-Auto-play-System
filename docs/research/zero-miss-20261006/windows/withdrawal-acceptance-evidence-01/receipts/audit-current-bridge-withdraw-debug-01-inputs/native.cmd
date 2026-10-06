@echo off
echo TRACE native START
"C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\bridge-win-withdraw-debug-01\pixel_diagnostics_audit.exe" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\pixels-profile-release-01.json.rows.jsonl" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\pixels-withdraw-debug-01.json.rows.jsonl" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\real-pixels-selection-02\selection.json" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\audit-current-withdraw-debug-01.json"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
