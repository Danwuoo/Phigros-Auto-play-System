param([switch]$After)
$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$oldPath=Join-Path $campaignPath 'hold-ownership-x10d-o-bvi'
$batchPath=Join-Path $campaignPath 'hold-ownership-x10d-o-bvi-r1'
Set-Location -LiteralPath $repoPath
function ReadJson($p){Get-Content -LiteralPath $p -Raw|ConvertFrom-Json}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Record($p){$f=Get-Item -LiteralPath $p;[ordered]@{path=$f.FullName;bytes=$f.Length;sha256=(Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant()}}
function Save($p,$v){if(Test-Path -LiteralPath $p){throw "output exists: $p"};[IO.File]::WriteAllText($p,($v|ConvertTo-Json -Depth 25)+"`n",[Text.UTF8Encoding]::new($false))}
$timer=[Diagnostics.Stopwatch]::StartNew();$cache=@{};$errors=[Collections.Generic.List[object]]::new()
function Check($p,$sha,$bytes=$null){
 if($timer.Elapsed.TotalSeconds -gt 180){throw 'maintenance timeout'}
 $full=[IO.Path]::GetFullPath($p)
 if(-not $cache.ContainsKey($full)){$cache[$full]=Record $full}
 $a=$cache[$full]
 if($a.sha256 -ne $sha -or ($null -ne $bytes -and $a.bytes -ne [long]$bytes)){$errors.Add(@{path=$full;expected_sha=$sha;expected_bytes=$bytes;actual=$a})}
}
$head=(& git rev-parse HEAD).Trim();$index=(Get-FileHash -LiteralPath '.git/index').Hash.ToLowerInvariant()
$paths=@(& git -c core.quotepath=false ls-files --cached --others --exclude-standard|Where-Object{$_ -notlike 'research/x10d_o_bvi_r1/*' -and $_ -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI_R1_*'})
$dirty=@(& git -c core.quotepath=false status --porcelain=v1 --untracked-files=all|Where-Object{$_ -notmatch '(research/x10d_o_bvi_r1/|docs/HOLD_OWNERSHIP_X10D_O_BVI_R1_)'})
$controller=ReadJson "$oldPath/controller-review/controller-receipt.json"
if($After){
 $before=ReadJson "$batchPath/protection-before.json"
 foreach($f in $before.files){Check $f.path $f.sha256 $f.bytes}
 if(($paths -join "`n") -ne ($before.old_git_paths -join "`n") -or ($dirty -join "`n") -ne ($before.dirty -join "`n")){$errors.Add(@{reason='original Git paths or dirty changed'})}
}else{
 $integrity=ReadJson "$oldPath/controller-review/integrity.json"
 if($integrity.files.Count -ne 2031){throw '2031 denominator mismatch'}
 foreach($f in $integrity.files){Check $f.path $f.sha256 $f.bytes}
 $rehash2031=$cache.Count
 $ledger=ReadJson "$oldPath/controller-review/artifact-ledger.json"
 Check "$oldPath/controller-review/artifact-ledger.json" $controller.artifact_ledger_sha256
 foreach($f in $ledger.files){Check (Join-Path "$oldPath/controller-review" $f.path) $f.sha256 $f.bytes}
 foreach($f in $ledger.external){Check $f.path $f.sha256 $f.bytes}
 Check "$oldPath/oracle.json" '90f8680c4362d07a718ac2bb7142b37396871b7ffd81523a06ddd8a685ec5425'
 $original=ReadJson "$oldPath/protection-before.json"
 $oldDirty=@($dirty|Where-Object{$_ -notmatch '(research/x10d_o_bvi/|docs/HOLD_OWNERSHIP_X10D_O_BVI|docs/HOLD_OWNERSHIP_X10D_O_4I_CONTROLLER_ACCEPTANCE_)'})
 if(($oldDirty -join "`n") -ne ($original.dirty -join "`n")){$errors.Add(@{reason='original 454-path dirty differs'})}
 if($paths.Count -ne 479){$errors.Add(@{reason='original Git path count';actual=$paths.Count})}
 foreach($p in $paths){$full=Join-Path $repoPath $p;if(-not $cache.ContainsKey($full)){$cache[$full]=Record $full}}
 foreach($f in Get-ChildItem -LiteralPath $oldPath -Recurse -File){if(-not $cache.ContainsKey($f.FullName)){$cache[$f.FullName]=Record $f.FullName}}
}
if($head -ne $controller.head -or $index -ne $controller.index_sha256){$errors.Add(@{reason='HEAD/index mismatch'})}
$formal=@(& git diff HEAD --name-only -- src include CMakeLists.txt)
if($formal.Count){$errors.Add(@{reason='formal diff';paths=$formal})}
$campaign=Bytes $campaignPath;$newRoot=if(Test-Path -LiteralPath $batchPath){Bytes $batchPath}else{0L}
$external=Bytes $PSScriptRoot
foreach($f in Get-ChildItem -LiteralPath (Join-Path $repoPath 'docs') -Filter 'HOLD_OWNERSHIP_X10D_O_BVI_R1_*' -File){$external+=$f.Length}
$aggregate=$campaign+45307809+72115+9546+54056+11851+104033+12289+$external
$out=[long]374705;if(Test-Path -LiteralPath "$repoPath/out/x10d-o-bvi-r1"){$out+=Bytes "$repoPath/out/x10d-o-bvi-r1"}
$free=(Get-PSDrive C).Free
if(-not $After -and $campaign-$newRoot -ne $controller.capacity.campaign_physical){$errors.Add(@{reason='campaign carry mismatch';actual=$campaign-$newRoot;expected=$controller.capacity.campaign_physical})}
if($aggregate -gt 8589934592 -or 4144966+$newRoot+$external -gt 58720256 -or $out -gt 268435456 -or $free -lt 5704253440){$errors.Add(@{reason='capacity'})}
[IO.Directory]::CreateDirectory($batchPath)|Out-Null
$phase=if($After){'after'}else{'before'}
$v=[ordered]@{schema=1;phase=$phase;head=$head;index_sha256=$index;old_git_paths=$paths;dirty=$dirty;rehashed_controller_sources=$rehash2031;protected_count=$cache.Count;files=@($cache.Values|Sort-Object path);mismatches=@($errors.ToArray());controller_receipt=(Record "$oldPath/controller-review/controller-receipt.json");capacity=@{campaign=$campaign;original_campaign=$campaign-$newRoot;r1_batch=$newRoot;r1_external=$external;development_carry=4144966;development_charged=4144966+$newRoot+$external;controller_carry=605130;aggregate=$aggregate;aggregate_remaining=8589934592-$aggregate;out_total=$out;free=$free;minimum_free=5704253440;method='conservative entry logical bytes'}}
Save "$batchPath/protection-$phase.json" $v
Write-Output (@{phase=$phase;rehashed2031=$rehash2031;protected=$cache.Count;errors=$errors.Count;paths=$paths.Count;aggregate=$aggregate;free=$free}|ConvertTo-Json -Compress)
if($errors.Count){throw 'Protection/capacity mismatch: stop'}
