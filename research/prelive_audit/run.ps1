param([Parameter(Mandatory)][string]$Report,
 [Parameter(Mandatory)][ValidatePattern('^[a-z0-9-]{1,24}$')][string]$Attempt,
 [Parameter(Mandatory)][ValidatePattern('^[a-z0-9-]{1,24}$')][string]$BuildAttempt,
 [ValidateSet('release','debug','asan')][string]$Mode='release',
 [Parameter(Mandatory)][string]$GateReceipt,
 [Parameter(Mandatory)][string]$ToolSnapshot,[int]$ExpectedNative=0)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out=Join-Path $repo 'out/windows-handoff'
$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh='C:/Users/wurre/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/pwsh.exe'
$gate=[IO.Path]::GetFullPath($GateReceipt)
$tool=[IO.Path]::GetFullPath($ToolSnapshot)
$Report=[IO.Path]::GetFullPath($Report)
$root=Join-Path $repo ('out/prelive-current-'+$Mode+'-'+$BuildAttempt)
# This auditor is rebuilt in the exact current candidate CMake tree. No old
# prelive-audit-release-01 executable or user-supplied arbitrary binary is used.
$audit=Join-Path $root 'prelive_audit.exe'
$buildStage='build-prelive-'+$Mode+'-'+$BuildAttempt
$buildSeal=Join-Path $pkg ('current-build-'+$Mode+'-'+$BuildAttempt+'.json')
function NewJson($path,$value,[int]$cap=67108864){$bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 40)+"`n");if($bytes.Length -ge $cap){throw 'metadata-byte-cap'};$s=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read);try{$s.Write($bytes);$s.Flush($true)}finally{$s.Dispose()}}
function CheckEntry($f){if((Get-Item -LiteralPath $f.path).Length -ne $f.bytes -or (Get-FileHash -LiteralPath $f.path).Hash.ToLowerInvariant() -cne $f.sha256){throw ('input-sha '+$f.path)}}
function Binding($path){$item=Get-Item -LiteralPath $path;@{path=$item.FullName;bytes=[long]$item.Length;sha256=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()}}
function Fresh($path){if(Test-Path -LiteralPath $path){throw ('fresh-output-required '+$path)}}
$script:inputClosures=@{}
function InputClosure($stage,$paths){
 $canonical=@($paths|ForEach-Object {[IO.Path]::GetFullPath($_)}|Sort-Object -Unique)
 if(-not $canonical.Count -or $canonical.Count -gt 8192){throw 'input-closure-file-count'}
 $entries=@($canonical|ForEach-Object {Binding $_});$leaves=@()
 $rootPath=Join-Path $pkg ($stage+'-input-closure.json');$verificationPath=Join-Path $pkg ($stage+'-input-closure-verification.json')
 Fresh $rootPath;Fresh $verificationPath
 for($offset=0;$offset -lt $entries.Count;$offset+=64){
  $index=[int]($offset/64);$last=[Math]::Min($entries.Count-1,$offset+63);$leafPath=Join-Path $pkg ($stage+'-input-leaf-'+$index.ToString('000')+'.json')
  $leaf=@{schema='pas.current-input-closure-leaf.v1';stage=$stage;index=$index;files=@($entries[$offset..$last])}
  if([Text.Encoding]::UTF8.GetByteCount(($leaf|ConvertTo-Json -Depth 10)+"`n") -gt 32768){throw 'input-closure-leaf-byte-cap'}
  NewJson $leafPath $leaf 32768;$leaves+=@(Binding $leafPath)
 }
 if($leaves.Count -gt 128){throw 'input-closure-leaf-count'}
 NewJson $rootPath @{schema='pas.current-input-closure.v1';stage=$stage;verification_output=$verificationPath;files_count=$entries.Count;leaf_count=$leaves.Count;leaves=$leaves} 1048576
 $script:inputClosures[$stage]=@{root=(Binding $rootPath);leaves=$leaves;files=$entries;verification_output=$verificationPath;before_verified=$false;after_verified=$false}
 return @($rootPath)+@($leaves.path)
}
function CheckClosure($stage,$phase){
 $closure=$script:inputClosures[$stage];if(-not $closure){throw 'input-closure-required'}
 CheckEntry $closure.root;foreach($f in @($closure.leaves)+@($closure.files)){CheckEntry $f}
 if($phase -eq 'before'){$closure.before_verified=$true}else{$closure.after_verified=$true}
}
function ClosureReceipt($stage,[bool]$pass,[string]$reason=''){
 $closure=$script:inputClosures[$stage];$statePath=Join-Path (Join-Path $out $stage) 'state.json';$verificationPath=Join-Path (Join-Path $out $stage) ($stage+'-verification.json')
 $state=$null;$verification=$null;if(Test-Path -LiteralPath $statePath){$state=Binding $statePath};if(Test-Path -LiteralPath $verificationPath){$verification=Binding $verificationPath}
 if($pass -and (-not $closure.before_verified -or -not $closure.after_verified -or -not $state -or -not $verification)){throw 'input-closure-pass-predicate'}
 $status=if($pass){'COMPLETE'}else{'STOP'}
 NewJson $closure.verification_output @{schema='pas.current-input-closure-verification.v1';stage=$stage;status=$status;pass=$pass;before_verified=$closure.before_verified;after_verified=$closure.after_verified;root=$closure.root;state=$state;verification=$verification;files_count=$closure.files.Count;leaf_count=$closure.leaves.Count;reason=$reason} 1048576
}
function Receipt($stage,[int]$expected=0){
 $dir=Join-Path $out $stage;$vp=Join-Path $dir ($stage+'-verification.json')
 $v=Get-Content -LiteralPath $vp -Raw|ConvertFrom-Json
 $r=Get-Content -LiteralPath (Join-Path $dir ($stage+'-result.json')) -Raw|ConvertFrom-Json
 $s=Get-Content -LiteralPath (Join-Path $dir 'state.json') -Raw|ConvertFrom-Json
 if($v.pending -or -not $v.producer_complete -or $v.native_exit -ne $expected -or $v.runner_exit -ne $expected -or $v.verification_exit -ne 0 -or $r.pending -or $r.facts.active_final -ne 0 -or -not $r.facts.held_all_signaled -or -not $r.facts.streams_completed -or -not $r.facts.identity_trusted -or -not $r.facts.natural_quiescence -or $s.status -cne 'COMPLETE' -or $s.running -or $s.freeze_sha -cne (Get-FileHash -LiteralPath (Join-Path $dir 'freeze.json')).Hash.ToLowerInvariant() -or $s.contract_sha -cne (Get-FileHash -LiteralPath (Join-Path $dir 'contract.json')).Hash.ToLowerInvariant()){throw ('receipt-not-verified '+$stage)}
 foreach($f in $v.entries){CheckEntry $f}
 $anchors=@($s.receipts|Where-Object path -eq $vp);if($anchors.Count -ne 1){throw 'state-receipt-link'};CheckEntry $anchors[0]
}
function Capacity($stage,$phase){
 $roots=@(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -Directory|Where-Object Name -like 'prelive*')+@(Get-ChildItem -LiteralPath $out -Directory|Where-Object Name -like '*prelive*')
 $rows=@($roots|ForEach-Object {$sum=(Get-ChildItem -LiteralPath $_.FullName -File -Recurse|Measure-Object Length -Sum).Sum;@{path=$_.FullName;bytes=[long]$sum}})
 $total=[long](($rows|Measure-Object bytes -Sum).Sum);$free=[long](Get-PSDrive C).Free;$reserve=if($phase -eq 'before'){67108864}else{0}
 $valid=$total+$reserve -le 12884901888 -and $free-$reserve -ge 21474836480
 NewJson (Join-Path $pkg ($stage+'-capacity-'+$phase+'.json')) @{schema='pas.prelive-capacity.v1';stage=$stage;phase=$phase;roots=$rows;bytes=$total;reserve=$reserve;free=$free;cap=12884901888;minimum_free=21474836480;metadata_seal_cap=67108864;pass=$valid}
 if(-not $valid){throw 'capacity-gate'}
}
if(-not(Test-Path (Join-Path $pkg 'initial-audit.json'))){throw 'initial-audit-required'}
Receipt $buildStage
$freeze=Join-Path $out ($buildStage+'/freeze.json')
$files=(Get-Content -LiteralPath $freeze -Raw|ConvertFrom-Json).files
foreach($f in $files){CheckEntry $f}
$seal=Get-Content -LiteralPath $buildSeal -Raw|ConvertFrom-Json
if($seal.schema -cne 'pas.current-windows-build-binding.v1' -or $seal.mode -cne $Mode -or $seal.build_root -cne $root -or $seal.build_stage -cne $buildStage -or $seal.gate.path -cne $gate -or $seal.tool_snapshot.path -cne $tool -or $audit -notin $seal.files.path){throw 'exact-current-build-binding'}
foreach($f in @($seal.files)+@($seal.build_receipts)+@($seal.source_files)+@($seal.migration_targets)+@($seal.input_closure_receipts)+@($seal.gate,$seal.tool_snapshot,$seal.source_migration)){CheckEntry $f}
foreach($receipt in $seal.input_closure_receipts){$cr=Get-Content -LiteralPath $receipt.path -Raw|ConvertFrom-Json;if($cr.schema -cne 'pas.current-input-closure-verification.v1' -or -not $cr.pass -or -not $cr.before_verified -or -not $cr.after_verified -or $cr.status -cne 'COMPLETE'){throw 'build-input-closure-not-verified'}}
$g=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
if($g.schema -cne 'pas.windows-offline-process-gate.v1' -or -not $g.wrapper_positive -or $g.native_controls_verified -ne 4 -or $g.file_only_failed -ne 0){throw 'native-process-gate'}
CheckEntry $g.diagnostic;$qualificationInputs=@($gate,$g.diagnostic.path,$tool)
foreach($control in $g.native_controls){CheckEntry $control.receipt;$v=Get-Content -LiteralPath $control.receipt.path -Raw|ConvertFrom-Json;$qualificationInputs+=@($control.receipt.path);foreach($e in $v.entries){CheckEntry $e;$qualificationInputs+=@($e.path)}}
$snapshot=Get-Content -LiteralPath $tool -Raw|ConvertFrom-Json
foreach($f in $snapshot.files){if((Get-FileHash -LiteralPath $f.copy).Hash.ToLowerInvariant() -cne $f.sha256){throw 'tool-snapshot-changed'};$qualificationInputs+=@($f.copy)}
$r=Get-Content -LiteralPath $Report -Raw|ConvertFrom-Json
$suffix=if($r.schema -eq 'pas.prelive-current-pixels.v1'){@('.events.jsonl','.rows.jsonl')}else{@('.journal.jsonl','.attempts.json')}
$name='audit-data-prelive-'+$Mode+'-'+$Attempt
$params=Join-Path $pkg ($name+'-params.json')
$spec=Join-Path $pkg ($name+'-direct-spec.json')
$output=Join-Path $pkg ('audit-'+$Mode+'-'+$Attempt+'.json')
foreach($path in @($params,$spec,$output)){if(Test-Path -LiteralPath $path){throw ('fresh-audit-output-required '+$path)}}
$inputs=@($Report,$audit,$freeze,$buildSeal,$PSCommandPath,$pwsh,(Join-Path $out 'withdrawal-review-01/stage.ps1'))+@($suffix|ForEach-Object {$Report+$_})+@($files.path)+@($seal.source_files.path)+@($seal.migration_targets.path)+@($seal.input_closure_receipts.path)+@($seal.files.path)+@($seal.build_receipts.path)+$qualificationInputs
$closureInputs=InputClosure $name @($inputs|Sort-Object -Unique)
# Keep the unchanged qualified core and all fresh receipt roots. The input root,
# leaves and exact auditor/spec are direct core members; source leaves are checked
# by this wrapper before and after and must have a separate closure PASS receipt.
NewJson $spec @{stage_root=(Join-Path $out $name);stage=$name;exe=$audit;argv=@($Report,$output);native_exit=$ExpectedNative;runner_exit=$ExpectedNative;total_s=300;stream_cap=4194304;inputs=@($closureInputs)+@($params,$PSCommandPath,$pwsh,$tool)}
$afterRecorded=$false
try{
 Capacity $name 'before'
 NewJson $params @{script=(Join-Path $repo 'tools/zero_miss_windows/invoke.ps1');arguments=@{Specification=$spec;GateReceipt=$gate}}
 CheckClosure $name 'before'
 & $pwsh -NoProfile -File (Join-Path $out 'withdrawal-review-01/stage.ps1') -Parameters $params
 if($LASTEXITCODE -ne 0){throw 'audit stage unverified'}
 Receipt $name $ExpectedNative
 CheckClosure $name 'after'
 Capacity $name 'after';$afterRecorded=$true
 ClosureReceipt $name $true
}catch{
 $reason=$_.Exception.Message
 if(-not $afterRecorded){try{Capacity $name 'after'}catch{}}
 ClosureReceipt $name $false $reason
 throw
}
