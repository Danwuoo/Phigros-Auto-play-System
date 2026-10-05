$ErrorActionPreference='Stop';$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$batchPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-r1";$buildPath="$repoPath/out/x11-p-r1/negative"
function Run($name,$exe,[string[]]$a){if(Test-Path "$batchPath/$name-command.json"){throw 'existing command'};[IO.File]::WriteAllText("$batchPath/$name-command.json",(@{exe=$exe;arguments=$a}|ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false));& $exe @a *> "$batchPath/$name.log";$code=$LASTEXITCODE;[IO.File]::WriteAllText("$batchPath/$name-exit.json",(@{exit=$code;log_sha256=(Get-FileHash "$batchPath/$name.log").Hash.ToLowerInvariant()}|ConvertTo-Json),[Text.UTF8Encoding]::new($false));if($code){throw $name}}
Copy-Item "$repoPath/out/vcpkg_installed/x64-windows/bin/z.dll" "$buildPath/Release"
Run 'release-negatives' "$buildPath/Release/r1_negative.exe" @("$batchPath/release-negatives")
$set=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($f in Get-ChildItem $buildPath -Filter '*.read.1.tlog' -File -Recurse){foreach($line in [IO.File]::ReadAllLines($f.FullName)){if($line -match '^[A-Z]:\\' -and (Test-Path -LiteralPath $line -PathType Leaf)){[void]$set.Add($line)}}}
foreach($p in @("$buildPath/Release/r1_negative.exe","$buildPath/Release/z.dll","$buildPath/CMakeCache.txt","$buildPath/r1_negative.vcxproj")){[void]$set.Add($p)}
$files=@();foreach($p in $set|Sort-Object){$files+=@{path=$p;bytes=(Get-Item -LiteralPath $p).Length;sha256=(Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant()}}
[IO.File]::WriteAllText("$batchPath/negative-tool-freeze.json",(@{files=$files;clock='FakeClock; not latency sample';scope='raw per-call negative receipt/release serialization, linked existing frozen B1 core; no strategy/meter rebuild';additional_cost_runs=0;additional_stress_runs=0}|ConvertTo-Json -Depth 15),[Text.UTF8Encoding]::new($false))
