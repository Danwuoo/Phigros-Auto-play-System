param([Parameter(Mandatory)][string]$NoiseRepairBinding,
      [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedScriptSha256,
      [ValidatePattern('^[a-z0-9-]{1,32}$')][string]$Attempt='main69-e1',
      [ValidatePattern('^[a-z0-9-]{1,20}$')][string]$Check='pre-cost',
      [ValidateSet(0,2)][int]$Expected=2)
$ErrorActionPreference='Stop';$taskReaderScript=$PSCommandPath
. (Join-Path $PSScriptRoot '../common.ps1')
$script:maintenancePath=$taskReaderScript
$own=Binding $taskReaderScript;if($own.sha256 -cne $ExpectedScriptSha256){throw 'externally pinned readonly recipe changed'}
$deltaPin=Binding ([IO.Path]::GetFullPath($NoiseRepairBinding));CheckEntry $deltaPin
$delta=Get-Content -LiteralPath $deltaPin.path -Raw|ConvertFrom-Json
if($delta.schema -cne 'pas.cold-producer-eligibility-delta-build.v1' -or -not $delta.cold_harness_delta){throw 'explicit cold eligibility reader build required'}
$inventoryPin=Binding (Join-Path $repo 'out/prelive-pre-emulator-planning/main32-stage-inventory.json');CheckEntry $inventoryPin
$inventory=Get-Content -LiteralPath $inventoryPin.path -Raw|ConvertFrom-Json
$catalogue=@{}
function ReaderAddStage([string]$stage){
 if($catalogue.ContainsKey($stage)){return}
 if(Test-Path -LiteralPath (Join-Path $pkg ($stage+'-direct-spec.json'))){$catalogue[$stage]=ProcessEvidence $stage;return}
 $proof=@{specification=(Binding (Join-Path $out ($stage+'-inputs/spec.json')));input_closure=(Binding (Join-Path $pkg ($stage+'-input-closure-verification.json')))}
 foreach($row in @(@('result',($stage+'-result.json')),@('verification',($stage+'-verification.json')),@('freeze','freeze.json'),@('state','state.json'),@('contract','contract.json'),@('command',($stage+'-command.json')))){$proof[$row[0]]=Binding (Join-Path $out ($stage+'/'+$row[1]))}
 $catalogue[$stage]=$proof
}
function ReaderAddReceipt($pin){CheckEntry $pin;$v=Get-Content -LiteralPath $pin.path -Raw|ConvertFrom-Json;ReaderAddStage $v.stage}
function ReaderPinIf([string]$path){if(Test-Path -LiteralPath $path -PathType Leaf){return Binding $path};return $null}
$prefix=Get-Content -LiteralPath $inventory.prefix_receipt.path -Raw|ConvertFrom-Json
ReaderAddReceipt $prefix.producer_receipt;ReaderAddReceipt $prefix.auditor_receipt
$faults=Get-Content -LiteralPath $inventory.faults_status.path -Raw|ConvertFrom-Json
foreach($row in $faults.rows){ReaderAddReceipt $row.process.verification;ReaderAddStage ('audit-fault-pre-emulator-main32-'+$row.scenario)}
$production=Get-Content -LiteralPath $inventory.production.original_status.path -Raw|ConvertFrom-Json
foreach($row in $production.rows){ReaderAddReceipt $row.process.verification;ReaderAddStage ('audit-production-pre-emulator-main32-'+$row.id)}
foreach($proof in @($delta.configure_process,$delta.build_process,$delta.affinity_controls_process,$delta.reader_controls_process,$delta.verification_controls_process)+@($delta.score_control_processes|ForEach-Object {$_.Native8})){ReaderAddReceipt $proof.verification}
$aaRoot=Join-Path $repo ('out/prelive-pre-emulator-aa-'+$Attempt);$abbaRoot=Join-Path $repo ('out/prelive-pre-emulator-abba-'+$Attempt)
$aa=ReaderPinIf (Join-Path $aaRoot 'aa-input.json');$abba=ReaderPinIf (Join-Path $abbaRoot 'comparison-input.json')
foreach($pair in @(@($aa,'audit-pre-emulator-'),@($abba,'audit-abba-pre-emulator-'))){if($pair[0]){$input=Get-Content -LiteralPath $pair[0].path -Raw|ConvertFrom-Json;foreach($row in $input.runs){ReaderAddReceipt $row.process_evidence.verification;ReaderAddStage ($pair[1]+$row.run_id)}}}
$noise=ReaderPinIf (Join-Path $aaRoot 'noise-freeze.json');$comparison=ReaderPinIf (Join-Path $abbaRoot 'comparison.json')
$noiseProcess=$null;if($noise){$noiseProcess=ProcessEvidence ('noise-pre-emulator-'+$Attempt)}
$comparisonProcess=$null;if($comparison){$comparisonProcess=ProcessEvidence ('comparison-pre-emulator-'+$Attempt)}
$artifact=Join-Path $repo ('out/prelive-producer-eligibility-readonly-'+$Attempt+'-'+$Check);Fresh $artifact;$null=[IO.Directory]::CreateDirectory($artifact)
$inputPath=Join-Path $artifact 'input.json';$outputPath=Join-Path $artifact 'report.json'
NewJson $inputPath @{schema='pas.readonly-narrow-preflight-input.v1';device_operations=0;gate=(Binding $gate);original_inventory=$inventoryPin;narrow_build=$deltaPin;processes=@($catalogue.Values);aa_status=(ReaderPinIf (Join-Path $aaRoot 'status.json'));aa_input=$aa;noise=$noise;noise_process=$noiseProcess;abba_status=(ReaderPinIf (Join-Path $abbaRoot 'status.json'));abba_input=$abba;comparison=$comparison;comparison_process=$comparisonProcess}
# Bounded metadata projection: original closure leaves are terminal metadata;
# the C++ reader verifies each needed leaf and every directly read pin itself.
# Original raw, archives and long-path source copies are not re-imported here.
$paths=@{};$pending=[Collections.Generic.Queue[string]]::new()
function ReaderStagePath([string]$path){
 $full=[IO.Path]::GetFullPath($path);if($paths.ContainsKey($full) -or -not(Test-Path -LiteralPath $full -PathType Leaf)){return}
 $file=Get-Item -LiteralPath $full;if($file.Length -gt 268435456 -or $full.Length -gt 259){throw 'existing readonly native file/path bound'}
 $paths[$full]=$true
 if($file.Extension -ceq '.json' -and $file.Length -le 4194304 -and $file.Name -notlike '*.attempts.json' -and $file.Name -notlike '*-input-leaf-*.json' -and $file.Name -cne 'freeze.json' -and $file.Name -notlike '*-input-closure.json'){$pending.Enqueue($full)}
}
function ReaderMetadataPins($value){
 if($null -eq $value){return}
 if($value -is [Collections.IDictionary]){foreach($entry in $value.Values){ReaderMetadataPins $entry};return}
 if($value -is [Collections.IEnumerable] -and $value -isnot [string]){foreach($entry in $value){ReaderMetadataPins $entry};return}
 if($value -is [pscustomobject]){
  if($value.path -and $value.sha256 -and $null -ne $value.bytes -and [IO.Path]::GetExtension([string]$value.path) -ceq '.json' -and $value.path -notlike '*.attempts.json' -and [long]$value.bytes -le 4194304){ReaderStagePath $value.path}
  foreach($property in $value.PSObject.Properties){ReaderMetadataPins $property.Value}
 }
}
foreach($path in @($inputPath,$deltaPin.path,$own.path)){ReaderStagePath $path}
$old=Get-Content -LiteralPath $inventory.source_snapshot.path -Raw|ConvertFrom-Json;foreach($row in $old.files){ReaderStagePath $row.copy.path}
$short=Get-Content -LiteralPath $delta.base_source_projection.path -Raw|ConvertFrom-Json;foreach($row in $short.files){ReaderStagePath $row.copy.path}
$current=Get-Content -LiteralPath $delta.source_manifest.path -Raw|ConvertFrom-Json
foreach($pin in @($current.files)+@($delta.files)+@($delta.compiled_outputs)+@($delta.imported_static_libraries|ForEach-Object {$_.binary})+@($delta.base_generated_inputs)){ReaderStagePath $pin.path}
if($delta.frame_metadata_delta){ReaderStagePath $delta.frame_metadata_delta.base_chain_source.path}
while($pending.Count){$value=Get-Content -LiteralPath $pending.Dequeue() -Raw|ConvertFrom-Json;ReaderMetadataPins $value;if($paths.Count -gt 8192){throw 'existing readonly native count bound'}}
$guard=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$inputs=@($paths.Keys)+@($recipe,(Join-Path $repo 'research/pre_emulator/common.ps1'))+(GateInputs)+@($guard.guard_source.path,$guard.process_source.path,$guard.cached_identity_controls.path,$guard.identity_wait_controls.path)
$reader=@($delta.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq 'qualification_reader.exe'})
if($reader.Count -ne 1){throw 'one new actual readonly reader required'}
$seal=@{files=$delta.files};$Operation='tests';$commit=$delta.source_commit
Native ('readonly-eligibility-'+$Attempt+'-'+$Check) $reader[0].path @($repo,$inputPath,$outputPath) $inputs 900 $Expected
$result=Get-Content -LiteralPath $outputPath -Raw|ConvertFrom-Json
if($Expected -eq 2){foreach($section in @('base','prefix','faults','production','narrow_build')){if(-not $result.checks.$section.pass){throw ('necessary pre-cost readonly failed '+$section)}}}
elseif(-not $result.pass -or $result.status -cne 'COLD_BUNDLE_PASS'){throw 'full actual cold bundle not ready'}
$result|Select-Object status,pass,checks,issues,files,bytes,qualification_issued,live_ready,device_operations|ConvertTo-Json -Depth 12
