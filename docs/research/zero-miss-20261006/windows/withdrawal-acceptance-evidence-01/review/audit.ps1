param([ValidateSet('release','debug','asan')][string]$Mode,
      [ValidateSet('current','bad-policy','bad-rgb')][string]$Case='current')
$ErrorActionPreference='Stop'
$taskRepo='C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006'
$pwsh='C:\Users\wurre\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe'
$outRoot=Join-Path $taskRepo 'out/windows-handoff'
$root=Join-Path $taskRepo ('out/bridge-win-withdraw-'+$Mode+'-01')
$build='build-bridge-withdraw-'+$Mode+'-01'
$receipt=Join-Path $outRoot ($build+'/'+$build+'-verification.json')
$freeze=Join-Path $outRoot ($build+'/freeze.json')
$inputs=@((Get-Content -LiteralPath $freeze -Raw|ConvertFrom-Json).files.path)+@($PSCommandPath,$receipt,$freeze)+
 @(Get-ChildItem -LiteralPath $root -File|Where-Object Extension -in '.exe','.lib','.dll'|ForEach-Object FullName)
$selection=Join-Path $outRoot 'real-pixels-selection-02/selection.json'
$selected=Get-Content -LiteralPath $selection -Raw|ConvertFrom-Json
$inputs+=@($selection,$selected.index_path,(Join-Path $selected.record_root 'manifest.json'))+
 @($selected.frames|Where-Object {$_.index.ordinal -in 1519,3030}|ForEach-Object path)
$before=Join-Path $outRoot 'pixels-profile-release-01.json.rows.jsonl'
$after=if($Case -eq 'current'){Join-Path $outRoot ('pixels-withdraw-'+$Mode+'-01.json.rows.jsonl')}
 else {Join-Path $PSScriptRoot ('controls/'+$Case+'.rows.jsonl')}
$inputs+=@($before,$after)
$stage='audit-'+$Case+'-bridge-withdraw-'+$Mode+'-01'
$report=Join-Path $outRoot ('audit-'+$Case+'-withdraw-'+$Mode+'-01.json')
$nativeArguments=@{StageName=$stage;Executable=(Join-Path $root 'pixel_diagnostics_audit.exe');
 Arguments=@($before,$after,$selection,$report);GateReceipt=(Join-Path $outRoot 'native-qualification-01/native-gate.json');
 Inputs=$inputs;TotalSeconds=300;ExpectedNative=$(if($Case -eq 'current'){0}else{1})}
$json=Join-Path $PSScriptRoot ($Mode+'-audit-'+$Case+'.json')
$bytes=[Text.Encoding]::UTF8.GetBytes((@{script=(Join-Path $taskRepo 'tools/zero_miss_windows/native-stage.ps1');arguments=$nativeArguments}|ConvertTo-Json -Depth 12))
$s=[IO.FileStream]::new($json,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try{$s.Write($bytes);$s.Flush($true)}finally{$s.Dispose()}
& $pwsh -NoProfile -File (Join-Path $PSScriptRoot 'stage.ps1') -Parameters $json
if($LASTEXITCODE -ne 0){throw ($stage+'-exit-'+$LASTEXITCODE)}
$result=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
if($Case -eq 'current') {
 if(-not $result.passed -or $result.compared_rows -ne 256 -or $result.raw_png_points_verified -ne 1665){throw 'current-audit-regression'}
}else{
 $expected=if($Case -eq 'bad-policy'){'audit decision field changed'}else{'audit raw PNG pixel mismatch'}
 if($result.passed -or $result.error -cne $expected){throw 'audit-negative-control-not-rejected'}
}
$result|Select-Object schema,passed,error,compared_rows,raw_png_points_verified|ConvertTo-Json -Compress
