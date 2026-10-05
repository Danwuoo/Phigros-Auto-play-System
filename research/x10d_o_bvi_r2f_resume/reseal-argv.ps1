# One preformal author correction; originals preserved in failures/preformal-empty-argv.
. "$PSScriptRoot/common.ps1"
. "$PSScriptRoot/transaction.ps1"
$state=Json "$Evidence/state.json"
if($state.status -cne 'READY' -or $state.next -cne 'natural' -or $state.native_stages_consumed -ne 0 -or (Test-Path "$Evidence/natural-command.json") -or (Test-Path "$Evidence/argv-readback.json")){throw 'preformal-reseal-once'}
$contract=Json "$Evidence/contract.json"
$fixtureArgs=@("$Campaign/hold-ownership-x10d-o-bvi/normalized-execution.json","$Campaign/hold-ownership-x10d-o-bvi/oracle.json","$Campaign/hold-ownership-x10d-o-bvi-r1/typed-r1.json","$Repo/research/x10d_o_bvi/supplemental.json","$Campaign/hold-ownership-x10d-o-bvi-r1/r1-cases.json","$Campaign/hold-ownership-x10d-o-bvi-r1/expected-coverage.json")
foreach($spec in $contract.stages){
 switch -Wildcard ($spec.name){
  'natural' {$spec.argv=@('-NoProfile','-Command','exit 0')}
  'nonzero' {$spec.argv=@('-NoProfile','-Command','exit 7')}
  'owned-child' {$spec.argv=@('-NoProfile','-File',(Join-Path $Source 'control-parent.ps1'),'-Binding',(Join-Path $Evidence 'child-binding.json'),'-Attempt',$Attempt)}
  'configure-*' {$spec.argv=@('/d','/s','/c',(Join-Path $Source "$($spec.name).cmd"))}
  'build-*' {$spec.argv=@('/d','/s','/c',(Join-Path $Source "$($spec.name).cmd"))}
  'wrong-contact-release' {$spec.argv=$fixtureArgs+@("$Evidence/$($spec.report)",'wrong-contact-only')}
  'suite-*' {$spec.argv=$fixtureArgs+@("$Evidence/$($spec.report)")}
  default {throw 'stage-allowlist'}
 }
 if($spec.argv.Count -lt 3){throw 'empty-argv'}
 if($spec.name -eq 'configure-asan'){
  $dlls=@($contract.dependencies|Where-Object{$_.path -like '*asan*dynamic*x86_64.dll'})
  $dllBytes=[long]0;foreach($e in $dlls){$dllBytes+=$e.bytes}
  $spec.reserve_out=4194304+$dllBytes;$spec.reserve_basis='CMake/probes4MiB plus exact installed copied ASan DLL bytes; bounded streams/receipts'
 }
}
function Rewrite($p,$v){$h=[IO.FileStream]::new($p,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::Read);try{PutHandle $h $v}finally{$h.Dispose()}}
Rewrite "$Evidence/contract.json" $contract
$sha=Sha "$Evidence/contract.json"
$runner=[IO.File]::ReadAllText("$Source/run.ps1").Replace($state.contract_sha,$sha)
[IO.File]::WriteAllText("$Source/run.ps1",$runner,[Text.UTF8Encoding]::new($false))
$freeze=Json "$Evidence/freeze.json"
$freeze.files=@(Get-ChildItem -LiteralPath $Source -File|Sort-Object Name|ForEach-Object{Entry $_.FullName});$freeze.contract_sha=$sha
Rewrite "$Evidence/freeze.json" $freeze
NewJson "$Evidence/argv-readback.json" @{schema=1;stages=$contract.stages;count=13;controls=3;product_commands=10;preformal_author_correction=$true;native_calls=0;pretest_shared_source_unchanged=$true;contract=(Entry "$Evidence/contract.json");freeze=(Entry "$Evidence/freeze.json")}
foreach($e in (Json "$Evidence/transaction-round-2.json").shared_source){CheckEntry $e}
$state.contract_sha=$sha;$state.freeze_sha=Sha "$Evidence/freeze.json";$state.revision++
DurableState $Evidence $state
@{resealed_before_first_native=$true;stages=$contract.stages.Count;argv_counts=@($contract.stages|ForEach-Object{$_.argv.Count});contract_sha=$sha;capacity=(Capacity)}|ConvertTo-Json -Depth 5 -Compress
