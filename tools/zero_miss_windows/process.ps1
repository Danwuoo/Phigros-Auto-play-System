param([Parameter(Mandatory)][string]$TaskRepo,
      [Parameter(Mandatory)][string]$TaskAttempt,
      [Parameter(Mandatory)][string]$TaskEvidence,
      [Parameter(Mandatory)][string]$TaskSource)

# Import the existing safety core; historical bindings and STOP are never used.
$taskBinding = @{ repo=[IO.Path]::GetFullPath($TaskRepo); attempt=$TaskAttempt;
  evidence=[IO.Path]::GetFullPath($TaskEvidence); source=[IO.Path]::GetFullPath($TaskSource) }
$historical = Join-Path $taskBinding.repo 'research/x10d_o_bvi_build'
. (Join-Path $historical 'common.ps1')
. (Join-Path $historical 'transaction.ps1')
. (Join-Path $historical 'identity.ps1')
$script:Repo=$taskBinding.repo
$script:Attempt=$taskBinding.attempt
$script:Evidence=$taskBinding.evidence
$script:Source=$taskBinding.source
$script:Out=Join-Path $Evidence 'product'
if (-not $Evidence.StartsWith((Join-Path $Repo 'out/windows-handoff/') ,[StringComparison]::OrdinalIgnoreCase)) { throw 'local-evidence-root' }
NoAlias $Evidence
NoAlias $Source

function NewLocalState($status='INIT_PENDING',$next=$null,$contractSha=$null,$freezeSha=$null) {
  @{schema=1;attempt=$Attempt;status=$status;next=$next;revision=0;
    contract_sha=$contractSha;freeze_sha=$freezeSha;running=$null;reason=$null;
    native_stages_consumed=0;native_launches_known=0;receipts=@();
    diagnostic_receipts=@();diagnostic_slots_consumed=0;wrapper_revision=0;
    wrapper_repairs=0;diagnostic_positive=$false;product_frozen=$false;
    product_contract_sha=$null}
}

function InvokeLocalOwnedStage {
  param([Parameter(Mandatory)][string]$StageRoot,
        [Parameter(Mandatory)][string]$Stage,
        [Parameter(Mandatory)][string]$Executable,
        [string[]]$Arguments=@(), [int]$NativeExit=0, [int]$RunnerExit=0,
        [ValidateRange(30,1800)][int]$TotalSeconds=120,
        [ValidateRange(4096,16777216)][int]$StreamCap=4194304,
        [string[]]$Inputs=@(), [string]$ChildBinding=$null)
  $StageRoot=[IO.Path]::GetFullPath($StageRoot)
  if(-not $StageRoot.StartsWith((Join-Path $Repo 'out/windows-handoff/'),[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $StageRoot)){throw 'fresh-stage-root'}
  NoAlias $StageRoot
  [IO.Directory]::CreateDirectory($StageRoot)|Out-Null
  $previousEvidence=$script:Evidence
  $script:Evidence=$StageRoot
  $tx=$null;$lock=$null
  try {
    $lock=[IO.FileStream]::new((Join-Path $StageRoot 'attempt.lock'),[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    $spec=[pscustomobject]@{name=$Stage;exe=[IO.Path]::GetFullPath($Executable);argv=$Arguments;
      native_exit=$NativeExit;runner_exit=$RunnerExit;total_s=$TotalSeconds;
      stream_cap=$StreamCap;report=$null;next=$null}
    if($ChildBinding){
      $binding=Json $ChildBinding
      $binding.child_identity=Join-Path $StageRoot 'child-identity.json'
      $binding.parent_identity=Join-Path $StageRoot 'parent-identity.json'
      $binding.parent_poll=Join-Path $StageRoot 'parent-poll.json'
      $boundPath=Join-Path $StageRoot 'child-binding.json'
      NewJson $boundPath $binding
      $spec.argv=@($Arguments|ForEach-Object {if($_ -ceq $ChildBinding){$boundPath}else{$_}})
      $Inputs+=@($ChildBinding,$boundPath)
    }
    $freezePaths=@((Join-Path $historical 'owned.cs'),(Join-Path $historical 'identity.ps1'),
      (Join-Path $historical 'transaction.ps1'),(Join-Path $historical 'common.ps1'),
      (Join-Path $historical 'base.ps1'),$PSCommandPath,$spec.exe)+$Inputs
    $entries=@($freezePaths|Sort-Object -Unique|ForEach-Object { Entry $_ })
    NewJson (Join-Path $StageRoot 'contract.json') @{schema='pas.local-owned-stage.v1';attempt=$Attempt;spec=$spec;cwd=$Repo;device_allowed=$false}
    NewJson (Join-Path $StageRoot 'freeze.json') @{files=$entries}
    $state=NewLocalState READY $Stage (Sha (Join-Path $StageRoot 'contract.json')) (Sha (Join-Path $StageRoot 'freeze.json'))
    NewJson (Join-Path $StageRoot 'state.json') $state
    CheckLaunchState $StageRoot $state $Stage $state.contract_sha $state.freeze_sha
    foreach($entry in $entries){CheckEntry $entry}
    if(-not ('R2FOwned' -as [type])){Add-Type -Path (Join-Path $historical 'owned.cs')}
    $quote=[R2FOwned].GetMethod('Quote',[Reflection.BindingFlags]'NonPublic,Static')
    $commandline=(@($quote.Invoke($null,@([string]$spec.exe)))+@($spec.argv|ForEach-Object {$quote.Invoke($null,@([string]$_))})) -join ' '
    $tx=NewTransaction $StageRoot $spec $state
    BeginTransaction $tx @{attempt=$Attempt;stage=$Stage;spec=$spec;cwd=$Repo;
      commandline_to_CreateProcess=$commandline;pending=$false;launch='not_launched'}
    $facts=[R2FOwned+Result]::new();$tx.facts=$facts
    $callback=[Action[string,R2FOwned+Result]]{param($phase,$result)CheckpointTransaction $tx $phase $result}
    [R2FOwned]::Run($facts,$spec.exe,[string[]]$spec.argv,$Repo,$tx.handles["$Stage.stdout.log"],$tx.handles["$Stage.stderr.log"],$TotalSeconds,$StreamCap,$callback)
    foreach($entry in $entries){CheckEntry $entry}
    $verification=FinishTransaction $tx $facts @{offline=$true;device_commands=0}
    @{stage=$Stage;native_exit=$facts.exit_code;runner_exit=$facts.runner_exit;
      verification_exit=$verification.verification_exit;active_final=$facts.active_final;
      held_all_signaled=$facts.held_all_signaled;streams_completed=$facts.streams_completed;
      elapsed_s=$facts.elapsed_s;receipt=(Entry (Join-Path $StageRoot "$Stage-verification.json"))}
  } catch {
    if($tx){StopTransaction $tx $_.Exception.Message}
    throw
  } finally {
    if($tx){CloseTransaction $tx};if($lock){$lock.Dispose()}
    $script:Evidence=$previousEvidence
  }
}
