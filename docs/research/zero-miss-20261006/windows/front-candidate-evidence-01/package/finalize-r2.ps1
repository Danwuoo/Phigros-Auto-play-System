$ErrorActionPreference='Stop'
$repo='C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006'
$out=Join-Path $repo 'out/windows-handoff'
$pkg=$PSScriptRoot
$dest=Join-Path $repo 'docs/research/zero-miss-20261006/windows/front-candidate-evidence-01'
function NewBytes($path,[byte[]]$bytes){
 [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))|Out-Null
 $s=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
 try{$s.Write($bytes);$s.Flush($true)}finally{$s.Dispose()}
}
function NewJson($path,$value){NewBytes $path ([Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 50)+"`n"))}
function Entry($path){$f=Get-Item -LiteralPath $path;@{path=$f.FullName;bytes=$f.Length;sha256=(Get-FileHash -LiteralPath $f.FullName).Hash.ToLowerInvariant()}}
function Check($entry){$f=Get-Item -LiteralPath $entry.path;if($f.Length -ne $entry.bytes -or (Get-FileHash -LiteralPath $f.FullName).Hash.ToLowerInvariant() -cne $entry.sha256){throw ('SHA mismatch '+$entry.path)}}
function CopyNew($source,$destination){NewBytes $destination ([IO.File]::ReadAllBytes($source))}
$start=Get-Content -LiteralPath (Join-Path $pkg 'capacity-start.json') -Raw|ConvertFrom-Json
foreach($f in $start.protected_stops){if((Get-FileHash -LiteralPath $f.path).Hash.ToLowerInvariant() -cne $f.sha256){throw 'protected STOP changed'}}
$old=0;$sealedStops=0
foreach($base in @('docs/research/zero-miss-20261006/windows/evidence','docs/research/zero-miss-20261006/windows/withdrawal-acceptance-evidence-01')){
 $base=Join-Path $repo $base
 $index=Join-Path $base $(if($base.EndsWith('evidence')){'SHA256_INDEX_FINAL.json'}else{'SHA256_INDEX.json'})
 $items=(Get-Content -LiteralPath $index -Raw|ConvertFrom-Json).files
 foreach($f in $items){$path=Join-Path $base $f.relative;Check @{path=$path;bytes=$f.bytes;sha256=$f.sha256};$old++
  if($f.relative.EndsWith('state.json')){$state=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json;if($state.status -like '*STOP*'){$sealedStops++}}
 }
}
$selectionPath=Join-Path $out 'real-pixels-selection-02/selection.json'
$selection=Get-Content -LiteralPath $selectionPath -Raw|ConvertFrom-Json
foreach($f in $selection.frames){Check $f}
if((Get-FileHash -LiteralPath $selection.index_path).Hash.ToLowerInvariant() -cne $selection.index_sha256 -or
 (Get-FileHash -LiteralPath (Join-Path $selection.record_root 'manifest.json')).Hash.ToLowerInvariant() -cne $selection.record_manifest_sha256){throw 'record manifest/index'}
$core=@();foreach($line in [IO.File]::ReadLines((Join-Path $repo 'docs/research/zero-miss-20261005/round4/evidence/final-core.sha256'))){
 if(-not $line.Trim()){continue};$parts=$line -split '\s+',2;$p=Join-Path $repo $parts[1];$e=Entry $p;if($e.sha256 -cne $parts[0]){throw 'v3 core changed'};$core+=$e
}
$source='e26231ec5acef9634df007770c1f06031d5326d4'
if((& git -C $repo rev-parse HEAD) -cne $source){throw 'source commit'}
$original='C:\Users\wurre\Desktop\Phigros-Auto-play-System'
if((& git -C $original rev-parse HEAD) -cne '74e54437d4a3ad2b2bd1a3b09312211a92f2359e'){throw 'original HEAD'}
$aux='\\?\C:\Users\wurre\Desktop\Phigros-Auto-play-System\apps\runtime_x11_p\aux\CMakeLists.txt'
$auxSha=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([IO.File]::ReadAllBytes($aux))).ToLowerInvariant()
if($auxSha -cne 'e5c78a1d90a5e430c4f7c5328c7e9a8b3571800b0b8fc698d082b1cbe16d5501'){throw 'user AUX changed'}
$zip=Entry (Join-Path $original 'phigros-zero-miss-handoff-round4-9dc99d6.zip')
if($zip.sha256 -cne '18040114639c560c0ed33d6d7949affd1a4fb1ce2be807adf1f53b948656ee9b'){throw 'user ZIP changed'}
$stages=@();$receiptsDirs=@(Get-ChildItem -LiteralPath $out -Directory|Where-Object {$_.Name -like '*hold-front*' -and $_.Name -ne 'hold-front-package-01'})
foreach($dir in $receiptsDirs){
 $statePath=Join-Path $dir.FullName 'state.json';if(-not(Test-Path $statePath)){continue}
 $state=Get-Content -LiteralPath $statePath -Raw|ConvertFrom-Json
 if($state.status -notin @('COMPLETE','STOP')){throw ('unfinished stage '+$dir.Name)}
 $r=Get-Content -LiteralPath (Join-Path $dir.FullName ($dir.Name+'-result.json')) -Raw|ConvertFrom-Json
 $v=Get-Content -LiteralPath (Join-Path $dir.FullName ($dir.Name+'-verification.json')) -Raw|ConvertFrom-Json
 if($r.facts.active_final -ne 0 -or -not $r.facts.held_all_signaled -or -not $r.facts.streams_completed){throw 'process not closed'}
 if($state.status -eq 'COMPLETE'){if($v.pending -or $v.verification_exit -ne 0){throw 'incomplete receipt'};foreach($f in $v.entries){Check $f}}
 $stages+=@{stage=$dir.Name;status=$state.status;native=$r.facts.exit_code;runner=$r.facts.runner_exit;verification=$v.verification_exit;
  reason=$state.reason;active_final=$r.facts.active_final;held_signaled=$r.facts.held_all_signaled;streams_completed=$r.facts.streams_completed;
  identity_trusted=$r.facts.identity_trusted;elapsed_s=$r.facts.elapsed_s}
}
$configs=@();$linked=@()
foreach($mode in @('release','debug','asan')){
 $attempt=if($mode -eq 'release'){'03'}else{'02'}
 $freeze=Get-Content -LiteralPath (Join-Path $out ('build-hold-front-'+$mode+'-'+$attempt+'/freeze.json')) -Raw|ConvertFrom-Json
 foreach($f in $freeze.files){Check $f}
 $closurePath=Join-Path $pkg ('closure-'+$mode+'-'+$attempt+'.json');$closure=Get-Content -LiteralPath $closurePath -Raw|ConvertFrom-Json
 foreach($lib in $closure.libraries.PSObject.Properties){Check $lib.Value;$linked+=@{configuration=$mode;name=$lib.Name;file=$lib.Value}}
 $front=Get-Content -LiteralPath (Join-Path $pkg ('front-'+$mode+'-'+$attempt+'.json')) -Raw|ConvertFrom-Json
 $prefix=Get-Content -LiteralPath (Join-Path $pkg ('prefix-'+$mode+'-'+$attempt+'.json')) -Raw|ConvertFrom-Json
 $pixel=Get-Content -LiteralPath (Join-Path $pkg ('pixels-'+$mode+'-'+$attempt+'.json')) -Raw|ConvertFrom-Json
 $audit=Get-Content -LiteralPath (Join-Path $pkg ('audit-'+$mode+'-'+$attempt+'.json')) -Raw|ConvertFrom-Json
 if($front.assertions -ne 56 -or $front.failed_assertions -ne 0 -or $prefix.assertions -ne 113 -or $prefix.failed_assertions -ne 0 -or
  $pixel.processed_frames -ne 256 -or $pixel.allowed_targets -ne 0 -or $pixel.own_fake_downs -ne 0 -or $pixel.own_fake_moves -ne 0 -or $pixel.own_fake_ups -ne 0 -or -not $pixel.final_release_verified -or
  -not $audit.passed -or $audit.compared_rows -ne 256 -or $audit.front_raw_candidates -ne 859 -or $audit.visible_terminal_proposals -ne 191 -or $audit.front_edge_raw_points_verified -ne 1910 -or $audit.raw_png_points_verified -ne 1665){throw 'final native report contract'}
 $configs+=@{mode=$mode;build='out/hold-front-'+$mode+'-'+$attempt;front_checks=$front.assertions;prefix_checks=$prefix.assertions;rows=$audit.compared_rows;
  raw_proposals=$audit.front_raw_candidates;terminal_proposals=$audit.visible_terminal_proposals;front_raw_points=$audit.front_edge_raw_points_verified;original_raw_points=$audit.raw_png_points_verified;
  baseline_compute_qpc_ms=$pixel.unchanged_baseline_chain_compute_ms;sidecar_compute_qpc_ms=$pixel.post_dispatch_front_sidecar_compute_ms;timing_excludes=$pixel.timing_excludes;
  kind_counts=$audit.front_kind_counts;reason_counts=$audit.front_reason_counts}
}
foreach($kind in @('epoch','geometry','front-rgb')){
 $controlAttempt=if($kind -eq 'front-rgb'){'neg-front-rgb-02'}else{'neg-'+$kind}
 $controlStage=@($stages|Where-Object stage -eq ('audit-hold-front-release-'+$controlAttempt))
 if($controlStage.Count -ne 1 -or $controlStage[0].status -ne 'COMPLETE' -or $controlStage[0].native -ne 1 -or $controlStage[0].runner -ne 1 -or $controlStage[0].verification -ne 0){throw 'negative stage not accepted'}
 $j=Get-Content -LiteralPath (Join-Path $pkg ('audit-release-'+$controlAttempt+'.json')) -Raw|ConvertFrom-Json
 $expected=if($kind -eq 'front-rgb'){'audit front raw RGB mismatch'}else{'audit full declared offline context mismatch'}
 if($j.passed -or $j.error -cne $expected){throw 'negative audit not rejected'}
}
$roots=@(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -Directory -Filter 'hold-front-*')
$buildBytes=[long]0;foreach($dir in $roots){$buildBytes+=[long](Get-ChildItem $dir.FullName -Recurse -File|Measure-Object Length -Sum).Sum}
$stageBytes=[long]0;foreach($dir in $receiptsDirs){$stageBytes+=[long](Get-ChildItem $dir.FullName -Recurse -File|Measure-Object Length -Sum).Sum}
$packageBytes=[long](Get-ChildItem $pkg -Recurse -File|Measure-Object Length -Sum).Sum
if($buildBytes+$stageBytes+$packageBytes -gt 2147483648 -or (Get-PSDrive C).Free -lt 21474836480){throw 'new out capacity'}
$bins=@();foreach($dir in $roots){foreach($f in Get-ChildItem $dir.FullName -File|Where-Object Extension -in '.exe','.lib','.dll'){$bins+=Entry $f.FullName}}
$externalControls=@(Get-ChildItem $pkg -Filter 'negative-*.rows.jsonl' -File|ForEach-Object {Entry $_.FullName})
$ownSource=@(Get-ChildItem (Join-Path $repo 'research/hold_front_current') -File|ForEach-Object {Entry $_.FullName})
$v1=Get-Content -LiteralPath (Join-Path $pkg 'source-v1/source-index.json') -Raw|ConvertFrom-Json;foreach($f in $v1){Check $f}
NewJson (Join-Path $pkg 'binaries-and-controls-external.json') @{binaries=$bins;linked_libraries=$linked;negative_control_traces=$externalControls;copied=$false}
NewJson (Join-Path $pkg 'final-verification.json') @{schema='pas.current-front-handoff.v1';source_commit=$source;first_source_commit='28493f12e45e2110e74b4d5b0ab99a03f6755cf6';
 branch='codex/hold-visible-front-20261006';base='4fe388428c7ccca4ef15d18a1d8e44cddd5198c3';own_source=$ownSource;v3_core=$core;linked_libraries=$linked;
 configurations=$configs;all_stages=$stages;expected_negative_rejections=3;new_process_stops=@($stages|Where-Object status -eq 'STOP');
 old_sealed_files_verified=$old;old_sealed_STOPs_verified=$sealedStops;old_current_out_STOPs_verified=$start.protected_stops.Count;
 original_HEAD='74e54437d4a3ad2b2bd1a3b09312211a92f2359e';original_dirty=@(& git -C $original status --short);user_aux_sha=$auxSha;user_zip=$zip;
 original_index_sha=$selection.index_sha256;original_manifest_sha=$selection.record_manifest_sha256;selected_original_PNGs_verified=$selection.frames.Count;
 build_bytes=$buildBytes;stage_bytes=$stageBytes;package_bytes_before_final_reports=$packageBytes;out_cap=2147483648;metadata_cap=16777216;free_bytes=(Get-PSDrive C).Free;
 protected_original_tests_changed=$false;original36_fail_preserved=$true;root349_and_v3_3938_rerun=$false;original_timeouts_and_skips_preserved=$true;
 product_completed=$false;cost_gate='NOT_READY';head_role='proposed except external human approximate 3030 label';physical_legal_opportunity_gold=0;
 device_commands=0;new_goals=0;new_agents_or_tasks=0;push_PR_merge=$false;
 timing_v1_correction='v1 baseline chain report excluded new measure/consume/shadow, despite generic old timing_excludes; v2 explicitly renames and separately measures sidecar; no full candidate cost qualification'}
if(Test-Path $dest){throw 'fresh archive required'}
[IO.Directory]::CreateDirectory($dest)|Out-Null
NewBytes (Join-Path $dest '.gitattributes') ([Text.Encoding]::UTF8.GetBytes("* -text`n"))
foreach($dir in $receiptsDirs){foreach($f in Get-ChildItem $dir.FullName -Recurse -File){$relative=[IO.Path]::GetRelativePath($out,$f.FullName);CopyNew $f.FullName (Join-Path $dest ('receipts/'+$relative))}}
foreach($f in Get-ChildItem $pkg -Recurse -File){if($f.Name -like 'negative-*.rows.jsonl'){continue};$relative=[IO.Path]::GetRelativePath($pkg,$f.FullName);CopyNew $f.FullName (Join-Path $dest ('package/'+$relative))}
foreach($f in $ownSource){CopyNew $f.path (Join-Path $dest ('source-e26231e/'+[IO.Path]::GetFileName($f.path)))}
foreach($dir in $roots){foreach($leaf in @('CMakeCache.txt','build.ninja','CMakeFiles/rules.ninja')){$path=Join-Path $dir.FullName $leaf;if(Test-Path $path){CopyNew $path (Join-Path $dest ('build-maps/'+$dir.Name+'/'+$leaf))}}}
CopyNew $selectionPath (Join-Path $dest 'selection.json')
CopyNew (Join-Path $repo 'docs/research/zero-miss-20261006/windows/HUMAN_REVIEW_20261006.json') (Join-Path $dest 'HUMAN_REVIEW_20261006.json')
$items=@(Get-ChildItem $dest -Recurse -File|Sort-Object FullName|ForEach-Object { $e=Entry $_.FullName;@{relative=[IO.Path]::GetRelativePath($dest,$_.FullName).Replace('\','/');bytes=$e.bytes;sha256=$e.sha256}})
$metadataBytes=[long]($items|Measure-Object bytes -Sum).Sum
$indexValue=@{schema='pas.current-front-sha-index.v1';excluded_self='SHA256_INDEX.json';files=$items;external_controls_preserved_in_out=$externalControls;source_commit=$source}
$indexBytes=[Text.Encoding]::UTF8.GetBytes(($indexValue|ConvertTo-Json -Depth 20)+"`n")
if($metadataBytes+$indexBytes.Length -gt 16777216){throw 'metadata cap: partial archive retained, not sealed'}
NewBytes (Join-Path $dest 'SHA256_INDEX.json') $indexBytes
foreach($f in $items){Check @{path=(Join-Path $dest $f.relative);bytes=$f.bytes;sha256=$f.sha256}}
NewJson (Join-Path $pkg 'seal-result.json') @{complete=$true;files=$items.Count+1;indexed=$items.Count;bytes=$metadataBytes+$indexBytes.Length;index=(Entry (Join-Path $dest 'SHA256_INDEX.json'));external_controls=$externalControls}
Get-Content -LiteralPath (Join-Path $pkg 'seal-result.json') -Raw