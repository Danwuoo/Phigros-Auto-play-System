param([Parameter(Mandatory)][string]$LaunchPlan,
      [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedPlanSha256)
$ErrorActionPreference='Stop'
function LRequire([bool]$ok,[string]$reason){if(-not $ok){throw ('scheduler launch: '+$reason)}}
function LRead([string]$path,[long]$cap=65536){LRequire ((Get-Item -LiteralPath $path).Length -le $cap) 'small metadata';Get-Content -LiteralPath $path -Raw|ConvertFrom-Json}
function LCheck($pin){LRequire ((Get-Item -LiteralPath $pin.path).Length -eq $pin.bytes -and (Get-FileHash -LiteralPath $pin.path -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $pin.sha256) 'pinned input'}
function LNew([string]$path,$value){$bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 12));LRequire ($bytes.Length -le 65536) 'output bound';$stream=[IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}}
$launchPin=@{path=[IO.Path]::GetFullPath($LaunchPlan);bytes=(Get-Item -LiteralPath $LaunchPlan).Length;sha256=$ExpectedPlanSha256};LCheck $launchPin
$p=LRead $launchPin.path
LRequire ($p.schema -ceq 'pas.cold-scheduler-launch-plan.v1' -and $p.device_operations -eq 0 -and $p.diagnostic_only -is [bool] -and $p.diagnostic_only -and $p.live_ready -is [bool] -and -not $p.live_ready) 'launch scope'
$repo=[IO.Path]::GetFullPath($p.workspace)
LRequire ([Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) 'actual elevated token required'
LRequire ((& git -C $repo rev-parse HEAD) -ceq $p.execution_source_commit -and (& git -C $repo branch --show-current) -ceq 'main' -and -not (& git -C $repo status --porcelain)) 'clean execution source'
foreach($pin in @($p.launcher,$p.controller,$p.component_build,$p.producer)){LCheck $pin}
LRequire ([string]::Equals([IO.Path]::GetFullPath($p.launcher.path),[IO.Path]::GetFullPath($PSCommandPath),[StringComparison]::OrdinalIgnoreCase)) 'literal launcher identity'
$root=[IO.Path]::GetFullPath($p.artifact_root)
LRequire ([IO.Path]::GetDirectoryName($root) -ceq (Join-Path $repo 'out') -and [IO.Path]::GetFileName($root) -cmatch '^prelive-[a-z0-9-]{1,80}$' -and (Test-Path -LiteralPath $root -PathType Container)) 'artifact containment'
$requestPath=Join-Path $root 'trace-request.json'
$frequency=[Diagnostics.Stopwatch]::Frequency;$start=[Diagnostics.Stopwatch]::GetTimestamp();$deadline=$start+75*$frequency
$request=@{schema='pas.cold-scheduler-trace-request.v1';workspace=$repo;artifact_root=$root;session_name=$p.session_name;session_guid=$p.session_guid;start_qpc=$start;deadline_qpc=$deadline;qpc_frequency=$frequency;duration_limit_s=75;buffer_limit_bytes=67108864;raw_limit_bytes=134217728;diagnostic_only=$true;cost_qualification=$false;live_ready=$false;device_operations=0;base_source_commit='12ec0550e882e3b06751d67fa67c43dda3e9312b';compiled_source_commit='5dce75d12c63ed51de5d996cedcd44e29d729841';execution_source_commit=$p.execution_source_commit;tool_source_commit=$p.execution_source_commit;component_build=$p.component_build;producer=$p.producer}
LNew $requestPath $request
$watch=$null;$control=$null;$phase='preflight';$reason=$null;$preflightExit=$null
try {
  & $p.controller.path --preflight $requestPath
  $preflightExit=$LASTEXITCODE
  LRequire ($preflightExit -eq 0) 'read-only session preflight STOP'
  $pre=LRead (Join-Path $root 'preflight.json')
  LRequire ($pre.query_clear -is [bool] -and $pre.query_clear -and $pre.in_outer_job -is [bool] -and -not $pre.in_outer_job) 'collision and upper Job preflight'
  $phase='watchdog'
  $watch=Start-Process -FilePath $p.controller.path -ArgumentList @('--watchdog',('"'+$requestPath+'"')) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root 'watchdog.stdout.log') -RedirectStandardError (Join-Path $root 'watchdog.stderr.log')
  while(-not(Test-Path -LiteralPath (Join-Path $root 'watchdog-ready.json')) -and [Diagnostics.Stopwatch]::GetTimestamp() -lt $deadline -and -not $watch.HasExited){[Threading.Thread]::Sleep(10)}
  LRequire ((Test-Path -LiteralPath (Join-Path $root 'watchdog-ready.json')) -and -not $watch.HasExited) 'independent watchdog ready'
  $phase='controller'
  $control=Start-Process -FilePath $p.controller.path -ArgumentList @('--controller',('"'+$requestPath+'"')) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root 'controller.stdout.log') -RedirectStandardError (Join-Path $root 'controller.stderr.log')
  while([Diagnostics.Stopwatch]::GetTimestamp() -lt $deadline -and (-not $watch.HasExited -or -not $control.HasExited)){[Threading.Thread]::Sleep(25)}
  # The independently held processes own their jobs and the same fixed deadline;
  # launcher termination never extends it or terminates both cleanup roles.
  if(-not $control.HasExited){$null=$control.WaitForExit(3000)}
  if(-not $watch.HasExited){$null=$watch.WaitForExit(3000)}
  $phase='complete'
} catch {$reason=$_.Exception.Message}
$value=@{schema='pas.cold-scheduler-elevated-launch-result.v1';launch_plan=$launchPin;request_path=$requestPath;execution_source_commit=$p.execution_source_commit;phase=$phase;reason=$reason;admin=$true;launcher_pid=$PID;start_qpc=$start;deadline_qpc=$deadline;qpc_frequency=$frequency;preflight_exit=$preflightExit;controller_pid=if($control){$control.Id}else{$null};watchdog_pid=if($watch){$watch.Id}else{$null};controller_exited=if($control){$control.HasExited}else{$null};watchdog_exited=if($watch){$watch.HasExited}else{$null};controller_exit=if($control -and $control.HasExited){$control.ExitCode}else{$null};watchdog_exit=if($watch -and $watch.HasExited){$watch.ExitCode}else{$null};recording_attempts=if($control){1}else{0};device_operations=0;qualification_issued=$false;live_ready=$false}
LNew (Join-Path $root 'elevated-launch-result.json') $value
if($reason){exit 2}
exit 0
