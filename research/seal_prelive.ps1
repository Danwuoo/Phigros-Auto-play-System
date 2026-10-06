param([ValidatePattern('^[a-z0-9-]{1,24}$')][string]$Attempt='01')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$pkg=Join-Path $repo 'out/prelive-20261006'
$win=Join-Path $repo 'docs/research/zero-miss-20261006/windows'
$seal=Join-Path $win ('prelive-evidence-'+$Attempt)
if(Test-Path -LiteralPath $seal){throw 'fresh seal'}
function Entry($path){$f=Get-Item -LiteralPath $path;@{path=$f.FullName;bytes=$f.Length;sha256=(Get-FileHash -LiteralPath $f.FullName).Hash.ToLowerInvariant()}}
function Save($path,$value){$b=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 50)+"`n");if($b.Length -gt 67108864){throw 'metadata record cap'};$s=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew);try{$s.Write($b);$s.Flush($true)}finally{$s.Dispose()}}
$stages=@(Get-ChildItem (Join-Path $repo 'out/windows-handoff') -Directory|Where-Object {$_.Name -like '*prelive*' -and $_.Name -notlike '*-inputs'})
$stageRows=@(foreach($stage in $stages){$vpath=Join-Path $stage.FullName ($stage.Name+'-verification.json');$rpath=Join-Path $stage.FullName ($stage.Name+'-result.json');$state=Join-Path $stage.FullName 'state.json';$v=if(Test-Path $vpath){Get-Content -LiteralPath $vpath -Raw|ConvertFrom-Json}else{$null};$r=if(Test-Path $rpath){Get-Content -LiteralPath $rpath -Raw|ConvertFrom-Json}else{$null};if($v.pending -or $r.pending){throw ('unfinished process '+$stage.Name)};if($r.facts.created -and ($r.facts.active_final -ne 0 -or -not $r.facts.held_all_signaled -or -not $r.facts.streams_completed)){throw ('unclosed job '+$stage.Name)};@{stage=$stage.Name;verification=$v.verification_exit;native=$r.facts.exit_code;runner=$r.facts.runner_exit;reason=$r.facts.reason;active_final=$r.facts.active_final;held_all_signaled=$r.facts.held_all_signaled;streams_completed=$r.facts.streams_completed;identity_trusted=$r.facts.identity_trusted;receipt=if(Test-Path $vpath){Entry $vpath}else{$null};state=if(Test-Path $state){Get-Content $state -Raw|ConvertFrom-Json}else{$null};status=if($v.verification_exit -eq 0 -and -not $v.pending){'verified'}elseif($r.facts.created){'retained_failed'}else{'preflight_retained'}}})
[IO.Directory]::CreateDirectory($seal)|Out-Null
$metadata=@()
$stageDirs=@(Get-ChildItem (Join-Path $repo 'out/windows-handoff') -Directory|Where-Object Name -like '*prelive*')
foreach($dir in $stageDirs){foreach($file in Get-ChildItem -LiteralPath $dir.FullName -File -Recurse){$rel=[IO.Path]::GetRelativePath((Join-Path $repo 'out/windows-handoff'),$file.FullName);$metadata+=@{source=$file.FullName;dest=(Join-Path $seal ('receipts/'+$rel))}}}
foreach($file in Get-ChildItem -LiteralPath $pkg -File -Recurse){if($file.Name -like '*.attempts.json' -or $file.Extension -in '.jsonl','.zip'){continue};if($file.Extension -notin '.json','.cpp','.hpp','.ps1','.md','.txt','.png'){continue};$metadata+=@{source=$file.FullName;dest=(Join-Path $seal ('package/'+[IO.Path]::GetRelativePath($pkg,$file.FullName)))}}
$estimate=[long](($metadata|ForEach-Object {(Get-Item -LiteralPath $_.source).Length}|Measure-Object -Sum).Sum)
if($estimate+8388608 -gt 67108864){throw 'metadata headroom'}
foreach($m in $metadata){[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($m.dest))|Out-Null;Copy-Item -LiteralPath $m.source -Destination $m.dest;if((Get-FileHash -LiteralPath $m.source).Hash -cne (Get-FileHash -LiteralPath $m.dest).Hash){throw 'copy-sha'}}
$newRoots=@(Get-ChildItem (Join-Path $repo 'out') -Directory|Where-Object Name -like 'prelive*')
$allFiles=@(foreach($dir in $newRoots){Get-ChildItem -LiteralPath $dir.FullName -File -Recurse})
$external=@($allFiles|ForEach-Object {Entry $_.FullName})
Save (Join-Path $seal 'external-new-out.json') @{files=$external;measurement_raw_external=$true;binary_dependency_external=$true;no_raw_downsample=$true;compiler_intermediate_archive='package/compiler-object-archive-01/object-bindings.json';retained_objects_zip=Entry (Join-Path $pkg 'compiler-object-archive-01/objects.zip')}
$roots=@{release='prelive-current-release-13';debug='prelive-current-debug-05';asan='prelive-current-asan-04'}
$candidate=@(foreach($mode in @('release','debug','asan')){$root=Join-Path $repo ('out/'+$roots[$mode]);@{mode=$mode;root=$root;files=@(Get-ChildItem -LiteralPath $root -File|Where-Object Extension -in '.exe','.lib','.dll','.map'|ForEach-Object {Entry $_.FullName});cache=Entry (Join-Path $root 'CMakeCache.txt');ninja=Entry (Join-Path $root 'build.ninja');core_donor=if($mode -eq 'release'){'prelive-current-release-11'}elseif($mode -eq 'debug'){'prelive-current-debug-04'}else{'prelive-current-asan-03'}}})
$loaded=@(Get-ChildItem -LiteralPath $pkg -File|Where-Object {$_.Name -match '^(pipeline|pixels)-(release|debug|asan)-.*\.json$' -and $_.Name -notlike '*.attempts.json'}|ForEach-Object {$r=Get-Content -LiteralPath $_.FullName -Raw|ConvertFrom-Json;@($r.loaded_module_paths)})|Where-Object {$_}|Sort-Object -Unique
$moduleFiles=@($loaded|ForEach-Object {Entry $_})
Save (Join-Path $seal 'candidate-manifest.json') @{schema='pas.current-rails-candidate-closure.v1';algorithm_commit='54c0e86';build_recipe_commit='be7ee7e';cost_controller_commit='602d756';head_at_seal=(& git -C $repo rev-parse HEAD);source=@(Get-ChildItem (Join-Path $repo 'research/prelive_current') -File|ForEach-Object {Entry $_.FullName})+@(Entry (Join-Path $repo 'include/pas/game.hpp'))+@(Entry (Join-Path $repo 'src/game.cpp'));binaries=$candidate;loaded_module_backing_files=$moduleFiles;module_memory_image_hash_not_measured=$true;profile=Entry (Join-Path $repo 'configs/phigros-hd-assist-five-lead35.json');cold_only=$true;endpoint_count=0;live_cli_hook=$false;normal_cost_gate='NOT_READY';product_goal_complete=$false;intermediate_snapshot_08='late hash-only receipt, not exact pre-run source snapshot; do not use for source reconstruction'}
$original='C:/Users/wurre/Desktop/Phigros-Auto-play-System'
$origHead=(& git -C $original rev-parse HEAD);$origDirty=@(& git -C $original status --short)
$audit=Get-Content (Join-Path $pkg 'initial-audit.json') -Raw|ConvertFrom-Json
if($origHead -cne $audit.original_head -or ($origDirty -join "`n") -cne ($audit.original_dirty -join "`n")){throw 'original checkout changed'}
$zip=Entry (Join-Path $original 'phigros-zero-miss-handoff-round4-9dc99d6.zip');if($zip.sha256 -cne $audit.zip_sha){throw 'handoff zip changed'}
$aux='\\?\C:\Users\wurre\Desktop\Phigros-Auto-play-System\apps\runtime_x11_p\aux\CMakeLists.txt';$fs=[IO.File]::OpenRead($aux);$sha=[Security.Cryptography.SHA256]::Create();try{$auxHash=[Convert]::ToHexString($sha.ComputeHash($fs)).ToLowerInvariant()}finally{$fs.Dispose();$sha.Dispose()};if($auxHash -cne $audit.aux_sha){throw 'original aux changed'}
$selection=Get-Content (Join-Path $pkg 'full-prefix-selection.json') -Raw|ConvertFrom-Json;foreach($f in $selection.frames){$e=Entry $f.path;if($e.bytes -ne $f.bytes -or $e.sha256 -cne $f.sha256){throw 'original png changed'}}
$ledger=@(foreach($root in $newRoots){@{root=$root.FullName;bytes=[long](Get-ChildItem -LiteralPath $root.FullName -File -Recurse|Measure-Object Length -Sum).Sum}})
$stageBytes=[long](($stageDirs|ForEach-Object {(Get-ChildItem -LiteralPath $_.FullName -File -Recurse|Measure-Object Length -Sum).Sum}|Measure-Object -Sum).Sum)
$outBytes=[long](($ledger|Measure-Object bytes -Sum).Sum)+$stageBytes
$sealBytes=[long](Get-ChildItem -LiteralPath $seal -File -Recurse|Measure-Object Length -Sum).Sum
if($outBytes -gt 12884901888 -or $sealBytes+1048576 -gt 67108864 -or (Get-PSDrive C).Free -lt 21474836480){throw 'closure capacity'}
Save (Join-Path $seal 'closure.json') @{schema='pas.prelive-closure.v1';utc=[DateTime]::UtcNow.ToString('o');stages=$stageRows;new_out_bytes=$outBytes;metadata_before_index_bytes=$sealBytes;new_out_cap=12884901888;metadata_cap=67108864;minimum_free=21474836480;free=(Get-PSDrive C).Free;capacity_ledger=$ledger;stage_bytes=$stageBytes;original_head=$origHead;original_dirty=$origDirty;aux_sha=$auxHash;zip=$zip;original_png_checked=$selection.frames.Count;device_commands=0;physical_gold=0;runtime_gate='NOT_READY';product_goal_complete=$false;all_launched_jobs_final_zero=$true;raw_artifacts=Entry (Join-Path $seal 'external-new-out.json')}
$index=@(Get-ChildItem -LiteralPath $seal -File -Recurse|ForEach-Object {$e=Entry $_.FullName;@{relative=[IO.Path]::GetRelativePath($seal,$_.FullName).Replace('\','/');bytes=$e.bytes;sha256=$e.sha256}})
Save (Join-Path $seal 'SHA256_INDEX.json') @{schema='pas.prelive-metadata-seal.v1';entries=$index;capacity_file_bytes=$true;allocated_clusters_not_measured=$true}
foreach($e in $index){$f=Entry (Join-Path $seal $e.relative);if($f.bytes -ne $e.bytes -or $f.sha256 -cne $e.sha256){throw 'final seal verification'}}
@{seal=$seal;metadata_bytes=(Get-ChildItem -LiteralPath $seal -File -Recurse|Measure-Object Length -Sum).Sum;new_out_bytes=$outBytes;sealed_files=$index.Count;index=Entry (Join-Path $seal 'SHA256_INDEX.json')}|ConvertTo-Json -Depth 5
