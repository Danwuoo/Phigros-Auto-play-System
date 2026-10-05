. "$PSScriptRoot/common.ps1"
Add-Type -Path "$Source/owned.cs"
$scratch=Join-Path $Evidence 'scratch-mock-v1';[IO.Directory]::CreateDirectory($scratch)|Out-Null
$rows=[Collections.Generic.List[object]]::new();$script:mockNativeCalls=0
function Reject($id,[scriptblock]$operation){$caught=$null;try{& $operation;$script:mockNativeCalls++}catch{$caught=$_.Exception.Message};$rows.Add(@{id=$id;expected='rejected-before-native';rejected=($null -ne $caught);reason=$caught;pass=($null -ne $caught)})}
function Check($id,$pass,$facts){$rows.Add(@{id=$id;pass=[bool]$pass;facts=$facts})}
$old4i=Join-Path $Campaign 'hold-ownership-x10d-o-bvi';$oldR1=Join-Path $Campaign 'hold-ownership-x10d-o-bvi-r1';$oldR2d=Join-Path $Campaign 'hold-ownership-x10d-o-bvi-r2d'
Reject P01 {ExactPath $old4i $Evidence}
Reject P02-R1 {ExactPath $oldR1 $Evidence};Reject P02-R2D {ExactPath $oldR2d $Evidence}
Reject P03 {ExactPath "$Repo/../Phigros-Auto-play-System/measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2e" $Evidence}
Reject P04 {ExactPath ($Evidence.Replace('Phigros-Auto-play-System','Phigros-Auto-play-System-other')) $Evidence}
Reject P05 {ExactPath $Evidence $Evidence {param($p)throw 'mock-native-attributes:reparse'}}
Reject P06-out {InitializeRoots {param($p)$p -eq $Out}}
$existing="$scratch/existing.json";NewJson $existing @{kept='original mock bytes'};$before=Entry $existing
Reject P06-reservation {$h=NewHandle $existing;$h.Dispose()};Check P06-preserved ((Sha $existing) -ceq $before.sha256) $before
Reject P07-create {$h=NewHandle "$scratch/no-parent/result.json";$h.Dispose()}
$closed=NewHandle "$scratch/closed.json";$closed.Dispose();Reject P07-flush {PutHandle $closed @{pending=$true}}
Reject P08 {ExactPath $old4i $Evidence}
$change="$scratch/source.txt";[IO.File]::WriteAllText($change,'before');$anchor=Entry $change;[IO.File]::WriteAllText($change,'after');Reject P09 {CheckEntry $anchor}
function State(){@{attempt=$Attempt;status='READY';next='natural';contract_sha='contract';freeze_sha='freeze';receipts=@()}}
$s=State;$s.receipts=@(@{path="$scratch/missing.json";bytes=1;sha256='x'});Reject S01 {CheckState $s natural contract freeze}
$facts=[R2EOwned+Result]::new();[R2EOwned]::Exited($facts,7);$facts.runner_exit=7;Reject S02 {ExpectedExit $facts 0 0}
$receipt="$scratch/verification.json";NewJson $receipt @{attempt=$Attempt;verification_exit=0;pending=$false;entries=@()};$e=Entry $receipt;[IO.File]::AppendAllText($receipt,' ');$s=State;$s.receipts=@($e);Reject S03 {CheckState $s natural contract freeze}
$s=State;$s.attempt='other';Reject S04 {CheckState $s natural contract freeze}
$s=State;$s.next='nonzero';Reject S05 {CheckState $s natural contract freeze}
$s=State;$s.status='STOP';NewJson "$scratch/stop-state.json" $s;Reject S06-local {CheckState (Json "$scratch/stop-state.json") nonzero contract freeze}
$s=State;$s.status='RUNNING';Reject S07 {CheckState $s nonzero contract freeze}
$r=[R2EOwned+Result]::new();Check E01 ($r.launch -ceq 'not_launched' -and $null -eq $r.exit_code) $r
[R2EOwned]::Created($r,1234);$r.resumed=$false;[R2EOwned]::Try($r,'mock-assign',[Action]{throw 'injected assign failure'});Check E02 ($r.launch -ceq 'launched' -and $r.pid -eq 1234 -and $r.resumed -eq $false -and $r.errors.Count -eq 1) $r
[R2EOwned]::Exited($r,7);[R2EOwned]::Try($r,'mock-exit-save',[Action]{PutHandle $closed @{result='already exited'}});Check E03 ($r.launch -ceq 'launched' -and $r.root_exited -eq $true -and $r.exit_code -eq 7 -and $r.errors.Count -eq 2) $r
$script:cleanupSteps=0;foreach($op in @('Observe','Active','Terminate','WaitAll')){[R2EOwned]::Try($r,"mock-$op",[Action]{throw 'injected cleanup failure'});[R2EOwned]::Try($r,'next-cleanup',[Action]{$script:cleanupSteps++})};Check E04 ($r.errors.Count -eq 6 -and $script:cleanupSteps -eq 4) @{errors=$r.errors;later_operations=$script:cleanupSteps}
[R2EOwned]::Try($r,'mock-final-observe',[Action]{throw 'final observe failure'});[R2EOwned]::Try($r,'mock-close',[Action]{$script:cleanupSteps++});Check E05 ($null -eq $r.active_final -and $r.exit_code -eq 7 -and $script:cleanupSteps -eq 5) $r
$failed=@($rows|Where-Object pass -ne $true);$report=@{schema=1;namespace='sandbox-mock-v1-no-launch-authority';rows=$rows;failed=$failed.Count;mock_native_calls=$script:mockNativeCalls;actual_CreateProcess_calls=0;old_root_writes=0;child_identity_verified=$false;used_common_production_helpers=@('ExactPath','InitializeRoots','NewHandle','PutHandle','CheckEntry','CheckState','ExpectedExit','R2EOwned.Created','R2EOwned.Exited','R2EOwned.Try');limits='mock fault helpers only; no real API fault/child cleanup acceptance'}
NewJson "$Evidence/mock-v1.json" $report
if($failed.Count -or $script:mockNativeCalls){throw 'mock-oracle-failure'}
@{mock_rows=$rows.Count;failed=0;actual_CreateProcess=0;cross_shell_S06_pending=$true}|ConvertTo-Json -Compress
