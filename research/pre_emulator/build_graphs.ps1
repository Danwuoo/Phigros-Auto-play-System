param([ValidatePattern('^main[0-9]{2}$')][string]$BuildAttempt='main29')
. (Join-Path $PSScriptRoot 'common.ps1')
$script:maintenancePath=$PSCommandPath;$Operation='tests'
$planning=Join-Path $repo 'out/prelive-pre-emulator-planning'
function ExistingProcess([string]$stage){
 $spec=Join-Path $pkg ($stage+'-direct-spec.json')
 if(-not(Test-Path -LiteralPath $spec)){$spec=Join-Path $out ($stage+'-inputs/spec.json')}
 $row=@{}
 foreach($pair in @(@('specification',$spec),@('input_closure',(Join-Path $pkg ($stage+'-input-closure-verification.json'))),@('result',(Join-Path $out ($stage+'/'+$stage+'-result.json'))),@('verification',(Join-Path $out ($stage+'/'+$stage+'-verification.json'))),@('freeze',(Join-Path $out ($stage+'/freeze.json'))),@('state',(Join-Path $out ($stage+'/state.json'))),@('contract',(Join-Path $out ($stage+'/contract.json'))),@('command',(Join-Path $out ($stage+'/'+$stage+'-command.json'))))){$row[$pair[0]]=Binding $pair[1]}
 return $row
}
$releaseEvaluation=Join-Path $repo ('out/prelive-pre-emulator-evaluation-'+$BuildAttempt+'-release/evaluation.json')
$re=Get-Content -LiteralPath $releaseEvaluation -Raw|ConvertFrom-Json;CheckEntry $re.build
$rb=Get-Content -LiteralPath $re.build.path -Raw|ConvertFrom-Json
if($rb.configuration -cne 'release' -or -not $re.affected_cold_pass -or $re.tests.Count -ne 25){throw 'complete sealed Release verifier build required'}
$exe=@($rb.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'qualification_reader.exe'})
if($exe.Count -ne 1){throw 'one sealed Release read-only verifier required'}
$releaseGraph=$null
foreach($Mode in @('release','debug','asan')){
 $evaluation=Join-Path $repo ('out/prelive-pre-emulator-evaluation-'+$BuildAttempt+'-'+$Mode+'/evaluation.json')
 $e=Get-Content -LiteralPath $evaluation -Raw|ConvertFrom-Json;CheckEntry $e.build
 $b=Get-Content -LiteralPath $e.build.path -Raw|ConvertFrom-Json;$commit=$b.source_commit
 if($rb.source_commit -cne $commit -or (& git -C $repo rev-parse HEAD) -cne $commit -or (& git -C $repo status --porcelain)){throw 'same exact clean source required'}
 $stages=@()
 foreach($pin in @($b.configure_receipt)+@($b.build_receipts)+@($e.tests.receipt)){
  CheckEntry $pin;$v=Get-Content -LiteralPath $pin.path -Raw|ConvertFrom-Json;$stages+=@(ExistingProcess $v.stage)
 }
 $environment=Join-Path (Split-Path -Parent $evaluation) 'compiler-environment-binding.json'
 $eb=Get-Content -LiteralPath $environment -Raw|ConvertFrom-Json;$ev=Get-Content -LiteralPath $eb.verification.path -Raw|ConvertFrom-Json
 $environmentProcess=ExistingProcess $ev.stage
 $actualInput=Join-Path $planning ($BuildAttempt+'-'+$Mode+'-readonly-build-input.json');Fresh $actualInput
 NewJson $actualInput @{schema='pas.readonly-build-input.v1';evaluation=(Binding $evaluation);stages=$stages;source_commit=$commit;mode=$Mode;compiler_environment=(Binding $environment);environment_process=$environmentProcess;gate=(Binding $gate);qualification_issued=$false;device_operations=0}
 $inputs=@($PSCommandPath,$actualInput,$evaluation,$e.build.path,$environment,$b.source_manifest.path,$b.dependency_manifest.path,$b.core_build_plan.path,$releaseEvaluation,$re.build.path)+@($rb.files.path)+@($b.files.path)
 if($b.serial_symbol_archive){$inputs+=@($b.serial_symbol_archive.path);$sp=Get-Content -LiteralPath $b.serial_symbol_archive.path -Raw|ConvertFrom-Json;$inputs+=@($sp.archive.path)}
 foreach($p in @($environmentProcess)+$stages){
  $inputs+=@($p.Values.path);$closure=Get-Content -LiteralPath $p.input_closure.path -Raw|ConvertFrom-Json
  $inputs+=@($closure.root.path);$rootData=Get-Content -LiteralPath $closure.root.path -Raw|ConvertFrom-Json
  # Freeze all SHA-bound graph metadata. Reader checks every transitive payload
  # and rehashes all of it at finish(); flattening it here duplicated the graph
  # into the separate 8192-file native-stage cap (main28 Debug: 8247 files).
  $inputs+=@($rootData.leaves.path)
 }
 $inputs+=@(GateInputs)
 $g=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
 $inputs+=@($g.guard_source.path,$g.process_source.path,$g.cached_identity_controls.path,$g.identity_wait_controls.path)
 if($Mode -cne 'release'){
  if(-not $releaseGraph){throw 'Release verifier graph must pass first'}
  CheckEntry $releaseGraph;$inputs+=@($releaseGraph.path)
 }
 $inputs=@($inputs|Sort-Object -Unique);$seal=@{files=@($b.files)+@($rb.files)}
 $report=Join-Path $planning ($BuildAttempt+'-'+$Mode+'-readonly-build-report.json');Fresh $report
 # Configuration of the graph is independent of the verifier executable.
 # The fresh same-source Release verifier was already checked in the first
 # graph; Debug/ASan retain all their original binaries, instrumentation,
 # control tests and receipts. Native stage bounds and graph gates stay exact.
 Native ('test-preflight-'+$BuildAttempt+'-'+$Mode+'-actual-build-graph') $exe[0].path @($repo,$actualInput,$report) $inputs 900
 $r=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
 if(-not $r.pass -or $r.qualification_issued -or $r.source_commit -cne $commit -or $r.mode -cne $Mode){throw 'complete current graph required'}
 if($Mode -ceq 'release'){$releaseGraph=Binding $report}
 Write-Output ($Mode+' full cold graph PASS; same-source sealed Release verifier; no admission issued')
}
