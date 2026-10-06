$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$win=Join-Path $repo 'docs/research/zero-miss-20261006/windows'
$seal=Join-Path $win 'prelive-evidence-01'
$pkg=Join-Path $repo 'out/prelive-20261006'
$stage='audit-data-prelive-release-writer-final'
function Entry($path){$f=Get-Item -LiteralPath $path;@{path=$f.FullName;bytes=$f.Length;sha256=(Get-FileHash -LiteralPath $f.FullName).Hash.ToLowerInvariant()}}
function Save($path,$value){$b=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 50)+"`n");if($b.Length -gt 67108864){throw 'metadata record cap'};$s=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew);try{$s.Write($b);$s.Flush($true)}finally{$s.Dispose()}}
function Check($path,$expected){$e=Entry $path;if($e.bytes -ne $expected.bytes -or $e.sha256 -cne $expected.sha256){throw ('sha mismatch '+$path)}}
$oldIndex=Entry (Join-Path $seal 'SHA256_INDEX.json')
if($oldIndex.sha256 -cne '3c8ecef50639858ecf71f2ed88e4a4653aaae2071125cc07c625cc23db9394bd'){throw 'original seal index changed'}
foreach($e in (Get-Content -LiteralPath $oldIndex.path -Raw|ConvertFrom-Json).entries){Check (Join-Path $seal $e.relative) $e}
if(Test-Path -LiteralPath (Join-Path $seal 'SHA256_INDEX_FINAL.json')){throw 'final seal already exists'}
$audit=Get-Content -LiteralPath (Join-Path $pkg 'initial-audit.json') -Raw|ConvertFrom-Json
$oldSeals=@(foreach($s in $audit.seals){$base=Join-Path $win $s.name;$name=if($s.name -eq 'evidence'){'SHA256_INDEX_FINAL.json'}else{'SHA256_INDEX.json'};$index=Entry (Join-Path $base $name);if($index.sha256 -cne $s.index_sha){throw 'historical index changed'};$files=(Get-Content -LiteralPath $index.path -Raw|ConvertFrom-Json).files;foreach($e in $files){Check (Join-Path $base $e.relative) $e};@{name=$s.name;checked=$files.Count;index=$index}})
$coreNames=@('bvi.cpp','bvi.hpp','contact_policy.cpp','contact_policy.hpp','relation_policy.cpp','relation_policy.hpp','constraint_policy.cpp')
$freeze=Get-Content -LiteralPath (Join-Path $repo 'out/windows-handoff/build-bridge-withdraw-release-01/freeze.json') -Raw|ConvertFrom-Json
$core=@($freeze.files|Where-Object { $_.path -match '\\bvi_cold_v3\\' -and (Split-Path $_.path -Leaf) -in $coreNames })
if($core.Count -ne 7){throw 'v3 core count'};foreach($e in $core){Check $e.path $e}
$r=Get-Content -LiteralPath (Join-Path $repo ('out/windows-handoff/'+$stage+'/'+$stage+'-result.json')) -Raw|ConvertFrom-Json
$vpath=Join-Path $repo ('out/windows-handoff/'+$stage+'/'+$stage+'-verification.json')
$v=Get-Content -LiteralPath $vpath -Raw|ConvertFrom-Json
if($v.pending -or $v.native_exit -ne 0 -or $v.runner_exit -ne 0 -or $v.verification_exit -ne 0 -or $r.facts.active_final -ne 0 -or -not $r.facts.identity_trusted -or -not $r.facts.held_all_signaled -or -not $r.facts.streams_completed){throw 'latest writer audit receipt invalid'}
foreach($e in $v.entries){Check $e.path $e}
$science=Get-Content -LiteralPath (Join-Path $pkg 'audit-writer-final.json') -Raw|ConvertFrom-Json
if(-not $science.integrity -or -not $science.declared_writer_loss -or $science.complete_prefix_evidence){throw 'writer science disposition mismatch'}
$sources=@()
foreach($name in @($stage,($stage+'-inputs'))){$dir=Join-Path $repo ('out/windows-handoff/'+$name);foreach($f in Get-ChildItem -LiteralPath $dir -File -Recurse){$rel=[IO.Path]::GetRelativePath((Join-Path $repo 'out/windows-handoff'),$f.FullName);$sources+=@{source=$f.FullName;dest=(Join-Path $seal ('receipts/'+$rel))}}}
foreach($name in @('audit-writer-final.json',($stage+'-params.json'))){$sources+=@{source=(Join-Path $pkg $name);dest=(Join-Path $seal ('package/'+$name))}}
foreach($s in $sources){if(Test-Path -LiteralPath $s.dest){throw 'append would overwrite'};[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($s.dest))|Out-Null;Copy-Item -LiteralPath $s.source -Destination $s.dest;Check $s.dest (Entry $s.source)}
$original='C:/Users/wurre/Desktop/Phigros-Auto-play-System'
$head=(& git -C $original rev-parse HEAD);$dirty=@(& git -C $original status --short)
if($head -cne $audit.original_head -or ($dirty -join "`n") -cne ($audit.original_dirty -join "`n")){throw 'original checkout changed'}
$zip=Entry (Join-Path $original 'phigros-zero-miss-handoff-round4-9dc99d6.zip');if($zip.sha256 -cne $audit.zip_sha){throw 'handoff zip changed'}
$aux='\\?\C:\Users\wurre\Desktop\Phigros-Auto-play-System\apps\runtime_x11_p\aux\CMakeLists.txt';$fs=[IO.File]::OpenRead($aux);$sha=[Security.Cryptography.SHA256]::Create();try{$auxHash=[Convert]::ToHexString($sha.ComputeHash($fs)).ToLowerInvariant()}finally{$fs.Dispose();$sha.Dispose()};if($auxHash -cne $audit.aux_sha){throw 'original AUX changed'}
$selection=Get-Content -LiteralPath (Join-Path $pkg 'full-prefix-selection.json') -Raw|ConvertFrom-Json;foreach($e in $selection.frames){Check $e.path $e}
$roots=@(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -Directory|Where-Object Name -like 'prelive*')+@(Get-ChildItem -LiteralPath (Join-Path $repo 'out/windows-handoff') -Directory|Where-Object Name -like '*prelive*')
$outBytes=[long](($roots|ForEach-Object {(Get-ChildItem -LiteralPath $_.FullName -File -Recurse|Measure-Object Length -Sum).Sum}|Measure-Object -Sum).Sum)
$metadataBefore=[long](Get-ChildItem -LiteralPath $seal -File -Recurse|Measure-Object Length -Sum).Sum
$free=[long](Get-PSDrive C).Free
if($outBytes -gt 12884901888 -or $metadataBefore+2097152 -gt 67108864 -or $free -lt 21474836480){throw 'final capacity'}
Save (Join-Path $seal 'closure-final.json') @{schema='pas.prelive-closure-append.v1';utc=[DateTime]::UtcNow.ToString('o');head_at_append=(& git -C $repo rev-parse HEAD);preserved_first_index=$oldIndex;preserved_external_index=Entry (Join-Path $seal 'external-new-out.json');appended_external_files=@($sources|ForEach-Object {Entry $_.source});writer_receipt=Entry $vpath;writer_native=0;writer_runner=0;writer_verifier=0;active_final=0;held_all_signaled=$true;streams_completed=$true;writer_integrity=$true;writer_declared_loss=$true;writer_complete_prefix_evidence=$false;first_writer_audit_stop_preserved=$true;historical_seals=$oldSeals;v3_core_rechecked=$core;original_head=$head;original_dirty=$dirty;original_aux_sha=$auxHash;original_zip=$zip;original_png_rechecked=$selection.frames.Count;new_out_bytes=$outBytes;new_out_cap=12884901888;metadata_before_final_index_bytes=$metadataBefore;metadata_cap=67108864;free=$free;minimum_free=21474836480;file_bytes_only=$true;allocated_clusters_not_measured=$true;device_commands=0;normal_cost_gate='NOT_READY';physical_adoption='unknown';product_goal_complete=$false}
$entries=@(Get-ChildItem -LiteralPath $seal -File -Recurse|ForEach-Object {$e=Entry $_.FullName;@{relative=[IO.Path]::GetRelativePath($seal,$_.FullName).Replace('\','/');bytes=$e.bytes;sha256=$e.sha256}})
Save (Join-Path $seal 'SHA256_INDEX_FINAL.json') @{schema='pas.prelive-metadata-final-seal.v1';excluded_self='SHA256_INDEX_FINAL.json';preserved_first_index_sha256=$oldIndex.sha256;entries=$entries;capacity_file_bytes=$true;allocated_clusters_not_measured=$true}
foreach($e in $entries){Check (Join-Path $seal $e.relative) $e}
$bytes=[long](Get-ChildItem -LiteralPath $seal -File -Recurse|Measure-Object Length -Sum).Sum;if($bytes -gt 67108864){throw 'final metadata capacity'}
@{seal=$seal;indexed_files=$entries.Count;metadata_bytes=$bytes;new_out_bytes=$outBytes;final_index=Entry (Join-Path $seal 'SHA256_INDEX_FINAL.json');historical_files_rechecked=($oldSeals|Measure-Object checked -Sum).Sum;original_png_rechecked=$selection.frames.Count;v3_core_rechecked=$core.Count}|ConvertTo-Json -Depth 6
