param([Parameter(Mandatory)][string]$Parameters)
$ErrorActionPreference='Stop'
$job=Get-Content -LiteralPath $Parameters -Raw|ConvertFrom-Json -AsHashtable
$callArgs=$job.arguments
& $job.script @callArgs
exit $LASTEXITCODE
