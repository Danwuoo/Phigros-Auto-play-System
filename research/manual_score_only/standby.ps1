param([ValidateRange(1,20)][int]$Rounds=20,[ValidateSet(35,40)][int]$LeadMs=35,[ValidateSet('baseline','lazy')][string]$Variant='baseline')
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
Push-Location -LiteralPath $repo
try {
 $executable=if($Variant -eq 'lazy'){'./out/manual-score-main80-lazy-20261013/pas.exe'}else{'./out/manual-score-main80-20rounds-20261012-r2/pas.exe'}
 & $executable manual-session `
   --config ("configs/phigros-hd-assist-five-lead"+$LeadMs+".json") `
   --capability measurements/game-semantics-20260927/touch-five/summary.json `
   --score-only --rounds $Rounds
 if($LASTEXITCODE -ne 0){throw 'manual score-only session failed'}
} finally {Pop-Location}
