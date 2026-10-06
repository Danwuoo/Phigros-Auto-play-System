param([Parameter(Mandatory)][string]$BuildRoot,[Parameter(Mandatory)][string]$Mode,
      [Parameter(Mandatory)][string]$EvidenceRoot,[Parameter(Mandatory)][string]$GateReceipt,
      [Parameter(Mandatory)][string]$FrozenSupplemental,
      [ValidateRange(30,1800)][int]$SuiteSeconds=900)
$ErrorActionPreference='Stop'
$taskRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if($Mode -notmatch '^[a-z0-9-]+$'){throw 'mode'}
$bundle=Join-Path $taskRepo ('out/windows-handoff/legacy-'+$Mode+'-01-inputs')
if(Test-Path -LiteralPath $bundle){throw 'fresh-results'}
[IO.Directory]::CreateDirectory($bundle)|Out-Null
$evidence=Join-Path ([IO.Path]::GetFullPath($EvidenceRoot)) 'measurements/game-assist/2026-09-30-m0-manual-continue'
$argsNative=@((Join-Path $evidence 'hold-ownership-x10d-o-bvi/normalized-execution.json'),
 (Join-Path $evidence 'hold-ownership-x10d-o-bvi/oracle.json'),
 (Join-Path $evidence 'hold-ownership-x10d-o-bvi-r1/typed-r1.json'),
 ([IO.Path]::GetFullPath($FrozenSupplemental)),
 (Join-Path $evidence 'hold-ownership-x10d-o-bvi-r1/r1-cases.json'),
 (Join-Path $evidence 'hold-ownership-x10d-o-bvi-r1/expected-coverage.json'))
$expected=@('96d536c5b5e9e01e359da2ead01596428b46f4265f28ba295ee9d9b4297650c5',
 '90f8680c4362d07a718ac2bb7142b37396871b7ffd81523a06ddd8a685ec5425',
 'ce69f19d5c488e780c7d1cb7785601d3e3f80750b134440c636dda20f19ff8c3',
 '45ab5f3d97c3513fb6836211801922a9d86863de273739e35b5e4263b7a6c0a1',
 '8da656609b424eda46633145728dd485d12bfb25e83f1850235a527891e28f46',
 '4985ec8aac5d89f0863125daa6be483b26c1b200b00f5bbcb23fc691cc517f2a')
for($i=0;$i -lt 6;$i++){if((Get-FileHash -LiteralPath $argsNative[$i] -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expected[$i]){throw 'frozen-input-sha'}}
$binary=Join-Path $BuildRoot 'legacy.exe'
$inputs=@($PSCommandPath,$binary)+$argsNative
$inputs+=@(Get-ChildItem -LiteralPath $BuildRoot -File -Filter '*asan*.dll'|ForEach-Object FullName)
foreach($name in @('wrong-contact','suite')) {
 $report=Join-Path $bundle ($name+'.json')
 [IO.File]::WriteAllText($report,'{"attempt":"windows-bvi-v3-20261006","pending":true}',[Text.UTF8Encoding]::new($false))
 [IO.File]::Copy($report,(Join-Path $bundle ($name+'.reservation.json')),$false)
 $argv=@($argsNative)+@($report)
 if($name -eq 'wrong-contact'){$argv+=@('wrong-contact-only')}
 $stage='legacy-'+$Mode+'-'+$name+'-01'
 # Use the already-qualified cmd wrapper and retain the actual target exit in
 # TRACE. The frozen C++ audit also requires non-pending full result rows.
 foreach($value in @($binary)+$argv){if($value -match '["%\r\n]'){throw 'literal-cmd-argument'}}
 $cmdPath=Join-Path $bundle ($name+'.cmd')
 $command='"'+$binary+'" '+(($argv|ForEach-Object {'"'+$_+'"'}) -join ' ')
 $lines=@('@echo off','echo TRACE legacy NATIVE_START',$command,'set "PAS_NATIVE_EXIT=%errorlevel%"',
   'echo TRACE legacy NATIVE_END exit=%PAS_NATIVE_EXIT%','exit /b %PAS_NATIVE_EXIT%')
 [IO.File]::WriteAllText($cmdPath,($lines -join "`r`n")+"`r`n",[Text.UTF8Encoding]::new($false))
 $spec=@{stage_root=(Join-Path $taskRepo ('out/windows-handoff/'+$stage));stage=$stage;exe=$env:ComSpec;
   argv=@('/d','/s','/c',$cmdPath);native_exit=1;runner_exit=1;total_s=$SuiteSeconds;stream_cap=4194304;inputs=($inputs+@($cmdPath))}
 $specPath=Join-Path $bundle ($name+'.spec.json')
 [IO.File]::WriteAllText($specPath,($spec|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
 & (Join-Path $PSScriptRoot 'invoke.ps1') -Specification $specPath -GateReceipt $GateReceipt
 if($LASTEXITCODE -ne 0){throw 'legacy-native-stage'}
 $trace=[IO.File]::ReadAllText((Join-Path $spec.stage_root ($stage+'.stdout.log')))
 if($trace -notmatch 'TRACE legacy NATIVE_END exit=1'){throw 'native-trace-exit'}
 $facts=(Get-Content -LiteralPath (Join-Path $spec.stage_root ($stage+'-result.json')) -Raw|ConvertFrom-Json).facts
 [IO.File]::WriteAllText((Join-Path $bundle ($name+'.exit')),([string]$facts.exit_code)+"`n",[Text.UTF8Encoding]::new($false))
}
$stage='legacy-'+$Mode+'-audit-01'
$spec=@{stage_root=(Join-Path $taskRepo ('out/windows-handoff/'+$stage));stage=$stage;
 exe=(Join-Path $BuildRoot 'audit_io.exe');argv=@('--check-results',$bundle);native_exit=1;runner_exit=1;
 total_s=90;stream_cap=4194304;inputs=($inputs+@(Join-Path $bundle 'wrong-contact.json')+@(Join-Path $bundle 'suite.json')+@(Join-Path $bundle 'wrong-contact.exit')+@(Join-Path $bundle 'suite.exit'))}
$specPath=Join-Path $bundle 'audit.spec.json'
[IO.File]::WriteAllText($specPath,($spec|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'invoke.ps1') -Specification $specPath -GateReceipt $GateReceipt
if($LASTEXITCODE -ne 0){throw 'legacy-audit-stage'}
[IO.File]::Copy((Join-Path $spec.stage_root ($stage+'.stdout.log')),(Join-Path $bundle 'legacy-summary.json'),$false)
$summary=Get-Content -LiteralPath (Join-Path $bundle 'legacy-summary.json') -Raw|ConvertFrom-Json
if($summary.assertions -ne 3938 -or $summary.layer_cases -ne 356 -or $summary.failed_assertions -ne 36 -or
 $summary.new_case_failed_assertions -ne 0 -or -not $summary.wrong_contact_verified){throw 'legacy-denominator-or-regression'}
$cloud=Get-Content -LiteralPath (Join-Path $taskRepo 'docs/research/zero-miss-20261005/round4/evidence/final-release/legacy-summary.json') -Raw|ConvertFrom-Json
$frozenFailures=$cloud.failed_rows|ConvertTo-Json -Depth 30 -Compress
$localFailures=$summary.failed_rows|ConvertTo-Json -Depth 30 -Compress
if($frozenFailures -cne $localFailures){throw 'legacy-failure-identities-differ'}
@{mode=$Mode;assertions=3938;layer_cases=356;failed_assertions=36;new_failures=0;
 failure_rows_equal_cloud=$true;aggregate_pass=$false}|ConvertTo-Json -Compress
exit 0
