$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$batchPath="$repoPath/measurements/runtime-cost-x11-p-r3"
$outPath="$repoPath/out/x11-p-r3"
$campaignPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue"
function R3Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function R3Save($name,$value){$p="$batchPath/$name";if(Test-Path -LiteralPath $p){throw "existing $p"};[IO.File]::WriteAllText($p,($value|ConvertTo-Json -Depth 95)+"`n",[Text.UTF8Encoding]::new($false))}
function R3Bytes($p){if(!(Test-Path -LiteralPath $p)){return [long]0};[long](Get-ChildItem -LiteralPath $p -File -Recurse | Measure-Object Length -Sum).Sum}
function R3Capacity {
 $b=R3Bytes $batchPath;$o=R3Bytes $outPath;$c=(R3Bytes $campaignPath)+(R3Bytes "$repoPath/measurements/research-next-20261001");$free=(Get-PSDrive C).Free
 if($b+32MB -gt 2GB -or $o -gt 3GB -or $c -gt 8GB -or $b+$c+32MB -gt 10GB -or $free -lt 5GB+32MB){throw 'R3 capacity'}
 @{batch_bytes=$b;out_bytes=$o;old_campaign_plus_prior_bytes=$c;combined_bytes=$b+$c;disk_free_bytes=$free;controller_reserve_bytes=32MB;batch_limit_bytes=2GB;out_limit_bytes=3GB;old_limit_bytes=8GB;combined_limit_bytes=10GB;free_reserve_bytes=5GB}
}
function R3Run($name,$exe,[string[]]$arguments,[int[]]$expected=@(0)) {
 R3Save "$name-command.json" @{exe=$exe;arguments=$arguments;expected_exit=$expected;utc=[DateTime]::UtcNow.ToString('o');test_root=$env:R3_TEST_ROOT}
 & $exe @arguments *> "$batchPath/$name.log";$code=$LASTEXITCODE
 R3Save "$name-exit.json" @{exit=$code;log_sha256=(R3Hash "$batchPath/$name.log");utc=[DateTime]::UtcNow.ToString('o')}
 Write-Host "$name exit=$code";if($code -notin $expected){throw "$name unexpected exit=$code"};return $code
}
function R3ConfigureArgs($source,$build){@('-S',$source,'-B',$build,'-G','Visual Studio 18 2026','-A','x64','-T','v145','-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275','-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake','-DVCPKG_MANIFEST_MODE=OFF',"-DVCPKG_INSTALLED_DIR=$repoPath/out/vcpkg_installed",'-DVCPKG_TARGET_TRIPLET=x64-windows',"-DR3_REPO=$repoPath")}
