param([Parameter(Mandatory)][string]$ReceiptRepo,[ValidatePattern('^[a-z0-9-]{1,12}$')][string]$Attempt='journal01')
$ErrorActionPreference='Stop'
$candidateRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'));$repo=[IO.Path]::GetFullPath($ReceiptRepo)
$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006';$pwsh=(Get-Command pwsh).Source
$Operation='build';$Mode='release';$Campaign='';$script:inputClosures=@{};$script:maintenancePath=$PSCommandPath
$gate=Join-Path $out 'guard-v3-native-01/native-gate.json';$tool=Join-Path $out 'tool-snapshot-01/tool-snapshot.json';$recipe=Join-Path $repo 'research/prelive_current/run.ps1'
$tok=$null;$err=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tok,[ref]$err);if($err.Count){throw 'recipe parse'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage','Native')){
 $d=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name);if($d.Count -ne 1){throw 'shared function'};$t=$d[0].Extent.Text
 if($name -ceq 'Native'){$t=$t.Replace('$PSCommandPath','$script:maintenancePath').Replace('tools/zero_miss_windows/invoke.ps1','research/windows_process_guard_v3/invoke.ps1')}
 if($name -ceq 'GateInputs'){$t=$t.Replace('pas.windows-offline-process-gate.v1','pas.windows-offline-process-gate.v3')};if($name -ceq 'Capacity'){$t=$t.Replace('12884901888','17179869184')};. ([scriptblock]::Create($t))
}
$commit=(& git -C $candidateRepo rev-parse HEAD);if((& git -C $candidateRepo status --porcelain)){throw 'clean candidate required'}
$sourceRoot=Join-Path $candidateRepo ('out/prelive-source-'+$Attempt);Fresh $sourceRoot;[IO.Directory]::CreateDirectory($sourceRoot)|Out-Null
$retained=Join-Path $repo 'out/prelive-aa-aa01/source-manifest.json';$old=Get-Content -LiteralPath $retained -Raw|ConvertFrom-Json
$sources=@(& git -C $candidateRepo ls-files|Where-Object {$_ -match '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'}|ForEach-Object {
 $rel=$_;$path=Join-Path $candidateRepo $rel;if(-not(Test-Path -LiteralPath $path)){if($rel -cne 'apps/runtime_x11_p/aux/CMakeLists.txt'){throw 'missing source'};$donor=@($old.files|Where-Object git_relative_path -ceq $rel);if($donor.Count -ne 1){throw 'sparse donor'};CheckEntry $donor[0];$path=Join-Path $sourceRoot 'sparse-linux-aux-CMakeLists.txt';Copy-Item -LiteralPath $donor[0].path -Destination $path};$b=Binding $path;$b.relative_path=[IO.Path]::GetRelativePath($candidateRepo,$path).Replace('\','/');if($b.relative_path -cne $rel){$b.git_relative_path=$rel};$b
})
$sourcePath=Join-Path $sourceRoot 'source-manifest.json';NewJson $sourcePath @{source_commit=$commit;dirty=$false;repo_root=$candidateRepo;files=$sources}
$artifact=Join-Path $repo ('out/prelive-journal-evaluation-'+$Attempt);Fresh $artifact;[IO.Directory]::CreateDirectory($artifact)|Out-Null
$g=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json;$adapter=Join-Path $candidateRepo 'research/windows_owner_wake/cmake-stage-v2.ps1'
$base=@($PSCommandPath,$recipe,$adapter,$sourcePath,$retained,(Join-Path $PSScriptRoot 'PLAN.md'))+$sources.path+(GateInputs)+@($g.guard_source.path,$g.process_source.path,$g.cached_identity_controls.path,$g.identity_wait_controls.path)
$builds=@();$tests=@();$coreLine=Get-Content -LiteralPath (Join-Path $candidateRepo 'CMakeLists.txt')|Where-Object {$_ -match '^add_library\(pas_core STATIC '};$declared=(($coreLine -replace '^add_library\(pas_core STATIC ','') -replace '\)$','').Split(' ',[StringSplitOptions]::RemoveEmptyEntries)
foreach($mode in @('release','debug','asan')){
 $Mode=$mode;$Operation='build';$root=Join-Path $repo ('out/prelive-journal-'+$mode+'-'+$Attempt);$configuration=if($mode -eq 'release'){'Release'}else{'Debug'}
 $opts=@('-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake','-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed','-DVCPKG_TARGET_TRIPLET=x64-windows','-DVCPKG_MANIFEST_MODE=OFF','-DCMAKE_HAVE_LIBC_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=')
 if($mode -eq 'asan'){$opts+=@('-DPAS_PRELIVE_ASAN=ON','-DPAS_ASAN_SUPPORT_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')}
 $configure='configure-prelive-journal-'+$mode+'-'+$Attempt
 Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=$PSScriptRoot;BuildRoot=$root;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;Operation='Configure';Configuration=$configuration;Options=$opts;Inputs=$base}
 $ninja=Join-Path $root 'build.ninja';$cache=Join-Path $root 'CMakeCache.txt';$objects=@(Get-Content -LiteralPath $ninja|ForEach-Object {if($_ -match '^build ([^:]+\.obj): CXX_COMPILER__formal_core_'){$matches[1]}})
 if($objects.Count -ne $declared.Count){throw 'complete exact formal core object inventory'}
 foreach($rel in $declared){$name=[IO.Path]::GetFileName($rel)+'.obj';if(@($objects|Where-Object {($_.Replace('\','/').Split('/')[-1]) -ceq $name}).Count -ne 1){throw 'core source/object join'}}
 $receipts=@();for($i=0;$i -lt $objects.Count;$i+=5){$last=[Math]::Min($i+4,$objects.Count-1);$stage='build-prelive-journal-'+$mode+'-'+$Attempt+'-core-'+([int]($i/5)+1)
  Stage $stage $adapter @{ReceiptRepo=$repo;SourceRoot=$PSScriptRoot;BuildRoot=$root;StageName=$stage;ToolSnapshot=$tool;GateReceipt=$gate;Operation='Build';Configuration=$configuration;Options=@('--target')+@($objects[$i..$last]);Inputs=@($base+$cache+$ninja)};$receipts+=@(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))
 }
 $functional=@('current_tests','active_body_tests','tail_guard_tests','prefix_preservation','front_preservation','owner_wait_tests','producer_wait_tests','tap_reset_workload_tests','disabled_live_seam_tests','runtime_finish_tests')
 foreach($target in @('formal_core','exact_bvi_v3','current_front','own_bridge','current_rails','current_chain','prelive_audit','journal_encoding_tests','original_session_tests')+$functional){$stage='build-prelive-journal-'+$mode+'-'+$Attempt+'-'+$target.Replace('_','-');Stage $stage $adapter @{ReceiptRepo=$repo;SourceRoot=$PSScriptRoot;BuildRoot=$root;StageName=$stage;ToolSnapshot=$tool;GateReceipt=$gate;Operation='Build';Configuration=$configuration;Options=@('--target',$target);Inputs=@($base+$cache+$ninja)};$receipts+=@(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))}
 $files=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object {Binding $_.FullName});$seal=@{files=$files};$builds+=@(@{configuration=$mode;source_commit=$commit;build_root=$root;files=$files;library_donors=@();complete_core_rebuilt=$true;configure_receipt=(Binding (Join-Path $out ($configure+'/'+$configure+'-verification.json')));build_receipts=$receipts})
 $Operation='tests'
 foreach($target in $functional){$report=Join-Path $artifact ($mode+'-'+$target+'.json');$stage='test-prelive-journal-'+$mode+'-'+$Attempt+'-'+$target.Replace('_','-');Native $stage (Join-Path $root ('prelive/'+$target+'.exe')) @($report) @($base+$files.path+$receipts.path) 900;$v=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json;if($v.failed -ne 0){throw 'functional regression'};$tests+=@(@{configuration=$mode;test=$target;report=(Binding $report);receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))})}
 foreach($target in @('journal_encoding_tests','original_session_tests')){$report=Join-Path $artifact ($mode+'-'+$target+'.xml');$stage='test-prelive-journal-'+$mode+'-'+$Attempt+'-'+$target.Replace('_','-');Native $stage (Join-Path $root ($target+'.exe')) @('--gtest_output=xml:'+$report) @($base+$files.path+$receipts.path) 900;[xml]$xml=Get-Content -LiteralPath $report -Raw;if([int]$xml.testsuites.failures -ne 0 -or [int]$xml.testsuites.errors -ne 0){throw 'archive/journal regression'};$tests+=@(@{configuration=$mode;test=$target;report=(Binding $report);receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))})}
}
$path=Join-Path $artifact 'candidate-builds.json';NewJson $path @{schema='pas.journal-cost-build-binding.v1';source_commit=$commit;source_manifest=(Binding $sourcePath);builds=$builds;tests=$tests;complete_formal_core_rebuilt=$true;cost_qualified=$false;live_ready=$false;device_operations=0}
