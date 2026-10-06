$ErrorActionPreference='Stop'
$taskRepo='C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006'
$original='C:\Users\wurre\Desktop\Phigros-Auto-play-System'
$handoff=Join-Path $taskRepo 'out/windows-handoff'
$archive=Join-Path $taskRepo 'docs/research/zero-miss-20261006/windows/evidence'
$delta=Join-Path $archive 'final-profile-01'
if(Test-Path -LiteralPath $delta){throw 'fresh-final-delta'}
function Entry([string]$path) {
 $f=Get-Item -LiteralPath $path
 if($f.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'final-reparse'}
 [ordered]@{path=$f.FullName;bytes=[long]$f.Length;sha256=(Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
function Check([string]$path,[string]$sha,[long]$bytes=-1) {
 $e=Entry $path
 if($e.sha256 -cne $sha -or ($bytes -ge 0 -and $bytes -ne $e.bytes)){throw ('final-sha: '+$path)}
}
function NewJson([string]$path,$value,[long]$cap=2097152) {
 $bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 45)+"`n")
 if($bytes.Length -gt $cap){throw 'final-json-cap'}
 [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))|Out-Null
 $stream=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
 try{$stream.Write($bytes);$stream.Flush($true)}finally{$stream.Dispose()}
}
$sealed=Get-Content -LiteralPath (Join-Path $archive 'SHA256_INDEX.json') -Raw|ConvertFrom-Json
foreach($file in $sealed.files){Check (Join-Path $archive $file.relative) $file.sha256 $file.bytes}
$oldSources=Get-Content -LiteralPath (Join-Path $archive 'source-and-binary-sha.json') -Raw|ConvertFrom-Json
foreach($file in $oldSources.binaries){Check $file.path $file.sha256 $file.bytes}
$oldSourceRoot=Join-Path $handoff 'source-frozen-0f81a38-01'
$oldSource=Get-Content -LiteralPath (Join-Path $oldSourceRoot 'manifest.json') -Raw|ConvertFrom-Json
foreach($file in $oldSource.files){
 $path=Join-Path $oldSourceRoot $file.path;Check $path $file.sha256 $file.bytes
 $blob=(& git -C $taskRepo hash-object --no-filters -- $path).Trim();if($LASTEXITCODE -ne 0){throw 'old-source-hash-object'}
 $expected=(& git -C $taskRepo rev-parse ($oldSource.source_commit+':'+$file.path)).Trim()
 if($LASTEXITCODE -ne 0 -or $blob -cne $expected){throw 'old-source-git-blob'}
}
$external=Get-Content -LiteralPath (Join-Path $archive 'external-originals.json') -Raw|ConvertFrom-Json
foreach($file in @($external.original_stop_states)+@($external.existing_inputs)){Check $file.path $file.sha256 $file.bytes}
$selection=Get-Content -LiteralPath (Join-Path $handoff 'real-pixels-selection-02/selection.json') -Raw|ConvertFrom-Json
foreach($file in $selection.frames){Check $file.path $file.sha256 $file.bytes}
Check $selection.index_path $selection.index_sha256
Check (Join-Path $selection.record_root 'manifest.json') $selection.record_manifest_sha256
Check (Join-Path $original 'phigros-zero-miss-handoff-round4-9dc99d6.zip') '18040114639c560c0ed33d6d7949affd1a4fb1ce2be807adf1f53b948656ee9b' 5187688
$originalHead=(& git -C $original rev-parse HEAD).Trim();if($originalHead -cne '74e54437d4a3ad2b2bd1a3b09312211a92f2359e'){throw 'original-HEAD-changed'}
$originalDirty=@(& git -C $original status --short)
$aux='\\?\'+$original+'\apps\runtime_x11_p\aux\CMakeLists.txt'
$auxSha=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([IO.File]::ReadAllBytes($aux))).ToLowerInvariant()
if($auxSha -cne 'e5c78a1d90a5e430c4f7c5328c7e9a8b3571800b0b8fc698d082b1cbe16d5501'){throw 'original-dirty-bytes-changed'}
$sourceCommit=(& git -C $taskRepo rev-parse HEAD).Trim()
if($sourceCommit -cne '29e3ed09ae176a93a159f3b7993c8047505e9482'){throw 'final-source-commit'}
$deliveryTree=(& git -C $taskRepo rev-parse '18965192f43e47ce9d5f6a2d8ad372b4bdb0507b^{tree}').Trim()
if($deliveryTree -cne '6a4a9950423f0775992780b65b96ce8dcf2bb98e'){throw 'delivery-tree'}
$formalChanged=@(& git -C $taskRepo diff --name-only 18965192f43e47ce9d5f6a2d8ad372b4bdb0507b HEAD -- src include tests CMakeLists.txt research/bvi_cold_v3 research/x10d_o_bvi_build)
if($LASTEXITCODE -ne 0 -or $formalChanged.Count -ne 0){throw 'formal-or-frozen-sources-changed'}
$coreNames=@('bvi.cpp','bvi.hpp','constraint_policy.cpp','contact_policy.cpp','contact_policy.hpp','relation_policy.cpp','relation_policy.hpp')
$coreSha=@('866ad2440d088ec1caf360eb5d4a9c796fdfcbcf2e15aff92374baf2291ea824','565e7c058328d01b11b75e7bf05a96a96dcdd378b42f000a422c000957671d5f','693cb7a1fa177035e1542c772dd2e26e63cd280fcdc34067aea4f3959297c821','b08520d87591b914fd357205e506a5080abb690dc4efe08c6e44e1225aea1c6f','08f48f197ef36449bc04b81cdd27005c50e56bc4766fde9e5d3bc58c38ccb691','37a23c696654b3935a51d422991f260213094a48d1488dc45e74334b29ac1efc','bbb47373899ac0531e424849aaa3d32638348ea25e8ebf682ddfaf906ac9feb0')
for($i=0;$i -lt $coreNames.Count;++$i){Check (Join-Path $taskRepo ('research/bvi_cold_v3/'+$coreNames[$i])) $coreSha[$i]}
$stages=@();$results=@()
foreach($mode in @('release','debug','asan')) {
 foreach($prefix in @('configure','build','prefix','pixels')) {
  $name=$prefix+'-bridge-profile-'+$mode+'-01';$root=Join-Path $handoff $name
  $state=Get-Content -LiteralPath (Join-Path $root 'state.json') -Raw|ConvertFrom-Json
  $v=Get-Content -LiteralPath (Join-Path $root ($name+'-verification.json')) -Raw|ConvertFrom-Json
  $facts=(Get-Content -LiteralPath (Join-Path $root ($name+'-result.json')) -Raw|ConvertFrom-Json).facts
  if($state.status -eq 'STOP' -or $v.pending -or $v.native_exit -ne 0 -or $v.runner_exit -ne 0 -or
     $v.verification_exit -ne 0 -or $facts.active_final -ne 0 -or -not $facts.identity_trusted -or
     -not $facts.streams_completed -or -not $facts.held_all_signaled){throw ('final-stage: '+$name)}
  foreach($file in $v.entries){Check $file.path $file.sha256 $file.bytes}
  $stages+=@{name=$name;state=$state.status;native_exit=$v.native_exit;runner_exit=$v.runner_exit;verification_exit=$v.verification_exit;active_final=$facts.active_final;elapsed_s=$facts.elapsed_s}
 }
 $prefix=Get-Content -LiteralPath (Join-Path $handoff ('prefix-profile-'+$mode+'-01.json')) -Raw|ConvertFrom-Json
 $pixels=Get-Content -LiteralPath (Join-Path $handoff ('pixels-profile-'+$mode+'-01.json')) -Raw|ConvertFrom-Json
 $pv=Get-Content -LiteralPath (Join-Path $handoff ('pixels-bridge-profile-'+$mode+'-01-pixel-controls/verification.json')) -Raw|ConvertFrom-Json
 if($prefix.assertions -ne 113 -or $prefix.failed_assertions -ne 0 -or $pixels.processed_frames -ne 256 -or
    $pixels.failed -ne 0 -or $pixels.invalid_frames -ne 0 -or $pixels.allowed_targets -ne 0 -or
    $pixels.own_fake_downs -ne 0 -or $pixels.own_fake_moves -ne 0 -or $pixels.own_fake_ups -ne 0 -or
    -not $pixels.final_release_verified -or -not $pv.passed -or -not $pv.before_verified -or -not $pv.after_verified){throw 'final-regression'}
 $results+=@{mode=$mode;prefix_assertions=$prefix.assertions;prefix_failed=$prefix.failed_assertions;
   real_frames=$pixels.processed_frames;eligible_targets=$pixels.allowed_targets;own_fake_downs=$pixels.own_fake_downs;
   final_release_verified=$pixels.final_release_verified;input_before_after_verified=$pv.passed;
   complete_chain_ms=$pixels.complete_observer_bridge_owner_scheduler_receipts_ms}
}
$paths=@(Get-ChildItem -LiteralPath $handoff -File -Recurse|Where-Object {
 $_.FullName -match 'bridge-profile-|prefix-profile-|pixels-profile-|\\final-profile-01\\|\\source-frozen-0f81a38-01\\manifest.json$' -and
 $_.Extension -in '.json','.jsonl','.log','.xml','.ps1'
})
$oldBytes=(Get-ChildItem -LiteralPath $archive -File -Recurse|Measure-Object Length -Sum).Sum
$planned=($paths|Measure-Object Length -Sum).Sum
if($oldBytes+$planned+4194304 -gt 67108864){throw 'combined-metadata-reserve'}
[IO.Directory]::CreateDirectory($delta)|Out-Null
$copies=@(foreach($source in $paths){
 $entry=Entry $source.FullName;$relative='receipts/'+[IO.Path]::GetRelativePath($handoff,$source.FullName)
 $target=[IO.Path]::GetFullPath((Join-Path $delta $relative))
 if(-not $target.StartsWith($delta+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'delta-containment'}
 [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))|Out-Null
 [IO.File]::Copy($source.FullName,$target,$false);Check $target $entry.sha256 $entry.bytes
 @{relative=$relative;source=$entry.path;sha256=$entry.sha256;bytes=$entry.bytes}
})
$sources=@(Get-ChildItem -LiteralPath (Join-Path $taskRepo 'research/bvi_windows'),(Join-Path $taskRepo 'tools/zero_miss_windows') -File -Recurse|ForEach-Object {Entry $_})
foreach($source in $sources){
 $rel=[IO.Path]::GetRelativePath($taskRepo,$source.path).Replace('\','/')
 $blob=(& git -C $taskRepo hash-object --no-filters -- $source.path).Trim()
 $expected=(& git -C $taskRepo rev-parse ($sourceCommit+':'+$rel)).Trim()
 if($LASTEXITCODE -ne 0 -or $blob -cne $expected){throw 'final-source-git-blob'}
}
$binaries=@(foreach($mode in @('release','debug','asan')){
 Get-ChildItem -LiteralPath (Join-Path $taskRepo ('out/bridge-win-profile-'+$mode+'-01')) -File|
 Where-Object Extension -in '.exe','.dll','.lib'|ForEach-Object {Entry $_}
})
NewJson (Join-Path $delta 'source-and-binary-sha.json') @{schema='pas.windows-final-offline-profile-identity.v1';
 development_source_commit=$sourceCommit;sources=$sources;binaries=$binaries;stages=$stages;regressions=$results;
 profile=@{width=1280;height=720;stride=3840;rotation=1;pixel_format='RGB888 top-down'};
 prior_checkpoint='0f81a387676ec67b470f537812113cbe33df8fa0';prior_binaries_verified_unchanged=$oldSources.binaries.Count;
 prior_raw_source_snapshot=(Join-Path $oldSourceRoot 'manifest.json');prior_snapshot_git_blobs_verified=$oldSource.files.Count;
 scope='Exact v3 and original formal closure unchanged; rotation profile guard only. All current builds new, no overwritten binaries.'}
$outBytes=(Get-ChildItem -LiteralPath (Join-Path $taskRepo 'out') -File -Recurse|Measure-Object Length -Sum).Sum
if($outBytes -gt 12884901888){throw 'out-byte-cap'}
NewJson (Join-Path $delta 'final-verification.json') @{schema='pas.windows-final-integrity.v1';passed=$true;
 source_commit=$sourceCommit;delivery_tree=$deliveryTree;unchanged_v3_core_files=7;unchanged_formal_sources=$true;
 original_head=$originalHead;original_status=$originalDirty;original_dirty_aux_sha256=$auxSha;
 preserved_original_STOP_states_verified=$external.original_stop_states.Count;
 selected_original_pngs_verified=$selection.frames.Count;physical_human_gold=0;device_commands=0;
 unchanged_old_archive_files_verified=$sealed.files.Count;unchanged_old_binaries_verified=$oldSources.binaries.Count;
 final_stages_verified=$stages.Count;all_native_owned_processes_final_zero=$true;
 legacy_failures_retained=36;legacy_oracles_modified=$false;root_ctest_timeouts_retained=2;
 out_bytes=$outBytes;out_hard_cap_bytes=12884901888;free_bytes=(Get-PSDrive C).Free;
 metadata_hard_cap_bytes=67108864;product_goal_completed=$false;runtime_cost_gate='NOT_READY'}
NewJson (Join-Path $delta 'copy-index.json') @{schema='pas.windows-append-only-evidence-copy.v1';copies=$copies}
$all=@(Get-ChildItem -LiteralPath $archive -File -Recurse|Sort-Object FullName|ForEach-Object {
 $e=Entry $_.FullName;@{relative=[IO.Path]::GetRelativePath($archive,$_.FullName).Replace('\','/');bytes=$e.bytes;sha256=$e.sha256}
})
NewJson (Join-Path $archive 'SHA256_INDEX_FINAL.json') @{schema='pas.windows-final-evidence-index.v1';files=$all;
 excluded_self='SHA256_INDEX_FINAL.json';source_commit=$sourceCommit;prior_index_retained='SHA256_INDEX.json'}
$archiveBytes=(Get-ChildItem -LiteralPath $archive -File -Recurse|Measure-Object Length -Sum).Sum
if($archiveBytes -gt 67108864){throw 'final-archive-cap'}
[ordered]@{passed=$true;new_copies=$copies.Count;all_indexed_files=$all.Count;archive_bytes=$archiveBytes;out_bytes=$outBytes;stages=$stages.Count;prefix=113;real_frames=256;old_STOPs=$external.original_stop_states.Count}|ConvertTo-Json
