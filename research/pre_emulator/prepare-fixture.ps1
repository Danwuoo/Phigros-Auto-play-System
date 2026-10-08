param([Parameter(Mandatory)][string]$SourceManifest,[Parameter(Mandatory)][string]$Destination)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
function Snapshot($path){@{path=[IO.Path]::GetFullPath($path);bytes=(Get-Item -LiteralPath $path).Length;sha256=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()}}
$dest=[IO.Path]::GetFullPath($Destination)
if([IO.Path]::GetDirectoryName($dest) -cne (Join-Path $repo 'out') -or [IO.Path]::GetFileName($dest) -notmatch '^prelive-pre-emulator-fixture-repo-main[0-9]{2}-(release|debug|asan)$' -or (Test-Path -LiteralPath $dest)){throw 'fresh isolated fixture root'}
$m=Get-Content -LiteralPath $SourceManifest -Raw|ConvertFrom-Json
if((& git -C $repo rev-parse HEAD) -cne $m.source_commit -or (& git -C $repo status --porcelain)){throw 'exact clean fixture source'}
[IO.Directory]::CreateDirectory($dest)|Out-Null
$rows=@();foreach($f in $m.files){
 $current=Snapshot $f.path;if($current.bytes -ne $f.bytes -or $current.sha256 -cne $f.sha256){throw 'fixture original SHA'}
 $rel=[IO.Path]::GetRelativePath($repo,$f.path);if($rel.StartsWith('..') -or [IO.Path]::IsPathRooted($rel)){throw 'fixture containment'}
 $to=Join-Path $dest $rel;[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($to))|Out-Null;[IO.File]::Copy($f.path,$to,$false)
 $copy=Snapshot $to;if($copy.bytes -ne $f.bytes -or $copy.sha256 -cne $f.sha256){throw 'fixture copy SHA'};$rows+=@(@{relative_path=$rel;original=$f;copy=$copy})
}
$profile=Join-Path $repo 'configs/phigros-hd-assist-five-lead35.json';$to=Join-Path $dest 'configs/phigros-hd-assist-five-lead35.json'
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($to))|Out-Null;[IO.File]::Copy($profile,$to,$false)
$original=Snapshot $profile;$copy=Snapshot $to;if($copy.bytes -ne $original.bytes -or $copy.sha256 -cne $original.sha256){throw 'profile fixture SHA'}
$rows+=@(@{relative_path='configs/phigros-hd-assist-five-lead35.json';original=$original;copy=$copy})
$legacyCommit='7ac2a31a1c5b0b76d96de9daa756f3990875e5c9';$legacyPath='docs/research/zero-miss-20261006/windows/PRELIVE_CURRENT_COST_PROTOCOL.md'
$to=Join-Path $dest $legacyPath;[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($to))|Out-Null
$psi=[Diagnostics.ProcessStartInfo]::new((Get-Command git).Source);$psi.UseShellExecute=$false;$psi.CreateNoWindow=$true;$psi.RedirectStandardOutput=$true;$psi.RedirectStandardError=$true
foreach($arg in @('-C',$repo,'show',($legacyCommit+':'+$legacyPath))){$psi.ArgumentList.Add($arg)}
$process=[Diagnostics.Process]::Start($psi);$stream=[IO.File]::Open($to,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
try{$process.StandardOutput.BaseStream.CopyTo($stream);$errorText=$process.StandardError.ReadToEnd();$process.WaitForExit();if($process.ExitCode -ne 0){throw ('legacy blob read '+$errorText)}}finally{$stream.Dispose();$process.Dispose()}
$copy=Snapshot $to;if($copy.bytes -ne 5118 -or $copy.sha256 -cne '58d6136fcbce1ebf6c25a23795f10aab78013d46318c2b6261b7f6206abe91e6'){throw 'byte-exact original legacy fixture protocol'}
$rows+=@(@{relative_path=$legacyPath;copy=$copy;git_commit=$legacyCommit})
$proof=@{schema='pas.isolated-analyzer-fixture-repository.v1';source_commit=$m.source_commit;repository=$dest;source_manifest=(Snapshot $SourceManifest);files=$rows;formal_repository_changed=$false;scope='test-double current source and byte-exact legacy protocol; actual R2 protocol unchanged';device_operations=0;live_ready=$false}
[IO.File]::WriteAllText((Join-Path $dest 'fixture-repository-proof.json'),($proof|ConvertTo-Json -Depth 20),[Text.UTF8Encoding]::new($false))
