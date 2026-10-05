param([ValidateSet('B0','B1','asan')][string]$Role)
. "$PSScriptRoot/r2-common.ps1"
if(Test-Path -LiteralPath "$batchPath/source-binding-before-stress.json"){throw 'freeze prohibits rebuild'}
$configureArgs=R2ConfigureArgs "$repoPath/apps/runtime_x11_p_r2" "$outPath/build-$Role"
$coreRole=if($Role -eq 'asan'){'B1'}else{$Role}
$configureArgs+=@("-DR2_ROLE=$coreRole", "-DR2_ASAN=$(if($Role -eq 'asan'){'ON'}else{'OFF'})")
R2Run "configure-$Role-1" cmake $configureArgs
$config=if($Role -eq 'asan'){'Debug'}else{'Release'}
R2Run "build-$Role-1" cmake @('--build',"$outPath/build-$Role",'--config',$config,'--parallel','2')
R2Capacity | ConvertTo-Json
