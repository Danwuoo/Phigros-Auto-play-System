param([Parameter(Mandatory)][string]$BuildRoot,[Parameter(Mandatory)][string]$ArchiveRoot)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$root=[IO.Path]::GetFullPath($BuildRoot);$archives=[IO.Path]::GetFullPath($ArchiveRoot)
$out=[IO.Path]::GetFullPath((Join-Path $repo 'out'))
if([IO.Path]::GetDirectoryName($root) -cne $out -or [IO.Path]::GetFileName($root) -notmatch '^prelive-current-(release|debug|asan)-win0[12]$'){throw 'explicit-task-build-root-required'}
$archiveScope='C:\Users\wurre\Downloads\PAS-readiness-retained-builds-20261007'
if(-not $archives.Equals($archiveScope,[StringComparison]::OrdinalIgnoreCase) -and -not $archives.StartsWith($archiveScope+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'retained-build-archive-scope'}
if((Get-Item -LiteralPath $root).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'build-root-reparse'}
if(@(Get-Process|Where-Object {$_.Path -and $_.Path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)}).Count){throw 'build-binary-active'}
[IO.Directory]::CreateDirectory($archives)|Out-Null
$zipPath=Join-Path $archives ((Split-Path $root -Leaf)+'.zip');$receiptPath=$zipPath+'.verified.json'
if((Test-Path -LiteralPath $zipPath) -or (Test-Path -LiteralPath $receiptPath)){throw 'fresh-retained-archive-required'}
$rows=@();$files=@(Get-ChildItem -LiteralPath $root -Recurse -File|Sort-Object FullName)
foreach($file in $files){
 if($file.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'build-member-reparse'}
 if($file.Name -match '(attempts|journal|events|rows)\.(json|jsonl)$'){throw 'raw-evidence-must-remain-in-place'}
 $rows+=@(@{relative_path=[IO.Path]::GetRelativePath($root,$file.FullName).Replace('\','/');bytes=[long]$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()})
}
$stream=[IO.FileStream]::new($zipPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
$archive=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
try{foreach($row in $rows){$entry=$archive.CreateEntry($row.relative_path,[IO.Compression.CompressionLevel]::Fastest);$target=$entry.Open();$input=[IO.File]::OpenRead((Join-Path $root $row.relative_path));try{$input.CopyTo($target)}finally{$input.Dispose();$target.Dispose()}}}finally{$archive.Dispose();$stream.Dispose()}
$archive=[IO.Compression.ZipFile]::OpenRead($zipPath)
try{if($archive.Entries.Count -ne $rows.Count){throw 'archive-entry-count'};foreach($row in $rows){$entry=$archive.GetEntry($row.relative_path);if(-not $entry -or $entry.Length -ne $row.bytes){throw 'archive-member-size'};$input=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create();try{$actual=[Convert]::ToHexString($sha.ComputeHash($input)).ToLowerInvariant()}finally{$sha.Dispose();$input.Dispose()};if($actual -cne $row.sha256){throw 'archive-member-SHA'}}}finally{$archive.Dispose()}
# Recheck originals after the archive stream check, before removing this one
# explicitly resolved compiler-output directory. All test raw/receipts stay.
foreach($row in $rows){$path=Join-Path $root $row.relative_path;if((Get-Item -LiteralPath $path).Length -ne $row.bytes -or (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -cne $row.sha256){throw 'build-changed-during-archive'}}
$receipt=@{schema='pas.recoverable-build-archive.v1';original_root=$root;archive_path=$zipPath;archive_sha256=(Get-FileHash -LiteralPath $zipPath).Hash.ToLowerInvariant();archive_bytes=(Get-Item -LiteralPath $zipPath).Length;
 files=$rows;all_stream_SHAs_verified=$true;raw_evidence_removed=$false;source_removed=$false;historical_receipts_require_archive_restore=$true}
[IO.File]::WriteAllText($receiptPath,($receipt|ConvertTo-Json -Depth 10)+"`n",[Text.UTF8Encoding]::new($false))
# Both absolute paths were validated against this task's fixed out root above.
Remove-Item -LiteralPath $root -Recurse -Force
[pscustomobject]@{receipt=$receiptPath;files=$rows.Count;original_bytes=[long](($rows|Measure-Object bytes -Sum).Sum);archive_bytes=$receipt.archive_bytes}|ConvertTo-Json -Compress
