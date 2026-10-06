param([Parameter(Mandatory)][string]$Destination)
$ErrorActionPreference='Stop'
$repo='C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006'
$original='C:\Users\wurre\Desktop\Phigros-Auto-play-System'
$handoff=Join-Path $repo 'out/windows-handoff'
$Destination=[IO.Path]::GetFullPath($Destination)
if(-not $Destination.StartsWith((Join-Path $repo 'docs/research/zero-miss-20261006/windows/'),[StringComparison]::OrdinalIgnoreCase) -or
   (Test-Path -LiteralPath $Destination)){throw 'fresh-export-root'}
function Entry($path) {
  $f=Get-Item -LiteralPath $path
  if($f.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'export-reparse'}
  @{path=$f.FullName;bytes=[long]$f.Length;sha256=(Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
function NewJson($path,$value,[long]$cap=2097152) {
  $b=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 45)+"`n")
  if($b.Length -gt $cap){throw 'export-json-cap'}
  [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))|Out-Null
  $h=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
  try{$h.Write($b);$h.Flush($true)}finally{$h.Dispose()}
}
$cap=67108864
$planned=@(Get-ChildItem -LiteralPath $handoff -File -Recurse|
  Where-Object {$_.Extension -in '.json','.jsonl','.log','.xml'})
$sources=@($planned|ForEach-Object {@{source=$_.FullName;relative='receipts/'+[IO.Path]::GetRelativePath($handoff,$_.FullName)}})
foreach($name in 'bvi-win-release-01','bvi-win-debug-01','bvi-win-asan-01','bvi-win-debug-stack8-01','bvi-win-asan-stack8-01') {
  $root=Join-Path $repo ('out/'+$name+'/results')
  $sources+=@(Get-ChildItem -LiteralPath $root -File -Filter '*.json'|ForEach-Object {
    @{source=$_.FullName;relative='fixture-reports/'+$name+'/'+$_.Name}
  })
}
$sources+=@(@{source=(Join-Path $repo 'out/bridge-win-release-01/prefix-results.json');relative='fixture-reports/initial-prefix-release-01.json'})
foreach($name in 'DELIVERY.json','SHA256SUMS','PATCH_VERIFICATION.txt') {
  $sources+=@{source=('C:/Users/wurre/Desktop/Phigros-handoff-round4-9dc99d6-20261006/'+$name);relative='package/'+$name}
}
$sources+=@{source=$PSCommandPath;relative='export.ps1'}
$plannedBytes=($sources|ForEach-Object {(Get-Item -LiteralPath $_.source).Length}|Measure-Object -Sum).Sum
if($plannedBytes -gt $cap-8388608){throw 'export-byte-reserve'}
[IO.Directory]::CreateDirectory($Destination)|Out-Null
$copies=@(foreach($source in $sources) {
  $entry=Entry $source.source
  $target=[IO.Path]::GetFullPath((Join-Path $Destination $source.relative))
  if(-not $target.StartsWith($Destination+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'export-containment'}
  [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))|Out-Null
  [IO.File]::Copy($entry.path,$target,$false)
  $copy=Entry $target
  if($copy.bytes -ne $entry.bytes -or $copy.sha256 -cne $entry.sha256){throw 'export-copy-sha'}
  @{relative=$source.relative;source=$entry.path;bytes=$entry.bytes;sha256=$entry.sha256}
})
$stages=@(Get-ChildItem -LiteralPath $handoff -Directory|ForEach-Object {
  $state=Join-Path $_.FullName 'state.json'
  if(Test-Path -LiteralPath $state) {
    $s=Get-Content -LiteralPath $state -Raw|ConvertFrom-Json
    $verification=Join-Path $_.FullName ($_.Name+'-verification.json')
    $v=if(Test-Path -LiteralPath $verification){Get-Content -LiteralPath $verification -Raw|ConvertFrom-Json}else{$null}
    $native=if($v.PSObject.Properties.Name -contains 'native_exit'){$v.native_exit}else{$v.facts.exit_code}
    $runner=if($v.PSObject.Properties.Name -contains 'runner_exit'){$v.runner_exit}else{$v.facts.runner_exit}
    @{directory=$_.Name;state=$s.status;reason=$s.reason;native_exit=$native;runner_exit=$runner;
      verification_exit=$v.verification_exit;state_file=(Entry $state)}
  }
})
NewJson (Join-Path $Destination 'stage-index.json') @{schema='pas.windows-local-attempt-index.v1';stages=$stages;
  note='All top-level actual attempts; nested file-only and native qualification controls retained separately. Oracle FAIL native1 with verification0 is not aggregate pass.'}
$oldPaths=@(& rg -l --hidden --no-ignore -g state.json -g '*STOP*' 'STOP' (Join-Path $original 'measurements'))
if($LASTEXITCODE -notin 0,1){throw 'old-STOP-enumeration'}
$oldStops=@($oldPaths|Sort-Object -Unique|ForEach-Object {Entry $_})
$external=@(
 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi/normalized-execution.json',
 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi/oracle.json',
 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/typed-r1.json',
 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/r1-cases.json',
 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/expected-coverage.json',
 'measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p/capability-reference.json',
 'measurements/game-assist/2026-09-30-m0-manual-continue/full-recording36g-01/sessions/manual-session-22885039263800/manifest.json'
 |ForEach-Object {Entry (Join-Path $original $_)})
NewJson (Join-Path $Destination 'external-originals.json') @{schema='pas.windows-original-reference.v1';original_stop_states=$oldStops;
  existing_inputs=$external;originals_modified_by_this_task=$false;historical_capability_not_current_device=$true}
$newSource=@(Get-ChildItem -LiteralPath (Join-Path $repo 'research/bvi_windows'),(Join-Path $repo 'tools/zero_miss_windows') -File -Recurse|ForEach-Object {Entry $_})
$binaries=@(foreach($name in 'win-r4-ninja-release-01','win-r4-ninja-debug-01','win-r4-ninja-asan-04','bvi-win-release-01','bvi-win-debug-stack8-01','bvi-win-asan-stack8-01','bridge-win-release-01','bridge-win-debug-01','bridge-win-asan-01'){
  Get-ChildItem -LiteralPath (Join-Path $repo ('out/'+$name)) -File -Recurse|
    Where-Object {$_.Extension -in '.exe','.dll','.lib' -and $_.FullName -notmatch 'CMakeFiles|asan_support|frozen-fixtures'}|ForEach-Object {Entry $_}
})
NewJson (Join-Path $Destination 'source-and-binary-sha.json') @{schema='pas.windows-offline-compiled-identity.v1';
  development_source_commit='0f81a387676ec67b470f537812113cbe33df8fa0';sources=$newSource;binaries=$binaries;
  closure='Formal pas_core original source plus exact v3 core; complete freezes, CMake argv and linker commands are in receipts. No production BVI hook.'}
$os=Get-CimInstance Win32_OperatingSystem
$cpu=Get-CimInstance Win32_Processor
$runtime='C:/Users/wurre/AppData/Local/Programs/Python/Python310/Lib/site-packages/cmake/data/bin/cmake.exe'
NewJson (Join-Path $Destination 'environment.json') @{schema='pas.windows-offline-host.v1';captured_utc=[DateTime]::UtcNow.ToString('o');
  os=@{caption=$os.Caption;version=$os.Version;build=$os.BuildNumber;total_memory_kib=$os.TotalVisibleMemorySize;free_memory_kib=$os.FreePhysicalMemory};
  cpu=@($cpu|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);cmake=(Entry $runtime);
  compiler_snapshot_manifest=(Entry (Join-Path $handoff 'msvc-isolated-probe-01/tool-snapshot.json'));
  vcpkg_root_environment=$env:VCPKG_ROOT;vcpkg_installed_dir=(Join-Path $original 'out/vcpkg_installed');
  manifest_install=$false;cpu_vision=$false;device_read=$false}
$outBytes=(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -File -Recurse|Measure-Object Length -Sum).Sum
$archiveBytes=(Get-ChildItem -LiteralPath $Destination -File -Recurse|Measure-Object Length -Sum).Sum
if($outBytes -gt 12884901888 -or $archiveBytes -gt $cap){throw 'capacity-final'}
NewJson (Join-Path $Destination 'capacity.json') @{schema='pas.windows-local-capacity.v1';new_out_bytes=$outBytes;
  new_out_hard_cap_bytes=12884901888;archive_bytes_before_index=$archiveBytes;archive_hard_cap_bytes=$cap;
  existing_selected_png_bytes=76971667;new_png_copies=0;new_models=0;free_disk_bytes=(Get-PSDrive C).Free;
  note='Existing originals shared read-only; build products/dependencies/raw PNG are excluded from Git. All failed attempts remain in out.'}
$index=@(Get-ChildItem -LiteralPath $Destination -File -Recurse|Sort-Object FullName|ForEach-Object {
  $e=Entry $_.FullName;@{relative=[IO.Path]::GetRelativePath($Destination,$e.path);bytes=$e.bytes;sha256=$e.sha256}
})
NewJson (Join-Path $Destination 'SHA256_INDEX.json') @{schema='pas.windows-evidence-index.v1';files=$index;excluded_self='SHA256_INDEX.json'}
$finalBytes=(Get-ChildItem -LiteralPath $Destination -File -Recurse|Measure-Object Length -Sum).Sum
if($finalBytes -gt $cap){throw 'archive-final-cap'}
@{copied_attempt_files=$copies.Count;indexed_files=$index.Count;archive_bytes=$finalBytes;new_out_bytes=$outBytes;
  old_STOP_refs=$oldStops.Count}|ConvertTo-Json -Compress
