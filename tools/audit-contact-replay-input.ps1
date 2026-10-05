param([string]$Manifest,[string]$NewOutput)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $NewOutput){throw 'Audit output must be new'}
$x1Manifest=Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json
$x1Repo=(Resolve-Path "$PSScriptRoot/..").Path
$x1FrozenRoot=Join-Path $x1Repo 'measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01'
$x1Freeze=Get-Content -LiteralPath (Join-Path $x1FrozenRoot 'candidate36h-tint1-freeze.json') -Raw | ConvertFrom-Json
$x1ProfilePath=Join-Path $x1FrozenRoot 'profile.json'
$x1Profile=Get-Content -LiteralPath $x1ProfilePath -Raw | ConvertFrom-Json
$x1Actual=Get-Content -LiteralPath $x1Manifest.profile -Raw | ConvertFrom-Json
$x1SourcePath=Join-Path $x1Manifest.session_root 'manifest.json'
$x1Source=Get-Content -LiteralPath $x1SourcePath -Raw | ConvertFrom-Json
function File-Sha([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Flatten-Config($Value,[string]$Path,$Map){
  if($null -eq $Value){$Map[$Path]=$null;return}
  if($Value -is [pscustomobject]){
    foreach($x1Entry in $Value.PSObject.Properties){Flatten-Config $x1Entry.Value "$Path/$($x1Entry.Name)" $Map}
  }elseif($Value -is [Array]){
    for($x1I=0;$x1I -lt $Value.Count;++$x1I){Flatten-Config $Value[$x1I] "$Path/$x1I" $Map}
  }else{$Map[$Path]=$Value}
}
function Config-Diff($A,$B){
  $x1Left=@{};$x1Right=@{};Flatten-Config $A '' $x1Left;Flatten-Config $B '' $x1Right
  @(@($x1Left.Keys)+@($x1Right.Keys) | Sort-Object -Unique | ForEach-Object {
    if(!$x1Left.ContainsKey($_) -or !$x1Right.ContainsKey($_) -or $x1Left[$_] -cne $x1Right[$_]){
      @{path=$_;left=$x1Left[$_];right=$x1Right[$_]}
    }
  })
}
$x1FrozenSha=File-Sha $x1ProfilePath
if($x1FrozenSha -ne $x1Freeze.profile_sha256){throw 'Frozen profile hash mismatch'}
if((File-Sha $x1Manifest.profile) -ne $x1Manifest.profile_sha256){throw 'Replay profile hash mismatch'}
$x1Differences=Config-Diff $x1Profile $x1Actual
$x1SourceDifferences=Config-Diff $x1Source.config $x1Actual
foreach($x1Difference in @($x1Differences)+@($x1SourceDifferences)){
  if($x1Difference.path -ne '/log_dir'){throw "Effective configuration differs: $($x1Difference.path)"}
}
if(!$x1Source.capability_preflight.fingerprint_matches -or $x1Source.capability_preflight.mismatches.Count){throw 'Recorded fingerprint mismatch'}
$x1Report=@{
  input_manifest_sha256=File-Sha $Manifest
  frozen_profile_path=$x1ProfilePath;frozen_profile_sha256=$x1FrozenSha
  replay_profile_sha256=File-Sha $x1Manifest.profile
  frozen_vs_replay_differences=$x1Differences
  original_vs_replay_differences=$x1SourceDifferences
  effective_policy_equal=$true
  source_session_manifest_sha256=File-Sha $x1SourcePath
  source_session_summary_sha256=File-Sha (Join-Path $x1Manifest.session_root 'summary.json')
  source_config_declared_sha256=$x1Source.config_sha256
  recorded_capability_preflight=$x1Source.capability_preflight
  fingerprint_check='historical recorded evidence only; no new device query'
  coordinate_domain='fake receipts use frame pixel coordinates; no Android transport or native remapping'
  effective_config=$x1Actual
  frozen_binary_sha256=File-Sha (Join-Path $x1FrozenRoot 'candidate36h-tint1-runtime/pas.exe')
}
if($x1Report.frozen_binary_sha256 -ne $x1Freeze.binary_sha256){throw 'Frozen runtime binary hash mismatch'}
[IO.File]::WriteAllText([IO.Path]::GetFullPath($NewOutput),($x1Report | ConvertTo-Json -Depth 30),[Text.UTF8Encoding]::new($false))
Write-Output 'Frozen/current/source effective policy equal; only log_dir differs'
