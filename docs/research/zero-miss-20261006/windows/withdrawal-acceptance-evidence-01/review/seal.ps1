$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo='C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006'
$original='C:\Users\wurre\Desktop\Phigros-Auto-play-System'
$out=Join-Path $repo 'out/windows-handoff'
$destination=Join-Path $repo 'docs/research/zero-miss-20261006/windows/withdrawal-acceptance-evidence-01'
function Read-Json([string]$path){Get-Content -LiteralPath $path -Raw|ConvertFrom-Json}
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Require([bool]$condition,[string]$message){if(!$condition){throw $message}}
function Check-File($entry){Require ((Get-Item -LiteralPath $entry.path).Length -eq $entry.bytes) "size: $($entry.path)";Require ((Hash $entry.path) -eq $entry.sha256) "SHA: $($entry.path)"}
function Fresh-Json([string]$path,$value){
    $bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 70)+"`n")
    $stream=[IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
    try{$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}
Require (!(Test-Path -LiteralPath $destination)) 'fresh archive required'
Require ((Hash (Join-Path $original 'phigros-zero-miss-handoff-round4-9dc99d6.zip')) -eq '18040114639c560c0ed33d6d7949affd1a4fb1ce2be807adf1f53b948656ee9b') 'ZIP SHA'
$originalHead=(& git -C $original rev-parse HEAD).Trim()
Require ($originalHead -eq '74e54437d4a3ad2b2bd1a3b09312211a92f2359e') 'original HEAD'
$dirty=@(& git -C $original status --porcelain)
Require ($dirty.Count -eq 2 -and $dirty -contains ' M apps/runtime_x11_p/aux/CMakeLists.txt' -and $dirty -contains '?? phigros-zero-miss-handoff-round4-9dc99d6.zip') 'original dirty state'
$aux='\\?\'+(Join-Path $original 'apps/runtime_x11_p/aux/CMakeLists.txt').Replace('/','\')
Require ((Hash $aux) -eq 'e5c78a1d90a5e430c4f7c5328c7e9a8b3571800b0b8fc698d082b1cbe16d5501') 'user AUX SHA'
$oldArchive=Join-Path $repo 'docs/research/zero-miss-20261006/windows/evidence'
$oldIndex=Read-Json (Join-Path $oldArchive 'SHA256_INDEX_FINAL.json')
foreach($file in $oldIndex.files){Check-File ([pscustomobject]@{path=Join-Path $oldArchive $file.relative;bytes=$file.bytes;sha256=$file.sha256})}
$external=Read-Json (Join-Path $oldArchive 'external-originals.json')
foreach($file in @($external.existing_inputs)+@($external.original_stop_states)){Check-File $file}
$selection=Read-Json (Join-Path $out 'real-pixels-selection-02/selection.json')
foreach($file in $selection.frames){Check-File $file}
Require ((Hash $selection.index_path) -eq $selection.index_sha256) 'record index SHA'
Require ((Hash (Join-Path $selection.record_root 'manifest.json')) -eq $selection.record_manifest_sha256) 'record manifest SHA'
$v3=[ordered]@{
'bvi.cpp'='866ad2440d088ec1caf360eb5d4a9c796fdfcbcf2e15aff92374baf2291ea824'
'bvi.hpp'='565e7c058328d01b11b75e7bf05a96a96dcdd378b42f000a422c000957671d5f'
'constraint_policy.cpp'='693cb7a1fa177035e1542c772dd2e26e63cd280fcdc34067aea4f3959297c821'
'contact_policy.cpp'='b08520d87591b914fd357205e506a5080abb690dc4efe08c6e44e1225aea1c6f'
'contact_policy.hpp'='08f48f197ef36449bc04b81cdd27005c50e56bc4766fde9e5d3bc58c38ccb691'
'relation_policy.cpp'='37a23c696654b3935a51d422991f260213094a48d1488dc45e74334b29ac1efc'
'relation_policy.hpp'='bbb47373899ac0531e424849aaa3d32638348ea25e8ebf682ddfaf906ac9feb0'
}
foreach($name in $v3.Keys){Require ((Hash (Join-Path $repo "research/bvi_cold_v3/$name")) -eq $v3[$name]) "v3: $name"}
$stages=@(Get-ChildItem -LiteralPath $out -Directory|Where-Object {$_.Name -match '-bridge-withdraw-' -and $_.Name -notmatch '-inputs$|-pixel-controls$'}|Sort-Object Name)
$facts=@();$frozenEntries=0;$uniqueFreeze=@{};$binaries=@()
foreach($stage in $stages){
    $name=$stage.Name;$verification=Read-Json (Join-Path $stage.FullName "$name-verification.json")
    $result=Read-Json (Join-Path $stage.FullName "$name-result.json")
    $value=$result.facts;$freeze=Read-Json (Join-Path $stage.FullName 'freeze.json')
    if($verification.PSObject.Properties.Name -contains 'entries'){foreach($entry in $verification.entries){Check-File $entry}}
    foreach($file in $freeze.files){if(!$uniqueFreeze.ContainsKey($file.path)){Check-File $file;$uniqueFreeze[$file.path]=$file.sha256}else{Require ($uniqueFreeze[$file.path] -eq $file.sha256) "conflicting frozen SHA: $($file.path)"};$frozenEntries++}
    Require ($value.active_final -eq 0 -and $value.held_all_signaled -and $value.streams_completed) "unfinished stage: $name"
    $stop=$name -eq 'prefix-bridge-withdraw-release-01'
    $negative=$name -match '^audit-bad-'
    $expectedNative=if($negative){1}else{0};$expectedRunner=if($stop){125}else{$expectedNative};$expectedVerifier=if($stop){1}else{0}
    Require ($value.exit_code -eq $expectedNative -and $value.runner_exit -eq $expectedRunner -and $verification.verification_exit -eq $expectedVerifier) "unexpected stage exit: $name"
    Require ($stop -or $value.identity_trusted) "untrusted stage: $name"
    $facts+=[ordered]@{stage=$name;native_exit=$value.exit_code;runner_exit=$value.runner_exit;verification_exit=$verification.verification_exit;active_final=$value.active_final;held_all_signaled=$value.held_all_signaled;streams_completed=$value.streams_completed;identity_trusted=$value.identity_trusted;elapsed_s=$value.elapsed_s;classification=if($stop){'preserved process integrity STOP'}elseif($negative){'expected rejection control'}else{'verified pass'}}
}
$configurations=@()
foreach($mode in @('release','debug','asan')){
    $attempt=if($mode -eq 'release'){'02'}else{'01'}
    $prefix=Read-Json (Join-Path $out "prefix-withdraw-$mode-$attempt.json")
    $diagnostic=Read-Json (Join-Path $out "diagnostic-withdraw-$mode-$attempt.json")
    $inherited=Read-Json (Join-Path $out "inherited-audit-withdraw-$mode-$attempt.json")
    $audit=Read-Json (Join-Path $out "audit-current-withdraw-$mode-01.json")
    $pixels=Read-Json (Join-Path $out "pixels-withdraw-$mode-01.json")
    $pixelControls=Read-Json (Join-Path $out "pixels-bridge-withdraw-$mode-01-pixel-controls/verification.json")
    Require ($prefix.assertions -eq 113 -and $prefix.failed_assertions -eq 0) "prefix: $mode"
    Require ($diagnostic.assertions -eq 32 -and $diagnostic.failed_assertions -eq 0) "diagnostics: $mode"
    foreach($a in @($inherited,$audit)){Require ($a.passed -and $a.compared_rows -eq 256 -and $a.raw_png_points_verified -eq 1665) "audit: $mode"}
    Require ($pixels.processed_frames -eq 256 -and $pixels.failed -eq 0 -and $pixels.invalid_frames -eq 0 -and $pixels.allowed_targets -eq 0 -and $pixels.own_fake_downs -eq 0 -and $pixels.own_fake_moves -eq 0 -and $pixels.own_fake_ups -eq 0 -and $pixels.final_release_verified -and $pixelControls.passed) "pixels: $mode"
    $build=Join-Path $repo "out/bridge-win-withdraw-$mode-01"
    foreach($file in (Get-ChildItem -LiteralPath $build -File|Where-Object {$_.Extension -in '.exe','.lib','.dll'})){$binaries+=[ordered]@{configuration=$mode;path=$file.FullName;bytes=$file.Length;sha256=Hash $file.FullName}}
    $configurations+=[ordered]@{configuration=$mode;prefix_assertions=113;diagnostic_assertions=32;decision_rows=256;raw_png_points=1665;inherited_audit_passed=$true;current_audit_passed=$true;eligible=0;fake_down=0;compute_qpc_ms=$pixels.complete_observer_bridge_owner_scheduler_receipts_ms;decode_and_compute_qpc_ms=$pixels.decode_and_complete_chain_ms;timing_excludes=$pixels.timing_excludes;runtime_cost_gate=$pixels.runtime_cost_gate}
}
foreach($case in @('policy','rgb')){$control=Read-Json (Join-Path $out "audit-bad-$case-withdraw-release-01.json");Require (!$control.passed -and $control.error -eq $(if($case -eq 'policy'){'audit decision field changed'}else{'audit raw PNG pixel mismatch'})) "negative: $case"}
$newBuildBytes=[long]0
foreach($mode in @('release','debug','asan')){$newBuildBytes+=(Get-ChildItem -LiteralPath (Join-Path $repo "out/bridge-win-withdraw-$mode-01") -Recurse -File|Measure-Object Length -Sum).Sum}
$outStageBytes=($stages|ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -Recurse -File}|Measure-Object Length -Sum).Sum
Require ($newBuildBytes+$outStageBytes -lt 2GB) 'new build/stage budget exceeded'
Require ((Get-PSDrive C).Free -gt 20GB) 'minimum free capacity'
$sourceFiles=@(& git -C $repo diff-tree --no-commit-id --name-only -r 774f3e7020818e94c508cc1481dd0986ebe417a0)
Require (@($sourceFiles|Where-Object {$_ -notlike 'research/bvi_windows/*'}).Count -eq 0) 'source changes escaped standalone bridge'
$copyPlan=[Collections.Generic.List[object]]::new()
function Plan([string]$source,[string]$relative){$item=Get-Item -LiteralPath $source;Require (!$item.PSIsContainer -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) 'archive file/reparse';$copyPlan.Add([pscustomobject]@{source=$item.FullName;relative=$relative;bytes=$item.Length;sha256=Hash $item.FullName})}
foreach($stage in $stages){foreach($dir in @(($stage.FullName),($stage.FullName+'-inputs'),($stage.FullName+'-pixel-controls'))){if(Test-Path -LiteralPath $dir){foreach($file in (Get-ChildItem -LiteralPath $dir -Recurse -File)){Plan $file.FullName ('receipts/'+[IO.Path]::GetRelativePath($out,$file.FullName).Replace('\','/'))}}}}
foreach($file in (Get-ChildItem -LiteralPath $out -File|Where-Object {$_.Name -match '^(prefix|diagnostic|inherited-audit|audit-(current|bad-policy|bad-rgb)|pixels)-withdraw-|^audit-(current|bad-policy|bad-rgb)-withdraw-|^pixels-(review|profile)-release-01\.json'})){Plan $file.FullName ('results/'+$file.Name)}
foreach($file in (Get-ChildItem -LiteralPath (Join-Path $out 'withdrawal-review-01') -Recurse -File)){Plan $file.FullName ('review/'+[IO.Path]::GetRelativePath((Join-Path $out 'withdrawal-review-01'),$file.FullName).Replace('\','/'))}
Plan (Join-Path $out 'real-pixels-selection-02/selection.json') 'selection.json'
Plan (Join-Path $repo 'docs/research/zero-miss-20261006/windows/HUMAN_REVIEW_20261006.json') 'HUMAN_REVIEW_20261006.json'
foreach($file in $sourceFiles){Plan (Join-Path $repo $file) ('source-774f3e7/'+$file)}
$partial=Join-Path $repo 'docs/research/zero-miss-20261006/windows/real-pixels-review-evidence-01'
$partialFiles=@(Get-ChildItem -LiteralPath $partial -Recurse -File|ForEach-Object {[ordered]@{relative=[IO.Path]::GetRelativePath($partial,$_.FullName).Replace('\','/');bytes=$_.Length;sha256=Hash $_.FullName}})
$plannedBytes=($copyPlan|Measure-Object bytes -Sum).Sum
Require ($plannedBytes+1MB -lt 16MB) 'new metadata budget/reserve exceeded'
[IO.Directory]::CreateDirectory($destination)|Out-Null
foreach($entry in $copyPlan){
    $target=[IO.Path]::GetFullPath((Join-Path $destination $entry.relative))
    Require ($target.StartsWith($destination+'\',[StringComparison]::OrdinalIgnoreCase)) 'archive containment'
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))|Out-Null
    [IO.File]::Copy($entry.source,$target,$false)
    Require ((Hash $target) -eq $entry.sha256) "copy SHA: $target"
}
Fresh-Json (Join-Path $destination 'partial-child-archive-index.json') ([ordered]@{status='failed/interrupted export, no original completed seal; preserved as partial only';root=$partial;files=$partialFiles})
Fresh-Json (Join-Path $destination 'binaries-external.json') $binaries
$summary=[ordered]@{schema='pas.withdrawal-acceptance-and-own-development.v1';source_commit='774f3e7020818e94c508cc1481dd0986ebe417a0';withdrawn_source_commit='57b00d566db805d4f62bc63f054b3b7a268e7734';withdrawn_thread='01a10f9b-5ada-7a33-8347-2d9be79405f8';thread_title='Phigros 真圖橋接與離線開發';withdrawn_thread_last_verified='idle; withdrawal turn completed; no further build/edit/commit authorized';original_head=$originalHead;original_dirty=$dirty;zip_sha256_verified=$true;user_aux_sha256_verified=$true;old_sealed_metadata_verified=$oldIndex.files.Count;old_stop_states_verified=$external.original_stop_states.Count;old_external_inputs_verified=$external.existing_inputs.Count;selected_pngs_verified=$selection.frames.Count;v3_core_unchanged=$v3;freeze_entries_rechecked=$frozenEntries;unique_frozen_files_rechecked=$uniqueFreeze.Count;formal_source_and_oracles_changed=$false;configurations=$configurations;stages=$facts;initial_release_process_stop_preserved=$true;expected_negative_rejections=2;new_human_semantic_labels=1;physical_legal_opportunity_gold=0;frame1519_identity='unknown';gameplay_runs=0;device_commands=0;original_legacy_failures=36;original_legacy_rerun_in_this_diagnostic_change=$false;product_completed=$false;runtime_cost_gate='not_ready';new_build_bytes=$newBuildBytes;new_stage_bytes=$outStageBytes;planned_metadata_bytes=$plannedBytes;new_out_cap_bytes=2GB;new_metadata_cap_bytes=16MB;minimum_free_bytes=20GB;free_bytes=(Get-PSDrive C).Free;copied_sources=$copyPlan}
Fresh-Json (Join-Path $destination 'verification-and-provenance.json') $summary
$archiveFiles=@(Get-ChildItem -LiteralPath $destination -Recurse -File|Sort-Object FullName|ForEach-Object {[ordered]@{relative=[IO.Path]::GetRelativePath($destination,$_.FullName).Replace('\','/');bytes=$_.Length;sha256=Hash $_.FullName}})
Fresh-Json (Join-Path $destination 'SHA256_INDEX.json') ([ordered]@{schema='pas.immutable-local-evidence-index.v1';excluded_self='SHA256_INDEX.json';files=$archiveFiles})
$actual=(Get-ChildItem -LiteralPath $destination -Recurse -File|Measure-Object Length -Sum).Sum
Require ($actual -lt 16MB) 'final metadata budget'
foreach($entry in $archiveFiles){Check-File ([pscustomobject]@{path=Join-Path $destination $entry.relative;bytes=$entry.bytes;sha256=$entry.sha256})}
[ordered]@{archive=$destination;indexed_files=$archiveFiles.Count;archive_bytes=$actual;native_stages=$facts.Count;new_build_bytes=$newBuildBytes;frozen_entries=$frozenEntries;unique_frozen=$uniqueFreeze.Count;selected_pngs=$selection.frames.Count;old_metadata=$oldIndex.files.Count;preserved_old_STOP=$external.original_stop_states.Count;new_human_semantic_labels=1;device_commands=0}|ConvertTo-Json
