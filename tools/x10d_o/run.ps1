param([Parameter(Mandatory)][string]$Name,[Parameter(Mandatory)][string]$Exe,[string[]]$Arguments=@(),[int]$TimeoutSeconds=180,[switch]$AllowFailure)
$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/../..").Path
$campaignPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue"
$batchPath="$campaignPath/hold-ownership-x10d-o"
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($suffix,$value){$p="$batchPath/$Name-$suffix.json";if(Test-Path $p){throw 'receipt exists'};[IO.File]::WriteAllText($p,($value|ConvertTo-Json -Depth 20)+"`n",[Text.UTF8Encoding]::new($false))}
if($Name -notmatch '^[a-z0-9-]{1,60}$' -or $TimeoutSeconds -gt 300 -or $TimeoutSeconds -lt 1){throw 'command bound'}
if(@(Get-ChildItem $batchPath -Filter '*-command.json').Count -ge 24){throw 'attempt cap'}
$b=Bytes $batchPath;$o=Bytes "$repoPath/out/x10d-o";$c=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")
if($b -gt 234881024 -or $o -gt 1073741824 -or $c+33554432 -gt 8589934592 -or (Get-PSDrive C).Free -lt (268435456-$b)+(1073741824-$o)+5368709120){throw 'capacity'}
# Windows argv quoting; no shell or command-string evaluation.
function Quote([string]$s){if($s -notmatch '[\s"]' -and $s.Length){return $s};return '"'+[regex]::Replace([regex]::Replace($s,'(\\*)"','$1$1\"'),'(\\+)$','$1$1')+'"'}
Save 'command' @{exe=$Exe;arguments=$Arguments;timeout_s=$TimeoutSeconds;workers=2;stdout_limit=8388608;stderr_limit=8388608;root_descendant_quiescence_s=15;rgb_clip_root=$env:PAS_RGB_CLIP_ROOT;capacity_before=@{batch=$b;out=$o;aggregate=$c;free=(Get-PSDrive C).Free}}
$stopwatch=[Diagnostics.Stopwatch]::StartNew();$reason='';$desc=@{};$p=$null
try{
 $p=Start-Process -FilePath $Exe -ArgumentList (($Arguments|ForEach-Object {Quote $_}) -join ' ') -WorkingDirectory $repoPath -WindowStyle Hidden -RedirectStandardOutput "$batchPath/$Name.stdout.log" -RedirectStandardError "$batchPath/$Name.stderr.log" -PassThru
 while(!$p.HasExited){
  foreach($child in @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($p.Id)" -ErrorAction SilentlyContinue)){$desc[[int]$child.ProcessId]=[string]$child.CreationDate}
  foreach($id in @($desc.Keys)){foreach($child in @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$id" -ErrorAction SilentlyContinue)){$desc[[int]$child.ProcessId]=[string]$child.CreationDate}}
  if($desc.Count -gt 64){$reason='descendant_capacity';break}
  if($stopwatch.Elapsed.TotalSeconds -gt $TimeoutSeconds){$reason='timeout';break}
  foreach($log in @("$batchPath/$Name.stdout.log","$batchPath/$Name.stderr.log")){if((Get-Item $log -ErrorAction SilentlyContinue).Length -gt 8388608){$reason='log_capacity'}}
  if($reason){break};Start-Sleep -Milliseconds 250;$p.Refresh()
 }
 if($reason){throw $reason}
 $p.WaitForExit();$exitCode=$p.ExitCode
 $q=[Diagnostics.Stopwatch]::StartNew();do{
  $alive=@();foreach($id in @($desc.Keys)){$child=Get-CimInstance Win32_Process -Filter "ProcessId=$id" -ErrorAction SilentlyContinue;if($child -and [string]$child.CreationDate -eq $desc[$id]){$alive+=$id}}
  if(!$alive.Count){break};Start-Sleep -Milliseconds 250
 }while($q.Elapsed.TotalSeconds -lt 15)
 if($alive.Count){$reason='descendants_nonquiescent';throw $reason}
 foreach($log in @("$batchPath/$Name.stdout.log","$batchPath/$Name.stderr.log")){if((Get-Item $log).Length -gt 8388608 -or @(Get-Content $log).Count -gt 100000){$reason='log_capacity';throw $reason}}
}catch{
 $reason=$_.Exception.Message;$exitCode=-1
 foreach($id in @($desc.Keys)){$child=Get-CimInstance Win32_Process -Filter "ProcessId=$id" -ErrorAction SilentlyContinue;if($child -and [string]$child.CreationDate -eq $desc[$id]){Stop-Process -Id $id -Force -ErrorAction SilentlyContinue}}
 if($p -and !$p.HasExited){$p.Kill();$p.WaitForExit()}
}
Save 'exit' @{exit=$exitCode;failure=$reason;elapsed_s=$stopwatch.Elapsed.TotalSeconds;descendants=@($desc.Keys|ForEach-Object {@{pid=$_;creation=$desc[$_]}});root_pid=if($p){$p.Id}else{$null};stdout_sha256=(Get-FileHash "$batchPath/$Name.stdout.log").Hash.ToLowerInvariant();stderr_sha256=(Get-FileHash "$batchPath/$Name.stderr.log").Hash.ToLowerInvariant()}
Write-Output "$Name exit=$exitCode failure=$reason elapsed=$($stopwatch.Elapsed.TotalSeconds)"
if($exitCode -ne 0 -and !$AllowFailure){throw 'bounded command failed; evidence retained'}
