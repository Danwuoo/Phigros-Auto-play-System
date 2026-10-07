param([Parameter(Mandatory)][string]$GateReceipt,[Parameter(Mandatory)][string]$ToolSnapshot,
 [Parameter(Mandatory)][string]$Prerequisites,[Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$PrerequisitesSha256,
 [ValidatePattern('^[a-z0-9-]{1,12}$')][string]$Attempt='win01',
 [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$BuildAttempt='win03')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh=(Get-Command pwsh -ErrorAction Stop).Source
$gate=[IO.Path]::GetFullPath($GateReceipt);$tool=[IO.Path]::GetFullPath($ToolSnapshot)
$Mode='release';$Operation='pipeline';$Campaign='';$script:inputClosures=@{}
$root=Join-Path $repo ('out/prelive-current-release-'+$BuildAttempt)
$recipe=Join-Path $repo 'research/prelive_current/run.ps1';$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'shared-recipe-parser'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage','Native')){
 $function=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name)
 if($function.Count -ne 1){throw ('shared-recipe-function '+$name)};. ([scriptblock]::Create($function[0].Extent.Text))
}
if((Get-FileHash -LiteralPath $Prerequisites).Hash.ToLowerInvariant() -cne $PrerequisitesSha256){throw 'externally-sealed-prerequisites'}
$pre=Get-Content -LiteralPath $Prerequisites -Raw|ConvertFrom-Json
if($pre.schema -cne 'pas.windows-cold-prerequisites.v1' -or -not $pre.pass -or $pre.device_operations -ne 0){throw 'functional-prefix-prerequisites-required'}
foreach($b in $pre.bindings){CheckEntry $b}
$commit=(& git -C $repo rev-parse HEAD)
if((& git -C $repo status --porcelain) -or $commit -cne $pre.source_commit){throw 'clean-same-source-commit-required'}
$buildSeal=Join-Path $pkg ('current-build-release-'+$BuildAttempt+'.json');$seal=Get-Content -LiteralPath $buildSeal -Raw|ConvertFrom-Json
if($seal.target_commit -cne $commit -or $seal.configuration -cne 'Release'){throw 'same-source-Release-build-required'}
foreach($b in @($seal.files)+@($seal.source_files)+@($seal.migration_targets)+@($seal.input_closure_receipts)+@($seal.build_receipts)+@($seal.gate,$seal.tool_snapshot,$seal.source_migration)){CheckEntry $b}
$campaignRoot=Join-Path $repo ('out/prelive-aa-'+$Attempt);Fresh $campaignRoot;[IO.Directory]::CreateDirectory($campaignRoot)|Out-Null
$sourceFiles=@(& git -C $repo ls-files | Where-Object {$_ -match '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)' }|ForEach-Object {
 $relative=$_;$path=Join-Path $repo $relative
 if(-not (Test-Path -LiteralPath $path)){
  # This one Linux-only auxiliary CMake file is sparse because AUX is a
  # reserved Win32 component. Preserve its exact committed blob under a safe
  # flat filename rather than omitting it from the source manifest.
  if($relative -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -or (& git -C $repo ls-files -v -- $relative) -cne ('S '+$relative)){throw 'unexplained-source-missing'}
  $path=Join-Path $campaignRoot 'sparse-linux-aux-CMakeLists.txt'
  $startInfo=[Diagnostics.ProcessStartInfo]::new((Get-Command git.exe).Source)
  $startInfo.UseShellExecute=$false;$startInfo.CreateNoWindow=$true;$startInfo.RedirectStandardOutput=$true
  foreach($arg in @('-C',$repo,'show',($commit+':'+$relative))){$startInfo.ArgumentList.Add($arg)}
  $process=[Diagnostics.Process]::Start($startInfo);$memory=[IO.MemoryStream]::new()
  try{$process.StandardOutput.BaseStream.CopyTo($memory);$process.WaitForExit();if($process.ExitCode -ne 0 -or $memory.Length -ge 1048576){throw 'sparse-source-blob'};[IO.File]::WriteAllBytes($path,$memory.ToArray())}finally{$memory.Dispose();$process.Dispose()}
 }
 $b=Binding $path;$b.relative_path=[IO.Path]::GetRelativePath($repo,$path).Replace('\','/')
 if($b.relative_path -cne $relative){$b.git_relative_path=$relative;$b.snapshot_kind='reserved-Win32-name-sparse-Git-blob'}
 $b
})
$sourcePath=Join-Path $campaignRoot 'source-manifest.json';NewJson $sourcePath @{source_commit=$commit;dirty=$false;repo_root=$repo;files=$sourceFiles}
$environment=Join-Path $campaignRoot 'environment.json'
$background=@(Get-Process|Sort-Object Id|ForEach-Object {@{pid=$_.Id;name=$_.ProcessName;cpu_s=$_.CPU;working_set=$_.WorkingSet64}})
NewJson $environment @{platform='windows-x64';time_utc=[DateTime]::UtcNow.ToString('o');os=[Environment]::OSVersion.VersionString;
 processor=@(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);memory_bytes=(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory;
 background_processes=$background;source_commit=$commit;source_dirty=$false;configuration='Release';build_binding=(Binding $buildSeal);gate=(Binding $gate);tool_snapshot=(Binding $tool)}
$corpus=Join-Path $campaignRoot 'corpus.json'
$corpusExe=Join-Path $repo 'out/windows-qualification-release-reader03/pas_cost_corpus.exe'
$corpusStage='corpus-prelive-'+$Attempt
Stage $corpusStage (Join-Path $repo 'out/readiness-20261007-01/functional-gui-stage.ps1') @{StageName=$corpusStage;Executable=$corpusExe;GateReceipt=$gate;
 Inputs=@($PSCommandPath,$recipe,$buildSeal,$corpusExe)+$sourceFiles.path+(GateInputs)}
$trace=Get-Content -LiteralPath (Join-Path $out ($corpusStage+'/'+$corpusStage+'.stdout.log')) -Raw
$start=$trace.IndexOf('{');$end=$trace.LastIndexOf('}')
if($start -lt 0 -or $end -lt $start){throw 'corpus-json-output'}
$cj=$trace.Substring($start,$end-$start+1)|ConvertFrom-Json
if($cj.schema -cne 'pas.windows-cost-corpus.v1' -or $cj.rows.Count -ne 267 -or $cj.device_endpoints -ne 0){throw 'fixed-corpus-required'}
NewJson $corpus $cj
$bindings=@();foreach($pair in @(@('candidate_binary',(Join-Path $root 'current_chain.exe')),@('analyzer_binary',(Join-Path $root 'cost_analysis.exe')),
 @('auditor_binary',(Join-Path $root 'prelive_audit.exe')),@('profile',(Join-Path $repo 'configs/phigros-hd-assist-five-lead35.json')),
 @('cost_protocol',(Join-Path $repo 'docs/research/zero-miss-20261006/windows/PRELIVE_CURRENT_COST_PROTOCOL.md')),
 @('environment',$environment),@('source_manifest',$sourcePath),@('corpus',$corpus),@('build_binding',$buildSeal),@('prerequisites',$Prerequisites))){$b=Binding $pair[1];$b.role=$pair[0];$bindings+=@($b)}
$plans=@();foreach($scene in @('tap','hold-closed-reset-v2','dense')){for($i=1;$i -le 4;$i++){$id=$Attempt+'-'+$(if($scene -eq 'hold-closed-reset-v2'){'hold'}else{$scene})+$i;
 $plans+=@(@{run_id=$id;scene=$scene;mode='A';report_path=(Join-Path $pkg ('pipeline-release-'+$id+'.json'))})}}
$Campaign=Join-Path $campaignRoot 'campaign.json';NewJson $Campaign @{schema='pas.current-rails-campaign-freeze.v2';configuration='Release';platform='windows-x64';
 id=('windows-aa-'+$Attempt);source_commit=$commit;scenes=@('tap','hold-closed-reset-v2','dense');bindings=$bindings;planned_runs=$plans;
 requested=2560;warmup=256;measurement=2304;cadence_ms=20;protocol_modified=$false;live_ready=$false;device_operations=0}
$inputs=@($PSCommandPath,$recipe,$buildSeal,$Prerequisites,$Campaign)+$bindings.path+$sourceFiles.path+$seal.files.path+$seal.source_files.path+$seal.input_closure_receipts.path+(GateInputs)
$runs=@();$statuses=@();$stopped=$false;$reason=''
try {
 for($i=0;$i -lt $plans.Count;$i++){
  $plan=$plans[$i];$id=$plan.run_id;$stage='pipeline-prelive-release-'+$id
  & $pwsh -NoProfile -File $recipe -Operation pipeline -Mode release -BuildAttempt $BuildAttempt -Attempt $id -Variant A -Scene $plan.scene -Frames 2560 -Campaign $Campaign -CampaignRunId $id -GateReceipt $gate -ToolSnapshot $tool
  if($LASTEXITCODE -ne 0){throw ('qualified-direct-producer-STOP '+$id)}
  $audit=Join-Path $campaignRoot ($id+'-audit.json');Native ('audit-prelive-release-'+$id) (Join-Path $root 'prelive_audit.exe') @($plan.report_path,$audit) @($inputs+$plan.report_path+($plan.report_path+'.attempts.json')+($plan.report_path+'.journal.jsonl')) 900
  $entry=Binding $plan.report_path;$entry.run_id=$id;$entry.scene=$plan.scene;$entry.audit_path=$audit;$entry.audit_sha256=(Binding $audit).sha256;$process=@{}
  foreach($pair in @(@('specification',(Join-Path $pkg ($stage+'-direct-spec.json'))),@('input_closure',(Join-Path $pkg ($stage+'-input-closure-verification.json'))),
    @('result',(Join-Path $out ($stage+'/'+$stage+'-result.json'))),@('verification',(Join-Path $out ($stage+'/'+$stage+'-verification.json'))),
    @('freeze',(Join-Path $out ($stage+'/freeze.json'))),@('state',(Join-Path $out ($stage+'/state.json'))),@('contract',(Join-Path $out ($stage+'/contract.json'))),
    @('command',(Join-Path $out ($stage+'/'+$stage+'-command.json'))))){$process[$pair[0]]=Binding $pair[1]}
  $entry.process_evidence=$process;$runs+=@($entry)
  $input=Join-Path $campaignRoot ($id+'-gate-input.json');NewJson $input @{schema='pas.current-rails-cost-input.v2';campaign=(Binding $Campaign);plan_index=$i;run=$entry}
  $verdict=Join-Path $campaignRoot ($id+'-gate.json');Native ('gate-prelive-release-'+$id) (Join-Path $root 'cost_analysis.exe') @('run',$input,$verdict) @($inputs+$input+$audit+$plan.report_path+($plan.report_path+'.attempts.json')+($plan.report_path+'.journal.jsonl')+$process.Values.path) 900
  $gateValue=Get-Content -LiteralPath $verdict -Raw|ConvertFrom-Json;$statuses+=@(@{run_id=$id;status=$(if($gateValue.validity.pass){'PASS'}else{'NOT_READY'});verdict=(Binding $verdict)})
  if(-not $gateValue.validity.pass){throw ('necessary-cost-validity-failed '+$id)}
 }
}catch{$stopped=$true;$reason=$_.Exception.Message}
$remaining=@($plans|Where-Object {$_.run_id -notin $statuses.run_id}|ForEach-Object {@{run_id=$_.run_id;status=$(if(Test-Path -LiteralPath $_.report_path){'PRODUCED_BUT_UNQUALIFIED'}else{'NOT_RUN'})}})
NewJson (Join-Path $campaignRoot 'status.json') @{schema='pas.windows-aa-batch-status.v1';campaign=(Binding $Campaign);source_commit=$commit;
 normal_stopped=$stopped;reason=$reason;runs=$runs;statuses=$statuses;remaining=$remaining;all_12_cost_validity_pass=($statuses.Count -eq 12 -and -not $stopped);
 noise_qualified=$false;ABBA='NOT_RUN';production_packet_cost_qualified=$false;live_ready=$false;device_operations=0}
if($stopped){throw $reason}
$aaInput=Join-Path $campaignRoot 'aa-input.json';NewJson $aaInput @{schema='pas.current-rails-cost-input.v2';campaign=(Binding $Campaign);runs=$runs;normal_stopped=$false}
$noise=Join-Path $campaignRoot 'noise-freeze.json'
$runPaths=@($runs|ForEach-Object {$_.path;$_.audit_path;$_.path+'.attempts.json';$_.path+'.journal.jsonl';$_.process_evidence.Values.path})
Native ('noise-prelive-release-'+$Attempt) (Join-Path $root 'cost_analysis.exe') @('aa',$aaInput,$noise) @($inputs+$aaInput+$runPaths) 900
$noiseValue=Get-Content -LiteralPath $noise -Raw|ConvertFrom-Json
NewJson (Join-Path $campaignRoot 'noise-status.json') @{noise=(Binding $noise);adequate=[bool]$noiseValue.ready;comparison_allowed=[bool]$noiseValue.comparison_allowed;
 ABBA='NOT_RUN';live_ready=$false;device_operations=0}
if(-not $noiseValue.ready){throw 'AA-noise-inadequate-ABBA-not-permitted'}
