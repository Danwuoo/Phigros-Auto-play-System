$ErrorActionPreference='Stop'
$taskRepo='C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006'
$outRoot=Join-Path $taskRepo 'out/windows-handoff'
$root=$PSScriptRoot
function JsonNew($path,$value) {
 $bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 35)+"`n")
 if($bytes.Length -gt 1048576){throw 'acceptance-metadata-cap'}
 $s=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
 try{$s.Write($bytes);$s.Flush($true)}finally{$s.Dispose()}
}
function ShaCheck($file) {
 if((Get-FileHash -LiteralPath $file.path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $file.sha256 -or
    ($null -ne $file.bytes -and (Get-Item -LiteralPath $file.path).Length -ne $file.bytes)){throw ('inherited-sha: '+$file.path)}
}
$snapshot=Join-Path $root 'source-57b00d5'
[IO.Directory]::CreateDirectory($snapshot)|Out-Null
$sourcePath=Join-Path $taskRepo 'research/bvi_windows/bridge/pixel_chain.cpp'
$sourceSha=(Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash.ToLowerInvariant()
if($sourceSha -cne '7724f8e2a189bfd9b30431a520f4ef02b7aa33be763fed716edb9fb2a55e3bd4'){throw 'inherited-source'}
[IO.File]::Copy($sourcePath,(Join-Path $snapshot 'pixel_chain.cpp'),$false)
$checks=0;$stages=@()
foreach($name in @('configure-bridge-review-release-01','build-bridge-review-release-01','prefix-bridge-review-release-01','pixels-bridge-review-release-01')) {
 $stageRoot=Join-Path $outRoot $name
 $v=Get-Content -LiteralPath (Join-Path $stageRoot ($name+'-verification.json')) -Raw|ConvertFrom-Json
 $f=(Get-Content -LiteralPath (Join-Path $stageRoot ($name+'-result.json')) -Raw|ConvertFrom-Json).facts
 if($v.pending -or $v.native_exit -ne 0 -or $v.runner_exit -ne 0 -or $v.verification_exit -ne 0 -or
    $f.active_final -ne 0 -or -not $f.identity_trusted -or -not $f.held_all_signaled -or -not $f.streams_completed){throw ('inherited-stage: '+$name)}
 foreach($file in $v.entries){ShaCheck $file;++$checks}
 foreach($file in (Get-Content -LiteralPath (Join-Path $stageRoot 'freeze.json') -Raw|ConvertFrom-Json).files){ShaCheck $file;++$checks}
 $stages+=@{stage=$name;native_exit=$v.native_exit;runner_exit=$v.runner_exit;verification_exit=$v.verification_exit;active_final=$f.active_final}
}
$oldRoot=Join-Path $taskRepo 'docs/research/zero-miss-20261006/windows/evidence'
$old=Get-Content -LiteralPath (Join-Path $oldRoot 'SHA256_INDEX_FINAL.json') -Raw|ConvertFrom-Json
foreach($file in $old.files){ShaCheck @{path=(Join-Path $oldRoot $file.relative);sha256=$file.sha256;bytes=$file.bytes}}
$selection=Get-Content -LiteralPath (Join-Path $outRoot 'real-pixels-selection-02/selection.json') -Raw|ConvertFrom-Json
foreach($file in $selection.frames){ShaCheck $file}
ShaCheck @{path=$selection.index_path;sha256=$selection.index_sha256}
ShaCheck @{path=(Join-Path $selection.record_root 'manifest.json');sha256=$selection.record_manifest_sha256}
$pv=Get-Content -LiteralPath (Join-Path $outRoot 'pixels-bridge-review-release-01-pixel-controls/verification.json') -Raw|ConvertFrom-Json
$prefix=Get-Content -LiteralPath (Join-Path $outRoot 'prefix-review-release-01.json') -Raw|ConvertFrom-Json
$pixels=Get-Content -LiteralPath (Join-Path $outRoot 'pixels-review-release-01.json') -Raw|ConvertFrom-Json
if(-not $pv.passed -or -not $pv.before_verified -or -not $pv.after_verified -or $pv.frames_verified -ne 256 -or
   $prefix.assertions -ne 113 -or $prefix.failed_assertions -ne 0 -or $pixels.processed_frames -ne 256 -or
   $pixels.failed -ne 0 -or $pixels.invalid_frames -ne 0 -or $pixels.allowed_targets -ne 0 -or
   $pixels.own_fake_downs -ne 0 -or -not $pixels.final_release_verified){throw 'inherited-reports'}
$files=@(Get-ChildItem -LiteralPath (Join-Path $taskRepo 'out/bridge-win-review-release-01') -File|
 Where-Object Extension -in '.exe','.lib','.dll'|ForEach-Object {@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
$doc=Join-Path $taskRepo 'docs/research/zero-miss-20261006/windows/REAL_PIXELS_REVIEW.md'
[IO.File]::Copy($doc,(Join-Path $snapshot 'REAL_PIXELS_REVIEW.original.md'),$false)
$fact=[ordered]@{schema='pas.withdrawn-development-independent-acceptance.v1';
 source_commit='57b00d566db805d4f62bc63f054b3b7a268e7734';source_path=$sourcePath;source_sha256=$sourceSha;
 source_snapshot=(Join-Path $snapshot 'pixel_chain.cpp');stages=$stages;frozen_entries_rechecked=$checks;
 original_sealed_metadata_verified=$old.files.Count;original_selected_pngs_verified=$selection.frames.Count;
 inherited_release_prefix=113;inherited_release_pixels=256;eligible_targets=0;all_owned_stages_exited=$true;
 binaries=$files;code_status='diagnostic-only conditionally accepted; sample-bound comment and trace-cap documentation inconsistent';
 sample_bound_actual=185;sample_bound_comment=925;trace_cap_actual=33554432;trace_cap_documented=16777216;
 archive_status='partial preserved; failed PowerShell bare false and interrupted export; no completed seal';
 required_followup='independent C++ behavior audit, bounded diagnostic tests and corrected provenance; no change to v3 policy';
 human_gold=0;device_commands=0;new_package_out_cap_bytes=2147483648;new_package_metadata_cap_bytes=16777216;
 minimum_free_bytes=21474836480;free_bytes=(Get-PSDrive C).Free;old_STOPS_reopened=$false}
if($fact.free_bytes -lt $fact.minimum_free_bytes){throw 'free-space-reserve'}
JsonNew (Join-Path $root 'inherited-acceptance.json') $fact
$fact|Select-Object schema,frozen_entries_rechecked,original_sealed_metadata_verified,original_selected_pngs_verified,code_status,archive_status|ConvertTo-Json
