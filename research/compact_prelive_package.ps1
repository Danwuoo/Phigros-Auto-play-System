param([ValidatePattern('^[a-z0-9-]{1,16}$')][string]$Attempt='01',
 [string]$MainRoot='C:/Users/wurre/Desktop/Phigros-Auto-play-System')
$ErrorActionPreference='Stop'
$dev=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$MainRoot=[IO.Path]::GetFullPath($MainRoot)
$prior=Get-Content -LiteralPath (Join-Path $dev 'docs/research/zero-miss-20261006/windows/PRELIVE_IGNORED_PACKAGE.json') -Raw|ConvertFrom-Json
function HashFile($path){$s=[IO.File]::OpenRead($path);$sha=[Security.Cryptography.SHA256]::Create();try{[Convert]::ToHexString($sha.ComputeHash($s)).ToLowerInvariant()}finally{$sha.Dispose();$s.Dispose()}}
if((HashFile $prior.archive) -cne $prior.archive_sha256){throw 'source archive changed'}
$head=(& git -C $dev rev-parse HEAD)
$mainHead=(& git -C $MainRoot rev-parse HEAD)
if($head -cne $mainHead -or (& git -C $MainRoot branch --show-current) -ne 'main'){throw 'merge into main first'}
$name='phigros-prelive-compact-20261006-'+$head.Substring(0,7)+'-'+$Attempt+'.zip'
$destination=Join-Path $MainRoot $name;$partial=$destination+'.partial';$checksum=$destination+'.sha256'
$receipt=Join-Path $dev 'docs/research/zero-miss-20261006/windows/PRELIVE_COMPACT_PACKAGE.json'
foreach($p in @($destination,$partial,$checksum,$receipt)){if(Test-Path -LiteralPath $p){throw ('fresh output required '+$p)}}
foreach($p in @($destination,$partial,$checksum)){if([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($p)) -ne $MainRoot){throw 'output escaped main root'}}
$aux='\\?\'+(Join-Path $MainRoot 'apps/runtime_x11_p/aux/CMakeLists.txt').Replace('/','\')
$auxBefore=HashFile $aux
Add-Type -AssemblyName System.IO.Compression
$source=[IO.Compression.ZipFile]::OpenRead($prior.archive)
$stream=$source.GetEntry('_package/FILES.json').Open();$reader=[IO.StreamReader]::new($stream)
try{$manifest=$reader.ReadToEnd()|ConvertFrom-Json}finally{$reader.Dispose()}
function Keep($r){
 switch($r.category){
  'prelive-all-attempts-raw-and-builds' {
   if($r.entry -like '*/out/prelive-20261006/*'){return $r.entry -notlike '*/compiler-object-archive-01/objects.zip'}
   if($r.entry -match '/out/(prelive-current-release-13|prelive-current-debug-05|prelive-current-asan-04|prelive-audit-release-01|prelive-review-release-02)/[^/]+$'){
    return [IO.Path]::GetExtension($r.entry) -in @('.exe','.dll','.map')
   }
   return $false
  }
  'installed-release-debug-third-party-dependencies' {return [IO.Path]::GetExtension($r.entry) -notin @('.lib','.pdb')}
  'process-qualification-io-adapters-and-all-receipts' {return $r.entry -notlike '*/msvc-isolated-probe-01/bin/*'}
  default {return $true}
 }
}
$keep=@($manifest.entries|Where-Object {Keep $_})
$excluded=@($manifest.entries|Where-Object {-not(Keep $_)})
$payload=[long](($keep|Measure-Object bytes -Sum).Sum)
$zipCap=[long]3221225472;$minimumFree=[long]21474836480
if($payload -gt 6442450944 -or (Get-PSDrive C).Free-$payload -lt $minimumFree){throw 'compact capacity preflight'}
$readme=@"
Phigros compact recent development data / 2026-10-06

Merged main source checkpoint: $head
Main remote: https://github.com/Danwuoo/Phigros-Auto-play-System
Original data checkpoint: $($prior.data_source_commit)
Full original package: $([IO.Path]::GetFileName($prior.archive))
Full package SHA256: $($prior.archive_sha256)

This compact ZIP is for current evidence review, offline candidate execution,
and continuing research using Git source plus an existing build environment.
All own raw events/attempts/Journal/timing data, failure results/STOP receipts,
3063 original prefix PNGs and 60 formal regression clips are retained exactly.
Final Release/Debug/ASan EXEs/DLLs/maps and audit/review EXEs are retained.
Installed third-party runtime DLLs, headers, tools, licenses and metadata remain.
FILES.json lists retained entry bytes/SHA; EXCLUDED.json lists omitted old entries.

To reduce size, omit earlier build roots, compiler objects, static/import libs,
PDBs, the archived old compiler objects ZIP, and isolated MSVC compiler binaries.
No original file or full archive was deleted. This is NOT a complete standalone
rebuild environment: third-party CMake package files and vcpkg install status
describe the original full installation, whose library files are absent here.
Use existing verified dependencies or the original full package for rebuilding;
do not adopt the old host's process qualification or restore partial vcpkg data
over a complete installation. MSVC/SDK/CMake/Ninja/PowerShell remain prerequisites.

Extract first to an empty review directory. Payload prefixes correspond to the
development and original main checkout directory names below the Desktop parent.
Verify every retained file before selective restore. Existing STOPs/frozen data
must never be overwritten. Git tracked source/sealed evidence comes from main.
Original recording index covers 7722 frames; this package includes PNG 1..3063,
not the remaining 4659. Other unrelated historical data is outside both scopes.
Absolute paths in old receipts/caches are preserved as historical facts.

Main merge changes repository integration only. Candidate source/binary remains
the same offline current-rails-v1; no new tests or device actions were performed.
Cost/physical gates remain NOT_READY, A/A stopped, A/B unrun, live CLI unconnected.
Chapter Legacy current roster/IN unlock remains unknown, complete IN Miss=0 goal
is incomplete, and no AP prerequisite is added.
"@
$out=[IO.FileStream]::new($partial,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::Read)
$zip=[IO.Compression.ZipArchive]::new($out,[IO.Compression.ZipArchiveMode]::Create,$true)
$buffer=[byte[]]::new(1048576);$clock=[Diagnostics.Stopwatch]::StartNew();$next=0.0;$written=0
function TextEntry($name,$value){$e=$zip.CreateEntry($name,[IO.Compression.CompressionLevel]::Optimal);$s=$e.Open();try{$b=[Text.Encoding]::UTF8.GetBytes($value);$s.Write($b)}finally{$s.Dispose()}}
try{
 foreach($r in $keep){
  $old=$source.GetEntry($r.entry);if($null -eq $old -or $old.Length -ne $r.bytes){throw 'source entry missing'}
  $entry=$zip.CreateEntry($r.entry,[IO.Compression.CompressionLevel]::Optimal)
  $input=$old.Open();$output=$entry.Open();$hash=[Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256)
  try{while(($n=$input.Read($buffer,0,$buffer.Length)) -gt 0){$hash.AppendData($buffer,0,$n);$output.Write($buffer,0,$n)};$digest=[Convert]::ToHexString($hash.GetHashAndReset()).ToLowerInvariant()}
  finally{$hash.Dispose();$output.Dispose();$input.Dispose()}
  if($digest -cne $r.sha256){throw ('source SHA mismatch '+$r.entry)};$written++
  if($out.Length -gt $zipCap -or (Get-PSDrive C).Free -lt $minimumFree){throw 'compact write capacity'}
  if($clock.Elapsed.TotalSeconds -ge $next){Write-Output ('compact '+$written+'/'+$keep.Count+' bytes='+$out.Length);$next=$clock.Elapsed.TotalSeconds+20}
 }
 TextEntry '_package/README.txt' $readme
 TextEntry '_package/FILES.json' (@{schema='pas.compact-retained-payload.v1';merged_main_commit=$head;original_data_commit=$prior.data_source_commit;entries=$keep}|ConvertTo-Json -Depth 15)
 TextEntry '_package/EXCLUDED.json' (@{schema='pas.compact-excluded-payload.v1';reason='Rebuild artifacts and compiler/static dependency bulk excluded; all original files and full archive retained';entries=$excluded}|ConvertTo-Json -Depth 15)
 TextEntry '_package/SOURCE_ARCHIVE.json' ($prior|ConvertTo-Json -Depth 10)
 TextEntry '_package/compact_prelive_package.ps1' ([IO.File]::ReadAllText($PSCommandPath))
 foreach($doc in @('PRELIVE_RESULTS.md','PRELIVE_REPRODUCTION.md','PRELIVE_OPERATION_PACKET.md','PRELIVE_RESULT_TEMPLATE.json')){TextEntry ('_package/'+$doc) ([IO.File]::ReadAllText((Join-Path $dev ('docs/research/zero-miss-20261006/windows/'+$doc))))}
}finally{$zip.Dispose();$out.Flush($true);$out.Dispose();$source.Dispose()}
$verify=[IO.Compression.ZipFile]::OpenRead($partial);$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase);$checked=0;$next=$clock.Elapsed.TotalSeconds
try{
 if($verify.Entries.Count -ne $keep.Count+9){throw 'compact entry count'}
 foreach($e in $verify.Entries){if(-not $seen.Add($e.FullName) -or $e.FullName.Contains('\') -or $e.FullName.Contains(':') -or $e.FullName -match '(^|/)\.\.(/|$)'){throw 'unsafe/duplicate compact entry'}}
 foreach($r in $keep){$e=$verify.GetEntry($r.entry);if($null -eq $e -or $e.Length -ne $r.bytes){throw 'compact entry bytes'};$s=$e.Open();$sha=[Security.Cryptography.SHA256]::Create();try{$digest=[Convert]::ToHexString($sha.ComputeHash($s)).ToLowerInvariant()}finally{$sha.Dispose();$s.Dispose()};if($digest -cne $r.sha256){throw ('compact entry SHA '+$r.entry)};$checked++;if($clock.Elapsed.TotalSeconds -ge $next){Write-Output ('verify '+$checked+'/'+$keep.Count);$next=$clock.Elapsed.TotalSeconds+20}}
}finally{$verify.Dispose()}
if((HashFile $aux) -cne $auxBefore -or (& git -C $MainRoot rev-parse HEAD) -cne $mainHead){throw 'main/AUX changed during packaging'}
if((Get-Item -LiteralPath $partial).Length -ge $prior.archive_bytes -or (Get-Item -LiteralPath $partial).Length -gt $zipCap){throw 'compact did not reduce size'}
Move-Item -LiteralPath $partial -Destination $destination
$zipHash=HashFile $destination
[IO.File]::WriteAllText($checksum,$zipHash+'  '+$name+"`n",[Text.UTF8Encoding]::new($false))
$bytes=[long](Get-Item -LiteralPath $destination).Length
$record=@{schema='pas.compact-package-receipt.v1';utc=[DateTime]::UtcNow.ToString('o');archive=$destination;archive_bytes=$bytes;archive_sha256=$zipHash;full_archive=$prior.archive;full_archive_sha256=$prior.archive_sha256;full_archive_bytes=$prior.archive_bytes;reduction_percent=(1-$bytes/[double]$prior.archive_bytes)*100;merged_main_commit=$head;data_source_commit=$prior.data_source_commit;payload_files=$keep.Count;payload_bytes=$payload;excluded_files=$excluded.Count;all_retained_entries_sha_verified=$checked;original_png_prefix=3063;formal_clips=60;raw_reduced=$false;complete_rebuild_dependencies=$false;old_data_deleted=0;original_aux_sha256=$auxBefore;free_after=(Get-PSDrive C).Free;zip_cap=$zipCap;duration_s=$clock.Elapsed.TotalSeconds;source_recipe_sha256=HashFile $PSCommandPath;device_commands=0;cost_gate='NOT_READY';product_goal_complete=$false}
$b=[Text.Encoding]::UTF8.GetBytes(($record|ConvertTo-Json -Depth 10)+"`n");$s=[IO.FileStream]::new($receipt,[IO.FileMode]::CreateNew);try{$s.Write($b);$s.Flush($true)}finally{$s.Dispose()}
$record|ConvertTo-Json -Depth 8
