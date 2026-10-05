param([string]$ReviewRoot)
$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$batchPath="$repoPath/measurements/runtime-cost-x11-p-r3"
if(!$ReviewRoot){$ReviewRoot="$batchPath/controller-review"}
if(!(Test-Path -LiteralPath "$ReviewRoot/verification.json")){throw 'Run the bounded acceptance checks first'}
if(Test-Path -LiteralPath "$ReviewRoot/controller-crosscheck.json"){throw 'Existing review; do not overwrite'}
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$value){$p="$ReviewRoot/$name";if(Test-Path -LiteralPath $p){throw "Existing $p"};[IO.File]::WriteAllText($p,($value|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))}
# Independent maintenance cross-check of the recorded denominator and quantile claims.
# This does not change the C++ analysis, the frozen protocol, or any cost sample.
function CheckMetric($expected,[double[]]$values){
 if($expected.n -ne $values.Count -or !$values.Count){throw 'metric count'}
 [Array]::Sort($values)
 foreach($pair in @(@('p50',.5),@('p95',.95),@('p99',.99),@('max',1.0))){
  $actual=$values[[int][Math]::Ceiling([double]$pair[1]*$values.Count)-1]
  if([Math]::Abs($actual-[double]$expected.($pair[0])) -gt 1e-8){throw "quantile $($pair[0])"}
 }
 $jitter=$values[[int][Math]::Ceiling(.95*$values.Count)-1]-$values[[int][Math]::Ceiling(.05*$values.Count)-1]
 if([Math]::Abs($jitter-[double]$expected.jitter_p95_minus_p5) -gt 1e-8){throw 'jitter'}
}
$qualification=Get-Content "$batchPath/qualification.json" -Raw|ConvertFrom-Json
if($qualification.AA_runs -ne 5 -or $qualification.ABBA_runs -ne 0 -or $qualification.stress_runs -ne 0){throw 'Unexpected campaign scope'}
$expectedNames=@('aa-rgb-1','aa-rgb-2','aa-rgb-3','aa-rgb-4','aa-owner-1')
if(($qualification.runs.name -join '|') -cne ($expectedNames -join '|')){throw 'Run order'}
$actualDirs=@(Get-ChildItem -LiteralPath $batchPath -Directory | Where-Object Name -Match '^(aa-|ab-|stress-)')
if((($actualDirs.Name|Sort-Object) -join '|') -cne (($expectedNames|Sort-Object) -join '|')){throw 'Unexpected run directory'}
foreach($p in @('noise-frozen.json','cost-evaluation.json')){if(Test-Path -LiteralPath "$batchPath/$p"){throw 'Unexpected comparison output'}}
$reports=@()
foreach($r in $qualification.runs){
 $root="$batchPath/$($r.name)";$s=Get-Content "$root/summary.json" -Raw|ConvertFrom-Json
 $frames=@([IO.File]::ReadLines("$root/frames.jsonl")|ForEach-Object {$_|ConvertFrom-Json})
 $published=@($frames|Where-Object published);$consumed=@($frames|Where-Object consumed);$owned=@($frames|Where-Object owned)
 if($frames.Count -ne 2560 -or $consumed.Count -ne $s.consumed -or $owned.Count -ne $s.owner_seen -or $published.Count -ne $s.published){throw 'frame counts'}
 for($i=0;$i -lt $frames.Count;$i++){if($frames[$i].attempt -ne $i+1){throw 'attempt sequence'}}
 $mw=@($frames|Where-Object attempt -gt 256);$mc=@($mw|Where-Object consumed);$mo=@($mw|Where-Object owned)
 foreach($phase in @('all','measurement')){
  $c=if($phase -eq 'all'){$consumed}else{$mc};$o=if($phase -eq 'all'){$owned}else{$mo}
  $metrics=$s.windows.$phase.metrics_ms
  CheckMetric $metrics.recognition @($c|ForEach-Object {($_.recognition_complete_ns-$_.recognition_start_ns)/1e6})
  CheckMetric $metrics.owner @($o|ForEach-Object {($_.owner_end_ns-$_.owner_start_ns)/1e6})
  CheckMetric $metrics.capture_to_owner @($o|ForEach-Object {($_.owner_end_ns-$_.capture_complete_ns)/1e6})
 }
 $receipts=@([IO.File]::ReadLines("$root/receipts.jsonl")|ForEach-Object {$_|ConvertFrom-Json})
 CheckMetric $s.metrics_ms.lateness @($receipts|ForEach-Object {($_.injection_start_ns-$_.scheduled_ns)/1e6})
 if($receipts.Count -ne $s.receipt_n){throw 'receipt count'}
 $completion=Get-Content "$root/archive/round-1/summary.json" -Raw|ConvertFrom-Json
 foreach($seg in $completion.event_segments){if((Hash "$root/archive/round-1/$($seg.path)") -ne $seg.sha256){throw 'segment digest'}}
 $full90=$consumed.Count*10 -ge 2560*9 -and $owned.Count*10 -ge 2560*9
 $expectedPass=$r.name -ne 'aa-owner-1'
 if($full90 -ne $expectedPass){throw 'original 90 percent outcome'}
 & "$repoPath/out/x11-p-r3/build-B0/Release/r3_gate.exe" check $root *> "$ReviewRoot/$($r.name)-gate.log"
 $gateExit=$LASTEXITCODE;if($gateExit -ne $(if($expectedPass){0}else{2})){throw 'gate exit'}
 $reports+=@{run=$r.name;attempts=$frames.Count;published=$published.Count;consumed=$consumed.Count;owned=$owned.Count;consumer_skips=$published.Count-$consumed.Count;decision_skips=$consumed.Count-$owned.Count;original_full90_pass=$full90;measurement_recognition_n=$mc.Count;measurement_owner_n=$mo.Count;principal_n2000=($mc.Count -ge 2000 -and $mo.Count -ge 2000);gate_exit=$gateExit;independent_quantiles='all and measurement recognition/owner/capture_to_owner; full lateness';segment_hashes_checked=$completion.event_segments.Count;summary_sha256=(Hash "$root/summary.json")}
}
$input=Get-Content "$batchPath/input-binding-before-cost.json" -Raw|ConvertFrom-Json
foreach($f in $input.files){if((Hash $f.path) -ne $f.sha256 -or (Get-Item -LiteralPath $f.path).Length -ne $f.bytes){throw 'input freeze'}}
$dependency=Get-Content "$batchPath/compiled-dependency-freeze.json" -Raw|ConvertFrom-Json
foreach($m in $dependency.link_maps){$text=[IO.File]::ReadAllText($m.path);if((Hash $m.path) -ne $m.sha256 -or $text -match 'pas_core:session_archive\.obj' -or $text -notmatch 'SessionArchive.*session_archive\.obj'){throw 'archive link map'}}
. "$repoPath/tools/collect-runtime-x11-p-r1-tests.ps1"
$historical=Get-Content "$batchPath/tests-final-collected.json" -Raw|ConvertFrom-Json
$xmlChecks=@();foreach($x in $historical.historical_XML_verified_only){$p="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-r2/$($x.name).xml";$t=Get-R1TestResult $p;if($t.sha256 -ne $x.result.sha256 -or $t.tests -ne $x.result.tests -or $t.failures -ne $x.result.failures -or $t.skipped -or $t.errors -or $t.disabled){throw 'historical XML'};$xmlChecks+=@{name=$x.name;result=$t;rerun=$false}}
$bridges=@();foreach($role in @('B0','B1')){
 $new="$ReviewRoot/bridge-$role";$ref="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p/bridge-$role/public-events.jsonl"
 Save "bridge-$role-command.json" @{exe="$repoPath/out/x11-p-r3/build-$role/Release/r3_meter.exe";arguments=@('bridge',$new,$ref)}
 & "$repoPath/out/x11-p-r3/build-$role/Release/r3_meter.exe" bridge $new $ref *> "$ReviewRoot/bridge-$role.log"
 if($LASTEXITCODE -ne 0){throw 'bridge exit'}
 if((Hash "$new/summary.json") -ne (Hash "$batchPath/bridge-final-$role/summary.json")){throw 'bridge summary differs'}
 $bridges+=@{role=$role;exit=0;summary_sha256=(Hash "$new/summary.json");byte_identical=$true}
}
Save 'controller-crosscheck.json' @{runs=$reports;bridges=$bridges;input_bindings=$input.files.Count;link_maps=$dependency.link_maps.Count;historical_XML=$xmlChecks;candidate_performance='Unknown';AA_noise='Not evaluated';new_cost=0;new_stress=0;new_build=0;review_bytes_before_report=(Bytes $ReviewRoot)}
if((Bytes $ReviewRoot) -gt 32MB){throw 'controller reserve exceeded'}
Write-Output 'Independent denominators, selected raw quantiles, segment SHA, gate exits, maps, old XML and both bridges verified.'
