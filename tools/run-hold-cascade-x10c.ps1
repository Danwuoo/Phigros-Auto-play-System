param([ValidateSet('Build','Tests','ASan','Replay','Audit')][string]$Mode,[ValidateSet('baseline','variant')][string]$Role='baseline',[ValidateSet('on','off')][string]$Trace='on',[string]$Tag='1')
$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'hold-cascade-x10c'
$buildPath=Join-Path $repoPath 'out/x10c/build'
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$value){$data=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Depth 40)+"`n");$s=[IO.File]::Open("$batchPath/$name",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$s.Write($data,0,$data.Length)}finally{$s.Dispose()}}
function Run($name,$exe,[string[]]$a){Save "$name-command.json" @{exe=$exe;arguments=$a;PAS_RGB_CLIP_ROOT=$env:PAS_RGB_CLIP_ROOT};& $exe @a *> "$batchPath/$name.log";$code=$LASTEXITCODE;Save "$name-exit.json" @{exit=$code;log_sha256=(Hash "$batchPath/$name.log")};Write-Output "$name exit=$code";if($code -ne 0){throw "$name failed; evidence retained"}}
if($Tag -notmatch '^[a-zA-Z0-9-]{1,24}$'){throw 'tag'}
if($Mode -eq 'Build'){
  Run "configure-$Tag" 'cmake' @('-S',"$repoPath/apps/frame_review/x10c_offline",'-B',$buildPath,'-G','Visual Studio 18 2026','-A','x64','-T','v145','-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275',"-DX10C_BASELINE=$repoPath/out/x10c/baseline","-DX10C_VARIANT=$repoPath/out/x10c/variant","-DX1_REPO=$repoPath","-DCMAKE_PREFIX_PATH=$repoPath/out/vcpkg_installed/x64-windows")
  Run "build-$Tag" 'cmake' @('--build',$buildPath,'--config','Release','--parallel','2')
  Copy-Item -LiteralPath "$repoPath/out/vcpkg_installed/x64-windows/bin/z.dll","$repoPath/out/vcpkg_installed/x64-windows/bin/gtest.dll","$repoPath/out/vcpkg_installed/x64-windows/bin/gtest_main.dll" -Destination "$buildPath/Release"
}elseif($Mode -eq 'Tests'){
  $env:PAS_RGB_CLIP_ROOT="$campaignPath/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
  foreach($r in @('baseline','variant')){Run "$r-tests-$Tag" "$buildPath/Release/$($r)_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/$r-tests-$Tag.xml")}
  Run "diagnostic-tests-$Tag" "$buildPath/Release/x10c_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/diagnostic-tests-$Tag.xml")
}elseif($Mode -eq 'ASan'){
  $asanPath=Join-Path $repoPath 'out/x10c/asan'
  Run "asan-configure-$Tag" 'cmake' @('-S',"$repoPath/apps/frame_review/x10c_offline",'-B',$asanPath,'-G','Visual Studio 18 2026','-A','x64','-T','v145','-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275',"-DX10C_BASELINE=$repoPath/out/x10c/baseline","-DX10C_VARIANT=$repoPath/out/x10c/variant","-DX1_REPO=$repoPath","-DCMAKE_PREFIX_PATH=$repoPath/out/vcpkg_installed/x64-windows",'-DX1_SANITIZER_SUPPORT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')
  Run "asan-build-$Tag" 'cmake' @('--build',$asanPath,'--config','Debug','--target','x10c_tests','--parallel','2')
  Copy-Item -LiteralPath "$repoPath/out/vcpkg_installed/x64-windows/debug/bin/gtest.dll","$repoPath/out/vcpkg_installed/x64-windows/debug/bin/gtest_main.dll" -Destination "$asanPath/Debug"
  Run "asan-diagnostic-tests-$Tag" "$asanPath/Debug/x10c_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/asan-diagnostic-tests-$Tag.xml")
}elseif($Mode -eq 'Replay'){
  if(@(Get-ChildItem -LiteralPath $batchPath -Filter '*-replay-command.json').Count -ge 4){throw 'four replay attempts used'}
  $prov=Get-Content -LiteralPath "$batchPath/$Role-provenance.json" -Raw|ConvertFrom-Json
  foreach($e in $prov.instrumented_source_sha256.PSObject.Properties){if((Hash "$($prov.export_root)/$($e.Name)") -ne $e.Value){throw 'source mismatch'}}
  $name="$Role-$Trace-$Tag"
  Run "$name-replay" "$buildPath/Release/$($Role)_replay.exe" @('contact-x10c',"$batchPath/input-manifest.json","$batchPath/$Role-provenance.json","$batchPath/$name",$Trace,'owner','frame-first')
}else{
  if(Test-Path -LiteralPath "$batchPath/full-action-audit-$Tag.json"){throw 'new output required'}
  & "$buildPath/Release/x10c_audit.exe" "$batchPath/baseline-on-1" "$batchPath/variant-on-1" > "$batchPath/full-action-audit-$Tag.json"
  if($LASTEXITCODE -ne 0){throw 'audit failed'}
}
$b=Bytes $batchPath;$o=Bytes "$repoPath/out/x10c";$c=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")
if($b -gt 134217728 -or $o -gt 805306368 -or $c -gt 8589934592){throw 'capacity exceeded'}
Write-Output "batch=$b out=$o campaign+prior=$c"
