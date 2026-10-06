$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Set-Location -LiteralPath $repo
$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $out 'hold-front-package-01'
$docs=Join-Path $repo 'docs/research/zero-miss-20261006/windows'
$archive=Join-Path $docs 'front-controller-acceptance-evidence-01'
function Assert($ok,$why){if(-not $ok){throw $why}}
function Entry($path){$file=Get-Item -LiteralPath $path;@{path=$file.FullName;bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()}}
function Check($entry){$file=Get-Item -LiteralPath $entry.path;Assert ($file.Length -eq $entry.bytes -and (Get-FileHash -LiteralPath $entry.path).Hash.ToLowerInvariant() -ceq $entry.sha256) ('input-SHA: '+$entry.path)}
function NewJson($path,$value){
 $s=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
 try{$bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 35)+"`n");$s.Write($bytes);$s.Flush($true)}finally{$s.Dispose()}
}
$provenance=Get-Content (Join-Path $PSScriptRoot 'audit-after-02.json') -Raw|ConvertFrom-Json
$controls=Get-Content (Join-Path $PSScriptRoot 'control-input-audit.json') -Raw|ConvertFrom-Json
$comparison=Get-Content (Join-Path $PSScriptRoot 'trace-comparison.json') -Raw|ConvertFrom-Json
Assert ($provenance.passed -and $controls.passed -and $comparison.passed) 'controller-audits-incomplete'
$dirs=@(Get-ChildItem -LiteralPath $out -Directory|Where-Object {$_.Name -match '^(configure|build|front|prefix|pixels|audit)-hold-front-(release|debug|asan)-controller-' -and (Test-Path (Join-Path $_.FullName ($_.Name+'-verification.json')))})
Assert ($dirs.Count -eq 14) 'controller-stage-denominator'
$stages=@();foreach($dir in $dirs){
 $stage=$dir.Name;$v=Get-Content (Join-Path $dir.FullName ($stage+'-verification.json')) -Raw|ConvertFrom-Json
 $r=Get-Content (Join-Path $dir.FullName ($stage+'-result.json')) -Raw|ConvertFrom-Json
 $state=Get-Content (Join-Path $dir.FullName 'state.json') -Raw|ConvertFrom-Json
 Assert (-not $v.pending -and -not $r.pending -and $r.facts.active_final -eq 0 -and $r.facts.held_all_signaled -and $r.facts.streams_completed) ('stage-not-closed: '+$stage)
 Assert ($r.facts.created -and $r.facts.assigned -and $r.facts.resumed) 'owned-launch-contract'
 $stop=$stage -ceq 'pixels-hold-front-release-controller-01'
 $expected=if($stage -match 'audit-hold-front-release-controller-(epoch|geometry|rgb)$'){1}else{0}
 Assert ($r.facts.exit_code -eq $expected) ('native-unexpected: '+$stage)
 if($stop){Assert ($state.status -ceq 'STOP' -and $r.facts.runner_exit -eq 125 -and $v.verification_exit -eq 1 -and -not $r.facts.identity_trusted) 'STOP-not-preserved'}
 else {Assert ($state.status -ceq 'COMPLETE' -and $v.native_exit -eq $expected -and $v.runner_exit -eq $expected -and $v.verification_exit -eq 0 -and $r.facts.identity_trusted) ('stage-not-verified: '+$stage)}
 foreach($entry in $v.entries){Check $entry}
 $freeze=Get-Content (Join-Path $dir.FullName 'freeze.json') -Raw|ConvertFrom-Json
 foreach($entry in $freeze.files){Check $entry}
 $stages+=@{stage=$stage;status=$state.status;native=$r.facts.exit_code;runner=$r.facts.runner_exit;verification=$v.verification_exit;identity_trusted=$r.facts.identity_trusted;active_final=$r.facts.active_final;held_signaled=$r.facts.held_all_signaled;streams_completed=$r.facts.streams_completed;elapsed_s=$r.facts.elapsed_s;reason=$v.reason}
}
$contracts=@();foreach($mode in @('release','debug','asan')){
 $front=Get-Content (Join-Path $pkg ('front-'+$mode+'-controller-01.json')) -Raw|ConvertFrom-Json
 $prefix=Get-Content (Join-Path $pkg ('prefix-'+$mode+'-controller-01.json')) -Raw|ConvertFrom-Json
 Assert ($front.assertions -eq 56 -and $front.failed_assertions -eq 0 -and $prefix.assertions -eq 113 -and $prefix.failed_assertions -eq 0) ('contracts: '+$mode)
 $contracts+=@{mode=$mode;front_checks=56;front_failures=0;prefix_checks=113;prefix_failures=0;fresh_build=($mode -eq 'release');build=$(if($mode -eq 'release'){'out/hold-front-release-controller-01'}else{'out/hold-front-'+$mode+'-02'})}
}
$pixels=Get-Content (Join-Path $pkg 'pixels-release-controller-02.json') -Raw|ConvertFrom-Json
$audit=Get-Content (Join-Path $pkg 'audit-release-controller-02.json') -Raw|ConvertFrom-Json
Assert ($audit.passed -and $audit.compared_rows -eq 256 -and $audit.visible_terminal_proposals -eq 191 -and $audit.front_edge_raw_points_verified -eq 1910 -and $audit.raw_png_points_verified -eq 1665) 'fresh-raw-audit'
Assert ($pixels.processed_frames -eq 256 -and $pixels.invalid_frames -eq 0 -and $pixels.failed -eq 0 -and $pixels.allowed_targets -eq 0 -and $pixels.own_fake_downs -eq 0 -and $pixels.own_fake_moves -eq 0 -and $pixels.own_fake_ups -eq 0 -and $pixels.final_release_verified) 'fresh-chain'
$negatives=@();foreach($kind in @('epoch','geometry','rgb')){
 $path=Join-Path $pkg ('audit-release-controller-'+$kind+'.json');$negative=Get-Content $path -Raw|ConvertFrom-Json
 $negativeExpectedError=if($kind -eq 'rgb'){'audit front raw RGB mismatch'}else{'audit full declared offline context mismatch'}
 Assert (-not $negative.passed -and $negative.error -ceq $negativeExpectedError) ('negative-control: '+$kind)
 $negatives+=@{kind=$kind;expected_rejection=$true;error=$negative.error;report=(Entry $path)}
}
$freshRoot=Join-Path $repo 'out/hold-front-release-controller-01'
$binding=@{schema='pas.front-controller-source-and-binary-binding.v1';implementation_commit='e26231ec5acef9634df007770c1f06031d5326d4';source_tree=(& git rev-parse e26231e:research/hold_front_current);linked_library_source='774f3e7020818e94c508cc1481dd0986ebe417a0';v3_core_source='c2dda1db9fe95b46ff103300d43583857130cd8f';sources=@(Get-ChildItem research/hold_front_current -File|ForEach-Object {Entry $_.FullName});fresh_release_binaries=@(Get-ChildItem $freshRoot -File|Where-Object Extension -in '.exe','.lib','.dll'|ForEach-Object {Entry $_.FullName});existing_configurations_manifest=(Entry (Join-Path $docs 'front-candidate-evidence-01/package/binaries-and-controls-external.json'));copied_binaries=$false}
NewJson (Join-Path $PSScriptRoot 'source-and-binaries-external.json') $binding
$capacity=Get-Content (Join-Path $PSScriptRoot 'capacity-start.json') -Raw|ConvertFrom-Json
$stagePaths=@();foreach($dir in $dirs){$stagePaths+=$dir.FullName;foreach($suffix in @('-inputs','-pixel-controls')){$path=Join-Path $out ($dir.Name+$suffix);if(Test-Path $path){$stagePaths+=$path}}}
$newPackage=@(Get-ChildItem $pkg -File -Filter '*controller*')
$allocated=@(Get-ChildItem $PSScriptRoot -File -Recurse)+@(Get-ChildItem $freshRoot -File -Recurse)+$newPackage
foreach($path in $stagePaths){$allocated+=@(Get-ChildItem $path -File -Recurse)}
$outBytes=($allocated|Measure-Object Length -Sum).Sum
Assert ($outBytes -lt $capacity.out_cap_bytes -and (Get-PSDrive C).Free -ge $capacity.min_free_bytes) 'controller-capacity'
$summary=@{schema='pas.front-controller-independent-acceptance.v1';offline_geometry_acceptance='passed';product_completed=$false;formal_runtime_adoption='not_qualified';physical_legal_opportunity_gold=0;device_commands=0;new_tasks=0;new_automations=0;push_PR_merge=0;source=$binding.implementation_commit;worker_delivery='055ec799d53ccc842ee7a2fa3033369ccc389f8d';review_branch='codex/hold-front-acceptance-20261006';provenance_audit=(Entry (Join-Path $PSScriptRoot 'audit-after-02.json'));contracts=$contracts;stages=$stages;complete_stages=@($stages|Where-Object status -eq COMPLETE).Count;stop_stages=@($stages|Where-Object status -eq STOP).Count;worker_COMPLETE=30;worker_STOP=3;negative_controls=$negatives;compared_rows=256;raw_proposals=859;visible_terminal_proposals=191;unknown_Hold=145;other_unsupported=523;new_raw_points=1910;old_raw_points=1665;eligible_targets=0;own_fake_downs=0;own_fake_moves=0;own_fake_ups=0;full_trace_comparison=(Entry (Join-Path $PSScriptRoot 'trace-comparison.json'));boundary3030=$audit.frame3030_candidate2;timing_baseline_ms=$pixels.unchanged_baseline_chain_compute_ms;timing_sidecar_ms=$pixels.post_dispatch_front_sidecar_compute_ms;timing_scope=$pixels.timing_excludes;cost_gate='NOT_READY';producer_probes_per_candidate=975;producer_probe_batch_cap=124800;producer_plus_consumer_probe_batch_upper_bound=249600;shadow_v3_cost_additional=$true;legacy3938_rerun=$false;original36_fail_preserved=$true;root349_rerun=$false;original_timeout_and_skip_preserved=$true;maintenance_audit_failed_reports=@('audit-before.json','audit-before-02.json','audit-before-03.json','audit-before-04.json','audit-after.json');maintenance_failures_scope='Review bookkeeping assumptions corrected; no candidate, original oracle or qualified wrapper modification; all reports preserved';out_allocated_bytes_before_seal=$outBytes;out_cap_bytes=$capacity.out_cap_bytes;metadata_cap_bytes=$capacity.metadata_cap_bytes;free_bytes=(Get-PSDrive C).Free}
Assert ($summary.complete_stages -eq 13 -and $summary.stop_stages -eq 1) 'stage-summary-denominator'
NewJson (Join-Path $PSScriptRoot 'acceptance.json') $summary
Assert (-not(Test-Path $archive)) 'fresh-controller-archive-required'
[IO.Directory]::CreateDirectory($archive)|Out-Null
function CopyFiles($from,$to){
 [IO.Directory]::CreateDirectory($to)|Out-Null
 foreach($file in Get-ChildItem -LiteralPath $from -File -Recurse){
  $relative=[IO.Path]::GetRelativePath($from,$file.FullName);$destination=Join-Path $to $relative
  [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))|Out-Null
  [IO.File]::Copy($file.FullName,$destination,$false)
 }
}
CopyFiles $PSScriptRoot (Join-Path $archive 'controller')
[IO.Directory]::CreateDirectory((Join-Path $archive 'package'))|Out-Null
foreach($file in $newPackage){[IO.File]::Copy($file.FullName,(Join-Path $archive ('package/'+$file.Name)),$false)}
foreach($path in $stagePaths){CopyFiles $path (Join-Path $archive ('receipts/'+[IO.Path]::GetFileName($path)))}
[IO.Directory]::CreateDirectory((Join-Path $archive 'build-maps'))|Out-Null
foreach($name in @('CMakeCache.txt','build.ninja')){[IO.File]::Copy((Join-Path $freshRoot $name),(Join-Path $archive ('build-maps/'+$name)),$false)}
[IO.File]::WriteAllText((Join-Path $archive '.gitattributes'),"* -text`n",[Text.UTF8Encoding]::new($false))
$files=@();foreach($file in Get-ChildItem $archive -File -Recurse){$e=Entry $file.FullName;$files+=@{relative=[IO.Path]::GetRelativePath($archive,$file.FullName).Replace('\','/');bytes=$e.bytes;sha256=$e.sha256}}
NewJson (Join-Path $archive 'SHA256_INDEX.json') @{schema='pas.front-controller-seal.v1';excluded_self='SHA256_INDEX.json';worker_seal_preserved=(Entry (Join-Path $docs 'front-candidate-evidence-01/SHA256_INDEX.json'));files=$files}
$index=Get-Content (Join-Path $archive 'SHA256_INDEX.json') -Raw|ConvertFrom-Json
foreach($e in $index.files){Check @{path=(Join-Path $archive $e.relative);bytes=$e.bytes;sha256=$e.sha256}}
$bytes=(Get-ChildItem $archive -File -Recurse|Measure-Object Length -Sum).Sum
Assert ($bytes -lt $capacity.metadata_cap_bytes) 'metadata-capacity'
NewJson (Join-Path $PSScriptRoot 'seal.json') @{schema='pas.front-controller-seal-result.v1';archive=$archive;files=$files.Count+1;entries=$files.Count;bytes=$bytes;index=(Entry (Join-Path $archive 'SHA256_INDEX.json'));verified=$true;free_bytes=(Get-PSDrive C).Free}
@{acceptance='offline_geometry_passed';complete=$summary.complete_stages;STOP=$summary.stop_stages;seal_files=$files.Count+1;seal_bytes=$bytes;index_sha256=(Get-FileHash (Join-Path $archive 'SHA256_INDEX.json')).Hash.ToLowerInvariant()}|ConvertTo-Json
