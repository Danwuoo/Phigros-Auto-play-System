param([ValidateSet('aa','independent','bindings')][string]$Operation='aa')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh='C:/Users/wurre/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/pwsh.exe'
$entry=Join-Path $PSScriptRoot 'prelive_current/run.ps1'
function Save($name,$value){$path=Join-Path $pkg $name;$s=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew);try{$b=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 32)+"`n");$s.Write($b);$s.Flush($true)}finally{$s.Dispose()}}
function Run($arguments){& $pwsh -NoProfile -File $entry @arguments;if($LASTEXITCODE -ne 0){throw ('qualified stage failed '+($arguments -join ' '))}}
if($Operation -eq 'aa'){
 $envManifest=@{schema='pas.current-rails-cost-environment.v1';utc=[DateTime]::UtcNow.ToString('o');head=(& git -C $repo rev-parse HEAD);dirty=@(& git -C $repo status --short);cpu=@(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed);os=(Get-CimInstance Win32_OperatingSystem|Select-Object Caption,Version,BuildNumber,TotalVisibleMemorySize);background=@(Get-Process|Group-Object ProcessName|Select-Object Name,Count);qpc_frequency=[Diagnostics.Stopwatch]::Frequency;corpus='C++ source stimulus() 12 RGB frames; fixed 2560/20ms; no PNG/song input';thermal_power_policy='existing host settings; no changes; turbo/thermal state unmeasured';roots=@{release='prelive-current-release-13';debug='prelive-current-debug-05';asan='prelive-current-asan-04'};protocol_sha=(Get-FileHash (Join-Path $repo 'docs/research/zero-miss-20261006/windows/PRELIVE_CURRENT_COST_PROTOCOL.md')).Hash.ToLowerInvariant();binaries=@(Get-ChildItem (Join-Path $repo 'out/prelive-current-release-13') -File|Where-Object Extension -in '.exe','.lib','.dll','.map'|ForEach-Object {@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}})}
 Save 'cost-environment-before.json' $envManifest
 $runs=@();$stop=$false;$remaining=@()
 foreach($scene in @('tap','hold','dense')){for($i=1;$i -le 4;$i++){$attempt='aa-'+$scene+'-'+$i;if($stop){$remaining+=@{attempt=$attempt;reason='previous necessary gate failed';status='not_run'};continue}
  Run @('-Operation','pipeline','-Mode','release','-BuildAttempt','13','-Attempt',$attempt,'-Variant','A','-Scene',$scene,'-Frames','2560')
  $path=Join-Path $pkg ('pipeline-release-'+$attempt+'.json');$runs+=@{scene=$scene;path=$path;sha256=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()}
  & $pwsh -NoProfile -File (Join-Path $PSScriptRoot 'prelive_audit/run.ps1') -Report $path -Attempt $attempt
  if($LASTEXITCODE -ne 0){throw 'AA integrity stage failed'}
  $integrity=Get-Content -LiteralPath (Join-Path $pkg ('audit-'+$attempt+'.json')) -Raw|ConvertFrom-Json
  if(-not $integrity.integrity -or -not $integrity.complete_prefix_evidence){$stop=$true}
  $input=Join-Path $pkg ($attempt+'-gate-input.json');Save ($attempt+'-gate-input.json') @{path=$path}
  Run @('-Operation','analysis','-Mode','release','-BuildAttempt','13','-Attempt',$attempt,'-Analysis','run','-AnalysisInput',$input)
  $gate=Get-Content -LiteralPath (Join-Path $pkg ('analysis-release-'+$attempt+'.json')) -Raw|ConvertFrom-Json
  if(-not $gate.validity.pass){$stop=$true}
 }}
 Save 'aa-input.json' @{runs=$runs;not_run=$remaining;normal_stopped=$stop}
 Run @('-Operation','analysis','-Mode','release','-BuildAttempt','13','-Attempt','aa-freeze','-Analysis','aa','-AnalysisInput',(Join-Path $pkg 'aa-input.json'))
 $noise=Get-Content -LiteralPath (Join-Path $pkg 'analysis-release-aa-freeze.json') -Raw|ConvertFrom-Json
 Save 'ab-disposition.json' @{comparison_allowed=($noise.comparison_allowed -and -not $stop);noise_path=(Join-Path $pkg 'analysis-release-aa-freeze.json');noise_sha=(Get-FileHash (Join-Path $pkg 'analysis-release-aa-freeze.json')).Hash.ToLowerInvariant();status=if($noise.comparison_allowed -and -not $stop){'qualified; comparison not yet run'}else{'not_run: frozen AA gate NOT_READY'};planned_runs=36;candidate_results_used_in_noise=0}
}elseif($Operation -eq 'independent'){
 foreach($mode in @('debug','asan')){$build=if($mode -eq 'debug'){'05'}else{'04'};foreach($scene in @('tap','hold')){Run @('-Operation','pipeline','-Mode',$mode,'-BuildAttempt',$build,'-Attempt',('load-'+$scene+'-01'),'-Variant','B','-Scene',$scene,'-Frames','1000')}}
 foreach($scene in @('slow','writer','rpc','fault')){Run @('-Operation','pipeline','-Mode','release','-BuildAttempt','13','-Attempt',('stress-'+$scene+'-01'),'-Variant','B','-Scene',$scene,'-Frames','1000')}
 foreach($scene in @('tap','dense')){Run @('-Operation','pipeline','-Mode','release','-BuildAttempt','13','-Attempt',('long-'+$scene+'-01'),'-Variant','B','-Scene',$scene,'-Frames','10000')}
}else{
 foreach($mode in @('release','debug','asan')){$b=if($mode -eq 'release'){'13'}elseif($mode -eq 'debug'){'05'}else{'04'};Run @('-Operation','bindings','-Mode',$mode,'-BuildAttempt',$b,'-Attempt','closure')}
}
