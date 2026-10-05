param([ValidateSet('B0','B1','asan')][string]$Role)
. "$PSScriptRoot/r3-common.ps1"
if(Test-Path "$batchPath/source-binding-before-cost.json"){throw 'freeze prohibits rebuild'}
$args3=R3ConfigureArgs "$repoPath/apps/runtime_x11_p_r3" "$outPath/build-$Role"
$coreRole=if($Role -eq 'asan'){'B1'}else{$Role}
$args3+=@("-DR3_ROLE=$coreRole","-DR3_ASAN=$(if($Role -eq 'asan'){'ON'}else{'OFF'})")
R3Run "configure-$Role-1" cmake $args3 | Out-Null
R3Run "build-$Role-1" cmake @('--build',"$outPath/build-$Role",'--config',$(if($Role -eq 'asan'){'Debug'}else{'Release'}),'--parallel','2') | Out-Null
R3Capacity | ConvertTo-Json
