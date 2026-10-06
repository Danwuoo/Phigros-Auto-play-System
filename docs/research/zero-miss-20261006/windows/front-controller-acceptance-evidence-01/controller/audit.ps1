param([string]$Report='audit-before.json')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Set-Location -LiteralPath $repo
$original='C:\Users\wurre\Desktop\Phigros-Auto-play-System'
$docs=Join-Path $repo 'docs/research/zero-miss-20261006/windows'
function Assert($ok,$why){if(-not $ok){throw $why}}
function Check($path,$bytes,$sha){
 $file=Get-Item -LiteralPath $path
 Assert ($file.Length -eq $bytes) ('length: '+$path)
 Assert ((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -ceq $sha) ('sha: '+$path)
}
function Blob($path){
 $b=[IO.File]::ReadAllBytes($path);$h=[Text.Encoding]::ASCII.GetBytes(('blob '+$b.Length+[char]0))
 $all=[byte[]]::new($h.Length+$b.Length);$h.CopyTo($all,0);$b.CopyTo($all,$h.Length)
 [Convert]::ToHexString([Security.Cryptography.SHA1]::HashData($all)).ToLowerInvariant()
}
function NewJson($path,$value){
 $s=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
 try{$bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 25)+"`n");$s.Write($bytes);$s.Flush($true)}finally{$s.Dispose()}
}
$result=@{schema='pas.front-controller-provenance-audit.v1';passed=$false;delivery='055ec799d53ccc842ee7a2fa3033369ccc389f8d';source='e26231ec5acef9634df007770c1f06031d5326d4';device_commands=0;paid_compute=0;new_tasks=0}
try {
 $git=@{};foreach($line in (& git ls-files -s -- docs/research/zero-miss-20261006/windows research/hold_front_current)){
  Assert ($line -match '^\d+ ([0-9a-f]{40}) \d+\t(.+)$') 'git-index-parse';$git[$Matches[2]]=$Matches[1]
 }
 $archives=@();foreach($name in @('evidence','withdrawal-acceptance-evidence-01','front-candidate-evidence-01')){
  $root=[IO.Path]::GetFullPath((Join-Path $docs $name));$index=Join-Path $root $(if($name -eq 'evidence'){'SHA256_INDEX_FINAL.json'}else{'SHA256_INDEX.json'})
  $seal=Get-Content -LiteralPath $index -Raw|ConvertFrom-Json;$count=0;$total=0
  foreach($entry in $seal.files){
   $path=[IO.Path]::GetFullPath((Join-Path $root $entry.relative));Assert ($path.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'archive-path'
   Check $path $entry.bytes $entry.sha256
   $relative=[IO.Path]::GetRelativePath($repo,$path).Replace('\','/');Assert ($git.ContainsKey($relative)) ('archive-untracked: '+$relative)
   Assert ((Blob $path) -ceq $git[$relative]) ('git-blob: '+$relative);$count++;$total+=$entry.bytes
  }
  Assert (@(Get-ChildItem -LiteralPath $root -File -Recurse).Count -eq $count+1) 'archive-denominator'
  $relative=[IO.Path]::GetRelativePath($repo,$index).Replace('\','/');Assert ((Blob $index) -ceq $git[$relative]) 'index-git-blob'
  $archives+=@{name=$name;entries=$count;files=$count+1;bytes=$total+(Get-Item $index).Length;index_sha256=(Get-FileHash $index).Hash.ToLowerInvariant()}
 }
 Assert ($archives[2].entries -eq 749 -and $archives[2].index_sha256 -ceq '03bdc52737bc2e0afb6a0ab4c41ede24e3ab3ea63db480d253ce949e095d4fb3') 'new-seal-identity'
 $result.archives=$archives
 $pkg=Join-Path $docs 'front-candidate-evidence-01/package'
 $external=Get-Content (Join-Path $pkg 'binaries-and-controls-external.json') -Raw|ConvertFrom-Json
 foreach($entry in @($external.binaries)+@($external.linked_libraries.file)+@($external.negative_control_traces)){Check $entry.path $entry.bytes $entry.sha256}
 $result.external=@{binaries=$external.binaries.Count;libraries=$external.linked_libraries.Count;negative_traces=$external.negative_control_traces.Count}
 $delivery=Get-Content (Join-Path $pkg 'final-verification.json') -Raw|ConvertFrom-Json
 $result.core_entries=0;foreach($entry in $delivery.v3_core){Check $entry.path $entry.bytes $entry.sha256;$result.core_entries++}
 $sourceFiles=@(Get-ChildItem research/hold_front_current -File);foreach($file in $sourceFiles){
  $snapshot=Join-Path $docs ('front-candidate-evidence-01/source-e26231e/'+$file.Name)
  Check $snapshot $file.Length ((Get-FileHash $file.FullName).Hash.ToLowerInvariant())
  $rel='research/hold_front_current/'+$file.Name;Assert ((Blob $file.FullName) -ceq $git[$rel]) 'source-working-tree-git'
 }
 $result.source_files=$sourceFiles.Count
 $oldGit=@{};foreach($line in (& git ls-tree -r 28493f12e45e2110e74b4d5b0ab99a03f6755cf6 -- research/hold_front_current)){
  Assert ($line -match '^\d+ blob ([0-9a-f]{40})\t(.+)$') 'old-git-tree-parse';$oldGit[$Matches[2]]=$Matches[1]
 }
 $oldSourceCount=0;foreach($file in Get-ChildItem (Join-Path $pkg 'source-v1') -File){
  $rel='research/hold_front_current/'+$file.Name
  if($oldGit.ContainsKey($rel)){Assert ((Blob $file.FullName) -ceq $oldGit[$rel]) ('old-source-git: '+$rel);$oldSourceCount++}
 }
 Assert ($oldSourceCount -eq $oldGit.Count -and $oldSourceCount -eq 7) 'old-source-denominator';$result.initial_source_git_blobs=$oldSourceCount
 & git diff --quiet 4fe388428c7ccca4ef15d18a1d8e44cddd5198c3 HEAD -- src include tests CMakeLists.txt research/bvi_cold_v3 research/bvi_windows tools/zero_miss_windows
 Assert ($LASTEXITCODE -eq 0) 'protected-code-changed';$result.protected_diff=$false
 $stages=@();foreach($expected in $delivery.all_stages){
  $dir=Join-Path $repo ('out/windows-handoff/'+$expected.stage)
  $v=Get-Content (Join-Path $dir ($expected.stage+'-verification.json')) -Raw|ConvertFrom-Json
  $r=Get-Content (Join-Path $dir ($expected.stage+'-result.json')) -Raw|ConvertFrom-Json
  Assert (-not $v.pending -and -not $r.pending -and $r.facts.exit_code -eq $expected.native -and $r.facts.runner_exit -eq $expected.runner -and $v.verification_exit -eq $expected.verification) ('stage-exits: '+$expected.stage)
  $state=Get-Content (Join-Path $dir 'state.json') -Raw|ConvertFrom-Json
  Assert ($state.status -ceq $expected.status) ('stage-state: '+$expected.stage)
  if($state.status -ceq 'COMPLETE'){Assert ($v.native_exit -eq $expected.native -and $v.runner_exit -eq $expected.runner) 'complete-verification-exits'}
  Assert ($r.facts.active_final -eq 0 -and $r.facts.held_all_signaled -and $r.facts.streams_completed) ('stage-not-closed: '+$expected.stage)
  Assert ($r.facts.created -and $r.facts.assigned -and $r.facts.resumed) 'job-launch-contract'
  foreach($entry in $v.entries){Check $entry.path $entry.bytes $entry.sha256}
  Assert ($r.facts.identity_trusted -eq $expected.identity_trusted) 'stage-identity'
  $stages+=@{stage=$expected.stage;native=$r.facts.exit_code;runner=$r.facts.runner_exit;verification=$v.verification_exit;active_final=$r.facts.active_final;held_signaled=$r.facts.held_all_signaled;streams_completed=$r.facts.streams_completed;identity_trusted=$r.facts.identity_trusted;status=$state.status}
 }
 Assert ($stages.Count -eq 33 -and @($stages|Where-Object status -eq STOP).Count -eq 3) 'worker-stage-denominator'
 $result.worker_stages=$stages
 $freezeCount=0;foreach($pair in @(@('release','03'),@('debug','02'),@('asan','02'))){
  $freeze=Get-Content (Join-Path $repo ('out/windows-handoff/build-hold-front-'+$pair[0]+'-'+$pair[1]+'/freeze.json')) -Raw|ConvertFrom-Json
  foreach($entry in $freeze.files){Check $entry.path $entry.bytes $entry.sha256;$freezeCount++}
 }
 $result.final_build_inputs_verified=$freezeCount
 $selection=Get-Content out/windows-handoff/real-pixels-selection-02/selection.json -Raw|ConvertFrom-Json
 Assert ($selection.selected_count -eq 256 -and $selection.human_gold -eq 0 -and $selection.record_denominator -eq 7722) 'selection-denominator'
 foreach($entry in $selection.frames){Check $entry.path $entry.bytes $entry.sha256}
 Assert ((Get-FileHash $selection.index_path).Hash.ToLowerInvariant() -ceq $selection.index_sha256) 'original-index'
 Assert ((Get-FileHash (Join-Path $selection.record_root 'manifest.json')).Hash.ToLowerInvariant() -ceq $selection.record_manifest_sha256) 'original-manifest'
 $result.original_pngs=256;$result.physical_legal_opportunity_gold=0
 $result.original_HEAD=(& git -C $original rev-parse HEAD);Assert ($result.original_HEAD -ceq '74e54437d4a3ad2b2bd1a3b09312211a92f2359e') 'original-head'
 $result.original_dirty=@(& git -C $original status --short)
 Assert ($result.original_dirty.Count -eq 2 -and $result.original_dirty[0] -ceq ' M apps/runtime_x11_p/aux/CMakeLists.txt' -and $result.original_dirty[1] -ceq '?? phigros-zero-miss-handoff-round4-9dc99d6.zip') 'original-dirty'
 $result.aux_sha256=(Get-FileHash -LiteralPath ('\\?\'+$original+'\apps\runtime_x11_p\aux\CMakeLists.txt')).Hash.ToLowerInvariant()
 Assert ($result.aux_sha256 -ceq 'e5c78a1d90a5e430c4f7c5328c7e9a8b3571800b0b8fc698d082b1cbe16d5501') 'user-aux'
 $result.zip_sha256=(Get-FileHash -LiteralPath (Join-Path $original 'phigros-zero-miss-handoff-round4-9dc99d6.zip')).Hash.ToLowerInvariant()
 Assert ($result.zip_sha256 -ceq '18040114639c560c0ed33d6d7949affd1a4fb1ce2be807adf1f53b948656ee9b') 'user-zip'
 $result.free_bytes=(Get-PSDrive C).Free;Assert ($result.free_bytes -ge 21474836480) 'minimum-free'
 $result.passed=$true
}catch{$result.error=$_.Exception.Message;$result.error_line=$_.InvocationInfo.ScriptLineNumber}
NewJson (Join-Path $PSScriptRoot $Report) $result
$result|Select-Object passed,error,source_files,core_entries,final_build_inputs_verified,original_pngs,external,free_bytes|ConvertTo-Json -Depth 4
if(-not $result.passed){exit 1}
