$ErrorActionPreference='Stop'
$repo='C:\Users\wurre\Desktop\Phigros-Auto-play-System'
$old=Join-Path $repo 'research/x10d_o_bvi_r2f_control'
$new=Join-Path $repo 'research/x10d_o_bvi_build'
$batch=Join-Path $repo 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-build'
if(Test-Path $batch){throw 'bootstrap-existing-batch'}
if(Test-Path (Join-Path $repo 'out/x10d-o-bvi-build')){throw 'bootstrap-existing-out'}
[IO.Directory]::CreateDirectory($batch)|Out-Null
$map=@('bvi-r2f-20261005-03','bvi-build-20261005-01','x10d_o_bvi_r2f_control','x10d_o_bvi_build','hold-ownership-x10d-o-bvi-r2f-control','hold-ownership-x10d-o-bvi-build','x10d-o-bvi-r2f-control','x10d-o-bvi-build')
foreach($leaf in @('bvi.cpp','bvi.hpp','main.cpp','driver.inc','CMakeLists.txt','owned.cs','identity.ps1','control-child.ps1','control-parent.ps1','transaction.ps1','mock.ps1','control-pretest.ps1','transaction-pretest.ps1')){
 $s=[IO.File]::ReadAllText((Join-Path $old $leaf))
 if($leaf -in @('main.cpp','control-child.ps1')){$s=$s.Replace($map[0],$map[1])}
 if($leaf -eq 'transaction-pretest.ps1'){$s=$s.Replace('40902683','40249475').Replace('40746474','40093266').Replace('1040357','1693565')}
 [IO.File]::WriteAllText((Join-Path $new $leaf),$s,[Text.UTF8Encoding]::new($false))
}
# Preserve the old protection reconstruction verbatim; exclude only this new package from Git input.
$s=[IO.File]::ReadAllText((Join-Path $old 'common.ps1'))
$s=$s.Replace("function Protection(){","function LegacyProtection(){")
$s=$s.Replace("$"+'script:Attempt='+"'bvi-r2f-20261005-03'","$"+'script:Attempt='+"'bvi-build-20261005-01'")
$s=$s.Replace("$"+'script:Source=Join-Path $Repo '+"'research/x10d_o_bvi_r2f_control'","$"+'script:Source=Join-Path $Repo '+"'research/x10d_o_bvi_build'")
$s=$s.Replace("$"+'script:Evidence=Join-Path $Campaign '+"'hold-ownership-x10d-o-bvi-r2f-control'","$"+'script:Evidence=Join-Path $Campaign '+"'hold-ownership-x10d-o-bvi-build'")
$s=$s.Replace("$"+'script:Out=Join-Path $Repo '+"'out/x10d-o-bvi-r2f-control'","$"+'script:Out=Join-Path $Repo '+"'out/x10d-o-bvi-build'")
$s=$s.Replace('40902683','40249475').Replace('8335047','8988255').Replace('8285248004','8285901212')
$s=$s.Replace('HOLD_OWNERSHIP_X10D_O_BVI_R2F_CONTROL_RESULT_20261005.md','HOLD_OWNERSHIP_X10D_O_BVI_BUILD_RESULT_20261005.md').Replace('HOLD_OWNERSHIP_X10D_O_BVI_R2F_CONTROL_HANDOFF_20261005.md','HOLD_OWNERSHIP_X10D_O_BVI_BUILD_HANDOFF_20261005.md')
$needle=" $"+'originalPaths=@($allPaths|Where-Object'
$inject=" $"+'allPaths=@($allPaths|Where-Object{$_ -notlike ''research/x10d_o_bvi_build/*'' -and $_ -notlike ''docs/HOLD_OWNERSHIP_X10D_O_BVI_BUILD_*''})'+"`n $"+'allDirty=@($allDirty|Where-Object{$p=$_.Substring(3);$p -notlike ''research/x10d_o_bvi_build/*'' -and $p -notlike ''docs/HOLD_OWNERSHIP_X10D_O_BVI_BUILD_*''})'+"`n"
if(-not $s.Contains($needle)){throw 'bootstrap-protection-insertion'}
$s=$s.Replace($needle,$inject+$needle)
$s=$s.Replace('function CheckState(','function BaseCheckState(')
$s=$s.Replace('$before.git_paths_sha256','$before.legacy.git_paths_sha256').Replace('$before.dirty_sha256','$before.legacy.dirty_sha256')
[IO.File]::WriteAllText((Join-Path $new 'base.ps1'),$s,[Text.UTF8Encoding]::new($false))
foreach($leaf in @('bvi.cpp','bvi.hpp','owned.cs','identity.ps1','transaction.ps1','driver.inc')){
 if((Get-FileHash -LiteralPath "$new/$leaf").Hash -cne (Get-FileHash -LiteralPath "$old/$leaf").Hash){throw "immutable-copy-$leaf"}
}
'bootstrap complete; no native trial'
