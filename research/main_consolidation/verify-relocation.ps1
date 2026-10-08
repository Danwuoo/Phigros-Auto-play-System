param(
 [Parameter(Mandatory)][string]$OriginalPath,
 [Parameter(Mandatory)][ValidateRange(0,68719476736)][long]$ExpectedBytes,
 [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedSha256,
 [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$RelocationIndexSha256
)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$audit=Join-Path $repo 'out/consolidation-20261008-01'
$indexPath=Join-Path $audit 'relocation-index.json'
function Safe([string]$path,[string]$base){
 if($path -match '^\\\\' -or $path.Substring(2).Contains(':')){throw 'unsafe relocation path'}
 $full=[IO.Path]::GetFullPath($path)
 if(-not $full.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'relocation scope'}
 $check=$full;while($check){if(Test-Path -LiteralPath $check){if((Get-Item -Force -LiteralPath $check).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'relocation link'}};$check=[IO.Path]::GetDirectoryName($check)}
 return $full
}
function Json([string]$path,[long]$cap){if((Get-Item -LiteralPath $path).Length -gt $cap){throw 'relocation metadata cap'};return Get-Content -Raw -LiteralPath $path|ConvertFrom-Json}
function VerifyFile([string]$path){$path=Safe $path $repo;$info=Get-Item -LiteralPath $path;if($info.PSIsContainer -or $info.Length -ne $ExpectedBytes -or (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -cne $ExpectedSha256){throw 'relocated bytes or SHA mismatch'};return $path}
$index=Json $indexPath 1048576
if((Get-FileHash -LiteralPath $indexPath).Hash.ToLowerInvariant() -cne $RelocationIndexSha256 -or $index.schema -cne 'pas.project-relocation-index.v1' -or -not $index.all_verified -or $index.actions.Count -gt 128){throw 'pinned relocation index required'}
$old=[IO.Path]::GetFullPath($OriginalPath)
if(-not $old.StartsWith('C:\Users\wurre\',[StringComparison]::OrdinalIgnoreCase) -or $old.Substring(2).Contains(':')){throw 'historical path outside surveyed scope'}
$matches=@($index.actions|Where-Object {$old -eq $_.source -or $old.StartsWith($_.source+'\',[StringComparison]::OrdinalIgnoreCase)}|Sort-Object { $_.source.Length } -Descending)
if($matches.Count){
 $action=$matches[0];if(-not $action.verified){throw 'unverified relocation action'}
 $suffix=$old.Substring($action.source.Length);$path=VerifyFile ($action.destination+$suffix)
 @{schema='pas.historical-relocation-check.v1';original_path=$old;retained_path=$path;bytes=$ExpectedBytes;sha256=$ExpectedSha256;verified=$true;current_main_qualification=$false;device_operations=0}|ConvertTo-Json
 exit 0
}
# Exact physical source bytes are retained separately from canonical Git trees.
# This is historical proof only; it does not rebuild or qualify old binaries.
$snapshots=Json (Join-Path $audit 'physical-source-snapshots.json') 1048576
if(@($snapshots).Count -gt 32){throw 'source snapshot count cap'}
$roots=@($snapshots|Where-Object {$old.StartsWith($_.root+'\',[StringComparison]::OrdinalIgnoreCase)})
if($roots.Count -ne 1 -or -not $roots[0].verified -or $ExpectedBytes -gt 16777216){throw 'historical path has no bounded retained source'}
$snapshot=$roots[0];$archive=Safe $snapshot.archive $repo
$manifest=Json ([IO.Path]::ChangeExtension($archive,'.json')) 4194304
if((Get-Item -LiteralPath $archive).Length -gt 268435456 -or (Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant() -cne $manifest.archive_sha256 -or @($manifest.files).Count -gt 8192){throw 'source snapshot seal'}
$relative=$old.Substring($snapshot.root.Length+1).Replace('\','/')
$rows=@($manifest.files|Where-Object path -ceq $relative)
if($rows.Count -ne 1 -or $rows[0].bytes -ne $ExpectedBytes -or $rows[0].sha256 -cne $ExpectedSha256){throw 'source snapshot entry seal'}
$zip=[IO.Compression.ZipFile]::OpenRead($archive)
try{
 if($zip.Entries.Count -gt 8192){throw 'source ZIP entry cap'}
 $entry=$zip.GetEntry($relative);if(-not $entry -or $entry.Length -ne $ExpectedBytes){throw 'source ZIP entry identity'}
 $stream=$entry.Open();$hash=[Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256)
 try{$buffer=[byte[]]::new(65536);$bytes=0L;while(($n=$stream.Read($buffer,0,$buffer.Length)) -gt 0){$bytes+=$n;if($bytes -gt $ExpectedBytes){throw 'source ZIP decoded byte cap'};$hash.AppendData($buffer,0,$n)};if($bytes -ne $ExpectedBytes -or [Convert]::ToHexString($hash.GetHashAndReset()).ToLowerInvariant() -cne $ExpectedSha256){throw 'source ZIP decoded seal'}}finally{$stream.Dispose();$hash.Dispose()}
}finally{$zip.Dispose()}
@{schema='pas.historical-relocation-check.v1';original_path=$old;retained_archive=$archive;retained_entry=$relative;source_commit=$snapshot.source_commit;bytes=$ExpectedBytes;sha256=$ExpectedSha256;verified=$true;current_main_qualification=$false;device_operations=0}|ConvertTo-Json
