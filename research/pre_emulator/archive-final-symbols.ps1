param([ValidateSet('release','debug','asan')][string]$Mode,[ValidatePattern('^main[0-9]{2}$')][string]$BuildAttempt,[string]$FinalStage)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'));$root=[IO.Path]::GetFullPath((Join-Path $repo ('out/prelive-pre-emulator-'+$Mode+'-'+$BuildAttempt)))
if([IO.Path]::GetDirectoryName($root) -cne (Join-Path $repo 'out') -or (Get-Item -LiteralPath $root).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'fixed symbol root'}
$stageRoot=Join-Path $repo ('out/windows-handoff/'+$FinalStage);$receiptPath=Join-Path $stageRoot ($FinalStage+'-verification.json')
$v=Get-Content -Raw -LiteralPath $receiptPath|ConvertFrom-Json;$r=Get-Content -Raw -LiteralPath (Join-Path $stageRoot ($FinalStage+'-result.json'))|ConvertFrom-Json;$s=Get-Content -Raw -LiteralPath (Join-Path $stageRoot 'state.json')|ConvertFrom-Json
if($s.status -cne 'COMPLETE' -or -not $v.producer_complete -or $v.native_exit -ne 0 -or $v.runner_exit -ne 0 -or $v.verification_exit -ne 0 -or $r.facts.active_final -ne 0 -or -not $r.facts.held_all_signaled -or -not $r.facts.streams_completed){throw 'all compilation must finish before symbol archive'}
if(@(Get-Process|Where-Object {$_.Path -and $_.Path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)}).Count){throw 'symbol consumers still active'}
function Snapshot($path){@{path=$path;bytes=(Get-Item -LiteralPath $path).Length;sha256=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()}}
# Only final executable symbols. Compiler PDBs, libraries and objects stay live.
$files=@(Get-ChildItem -LiteralPath $root -File -Recurse -Filter '*.exe'|ForEach-Object {$p=[IO.Path]::ChangeExtension($_.FullName,'.pdb');if(Test-Path -LiteralPath $p){$item=Get-Item -LiteralPath $p;if(-not $p.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or $item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'literal final PDB scope'};$f=Snapshot $p;$f.relative=[IO.Path]::GetRelativePath($root,$p).Replace('\','/');$f}})
if(-not $files.Count){return}
$dest=Join-Path $repo ('out/retained/consolidation-20261008/'+$BuildAttempt+'-'+$Mode+'-final-symbols');if(Test-Path -LiteralPath $dest){throw 'fresh symbols archive'};[IO.Directory]::CreateDirectory($dest)|Out-Null
$archive=Join-Path $dest 'symbols.zip';Add-Type -AssemblyName System.IO.Compression
$stream=[IO.File]::Open($archive,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);$zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create,$false)
try{foreach($f in $files){$entry=$zip.CreateEntry($f.relative,[IO.Compression.CompressionLevel]::Optimal);$input=[IO.File]::OpenRead($f.path);$output=$entry.Open();try{$input.CopyTo($output)}finally{$input.Dispose();$output.Dispose()}}}finally{$zip.Dispose();$stream.Dispose()}
$zip=[IO.Compression.ZipFile]::OpenRead($archive)
try{if($zip.Entries.Count -ne $files.Count){throw 'symbol archive count'};foreach($f in $files){$entry=$zip.GetEntry($f.relative);if(-not $entry -or $entry.Length -ne $f.bytes){throw 'symbol entry bytes'};$input=$entry.Open();$hash=[Security.Cryptography.SHA256]::Create();try{$sha=[Convert]::ToHexString($hash.ComputeHash($input)).ToLowerInvariant()}finally{$input.Dispose();$hash.Dispose()};$current=Snapshot $f.path;if($sha -cne $f.sha256 -or $current.sha256 -cne $f.sha256 -or $current.bytes -ne $f.bytes){throw 'symbol original/readback SHA'}}}finally{$zip.Dispose()}
$proof=@{schema='pas.serial-final-symbol-archive.v1';source_commit=(& git -C $repo rev-parse HEAD);root=$root;archive=(Snapshot $archive);files=$files;final_stage_receipt=(Snapshot $receiptPath);readback_verified=$true;serial_before_build_seal=$true;device_operations=0;live_ready=$false}
$proofPath=Join-Path $dest 'proof.json';[IO.File]::WriteAllText($proofPath,($proof|ConvertTo-Json -Depth 20),[Text.UTF8Encoding]::new($false))
foreach($f in $files){$p=[IO.Path]::GetFullPath($f.path);if(-not $p.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetExtension($p) -cne '.pdb' -or -not(Test-Path -LiteralPath ([IO.Path]::ChangeExtension($p,'.exe')))){throw 'checked final symbol deletion'};Remove-Item -LiteralPath $p -Force}
Write-Output ('serial final symbol archive verified '+$files.Count+' files')
