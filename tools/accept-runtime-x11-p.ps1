$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue"
$sourcePath="$campaignPath/runtime-x11-p"
$reviewPath="$campaignPath/runtime-x11-p-controller"
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$v){$p="$reviewPath/$name";if(Test-Path -LiteralPath $p){throw "existing $p"};[IO.File]::WriteAllText($p,($v|ConvertTo-Json -Depth 70)+"`n",[Text.UTF8Encoding]::new($false))}
function Run($name,$exe,[string[]]$arguments,[int]$expected){Save "$name-command.json" @{exe=$exe;arguments=$arguments;expected_exit=$expected};& $exe @arguments *> "$reviewPath/$name.log";$code=$LASTEXITCODE;Save "$name-exit.json" @{exit=$code;log_sha256=(Hash "$reviewPath/$name.log")};if($code -ne $expected){throw "$name exit $code expected $expected"};Write-Output "$name exit=$code"}
if(Test-Path -LiteralPath $reviewPath){throw 'new review root required'}
if((Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")+32MB -gt 8GB){throw 'reserve'}
New-Item -ItemType Directory -Path $reviewPath|Out-Null
Save 'workspace-before.json' @{head=(git rev-parse HEAD);branch=(git branch --show-current);worktrees=@(git worktree list);status=@(git status --short);formal_diff=@(git diff HEAD -- src include CMakeLists.txt)}
$checks=@()
foreach($name in @('source-binding-before-cost.json','compiled-dependency-freeze.json','artifact-ledger.json')){
 $j=Get-Content -LiteralPath "$sourcePath/$name" -Raw|ConvertFrom-Json
 foreach($f in $j.files){if((Hash $f.path) -ne $f.sha256 -or (Get-Item -LiteralPath $f.path).Length -ne $f.bytes){throw "binding $($f.path)"}}
 $checks+=@{name=$name;sha256=(Hash "$sourcePath/$name");verified=$j.files.Count}
 Write-Output "$name verified=$($j.files.Count)"
}
$original=Get-Content "$sourcePath/final-summary.json" -Raw|ConvertFrom-Json
if((Hash "$sourcePath/artifact-ledger.json") -ne $original.ledger_sha256){throw 'ledger root'}
if((Hash $original.original_live_binary.path) -ne $original.original_live_binary.sha256){throw 'original binary'}
Save 'bindings.json' @{checks=$checks;original_summary_sha256=(Hash "$sourcePath/final-summary.json");original_live_binary=$original.original_live_binary}
$coreFiles=0
$hook=@'
        // A newer complete snapshot with no current Note must withdraw a
        // pending Down immediately. Missing grace applies only after a
        // contact has started; it cannot license a new touch from old pixels.
        if(identity.submitted) if(const auto cursor=scheduler_.executed_steps(identity.intent);
           cursor&&*cursor==0) {
            cancel_contact(id,identity,"pending_down_current_object_missing");
            continue;
        }
'@
$hook=$hook.Replace("`r`n","`n")+"`n"
foreach($f in Get-ChildItem "$repoPath/out/x11-p/B0/src","$repoPath/out/x11-p/B0/include" -File -Recurse){
 $other=$f.FullName.Replace('\B0\','\B1\');$a=[IO.File]::ReadAllText($f.FullName).Replace("`r`n","`n");$b=[IO.File]::ReadAllText($other).Replace("`r`n","`n")
 if($f.Name -eq 'game.cpp'){if(!$b.Contains($hook)){throw 'missing hook'};$b=$b.Replace($hook,'')}
 if($a -cne $b){throw "source inverse $other"};++$coreFiles
}
Save 'source-inverse.json' @{files=$coreFiles;only_decision_delta='pending cursor zero missing hook';formal_diff=@(git diff HEAD -- src include CMakeLists.txt)}
foreach($d in Get-ChildItem -LiteralPath $sourcePath -Directory|Where-Object Name -Match '^(aa-|ab-|stress-)'){
 New-Item -ItemType Directory -Path "$reviewPath/$($d.Name)"|Out-Null
 Copy-Item -LiteralPath "$($d.FullName)/summary.json" -Destination "$reviewPath/$($d.Name)/summary.json"
}
$gate="$repoPath/out/x11-p/aux/Release/x11_gate.exe"
Run 'noise' $gate @('noise',$reviewPath) 2
Run 'evaluate' $gate @('evaluate',$reviewPath) 2
foreach($f in @('noise-frozen.json','cost-evaluation.json')){if((Hash "$reviewPath/$f") -ne (Hash "$sourcePath/$f")){throw "gate mismatch $f"}}
$env:PAS_RGB_CLIP_ROOT="$campaignPath/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
$testResults=@()
foreach($role in @('B0','B1')){
 $build="$repoPath/out/x11-p/build-$role/Release"
 Run "provenance-$role" "$repoPath/out/x11-p/runtime-$role/pas.exe" @('x11-provenance') 0
 $prov=Get-Content "$reviewPath/provenance-$role.log" -Raw|ConvertFrom-Json
 foreach($p in $prov.compiled_source_sha256.PSObject.Properties){if((Hash "$repoPath/out/x11-p/$role/$($p.Name)") -ne $p.Value){throw 'provenance'}}
 foreach($suite in @('original','pending')){
  $exe=if($suite -eq 'original'){'pas_tests.exe'}else{'x11_pending_tests.exe'}
  $expected=if(($role -eq 'B1' -and $suite -eq 'original') -or ($role -eq 'B0' -and $suite -eq 'pending')){1}else{0}
  $name="tests-$role-$suite"
  Run $name "$build/$exe" @('--gtest_brief=1',"--gtest_output=xml:$reviewPath/$name.xml") $expected
  [xml]$xml=Get-Content "$reviewPath/$name.xml" -Raw
  $cases=@($xml.testsuites.testsuite.testcase)
  $failed=@($cases|Where-Object {$_.failure}|ForEach-Object {"$($_.classname).$($_.name)"})
  $skipped=@($cases|Where-Object {$_.skipped -or $_.result -eq 'skipped'})
  $old=[xml](Get-Content "$sourcePath/$name.xml" -Raw)
  $expectedFailed=@($old.testsuites.testsuite.testcase|Where-Object {$_.failure}|ForEach-Object {"$($_.classname).$($_.name)"})
  if(($failed -join '|') -cne ($expectedFailed -join '|') -or $skipped.Count -ne 0 -or [int]$xml.testsuites.errors -ne 0 -or [int]$xml.testsuites.disabled -ne 0){throw "unexpected tests $name"}
  $testResults+=@{name=$name;tests=$cases.Count;failed=$failed;skipped=$skipped.Count;xml_sha256=(Hash "$reviewPath/$name.xml")}
 }
 Run "bridge-$role" "$build/x11_cost.exe" @('bridge',"$reviewPath/bridge-$role") 0
 if((Hash "$reviewPath/bridge-$role/public-events.jsonl") -ne (Hash "$sourcePath/bridge-$role/public-events.jsonl")){throw 'bridge mismatch'}
}
Save 'verification.json' @{tests=$testResults;gate_outputs_byte_equal=$true;bridge_outputs_byte_equal=$true;cost_runs_added=0;full_replays_added=0;live=0;state='not-ready';source_inverse_files=$coreFiles;script_sha256=(Hash $PSCommandPath)}
Copy-Item -LiteralPath $PSCommandPath -Destination "$reviewPath/accept-runtime-x11-p.ps1"
$files=@(Get-ChildItem -LiteralPath $reviewPath -File -Recurse|ForEach-Object {@{path=$_.FullName;bytes=$_.Length;sha256=(Hash $_.FullName)}})
Save 'artifact-ledger.json' @{files=$files}
$reviewBytes=Bytes $reviewPath;$total=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")
if($reviewBytes+16KB -gt 32MB -or $total+16KB -gt 8GB){throw 'capacity'}
Save 'final-summary.json' @{state='not-ready';acceptance='cold engineering and negative cost evidence accepted';ledger_sha256=(Hash "$reviewPath/artifact-ledger.json");bytes_before_final_summary=$reviewBytes;campaign_plus_prior_before_final_summary=$total;batch_limit=32MB;campaign_limit=8GB;head=(git rev-parse HEAD);formal_source_unchanged=(@(git diff HEAD -- src include CMakeLists.txt).Count -eq 0);cost_runs_added=0;stress_runs_added=0;full_replays_added=0;emulator_started=$false}
Write-Output "accepted negative result; controller bytes before summary=$reviewBytes campaign=$total"
