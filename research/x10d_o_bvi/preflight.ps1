param([switch]$After)
$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'hold-ownership-x10d-o-bvi'
$r4Path=Join-Path $campaignPath 'hold-ownership-x10d-o-observability'
$timer=[Diagnostics.Stopwatch]::StartNew()
function Bound {if($timer.Elapsed.TotalSeconds -gt 180){throw 'maintenance timeout'}}
function Json($p){Get-Content -LiteralPath $p -Raw | ConvertFrom-Json}
function Bytes($p){$sum=[long]0;if(Test-Path -LiteralPath $p){foreach($f in Get-ChildItem -LiteralPath $p -Recurse -File){Bound;$sum+=$f.Length}};return $sum}
function Record($p){Bound;$f=Get-Item -LiteralPath $p;[ordered]@{path=$f.FullName;bytes=$f.Length;sha256=(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}}
function Save($p,$v){if(Test-Path -LiteralPath $p){throw "output exists $p"};[IO.File]::WriteAllText($p,($v|ConvertTo-Json -Depth 24)+"`n",[Text.UTF8Encoding]::new($false))}
Set-Location -LiteralPath $repoPath
$paths=@(& git -c core.quotepath=false ls-files --cached --others --exclude-standard)
$oldPaths=@($paths|Where-Object{$_ -notlike 'research/x10d_o_bvi/*' -and $_ -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI*'})
if($oldPaths.Count -ne 454){throw "existing path count $($oldPaths.Count)"}
$head=(& git rev-parse HEAD).Trim();$index=(Get-FileHash -LiteralPath '.git/index').Hash.ToLowerInvariant()
$dirty=@(& git -c core.quotepath=false status --porcelain=v1 --untracked-files=all | Where-Object{$_ -notmatch '(research/x10d_o_bvi/|docs/HOLD_OWNERSHIP_X10D_O_BVI)'})
$cache=@{};$fails=[Collections.Generic.List[object]]::new();$checks=0
function Check($p,$sha,$bytes=$null){$full=[IO.Path]::GetFullPath($p);if(-not $cache.ContainsKey($full)){$cache[$full]=Record $full};$a=$cache[$full];$script:checks++;if($a.sha256 -ne $sha -or ($null -ne $bytes -and $a.bytes -ne [long]$bytes)){$fails.Add(@{path=$full;expected=$sha;actual=$a})}}
if($After){
 $before=Json "$batchPath/protection-before.json"
 foreach($f in $before.files){Check $f.path $f.sha256 $f.bytes}
 if($head -ne $before.head -or $index -ne $before.index_sha256 -or ($dirty -join "`n") -ne ($before.dirty -join "`n")){$fails.Add(@{reason='HEAD/index/original dirty changed'})}
}else{
 $receipt=Json "$r4Path/controller-review/controller-receipt.json"
 if($head -ne $receipt.head -or $index -ne $receipt.index_sha256){throw '4R source state mismatch'}
 $integrity=Json "$r4Path/controller-review/integrity.json"
 foreach($f in $integrity.files){Check $f.path $f.sha256 $f.bytes}
 foreach($name in @('artifact-ledger.json','controller-review/artifact-ledger.json')){
  $ledger=Json (Join-Path $r4Path $name);$ledgerRoot=Split-Path (Join-Path $r4Path $name)
  foreach($f in $ledger.files){Check (Join-Path $ledgerRoot $f.path) $f.sha256 $f.bytes}
  foreach($f in $ledger.external){Check $f.path $f.sha256 $f.bytes}
 }
 foreach($p in $oldPaths){$full=Join-Path $repoPath $p;if(-not $cache.ContainsKey($full)){$cache[$full]=Record $full}}
 foreach($p in @($r4Path,(Join-Path $campaignPath 'hold-ownership-x10d-o'),(Join-Path $repoPath 'out/x10d-o'))){foreach($f in Get-ChildItem -LiteralPath $p -File -Recurse){if(-not $cache.ContainsKey($f.FullName)){$cache[$f.FullName]=Record $f.FullName}}}
}
$campaign=Bytes $campaignPath;$external=Bytes $PSScriptRoot
foreach($f in Get-ChildItem -LiteralPath (Join-Path $repoPath 'docs') -File -Filter 'HOLD_OWNERSHIP_X10D_O_BVI*'){$external+=$f.Length}
$batch=Bytes $batchPath;$out=Bytes (Join-Path $repoPath 'out/x10d-o-bvi');$free=(Get-PSDrive -Name C).Free
$carry=[long]45307809+72115+9546+54056+11851
$aggregate=$campaign+$carry+$external
if($aggregate -gt 8589934592 -or $batch+$external -gt 58720256 -or $out -gt 268435456 -or $free -lt 5704253440){throw 'capacity bound'}
if($fails.Count){throw ($fails|ConvertTo-Json -Depth 5)}
if(-not (Test-Path -LiteralPath $batchPath)){[IO.Directory]::CreateDirectory($batchPath)|Out-Null}
$phase=if($After){'after'}else{'before'}
$v=[ordered]@{schema=1;phase=$phase;head=$head;index_sha256=$index;old_git_paths=$oldPaths;dirty=$dirty;hash_checks=$checks;protected_count=$cache.Count;mismatches=@($fails.ToArray());files=@($cache.Values|Sort-Object path);capacity=@{campaign_entry_bytes=$campaign;carry_external_prior=$carry;new_external_bytes=$external;development_charged=$batch+$external;aggregate_charged=$aggregate;aggregate_remaining=8589934592-$aggregate;out_bytes=$out;free_bytes=$free;minimum_free=5704253440;method='conservative logical entry lengths'}}
Save "$batchPath/protection-$phase.json" $v
Write-Output (@{phase=$phase;protected=$cache.Count;mismatches=$fails.Count;aggregate=$aggregate;free=$free;elapsed_s=$timer.Elapsed.TotalSeconds}|ConvertTo-Json -Compress)
