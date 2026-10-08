param([ValidatePattern('^main[0-9]{2}$')][string]$BuildAttempt='main12',[ValidatePattern('^[a-z0-9-]{1,16}$')][string]$Attempt='main12')
. (Join-Path $PSScriptRoot 'common.ps1');$script:maintenancePath=$PSCommandPath
LoadBuilds $BuildAttempt
$prefixPath=Join-Path $repo ('out/prelive-pre-emulator-prefix-'+$Attempt+'/prefix-receipt.json')
$prefix=Get-Content -LiteralPath $prefixPath -Raw|ConvertFrom-Json
if(-not $prefix.pass -or $prefix.source_commit -cne $commit -or $prefix.frames -ne 3063 -or $prefix.live_ready -or $prefix.device_operations -ne 0 -or $prefix.binary.path -cne (Exe 'current_chain')){throw 'same current full prefix required'}
foreach($b in @($prefix.source_manifest,$prefix.binary,$prefix.selection,$prefix.report,$prefix.audit,$prefix.producer_receipt,$prefix.auditor_receipt)){CheckEntry $b}
$artifact=Join-Path $repo ('out/prelive-pre-emulator-aa-'+$Attempt);Fresh $artifact;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$environment=Join-Path $artifact 'environment.json'
NewJson $environment @{platform='windows-x64';time_utc=[DateTime]::UtcNow.ToString('o');os=[Environment]::OSVersion.VersionString;processor=@(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);memory_bytes=(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory;background_processes=@(Get-Process|Sort-Object Id|ForEach-Object {@{pid=$_.Id;name=$_.ProcessName;cpu_s=$_.CPU;working_set=$_.WorkingSet64}});source_commit=$commit;source_dirty=$false;configuration='Release';guard=(Binding $gate);tool_snapshot=(Binding $tool);heavy_builds_and_tests_running=$false}
$oldRoot=Join-Path $repo 'out/retained/consolidation-20261008/worktrees/PAS-windows-readiness-20261007/out/prelive-aa-aa01'
$old=Get-Content -LiteralPath (Join-Path $oldRoot 'campaign.json') -Raw|ConvertFrom-Json
$originalCorpus=@($old.bindings|Where-Object role -ceq 'corpus')[0];$corpusFile=Join-Path $oldRoot 'corpus.json';$actual=Binding $corpusFile
if($actual.bytes -ne $originalCorpus.bytes -or $actual.sha256 -cne $originalCorpus.sha256){throw 'preserved original corpus SHA'}
$oldCorpus=Get-Content -LiteralPath $corpusFile -Raw|ConvertFrom-Json
$tap=@($tests|Where-Object {$_.test -ceq 'tap_reset_workload_tests' -and $_.report.path.Contains('release')})
if($tap.Count -ne 1){throw 'one current Release Tap corpus'};$tv=Get-Content -LiteralPath $tap[0].report.path -Raw|ConvertFrom-Json
if($tv.failed -ne 0 -or $tv.assertions -ne 56 -or $tv.corpus.Count -ne 96){throw 'Tap controls and 96 fixed RGB SHA rows'}
$rows=@($oldCorpus.rows|Where-Object scene -cne 'tap');foreach($r in $tv.corpus){$rows+=@(@{scene='tap-closed-reset-v1';phase=$r.phase;offset=$r.offset;bytes=$r.bytes;sha256=$r.sha256})}
if($rows.Count -ne 351){throw '351 corpus rows required'}
$corpus=Join-Path $artifact 'corpus-r2.json';NewJson $corpus @{schema='pas.prelive-r2-corpus.v1';rows=$rows;tap_cycle_frames=32;tap_offsets=@(-2,0,2);hold_cycle_frames=81;hold_offsets=@(-2,0,2);legacy_dense_loop_frames=12;cadence_ms=20;requested=2560;warmup=256;measurement=2304;physical_gold=0;device_endpoints=0;original_corpus=(Binding $corpusFile);tap_functional_report=$tap[0].report}
$manifest=Join-Path $artifact 'source-manifest.json';$inputs+=@(SourceManifest $manifest)
$protocol=Join-Path $repo 'docs/research/zero-miss-20261006/windows/PRELIVE_COST_PROTOCOL_R2_20261007.md';if((Binding $protocol).sha256 -cne '0000ad66d05649ebc4f60570b58e9a9a4cb58f7e6629279e4d38a945c8d4c060'){throw 'unchanged R2 protocol'}
$bindings=@();foreach($p in @(@('candidate_binary',(Exe 'current_chain')),@('analyzer_binary',(Exe 'cost_analysis')),@('auditor_binary',(Exe 'prelive_audit')),@('source_manifest',$manifest),@('profile',(Join-Path $repo 'configs/phigros-hd-assist-five-lead35.json')),@('cost_protocol',$protocol),@('corpus',$corpus),@('environment',$environment),@('process_guard',$gate),@('current_candidate_prefix',$prefixPath),@('maintenance_recipe',$PSCommandPath),@('measurement_prerequisites_plan',(Join-Path $PSScriptRoot 'PLAN.md')))){$b=Binding $p[1];$b.role=$p[0];$bindings+=@($b)}
$plans=@();foreach($scene in @('tap-closed-reset-v1','hold-closed-reset-v2','dense')){for($i=1;$i -le 4;$i++){$id=$Attempt+'-'+$scene+'-'+$i;$path=Join-Path $artifact ($id+'.json');Fresh $path;$plans+=@(@{run_id=$id;scene=$scene;mode='A';report_path=$path})}}
$Campaign=Join-Path $artifact 'campaign.json'
NewJson $Campaign @{schema='pas.current-rails-campaign-freeze.v2';id=('windows-aa-'+$Attempt);source_commit=$commit;configuration='Release';platform='windows-x64';requested=2560;warmup=256;measurement=2304;cadence_ms=20;scenes=@('tap-closed-reset-v1','hold-closed-reset-v2','dense');bindings=$bindings;planned_runs=$plans;protocol_modified=$false;protocol_revision='R2';old_STOP_resumed=$false;old_results_reused_as_new_runs=$false;live_ready=$false;device_operations=0}
$inputs+=@($Campaign)+$bindings.path+@($prefix.report.path,$prefix.audit.path,$prefix.producer_receipt.path,$prefix.auditor_receipt.path)
$Operation='pipeline';$runs=@();$statuses=@();$stopped=$false;$reason=''
function CostCapacity($id){
 $raw=@(Get-ChildItem -LiteralPath $artifact -File|Where-Object {$_.Name -match '(\.journal\.(jsonl|jsonl\.gz)|\.attempts\.json)$'})
 $bytes=[long](($raw|Measure-Object Length -Sum).Sum);$metadata=[long]((Get-ChildItem -LiteralPath $artifact -File|Where-Object FullName -notin $raw.FullName|Measure-Object Length -Sum).Sum)
 $pass=$bytes+1073741824 -le 12884901888 -and $metadata -le 67108864
 NewJson (Join-Path $artifact ('capacity-'+$id+'.json')) @{raw_bytes=$bytes;raw_reserve=1073741824;raw_cap=12884901888;metadata_bytes=$metadata;metadata_cap=67108864;pass=$pass;device_operations=0};if(-not $pass){throw 'R2 raw/metadata capacity'}
}
try{
 for($i=0;$i -lt $plans.Count;$i++){
  $p=$plans[$i];$id=$p.run_id;$stage='pipeline-pre-emulator-'+$id;CostCapacity ('before-'+$id)
  Native $stage (Exe 'current_chain') @('pipeline','A',$p.scene,'2560',$Campaign,$id,$p.report_path) $inputs 900
  $audit=Join-Path $artifact ($id+'-audit.json')
  Native ('audit-pre-emulator-'+$id) (Exe 'prelive_audit') @($p.report_path,$audit) @($inputs+$p.report_path+($p.report_path+'.attempts.json')+($p.report_path+'.journal.jsonl')) 900
  $entry=Binding $p.report_path;$entry.run_id=$id;$entry.scene=$p.scene;$entry.audit_path=$audit;$entry.audit_sha256=(Binding $audit).sha256;$entry.process_evidence=ProcessEvidence $stage;$runs+=@($entry)
  $input=Join-Path $artifact ($id+'-gate-input.json');NewJson $input @{schema='pas.current-rails-cost-input.v2';campaign=(Binding $Campaign);plan_index=$i;run=$entry}
  $verdict=Join-Path $artifact ($id+'-gate.json')
  Native ('gate-pre-emulator-'+$id) (Exe 'cost_analysis') @('run',$input,$verdict) @($inputs+$input+$audit+$p.report_path+($p.report_path+'.attempts.json')+($p.report_path+'.journal.jsonl')+$entry.process_evidence.Values.path) 900
  $value=Get-Content -LiteralPath $verdict -Raw|ConvertFrom-Json
  $statuses+=@(@{run_id=$id;status=$(if($value.validity.pass){'PASS'}else{'NOT_READY'});verdict=(Binding $verdict)})
  Write-Output ($id+' '+$statuses[-1].status);CostCapacity ('after-'+$id)
  if(-not $value.validity.pass){throw ('necessary cost gate failed '+$id)}
 }
}catch{$stopped=$true;$reason=$_.Exception.Message}
NewJson (Join-Path $artifact 'status.json') @{schema='pas.windows-aa-batch-status.v1';campaign=(Binding $Campaign);source_commit=$commit;normal_stopped=$stopped;reason=$reason;runs=$runs;statuses=$statuses;remaining=@($plans|Where-Object {$_.run_id -notin $statuses.run_id}|ForEach-Object {@{run_id=$_.run_id;status=$(if(Test-Path -LiteralPath $_.report_path){'PRODUCED_BUT_UNQUALIFIED'}else{'NOT_RUN'})}});all_12_cost_validity_pass=($statuses.Count -eq 12 -and -not $stopped);noise_qualified=$false;ABBA='NOT_RUN';live_ready=$false;device_operations=0}
if($stopped){throw $reason}
$aaInput=Join-Path $artifact 'aa-input.json';NewJson $aaInput @{schema='pas.current-rails-cost-input.v2';campaign=(Binding $Campaign);runs=$runs;normal_stopped=$false}
$noise=Join-Path $artifact 'noise-freeze.json';$runPaths=@($runs|ForEach-Object {$_.path;$_.audit_path;$_.path+'.attempts.json';$_.path+'.journal.jsonl';$_.process_evidence.Values.path})
Native ('noise-pre-emulator-'+$Attempt) (Exe 'cost_analysis') @('aa',$aaInput,$noise) @($inputs+$aaInput+$runPaths) 900
$n=Get-Content -LiteralPath $noise -Raw|ConvertFrom-Json
NewJson (Join-Path $artifact 'noise-status.json') @{noise=(Binding $noise);adequate=[bool]$n.ready;comparison_allowed=[bool]$n.comparison_allowed;ABBA='NOT_RUN';live_ready=$false;device_operations=0}
if(-not $n.ready){throw 'AA noise inadequate; ABBA NOT_RUN'}
