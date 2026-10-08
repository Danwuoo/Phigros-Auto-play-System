$ErrorActionPreference='Stop'
$repo='C:\Users\wurre\Desktop\PAS-windows-readiness-20261007';$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh=(Get-Command pwsh).Source;$Mode='release';$Operation='tests';$Campaign='';$script:inputClosures=@{};$script:maintenancePath=$PSCommandPath
$gate=Join-Path $out 'guard-v3-native-01/native-gate.json';$tool=Join-Path $out 'tool-snapshot-01/tool-snapshot.json';$recipe=Join-Path $repo 'research/prelive_current/run.ps1'
$tok=$null;$err=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tok,[ref]$err);if($err.Count){throw 'shared parse'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage','Native')){
 $d=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name);if($d.Count -ne 1){throw 'shared function'};$t=$d[0].Extent.Text
 if($name -ceq 'Native'){$t=$t.Replace('$PSCommandPath','$script:maintenancePath').Replace('tools/zero_miss_windows/invoke.ps1','research/windows_process_guard_v3/invoke.ps1')}
 if($name -ceq 'Capacity'){$t=$t.Replace('12884901888','34359738368')};if($name -ceq 'GateInputs'){$t=$t.Replace('pas.windows-offline-process-gate.v1','pas.windows-offline-process-gate.v3')};. ([scriptblock]::Create($t))
}
$Attempt='coopaa01';$Operation='pipeline';$Evaluation=Join-Path $repo 'out/windows-handoff/guard-v3-evaluation-01/evaluation.json'
$ev=Get-Content -LiteralPath $Evaluation -Raw|ConvertFrom-Json
if($ev.stopped -or $ev.completed -ne 27 -or $ev.planned -ne 27 -or $ev.device_operations -ne 0){throw 'full 27 process controls'};CheckEntry $ev.gate;if($ev.gate.path -cne $gate){throw 'same evaluated guard'};foreach($r in $ev.rows){CheckEntry $r.receipt}
$buildSeal=Join-Path $repo 'out/prelive-cooperative-evaluation-freeze02/candidate-builds.json';$newBuild=Get-Content -LiteralPath $buildSeal -Raw|ConvertFrom-Json
$sourceBinding=$newBuild.source_manifest;CheckEntry $sourceBinding;$sourceValue=Get-Content -LiteralPath $sourceBinding.path -Raw|ConvertFrom-Json;$commit=$newBuild.source_commit
if($commit -cne '45f8a5f9ec0f256b07f1b31a5bead1bf0a3aacf1' -or $sourceValue.dirty -or $sourceValue.source_commit -cne $commit -or (& git -C $sourceValue.repo_root rev-parse HEAD) -cne $commit -or (& git -C $sourceValue.repo_root status --porcelain)){throw 'clean frozen candidate'}
foreach($f in $sourceValue.files){CheckEntry $f}
if($newBuild.tests.Count -ne 39 -or $newBuild.builds.Count -ne 3 -or $newBuild.supplemental_builds.Count -ne 3 -or -not $newBuild.cost_measurement_prerequisites_pass -or -not $newBuild.cold_affected_regressions_pass -or $newBuild.device_operations -ne 0){throw 'current 39 affected prerequisites'}
CheckEntry $newBuild.proof_collection;$pc=Get-Content -LiteralPath $newBuild.proof_collection.path -Raw|ConvertFrom-Json
if(-not $pc.all_partition_files_rehashed -or $pc.per_partition_file_cap -ne 8192 -or $pc.native_launches -ne 0 -or $pc.partitions.Count -ne 3 -or $pc.final_partition_unique_files -gt 8192){throw 'bounded proof partitions'}
foreach($p in $pc.partitions){CheckEntry $p;$v=Get-Content -LiteralPath $p.path -Raw|ConvertFrom-Json;if(-not $v.all_files_rehashed -or $v.unique_files -gt 8192 -or $v.native_launches -ne 0){throw 'proof partition'}}
foreach($t in $newBuild.tests){CheckEntry $t.report;CheckEntry $t.receipt;$v=Get-Content -LiteralPath $t.receipt.path -Raw|ConvertFrom-Json;Receipt $v.stage}
foreach($b in @($newBuild.builds)+@($newBuild.supplemental_builds)){foreach($f in $b.files){CheckEntry $f};foreach($r in @($b.configure_receipt)+@($b.build_receipts)){CheckEntry $r;$v=Get-Content -LiteralPath $r.path -Raw|ConvertFrom-Json;Receipt $v.stage}}
$release=@($newBuild.builds|Where-Object configuration -ceq 'release')[0];$root=$release.build_root
$toolBuildPath=$newBuild.cost_tools.path;CheckEntry $newBuild.cost_tools;$toolBuild=Get-Content -LiteralPath $toolBuildPath -Raw|ConvertFrom-Json
if($toolBuild.source_commit -cne $commit -or $toolBuild.tests.Count -ne 2 -or $toolBuild.device_operations -ne 0){throw 'same source complete cost tools'};foreach($f in $toolBuild.files){CheckEntry $f};foreach($r in @($toolBuild.configure_receipt)+@($toolBuild.build_receipts)){CheckEntry $r;$v=Get-Content -LiteralPath $r.path -Raw|ConvertFrom-Json;Receipt $v.stage};foreach($r in $toolBuild.tests){CheckEntry $r.report;CheckEntry $r.receipt;$v=Get-Content -LiteralPath $r.receipt.path -Raw|ConvertFrom-Json;Receipt $v.stage}
$analyzer=Binding (Join-Path $toolBuild.build_root 'cost_analysis.exe');$analyzer.role='analyzer_binary';$auditor=Binding (Join-Path $root 'prelive_audit.exe');$auditor.role='auditor_binary'
$seal=@{files=@($release.files)+@($toolBuild.files)};$maintenanceCommit=$commit
$prefixPath=$newBuild.prefix.path;CheckEntry $newBuild.prefix;$prefix=Get-Content -LiteralPath $prefixPath -Raw|ConvertFrom-Json
if(-not $prefix.pass -or $prefix.source_commit -cne $commit -or $prefix.frames -ne 3063 -or $prefix.device_operations -ne 0 -or $prefix.live_ready -or $prefix.binary.path -cne (Join-Path $root 'current_chain.exe')){throw 'current 3063 prefix'};foreach($f in @($prefix.binary,$prefix.selection,$prefix.report,$prefix.audit,$prefix.source_manifest,$prefix.producer_receipt,$prefix.auditor_receipt)){CheckEntry $f}
$prefixCapacity=$newBuild.capacity.path;CheckEntry $newBuild.capacity;$cap=Get-Content -LiteralPath $prefixCapacity -Raw|ConvertFrom-Json;if(-not $cap.pass -or $cap.bytes -gt 6442450944 -or $cap.cap -ne 6442450944){throw 'declared 6 GiB candidate cap'}
$campaignRoot=Join-Path $repo ('out/prelive-aa-'+$Attempt);Fresh $campaignRoot;[IO.Directory]::CreateDirectory($campaignRoot)|Out-Null
$environment=Join-Path $campaignRoot 'environment.json'
NewJson $environment @{platform='windows-x64';time_utc=[DateTime]::UtcNow.ToString('o');os=[Environment]::OSVersion.VersionString;processor=@(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);memory_bytes=(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory;background_processes=@(Get-Process|Sort-Object Id|ForEach-Object {@{pid=$_.Id;name=$_.ProcessName;cpu_s=$_.CPU;working_set=$_.WorkingSet64}});source_commit=$commit;source_dirty=$false;configuration='Release';guard=(Binding $gate);build_binding=(Binding $buildSeal);tool_snapshot=(Binding $tool);heavy_builds_and_tests_running=$false}
$r2Protocol=Join-Path $sourceValue.repo_root 'docs/research/zero-miss-20261006/windows/PRELIVE_COST_PROTOCOL_R2_20261007.md';if((Binding $r2Protocol).sha256 -cne '0000ad66d05649ebc4f60570b58e9a9a4cb58f7e6629279e4d38a945c8d4c060'){throw 'unchanged R2'}
$oldCampaign=Join-Path $repo 'out/prelive-aa-aa01/campaign.json';$old=Get-Content -LiteralPath $oldCampaign -Raw|ConvertFrom-Json;foreach($b in $old.bindings){CheckEntry $b}
$tap=@($newBuild.tests|Where-Object {$_.configuration -ceq 'release' -and $_.test -ceq 'tap_reset_workload_tests'})[0];$tv=Get-Content -LiteralPath $tap.report.path -Raw|ConvertFrom-Json;if($tv.failed -ne 0 -or $tv.assertions -ne 56 -or $tv.corpus.Count -ne 96){throw 'Tap 56 controls 96 RGB rows'}
foreach($rel in @('research/prelive_current/streaming_workload.hpp','research/prelive_current/chain.cpp')){if((Binding (Join-Path $sourceValue.repo_root $rel)).sha256 -cne (Binding (Join-Path 'C:/Users/wurre/Desktop/PAS-journal-cost-20261007' $rel)).sha256){throw 'unchanged corpus generator source'}}
$oldCorpusBinding=@($old.bindings|Where-Object role -ceq 'corpus')[0];$oc=Get-Content -LiteralPath $oldCorpusBinding.path -Raw|ConvertFrom-Json;$rows=@($oc.rows|Where-Object scene -cne 'tap');foreach($r in $tv.corpus){$rows+=@(@{scene='tap-closed-reset-v1';phase=$r.phase;offset=$r.offset;bytes=$r.bytes;sha256=$r.sha256})};if($rows.Count -ne 351){throw '351 full corpus'}
$corpusPath=Join-Path $campaignRoot 'corpus-r2.json';NewJson $corpusPath @{schema='pas.prelive-r2-corpus.v1';rows=$rows;tap_cycle_frames=32;tap_offsets=@(-2,0,2);hold_cycle_frames=81;hold_offsets=@(-2,0,2);legacy_dense_loop_frames=12;cadence_ms=20;requested=2560;warmup=256;measurement=2304;physical_gold=0;device_endpoints=0;old_corpus=$oldCorpusBinding;tap_functional_report=$tap.report}
$bindings=@($old.bindings|Where-Object role -ceq 'profile')
foreach($p in @(@('candidate_binary',(Join-Path $root 'current_chain.exe')),@('source_manifest',$sourceBinding.path),@('build_binding',$buildSeal),@('prerequisites',$buildSeal),@('current_candidate_prefix',$prefixPath),@('candidate_capacity',$prefixCapacity),@('analyzer_binary',$analyzer.path),@('auditor_binary',$auditor.path),@('cost_tools_build',$toolBuildPath),@('measurement_prerequisites_plan',$newBuild.measurement_plan.path),@('cold_proof_collection',$newBuild.proof_collection.path),@('cost_protocol',$r2Protocol),@('corpus',$corpusPath),@('environment',$environment),@('process_guard',$gate),@('maintenance_recipe',$PSCommandPath),@('process_evaluation',$Evaluation))){$b=Binding $p[1];$b.role=$p[0];$bindings+=@($b)}
$plans=@();foreach($p in $old.planned_runs){$copy=@{run_id=$p.run_id.Replace('aa01',$Attempt);scene=$(if($p.scene -ceq 'tap'){'tap-closed-reset-v1'}else{$p.scene});mode=$p.mode;report_path=$p.report_path.Replace('aa01',$Attempt)};Fresh $copy.report_path;$plans+=@($copy)}
$Campaign=Join-Path $campaignRoot 'campaign.json';$newCampaign=@{};foreach($p in $old.PSObject.Properties){$newCampaign[$p.Name]=$p.Value}
$newCampaign.id='windows-aa-'+$Attempt;$newCampaign.source_commit=$commit;$newCampaign.bindings=$bindings;$newCampaign.planned_runs=$plans;$newCampaign.scenes=@('tap-closed-reset-v1','hold-closed-reset-v2','dense');$newCampaign.maintenance_commit=$maintenanceCommit
$newCampaign.change='sole owner checkpoints during exact all-RGB hashing; refreshed QPC and execution ledger; same R2 corpus, cadence, denominator and numeric gates'
$newCampaign.previous_numeric_STOP=Binding (Join-Path $repo 'out/prelive-aa-journalaa01/status.json');$newCampaign.original_stopped_campaign=Binding $oldCampaign;$newCampaign.old_STOP_resumed=$false;$newCampaign.old_results_reused_as_new_runs=$false;$newCampaign.candidate_source_and_binary_changed=$true;$newCampaign.process_guard_changed=$false;$newCampaign.guard_version='pas.owned-held-identity.v3';$newCampaign.thresholds_cadence_and_order_changed=$false;$newCampaign.corpus_changed=$false;$newCampaign.protocol_revision='R2'
$newCampaign.build_storage_declaration=$newBuild.measurement_plan;$newCampaign.storage_declaration=Binding (Join-Path $repo 'docs/research/live-readiness-20261007/JOURNAL_COST_PHASE_CAPACITY_20261007.md');$newCampaign.accumulated_storage_cap_bytes=34359738368;$newCampaign.new_candidate_storage_cap_bytes=6442450944;$newCampaign.storage_scope_revision=$true;$newCampaign.new_cost_raw_cap_bytes=12884901888;$newCampaign.new_cost_metadata_cap_bytes=268435456
foreach($f in @('schema','configuration','platform','requested','warmup','measurement','cadence_ms','protocol_modified','live_ready','device_operations')){if(-not $newCampaign.ContainsKey($f) -or (ConvertTo-Json -InputObject $newCampaign[$f] -Compress) -cne (ConvertTo-Json -InputObject $old.$f -Compress)){throw 'original frozen contract field'}}
NewJson $Campaign $newCampaign
$g=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$inputs=@($PSCommandPath,$recipe,$buildSeal,$Campaign,$Evaluation,$prefixPath,$toolBuildPath)+$bindings.path+$sourceValue.files.path+$seal.files.path+$newBuild.tests.report.path+$newBuild.tests.receipt.path+$toolBuild.tests.report.path+$toolBuild.tests.receipt.path+$toolBuild.build_receipts.path+$pc.partitions.path+@($prefix.report.path,$prefix.audit.path,$prefix.producer_receipt.path,$prefix.auditor_receipt.path,$newCampaign.build_storage_declaration.path,$newCampaign.storage_declaration.path)+(GateInputs)+@($g.guard_source.path,$g.process_source.path,$g.cached_identity_controls.path,$g.identity_wait_controls.path)
function CostPhaseCapacity($id){
 $rawFiles=@(Get-ChildItem -LiteralPath $pkg -File|Where-Object {$_.Name -like 'pipeline-release-journalaa*' -or $_.Name -like 'pipeline-release-journalab*' -or $_.Name -like 'pipeline-release-journalstress*' -or $_.Name -like 'pipeline-release-coopaa*'})
 $rawBytes=[long](($rawFiles|Measure-Object Length -Sum).Sum)
 $meta=@(Get-ChildItem -LiteralPath $pkg -File|Where-Object {$_.Name -like '*coopaa*' -and $_.FullName -notin $rawFiles.FullName})+@(Get-ChildItem -LiteralPath $campaignRoot -File -Recurse)
 foreach($d in @(Get-ChildItem -LiteralPath $out -Directory|Where-Object Name -like '*coopaa*')){$f=@(Get-ChildItem -LiteralPath $d.FullName -File -Recurse);if([long](($f|Measure-Object Length -Sum).Sum) -gt 67108864){throw '64 MiB native stage'};$meta+=$f}
 $mb=[long](($meta|Measure-Object Length -Sum).Sum);$pass=$rawBytes -le 12884901888 -and $mb -le 67108864
 NewJson (Join-Path $campaignRoot ('cost-capacity-'+$id+'.json')) @{raw_bytes=$rawBytes;raw_cap=12884901888;AA_metadata_bytes=$mb;AA_metadata_cap=67108864;accumulated_cap=34359738368;pass=$pass;offline=$true;device_operations=0};if(-not $pass){throw 'predeclared capacity'}
}
$runs=@();$statuses=@();$stopped=$false;$reason=''
try{
 for($i=0;$i -lt $plans.Count;$i++){
  $plan=$plans[$i];$id=$plan.run_id;$stage='pipeline-prelive-release-'+$id
  Native $stage (Join-Path $root 'current_chain.exe') @('pipeline','A',$plan.scene,'2560',$Campaign,$id,$plan.report_path) $inputs 900
  $audit=Join-Path $campaignRoot ($id+'-audit.json')
  Native ('audit-prelive-release-'+$id) $auditor.path @($plan.report_path,$audit) @($inputs+$plan.report_path+($plan.report_path+'.attempts.json')+($plan.report_path+'.journal.jsonl')) 900
  $entry=Binding $plan.report_path;$entry.run_id=$id;$entry.scene=$plan.scene;$entry.audit_path=$audit;$entry.audit_sha256=(Binding $audit).sha256;$process=@{}
  foreach($pair in @(@('specification',(Join-Path $pkg ($stage+'-direct-spec.json'))),@('input_closure',(Join-Path $pkg ($stage+'-input-closure-verification.json'))),
   @('result',(Join-Path $out ($stage+'/'+$stage+'-result.json'))),@('verification',(Join-Path $out ($stage+'/'+$stage+'-verification.json'))),
   @('freeze',(Join-Path $out ($stage+'/freeze.json'))),@('state',(Join-Path $out ($stage+'/state.json'))),@('contract',(Join-Path $out ($stage+'/contract.json'))),
   @('command',(Join-Path $out ($stage+'/'+$stage+'-command.json'))))){$process[$pair[0]]=Binding $pair[1]}
  $entry.process_evidence=$process;$runs+=@($entry)
  $input=Join-Path $campaignRoot ($id+'-gate-input.json');NewJson $input @{schema='pas.current-rails-cost-input.v2';campaign=(Binding $Campaign);plan_index=$i;run=$entry}
  $verdict=Join-Path $campaignRoot ($id+'-gate.json')
  Native ('gate-prelive-release-'+$id) $analyzer.path @('run',$input,$verdict) @($inputs+$input+$audit+$plan.report_path+($plan.report_path+'.attempts.json')+($plan.report_path+'.journal.jsonl')+$process.Values.path) 900
  $value=Get-Content -LiteralPath $verdict -Raw|ConvertFrom-Json
  $statuses+=@(@{run_id=$id;status=$(if($value.validity.pass){'PASS'}else{'NOT_READY'});verdict=(Binding $verdict)})
  CostPhaseCapacity $id
  if(-not $value.validity.pass){throw ('necessary-cost-validity-failed '+$id)}
 }
}catch{$stopped=$true;$reason=$_.Exception.Message}
NewJson (Join-Path $campaignRoot 'status.json') @{schema='pas.windows-aa-batch-status.v1';campaign=(Binding $Campaign);source_commit=$commit;maintenance_commit=$maintenanceCommit;
 normal_stopped=$stopped;reason=$reason;runs=$runs;statuses=$statuses;
 remaining=@($plans|Where-Object {$_.run_id -notin $statuses.run_id}|ForEach-Object {@{run_id=$_.run_id;status=$(if(Test-Path -LiteralPath $_.report_path){'PRODUCED_BUT_UNQUALIFIED'}else{'NOT_RUN'})}});
 all_12_cost_validity_pass=($statuses.Count -eq 12 -and -not $stopped);noise_qualified=$false;ABBA='NOT_RUN';live_ready=$false;device_operations=0}
if($stopped){throw $reason}
$aaInput=Join-Path $campaignRoot 'aa-input.json';NewJson $aaInput @{schema='pas.current-rails-cost-input.v2';campaign=(Binding $Campaign);runs=$runs;normal_stopped=$false}
$noise=Join-Path $campaignRoot 'noise-freeze.json'
$runPaths=@($runs|ForEach-Object {$_.path;$_.audit_path;$_.path+'.attempts.json';$_.path+'.journal.jsonl';$_.process_evidence.Values.path})
Native ('noise-prelive-release-'+$Attempt) $analyzer.path @('aa',$aaInput,$noise) @($inputs+$aaInput+$runPaths) 900
$noiseValue=Get-Content -LiteralPath $noise -Raw|ConvertFrom-Json
NewJson (Join-Path $campaignRoot 'noise-status.json') @{noise=(Binding $noise);adequate=[bool]$noiseValue.ready;comparison_allowed=[bool]$noiseValue.comparison_allowed;ABBA='NOT_RUN';live_ready=$false;device_operations=0}
if(-not $noiseValue.ready){throw 'AA-noise-inadequate-ABBA-not-permitted'}
