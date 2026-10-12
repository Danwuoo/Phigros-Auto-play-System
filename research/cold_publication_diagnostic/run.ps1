param(
 [Parameter(Mandatory)][string]$Workspace,
 [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$FrozenHead,
 [Parameter(Mandatory)][string]$BuildBinding,
 [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedBuildBindingSha256,
 [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedRunnerSha256,
 [ValidatePattern('^main[0-9]{2}(?:-[a-z0-9]{1,10}){0,2}$')][string]$Attempt='main81-d1'
)
$ErrorActionPreference='Stop'
$workspaceRoot=[IO.Path]::GetFullPath($Workspace)
$self=$PSCommandPath
. (Join-Path $workspaceRoot 'research/pre_emulator/common.ps1')
# Retain the existing capacity implementation including the short source root.
. (Join-Path $workspaceRoot 'research/cold_scheduler_diagnostic/capacity.ps1')
$script:maintenancePath=$self;$Operation='pipeline'
function DRequire([bool]$ok,[string]$why){if(-not $ok){throw ('publication diagnostic '+$why)}}
function DRead($pin,[long]$cap=4194304){DRequire ($null-ne$pin -and $pin.path -and $pin.bytes-ge0 -and $pin.bytes-le$cap) 'bounded metadata pin';CheckEntry $pin;Get-Content -Raw -LiteralPath $pin.path|ConvertFrom-Json}
function DBool($v,[string]$key,[bool]$expected){DRequire ($v.$key-is[bool] -and $v.$key-eq$expected) ('typed '+$key)}
function DInt($v,[string]$key,[long]$expected){DRequire (($v.$key-is[int] -or $v.$key-is[long]) -and $v.$key-eq$expected) ('integer '+$key)}
function SamePin($a,$b,[string]$why){DRequire ($null-ne$a -and $null-ne$b -and [string]::Equals([IO.Path]::GetFullPath($a.path),[IO.Path]::GetFullPath($b.path),[StringComparison]::OrdinalIgnoreCase) -and $a.bytes-eq$b.bytes -and $a.sha256-ceq$b.sha256) $why}
function PinPaths($pins){@($pins|ForEach-Object {DRequire ($null-ne$_ -and $_.path) 'nonempty input pin';[string]$_.path})}
function EvidencePins($proof){@('specification','input_closure','result','verification','freeze','state','contract','command')|ForEach-Object {$proof.$_}}
function VerifyNative($proof){
 $keys=@('specification','input_closure','result','verification','freeze','state','contract','command')
 $actualKeys=if($proof-is[Collections.IDictionary]){@($proof.Keys)}else{@($proof.PSObject.Properties.Name)}
 DRequire (@(Compare-Object ($keys|Sort-Object) ($actualKeys|Sort-Object)).Count-eq0) 'Native8 exact roles'
 foreach($key in $keys){CheckEntry $proof.$key}
 $v=DRead $proof.verification;Receipt $v.stage
 $c=DRead $proof.input_closure
 DRequire ($c.schema-ceq'pas.current-input-closure-verification.v1' -and $c.stage-ceq$v.stage -and $c.status-ceq'COMPLETE') 'Native before/after closure'
 foreach($key in @('pass','before_verified','after_verified')){DBool $c $key $true}
 SamePin $c.verification $proof.verification 'closure native verification join'
 SamePin $c.state $proof.state 'closure state join'
}
function AssertSource {
 DRequire ((& git -C $repo rev-parse HEAD)-ceq$FrozenHead -and (& git -C $repo branch --show-current)-ceq'main' -and -not(& git -C $repo status --porcelain)) 'same clean frozen main'
 foreach($pin in $sourcePins){CheckEntry $pin}
}
function AssertFixed {AssertSource;foreach($pin in $fixedPins){CheckEntry $pin}}
function RawCapacity([string]$phase){
 $all=@(Get-ChildItem -LiteralPath $artifact -File -Recurse)
 $raw=@($all|Where-Object Name -match '(\.journal\.(jsonl|jsonl\.gz)|\.attempts\.json)$')
 $rawPaths=@($raw|ForEach-Object FullName)
 $rawBytes=[long](($raw|Measure-Object Length -Sum).Sum)
 $metaBytes=[long](($all|Where-Object FullName -notin $rawPaths|Measure-Object Length -Sum).Sum)
 $reserve=if($phase-ceq'before'){1073741824}else{0}
 $pass=$rawBytes+$reserve-le12884901888 -and $metaBytes-le67108864
 NewJson (Join-Path $artifact ('capacity-'+$phase+'.json')) @{schema='pas.cold-publication-run-capacity.v1';raw_bytes=$rawBytes;raw_reserve=$reserve;raw_cap=12884901888;metadata_bytes=$metaBytes;metadata_cap=67108864;pass=$pass;diagnostic_only=$true;qualification_issued=$false;device_operations=0}
 DRequire $pass 'unchanged raw/metadata caps (diagnostic segments included in metadata)'
}
$runnerPin=Binding $self
DRequire ($runnerPin.sha256-ceq$ExpectedRunnerSha256) 'external runner SHA'
$bindingPin=Binding ([IO.Path]::GetFullPath($BuildBinding))
DRequire ($bindingPin.sha256-ceq$ExpectedBuildBindingSha256) 'external build-binding SHA'
$artifact=Join-Path $repo ('out/prelive-cold-publication-diagnostic-'+$Attempt)
Fresh $artifact;$null=[IO.Directory]::CreateDirectory($artifact)
$phase='preflight';$producerEvidence=$null;$auditEvidence=$null;$reportPin=$null;$auditPin=$null;$indexPin=$null;$segmentPins=@();$Campaign=''
$producerStage='producer-publication-'+$Attempt;$auditorStage='audit-publication-'+$Attempt
try {
 $d=DRead $bindingPin
 DRequire ($d.schema-ceq'pas.cold-publication-diagnostic-build.v1' -and $d.source_commit-ceq$FrozenHead) 'fresh diagnostic build identity'
 DBool $d 'source_dirty' $false;DBool $d 'diagnostic_only' $true
 foreach($key in @('formal_rebuilt','qualification_issued','cost_qualification','cost_ready','live_ready')){DBool $d $key $false}
 DInt $d 'translation_unit_count' 3;DInt $d 'device_operations' 0;DInt $d 'new_etw_recordings' 0
 DRequire ($d.core_archive_member_replaced-ceq'core.cpp.obj') 'explicit instrumented core member replacement'
 DBool $d 'original_archive_unchanged' $true
 foreach($proof in @($d.configure_process,$d.build_process,$d.controls_process)){VerifyNative $proof}
 $controls=DRead $d.controls_report
 DRequire ($controls.schema-ceq'pas.cold-publication-diagnostic-controls.v1' -and $controls.source_build_commit-ceq$FrozenHead) 'fresh controls identity'
 DBool $controls 'source_build_dirty' $false;DBool $controls 'pass' $true;DInt $controls 'failed' 0
 DRequire (@($controls.checks).Count-eq16 -and @($controls.checks.id|Sort-Object -Unique).Count-eq16) 'original sixteen aggregate controls'
 foreach($check in $controls.checks){DBool $check 'pass' $true}
 $base=DRead $d.base_build
 DRequire ($d.base_build.sha256-ceq'7478f48f0cb7386836bfddc0bffbdf83c55bdf7f32727ae95bcb107b8a2bb058' -and $base.source_commit-ceq'3ee69edc85acb6b9b968424374a376f351d0e2dd' -and $d.base_source_commit-ceq$base.source_commit) 'preserved original main80 binding'
 $cpu=DRead $base.base_cpu_build
 $auditors=@($cpu.files|Where-Object {[IO.Path]::GetFileName($_.path)-ceq'prelive_audit.exe'})
 DRequire ($auditors.Count-eq1 -and $auditors[0].bytes-eq266752 -and $auditors[0].sha256-ceq'02654e55a8227202e0ab473877474ad7fc0f4f999910d03b8b104889dca069e8') 'actual original full raw auditor'
 $auditor=$auditors[0]
 $auditDlls=@($cpu.files|Where-Object {[IO.Path]::GetDirectoryName($_.path)-ceq[IO.Path]::GetDirectoryName($auditor.path) -and [IO.Path]::GetExtension($_.path)-ceq'.dll'})
 DRequire ($auditDlls.Count-eq1 -and [IO.Path]::GetFileName($auditDlls[0].path)-ceq'z.dll') 'original auditor runtime DLL'
 $producers=@($d.files|Where-Object {[IO.Path]::GetFileName($_.path)-ceq'current_chain.exe'})
 DRequire ($producers.Count-eq1 -and [IO.Path]::GetDirectoryName($producers[0].path)-ceq$d.build_root) 'one fresh diagnostic producer'
 $producer=$producers[0]
 $m=DRead $d.source_manifest
 DRequire ($m.schema-ceq'pas.cooperative-source.v1' -and $m.source_commit-ceq$FrozenHead -and @($m.files).Count-gt0 -and @($m.files).Count-le4096) 'complete diagnostic source manifest'
 DBool $m 'dirty' $false;$sourcePins=@($m.files)
 $snap=DRead $d.source_snapshot
 DRequire ($snap.source_commit-ceq$FrozenHead -and @($snap.files).Count-eq$sourcePins.Count) 'lossless current source snapshot'
 SamePin $snap.source_manifest $d.source_manifest 'snapshot manifest join'
 $snapshotPins=@();$seen=@{}
 foreach($row in $snap.files){
  $matches=@($sourcePins|Where-Object {$_.path-ceq$row.original.path})
  DRequire ($matches.Count-eq1 -and -not$seen.ContainsKey($row.original.path)) 'unique snapshot original'
  SamePin $row.original $matches[0] 'exact original source snapshot row'
  DRequire ($row.copy.bytes-eq$row.original.bytes -and $row.copy.sha256-ceq$row.original.sha256) 'byte-identical source copy'
  $seen[$row.original.path]=$true;$snapshotPins+=@($row.copy)
 }
 $sourcePath=Join-Path $artifact 'source-manifest.json'
 $sourceRows=@($sourcePins|ForEach-Object {CheckEntry $_;$pin=Binding $_.path;$pin.relative_path=[IO.Path]::GetRelativePath($repo,$pin.path).Replace('\','/');DRequire (-not$pin.relative_path.StartsWith('../',[StringComparison]::Ordinal)) 'source inside main';$pin})
 NewJson $sourcePath @{schema='pas.current-source-closure.v1';source_commit=$FrozenHead;repo_root=$repo;dirty=$false;files=$sourceRows;original_build_source_manifest=$d.source_manifest;source_snapshot=$d.source_snapshot;diagnostic_only=$true;qualification_issued=$false;live_ready=$false;device_operations=0}
 $oldCampaignPath=Join-Path $repo 'out/prelive-pre-emulator-aa-main80-e1/campaign.json'
 $oldCampaignPin=Binding $oldCampaignPath
 DRequire ($oldCampaignPin.sha256-ceq'24f144ff844fd7086fa1794eca5999429bc6dcf516eddbb6847da1f0dfafb19b') 'immutable historical campaign reference'
 $oldCampaign=DRead $oldCampaignPin
 $profile=@($oldCampaign.bindings|Where-Object role -ceq'profile');$protocol=@($oldCampaign.bindings|Where-Object role -ceq'cost_protocol')
 DRequire ($profile.Count-eq1 -and $protocol.Count-eq1 -and $protocol[0].sha256-ceq'0000ad66d05649ebc4f60570b58e9a9a4cb58f7e6629279e4d38a945c8d4c060') 'unchanged effective profile and R2'
 if(@(Get-Process cl,cmake,ninja -ErrorAction SilentlyContinue).Count){throw 'heavy builds must finish before diagnostic timing'}
 $environment=Join-Path $artifact 'environment.json'
 NewJson $environment @{platform='windows-x64';time_utc=[DateTime]::UtcNow.ToString('o');os=[Environment]::OSVersion.VersionString;processor=@(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);memory_bytes=(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory;background_processes=@(Get-Process|Sort-Object Id|ForEach-Object {@{pid=$_.Id;name=$_.ProcessName;cpu_s=$_.CPU;working_set=$_.WorkingSet64}});source_commit=$FrozenHead;source_dirty=$false;configuration='Release';guard=(Binding $gate);tool_snapshot=(Binding $tool);heavy_builds_and_tests_running=$false;diagnostic_only=$true;live_ready=$false;device_operations=0}
 $bindings=@();foreach($entry in @(@('candidate_binary',$producer.path),@('auditor_binary',$auditor.path),@('source_manifest',$sourcePath),@('profile',$profile[0].path),@('cost_protocol',$protocol[0].path),@('environment',$environment),@('maintenance_recipe',$self),@('diagnostic_build',$bindingPin.path),@('base_build',$d.base_build.path),@('process_guard',$gate))){$pin=Binding $entry[1];$pin.role=$entry[0];$bindings+=@($pin)}
 $runId=$Attempt+'-tap-publication';$reportPath=Join-Path $artifact ($runId+'.json');$auditPath=Join-Path $artifact ($runId+'-audit.json')
 $Campaign=Join-Path $artifact 'campaign.json'
 NewJson $Campaign @{schema='pas.current-rails-campaign-freeze.v2';id=('cold-publication-diagnostic-'+$Attempt);source_commit=$FrozenHead;configuration='Release';platform='windows-x64';requested=2560;warmup=256;measurement=2304;cadence_ms=20;scenes=@('tap-closed-reset-v1');bindings=$bindings;planned_runs=@(@{run_id=$runId;mode='A';scene='tap-closed-reset-v1';report_path=$reportPath});protocol_modified=$false;protocol_revision='R2';diagnostic_only=$true;single_attempt=$true;old_STOP_resumed=$false;old_results_reused_as_new_runs=$false;qualification_issued=$false;cost_qualification=$false;normal_qualification=$false;AA_qualification=$false;noise_qualification=$false;ABBA_qualification=$false;live_ready=$false;device_operations=0;new_etw_recordings=0}
 $fixedPins=@($runnerPin,$bindingPin,$d.source_manifest,$d.source_snapshot,$d.compiler_environment,$d.base_build,$base.base_cpu_build,$d.controls_report,$auditor,$oldCampaignPin,$profile[0],$protocol[0])+@($d.files)+@($d.compiled_outputs)+@($d.imported_static_libraries|ForEach-Object {$_.binary})+@($d.motion_object)+$snapshotPins+$auditDlls
 foreach($proof in @($d.configure_process,$d.build_process,$d.controls_process)){$fixedPins+=@(EvidencePins $proof)}
 $g=Get-Content -Raw -LiteralPath $gate|ConvertFrom-Json
 $inputs=@(PinPaths $fixedPins)+@(PinPaths $sourcePins)+@(PinPaths $bindings)+@($Campaign)+@(GateInputs)+@($g.guard_source.path,$g.process_source.path,$g.cached_identity_controls.path,$g.identity_wait_controls.path,(Join-Path $repo 'research/pre_emulator/common.ps1'),(Join-Path $repo 'research/cold_scheduler_diagnostic/capacity.ps1'))
 $inputs=@($inputs|Sort-Object -Unique)
 $script:seal=@{files=@($d.files)+@($auditor)+$auditDlls}
 AssertFixed;RawCapacity 'before'
 $phase='producer'
 Native $producerStage $producer.path @('pipeline','A','tap-closed-reset-v1','2560',$Campaign,$runId,$reportPath) $inputs 900
 $producerEvidence=ProcessEvidence $producerStage;VerifyNative $producerEvidence
 $reportPin=Binding $reportPath;$r=DRead $reportPin 1048576
 DRequire ($r.schema-ceq'pas.prelive-complete-chain-cost.v1' -and $r.mode-ceq'A' -and $r.scene-ceq'tap-closed-reset-v1') 'actual diagnostic workload'
 DInt $r 'requested' 2560;DInt $r 'cadence_ms' 20;DInt $r 'warmup_attempts' 256;DInt $r 'device_endpoints' 0
 DBool $r 'diagnostic_only' $true
 foreach($key in @('qualification_issued','cost_qualification','normal_qualification','AA_qualification','noise_qualification','ABBA_qualification','live_ready')){DBool $r $key $false}
 $campaignPin=Binding $Campaign;$p=$r.provenance;$native=DRead $producerEvidence.result
 DRequire ($p.schema-ceq'pas.current-rails-run-provenance.v1' -and $p.source_commit-ceq$FrozenHead -and $p.campaign_sha256-ceq$campaignPin.sha256 -and $p.run_id-ceq$runId -and $p.candidate_binary_sha256-ceq$producer.sha256 -and $p.source_manifest_sha256-ceq(Binding $sourcePath).sha256 -and $p.report_path-ceq$reportPath -and $p.process.pid-eq$native.facts.pid -and $r.process_qos.process_id-eq$native.facts.pid) 'actual producer source/campaign/binary/PID join'
 $indexPin=$r.publication_diagnostic.index;$index=DRead $indexPin
 DRequire ($index.schema-ceq'pas.cold-publication-diagnostic.v1' -and $index.mode-ceq'A' -and $index.scene-ceq$r.scene) 'publication diagnostic schema/mode/scene'
 DRequire ($index.scene-ceq$r.scene -and $index.diagnostic_build_identity.source_commit-ceq$FrozenHead -and $index.diagnostic_build_identity.base_compiled_source_commit-ceq$base.source_commit) 'diagnostic current/base source identity'
 DBool $index.diagnostic_build_identity 'source_dirty' $false;DBool $index 'diagnostic_only' $true;DBool $index 'qualification_issued' $false;DBool $index 'cost_qualification' $false;DBool $index 'live_ready' $false;DInt $index 'device_operations' 0
 DInt $index 'requested' 2560;DInt $index 'cadence_ms' 20;DInt $index.summary 'slots' 2624
 DRequire (@($index.segments).Count-ge1 -and @($index.segments).Count-le5) 'bounded diagnostic segments'
 foreach($row in $index.segments){$s=DRead $row.file;DRequire ($s.schema-ceq'pas.cold-publication-diagnostic-segment.v1' -and @($s.rows).Count-eq$row.rows -and $row.rows-le2048) 'actual bounded numeric segment';$segmentPins+=@($row.file)}
 $phase='auditor';AssertFixed
 $rawPaths=@($reportPath,($reportPath+'.attempts.json'),($reportPath+'.journal.jsonl'))
 foreach($path in $rawPaths){DRequire (Test-Path -LiteralPath $path -PathType Leaf) 'complete raw outputs'}
 Native $auditorStage $auditor.path @($reportPath,$auditPath) @($inputs+$rawPaths+$indexPin.path+(PinPaths $segmentPins)+(PinPaths @(EvidencePins $producerEvidence))) 900
 $auditEvidence=ProcessEvidence $auditorStage;VerifyNative $auditEvidence;$auditPin=Binding $auditPath;$a=DRead $auditPin
 DRequire ($a.schema-ceq'pas.prelive-independent-integrity.v2') 'original full raw auditor schema'
 foreach($key in @('integrity','all_input_processed','metrics_verified','producer_metrics_verified','provenance_join_complete')){DBool $a $key $true}
 SamePin $a.input_bindings.report $reportPin 'actual audited report';SamePin $a.verifier_binary $auditor 'actual auditor image'
 DRequire (($a.run_provenance|ConvertTo-Json -Depth 20 -Compress)-ceq($p|ConvertTo-Json -Depth 20 -Compress)) 'raw journal provenance equals report'
 AssertFixed;RawCapacity 'after'
 NewJson (Join-Path $artifact 'status.json') @{schema='pas.cold-publication-diagnostic-run-status.v1';status='DIAGNOSTIC_COMPLETE';source_commit=$FrozenHead;build=$bindingPin;base_build=$d.base_build;campaign=$campaignPin;source_manifest=(Binding $sourcePath);report=$reportPin;attempts=(Binding $rawPaths[1]);journal=(Binding $rawPaths[2]);audit=$auditPin;publication_index=$indexPin;publication_segments=$segmentPins;producer_process=$producerEvidence;audit_process=$auditEvidence;raw_integrity_verified=$true;actual_producer_provenance_verified=$true;producer_deadline_overruns=$r.producer_deadline_overruns;publication_deadline_overruns=$r.publication_deadline_overruns;diagnostic_measurement_runs=1;original_STOP_preserved=$true;old_STOP_resumed=$false;diagnostic_only=$true;qualification_issued=$false;cost_qualification=$false;cost_ready=$false;live_ready=$false;device_operations=0;new_etw_recordings=0}
 Write-Output 'one publication diagnostic complete; original timing negatives preserved; no qualification'
} catch {
 NewJson (Join-Path $artifact 'stop-status.json') @{schema='pas.cold-publication-diagnostic-run-stop.v1';status='STOP';phase=$phase;reason=$_.Exception.Message;source_commit=$FrozenHead;build=$bindingPin;campaign_path=$Campaign;report=$reportPin;audit=$auditPin;publication_index=$indexPin;publication_segments=$segmentPins;producer_process=$producerEvidence;audit_process=$auditEvidence;diagnostic_only=$true;qualification_issued=$false;cost_qualification=$false;cost_ready=$false;live_ready=$false;device_operations=0;new_etw_recordings=0}
 throw
}
