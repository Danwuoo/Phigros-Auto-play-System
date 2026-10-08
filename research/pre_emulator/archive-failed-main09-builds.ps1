$ErrorActionPreference='Stop'
$taskRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskOut=Join-Path $taskRepo 'out'
$destination=Join-Path $taskOut 'retained/consolidation-20261008/failed-main09-build-archives'
if(Test-Path -LiteralPath $destination){throw 'fresh archive destination required'}
[IO.Directory]::CreateDirectory($destination)|Out-Null
Add-Type -AssemblyName System.IO.Compression
foreach($mode in @('release','debug','asan')){
 $root=[IO.Path]::GetFullPath((Join-Path $taskOut ('prelive-pre-emulator-'+$mode+'-main09')))
 if([IO.Path]::GetDirectoryName($root) -cne $taskOut -or (Get-Item -LiteralPath $root).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'explicit non-reparse failed main09 build root required'}
 $failed='build-prelive-pre-emulator-'+$mode+'-main09-library-pas-emulator'
 $failedRoot=Join-Path $taskOut ('windows-handoff/'+$failed)
 $facts=(Get-Content -LiteralPath (Join-Path $failedRoot ($failed+'-result.json')) -Raw|ConvertFrom-Json).facts
 if($facts.active_final -ne 0 -or -not $facts.held_all_signaled -or -not $facts.streams_completed -or $facts.exit_code -ne 2){throw 'failed compiler outputs must be quiescent and identified'}
 $proofPath=Join-Path $destination ($mode+'-failed-build-snapshot.json')
 $proof=@{schema='pas.failed-build-preservation.v1';source_commit='ff2b0b5';qualification='FAIL';failure_stage=$failedRoot;configuration=$mode;build_root=$root;files=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object {@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}})}
 [IO.File]::WriteAllText($proofPath,($proof|ConvertTo-Json -Depth 12)+"`n",[Text.UTF8Encoding]::new($false))
 $items=@(Get-ChildItem -LiteralPath $root -File -Recurse)
 if($items.Count -ne $proof.files.Count -or @($items|Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count){throw 'exact archive set required'}
 if(@(Get-Process|Where-Object {$_.Path -and $_.Path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)}).Count){throw 'build binary active'}
 $rows=@()
 foreach($f in $proof.files){
  if(-not $f.path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Get-Item -LiteralPath $f.path).Length -ne $f.bytes -or (Get-FileHash -LiteralPath $f.path).Hash.ToLowerInvariant() -cne $f.sha256){throw 'sealed build changed'}
  $rows+=@(@{relative_path=[IO.Path]::GetRelativePath($root,$f.path).Replace('\','/');bytes=$f.bytes;sha256=$f.sha256})
 }
 $zipPath=Join-Path $destination ($mode+'.zip')
 $stream=[IO.FileStream]::new($zipPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
 $zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
 try{foreach($row in $rows){$entry=$zip.CreateEntry($row.relative_path,[IO.Compression.CompressionLevel]::Fastest);$target=$entry.Open();$input=[IO.File]::OpenRead((Join-Path $root $row.relative_path));try{$input.CopyTo($target)}finally{$input.Dispose();$target.Dispose()}}}finally{$zip.Dispose();$stream.Dispose()}
 $zip=[IO.Compression.ZipFile]::OpenRead($zipPath)
 try{if($zip.Entries.Count -ne $rows.Count){throw 'archive set changed'};foreach($row in $rows){$entry=$zip.GetEntry($row.relative_path);if(-not $entry -or $entry.Length -ne $row.bytes){throw 'archive entry bytes'};$input=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create();try{$actual=[Convert]::ToHexString($sha.ComputeHash($input)).ToLowerInvariant()}finally{$sha.Dispose();$input.Dispose()};if($actual -cne $row.sha256){throw 'archive entry hash'}}}finally{$zip.Dispose()}
 foreach($f in $proof.files){if((Get-Item -LiteralPath $f.path).Length -ne $f.bytes -or (Get-FileHash -LiteralPath $f.path).Hash.ToLowerInvariant() -cne $f.sha256){throw 'original changed during archive'}}
 $receipt=@{schema='pas.recoverable-build-archive.v1';original_root=$root;original_build_proof=$proofPath;original_build_proof_sha256=(Get-FileHash -LiteralPath $proofPath).Hash.ToLowerInvariant();archive_path=$zipPath;archive_bytes=(Get-Item -LiteralPath $zipPath).Length;archive_sha256=(Get-FileHash -LiteralPath $zipPath).Hash.ToLowerInvariant();files=$rows;all_stream_SHAs_verified=$true;raw_evidence_removed=$false;source_removed=$false;historical_receipts_require_archive_restore=$true}
 [IO.File]::WriteAllText(($zipPath+'.verified.json'),($receipt|ConvertTo-Json -Depth 12)+"`n",[Text.UTF8Encoding]::new($false))
 # Only the fixed, resolved, verified compiler-output directory above is removed.
 Remove-Item -LiteralPath $root -Recurse -Force
 Write-Output ($mode+' archived: '+$rows.Count+' verified entries')
}
