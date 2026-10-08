$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'));$sourceRepo=$repo
$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh=(Get-Command pwsh).Source;$Mode='release';$Operation='tests';$Campaign='';$CoreDonor=''
$script:inputClosures=@{};$script:maintenancePath=$PSCommandPath
$gate=Join-Path $out 'guard-main05-native-01/native-gate.json';$tool=Join-Path $out 'tool-main05-01/tool-snapshot.json'
$recipe=Join-Path $repo 'research/prelive_current/run.ps1';$tok=$null;$err=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tok,[ref]$err);if($err.Count){throw 'shared recipe parse'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage','Native')){
 $d=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name);if($d.Count -ne 1){throw 'shared function identity'};$t=$d[0].Extent.Text
 if($name -in @('Native','Stage')){$t=$t.Replace('Join-Path $out ''withdrawal-review-01/stage.ps1''','Join-Path $repo ''research/main_consolidation/stage.ps1''')}
 if($name -ceq 'Native'){$t=$t.Replace('$PSCommandPath','$script:maintenancePath').Replace('tools/zero_miss_windows/invoke.ps1','research/windows_process_guard_v3/invoke.ps1')}
 if($name -ceq 'GateInputs'){$t=$t.Replace('pas.windows-offline-process-gate.v1','pas.windows-offline-process-gate.v3')}
 . ([scriptblock]::Create($t))
}
function LoadBuilds([string]$attempt){
 $script:builds=@();$script:tests=@();$script:inputs=@($script:maintenancePath,$recipe,(Join-Path $PSScriptRoot 'common.ps1'))
 foreach($mode in @('release','debug','asan')){
  $path=Join-Path $repo ('out/prelive-pre-emulator-evaluation-'+$attempt+'-'+$mode+'/evaluation.json')
  $e=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json;if(-not $e.affected_cold_pass -or $e.tests.Count -ne 20 -or $e.device_operations -ne 0 -or $e.live_ready){throw 'three full cold configurations required'}
  CheckEntry $e.build;$b=Get-Content -LiteralPath $e.build.path -Raw|ConvertFrom-Json
  foreach($f in $b.files){CheckEntry $f}
  foreach($r in @($b.configure_receipt)+@($b.build_receipts)+@($e.tests.receipt)){CheckEntry $r;$v=Get-Content -LiteralPath $r.path -Raw|ConvertFrom-Json;Receipt $v.stage}
  foreach($t in $e.tests){CheckEntry $t.report}
  CheckEntry $b.source_manifest;$m=Get-Content -LiteralPath $b.source_manifest.path -Raw|ConvertFrom-Json
  foreach($f in $m.files){CheckEntry $f}
  $script:builds+=@($b);$script:tests+=@($e.tests);$script:inputs+=@($path,$e.build.path,$b.source_manifest.path,$b.core_build_plan.path)+$b.configure_receipt.path+$b.build_receipts.path+$e.tests.report.path+$e.tests.receipt.path
  if($mode -ceq 'release'){$script:inputs+=@($b.files.path)}
 }
 $script:commit=$builds[0].source_commit
 if(@($builds|Where-Object source_commit -cne $commit).Count -or (& git -C $repo rev-parse HEAD) -cne $commit -or (& git -C $repo status --porcelain)){throw 'same exact clean main source required'}
 $script:seal=@{files=@($builds|ForEach-Object {$_.files})}
 $g=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
 $script:inputs+=@(GateInputs)+@($g.guard_source.path,$g.process_source.path,$g.cached_identity_controls.path,$g.identity_wait_controls.path)
}
function Exe([string]$name,[string]$mode='release'){
 $b=@($builds|Where-Object configuration -ceq $mode)[0]
 $found=@($b.files|Where-Object {[IO.Path]::GetFileName($_.path) -ceq ($name+'.exe')})
 if($found.Count -ne 1){throw 'one sealed executable required'};return $found[0].path
}
function SourceManifest($path){
 $b=@($builds|Where-Object configuration -ceq 'release')[0];$m=Get-Content -LiteralPath $b.source_manifest.path -Raw|ConvertFrom-Json
 $files=@($m.files|ForEach-Object {CheckEntry $_;$row=Binding $_.path;$row.relative_path=[IO.Path]::GetRelativePath($repo,$_.path).Replace('\','/');$row})
 NewJson $path @{schema='pas.current-source-closure.v1';source_commit=$commit;repo_root=$repo;dirty=$false;files=$files;original_build_source_manifest=$b.source_manifest;device_operations=0;live_ready=$false}
 return $files.path
}
function ProcessEvidence($stage){
 $e=@{};foreach($p in @(@('specification',(Join-Path $pkg ($stage+'-direct-spec.json'))),@('input_closure',(Join-Path $pkg ($stage+'-input-closure-verification.json'))),@('result',(Join-Path $out ($stage+'/'+$stage+'-result.json'))),@('verification',(Join-Path $out ($stage+'/'+$stage+'-verification.json'))),@('freeze',(Join-Path $out ($stage+'/freeze.json'))),@('state',(Join-Path $out ($stage+'/state.json'))),@('contract',(Join-Path $out ($stage+'/contract.json'))),@('command',(Join-Path $out ($stage+'/'+$stage+'-command.json'))))){$e[$p[0]]=Binding $p[1]};return $e
}
