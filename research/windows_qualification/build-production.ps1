param([Parameter(Mandatory)][string]$GateReceipt,
      [Parameter(Mandatory)][string]$ToolSnapshot,
      [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$Attempt='prod01')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh=(Get-Command pwsh -ErrorAction Stop).Source
$gate=[IO.Path]::GetFullPath($GateReceipt);$tool=[IO.Path]::GetFullPath($ToolSnapshot)
$Mode='release';$Operation='build';$script:inputClosures=@{}
$recipe=Join-Path $repo 'research/prelive_current/run.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'shared-recipe-parser'}
# Reuse the unchanged bounded closure/owned-stage implementation, file functions
# only. The current wrapper entry point and its research build are not executed.
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage')){
 $function=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name)
 if($function.Count -ne 1){throw ('shared-recipe-function '+$name)}
 . ([scriptblock]::Create($function[0].Extent.Text))
}
$inputs=@($PSCommandPath,$recipe,(Join-Path $repo 'CMakeLists.txt'),(Join-Path $repo 'vcpkg.json'),(Join-Path $repo 'vcpkg-configuration.json'))+(GateInputs)
foreach($relative in @('src','include','apps/pas','proto','cmake','research/bvi_cold_v3','research/hold_front_current','research/prelive_current')){
 $inputs+=@(Get-ChildItem -LiteralPath (Join-Path $repo $relative) -File -Recurse|Where-Object {$_.Extension -in @('.cpp','.hpp','.h','.proto','.in') -or $_.Name -eq 'CMakeLists.txt'}|ForEach-Object FullName)
}
$dependencyRoot='C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed/x64-windows'
# Before/after bind installed package declarations and all Release link inputs.
# Actual compiler include expansion is retained by a separate Ninja deps stage.
foreach($relative in @('lib','share')){$inputs+=@(Get-ChildItem -LiteralPath (Join-Path $dependencyRoot $relative) -File -Recurse|ForEach-Object FullName)}
$inputs=@($inputs|Sort-Object -Unique)
$root=Join-Path $repo ('out/prelive-root-release-'+$Attempt)
$options=@('-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake',
 '-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed',
 '-DVCPKG_MANIFEST_INSTALL=OFF','-DVCPKG_TARGET_TRIPLET=x64-windows','-DPAS_ENABLE_CPU_VISION=OFF')
foreach($operation in @('Configure','Build')){
 $stage=$operation.ToLowerInvariant()+'-prelive-root-release-'+$Attempt
 $arguments=@{SourceRoot=$repo;BuildRoot=$root;StageName=$stage;ToolSnapshot=$tool;GateReceipt=$gate;Operation=$operation;Configuration='Release';Inputs=$inputs;TotalSeconds=900}
 $arguments.Options=if($operation -eq 'Configure'){$options}else{@('--target','pas','pas_disabled_live_seam_tests')}
 Stage $stage (Join-Path $repo 'research/prelive_current/cmake-stage.ps1') $arguments
}
$binding=Join-Path $pkg ('production-build-release-'+$Attempt+'.json');Fresh $binding
NewJson $binding @{schema='pas.current-production-cold-build.v1';source_commit=(& git -C $repo rev-parse HEAD);source_dirty=([bool](& git -C $repo status --porcelain));
 root=$root;configuration='Release';gate=(Binding $gate);tool_snapshot=(Binding $tool);
 sources=@($inputs|ForEach-Object {Binding $_});
 artifacts=@((Binding (Join-Path $root 'pas.exe')),(Binding (Join-Path $root 'pas_disabled_live_seam_tests.exe')));
 closure_receipts=@($script:inputClosures.Values|ForEach-Object {Binding $_.verification_output});
 production_branch_compiled=$true;production_binary_executed=$false;windows_cost_qualified=$false;live_ready=$false;device_operations=0}
