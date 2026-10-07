param([Parameter(Mandatory)][string]$FreshRoot)
$ErrorActionPreference='Stop'
$taskRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(Test-Path -LiteralPath $FreshRoot){throw 'fresh-guard-pretest'}
$testSource=Join-Path $FreshRoot 'source'
. (Join-Path $PSScriptRoot 'process.ps1') -TaskRepo $taskRepo -TaskAttempt 'guard-v3-20261007' -TaskEvidence $FreshRoot -TaskSource $testSource
[IO.Directory]::CreateDirectory($testSource)|Out-Null
$loader=". '$(Join-Path $PSScriptRoot 'process.ps1')' -TaskRepo '$taskRepo' -TaskAttempt '$Attempt' -TaskEvidence '$FreshRoot' -TaskSource '$testSource'"
[IO.File]::WriteAllText((Join-Path $testSource 'common.ps1'),$loader,[Text.UTF8Encoding]::new($false))
foreach($leaf in @('transaction.ps1','identity.ps1')){[IO.File]::WriteAllBytes((Join-Path $testSource $leaf),[IO.File]::ReadAllBytes((Join-Path $historical $leaf)))}
[IO.File]::WriteAllBytes((Join-Path $testSource 'owned.cs'),[IO.File]::ReadAllBytes($script:guardCore))
$original=Join-Path $historical 'control-pretest.ps1'
$pre=[IO.File]::ReadAllText($original).Replace('R2FOwned','PasOwnedV3')
# These are the original 39 identities/waits/shared-budget oracles. Only the
# maintenance type name changes; fake cases remain explicitly non-platform.
[IO.File]::WriteAllText((Join-Path $testSource 'control-pretest.ps1'),$pre,[Text.UTF8Encoding]::new($false))
NewJson (Join-Path $Evidence 'state.json') (NewLocalState)
NewJson (Join-Path $Evidence 'adapter.json') @{original=(Entry $original);derived=(Entry (Join-Path $testSource 'control-pretest.ps1'));change='R2FOwned type name to PasOwnedV3 only';device_operations=0}
& (Join-Path $testSource 'control-pretest.ps1')
exit $LASTEXITCODE
