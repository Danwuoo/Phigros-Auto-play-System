param([Parameter(Mandatory)][string]$FreshRoot)
$ErrorActionPreference='Stop'
$taskRepoRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$localFreshRoot=[IO.Path]::GetFullPath($FreshRoot)
if(Test-Path -LiteralPath $localFreshRoot){throw 'pretest-root-exists'}
if(-not $localFreshRoot.StartsWith((Join-Path $taskRepoRoot 'out/windows-handoff/'),[StringComparison]::OrdinalIgnoreCase)){throw 'pretest-root-containment'}
$localSource=Join-Path $localFreshRoot 'source'
. (Join-Path $PSScriptRoot 'process.ps1') -TaskRepo $taskRepoRoot -TaskAttempt 'windows-round4-20261006' -TaskEvidence $localFreshRoot -TaskSource $localSource
[IO.Directory]::CreateDirectory($localSource)|Out-Null
$original=Join-Path $Repo 'research/x10d_o_bvi_build'
foreach($leaf in @('common.ps1','transaction.ps1','identity.ps1','owned.cs')){
  [IO.File]::WriteAllBytes((Join-Path $Source $leaf),[IO.File]::ReadAllBytes((Join-Path $original $leaf)))
}
$diag=[IO.File]::ReadAllText((Join-Path $original 'diagnostic.ps1'))
$oldReadback='CheckState (Json "$Evidence/state.json")'
if(([regex]::Matches($diag,[regex]::Escape($oldReadback))).Count -ne 1){throw 'readback-adapter-source'}
$diag=$diag.Replace($oldReadback,"CheckState (Json (Join-Path `$tx.dir 'state.json'))")
$oldLogRead='$text=[IO.File]::ReadAllText((Join-Path $tx.dir "$($tx.stage).stdout.log"))'
if(([regex]::Matches($diag,[regex]::Escape($oldLogRead))).Count -ne 1){throw 'held-log-adapter-source'}
$heldLogRead=@'
$log=$tx.handles["$($tx.stage).stdout.log"]
 $savedPosition=$log.Position
 if($log.Length -gt 16777216){throw 'diagnostic-log-cap'}
 $log.Position=0
 $reader=[IO.StreamReader]::new($log,[Text.Encoding]::UTF8,$true,4096,$true)
 try{$text=$reader.ReadToEnd()}finally{$reader.Dispose();$log.Position=$savedPosition}
'@
$diag=$diag.Replace($oldLogRead,$heldLogRead)
[IO.File]::WriteAllText((Join-Path $Source 'diagnostic.ps1'),$diag,[Text.UTF8Encoding]::new($false))
. (Join-Path $Source 'diagnostic.ps1')
NewJson (Join-Path $Evidence 'state.json') (NewLocalState)
NewJson (Join-Path $Evidence 'wrapper-v0.json') @{files=@(@{entry=(Entry (Join-Path $Source 'diagnostic.ps1'))})}
$pre=[IO.File]::ReadAllText((Join-Path $original 'diagnostic-pretest.ps1'))
$pre=$pre.Replace('. "$PSScriptRoot/common.ps1"','').Replace('. "$Source/transaction.ps1"','').Replace('. "$Source/diagnostic.ps1"','')
$pre=$pre.Replace('$scratch="$Evidence/scratch-diagnostic"',"`$scratch=Join-Path `$Evidence 'scratch-diagnostic'")
$pre=$pre.Replace('$dir="$scratch/$($c.id)"','$dir=Join-Path $scratch $c.id')
# The original 20 cases, expected errors and fake facts stay literal and unchanged.
[IO.File]::WriteAllText((Join-Path $Source 'diagnostic-pretest.ps1'),$pre,[Text.UTF8Encoding]::new($false))
NewJson (Join-Path $Evidence 'adapter-manifest.json') @{schema='pas.local-process-adapter.v1';
  original=@(Entry (Join-Path $original 'diagnostic.ps1');Entry (Join-Path $original 'diagnostic-pretest.ps1'));
  derived=@(Entry (Join-Path $Source 'diagnostic.ps1');Entry (Join-Path $Source 'diagnostic-pretest.ps1'));
  changes=@('canonical Join-Path scratch/case roots','FinishDiagnostic reads transaction-local durable state','read flushed stdout through its retained owner handle; writer exclusion unchanged');
  unchanged_core=@(Entry (Join-Path $original 'owned.cs');Entry (Join-Path $original 'identity.ps1');Entry (Join-Path $original 'transaction.ps1'))}
& (Join-Path $Source 'diagnostic-pretest.ps1')
exit $LASTEXITCODE
