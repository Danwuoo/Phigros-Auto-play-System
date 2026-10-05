$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$batchPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1'
$oldPath=Join-Path (Split-Path $batchPath) 'hold-ownership-x10d-o-bvi'
$freezePath=Join-Path $batchPath 'pre-execution-source'
if(Test-Path -LiteralPath $freezePath){throw 'freeze exists'}
function Entry($p){$f=Get-Item -LiteralPath $p;@{path=$f.FullName;bytes=$f.Length;sha256=(Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant()}}
function Save($name,$v){$p=Join-Path $batchPath $name;if(Test-Path -LiteralPath $p){throw 'binding exists'};[IO.File]::WriteAllText($p,($v|ConvertTo-Json -Depth 20)+"`n",[Text.UTF8Encoding]::new($false))}
$s=Get-Content -LiteralPath "$batchPath/specification-freeze.json" -Raw|ConvertFrom-Json
foreach($f in @($s.files)+@($s.contract,$s.protocol,$s.old_oracle)){$actual=Entry $f.path;if($f.sha256 -ne $actual.sha256){throw 'pre-source specification changed'}}
[IO.Directory]::CreateDirectory($freezePath)|Out-Null
foreach($f in Get-ChildItem -LiteralPath $PSScriptRoot -File){Copy-Item -LiteralPath $f.FullName -Destination (Join-Path $freezePath $f.Name)}
$source=@(Get-ChildItem -LiteralPath $PSScriptRoot -File|ForEach-Object{Entry $_.FullName})
$inputs=@();foreach($p in @("$oldPath/normalized-execution.json","$oldPath/oracle.json","$oldPath/typed-execution.json","$repoPath/research/x10d_o_bvi/supplemental.json","$batchPath/typed-r1.json","$batchPath/r1-cases.json","$batchPath/expected-coverage.json","$batchPath/specification-freeze.json")){$inputs+=,(Entry $p)}
$deps=Get-Content -LiteralPath "$oldPath/dependency-manifest.json" -Raw|ConvertFrom-Json
$currentDeps=@();foreach($f in $deps.dependencies){if($f.path -notlike '*out\x10d-o-bvi\*' -and $f.path -notlike '*out/x10d-o-bvi/*'){$a=Entry $f.path;if($a.sha256 -ne $f.sha256 -or $a.bytes -ne $f.bytes){throw 'installed dependency changed'};$currentDeps+=,$a}}
Save 'pre-execution-binding.json' @{phase='source/oracle/protocol frozen before controls or configure';source=$source;source_copy=@(Get-ChildItem -LiteralPath $freezePath -File|ForEach-Object{Entry $_.FullName});inputs=$inputs;dependencies_before=$currentDeps;source_family='BVI-1 only';new_cases=20;schema_controls=4;original_cases=69;original_supplemental=22;utc=[DateTime]::UtcNow.ToString('o');tool_repairs=1;controls_declared=@('control-natural','control-nonzero','control-owned-child');configure_allowance=1;compiled_dependency_closure='not yet compiled; installed fingerprints only';dependency_limits=@('MSVC/Windows SDK/STL/CRT/system DLL complete closure not copied or verified','PowerShell Add-Type/Roslyn assembly closure not claimed','No candidate binary or tests yet')}
Write-Output (@{source=$source.Count;inputs=$inputs.Count;dependency_fingerprints=$currentDeps.Count;controls_executed=0;configure_executed=0;frozen=$true}|ConvertTo-Json -Compress)
