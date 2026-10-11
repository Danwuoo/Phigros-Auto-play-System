param([ValidatePattern('^main[0-9]{2}$')][string]$BuildAttempt='main12',[ValidatePattern('^[a-z0-9-]{1,16}$')][string]$Attempt='main12',[string]$NoiseRepairBinding='')
. (Join-Path $PSScriptRoot 'common.ps1');$script:maintenancePath=$PSCommandPath
if($NoiseRepairBinding){. (Join-Path $PSScriptRoot 'noise_repair_build.ps1');LoadNoiseRepairBuild $NoiseRepairBinding}
else{LoadBuilds $BuildAttempt}
$aaRoot=Join-Path $repo ('out/prelive-pre-emulator-aa-'+$Attempt)
$aaInput=Join-Path $aaRoot 'aa-input.json';$noisePath=Join-Path $aaRoot 'noise-freeze.json';$aaCampaign=Join-Path $aaRoot 'campaign.json'
$noise=Get-Content -LiteralPath $noisePath -Raw|ConvertFrom-Json;$aa=Get-Content -LiteralPath $aaInput -Raw|ConvertFrom-Json
if(-not $noise.ready -or -not $noise.comparison_allowed -or $aa.normal_stopped -or $aa.runs.Count -ne 12){throw 'complete AA12 and adequate noise required; ABBA NOT_RUN'}
CheckEntry $aa.campaign;$original=Get-Content -LiteralPath $aaCampaign -Raw|ConvertFrom-Json
if($original.source_commit -cne $commit -or $aa.campaign.path -cne $aaCampaign){throw 'same candidate AA required'}
foreach($b in $original.bindings){CheckEntry $b}
$artifact=Join-Path $repo ('out/prelive-pre-emulator-abba-'+$Attempt);Fresh $artifact;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$environment=Join-Path $artifact 'environment.json';$oldEnv=@($original.bindings|Where-Object role -ceq 'environment')[0]
$envValue=Get-Content -LiteralPath $oldEnv.path -Raw|ConvertFrom-Json;$envValue.time_utc=[DateTime]::UtcNow.ToString('o');$envValue.background_processes=@(Get-Process|Sort-Object Id|ForEach-Object {@{pid=$_.Id;name=$_.ProcessName;cpu_s=$_.CPU;working_set=$_.WorkingSet64}})
if(@(Get-Process cl,cmake,ninja -ErrorAction SilentlyContinue).Count){throw 'heavy builds must finish before cost'}
NewJson $environment $envValue
$bindings=@($original.bindings|Where-Object role -notin @('environment','maintenance_recipe'))
foreach($p in @(@('environment',$environment),@('maintenance_recipe',$PSCommandPath),@('aa_input',$aaInput),@('aa_noise',$noisePath),@('aa_campaign',$aaCampaign),@('comparison_analyzer_binary',(Exe 'cost_comparison')),@('early_batch_verifier_binary',(Exe 'batch_check')))){$b=Binding $p[1];$b.role=$p[0];$bindings+=@($b)}
$plans=@();foreach($scene in $original.scenes){for($batch=1;$batch -le 3;$batch++){foreach($offset in 0..3){$mode=@('A','B','B','A')[$offset];$id=$Attempt+'-'+$scene+'-'+$batch+'-'+$offset;$path=Join-Path $artifact ($id+'.json');Fresh $path;$plans+=@(@{run_id=$id;scene=$scene;mode=$mode;batch=$batch;report_path=$path})}}}
$Campaign=Join-Path $artifact 'campaign.json';$c=@{};foreach($p in $original.PSObject.Properties){$c[$p.Name]=$p.Value}
$c.id='windows-abba-'+$Attempt;$c.bindings=$bindings;$c.planned_runs=$plans;$c.comparison_kind='ABBA';$c.old_STOP_resumed=$false;NewJson $Campaign $c
$inputs+=@($Campaign)+$bindings.path+@($aa.runs|ForEach-Object {$_.path;$_.audit_path;$_.path+'.attempts.json';$_.path+'.journal.jsonl';$_.process_evidence.PSObject.Properties.Value.path})
$Operation='pipeline';$runs=@();$statuses=@();$batchRows=@();$stopped=$false;$reason=''
try{for($i=0;$i -lt $plans.Count;$i++){
 $p=$plans[$i];$id=$p.run_id;$stage='pipeline-abba-pre-emulator-'+$id
 Native $stage (Exe 'current_chain') @('pipeline',$p.mode,$p.scene,'2560',$Campaign,$id,$p.report_path) $inputs 900
 if($NoiseRepairBinding){
  $priority=(Get-Content -LiteralPath $p.report_path -Raw|ConvertFrom-Json).consumer_priority
  if(-not $priority.verified -or -not $priority.applied -or -not $priority.restored -or $priority.requested -ne $noiseRepairBuild.consumer_priority_requested -or $priority.applied_readback -ne $noiseRepairBuild.consumer_priority_requested -or $priority.process_class -ne 32 -or $priority.process_class_after_close -ne 32 -or $priority.restored_readback -ne $priority.previous -or $priority.failure -cne 'none' -or $priority.rollback_failure -cne 'none' -or $priority.fallback_attempted){throw 'same A/B consumer scheduling readback and restoration required'}
  NoiseRepairVerifyWorkerPlacement (Get-Content -LiteralPath $p.report_path -Raw|ConvertFrom-Json)
 }
 $audit=Join-Path $artifact ($id+'-audit.json');$raw=@($p.report_path,($p.report_path+'.attempts.json'),($p.report_path+'.journal.jsonl'))
 Native ('audit-abba-pre-emulator-'+$id) (Exe 'prelive_audit') @($p.report_path,$audit) @($inputs+$raw) 900
 $e=Binding $p.report_path;$e.run_id=$id;$e.scene=$p.scene;$e.audit_path=$audit;$e.audit_sha256=(Binding $audit).sha256;$e.process_evidence=ProcessEvidence $stage;$runs+=@($e)
 $input=Join-Path $artifact ($id+'-gate-input.json');NewJson $input @{schema='pas.windows-cost-comparison-input.v1';aa_input=(Binding $aaInput);noise=(Binding $noisePath);campaign=(Binding $Campaign);plan_index=$i;run=$e}
 $verdict=Join-Path $artifact ($id+'-gate.json')
 Native ('gate-abba-pre-emulator-'+$id) (Exe 'cost_comparison') @('--operation','run','--input',$input,'--expected-sha256',(Binding $input).sha256,'--source-commit',$commit,'--analyzer',(Exe 'cost_analysis'),'--output',$verdict) @($inputs+$raw+$input+$audit+$e.process_evidence.Values.path) 900
 $v=Get-Content -LiteralPath $verdict -Raw|ConvertFrom-Json;$statuses+=@(@{run_id=$id;pass=$v.validity.pass;verdict=(Binding $verdict)})
 if(-not $v.validity.pass){throw ('necessary ABBA gate failed '+$id)}
 if($i%4 -eq 3){
  $batchInput=Join-Path $artifact ($id+'-batch-input.json');$batchOut=Join-Path $artifact ($id+'-batch-verdict.json')
  NewJson $batchInput @{schema='pas.pre-emulator-abba-batch.v1';noise=(Binding $noisePath);scene=$p.scene;source_commit=$commit;runs=@($runs|Select-Object -Last 4)}
  $lastRaw=@($runs|Select-Object -Last 4|ForEach-Object {$_.path})
  Native ('batch-abba-pre-emulator-'+$id) (Exe 'batch_check') @($batchInput,$batchOut) @($inputs+$batchInput+$lastRaw) 900
  $b=Get-Content -LiteralPath $batchOut -Raw|ConvertFrom-Json;$batchRows+=@(@{scene=$p.scene;batch=$p.batch;verdict=(Binding $batchOut);pass=$b.non_regression})
  if(-not $b.non_regression){throw ('ABBA batch non-regression failed '+$id)}
 }
 Write-Output ($id+' necessary gates PASS')
 $bytes=[long]((Get-ChildItem -LiteralPath $artifact -File -Recurse|Measure-Object Length -Sum).Sum);if($bytes+1073741824 -gt 12884901888){throw 'ABBA raw headroom'}
}}catch{$stopped=$true;$reason=$_.Exception.Message}
NewJson (Join-Path $artifact 'status.json') @{schema='pas.pre-emulator-abba-status.v1';source_commit=$commit;campaign=(Binding $Campaign);runs=$runs;statuses=$statuses;batches=$batchRows;stopped=$stopped;reason=$reason;remaining=@($plans|Where-Object run_id -notin $statuses.run_id|ForEach-Object {@{run_id=$_.run_id;status='NOT_RUN'}});live_ready=$false;device_operations=0}
if($stopped){throw $reason}
$input=Join-Path $artifact 'comparison-input.json';$output=Join-Path $artifact 'comparison.json'
NewJson $input @{schema='pas.windows-cost-comparison-input.v1';aa_input=(Binding $aaInput);noise=(Binding $noisePath);campaign=(Binding $Campaign);runs=$runs;normal_stopped=$false}
$raw=@($runs|ForEach-Object {$_.path;$_.audit_path;$_.path+'.attempts.json';$_.path+'.journal.jsonl';$_.process_evidence.Values.path})
Native ('comparison-pre-emulator-'+$Attempt) (Exe 'cost_comparison') @('--operation','compare','--input',$input,'--expected-sha256',(Binding $input).sha256,'--source-commit',$commit,'--analyzer',(Exe 'cost_analysis'),'--output',$output) @($inputs+$input+$raw) 900
$v=Get-Content -LiteralPath $output -Raw|ConvertFrom-Json;if(-not $v.ready){throw 'full original 36-run comparison NOT_READY'}
