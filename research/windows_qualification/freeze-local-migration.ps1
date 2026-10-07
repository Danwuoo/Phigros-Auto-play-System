param([Parameter(Mandatory)][string]$PacketMigration,
 [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$PacketSha256,
 [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$SourceCommit,
 [Parameter(Mandatory)][string]$OutputRoot)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$packet=[IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($PacketMigration))
$destination=[IO.Path]::GetFullPath($OutputRoot)
if(-not $destination.StartsWith((Join-Path $repo 'out')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'local-migration-output-scope'}
if(Test-Path -LiteralPath $destination){throw 'fresh-local-migration-root'}
if((Get-FileHash -LiteralPath $PacketMigration).Hash.ToLowerInvariant() -cne $PacketSha256){throw 'original-packet-seal'}
$head=(& git -C $repo rev-parse HEAD)
if($LASTEXITCODE -ne 0 -or $head -cne $SourceCommit -or (& git -C $repo status --porcelain)){throw 'clean-committed-local-source-required'}
$migration=Get-Content -LiteralPath $PacketMigration -Raw|ConvertFrom-Json
& git -C $repo merge-base --is-ancestor $migration.target_commit $SourceCommit
if($LASTEXITCODE -ne 0){throw 'local-source-must-descend-from-handoff'}
[IO.Directory]::CreateDirectory((Join-Path $destination 'migration-blobs'))|Out-Null
function Bound($path){$f=Get-Item -LiteralPath $path;@{path=$f.FullName;bytes=[long]$f.Length;sha256=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()}}
function CopyBlob($blob){
 if(-not $blob){return}
 if($blob.path -notmatch '^migration-blobs/[0-9a-f]{64}\.txt$'){throw 'packet-canonical-blob-path'}
 $source=Join-Path $packet $blob.path;$b=Bound $source
 if($b.bytes -ne $blob.bytes -or $b.sha256 -cne $blob.sha256){throw 'packet-canonical-blob-seal'}
 $target=Join-Path $destination $blob.path
 if(-not(Test-Path -LiteralPath $target)){[IO.File]::Copy($source,$target,$false)}
}
foreach($row in $migration.paths){CopyBlob $row.canonical_target;foreach($old in $row.old_entries){CopyBlob $old.canonical_original;CopyBlob $old.preserved_original}}
# The only additional formal delta is deterministic binary-mode JSON in the
# analyzer fixture generator. The protocol, analyzer and production source are
# untouched. Original migration and preserved old bytes are never rewritten.
$relative='research/prelive_current/tests/cost_analysis_tests.cpp'
if(@($migration.paths|Where-Object relative_path -ceq $relative).Count){throw 'local-delta-already-in-packet'}
$current=Bound (Join-Path $repo $relative)
$bytes=[IO.File]::ReadAllBytes($current.path);$utf8=[Text.UTF8Encoding]::new($false,$true);$text=$utf8.GetString($bytes)
if($text.Contains("`r") -or $text.StartsWith([string][char]0xfeff,[StringComparison]::Ordinal)){throw 'local-canonical-LF-required'}
$crlf=$utf8.GetBytes($text.Replace("`n","`r`n"))
$variants=@(@{encoding='LF';bytes=$bytes.Length;sha256=$current.sha256},@{encoding='CRLF';bytes=$crlf.Length;sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($crlf)).ToLowerInvariant()})
$blobRelative='migration-blobs/'+$current.sha256+'.txt';[IO.File]::WriteAllBytes((Join-Path $destination $blobRelative),$bytes)
$before=Get-Content -LiteralPath (Join-Path $repo 'out/prelive-20261006/formal-source-before.json') -Raw|ConvertFrom-Json
$anchor=@($before|Where-Object {$_.path.EndsWith('\include\pas\game.hpp',[StringComparison]::OrdinalIgnoreCase)})
if($anchor.Count -ne 1){throw 'historical-root-anchor'}
$historicalRoot=$anchor[0].path.Substring(0,$anchor[0].path.Length-'\include\pas\game.hpp'.Length)
$oldPath=Join-Path $historicalRoot $relative;$oldRows=@{}
foreach($freeze in $migration.old_freezes){
 $f=Bound (Join-Path $repo $freeze.path);if($f.bytes -ne $freeze.bytes -or $f.sha256 -cne $freeze.sha256){throw 'original-three-freezes'}
 foreach($row in (Get-Content -LiteralPath $f.path -Raw|ConvertFrom-Json).files){if($row.path -ceq $oldPath){$oldRows[$row.sha256]=$row}}
}
if($oldRows.Count -gt 1){throw 'single-preserved-generator-required'}
$oldEntries=@()
if($oldRows.Count -eq 1){
 $previous=@($oldRows.Values)[0];$old=Bound $previous.path
 if($old.bytes -ne $previous.bytes -or $old.sha256 -cne $previous.sha256){throw 'preserved-generator-original-seal'}
 $oldBlob='migration-blobs/'+$old.sha256+'.txt';[IO.File]::Copy($old.path,(Join-Path $destination $oldBlob),$false)
 $oldEntries=@(@{path=$previous.path;bytes=$previous.bytes;sha256=$previous.sha256;preserved_original=@{path=$oldBlob;bytes=$old.bytes;sha256=$old.sha256}})
}
# The analyzer fixture was introduced after the original bridge freeze, just
# like the packet's independent review TU. Preserve its handoff Git blob even
# when there is no historical freeze member to migrate.
$start=[Diagnostics.ProcessStartInfo]::new((Get-Command git.exe).Source)
$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true
foreach($arg in @('-C',$repo,'show',($migration.target_commit+':'+$relative))){$start.ArgumentList.Add($arg)}
$process=[Diagnostics.Process]::Start($start);$memory=[IO.MemoryStream]::new()
try{$process.StandardOutput.BaseStream.CopyTo($memory);$process.WaitForExit();if($process.ExitCode -ne 0 -or $memory.Length -ge 1048576){throw 'handoff-generator-blob'};$originalBytes=$memory.ToArray()}finally{$memory.Dispose();$process.Dispose()}
$originalSha=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($originalBytes)).ToLowerInvariant()
$originalBlob='migration-blobs/'+$originalSha+'.txt';if(-not(Test-Path -LiteralPath (Join-Path $destination $originalBlob))){[IO.File]::WriteAllBytes((Join-Path $destination $originalBlob),$originalBytes)}
$migration.paths+=@{relative_path=$relative;canonical_target=@{path=$blobRelative;bytes=$current.bytes;sha256=$current.sha256};target_encodings=$variants;
 old_entries=$oldEntries;handoff_original=@{path=$originalBlob;bytes=$originalBytes.Length;sha256=$originalSha;source_commit=$migration.target_commit}}
$originalCommit=$migration.target_commit;$migration.target_commit=$SourceCommit
$migration|Add-Member -NotePropertyName local_derivation -NotePropertyValue @{packet=(Bound $PacketMigration);handoff_commit=$originalCommit;
 additional_source_delta=$relative;reason='Windows text-mode CRLF differed from byte-exact binary-mode scoped review restoration';protocol_modified=$false;original_receipts_modified=$false}
$manifest=Join-Path $destination 'SOURCE_MIGRATION.json'
[IO.File]::WriteAllText($manifest,($migration|ConvertTo-Json -Depth 40)+"`n",[Text.UTF8Encoding]::new($false))
Bound $manifest|ConvertTo-Json -Compress
