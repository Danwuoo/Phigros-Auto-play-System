param([ValidateSet('Build','Tests','ASan','Replay','Audit')][string]$Mode,[ValidateSet('baseline','variant')][string]$Role='baseline',[ValidateSet('on','off')][string]$Trace='on',[string]$Tag='1')
$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'pending-cancel-x10d-p'
$buildPath=Join-Path $repoPath 'out/x10d-p/build'
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$value){$s=[IO.File]::Open("$batchPath/$name",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$data=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Depth 40)+"`n");$s.Write($data,0,$data.Length)}finally{$s.Dispose()}}
function Run($name,$exe,[string[]]$a,[bool]$allowFailure=$false){Save "$name-command.json" @{exe=$exe;arguments=$a;PAS_RGB_CLIP_ROOT=$env:PAS_RGB_CLIP_ROOT};& $exe @a *> "$batchPath/$name.log";$code=$LASTEXITCODE;Save "$name-exit.json" @{exit=$code;log_sha256=(Hash "$batchPath/$name.log")};Write-Output "$name exit=$code";if($code -ne 0 -and !$allowFailure){throw "$name failed; evidence retained"}}
if($Tag -notmatch '^[a-zA-Z0-9-]{1,24}$'){throw 'tag'}
$options=@('-G','Visual Studio 18 2026','-A','x64','-T','v145','-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275',"-DX10D_BASELINE=$repoPath/out/x10d-p/baseline","-DX10D_VARIANT=$repoPath/out/x10d-p/variant","-DX1_REPO=$repoPath","-DCMAKE_PREFIX_PATH=$repoPath/out/vcpkg_installed/x64-windows")
if($Mode -eq 'Build'){
 if(Test-Path -LiteralPath "$batchPath/source-binding-before-replay.json"){throw 'bound replay binaries cannot rebuild'}
 Run "configure-$Tag" 'cmake' (@('-S',"$repoPath/apps/frame_review/x10d_p_offline",'-B',$buildPath)+$options)
 Run "build-$Tag" 'cmake' @('--build',$buildPath,'--config','Release','--parallel','2')
 Copy-Item -LiteralPath "$repoPath/out/vcpkg_installed/x64-windows/bin/z.dll","$repoPath/out/vcpkg_installed/x64-windows/bin/gtest.dll","$repoPath/out/vcpkg_installed/x64-windows/bin/gtest_main.dll" -Destination "$buildPath/Release"
}elseif($Mode -eq 'Tests'){
 $env:PAS_RGB_CLIP_ROOT="$campaignPath/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
 Run "baseline-original-$Tag" "$buildPath/Release/baseline_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/baseline-original-$Tag.xml")
 Run "baseline-red-$Tag" "$buildPath/Release/baseline_pending_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/baseline-red-$Tag.xml") $true
 Run "variant-original-$Tag" "$buildPath/Release/variant_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/variant-original-$Tag.xml") $true
 Run "variant-green-$Tag" "$buildPath/Release/variant_pending_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/variant-green-$Tag.xml") $true
}elseif($Mode -eq 'ASan'){
 $asanPath="$repoPath/out/x10d-p/asan"
 Run "asan-configure-$Tag" 'cmake' (@('-S',"$repoPath/apps/frame_review/x10d_p_offline",'-B',$asanPath)+$options+@('-DX1_SANITIZER_SUPPORT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include'))
 Run "asan-build-$Tag" 'cmake' @('--build',$asanPath,'--config','Debug','--target','variant_tests','variant_pending_tests','--parallel','2')
 Copy-Item -LiteralPath "$repoPath/out/vcpkg_installed/x64-windows/debug/bin/zd.dll","$repoPath/out/vcpkg_installed/x64-windows/debug/bin/gtest.dll","$repoPath/out/vcpkg_installed/x64-windows/debug/bin/gtest_main.dll" -Destination "$asanPath/Debug"
 $env:PAS_RGB_CLIP_ROOT="$campaignPath/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
 Run "asan-original-$Tag" "$asanPath/Debug/variant_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/asan-original-$Tag.xml") $true
 Run "asan-green-$Tag" "$asanPath/Debug/variant_pending_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/asan-green-$Tag.xml") $true
}elseif($Mode -eq 'Replay'){
 if(@(Get-ChildItem -LiteralPath $batchPath -Filter '*-replay-command.json').Count -ge 4){throw 'four attempts exhausted'}
 $binding=Get-Content -LiteralPath "$batchPath/source-binding-before-replay.json" -Raw|ConvertFrom-Json
 foreach($f in $binding.export_files){if((Hash "$repoPath/out/x10d-p/$($f.role)/$($f.path)") -ne $f.sha256){throw 'export changed'}}
 foreach($f in $binding.sources){if((Hash "$repoPath/$($f.path)") -ne $f.sha256){throw 'source changed'}}
 foreach($f in $binding.binaries){if((Hash "$repoPath/$($f.path)") -ne $f.sha256){throw 'binary changed'}}
 Run "$Role-$Trace-$Tag-replay" "$buildPath/Release/$($Role)_replay.exe" @('contact-x10c',"$batchPath/input-manifest.json","$batchPath/$Role-provenance.json","$batchPath/$Role-$Trace-$Tag",$Trace,'owner','frame-first')
}else{
 Run "action-audit-$Tag" "$buildPath/Release/x10d_audit.exe" @("$batchPath/baseline-on-1","$batchPath/variant-on-1")
 [IO.File]::Copy("$batchPath/action-audit-$Tag.log","$batchPath/full-action-audit-$Tag.json",$false)
}
$b=Bytes $batchPath;$o=Bytes "$repoPath/out/x10d-p";$c=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")
if($b -gt 134217728 -or $o -gt 805306368 -or $c -gt 8589934592){throw 'capacity exceeded'}
Write-Output "batch=$b out=$o campaign+prior=$c"
