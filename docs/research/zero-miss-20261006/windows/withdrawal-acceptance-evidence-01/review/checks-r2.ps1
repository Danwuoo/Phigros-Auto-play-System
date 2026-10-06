param([ValidateSet('release','debug','asan')][string]$Mode)
$ErrorActionPreference='Stop'
$taskRepo='C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006'
$pwsh='C:\Users\wurre\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe'
$outRoot=Join-Path $taskRepo 'out/windows-handoff'
$root=Join-Path $taskRepo ('out/bridge-win-withdraw-'+$Mode+'-01')
$build='build-bridge-withdraw-'+$Mode+'-01'
$receipt=Join-Path $outRoot ($build+'/'+$build+'-verification.json')
$verified=Get-Content -LiteralPath $receipt -Raw|ConvertFrom-Json
if($verified.pending -or $verified.native_exit -ne 0 -or $verified.runner_exit -ne 0 -or $verified.verification_exit -ne 0){throw 'checks-build-not-verified'}
$freeze=Join-Path $outRoot ($build+'/freeze.json')
$inputs=@((Get-Content -LiteralPath $freeze -Raw|ConvertFrom-Json).files.path)+@($PSCommandPath,$receipt,$freeze)+
 @(Get-ChildItem -LiteralPath $root -File|Where-Object Extension -in '.exe','.lib','.dll'|ForEach-Object FullName)
$selection=Join-Path $outRoot 'real-pixels-selection-02/selection.json'
$selected=Get-Content -LiteralPath $selection -Raw|ConvertFrom-Json
$auditInputs=@($selection,$selected.index_path,(Join-Path $selected.record_root 'manifest.json'))+
 @($selected.frames|Where-Object {$_.index.ordinal -in 1519,3030}|ForEach-Object path)
foreach($kind in @('prefix','diagnostic','inherited-audit')) {
 $stage=$kind+'-bridge-withdraw-'+$Mode+'-02'
 $report=Join-Path $outRoot ($kind+'-withdraw-'+$Mode+'-02.json')
 $extra=@()
 if($kind -eq 'prefix'){$exe='bridge_tests.exe';$argv=@($report)}
 elseif($kind -eq 'diagnostic'){$exe='pixel_diagnostics_tests.exe';$argv=@($report)}
 else {
  $exe='pixel_diagnostics_audit.exe'
  $old=Join-Path $outRoot 'pixels-profile-release-01.json.rows.jsonl'
  $review=Join-Path $outRoot 'pixels-review-release-01.json.rows.jsonl'
  $argv=@($old,$review,$selection,$report);$extra=$auditInputs+@($old,$review)
 }
 $nativeArguments=@{StageName=$stage;Executable=(Join-Path $root $exe);Arguments=$argv;
 GateReceipt=(Join-Path $outRoot 'native-qualification-01/native-gate.json');Inputs=($inputs+$extra);TotalSeconds=300}
 $json=Join-Path $PSScriptRoot ($Mode+'-r2-'+$kind+'.json')
 $bytes=[Text.Encoding]::UTF8.GetBytes((@{script=(Join-Path $taskRepo 'tools/zero_miss_windows/native-stage.ps1');arguments=$nativeArguments}|ConvertTo-Json -Depth 12))
 $s=[IO.FileStream]::new($json,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
 try{$s.Write($bytes);$s.Flush($true)}finally{$s.Dispose()}
 & $pwsh -NoProfile -File (Join-Path $PSScriptRoot 'stage.ps1') -Parameters $json
 if($LASTEXITCODE -ne 0){throw ($stage+'-exit-'+$LASTEXITCODE)}
 $result=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
 if($kind -eq 'prefix' -and ($result.assertions -ne 113 -or $result.failed_assertions -ne 0)){throw 'prefix-regression'}
 if($kind -eq 'diagnostic' -and ($result.assertions -ne 32 -or $result.failed_assertions -ne 0)){throw 'diagnostic-regression'}
 if($kind -eq 'inherited-audit' -and (-not $result.passed -or $result.compared_rows -ne 256 -or $result.raw_png_points_verified -ne 1665)){throw 'inherited-audit-regression'}
 $result|Select-Object schema,assertions,failed_assertions,passed,compared_rows,raw_png_points_verified|ConvertTo-Json -Compress
}
