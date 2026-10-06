@echo off
echo TRACE native START
"C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\hold-front-release-03\front_audit.exe" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\pixels-withdraw-release-01.json.rows.jsonl" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\hold-front-package-01\negative-epoch.rows.jsonl" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\real-pixels-selection-02\selection.json" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\docs\research\zero-miss-20261006\windows\HUMAN_REVIEW_20261006.json" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\hold-front-package-01\audit-release-neg-epoch.json"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
