$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$false
$script:X2Repo=(Resolve-Path "$PSScriptRoot/..").Path
$script:X2Campaign=Join-Path $script:X2Repo 'measurements/game-assist/2026-09-30-m0-manual-continue'
$script:X2Batch=Join-Path $script:X2Campaign 'confirmed-preserve-x2'
$script:X2Prior=Join-Path $script:X2Repo 'measurements/research-next-20261001'
function X2-Bytes([string]$root) {
  if(!(Test-Path -LiteralPath $root)){return [long]0}
  return [long](Get-ChildItem -LiteralPath $root -File -Recurse | Measure-Object Length -Sum).Sum
}
function X2-Sha([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
function X2-Check([long]$reserve) {
  if((X2-Bytes $script:X2Batch)+$reserve -gt 67108864){throw 'X2 64MiB hard budget'}
  if((X2-Bytes $script:X2Campaign)+(X2-Bytes $script:X2Prior)+$reserve -gt 8589934592){throw 'Campaign 8GiB hard budget'}
}
function X2-Write([string]$path,[string]$text) {
  $absolute=[IO.Path]::GetFullPath($path)
  if(!$absolute.StartsWith($script:X2Batch+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'X2 evidence outside batch'}
  if(Test-Path -LiteralPath $absolute){throw "Preserve existing evidence: $absolute"}
  $bytes=[Text.UTF8Encoding]::new($false).GetBytes($text)
  $lease=[Threading.Mutex]::new($false,'Local\PAS_X1ContactReplayBudget')
  if(!$lease.WaitOne(0)){ $lease.Dispose();throw 'batch_writer_busy' }
  try {X2-Check $bytes.Length;[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($absolute))|Out-Null;[IO.File]::WriteAllBytes($absolute,$bytes)}
  finally {$lease.ReleaseMutex();$lease.Dispose()}
}
function X2-Json([string]$path,$value) { X2-Write $path ($value|ConvertTo-Json -Depth 60) }
# The replay reserves 2MiB outside its stream limit; each command log is capped
# before every append and commands execute sequentially. No unbounded redirect.
function X2-Invoke([string]$name,[string]$exe,[string[]]$arguments,[long]$extraReserve=0) {
  X2-Check (1048576+$extraReserve)
  $log=Join-Path $script:X2Batch "logs/$name.log"
  if(Test-Path -LiteralPath $log){throw 'Command log must be new'}
  [IO.Directory]::CreateDirectory((Split-Path -Parent $log))|Out-Null
  [IO.File]::WriteAllText($log,'',[Text.UTF8Encoding]::new($false))
  $size=0;$encoding=[Text.UTF8Encoding]::new($false)
  & $exe @arguments 2>&1 | ForEach-Object {
    $line=$_.ToString()+"`n";$size+=$encoding.GetByteCount($line)
    if($size -gt 1048576){throw 'X2 command log hard cap'}
    [IO.File]::AppendAllText($log,$line,$encoding)
  }
  $code=$LASTEXITCODE
  X2-Json (Join-Path $script:X2Batch "commands/$name.json") @{name=$name;exe=$exe;arguments=$arguments;exit_code=$code;log_sha256=(X2-Sha $log)}
  return $code
}
