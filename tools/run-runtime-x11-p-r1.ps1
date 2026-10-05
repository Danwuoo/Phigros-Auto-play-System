param([ValidateSet('Build','Tests','Bridge','AA','ABBA','Stress')][string]$Mode,[ValidateSet('B0','B1')][string]$Role='B0',[int]$Attempt=1)
$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$batchPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-r1"
$outPath="$repoPath/out/x11-p-r1"
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Save($name,$value){$p="$batchPath/$name";if(Test-Path $p){throw "existing $name"};[IO.File]::WriteAllText($p,($value|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))}
function Run($name,$exe,[string[]]$a){Save "$name-command.json" @{exe=$exe;arguments=$a;rgb_root=$env:PAS_RGB_CLIP_ROOT};& $exe @a *> "$batchPath/$name.log";$code=$LASTEXITCODE;Save "$name-exit.json" @{exit=$code;log_sha256=(Hash "$batchPath/$name.log")};Write-Output "$name exit=$code";return $code}
function Capacity {function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum};$b=Bytes $batchPath;$o=Bytes $outPath;$c=(Bytes "$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue")+(Bytes "$repoPath/measurements/research-next-20261001");if($b -gt 128MB -or $o -gt 3GB -or $c -gt 8GB){throw 'capacity'};Write-Output "batch=$b out=$o campaign=$c"}
$buildPath="$outPath/build-$Role"
if($Mode -eq 'Build'){
 if(Test-Path "$batchPath/source-binding-before-cost.json"){throw 'freeze prohibits rebuild'}
 $options=@('-S',"$outPath/$Role",'-B',$buildPath,'-G','Visual Studio 18 2026','-A','x64','-T','v145',
 '-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275',
 '-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake',
 '-DVCPKG_MANIFEST_MODE=OFF',"-DVCPKG_INSTALLED_DIR=$repoPath/out/vcpkg_installed",'-DVCPKG_TARGET_TRIPLET=x64-windows',"-DX11_REPO=$repoPath")
 $code=Run "configure-$Role-$Attempt" cmake $options;if($code[-1] -ne 0){throw 'configure'}
 $code=Run "build-$Role-$Attempt" cmake @('--build',$buildPath,'--config','Release','--parallel','2','--target','pas','pas_tests','x11_cost','x11_pending_tests','x11_r1_tests','x11_gate');if($code[-1] -ne 0){throw 'build'}
}elseif($Mode -eq 'Tests'){
 $env:PAS_RGB_CLIP_ROOT="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
 $env:R1_TEST_ROOT="$batchPath/test-artifacts-$Role"
 Run "tests-$Role-original" "$buildPath/Release/pas_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/tests-$Role-original.xml")
 Run "tests-$Role-pending" "$buildPath/Release/x11_pending_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/tests-$Role-pending.xml")
 Run "tests-$Role-r1" "$buildPath/Release/x11_r1_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/tests-$Role-r1.xml")
 Run "provenance-$Role" "$buildPath/Release/pas.exe" @('x11-provenance')
 Run "help-$Role" "$buildPath/Release/pas.exe" @('--help')
}elseif($Mode -eq 'Bridge'){
 Run "bridge-$Role" "$buildPath/Release/x11_cost.exe" @('bridge',"$batchPath/bridge-$Role","$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p/bridge-$Role/public-events.jsonl")
}else{
 if(!(Test-Path "$batchPath/source-binding-before-cost.json")){throw 'bind sources/binaries first'}
 $binding=Get-Content "$batchPath/source-binding-before-cost.json" -Raw|ConvertFrom-Json
 foreach($f in $binding.files){if((Hash $f.path) -ne $f.sha256){throw "binding mismatch $($f.path)"}}
 if($Mode -eq 'AA'){
  if(Test-Path "$batchPath/noise-frozen.json"){throw 'AA frozen'}
  foreach($load in @('rgb','owner')){foreach($i in 1..4){$name="aa-$load-$i";$used=@(Get-ChildItem $batchPath -Filter '*-cost-command.json').Count;if($used -ge 24){throw 'cost budget'}
   Run "$name-cost" "$outPath/build-B0/Release/x11_cost.exe" @('cost',$load,"$batchPath/$name",'normal')
  }}
 }elseif($Mode -eq 'ABBA'){
  if(!(Test-Path "$batchPath/noise-frozen.json")){throw 'freeze AA first'}
  if(!(Get-Content "$batchPath/noise-frozen.json" -Raw|ConvertFrom-Json).noise_gate){throw 'noise insufficient; ABBA forbidden'}
  foreach($load in @('rgb','owner')){foreach($b in 1..2){$i=0;foreach($r in @('B0','B1','B1','B0')){++$i;$name="ab-$load-$b-$i-$r";$used=@(Get-ChildItem $batchPath -Filter '*-cost-command.json').Count;if($used -ge 24){throw 'cost budget'}
   Run "$name-cost" "$outPath/build-$r/Release/x11_cost.exe" @('cost',$load,"$batchPath/$name",'normal')
  }}}
 }else{
  foreach($load in @('rgb','owner')){foreach($r in @('B0','B1')){$name="stress-$load-$r";if(@(Get-ChildItem $batchPath -Filter "stress-*-$r-cost-command.json").Count -ge 2){throw 'stress budget'}
   # The second B1 stress validates the newly written concurrent harness and
   # rebuilt uninstrumented candidate core under ASan; it is not a cost sample.
   $exe=if($r -eq 'B1' -and $load -eq 'owner'){"$outPath/build-asan/Debug/x11_cost.exe"}else{"$outPath/build-$r/Release/x11_cost.exe"}
   Run "$name-cost" $exe @('cost',$load,"$batchPath/$name",'stress')
  }}
 }
}
Capacity
