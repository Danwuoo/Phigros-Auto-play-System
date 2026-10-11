param([Parameter(Mandatory)][string]$Specification,
      [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$SpecificationSha256,
      [Parameter(Mandatory)][string]$GateReceipt,
      [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$GateReceiptSha256)
$ErrorActionPreference='Stop'
# Save the actual adapter before importing any functions with dynamic PS paths.
$taskCollectionAdapter=[IO.Path]::GetFullPath($PSCommandPath)
$taskCollectionWorkspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

function DciPath([string]$raw,[string]$workspace,[bool]$existing=$true) {
 if(-not $raw -or -not [IO.Path]::IsPathFullyQualified($raw) -or $raw -match '^\\\\|(^|[\\/])\.\.?([\\/]|$)|(^|[\\/])[^\\/]*[ .]([\\/]|$)' -or $raw.Substring(2).Contains(':')){throw 'collection absolute unaliased path required'}
 $path=[IO.Path]::GetFullPath($raw);$base=[IO.Path]::GetFullPath($workspace)
 if(-not $path.StartsWith($base+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'collection workspace containment'}
 $cursor=$path;while($cursor){if(Test-Path -LiteralPath $cursor){if((Get-Item -LiteralPath $cursor).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'collection reparse path'}};$parent=[IO.Path]::GetDirectoryName($cursor);if($parent -eq $cursor){break};$cursor=$parent}
 if($existing -and -not (Test-Path -LiteralPath $path -PathType Leaf)){throw 'collection input file missing'}
 return $path
}
function DciPin($pin,[string]$workspace,[long]$cap=67108864) {
 if(-not $pin -or $pin.path -isnot [string] -or ($pin.bytes -isnot [int] -and $pin.bytes -isnot [long]) -or $pin.bytes -lt 0 -or $pin.bytes -gt $cap -or $pin.sha256 -isnot [string] -or $pin.sha256 -cnotmatch '^[0-9a-f]{64}$'){throw 'collection complete bounded external pin required'}
 $path=DciPath $pin.path $workspace
 $file=Get-Item -LiteralPath $path
 if($file.Length -ne $pin.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pin.sha256){throw 'collection externally pinned bytes differ'}
 return $path
}
function DciJson($pin,[string]$workspace,[long]$cap=67108864) {
 $path=DciPin $pin $workspace $cap
 $value=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json -Depth 100
 if(-not $value -or $value -is [array] -or $value -isnot [pscustomobject]){throw 'collection JSON object required'}
 return $value
}
function DciSamePin($left,$right) {
 return $left -and $right -and [string]::Equals([IO.Path]::GetFullPath($left.path),[IO.Path]::GetFullPath($right.path),[StringComparison]::OrdinalIgnoreCase) -and $left.bytes -eq $right.bytes -and $left.sha256 -ceq $right.sha256
}
function DciExternal([string]$path,[string]$sha,[string]$workspace) {
 $path=DciPath $path $workspace
 return @{path=$path;bytes=[long](Get-Item -LiteralPath $path).Length;sha256=$sha}
}
function DciCurrentSource([string]$workspace) {
 $head=& git -C $workspace rev-parse HEAD
 if($LASTEXITCODE -ne 0 -or $head -cnotmatch '^[0-9a-f]{40}$'){throw 'collection current source identity'}
 $dirty=@(& git -C $workspace status --porcelain=v1 --untracked-files=all)
 if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'collection current clean source required'}
 return $head
}
function DciNativeMetadata($proof,[string]$workspace) {
 $paths=@()
 foreach($role in @('specification','input_closure','result','verification','freeze','state','contract','command')){$value=DciJson $proof.$role $workspace 1048576;$paths+=@(DciPin $proof.$role $workspace 1048576)
  if($role -ceq 'input_closure'){
   if($value.schema -cne 'pas.current-input-closure-verification.v1'){throw 'collection native input closure schema'}
   $root=DciJson $value.root $workspace 32768;$paths+=@(DciPin $value.root $workspace 32768)
   if($root.schema -cne 'pas.current-input-closure.v1' -or @($root.leaves).Count -lt 1 -or @($root.leaves).Count -gt 128){throw 'collection native root metadata bound'}
   foreach($leaf in $root.leaves){$leafValue=DciJson $leaf $workspace 32768;if($leafValue.schema -cne 'pas.current-input-closure-leaf.v1'){throw 'collection native leaf metadata schema'};$paths+=@(DciPin $leaf $workspace 32768)}
  }
 }
 # Original evidence file pins inside leaves are interpreted by C++ and are
 # not walked/rehashed here. Only the exact native8/root/leaf metadata is frozen.
 return $paths
}
function DciPlan($request,[string]$workspace) {
 $fields=@('schema','command','stage','stage_root','exe','input','workspace','config','capability','output','game_package','total_s','stream_cap')
 if($request.schema -cne 'pas.current-device-collection-native-request.v1' -or @($request.PSObject.Properties).Count -ne $fields.Count -or @($request.PSObject.Properties|Where-Object {$_.Name -cnotin $fields}).Count){throw 'collection exact request schema'}
 if($request.command -cnotin @('candidate-collect-device','candidate-collect-capture-rpc','candidate-collect-physical')){throw 'collection command allowlist'}
 if($request.stage -isnot [string] -or $request.stage -cnotmatch '^candidate-collection-(?:device|capture-rpc|physical)-[a-z0-9-]{1,32}$' -or -not $request.stage.StartsWith($request.command.Replace('candidate-collect-','candidate-collection-')+'-',[StringComparison]::Ordinal)){throw 'collection stage/command join'}
 if(-not [string]::Equals([IO.Path]::GetFullPath($request.workspace),$workspace,[StringComparison]::OrdinalIgnoreCase)){throw 'collection exact workspace'}
 $stageRoot=DciPath $request.stage_root $workspace $false
 if(-not [string]::Equals($stageRoot,(Join-Path $workspace ('out/windows-handoff/'+$request.stage)),[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $stageRoot)){throw 'collection fresh stage root'}
 if(($request.total_s -isnot [int] -and $request.total_s -isnot [long]) -or $request.total_s -lt 30 -or $request.total_s -gt 1800 -or ($request.stream_cap -isnot [int] -and $request.stream_cap -isnot [long]) -or $request.stream_cap -lt 4096 -or $request.stream_cap -gt 16777216){throw 'collection original native time/stream bounds'}
 if($request.game_package -isnot [string] -or $request.game_package -cnotmatch '^[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+$'){throw 'collection game package required'}
 $collectionInput=DciJson $request.input $workspace
 if($collectionInput.schema -cne 'pas.current-candidate-collection-input.v1' -or $collectionInput.evidence_kind -cne 'actual' -or $collectionInput.source_commit -cnotmatch '^[0-9a-f]{40}$' -or $collectionInput.source_commit -cne (DciCurrentSource $workspace)){throw 'collection actual current source input required'}
 if(($collectionInput.PSObject.Properties.Name -ccontains 'game_package') -and $collectionInput.game_package -cne $request.game_package){throw 'collection input/CLI game package differs'}
 if($request.command -ceq 'candidate-collect-capture-rpc' -and -not ($collectionInput.PSObject.Properties.Name -ccontains 'device')){throw 'collection capture requires current device receipt'}
 if($request.command -ceq 'candidate-collect-physical' -and (-not ($collectionInput.PSObject.Properties.Name -ccontains 'device') -or -not ($collectionInput.PSObject.Properties.Name -ccontains 'capture_rpc'))){throw 'collection physical requires current device/capture receipts'}
 $roles=@('candidate_binary','source_manifest','production_build','cold_preflight_input','runtime_profile','capability','production_cost','authorization');$documents=@{};$paths=@(DciPin $request.input $workspace)
 foreach($role in $roles){if($role -ceq 'candidate_binary'){$paths+=@(DciPin $collectionInput.$role $workspace 268435456)}else{$documents[$role]=DciJson $collectionInput.$role $workspace;$paths+=@(DciPin $collectionInput.$role $workspace)}}
 $exe=DciPath $request.exe $workspace
 if([IO.Path]::GetFileName($exe) -cne 'pas.exe' -or -not [string]::Equals($exe,[IO.Path]::GetFullPath($collectionInput.candidate_binary.path),[StringComparison]::OrdinalIgnoreCase)){throw 'collection direct current production pas.exe required'}
 $config=DciPin $request.config $workspace 4194304;$capability=DciPin $request.capability $workspace 4194304
 if(-not (DciSamePin $request.config $collectionInput.runtime_profile) -or -not (DciSamePin $request.capability $collectionInput.capability)){throw 'collection config/capability exact pin joins'}
 $source=$documents.source_manifest;$build=$documents.production_build
 if($source.schema -cne 'pas.cooperative-source.v1' -or $source.source_commit -cne $collectionInput.source_commit -or $source.dirty -isnot [bool] -or $source.dirty -or @($source.files).Count -lt 1 -or @($source.files).Count -gt 8192){throw 'collection clean current complete source manifest'}
 foreach($pin in $source.files){$paths+=@(DciPin $pin $workspace)}
 if($build.schema -cne 'pas.cooperative-build.v1' -or $build.source_commit -cne $collectionInput.source_commit -or $build.configuration -cne 'release' -or -not (DciSamePin $build.source_manifest $collectionInput.source_manifest) -or @($build.files).Count -lt 1 -or @($build.files).Count -gt 8192){throw 'collection current Release production build join'}
 $binaryRows=@($build.files|Where-Object {DciSamePin $_ $collectionInput.candidate_binary})
 if($binaryRows.Count -ne 1){throw 'collection binary must be exact member of current production build'}
 foreach($pin in $build.files){$paths+=@(DciPin $pin $workspace 268435456)}
 $core=DciJson $build.core_build_plan $workspace;$paths+=@(DciPin $build.core_build_plan $workspace)
 if($core.schema -cne 'pas.fresh-core-build-plan.v1' -or $core.source_commit -cne $collectionInput.source_commit -or $core.configuration -cne 'release' -or $core.translation_units -ne 22 -or $core.old_donor_objects_used -isnot [bool] -or $core.old_donor_objects_used){throw 'collection current fresh22 production core required'}
 $production=$documents.production_cost
 if($production.schema -cne 'pas.current-production-chain-qualification.v1' -or $production.evidence_kind -cne 'actual' -or $production.source_commit -cne $collectionInput.source_commit -or $production.pass -isnot [bool] -or -not $production.pass -or -not (DciSamePin $production.cold_preflight_input $collectionInput.cold_preflight_input)){throw 'collection original current production prerequisites NOT_READY'}
 if($collectionInput.native_processes -isnot [array] -or @($collectionInput.native_processes).Count -lt 1 -or @($collectionInput.native_processes).Count -gt 128){throw 'collection production native catalogue required'}
 foreach($proof in $collectionInput.native_processes){$paths+=@(DciNativeMetadata $proof $workspace)}
 foreach($role in @('device','capture_rpc')){if($collectionInput.PSObject.Properties.Name -ccontains $role){$receipt=DciJson $collectionInput.$role $workspace;$paths+=@(DciPin $collectionInput.$role $workspace)
   if($receipt.evidence_kind -cne 'actual' -or $receipt.source_commit -cne $collectionInput.source_commit -or $receipt.candidate_binary_sha256 -cne $collectionInput.candidate_binary.sha256 -or $receipt.profile_sha256 -cne $collectionInput.runtime_profile.sha256 -or $receipt.capability_sha256 -cne $collectionInput.capability.sha256){throw 'collection prior device/capture current identity join'}
   $paths+=@(DciNativeMetadata $collectionInput.qualification_processes.$role $workspace)
   if($role -ceq 'device' -and $request.game_package -cne $receipt.game_package){throw 'collection current game package differs'}
  }}
 $owner=$collectionInput.PSObject.Properties.Name -ccontains 'physical_owner_observation';$review=$collectionInput.PSObject.Properties.Name -ccontains 'physical_review';$ownerProof=$collectionInput.qualification_processes.PSObject.Properties.Name -ccontains 'physical_owner'
 if($owner -ne $review -or $owner -ne $ownerProof){throw 'collection physical owner/review/native8 complete triple required'}
 if($owner){if($request.command -cne 'candidate-collect-physical' -or -not ($collectionInput.PSObject.Properties.Name -ccontains 'device') -or -not ($collectionInput.PSObject.Properties.Name -ccontains 'capture_rpc')){throw 'collection physical finalize current device/capture required'}
  foreach($role in @('physical_owner_observation','physical_review')){$null=DciJson $collectionInput.$role $workspace;$paths+=@(DciPin $collectionInput.$role $workspace)};$paths+=@(DciNativeMetadata $collectionInput.qualification_processes.physical_owner $workspace)
 }
 $output=DciPath $request.output $workspace $false
 $outputRelative=[IO.Path]::GetRelativePath($workspace,$output).Replace('\','/')
 if($outputRelative -cnotmatch '^out/(?:prelive[^/]*/|windows-handoff/)'){throw 'collection output must remain in existing retained capacity roots'}
 if([string]::Equals($output,$stageRoot,[StringComparison]::OrdinalIgnoreCase) -or $output.StartsWith($stageRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'collection report parent cannot precreate the fresh native stage'}
 $packageRoot=Join-Path $workspace 'out/prelive-20261006'
 if([string]::Equals([IO.Path]::GetDirectoryName($output),$packageRoot,[StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetFileName($output) -match ('^'+[regex]::Escape($request.stage)+'-(?:direct-spec|input-closure(?:-verification)?|input-leaf-[0-9]{3}|capacity-(?:before|after))[.]json$')){throw 'collection report cannot overwrite reserved native metadata'}
 if(Test-Path -LiteralPath $output){throw 'collection fresh report required'}
 $arguments=@($request.command,'--admission-input',$request.input.path,'--admission-sha256',$request.input.sha256,'--workspace',$workspace,'--config',$config,'--capability',$capability,'--output',$output,'--game-package',$request.game_package)
 return @{stage=$request.stage;stage_root=$stageRoot;exe=$exe;argv=$arguments;native_exit=0;runner_exit=0;total_s=[int]$request.total_s;stream_cap=[int]$request.stream_cap;inputs=@($paths|Sort-Object -Unique);output=$output;source_commit=$collectionInput.source_commit}
}
function DciAdaptOwned([string]$processSource) {
 $tokens=$null;$errors=$null;$tree=[Management.Automation.Language.Parser]::ParseFile($processSource,[ref]$tokens,[ref]$errors)
 if($errors.Count){throw 'collection original owned process parse'}
 $definitions=@($tree.FindAll({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq 'InvokeLocalOwnedStage')
 if($definitions.Count -ne 1){throw 'collection original owned function identity'}
 $original=$definitions[0].Extent.Text
 foreach($literal in @('device_allowed=$false','@{offline=$true;device_commands=0}','$PSCommandPath')){if(([regex]::Matches($original,[regex]::Escape($literal))).Count -ne 1){throw 'collection exact two scope policies/adapter freeze literal'}}
 $adapted=$original.Replace('device_allowed=$false','device_allowed=$true').Replace('@{offline=$true;device_commands=0}',"@{offline=`$false;device_scope='actual_current_device_collection';device_commands='not_counted'}").Replace('$PSCommandPath','$taskCollectionAdapter')
 $null=[Management.Automation.Language.Parser]::ParseInput($adapted,[ref]$tokens,[ref]$errors)
 if($errors.Count){throw 'collection adapted owned function parse'}
 return $adapted
}
function DciGuardInputs($gate,[string]$workspace,[string]$guardRoot) {
 if($gate.schema -cne 'pas.windows-offline-process-gate.v3' -or -not $gate.wrapper_positive -or $gate.native_controls_verified -ne 4 -or $gate.file_only_failed -ne 0){throw 'native-process-gate'}
 $paths=@();foreach($role in @('guard_source','process_source','cached_identity_controls','diagnostic','identity_wait_controls','initial_acquisition_controls')){$paths+=@(DciPin $gate.$role $workspace)}
 if(-not [string]::Equals($gate.guard_source.path,(Join-Path $guardRoot 'owned.cs'),[StringComparison]::OrdinalIgnoreCase) -or -not [string]::Equals($gate.process_source.path,(Join-Path $guardRoot 'process.ps1'),[StringComparison]::OrdinalIgnoreCase)){throw 'collection unchanged original guard/process required'}
 $initial=DciJson $gate.initial_acquisition_controls $workspace;$cache=DciJson $gate.cached_identity_controls $workspace;$wait=DciJson $gate.identity_wait_controls $workspace
 if($initial.schema -cne 'pas.initial-identity-acquisition-controls.v1' -or $initial.failed -ne 0 -or @($initial.cases).Count -ne 12 -or @($initial.cases|Where-Object {-not $_.pass}).Count -or $initial.actual_CreateProcess -ne 0 -or $initial.initial_window_ms -ne 50 -or $initial.initial_sleep_ms -ne 1 -or $initial.steady_sleep_ms -ne 25){throw 'initial-acquisition-controls-gate'}
 if($cache.failed -ne 0 -or @($cache.cases).Count -ne 9 -or $cache.actual_CreateProcess -ne 0 -or $wait.failed -ne 0 -or @($wait.cases).Count -ne 39 -or $wait.actual_CreateProcess -ne 0){throw 'guard-controls-gate'}
 foreach($pin in @($initial.shared_source)+@($wait.shared_source)){$paths+=@(DciPin $pin $workspace)}
 foreach($row in $gate.native_controls){$receipt=DciJson $row.receipt $workspace;$paths+=@(DciPin $row.receipt $workspace);foreach($pin in $receipt.entries){$paths+=@(DciPin $pin $workspace)}}
 return @($paths|Sort-Object -Unique)
}

# No collection or Add-Type occurs until all externally pinned request joins
# above are checked. Parser/rejection fixtures import only these function ASTs.
$requestPin=DciExternal $Specification $SpecificationSha256 $taskCollectionWorkspace
$gatePin=DciExternal $GateReceipt $GateReceiptSha256 $taskCollectionWorkspace
$request=DciJson $requestPin $taskCollectionWorkspace
$plan=DciPlan $request $taskCollectionWorkspace
$guardRoot=Join-Path $taskCollectionWorkspace 'research/windows_process_guard_v3'
$gateObject=DciJson $gatePin $taskCollectionWorkspace
$guardInputs=@(DciGuardInputs $gateObject $taskCollectionWorkspace $guardRoot)
$adapted=DciAdaptOwned (Join-Path $guardRoot 'process.ps1')
. (Join-Path $guardRoot 'process.ps1') -TaskRepo $taskCollectionWorkspace -TaskAttempt $plan.stage -TaskEvidence $plan.stage_root -TaskSource $guardRoot
. ([scriptblock]::Create($adapted))
# Shared recipe helpers have different metadata envelopes. Save the exact
# original guard functions before importing the recipe and restore them only
# for its owned launch; its NewJson retains PutHandle's original 64KiB limit.
$taskCollectionGuardNewJson=(Get-Item Function:NewJson).ScriptBlock
$taskCollectionGuardCheckEntry=(Get-Item Function:CheckEntry).ScriptBlock
$recipe=Join-Path $taskCollectionWorkspace 'research/prelive_current/run.ps1'
$tokens=$null;$errors=$null;$tree=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'collection shared recipe parse'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','Capacity','Receipt')){
 $definition=@($tree.FindAll({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name)
 if($definition.Count -ne 1){throw 'collection shared closure/capacity identity'}
 . ([scriptblock]::Create($definition[0].Extent.Text))
}
$taskCollectionRecipeNewJson=(Get-Item Function:NewJson).ScriptBlock
$taskCollectionRecipeCheckEntry=(Get-Item Function:CheckEntry).ScriptBlock
$repo=$taskCollectionWorkspace;$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006'
$Operation='device-collection';$CoreDonor='';$script:inputClosures=@{}
$directPath=Join-Path $pkg ($plan.stage+'-direct-spec.json')
Fresh $directPath
$direct=@{stage_root=$plan.stage_root;stage=$plan.stage;exe=$plan.exe;argv=$plan.argv;native_exit=0;runner_exit=0;total_s=$plan.total_s;stream_cap=$plan.stream_cap}
NewJson $directPath $direct
$metadata=@($Specification,$GateReceipt,$taskCollectionAdapter,$recipe,(Join-Path $guardRoot 'invoke.ps1'),(Join-Path $guardRoot 'process.ps1'),$directPath)+$guardInputs
$closureInputs=InputClosure $plan.stage @($plan.inputs+$metadata)
$afterRecorded=$false
try {
 Capacity $plan.stage 'before'
 CheckClosure $plan.stage 'before'
 [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($plan.output))|Out-Null
 try {
  Set-Item Function:NewJson -Value $taskCollectionGuardNewJson
  Set-Item Function:CheckEntry -Value $taskCollectionGuardCheckEntry
  $result=InvokeLocalOwnedStage -StageRoot $plan.stage_root -Stage $plan.stage -Executable $plan.exe -Arguments ([string[]]$plan.argv) -NativeExit 0 -RunnerExit 0 -TotalSeconds $plan.total_s -StreamCap $plan.stream_cap -Inputs @($closureInputs+$metadata)
 } finally {
  Set-Item Function:NewJson -Value $taskCollectionRecipeNewJson
  Set-Item Function:CheckEntry -Value $taskCollectionRecipeCheckEntry
 }
 Receipt $plan.stage 0
 CheckClosure $plan.stage 'after'
 Capacity $plan.stage 'after';$afterRecorded=$true
 ClosureReceipt $plan.stage $true
 $proof=@{specification=(Binding $directPath);input_closure=(Binding (Join-Path $pkg ($plan.stage+'-input-closure-verification.json')))}
 foreach($role in @('result','verification','command')){$proof[$role]=Binding (Join-Path $plan.stage_root ($plan.stage+'-'+$role+'.json'))}
 foreach($role in @('freeze','state','contract')){$proof[$role]=Binding (Join-Path $plan.stage_root ($role+'.json'))}
 @{schema='pas.current-device-collection-native-result.v1';stage=$plan.stage;result=$result;report=(Binding $plan.output);process_evidence=$proof;scope='actual_current_device_collection';qualification_issued=$false;live_ready=$false;device_operations='not_counted'}|ConvertTo-Json -Depth 12
} catch {
 $reason=$_.Exception.Message
 if(-not $afterRecorded){try{Capacity $plan.stage 'after'}catch{}}
 ClosureReceipt $plan.stage $false $reason
 throw
}
# The native guard publishes durable exits; no stale shell LASTEXITCODE is used.
exit 0
