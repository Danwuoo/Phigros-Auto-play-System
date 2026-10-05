$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$false
$script:X4Repo=(Resolve-Path "$PSScriptRoot/..").Path
$script:X4Campaign=Join-Path $script:X4Repo 'measurements/game-assist/2026-09-30-m0-manual-continue'
$script:X4Batch=Join-Path $script:X4Campaign 'preconfirmation-role-x4'
$script:X4Prior=Join-Path $script:X4Repo 'measurements/research-next-20261001'
function X4-Bytes([string]$root) { if(!(Test-Path -LiteralPath $root)){return [long]0}; return [long](Get-ChildItem -LiteralPath $root -File -Recurse | Measure-Object Length -Sum).Sum }
function X4-Sha([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
function X4-Check([long]$reserve) {
  if((X4-Bytes $script:X4Batch)+$reserve -gt 25165824){throw 'X4 24MiB hard budget'}
  if((X4-Bytes $script:X4Campaign)+(X4-Bytes $script:X4Prior)+$reserve -gt 8589934592){throw 'Campaign 8GiB hard budget'}
}
function X4-Write([string]$path,[string]$value) {
  $absolute=[IO.Path]::GetFullPath($path)
  if(!$absolute.StartsWith($script:X4Batch+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'X4 evidence outside batch'}
  if(Test-Path -LiteralPath $absolute){throw "Preserve existing evidence: $absolute"}
  $bytes=[Text.UTF8Encoding]::new($false).GetBytes($value)
  $lease=[Threading.Mutex]::new($false,'Local\PAS_X1ContactReplayBudget')
  if(!$lease.WaitOne(0)){$lease.Dispose();throw 'batch_writer_busy'}
  try {X4-Check $bytes.Length;[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($absolute))|Out-Null;[IO.File]::WriteAllBytes($absolute,$bytes)}
  finally {$lease.ReleaseMutex();$lease.Dispose()}
}
function X4-Json([string]$path,$value) {X4-Write $path ($value|ConvertTo-Json -Depth 80)}
function X4-Invoke([string]$name,[string]$exe,[string[]]$arguments,[long]$reserve=0) {
  X4-Check (1048576+$reserve)
  $log=Join-Path $script:X4Batch "logs/$name.log"
  if(Test-Path -LiteralPath $log){throw 'Command log must be new'}
  # A replay holds the shared writer lease for its full run. Keep its bounded
  # console log in memory, then write after the child releases that lease.
  $buffer=[Text.StringBuilder]::new();$size=0;$encoding=[Text.UTF8Encoding]::new($false)
  & $exe @arguments 2>&1 | ForEach-Object {
    $line=$_.ToString()+"`n";$n=$encoding.GetByteCount($line)
    if($size+$n -gt 1048576){throw 'X4 log hard cap'}
    [void]$buffer.Append($line);$size+=$n
  }
  $code=$LASTEXITCODE
  X4-Write $log ($buffer.ToString())
  X4-Json (Join-Path $script:X4Batch "commands/$name.json") @{name=$name;exe=$exe;arguments=$arguments;exit_code=$code;log_sha256=(X4-Sha $log)}
  return $code
}
