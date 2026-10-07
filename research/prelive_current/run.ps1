param([ValidateSet('release','debug','asan')][string]$Mode='release',
 [ValidateSet('build','tests','family','pixels','pipeline','analysis','bindings')][string]$Operation='build',
 [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$BuildAttempt='01',
 [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$Attempt='01',
 [string]$Selection='', [ValidateSet('A','B')][string]$Variant='B',
 [ValidateSet('tap','hold','hold-closed-reset-v2','dense','slow','writer','rpc','fault')][string]$Scene='tap',
 [ValidateRange(1000,10000)][int]$Frames=1000,[int]$ExpectedNative=0,
 [ValidateSet('aa','run')][string]$Analysis='run',[string]$AnalysisInput='',
 [string]$ClipRoot='C:/Users/wurre/Desktop/Phigros-Auto-play-System/measurements/game-assist/manual-session-3906621440800/pixel-clips',
 [string]$CoreDonor='', [string]$Campaign='', [string]$CampaignRunId='',
 [Parameter(Mandatory)][string]$GateReceipt,
 [Parameter(Mandatory)][string]$ToolSnapshot,[string]$FamilyBefore='',
 [string]$SourceMigration='',
 [ValidatePattern('^[0-9a-f]{64}$')][string]$SourceMigrationSha256='',
 [ValidatePattern('^[0-9a-f]{40}$')][string]$CandidateCommit='')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out=Join-Path $repo 'out/windows-handoff'
$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh='C:\Users\wurre\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe'
$gate=[IO.Path]::GetFullPath($GateReceipt)
$tool=[IO.Path]::GetFullPath($ToolSnapshot)
$root=Join-Path $repo ('out/prelive-current-'+$Mode+'-'+$BuildAttempt)
function NewJson($path,$value,[int]$cap=67108864){$bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 40)+"`n");if($bytes.Length -ge $cap){throw 'metadata-byte-cap'};$s=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read);try{$s.Write($bytes);$s.Flush($true)}finally{$s.Dispose()}}
function CheckEntry($f){if((Get-Item -LiteralPath $f.path).Length -ne $f.bytes -or (Get-FileHash -LiteralPath $f.path).Hash.ToLowerInvariant() -cne $f.sha256){throw ('input-sha '+$f.path)}}
function Binding($path){$item=Get-Item -LiteralPath $path;@{path=$item.FullName;bytes=[long]$item.Length;sha256=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()}}
function Fresh($path){if(Test-Path -LiteralPath $path){throw ('fresh-output-required '+$path)}}
function MigrationPath($raw,$base){
 if(-not $raw -or $raw -match '(^|[\\/])\.\.?([\\/]|$)' -or $raw -match '^\\\\' -or $raw -match '(^|[\\/])[^\\/]*[ .]([\\/]|$)'){throw 'migration-raw-path'}
 $base=[IO.Path]::GetFullPath($base);$path=if([IO.Path]::IsPathRooted($raw)){[IO.Path]::GetFullPath($raw)}else{if($raw.Contains(':')){throw 'migration-relative-stream-path'};[IO.Path]::GetFullPath((Join-Path $base $raw))}
 if(-not $path.StartsWith($base+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $path.Substring(2).Contains(':')){throw 'migration-path-scope'}
 # Portable packet references never follow symlinks/junctions out of scope.
 $check=$path;while($check){if(Test-Path -LiteralPath $check){if((Get-Item -LiteralPath $check).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'migration-reparse'}};$parent=[IO.Path]::GetDirectoryName($check);if($parent -eq $check){break};$check=$parent}
 return $path
}
function MigrationBlob($blob,$base){if(-not $blob){throw 'migration-blob-required'};@{path=(MigrationPath ([string]$blob.path) $base);bytes=$blob.bytes;sha256=$blob.sha256}}
function ByteVariants($blob){
 CheckEntry $blob;if($blob.bytes -ge 1048576){throw 'canonical-source-blob-byte-cap'};$bytes=[IO.File]::ReadAllBytes($blob.path);$utf8=[Text.UTF8Encoding]::new($false,$true);$text=$utf8.GetString($bytes)
 if($text.Contains("`r") -or $text.StartsWith([string][char]0xfeff,[StringComparison]::Ordinal)){throw 'canonical-utf8-lf-blob-required'}
 $crlf=$utf8.GetBytes($text.Replace("`n","`r`n"))
 return @(@{encoding='LF';bytes=[long]$bytes.Length;sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()},@{encoding='CRLF';bytes=[long]$crlf.Length;sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($crlf)).ToLowerInvariant()})
}
function Migration(){
 if(-not $SourceMigration -or -not $SourceMigrationSha256 -or -not $CandidateCommit){throw 'sealed-source-migration-and-candidate-commit-required'}
 $path=[IO.Path]::GetFullPath($SourceMigration);$packet=[IO.Path]::GetDirectoryName($path);if((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -cne $SourceMigrationSha256){throw 'handoff-source-migration-seal'}
 $m=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json
 if($m.schema -cne 'pas.windows-offline-source-migration.v1' -or $m.target_commit -cne $CandidateCommit){throw 'source-migration-identity'}
 $before=Get-Content -LiteralPath (Join-Path $pkg 'formal-source-before.json') -Raw|ConvertFrom-Json;$suffix='\include\pas\game.hpp';$anchor=@($before|Where-Object {$_.path.EndsWith($suffix,[StringComparison]::OrdinalIgnoreCase)})
 if($anchor.Count -ne 1){throw 'migration-historical-source-root-anchor'};$historicalRoot=$anchor[0].path.Substring(0,$anchor[0].path.Length-$suffix.Length)
 # Explicit source deltas only. This never remaps tools, libraries, receipts,
 # generated protobuf or the unchanged frozen v3/front/bridge source.
 $allowed=@('CMakeLists.txt','include/pas/game.hpp','include/pas/game_session.hpp','include/pas/runtime.hpp','include/pas/candidate_admission.hpp','include/pas/candidate_packet.hpp','src/game.cpp','src/game_session.cpp','src/manual_session.cpp','src/runtime.cpp','research/prelive_current/current.cpp','research/prelive_current/tail_current.hpp','research/prelive_current/own_bridge.cpp','research/prelive_current/own_bridge.hpp','research/prelive_current/runtime.hpp','research/prelive_current/owner_candidate_step.hpp','research/prelive_current/input_closure.hpp','research/prelive_current/receipt_stream.hpp','research/prelive_current/disabled_live_seam_tests.cpp','research/prelive_current/cost_analysis.cpp','research/prelive_current/campaign_provenance.hpp','research/prelive_current/chain.cpp','research/prelive_current/producer_timing.hpp','research/prelive_current/offline_output.hpp','research/prelive_current/streaming_workload.hpp','research/prelive_current/tests/cost_analysis_review_tests.cpp','research/prelive_current/tests/producer_admission_review_tests.cpp','research/prelive_current/tests/producer_timing_tests.cpp','research/prelive_current/tests/offline_output_tests.cpp','research/prelive_current/tests/streaming_workload_tests.cpp','research/prelive_audit/audit.cpp')
 # Explicit local Windows LF fixture repair, sealed by a separate derivative
 # migration. Existing packet targets and original freeze bytes remain pinned.
 $allowed+=@('research/prelive_current/tests/cost_analysis_tests.cpp')
 $required=@('CMakeLists.txt','include/pas/game.hpp','src/game.cpp','include/pas/game_session.hpp','include/pas/runtime.hpp','src/game_session.cpp','src/manual_session.cpp','src/runtime.cpp','include/pas/candidate_admission.hpp','include/pas/candidate_packet.hpp','research/prelive_current/own_bridge.cpp','research/prelive_current/own_bridge.hpp','research/prelive_current/runtime.hpp','research/prelive_current/owner_candidate_step.hpp','research/prelive_current/input_closure.hpp','research/prelive_current/receipt_stream.hpp','research/prelive_current/disabled_live_seam_tests.cpp')
 $old=@{};$targets=@{};$inputs=@($path);$freezes=@{}
 foreach($f in $m.old_freezes){if($f.mode -notin @('release','debug','asan') -or $freezes.ContainsKey($f.mode)){throw 'migration-freeze-mode'};$expected=Join-Path $out ('build-bridge-withdraw-'+$f.mode+'-01/freeze.json');$resolved=MigrationBlob $f $repo;if($resolved.path -cne $expected){throw 'migration-exact-old-freeze-path'};CheckEntry $resolved;$freezes[$f.mode]=$resolved;$inputs+=@($resolved.path)}
 if($freezes.Count -ne 3){throw 'migration-three-old-freezes-required'}
 $allOld=@();foreach($f in $freezes.Values){$allOld+=@((Get-Content -LiteralPath $f.path -Raw|ConvertFrom-Json).files)}
 foreach($row in $m.paths){
  $rel=[string]$row.relative_path;if($rel -notin $allowed -or $targets.ContainsKey($rel)){throw 'migration-source-path-allowlist'}
  $row.canonical_target=MigrationBlob $row.canonical_target $packet;$variants=ByteVariants $row.canonical_target
  if(@($row.target_encodings).Count -ne 2){throw 'migration-two-target-encodings'}
  foreach($encoding in @('LF','CRLF')){$declared=@($row.target_encodings|Where-Object encoding -ceq $encoding);$calculated=@($variants|Where-Object encoding -ceq $encoding);if($declared.Count -ne 1 -or $declared[0].bytes -ne $calculated[0].bytes -or $declared[0].sha256 -cne $calculated[0].sha256){throw 'migration-precomputed-target-encoding-proof'}}
  $target=Binding (Join-Path $repo $rel);$match=@($row.target_encodings|Where-Object {$_.bytes -eq $target.bytes -and $_.sha256 -ceq $target.sha256});if($match.Count -ne 1){throw 'migration-exact-current-target-bytes'}
  $targets[$rel]=$target;$inputs+=@($target.path,$row.canonical_target.path)
  foreach($previous in $row.old_entries){
   if($old.ContainsKey($previous.path)){throw 'migration-duplicate-old-path'}
   $matches=@($allOld|Where-Object {$_.path -ceq $previous.path -and $_.bytes -eq $previous.bytes -and $_.sha256 -ceq $previous.sha256});if(-not $matches.Count -or $previous.path -cne (Join-Path $historicalRoot $rel)){throw 'migration-old-source-entry-join'}
   if($previous.preserved_original -and $previous.canonical_original){throw 'migration-original-proof-ambiguous'}
   if($previous.preserved_original){$previous.preserved_original=MigrationBlob $previous.preserved_original $packet}
   if($previous.canonical_original){$previous.canonical_original=MigrationBlob $previous.canonical_original $packet}
   if($previous.preserved_original){CheckEntry $previous.preserved_original;if($previous.preserved_original.bytes -ne $previous.bytes -or $previous.preserved_original.sha256 -cne $previous.sha256){throw 'migration-preserved-original-bytes'};$inputs+=@($previous.preserved_original.path)}
   elseif($previous.canonical_original){$beforeVariants=ByteVariants $previous.canonical_original;$proof=@($beforeVariants|Where-Object {$_.encoding -ceq $previous.old_encoding -and $_.bytes -eq $previous.bytes -and $_.sha256 -ceq $previous.sha256});if($proof.Count -ne 1){throw 'migration-original-encoding-proof'};$inputs+=@($previous.canonical_original.path)}
   else{throw 'migration-preserved-original-proof-required'}
   $old[$previous.path]=@{row=$row;previous=$previous;target=$target}
  }
 }
 foreach($rel in $required){if(-not $targets.ContainsKey($rel)){throw ('migration-required-target '+$rel)}}
 return @{manifest=(Binding $path);target_commit=$CandidateCommit;old=$old;targets=@($targets.Values);inputs=@($inputs|Sort-Object -Unique)}
}
$script:inputClosures=@{}
function InputClosure($stage,$paths){
 $canonical=@($paths|ForEach-Object {[IO.Path]::GetFullPath($_)}|Sort-Object -Unique)
 if(-not $canonical.Count -or $canonical.Count -gt 8192){throw 'input-closure-file-count'}
 $entries=@($canonical|ForEach-Object {Binding $_});$leaves=@()
 $rootPath=Join-Path $pkg ($stage+'-input-closure.json');$verificationPath=Join-Path $pkg ($stage+'-input-closure-verification.json')
 Fresh $rootPath;Fresh $verificationPath
 for($offset=0;$offset -lt $entries.Count;$offset+=64){
  $index=[int]($offset/64);$last=[Math]::Min($entries.Count-1,$offset+63);$leafPath=Join-Path $pkg ($stage+'-input-leaf-'+$index.ToString('000')+'.json')
  $leaf=@{schema='pas.current-input-closure-leaf.v1';stage=$stage;index=$index;files=@($entries[$offset..$last])}
  if([Text.Encoding]::UTF8.GetByteCount(($leaf|ConvertTo-Json -Depth 10)+"`n") -gt 32768){throw 'input-closure-leaf-byte-cap'}
  NewJson $leafPath $leaf 32768;$leaves+=@(Binding $leafPath)
 }
 if($leaves.Count -gt 128){throw 'input-closure-leaf-count'}
 NewJson $rootPath @{schema='pas.current-input-closure.v1';stage=$stage;verification_output=$verificationPath;files_count=$entries.Count;leaf_count=$leaves.Count;leaves=$leaves} 1048576
 $script:inputClosures[$stage]=@{root=(Binding $rootPath);leaves=$leaves;files=$entries;verification_output=$verificationPath;before_verified=$false;after_verified=$false}
 return @($rootPath)+@($leaves.path)
}
function CheckClosure($stage,$phase){
 $closure=$script:inputClosures[$stage];if(-not $closure){throw 'input-closure-required'}
 CheckEntry $closure.root;foreach($f in @($closure.leaves)+@($closure.files)){CheckEntry $f}
 if($phase -eq 'before'){$closure.before_verified=$true}else{$closure.after_verified=$true}
}
function ClosureReceipt($stage,[bool]$pass,[string]$reason=''){
 $closure=$script:inputClosures[$stage];$statePath=Join-Path (Join-Path $out $stage) 'state.json';$verificationPath=Join-Path (Join-Path $out $stage) ($stage+'-verification.json')
 $state=$null;$verification=$null;if(Test-Path -LiteralPath $statePath){$state=Binding $statePath};if(Test-Path -LiteralPath $verificationPath){$verification=Binding $verificationPath}
 if($pass -and (-not $closure.before_verified -or -not $closure.after_verified -or -not $state -or -not $verification)){throw 'input-closure-pass-predicate'}
 $status=if($pass){'COMPLETE'}else{'STOP'}
 NewJson $closure.verification_output @{schema='pas.current-input-closure-verification.v1';stage=$stage;status=$status;pass=$pass;before_verified=$closure.before_verified;after_verified=$closure.after_verified;root=$closure.root;state=$state;verification=$verification;files_count=$closure.files.Count;leaf_count=$closure.leaves.Count;reason=$reason} 1048576
}
function GateInputs(){
 # The unchanged invoke.ps1 repeats qualification before every owned launch.
 # Freeze every referenced qualification byte as well as the supplied receipt.
 $g=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
 if($g.schema -cne 'pas.windows-offline-process-gate.v1' -or -not $g.wrapper_positive -or $g.native_controls_verified -ne 4 -or $g.file_only_failed -ne 0){throw 'native-process-gate'}
 CheckEntry $g.diagnostic;$paths=@($gate,$g.diagnostic.path)
 foreach($r in $g.native_controls){CheckEntry $r.receipt;$v=Get-Content -LiteralPath $r.receipt.path -Raw|ConvertFrom-Json;$paths+=@($r.receipt.path);foreach($e in $v.entries){CheckEntry $e;$paths+=@($e.path)}}
 $snapshot=Get-Content -LiteralPath $tool -Raw|ConvertFrom-Json
 $paths+=@($tool);foreach($f in $snapshot.files){if((Get-FileHash -LiteralPath $f.copy).Hash.ToLowerInvariant() -cne $f.sha256){throw 'tool-snapshot-changed'};$paths+=@($f.copy)}
 return @($paths|Sort-Object -Unique)
}
function Capacity($stage,$phase){$roots=@(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -Directory|Where-Object Name -like 'prelive*')+@(Get-ChildItem -LiteralPath $out -Directory|Where-Object Name -like '*prelive*');$rows=@($roots|ForEach-Object {$sum=(Get-ChildItem -LiteralPath $_.FullName -File -Recurse|Measure-Object Length -Sum).Sum;@{path=$_.FullName;bytes=[long]$sum}});$total=[long](($rows|Measure-Object bytes -Sum).Sum);$free=[long](Get-PSDrive C).Free;$reserve=if($phase -eq 'before' -and $Operation -in @('pipeline','pixels','tests','family')){1073741824}elseif($phase -eq 'before' -and $Operation -eq 'build'){if($CoreDonor){536870912}else{1610612736}}else{0};$valid=$total+$reserve -le 12884901888 -and $free-$reserve -ge 21474836480;NewJson (Join-Path $pkg ($stage+'-capacity-'+$phase+'.json')) @{schema='pas.prelive-capacity.v1';stage=$stage;phase=$phase;roots=$rows;bytes=$total;reserve=$reserve;free=$free;cap=12884901888;minimum_free=21474836480;pass=$valid;metadata_seal_cap=67108864};if(-not $valid){throw 'capacity-gate'}}
function Receipt($stage,[int]$expected=0){$dir=Join-Path $out $stage;$vp=Join-Path $dir ($stage+'-verification.json');$v=Get-Content -LiteralPath $vp -Raw|ConvertFrom-Json;$r=Get-Content -LiteralPath (Join-Path $dir ($stage+'-result.json')) -Raw|ConvertFrom-Json;$s=Get-Content -LiteralPath (Join-Path $dir 'state.json') -Raw|ConvertFrom-Json;
 if($v.pending -or -not $v.producer_complete -or $v.native_exit -ne $expected -or $v.runner_exit -ne $expected -or $v.verification_exit -ne 0 -or $r.pending -or $r.facts.active_final -ne 0 -or -not $r.facts.held_all_signaled -or -not $r.facts.streams_completed -or -not $r.facts.identity_trusted -or -not $r.facts.natural_quiescence -or $s.status -cne 'COMPLETE' -or $s.running -or $s.freeze_sha -cne (Get-FileHash -LiteralPath (Join-Path $dir 'freeze.json')).Hash.ToLowerInvariant() -or $s.contract_sha -cne (Get-FileHash -LiteralPath (Join-Path $dir 'contract.json')).Hash.ToLowerInvariant()){throw ('receipt-not-verified '+$stage)};foreach($f in $v.entries){CheckEntry $f};$anchors=@($s.receipts|Where-Object path -eq $vp);if($anchors.Count -ne 1){throw 'state-receipt-link'};CheckEntry $anchors[0]}
function Stage($stage,$script,$arguments){
 $json=Join-Path $pkg ($stage+'-params.json');$afterRecorded=$false
 if($arguments.ContainsKey('Inputs')){$arguments.Inputs=@(InputClosure $stage $arguments.Inputs)+@($json)}
 if(-not $script:inputClosures.ContainsKey($stage)){throw 'input-closure-required'}
 try{
  Capacity $stage 'before'
  NewJson $json @{script=$script;arguments=$arguments}
  CheckClosure $stage 'before'
  & $pwsh -NoProfile -File (Join-Path $out 'withdrawal-review-01/stage.ps1') -Parameters $json
  if($LASTEXITCODE -ne 0){throw ('stage-wrapper '+$stage+' '+$LASTEXITCODE)}
  $expected=if($arguments.ContainsKey('ExpectedNative')){[int]$arguments.ExpectedNative}elseif($arguments.ContainsKey('Specification')){[int](Get-Content -LiteralPath $arguments.Specification -Raw|ConvertFrom-Json).native_exit}else{0}
  Receipt $stage $expected
  CheckClosure $stage 'after'
  Capacity $stage 'after';$afterRecorded=$true
  ClosureReceipt $stage $true
 }catch{
  $reason=$_.Exception.Message
  if(-not $afterRecorded){try{Capacity $stage 'after'}catch{}}
  ClosureReceipt $stage $false $reason
  throw
 }
}
function Native($stage,$executable,$arguments,$stageInputs,[int]$seconds=300,[int]$expected=0){
 # Candidate/control/auditor is the direct native Job root; never cmd.exe.
 if($executable -notin $seal.files.path){throw 'native-not-in-current-build-binding'}
 $spec=Join-Path $pkg ($stage+'-direct-spec.json')
 $closureInputs=InputClosure $stage @($stageInputs+$PSCommandPath+$pwsh+$gate+$tool+(Join-Path $out 'withdrawal-review-01/stage.ps1'))
 $direct=@($closureInputs)+@($PSCommandPath,$pwsh,$gate,$tool,(Join-Path $out 'withdrawal-review-01/stage.ps1'),(Join-Path $pkg ($stage+'-params.json')))
 if($Campaign){$direct+=@($Campaign)}
 NewJson $spec @{stage_root=(Join-Path $out $stage);stage=$stage;exe=$executable;argv=@($arguments);native_exit=$expected;runner_exit=$expected;total_s=$seconds;stream_cap=4194304;inputs=$direct}
 Stage $stage (Join-Path $repo 'tools/zero_miss_windows/invoke.ps1') @{Specification=$spec;GateReceipt=$gate}
 Receipt $stage $expected
}
if(-not(Test-Path (Join-Path $pkg 'initial-audit.json'))){throw 'initial-audit-required'}
if((Get-PSDrive C).Free -lt 21474836480){throw 'minimum-free'}
$buildStage='build-prelive-'+$Mode+'-'+$BuildAttempt
$qualificationInputs=GateInputs
$source=@(Get-ChildItem -LiteralPath $PSScriptRoot -File|Where-Object {$_.Extension -in '.cpp','.hpp','.ps1' -or $_.Name -eq 'CMakeLists.txt'}|ForEach-Object FullName)+@(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'tests') -File -Recurse|Where-Object Extension -eq '.cpp'|ForEach-Object FullName)+@(Get-ChildItem -LiteralPath (Join-Path $repo 'research/prelive_audit') -File|Where-Object {$_.Extension -in '.cpp','.hpp','.ps1' -or $_.Name -eq 'CMakeLists.txt'}|ForEach-Object FullName)+@((Join-Path $repo 'research/prelive_portable/runtime_finish_tests.cpp'),(Join-Path $repo 'research/prelive_portable/runtime_finish_pixels.cpp'),(Join-Path $repo 'research/prelive_portable/verify_tail_family_goal.cpp'),(Join-Path $repo 'research/live_readiness_review/tail_only_owner_review.cpp'),(Join-Path $repo 'research/live_readiness_review/tail_only_pixels_review.cpp'),(Join-Path $repo 'research/live_readiness_review/live_seam_packet_review.cpp'),(Join-Path $repo 'research/live_readiness_review/live_seam_runtime_review_v2.cpp'))
foreach($relative in @('src','include/pas','research/bvi_cold_v3','research/bvi_windows/bridge','research/hold_front_current')){$source+=@(Get-ChildItem -LiteralPath (Join-Path $repo $relative) -File|Where-Object {$_.Extension -in '.cpp','.hpp' -or $_.Name -eq 'CMakeLists.txt'}|ForEach-Object FullName)}
$buildSeal=Join-Path $pkg ('current-build-'+$Mode+'-'+$BuildAttempt+'.json')
if($Operation -eq 'build'){
 Fresh $buildSeal
 $migration=Migration
 $oldFreeze=Join-Path $out ('build-bridge-withdraw-'+$Mode+'-01/freeze.json')
 $oldFiles=(Get-Content -LiteralPath $oldFreeze -Raw|ConvertFrom-Json).files
 $changed=(Get-Content -LiteralPath (Join-Path $pkg 'formal-source-before.json') -Raw|ConvertFrom-Json)
 foreach($f in $oldFiles){
  if($f.path -in $changed.path){$prior=@($changed|Where-Object path -eq $f.path)[0];if($prior.sha256 -cne $f.sha256 -or $prior.bytes -ne $f.bytes){throw 'before-source-freeze'};if(-not $migration.old.ContainsKey($f.path)){throw 'original-before-source-requires-target-migration'}}
  if($migration.old.ContainsKey($f.path)){$previous=$migration.old[$f.path].previous;if($previous.bytes -ne $f.bytes -or $previous.sha256 -cne $f.sha256){throw 'migration-selected-freeze-entry'}}else{CheckEntry $f}
 }
 $inputs=@($oldFiles.path|Where-Object {$_ -notmatch '-inputs\\.*\.cmd$' -and -not $migration.old.ContainsKey($_)})+$source+@(Join-Path $pkg 'initial-audit.json')+@(Join-Path $pkg 'formal-source-before.json')+@(Join-Path $repo 'docs/research/zero-miss-20261006/windows/PRELIVE_CURRENT_COST_PROTOCOL.md')+@(Get-ChildItem -LiteralPath (Join-Path $repo 'tests') -File|ForEach-Object FullName)+@(Get-ChildItem -LiteralPath (Join-Path $repo 'include') -File -Recurse|ForEach-Object FullName)+@(Join-Path $repo 'src/runtime.cpp')+@(Join-Path $repo 'proto/emulator_controller.proto')+@(Get-ChildItem -LiteralPath (Join-Path $repo 'out/win-r4-ninja-release-01/generated') -File|ForEach-Object FullName)
 $inputs+=@($oldFreeze,$pwsh,(Join-Path $out 'withdrawal-review-01/stage.ps1'))+$qualificationInputs
 $inputs+=@($migration.inputs)
 $options=@('-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake','-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed','-DVCPKG_MANIFEST_INSTALL=OFF','-DVCPKG_TARGET_TRIPLET=x64-windows')
 if($CoreDonor){$CoreDonor=[IO.Path]::GetFullPath($CoreDonor);$donorName=Split-Path $CoreDonor -Leaf;if($donorName -notmatch ('^prelive-current-'+$Mode+'-([a-z0-9-]+)$')){throw 'donor-mode'};$donorStage='build-prelive-'+$Mode+'-'+$Matches[1];Receipt $donorStage;$donorFreeze=Join-Path $out ($donorStage+'/freeze.json');$donorFiles=(Get-Content -LiteralPath $donorFreeze -Raw|ConvertFrom-Json).files;foreach($f in $donorFiles){if($f.path.StartsWith((Join-Path $repo 'src'),[StringComparison]::OrdinalIgnoreCase) -or $f.path.StartsWith((Join-Path $repo 'include'),[StringComparison]::OrdinalIgnoreCase) -or $f.path -eq (Join-Path $repo 'CMakeLists.txt')){CheckEntry $f}};$deps=Join-Path $out ('bindings-deps-prelive-'+$Mode+'-final/'+ 'bindings-deps-prelive-'+$Mode+'-final.stdout.log');$previousRoot=if($Mode -eq 'release'){'prelive-current-release-10'}elseif($Mode -eq 'debug'){'prelive-current-debug-03'}else{'prelive-current-asan-02'};$extraDeps=@();foreach($line in Get-Content -LiteralPath $deps){if($line -match '^    (.+)$'){$relative=$Matches[1];$path=[IO.Path]::GetFullPath((Join-Path (Join-Path $repo ('out/'+$previousRoot)) $relative));if(Test-Path -LiteralPath $path -PathType Leaf){$extraDeps+=@($path)}}};$donorBinding=Join-Path $pkg ('core-donor-'+$Mode+'-'+$BuildAttempt+'.json');$extraFiles=@($extraDeps|Sort-Object -Unique|ForEach-Object {@{path=$_;bytes=(Get-Item -LiteralPath $_).Length;sha256=(Get-FileHash -LiteralPath $_).Hash.ToLowerInvariant()}});NewJson $donorBinding @{donor=$CoreDonor;files=$extraFiles;formal_sources=$donorFiles|Where-Object {$_.path.StartsWith((Join-Path $repo 'src'),[StringComparison]::OrdinalIgnoreCase) -or $_.path.StartsWith((Join-Path $repo 'include'),[StringComparison]::OrdinalIgnoreCase)}};$inputs+=@($donorBinding,$donorFreeze,(Join-Path $CoreDonor 'formal_core.lib'),(Join-Path $CoreDonor 'CMakeCache.txt'),(Join-Path $CoreDonor 'build.ninja'),$deps);$options+=@('-DPAS_PRELIVE_CORE_DONOR='+$CoreDonor)}
 if($Mode -eq 'asan'){$options+=@('-DPAS_PRELIVE_ASAN=ON','-DPAS_ASAN_SUPPORT_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')}
 $configuration=if($Mode -eq 'release'){'Release'}else{'Debug'}
 foreach($op in @('Configure','Build')){$stage=$op.ToLowerInvariant()+'-prelive-'+$Mode+'-'+$BuildAttempt;$args=@{SourceRoot=$PSScriptRoot;BuildRoot=$root;StageName=$stage;ToolSnapshot=$tool;GateReceipt=$gate;Operation=$op;Configuration=$configuration;Inputs=($inputs|Sort-Object -Unique);TotalSeconds=900};if($op -eq 'Configure'){$args.Options=$options};Stage $stage (Join-Path $PSScriptRoot 'cmake-stage.ps1') $args}
 if($CoreDonor){foreach($f in $extraFiles){CheckEntry $f}}
 # Seal actual output bytes only after the qualified build is COMPLETE. This
 # local binding is not itself a cost qualification or a replacement receipt.
 $receiptPaths=@();foreach($prefix in @('configure','build')){$name=$prefix+'-prelive-'+$Mode+'-'+$BuildAttempt;$dir=Join-Path $out $name;foreach($leaf in @('state.json','freeze.json','contract.json',($name+'-verification.json'))){$receiptPaths+=@(Join-Path $dir $leaf)};$v=Get-Content -LiteralPath (Join-Path $dir ($name+'-verification.json')) -Raw|ConvertFrom-Json;$receiptPaths+=@($v.entries.path)}
 $closureReceipts=@();foreach($prefix in @('configure','build')){$name=$prefix+'-prelive-'+$Mode+'-'+$BuildAttempt;$closureReceipts+=@(Binding $script:inputClosures[$name].verification_output)}
 NewJson $buildSeal @{schema='pas.current-windows-build-binding.v1';mode=$Mode;configuration=$configuration;build_root=$root;build_stage=$buildStage;gate=(Binding $gate);tool_snapshot=(Binding $tool);source_migration=$migration.manifest;target_commit=$migration.target_commit;migration_targets=$migration.targets;source_files=@($inputs|Sort-Object -Unique|ForEach-Object {Binding $_});input_closure_receipts=$closureReceipts;files=@(Get-ChildItem -LiteralPath $root -File -Recurse|Sort-Object FullName|ForEach-Object {Binding $_.FullName});build_receipts=@($receiptPaths|Sort-Object -Unique|ForEach-Object {Binding $_});windows_cost_qualified=$false;live_ready=$false}
 exit 0
}
Receipt $buildStage
$freeze=Join-Path $out ($buildStage+'/freeze.json');$files=(Get-Content -LiteralPath $freeze -Raw|ConvertFrom-Json).files;foreach($f in $files){CheckEntry $f}
$seal=Get-Content -LiteralPath $buildSeal -Raw|ConvertFrom-Json
if($seal.schema -cne 'pas.current-windows-build-binding.v1' -or $seal.mode -cne $Mode -or $seal.build_root -cne $root -or $seal.build_stage -cne $buildStage -or $seal.gate.path -cne $gate -or $seal.tool_snapshot.path -cne $tool){throw 'exact-current-build-binding'}
foreach($f in @($seal.files)+@($seal.build_receipts)+@($seal.source_files)+@($seal.migration_targets)+@($seal.input_closure_receipts)+@($seal.gate,$seal.tool_snapshot,$seal.source_migration)){CheckEntry $f}
foreach($receipt in $seal.input_closure_receipts){$cr=Get-Content -LiteralPath $receipt.path -Raw|ConvertFrom-Json;if($cr.schema -cne 'pas.current-input-closure-verification.v1' -or -not $cr.pass -or -not $cr.before_verified -or -not $cr.after_verified -or $cr.status -cne 'COMPLETE'){throw 'build-input-closure-not-verified'}}
$inputs=@($source+$freeze+$files.path+$seal.source_files.path+$seal.migration_targets.path+$seal.input_closure_receipts.path+$seal.files.path+$seal.build_receipts.path)+@($buildSeal,$pwsh,(Join-Path $out 'withdrawal-review-01/stage.ps1'))+$qualificationInputs
$inputs+=@(Join-Path $root 'build.ninja')+@(Join-Path $root 'CMakeCache.txt')+@(Join-Path $root 'runtime_contract.cpp')+@(Join-Path $repo 'docs/research/zero-miss-20261006/windows/PRELIVE_CURRENT_COST_PROTOCOL.md')
$inputs+=@(Join-Path $repo 'configs/phigros-hd-assist-five-lead35.json')
if($Operation -eq 'bindings'){
 foreach($kind in @('deps','commands')){$stage='bindings-'+$kind+'-prelive-'+$Mode+'-'+$Attempt;Stage $stage (Join-Path $repo 'tools/zero_miss_windows/native-stage.ps1') @{StageName=$stage;Executable='C:/Users/wurre/AppData/Local/Programs/Python/Python310/Scripts/ninja.exe';Arguments=@('-C',$root,'-t',$kind);GateReceipt=$gate;Inputs=$inputs;TotalSeconds=300;ExpectedNative=0}}
 exit 0
}
if($Operation -eq 'analysis'){
 if(-not $AnalysisInput){throw 'analysis-input-required'};$j=Get-Content -LiteralPath $AnalysisInput -Raw|ConvertFrom-Json
 $inputs+=@($AnalysisInput)
 if($j.schema -cne 'pas.current-rails-cost-input.v2'){throw 'v2 bound analysis input required; historical campaign cannot resume'}
 $campaignObject=Get-Content -LiteralPath $j.campaign.path -Raw|ConvertFrom-Json
 $inputs+=@($j.campaign.path)+@($campaignObject.bindings.path)
 $sourceManifest=@($campaignObject.bindings|Where-Object role -eq 'source_manifest')[0]
 $inputs+=@((Get-Content -LiteralPath $sourceManifest.path -Raw|ConvertFrom-Json).files.path)
 $runs=if($j.runs){@($j.runs)}else{@($j.run)}
 foreach($run in $runs){$inputs+=@($run.path,$run.audit_path,($run.path+'.attempts.json'),($run.path+'.journal.jsonl'))
  foreach($property in $run.process_evidence.PSObject.Properties){$inputs+=@($property.Value.path)}
 }
 $stage='analysis-prelive-'+$Mode+'-'+$Attempt;$report=Join-Path $pkg ('analysis-'+$Mode+'-'+$Attempt+'.json');Fresh $report
 Native $stage (Join-Path $root 'cost_analysis.exe') @($Analysis,$AnalysisInput,$report) $inputs
 exit 0
}
if($Operation -eq 'tests'){
 $clipIndex=Join-Path $ClipRoot 'index.jsonl';$clipFiles=@(Get-Content -LiteralPath $clipIndex|Where-Object {$_}|ForEach-Object {$_|ConvertFrom-Json});foreach($c in $clipFiles){$p=Join-Path $ClipRoot $c.path;if((Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant() -cne $c.sha256){throw 'clip-input-sha'}}
 $inputs+=@($clipIndex)+@($clipFiles|ForEach-Object {Join-Path $ClipRoot $_.path});$env:PAS_RGB_CLIP_ROOT=$ClipRoot
 foreach($kind in @('prefix_preservation','front_preservation','own_prefix','current_tests','formal_affected','active_body_tests','tail_guard_tests','runtime_finish_tests','runtime_finish_pixels','cost_workload_tests','tail_only_owner_review','tail_only_pixels_review','disabled_live_seam_tests','live_seam_packet_review','live_seam_runtime_review')){$stage=$kind.Replace('_','-')+'-prelive-'+$Mode+'-'+$Attempt;$report=Join-Path $pkg ($kind+'-'+$Mode+'-'+$Attempt+'.json');Fresh $report;$nativeArgs=if($kind -eq 'formal_affected'){@('--gtest_output=json:'+$report)}else{@($report)};Native $stage (Join-Path $root ($kind+'.exe')) $nativeArgs $inputs}
 foreach($kind in @('streaming_workload_tests','producer_timing_tests','offline_output_tests')){
  $stage=$kind.Replace('_','-')+'-prelive-'+$Mode+'-'+$Attempt
  $nativeArgs=if($kind -eq 'offline_output_tests'){@((Join-Path $pkg ('offline-output-'+$Mode+'-'+$Attempt)))}else{@()}
  if($kind -eq 'offline_output_tests'){Fresh $nativeArgs[0]}
  Native $stage (Join-Path $root ($kind+'.exe')) $nativeArgs $inputs
 }
 $fixture=Join-Path $pkg ('cost-unit-fixtures-'+$Mode+'-'+$Attempt);Fresh $fixture
 Native ('cost-unit-prelive-'+$Mode+'-'+$Attempt) (Join-Path $root 'cost_analysis_tests.exe') @($fixture,$repo) $inputs
 # Review deliberately mutates only its fresh generated test doubles and
 # restores their exact bytes. Freeze all fixtures, then recheck after exit.
 $fixtureFiles=@(Get-ChildItem -LiteralPath $fixture -File -Recurse|Sort-Object FullName|ForEach-Object {Binding $_.FullName})
 $review=Join-Path $pkg ('cost-review-'+$Mode+'-'+$Attempt);Fresh $review
 Native ('cost-review-prelive-'+$Mode+'-'+$Attempt) (Join-Path $root 'cost_analysis_review_tests.exe') @($fixture,$review) @($inputs+$fixtureFiles.path)
 foreach($f in $fixtureFiles){CheckEntry $f}
 $closureReview=Join-Path $pkg ('input-closure-review-'+$Mode+'-'+$Attempt);Fresh $closureReview
 Native ('input-closure-review-prelive-'+$Mode+'-'+$Attempt) (Join-Path $root 'input_closure_review_tests.exe') @($fixture,$closureReview) @($inputs+$fixtureFiles.path)
 foreach($f in $fixtureFiles){CheckEntry $f}
 $producer=Join-Path $pkg ('producer-review-'+$Mode+'-'+$Attempt);Fresh $producer
 Native ('producer-review-prelive-'+$Mode+'-'+$Attempt) (Join-Path $root 'producer_admission_review_tests.exe') @($producer) $inputs
 foreach($c in $clipFiles){$p=Join-Path $ClipRoot $c.path;if((Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant() -cne $c.sha256){throw 'clip-input-sha-after'}}
 exit 0
}
if($Operation -eq 'family'){
 if(-not $FamilyBefore){throw 'explicit-frozen-v1-before-report-required'}
 $FamilyBefore=[IO.Path]::GetFullPath($FamilyBefore);$before=Binding $FamilyBefore
 $family1=Join-Path $pkg ('family-v1-'+$Mode+'-'+$Attempt+'.json');$verify1=Join-Path $pkg ('family-v1-verification-'+$Mode+'-'+$Attempt+'.json')
 $family2=Join-Path $pkg ('family-v2-'+$Mode+'-'+$Attempt+'.json');$verify2=Join-Path $pkg ('family-v2-verification-'+$Mode+'-'+$Attempt+'.json');$tailGoal=Join-Path $pkg ('family-v2-normal-tail-'+$Mode+'-'+$Attempt+'.json')
 foreach($path in @($family1,$verify1,$family2,$verify2,$tailGoal)){Fresh $path}
 Native ('family-v1-prelive-'+$Mode+'-'+$Attempt) (Join-Path $root 'cost_workload_family.exe') @($family1) $inputs
 Native ('verify-family-v1-prelive-'+$Mode+'-'+$Attempt) (Join-Path $root 'cost_workload_family_verify.exe') @($FamilyBefore,$family1,$verify1) @($inputs+$FamilyBefore+$family1)
 CheckEntry $before
 Native ('family-v2-prelive-'+$Mode+'-'+$Attempt) (Join-Path $root 'cost_workload_family_v2.exe') @($family2) $inputs
 Native ('verify-family-v2-prelive-'+$Mode+'-'+$Attempt) (Join-Path $root 'cost_workload_family_v2_verify.exe') @($family2,$verify2) @($inputs+$family2)
 Native ('normal-tail-family-prelive-'+$Mode+'-'+$Attempt) (Join-Path $root 'verify_tail_family_goal.exe') @($family2,$tailGoal) @($inputs+$family2+$verify2)
 exit 0
}
if($Operation -eq 'pixels'){
 if(-not $Selection){$Selection=Join-Path $out 'real-pixels-selection-02/selection.json'}
 $selected=Get-Content -LiteralPath $Selection -Raw|ConvertFrom-Json
 foreach($f in $selected.frames){CheckEntry $f}
 $inputs+=@($Selection,$selected.index_path,(Join-Path $selected.record_root 'manifest.json'))+@($selected.frames.path)
 $stage='pixels-prelive-'+$Mode+'-'+$Attempt
 $arguments=@('pixels',$Selection,(Join-Path $pkg ('pixels-'+$Mode+'-'+$Attempt+'.json')))
}else{
 $stage='pipeline-prelive-'+$Mode+'-'+$Attempt
 $report=Join-Path $pkg ('pipeline-'+$Mode+'-'+$Attempt+'.json')
 if($Campaign -or $CampaignRunId){
  if(-not $Campaign -or -not $CampaignRunId -or $Mode -ne 'release' -or $Frames -ne 2560 -or $Scene -notin @('tap','hold','hold-closed-reset-v2','dense')){throw 'bound campaign contract'}
  $Campaign=[IO.Path]::GetFullPath($Campaign);$frozen=Get-Content -LiteralPath $Campaign -Raw|ConvertFrom-Json
  $inputs+=@($Campaign)+@($frozen.bindings.path)
  $sourceManifest=@($frozen.bindings|Where-Object role -eq 'source_manifest')[0]
  $inputs+=@((Get-Content -LiteralPath $sourceManifest.path -Raw|ConvertFrom-Json).files.path)
  $arguments=@('pipeline',$Variant,$Scene,[string]$Frames,$Campaign,$CampaignRunId,$report)
 }else{$arguments=@('pipeline',$Variant,$Scene,[string]$Frames,$report)}
}
$nativeExpected=if($Operation -eq 'pipeline' -and $Campaign){0}else{$ExpectedNative}
Native $stage (Join-Path $root 'current_chain.exe') $arguments $inputs 900 $nativeExpected
if($Operation -eq 'pixels'){foreach($f in $selected.frames){CheckEntry $f}}
