param([string]$Batch,[string]$Tool,[string]$NewOutput,[string]$RunPrefix='final')
$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$false
$x1Batch=(Resolve-Path $Batch).Path
$x1Tool=(Resolve-Path $Tool).Path
if(Test-Path -LiteralPath $NewOutput){throw 'Check output must be new'}
$x1Determinism=@()
foreach($x1Lineage in 'c36h','main50'){
  $x1Runs=@('on-1','on-2','off') | ForEach-Object {
    Get-Content -LiteralPath "$x1Batch/$x1Lineage-$RunPrefix-$_/summary.json" -Raw | ConvertFrom-Json
  }
  foreach($x1Run in $x1Runs){
    if(!$x1Run.success -or $x1Run.contacts_at_exit -ne 0 -or $x1Run.verified_pngs -ne 7722){throw 'Run failed or incomplete'}
    foreach($x1Key in 'semantic_sha256','binary_sha256','input_manifest_sha256','source_provenance_sha256','cadence','tie','receipt_policy'){
      if($x1Run.$x1Key -cne $x1Runs[0].$x1Key){throw "Determinism mismatch: $x1Lineage/$x1Key"}
    }
  }
  $x1Determinism+=@{lineage=$x1Lineage;runs=3;binary_sha256=$x1Runs[0].binary_sha256;semantic_sha256=$x1Runs[0].semantic_sha256;equal=$true}
}
$x1Negative=Join-Path $x1Batch "negative-$RunPrefix"
if(Test-Path -LiteralPath $x1Negative){throw 'Negative output must be new'}
New-Item -ItemType Directory -Path $x1Negative | Out-Null
$x1Bad=Get-Content -LiteralPath "$x1Batch/input-manifest.json" -Raw | ConvertFrom-Json
$x1Bad.index_sha256='0000000000000000000000000000000000000000000000000000000000000000'
[IO.File]::WriteAllText("$x1Negative/bad-sha.json",($x1Bad|ConvertTo-Json -Depth 20),[Text.UTF8Encoding]::new($false))
$x1Cases=@(
  @{name='bad-index-SHA';reason='input_manifest_SHA';arguments=@('contact',"$x1Negative/bad-sha.json","$x1Batch/source-v2/c36h-source-provenance.json","$x1Negative/rejected-sha",'on')}
  @{name='existing-output';reason='output_exists';arguments=@('contact',"$x1Batch/input-manifest.json","$x1Batch/source-v2/c36h-source-provenance.json","$x1Batch/c36h-final-on-1",'on')}
  @{name='outside-batch';reason='output_outside_batch';arguments=@('contact',"$x1Batch/input-manifest.json","$x1Batch/source-v2/c36h-source-provenance.json",(Join-Path $x1Negative '../../../rejected-outside'),'on')}
  @{name='mixed-policy-compare';reason='comparison_policy_mismatch';arguments=@('contact-compare',"$x1Batch/input-manifest.json","$x1Batch/c36h-final-on-1","$x1Batch/main50-final-all","$x1Negative/rejected-mixed.json")}
)
$x1Checks=@()
foreach($x1Case in $x1Cases){
  $x1Arguments=$x1Case.arguments;$x1Log="$x1Negative/$($x1Case.name).log"
  & $x1Tool @x1Arguments *> $x1Log
  $x1Exit=$LASTEXITCODE;$x1Reason=Get-Content -LiteralPath $x1Log -Raw
  if($x1Exit -eq 0 -or !$x1Reason.Contains($x1Case.reason)){throw "Expected rejection missing: $($x1Case.name)"}
  $x1Checks+=@{name=$x1Case.name;exit_code=$x1Exit;expected_rejection=$true;reason=$x1Reason.Trim();log_sha256=(Get-FileHash -LiteralPath $x1Log).Hash.ToLowerInvariant()}
}
$x1Report=@{determinism=$x1Determinism;negative_cli=@{n=4;expected_rejections=4;unexpected_success=0;checks=$x1Checks}}
[IO.File]::WriteAllText([IO.Path]::GetFullPath($NewOutput),($x1Report|ConvertTo-Json -Depth 20),[Text.UTF8Encoding]::new($false))
Write-Output 'Both lineages: repeat + trace-on/off equal. Four invalid requests rejected.'
