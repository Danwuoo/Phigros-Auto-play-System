param([string]$ReviewRoot)
. "$PSScriptRoot/r3-common.ps1"
. "$repoPath/tools/collect-runtime-x11-p-r1-tests.ps1"
if(!$ReviewRoot){$ReviewRoot="$batchPath/controller-review"}
if(Test-Path -LiteralPath $ReviewRoot){throw 'new independent review root required'}
if((R3Bytes $batchPath)+32MB -gt 2GB){throw 'review reserve'}
New-Item -ItemType Directory $ReviewRoot | Out-Null
function ReviewRun($name,$exe,[string[]]$arguments){$p="$ReviewRoot/$name-command.json";[IO.File]::WriteAllText($p,(@{exe=$exe;arguments=$arguments}|ConvertTo-Json -Depth 8)+"`n");& $exe @arguments *> "$ReviewRoot/$name.log";$code=$LASTEXITCODE;[IO.File]::WriteAllText("$ReviewRoot/$name-exit.json",(@{exit=$code;log_sha256=(R3Hash "$ReviewRoot/$name.log")}|ConvertTo-Json)+"`n");if($code -ne 0){throw "review $name exit=$code"}}
$final=Get-Content "$batchPath/final-summary.json" -Raw|ConvertFrom-Json
if((R3Hash "$batchPath/artifact-ledger.json") -ne $final.artifact_ledger_sha256){throw 'ledger root'}
$ledger=Get-Content "$batchPath/artifact-ledger.json" -Raw|ConvertFrom-Json
foreach($f in $ledger.files){if((R3Hash $f.path) -ne $f.sha256 -or (Get-Item -LiteralPath $f.path).Length -ne $f.physical_file_bytes){throw "R3 ledger $($f.path)"}}
foreach($name in @('source-binding-before-cost.json','compiled-dependency-freeze.json')){$j=Get-Content "$batchPath/$name" -Raw|ConvertFrom-Json;foreach($f in $j.files){if((R3Hash $f.path) -ne $f.sha256 -or (Get-Item -LiteralPath $f.path).Length -ne $f.bytes){throw "binding $($f.path)"}}}
$tests=@();foreach($role in @('B0','B1','asan')){$config=if($role -eq 'asan'){'Debug'}else{'Release'};$env:R3_TEST_ROOT="$ReviewRoot/fixtures-$role";$filter=if($role -eq 'asan'){'*'}else{'-R3.ArchiveExactly65536SamplesFullRows:R3.ArchiveAbove65536FailsWithoutSampling'}
 ReviewRun "tests-$role" "$outPath/build-$role/$config/r3_tests.exe" @("--gtest_filter=$filter",'--gtest_brief=1',"--gtest_output=xml:$ReviewRoot/tests-$role.xml")
 $t=Get-R1TestResult "$ReviewRoot/tests-$role.xml";if($t.failures -or $t.errors -or $t.skipped -or $t.disabled -or $t.tests -ne $(if($role -eq 'asan'){10}else{8})){throw 'review test count'};$tests+=@{role=$role;result=$t}
}
$q=Get-Content "$batchPath/qualification.json" -Raw|ConvertFrom-Json
foreach($r in $q.runs){ReviewRun "$($r.name)-audit" "$outPath/build-B0/Release/r3_audit.exe" @("$batchPath/$($r.name)","$ReviewRoot/$($r.name)-audit.json");if((R3Hash "$ReviewRoot/$($r.name)-audit.json") -ne (R3Hash "$batchPath/$($r.name)-audit.json")){throw 'raw audit bytes'}}
if((R3Bytes $ReviewRoot) -gt 32MB){throw 'review capacity'}
[IO.File]::WriteAllText("$ReviewRoot/verification.json",(@{tests=$tests;raw_audit_byte_identical=$true;new_cost=0;new_stress=0;new_build=0;ledger_entries=$ledger.files.Count;development_original_XML_checked_only=$final.tests.historical_XML_verified_only;state=$final.state;independent_controller_decision='this script does not self-sign acceptance';review_file_bytes_before_verification=(R3Bytes $ReviewRoot)}|ConvertTo-Json -Depth 80)+"`n",[Text.UTF8Encoding]::new($false))
Write-Output 'independent checks completed; controller must interpret gates; no new cost or live'
