param([Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$FrozenHead,
 [ValidatePattern('^[a-z0-9-]{1,32}$')][string]$Attempt='main81-pub-d1')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../pre_emulator/common.ps1')
. (Join-Path $PSScriptRoot '../cold_scheduler_diagnostic/capacity.ps1')
$script:maintenancePath=$PSCommandPath
$Operation='build'
function DRequire([bool]$v,[string]$why){if(-not $v){throw ('publication diagnostic: '+$why)}}
function DRead($pin){CheckEntry $pin;DRequire ($pin.bytes -le 4194304) 'metadata size';Get-Content -LiteralPath $pin.path -Raw|ConvertFrom-Json}
function DBool($v,[string]$key,[bool]$expected){DRequire ($v.$key -is [bool] -and $v.$key -eq $expected) ('typed bool '+$key)}
function DInt($v,[string]$key,[long]$expected){DRequire (($v.$key -is [int] -or $v.$key -is [long]) -and $v.$key -eq $expected) ('typed integer '+$key)}
function DSame($a,$b){[string]::Equals([IO.Path]::GetFullPath($a.path),[IO.Path]::GetFullPath($b.path),[StringComparison]::OrdinalIgnoreCase) -and $a.bytes -eq $b.bytes -and $a.sha256 -ceq $b.sha256}
$proofRecipe=Join-Path $repo 'research/windows_production_pipeline_v3/wait_diagnostic_component/build.ps1'
$pt=$null;$pe=$null;$pa=[Management.Automation.Language.Parser]::ParseFile($proofRecipe,[ref]$pt,[ref]$pe)
DRequire ($pe.Count -eq 0) 'original proof recipe parses'
$pf=@($pa.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq 'DProof')
DRequire ($pf.Count -eq 1) 'one unchanged Native8 proof function'
. ([scriptblock]::Create($pf[0].Extent.Text))
function AssertSource {
 DRequire ((& git -C $repo rev-parse HEAD) -ceq $FrozenHead -and (& git -C $repo branch --show-current) -ceq 'main' -and -not (& git -C $repo status --porcelain)) 'exact clean frozen main'
 foreach($pin in $sourcePins){CheckEntry $pin}
}
function AssertInputs {AssertSource;foreach($pin in $fixed){CheckEntry $pin}}
$basePin=Binding (Join-Path $repo 'out/prelive-producer-eligibility-evaluation-main80-e1/build-binding.json')
DRequire ($basePin.sha256 -ceq '7478f48f0cb7386836bfddc0bffbdf83c55bdf7f32727ae95bcb107b8a2bb058') 'original main80 binding'
$base=DRead $basePin
DRequire ($base.source_commit -ceq '3ee69edc85acb6b9b968424374a376f351d0e2dd' -and $base.configuration -ceq 'release') 'main80 source identity'
$baseSource=DRead $base.source_manifest
$sourcePins=@();$added=@();$baseByPath=@{}
foreach($pin in $baseSource.files){$baseByPath[[IO.Path]::GetRelativePath($repo,$pin.path).Replace('\','/')]=$pin}
$tracked=@(& git -c core.quotePath=false -C $repo ls-files|Where-Object {$_ -cne 'apps/runtime_x11_p/aux/CMakeLists.txt' -and $_ -cmatch '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'})
DRequire ($LASTEXITCODE -eq 0) 'source denominator'
foreach($path in $tracked){
 $pin=Binding (Join-Path $repo $path);$sourcePins+=@($pin)
 if($baseByPath.ContainsKey($path)){DRequire ($pin.bytes -eq $baseByPath[$path].bytes -and $pin.sha256 -ceq $baseByPath[$path].sha256) ('unchanged original source '+$path)}
 else{DRequire ($path.StartsWith('research/cold_publication_diagnostic/',[StringComparison]::Ordinal)) ('only independent diagnostic additions '+$path);$added+=@($path)}
}
DRequire ($added.Count -ge 9 -and @($baseByPath.Keys|Where-Object {$_ -cnotin $tracked}).Count -eq 0) 'complete unchanged original source projection'
AssertSource
$roles=@('cpu_current_rails','cpu_current_front','cpu_exact_bvi_v3','cpu_own_bridge','cpu_pas_core')
$libs=@();foreach($role in $roles){$rows=@($base.imported_static_libraries|Where-Object role -ceq $role);DRequire ($rows.Count -eq 1) ('one imported role '+$role);CheckEntry $rows[0].binary;$libs+=@($rows[0])}
$motion=$base.motion_source_delta.new_object;CheckEntry $motion
$runtime=@($base.runtime18_sources);DRequire ($runtime.Count -eq 18) 'original 18 runtime sources'
$dependency=DRead $base.selected_dependency_manifest
$envPin=$base.compiler_environment;$envDoc=DRead $envPin
$fixed=@($basePin,$base.source_manifest,$base.selected_dependency_manifest,$envPin,$envDoc.environment,$envDoc.verification,$envDoc.state,$motion)+@($libs.binary)+$runtime+@($dependency.files)
foreach($pin in $fixed){CheckEntry $pin}
$artifact=Join-Path $repo ('out/prelive-publication-diagnostic-evaluation-'+$Attempt)
$buildRoot=Join-Path $repo ('out/prelive-publication-diagnostic-'+$Attempt)
Fresh $artifact;Fresh $buildRoot;$null=[IO.Directory]::CreateDirectory($artifact)
$manifest=Join-Path $artifact 'source-manifest.json'
NewJson $manifest @{schema='pas.cooperative-source.v1';source_commit=$FrozenHead;dirty=$false;files=$sourcePins;base_source_manifest=$base.source_manifest;added_compiled_paths=$added;changed_original_compiled_paths=@();diagnostic_only=$true;qualification_issued=$false;live_ready=$false;device_operations=0}
$snapRoot=Join-Path $repo ('out/prelive-psrc-'+$Attempt);Fresh $snapRoot;$null=[IO.Directory]::CreateDirectory($snapRoot)
$copies=@();foreach($pin in $sourcePins){$rel=[IO.Path]::GetRelativePath($repo,$pin.path);$dest=Join-Path $snapRoot ('files/'+$rel);$null=[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($dest));[IO.File]::Copy($pin.path,$dest,$false);$copy=Binding $dest;DRequire ($copy.bytes -eq $pin.bytes -and $copy.sha256 -ceq $pin.sha256) 'exact snapshot copy';$copies+=@(@{original=$pin;copy=$copy})}
foreach($pin in $sourcePins){CheckEntry $pin}
$snapshot=Join-Path $snapRoot 'snapshot.json'
NewJson $snapshot @{schema='pas.lossless-diagnostic-source-snapshot.v1';source_commit=$FrozenHead;source_manifest=(Binding $manifest);files=$copies;all_copy_SHAs_verified=$true;originals_rechecked=$true;qualification_issued=$false;device_operations=0}
$libCmake=Join-Path $artifact 'libraries.cmake';$text=''
foreach($row in $libs){$path=$row.binary.path.Replace('\','/');DRequire ($path -notmatch '[";$\r\n]') 'literal library path';$text+='set(PAS_'+$row.role+'_PATH "'+$path+'")'+"`n"+'set(PAS_'+$row.role+'_SHA256 "'+$row.binary.sha256+'")'+"`n"}
[IO.File]::WriteAllText($libCmake,$text,[Text.UTF8Encoding]::new($false))
$runtimeCmake=Join-Path $artifact 'runtime.cmake';$text="set(PAS_RUNTIME_PATHS`n";foreach($pin in $runtime){$path=$pin.path.Replace('\','/');DRequire ($path -notmatch '[";$\r\n]') 'literal runtime path';$text+=' "'+$path+'"'+"`n"};$text+=")`n";[IO.File]::WriteAllText($runtimeCmake,$text,[Text.UTF8Encoding]::new($false))
$adapter=Join-Path $repo 'research/main_consolidation/cmake-stage.ps1'
$toolchain='C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake'
$g=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$inputs=@($PSCommandPath,$proofRecipe,$recipe,$adapter,(Join-Path $repo 'research/main_consolidation/stage.ps1'),(Join-Path $repo 'research/pre_emulator/common.ps1'),(Join-Path $repo 'research/cold_scheduler_diagnostic/capacity.ps1'),$toolchain,$manifest,$snapshot,$libCmake,$runtimeCmake)+@($sourcePins.path)+@($fixed.path)+@(GateInputs)+@($g.guard_source.path,$g.process_source.path,$g.cached_identity_controls.path,$g.identity_wait_controls.path)
$options=@(('-DCMAKE_TOOLCHAIN_FILE='+$toolchain),('-DVCPKG_INSTALLED_DIR='+(Join-Path $repo 'out/vcpkg_installed')),'-DVCPKG_TARGET_TRIPLET=x64-windows','-DVCPKG_MANIFEST_MODE=OFF','-DVCPKG_APPLOCAL_DEPS=OFF','-DCMAKE_CXX_SCAN_FOR_MODULES=OFF','-DCMAKE_HAVE_LIB_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=','-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL',('-DPAS_SOURCE_WORKSPACE='+$repo),('-DPAS_SOURCE_COMMIT='+$FrozenHead),('-DPAS_LIBRARY_INPUTS_FILE='+$libCmake),('-DPAS_RUNTIME_INPUTS_FILE='+$runtimeCmake),('-DPAS_BASE_MOTION_OBJECT='+$motion.path),('-DPAS_BASE_MOTION_SHA='+$motion.sha256),('-DPAS_WORKER_AFFINITY_SHA='+(Binding (Join-Path $repo 'research/prelive_current/worker_affinity.hpp')).sha256),('-DPAS_PRODUCER_PLACEMENT_SHA='+(Binding (Join-Path $repo 'research/windows_production_pipeline_v3/producer_placement.hpp')).sha256))
$configure='configure-pub-'+$Attempt;AssertInputs
Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=$PSScriptRoot;BuildRoot=$buildRoot;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$envPin.path;Operation='Configure';Configuration='Release';Inputs=$inputs;Options=$options}
$configureProof=DProof $configure
$ninja=(Get-Content -LiteralPath (Join-Path $buildRoot 'build.ninja') -Raw).Replace('\','/')
$edges=@([regex]::Matches($ninja,'(?m)^build ([^\r\n:]+[.]obj): CXX_COMPILER[^ \r\n]+[ \t]+([^|\r\n]+)'))
DRequire ($edges.Count -eq 3 -and $ninja -notmatch 'applocal[.]ps1' -and $ninja -notmatch '/WHOLEARCHIVE' -and $ninja -match '/O2 /Ob2 /DNDEBUG') 'exact three diagnostic TUs and unchanged Release flags'
foreach($name in @('core.instrumented.cpp','chain.instrumented.cpp','controls.cpp')){DRequire (@($edges|Where-Object {$_.Groups[2].Value.Trim().EndsWith('/'+$name,[StringComparison]::Ordinal)}).Count -eq 1) ('one fresh source edge '+$name)}
$runtimeCopies=@();foreach($pin in $runtime){$copy=Binding (Join-Path $buildRoot ([IO.Path]::GetFileName($pin.path)));DRequire ($copy.bytes -eq $pin.bytes -and $copy.sha256 -ceq $pin.sha256) 'runtime exact copy';$runtimeCopies+=@($copy)}
$generated=Binding (Join-Path $buildRoot 'producer_eligibility_delta_identity.hpp')
$inputs+=@($generated.path)+(Join-Path $buildRoot 'CMakeCache.txt')+(Join-Path $buildRoot 'build.ninja')+@($runtimeCopies.path)+@($configureProof.Values.path)
$build='build-pub-'+$Attempt;AssertInputs
Stage $build $adapter @{ReceiptRepo=$repo;SourceRoot=$PSScriptRoot;BuildRoot=$buildRoot;StageName=$build;ToolSnapshot=$tool;GateReceipt=$gate;EnvironmentBinding=$envPin.path;Operation='Build';Configuration='Release';Inputs=$inputs;Options=@('--target','current_chain','publication_controls')}
$buildProof=DProof $build;AssertInputs
$outputs=@(Get-ChildItem -LiteralPath $buildRoot -File -Recurse|ForEach-Object {Binding $_.FullName})
$objects=@($outputs|Where-Object {$_.path -match '[.]obj$' -and $_.path.Replace('\','/') -cmatch '/CMakeFiles/(publication_core|current_chain|publication_controls)[.]dir/'})
DRequire ($objects.Count -eq 3) 'three actual fresh objects'
$map=Get-Content -LiteralPath (Join-Path $buildRoot 'current_chain.map') -Raw
DRequire ($map -match 'core[.]instrumented[.]cpp[.]obj' -and $map -notmatch 'pas_core:core[.]cpp[.]obj' -and $map -match 'game_motion[.]cpp[.]obj') 'actual core replacement and original motion object'
$exes=@(Binding (Join-Path $buildRoot 'current_chain.exe');Binding (Join-Path $buildRoot 'publication_controls.exe'))
$script:seal=@{files=$exes+$runtimeCopies}
$Operation='tests'
$reportPath=Join-Path $artifact 'publication-controls.json';$stage='test-pub-'+$Attempt+'-controls'
$compileOnlyLarge=@($dependency.files|Where-Object {$_.bytes -gt 268435456 -and [IO.Path]::GetExtension($_.path) -ieq '.lib'})
$controlPolicy=Join-Path $artifact 'controls-input-policy.json'
NewJson $controlPolicy @{schema='pas.cold-controls-input-policy.v1';source_commit=$FrozenHead;selected_dependency_manifest=$base.selected_dependency_manifest;compile_only_large_installed_archives=$compileOnlyLarge;configure_and_build_keep_all_selected_dependencies=$true;final_full_dependency_recheck_preserved=$true;actual_images_runtime_sources_objects_imported_libraries_preserved=$true;qualification_issued=$false;device_operations=0}
$controlInputs=@($inputs+$outputs.path+$buildProof.Values.path|Where-Object {$_ -notin $compileOnlyLarge.path})+@($controlPolicy)
Native $stage $exes[1].path @($reportPath) $controlInputs 300
$controlsProof=DProof $stage;$controls=DRead (Binding $reportPath)
DRequire ($controls.schema -ceq 'pas.cold-publication-diagnostic-controls.v1' -and $controls.source_build_commit -ceq $FrozenHead) 'fresh controls identity'
DBool $controls 'source_build_dirty' $false;DBool $controls 'pass' $true;DInt $controls 'failed' 0
DRequire (@($controls.checks).Count -eq 16 -and @($controls.checks.id|Sort-Object -Unique).Count -eq 16) 'sixteen aggregate controls'
foreach($check in $controls.checks){DBool $check 'pass' $true}
foreach($pin in $outputs){CheckEntry $pin};AssertInputs
$bindingPath=Join-Path $artifact 'build-binding.json'
NewJson $bindingPath @{schema='pas.cold-publication-diagnostic-build.v1';source_commit=$FrozenHead;source_dirty=$false;source_manifest=(Binding $manifest);source_snapshot=(Binding $snapshot);base_build=$basePin;base_source_commit=$base.source_commit;compiler_environment=$envPin;dependency_manifest=$base.selected_dependency_manifest;build_root=$buildRoot;files=$exes+$runtimeCopies;compiled_outputs=$outputs;translation_unit_count=3;imported_static_libraries=$libs;motion_object=$motion;core_archive_member_replaced='core.cpp.obj';original_archive_unchanged=$true;configure_process=$configureProof;build_process=$buildProof;controls_process=$controlsProof;controls_input_policy=(Binding $controlPolicy);controls_report=(Binding $reportPath);diagnostic_only=$true;formal_rebuilt=$false;qualification_issued=$false;cost_qualification=$false;cost_ready=$false;live_ready=$false;device_operations=0;new_etw_recordings=0}
@{binding=(Binding $bindingPath);translation_units=3;controls=16;qualification_issued=$false;device_operations=0}|ConvertTo-Json -Depth 4
