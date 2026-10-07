$ErrorActionPreference='Stop'
$repo='C:\Users\wurre\Desktop\PAS-windows-readiness-20261007';$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh=(Get-Command pwsh).Source;$Mode='release';$Operation='tests';$Campaign='';$script:inputClosures=@{};$script:maintenancePath=$PSCommandPath
$gate=Join-Path $out 'guard-v3-native-01/native-gate.json';$tool=Join-Path $out 'tool-snapshot-01/tool-snapshot.json';$recipe=Join-Path $repo 'research/prelive_current/run.ps1'
$tok=$null;$err=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tok,[ref]$err);if($err.Count){throw 'shared parse'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage','Native')){
 $d=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name);if($d.Count -ne 1){throw 'shared function'};$t=$d[0].Extent.Text
 if($name -ceq 'Native'){$t=$t.Replace('$PSCommandPath','$script:maintenancePath').Replace('tools/zero_miss_windows/invoke.ps1','research/windows_process_guard_v3/invoke.ps1')}
 if($name -ceq 'Capacity'){$t=$t.Replace('12884901888','34359738368')};if($name -ceq 'GateInputs'){$t=$t.Replace('pas.windows-offline-process-gate.v1','pas.windows-offline-process-gate.v3')};. ([scriptblock]::Create($t))
}
$sourceRepo='C:\Users\wurre\Desktop\PAS-owner-cooperative-20261007';$commit='45f8a5f9ec0f256b07f1b31a5bead1bf0a3aacf1'
if((& git -C $sourceRepo rev-parse HEAD) -cne $commit -or (& git -C $sourceRepo status --porcelain)){throw 'clean same candidate'}
$manifest=Join-Path $repo 'out/prelive-cooperative-prefix-prefix01/source-manifest.json';$source=Get-Content -LiteralPath $manifest -Raw|ConvertFrom-Json
if($source.dirty -or $source.source_commit -cne $commit){throw 'source manifest'};foreach($f in $source.files){CheckEntry $f}
$root=Join-Path $repo 'out/prelive-cooperative-cost-tools-release-tools01';$artifact=Join-Path $repo 'out/prelive-cooperative-cost-tools-evaluation-tools02';Fresh $artifact;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$builds=@();$configure='configure-prelive-cooperative-cost-tools-tools01'
foreach($stage in @($configure)+@('cost-analysis','cost-analysis-tests','pas-cost-receipt-verify','pas-cost-comparison','comparison-metrics-tests'|ForEach-Object {'build-prelive-cooperative-cost-tools-tools01-'+$_})){
 Receipt $stage;$builds+=@(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))
 $cr=Get-Content -LiteralPath (Join-Path $pkg ($stage+'-input-closure-verification.json')) -Raw|ConvertFrom-Json
 if(-not $cr.pass -or -not $cr.before_verified -or -not $cr.after_verified -or $cr.status -cne 'COMPLETE'){throw 'build closure'}
 CheckEntry $cr.root;foreach($l in (Get-Content -LiteralPath $cr.root.path -Raw|ConvertFrom-Json).leaves){CheckEntry $l;foreach($f in (Get-Content -LiteralPath $l.path -Raw|ConvertFrom-Json).files){CheckEntry $f}}
}
$files=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object {Binding $_.FullName});$seal=@{files=$files};$g=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$inputs=@($PSCommandPath,$recipe,$manifest,(Join-Path $repo 'out/build-cooperative-tools01.ps1'))+$source.files.path+$builds.path+$files.path+(GateInputs)+@($g.guard_source.path,$g.process_source.path,$g.cached_identity_controls.path,$g.identity_wait_controls.path)
$fixtures=Join-Path $artifact 'analyzer-fixtures';$stage='test-prelive-cooperative-cost-tools-tools02-analyzer'
# The native fixture CLI requires a fresh output directory AND repository root.
Native $stage (Join-Path $root 'cost_analysis_tests.exe') @($fixtures,$sourceRepo) $inputs 900
$report=Join-Path $fixtures 'result.json';$r=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
if($r.schema -cne 'pas.cost-analyzer-tests.v1' -or -not $r.pass -or $r.checks -ne 42){throw 'analyzer actual 42 controls'}
$tests=@(@{test='cost_analysis_tests';report=(Binding $report);receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))})
$report=Join-Path $artifact 'comparison-metrics.json';$stage='test-prelive-cooperative-cost-tools-tools02-metrics'
Native $stage (Join-Path $root 'comparison_metrics_tests.exe') @($report) @($inputs+$tests.report.path+$tests.receipt.path) 900
$r=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json;if($r.schema -cne 'pas.windows-cost-comparison-metric-tests.v1' -or $r.failed -ne 0 -or $r.assertions -ne 22){throw 'metrics actual 22 controls'}
$tests+=@(@{test='comparison_metrics_tests';report=(Binding $report);receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))})
NewJson (Join-Path $artifact 'build-binding.json') @{schema='pas.cooperative-cost-tools-build.v1';source_commit=$commit;source_manifest=(Binding $manifest);build_root=$root;files=$files;configure_receipt=$builds[0];build_receipts=$builds[1..5];tests=$tests;completion_recipe=(Binding $PSCommandPath);earlier_stop_preserved=(Binding (Join-Path $out 'test-prelive-cooperative-cost-tools-tools01-cost-analysis-tests/test-prelive-cooperative-cost-tools-tools01-cost-analysis-tests-verification.json'));earlier_stop_reason='missing fresh output root / repository CLI arguments';device_operations=0;live_ready=$false}
