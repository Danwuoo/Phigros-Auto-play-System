. "$PSScriptRoot/r2-common.ps1"
$binding=Get-Content "$batchPath/source-binding-before-stress.json" -Raw|ConvertFrom-Json
$dependencies=Get-Content "$batchPath/compiled-dependency-freeze.json" -Raw|ConvertFrom-Json
foreach($role in @('B0','B1','asan')){
 foreach($f in @($binding.files)+@($dependencies.files)){if((R2Hash $f.path) -ne $f.sha256 -or (Get-Item -LiteralPath $f.path).Length -ne $f.bytes){throw "freeze changed $($f.path)"}}
 if(@(Get-ChildItem -LiteralPath $batchPath -Filter 'stress-*-command.json' -File).Count -ge 3){throw 'stress quota'}
 [void](R2Capacity)
 $used=R2Bytes $batchPath;if($used+16MB -gt 128MB){throw 'stress reserve'}
 $config=if($role -eq 'asan'){'Debug'}else{'Release'}
 R2Run "stress-active-$role" "$outPath/build-$role/$config/r2_active_meter.exe" @('cost','owner',"$batchPath/stress-active-$role",'stress')
 $result=Get-Content "$batchPath/stress-active-$role/summary.json" -Raw|ConvertFrom-Json
 if(!$result.active_coverage.covered -or (R2Bytes "$batchPath/stress-active-$role") -gt 16MB){throw 'coverage/capacity failed; no extra stress'}
}
R2Run 'active-raw-audit' "$outPath/raw-audit/Release/r2_raw_audit.exe" @($batchPath,"$batchPath/active-raw-analysis.json",'active')
R2Capacity | ConvertTo-Json
