@echo off
echo TRACE native START
"C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\hold-front-release-03\front_tests.exe" "C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006\out\windows-handoff\hold-front-package-01\front-release-03.json"
set "PAS_NATIVE_EXIT=%errorlevel%"
echo TRACE native END exit=%PAS_NATIVE_EXIT%
exit /b %PAS_NATIVE_EXIT%
