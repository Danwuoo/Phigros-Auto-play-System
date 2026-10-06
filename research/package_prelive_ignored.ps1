param([ValidatePattern('^[a-z0-9-]{1,16}$')][string]$Attempt='01',
 [string]$MainRoot='C:/Users/wurre/Desktop/Phigros-Auto-play-System')
$ErrorActionPreference='Stop'
$dev=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$MainRoot=[IO.Path]::GetFullPath($MainRoot)
if($dev -eq $MainRoot){throw 'separate development and main roots required'}
$dataHead=(& git -C $dev rev-parse HEAD)
$branch=(& git -C $dev branch --show-current)
$mainHead=(& git -C $MainRoot rev-parse HEAD)
$mainDirty=@(& git -C $MainRoot status --short)
if($branch -ne 'codex/prelive-preparation-20261006'){throw 'unexpected development branch'}
if((& git -C $MainRoot branch --show-current) -ne 'main'){throw 'destination is not main checkout'}
$name='phigros-prelive-ignored-20261006-'+$dataHead.Substring(0,7)+'-'+$Attempt+'.zip'
$destination=Join-Path $MainRoot $name
$partial=$destination+'.partial'
$checksum=$destination+'.sha256'
$receipt=Join-Path $dev 'docs/research/zero-miss-20261006/windows/PRELIVE_IGNORED_PACKAGE.json'
foreach($p in @($destination,$partial,$checksum,$receipt)){if(Test-Path -LiteralPath $p){throw ('fresh output required '+$p)}}
foreach($p in @($destination,$partial,$checksum)){if([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($p)) -ne $MainRoot){throw 'destination escaped main root'}}
$pkg=Join-Path $dev 'out/prelive-20261006'
$seal=Join-Path $dev 'docs/research/zero-miss-20261006/windows/prelive-evidence-01'
$expected=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
$known=Get-Content -LiteralPath (Join-Path $seal 'external-new-out.json') -Raw|ConvertFrom-Json
foreach($e in $known.files){$expected[$e.path]=$e.sha256}
$closure=Get-Content -LiteralPath (Join-Path $seal 'closure-final.json') -Raw|ConvertFrom-Json
foreach($e in $closure.appended_external_files){$expected[$e.path]=$e.sha256}
$selection=Get-Content -LiteralPath (Join-Path $pkg 'full-prefix-selection.json') -Raw|ConvertFrom-Json
foreach($e in $selection.frames){$expected[$e.path]=$e.sha256}
$files=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
function AddFile($root,$label,$path,$category){
 $absolute=[IO.Path]::GetFullPath($path)
 if(-not $absolute.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'source escaped root'}
 $f=Get-Item -LiteralPath $absolute
 if($f.Attributes -band [IO.FileAttributes]::ReparsePoint){throw ('reparse file not packaged '+$absolute)}
 $relative=[IO.Path]::GetRelativePath($root,$absolute).Replace('\','/')
 if($relative -notmatch '^(out|measurements)/'){throw 'only ignored output/data payload allowed'}
 $entry=$label+'/'+$relative
 if($entry -match '(^|/)\.\.(/|$)' -or $entry.Contains(':')){throw 'unsafe entry path'}
 if(-not $files.ContainsKey($entry)){$files.Add($entry,@{source=$absolute;entry=$entry;bytes=[long]$f.Length;category=$category;root=$root;relative=$relative;stamp=$f.LastWriteTimeUtc.Ticks})}
}
function AddTree($root,$label,$relative,$category){$dir=Join-Path $root $relative;if(-not(Test-Path -LiteralPath $dir)){throw ('required directory missing '+$dir)};foreach($f in Get-ChildItem -LiteralPath $dir -File -Recurse){AddFile $root $label $f.FullName $category}}
foreach($dir in Get-ChildItem -LiteralPath (Join-Path $dev 'out') -Directory|Where-Object Name -like 'prelive*'){AddTree $dev 'PAS-zero-miss-r4-20261006' ('out/'+$dir.Name) 'prelive-all-attempts-raw-and-builds'}
AddTree $dev 'PAS-zero-miss-r4-20261006' 'out/windows-handoff' 'process-qualification-io-adapters-and-all-receipts'
AddTree $dev 'PAS-zero-miss-r4-20261006' 'out/win-r4-ninja-release-01/generated' 'frozen-generated-grpc-headers'
AddTree $MainRoot 'Phigros-Auto-play-System' 'out/vcpkg_installed/x64-windows' 'installed-release-debug-third-party-dependencies'
foreach($f in Get-ChildItem -LiteralPath (Join-Path $MainRoot 'out/vcpkg_installed/vcpkg') -File){AddFile $MainRoot 'Phigros-Auto-play-System' $f.FullName 'vcpkg-install-status'}
AddTree $MainRoot 'Phigros-Auto-play-System' 'out/vcpkg_installed/vcpkg/info' 'vcpkg-package-file-lists'
AddTree $MainRoot 'Phigros-Auto-play-System' 'measurements/game-assist/manual-session-3906621440800/pixel-clips' 'original-60-formal-regression-clips'
AddTree $MainRoot 'Phigros-Auto-play-System' 'measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p' 'historical-runtime-reference-evidence'
foreach($e in $selection.frames){AddFile $MainRoot 'Phigros-Auto-play-System' $e.path 'original-current-prefix-png'}
AddFile $MainRoot 'Phigros-Auto-play-System' $selection.index_path 'full-original-index-prefix-images-only'
foreach($f in Get-ChildItem -LiteralPath ([IO.Path]::GetDirectoryName($selection.index_path)) -File){AddFile $MainRoot 'Phigros-Auto-play-System' $f.FullName 'original-recording-metadata'}
$ordered=@($files.Values|Sort-Object entry)
if($ordered.Count -gt 30000){throw 'entry cap'}
$total=[long](($ordered|Measure-Object bytes -Sum).Sum)
$payloadCap=[long]34359738368
$zipCap=[long]17179869184
$minimumFree=[long]21474836480
if($total -gt $payloadCap -or (Get-PSDrive C).Free - $total -lt $minimumFree){throw 'package capacity preflight'}
foreach($root in @($dev,$MainRoot)){
 $rels=@($ordered|Where-Object root -eq $root|ForEach-Object relative)
 $ignored=@($rels|& git -C $root check-ignore --no-index --stdin)
 if($LASTEXITCODE -ne 0 -or $ignored.Count -ne $rels.Count){throw ('non-ignored payload '+$root)}
}
function HashFile($path){$fs=[IO.File]::OpenRead($path);$sha=[Security.Cryptography.SHA256]::Create();try{[Convert]::ToHexString($sha.ComputeHash($fs)).ToLowerInvariant()}finally{$sha.Dispose();$fs.Dispose()}}
$aux='\\?\'+(Join-Path $MainRoot 'apps/runtime_x11_p/aux/CMakeLists.txt').Replace('/','\')
$auxBefore=HashFile $aux
$readme=@"
Phigros recent ignored development payload / 2026-10-06

Data/source checkpoint: $dataHead
Development branch: $branch
Remote: https://github.com/Danwuoo/Phigros-Auto-play-System
Original main checkpoint: $mainHead

This ZIP complements Git. Get the pushed development branch for tracked source,
tests, sealed evidence and documentation. This ZIP contains ignored raw data,
all prelive build/attempt roots, original inputs and installed dependencies.
The packaging recipe itself is included below; its final receipt is committed
after successful archive verification. It does not change the candidate.

Extract first into an empty review directory. Two top-level payload prefixes map
to the two original checkout directory names below their parent Desktop folder:
PAS-zero-miss-r4-20261006/ -> development checkout
Phigros-Auto-play-System/ -> original main checkout
Do not overwrite an existing checkout, STOP, frozen receipt or source file.
Before selective restoration, check every file against FILES.json (SHA256/bytes).

Full current prefix: original PNG ordinals 1..3063; all 60 formal regression clips.
The original index describes 7722 frames, but PNGs 3064..7722 are deliberately
outside this recent-use package. Other historical captures/build roots are not
included. Full own timings, events, attempts and Journal data are preserved;
failed runs and STOPs are included. Earlier objects remain in their verified
objects.zip with object-bindings.json. No raw timing rows are downsampled.

Installed vcpkg x64-windows includes Release/Debug libraries, DLLs, headers,
PDBs, CMake package files, tools and licenses. vcpkg status/info is included;
old build/download staging is omitted. MSVC/Windows SDK/CMake/Ninja/PowerShell
installations remain host prerequisites. The isolated MSVC tool snapshot is
historical evidence, not a portable replacement for those installations.
Caches and receipts contain original absolute paths. Do not silently rewrite
frozen files or adopt an old host's qualification; a new host needs fresh roots,
dependency bindings and process qualification. There are no device commands.

Current state: offline current-rails-v1 integration and regressions completed;
A/A stopped at Hold zero-actions, A/B unrun, cost/physical gates NOT_READY.
No live CLI hook or accepted gameplay adoption. Chapter Legacy current roster /
IN unlock and complete IN Miss=0 goal remain unknown/incomplete; no AP condition.
"@
Add-Type -AssemblyName System.IO.Compression
$outStream=[IO.FileStream]::new($partial,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::Read)
$zip=[IO.Compression.ZipArchive]::new($outStream,[IO.Compression.ZipArchiveMode]::Create,$true)
$buffer=[byte[]]::new(1048576)
$rows=[Collections.Generic.List[object]]::new()
function TextEntry($name,$text){$e=$zip.CreateEntry($name,[IO.Compression.CompressionLevel]::Fastest);$stream=$e.Open();try{$b=[Text.Encoding]::UTF8.GetBytes($text);$stream.Write($b)}finally{$stream.Dispose()}}
$clock=[Diagnostics.Stopwatch]::StartNew();$next=0.0
try{
 foreach($f in $ordered){
  $entry=$zip.CreateEntry($f.entry,[IO.Compression.CompressionLevel]::Fastest)
  $input=[IO.FileStream]::new($f.source,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
  $output=$entry.Open();$hash=[Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256);$written=[long]0
  try{while(($n=$input.Read($buffer,0,$buffer.Length)) -gt 0){$hash.AppendData($buffer,0,$n);$output.Write($buffer,0,$n);$written+=$n};$digest=[Convert]::ToHexString($hash.GetHashAndReset()).ToLowerInvariant()}
  finally{$hash.Dispose();$output.Dispose();$input.Dispose()}
  $after=Get-Item -LiteralPath $f.source
  if($written -ne $f.bytes -or $after.Length -ne $f.bytes -or $after.LastWriteTimeUtc.Ticks -ne $f.stamp){throw ('source changed '+$f.source)}
  if($expected.ContainsKey($f.source) -and $expected[$f.source] -cne $digest){throw ('frozen source mismatch '+$f.source)}
  $rows.Add(@{entry=$f.entry;source=$f.source;bytes=$written;sha256=$digest;category=$f.category;ignored=$true})
  if($outStream.Length -gt $zipCap -or (Get-PSDrive C).Free -lt $minimumFree){throw 'package capacity during write'}
  if($clock.Elapsed.TotalSeconds -ge $next){Write-Output ('compress '+$rows.Count+'/'+$ordered.Count+' bytes='+$outStream.Length);$next=$clock.Elapsed.TotalSeconds+20}
 }
 TextEntry '_package/README.txt' $readme
 TextEntry '_package/FILES.json' (@{schema='pas.recent-ignored-payload.v1';data_source_commit=$dataHead;branch=$branch;main_head=$mainHead;payload_files=$rows.Count;payload_bytes=$total;file_bytes_only=$true;png_ordinals=@(1,3063);formal_clips=60;entries=$rows}|ConvertTo-Json -Depth 15)
 TextEntry '_package/.gitignore-main.txt' ([IO.File]::ReadAllText((Join-Path $MainRoot '.gitignore')))
 TextEntry '_package/.gitignore-development.txt' ([IO.File]::ReadAllText((Join-Path $dev '.gitignore')))
 TextEntry '_package/package_prelive_ignored.ps1' ([IO.File]::ReadAllText($PSCommandPath))
 foreach($doc in @('PRELIVE_RESULTS.md','PRELIVE_REPRODUCTION.md','PRELIVE_OPERATION_PACKET.md','PRELIVE_RESULT_TEMPLATE.json')){TextEntry ('_package/'+$doc) ([IO.File]::ReadAllText((Join-Path $dev ('docs/research/zero-miss-20261006/windows/'+$doc))))}
}finally{$zip.Dispose();$outStream.Flush($true);$outStream.Dispose()}
if((Get-Item -LiteralPath $partial).Length -gt $zipCap){throw 'ZIP cap'}
$check=[IO.Compression.ZipFile]::OpenRead($partial)
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$checked=0;$next=$clock.Elapsed.TotalSeconds
try{
 if($check.Entries.Count -ne $rows.Count+9){throw 'ZIP entry count'}
 foreach($e in $check.Entries){if(-not $seen.Add($e.FullName) -or $e.FullName.Contains('\') -or $e.FullName -match '(^|/)\.\.(/|$)' -or $e.FullName.Contains(':')){throw 'ZIP unsafe or duplicate path'}}
 foreach($r in $rows){$e=$check.GetEntry($r.entry);if($null -eq $e -or $e.Length -ne $r.bytes){throw 'ZIP entry bytes'};$s=$e.Open();$sha=[Security.Cryptography.SHA256]::Create();try{$digest=[Convert]::ToHexString($sha.ComputeHash($s)).ToLowerInvariant()}finally{$sha.Dispose();$s.Dispose()};if($digest -cne $r.sha256){throw ('ZIP entry SHA '+$r.entry)};$checked++;if($clock.Elapsed.TotalSeconds -ge $next){Write-Output ('verify '+$checked+'/'+$rows.Count);$next=$clock.Elapsed.TotalSeconds+20}}
 $manifestEntry=$check.GetEntry('_package/FILES.json');$s=$manifestEntry.Open();$sha=[Security.Cryptography.SHA256]::Create();try{$manifestSha=[Convert]::ToHexString($sha.ComputeHash($s)).ToLowerInvariant()}finally{$sha.Dispose();$s.Dispose()}
}finally{$check.Dispose()}
if((HashFile $aux) -cne $auxBefore -or (& git -C $MainRoot rev-parse HEAD) -cne $mainHead){throw 'original tracked state changed'}
Move-Item -LiteralPath $partial -Destination $destination
$zipHash=HashFile $destination
$zipBytes=[long](Get-Item -LiteralPath $destination).Length
[IO.File]::WriteAllText($checksum,$zipHash+'  '+$name+"`n",[Text.UTF8Encoding]::new($false))
$receiptData=@{schema='pas.recent-ignored-archive-receipt.v1';utc=[DateTime]::UtcNow.ToString('o');archive=$destination;archive_bytes=$zipBytes;archive_sha256=$zipHash;manifest_entry='_package/FILES.json';manifest_sha256=$manifestSha;data_source_commit=$dataHead;branch=$branch;main_head=$mainHead;main_dirty_before=$mainDirty;main_dirty_after=@(& git -C $MainRoot status --short);original_aux_sha256=$auxBefore;payload_files=$rows.Count;zip_entries=$rows.Count+9;payload_bytes=$total;all_payload_entries_sha_verified=$checked;ignored_paths_verified=$true;original_png_prefix=3063;formal_clips=60;historical_remaining_png_not_included=4659;archive_cap=$zipCap;payload_cap=$payloadCap;minimum_free=$minimumFree;free_after=(Get-PSDrive C).Free;duration_s=$clock.Elapsed.TotalSeconds;device_commands=0;cost_gate='NOT_READY';live_cli_hook=$false;product_goal_complete=$false;source_recipe_sha256=HashFile $PSCommandPath}
$b=[Text.Encoding]::UTF8.GetBytes(($receiptData|ConvertTo-Json -Depth 10)+"`n");$s=[IO.FileStream]::new($receipt,[IO.FileMode]::CreateNew);try{$s.Write($b);$s.Flush($true)}finally{$s.Dispose()}
$receiptData|ConvertTo-Json -Depth 8
