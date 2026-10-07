param([Parameter(Mandatory)][string]$ReceiptRepo,[ValidatePattern('^[a-z0-9-]{1,15}$')][string]$Attempt='wake01')
function CanonicalSource([string]$path){
 $raw=[IO.File]::ReadAllBytes($path);if($raw.Length -ge 1048576){throw 'source-byte-cap'}
 $decoded=[Text.UTF8Encoding]::new($false,$true).GetString($raw)
 if($decoded.StartsWith([string][char]0xfeff,[StringComparison]::Ordinal)){throw 'source-bom'}
 $lf=$decoded.Replace("`r`n","`n");if($lf.Contains("`r")){throw 'source-isolated-cr'}
 [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($lf))).ToLowerInvariant()
}
$ErrorActionPreference='Stop'
$candidateRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'));$repo=[IO.Path]::GetFullPath($ReceiptRepo)
$out=Join-Path $repo 'out/windows-handoff';$pkg=Join-Path $repo 'out/prelive-20261006'
$gate=Join-Path $out 'guard-v3-native-01/native-gate.json';$tool=Join-Path $out 'tool-snapshot-01/tool-snapshot.json'
$pwsh=(Get-Command pwsh -ErrorAction Stop).Source;$Operation='build';$Mode='release';$Campaign='';$script:inputClosures=@{}
$script:maintenancePath=$PSCommandPath
$recipe=Join-Path $repo 'research/prelive_current/run.ps1';$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tokens,[ref]$errors);if($errors.Count){throw 'recipe-parser'}
foreach($name in @('NewJson','CheckEntry','Binding','Fresh','InputClosure','CheckClosure','ClosureReceipt','GateInputs','Capacity','Receipt','Stage','Native')){
 $d=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name);if($d.Count -ne 1){throw 'shared-function'}
 $t=$d[0].Extent.Text;if($name -ceq 'Native'){$t=$t.Replace('$PSCommandPath','$script:maintenancePath').Replace('tools/zero_miss_windows/invoke.ps1','research/windows_process_guard_v3/invoke.ps1')}
 if($name -ceq 'GateInputs'){$t=$t.Replace('pas.windows-offline-process-gate.v1','pas.windows-offline-process-gate.v3')}
 . ([scriptblock]::Create($t))
}
$commit=(& git -C $candidateRepo rev-parse HEAD);if((& git -C $candidateRepo status --porcelain)){throw 'clean-candidate-required'}
$retained=Join-Path $repo 'out/prelive-aa-aa01/source-manifest.json';$old=Get-Content -LiteralPath $retained -Raw|ConvertFrom-Json;foreach($b in $old.files){CheckEntry $b}
$newRoot=Join-Path $candidateRepo ('out/prelive-source-'+$Attempt);Fresh $newRoot;[IO.Directory]::CreateDirectory($newRoot)|Out-Null
$sources=@(& git -C $candidateRepo ls-files|Where-Object {$_ -match '(\.(cpp|hpp|h|ps1|proto|in)$|(^|/)CMakeLists\.txt$)'}|ForEach-Object{
 $rel=$_;$path=Join-Path $candidateRepo $rel
 if(-not(Test-Path -LiteralPath $path)){
  if($rel -cne 'apps/runtime_x11_p/aux/CMakeLists.txt'){throw 'missing-source'}
  $path=Join-Path $newRoot 'sparse-linux-aux-CMakeLists.txt';$donor=@($old.files|Where-Object git_relative_path -ceq $rel);if($donor.Count -ne 1){throw 'sparse-donor'};CheckEntry $donor[0];Copy-Item -LiteralPath $donor[0].path -Destination $path
 }
 $b=Binding $path;$b.relative_path=[IO.Path]::GetRelativePath($candidateRepo,$path).Replace('\','/');if($b.relative_path -cne $rel){$b.git_relative_path=$rel};$b
})
$sourcePath=Join-Path $newRoot 'source-manifest.json';NewJson $sourcePath @{source_commit=$commit;dirty=$false;repo_root=$candidateRepo;files=$sources}
$artifactRoot=Join-Path $repo ('out/prelive-owner-wake-'+$Attempt);Fresh $artifactRoot;[IO.Directory]::CreateDirectory($artifactRoot)|Out-Null
$inherited=Join-Path $repo 'out/readiness-20261007-01/cold-prerequisites-02.json';$pre=Get-Content -LiteralPath $inherited -Raw|ConvertFrom-Json;if(-not $pre.pass){throw 'old-cold-prerequisites'};foreach($b in $pre.bindings){CheckEntry $b}
$guard=Get-Content -LiteralPath $gate -Raw|ConvertFrom-Json
$baseInputs=@($PSCommandPath,$recipe,$sourcePath,$retained,$inherited)+$sources.path+(GateInputs)+@($guard.guard_source.path,$guard.process_source.path,$guard.cached_identity_controls.path,$guard.identity_wait_controls.path)
$rows=@();$builds=@();$candidateFiles=@();$encodingJoins=@()
foreach($config in @('release','debug','asan')){
 $Mode=$config;$Operation='build';$root=Join-Path $repo ('out/prelive-current-'+$config+'-'+$Attempt)
 $donor=Join-Path $repo $(if($config -eq 'release'){'out/prelive-current-release-win04'}elseif($config -eq 'debug'){'out/prelive-current-debug-win03'}else{'out/prelive-current-asan-win03'})
 $before=Get-Content -LiteralPath (Join-Path $repo ('out/prelive-20261006/current-build-'+$config+'-'+$(if($config -eq 'release'){'win04'}else{'win03'})+'.json')) -Raw|ConvertFrom-Json
 $cached=Join-Path $donor 'CMakeCache.txt';$cacheBinding=@($before.files|Where-Object path -ceq $cached);if($cacheBinding.Count -ne 1){throw 'cached-probe-binding'};CheckEntry $cacheBinding[0];foreach($key in @('CMAKE_HAVE_LIBC_PTHREAD','CMAKE_HAVE_PTHREADS_CREATE','CMAKE_HAVE_PTHREAD_CREATE')){if(-not ([IO.File]::ReadAllText($cached).Contains($key+':INTERNAL='))){throw 'original-negative-thread-probe-required'}}
 $donorBinding=@();foreach($lib in @('formal_core','current_front','own_bridge','current_rails','exact_bvi_v3')){$bound=@($before.files|Where-Object path -ceq (Join-Path $donor ($lib+'.lib')));if($bound.Count -ne 1){throw 'bound-library-donor'};CheckEntry $bound[0];$donorBinding+=@($bound[0])}
 # Every unchanged formal core source is byte joined to the prior source
 # manifest. The changed host never substitutes or claims the donor binary.
 $coreLine=Get-Content -LiteralPath (Join-Path $candidateRepo 'CMakeLists.txt')|Where-Object {$_ -match '^add_library\(pas_core STATIC '}
 $coreNames=($coreLine -replace '^add_library\(pas_core STATIC ','') -replace '\)$','';$coreInputs=@()
 foreach($rel in @($old.files|Where-Object {$_.relative_path -match '\.(cpp|hpp|h)$' -and $_.relative_path -notin @('research/prelive_current/chain.cpp','research/prelive_current/cost_analysis.cpp','research/prelive_current/tests/cost_analysis_tests.cpp')}|ForEach-Object relative_path)){$b=Binding (Join-Path $candidateRepo $rel);$prior=@($old.files|Where-Object relative_path -ceq $rel);if($prior.Count -ne 1){throw 'core-source-join-missing'};CheckEntry $prior[0];$a=CanonicalSource $b.path;$z=CanonicalSource $prior[0].path;if($a -cne $z){throw 'canonical-core-source-changed'};$encodingJoins+=@(@{configuration=$config;relative_path=$rel;current=$b;original=$prior[0];canonical_lf_sha256=$a;only_utf8_CRLF_LF_difference=$true});$coreInputs+=@($b.path)}
 $configName=if($config -eq 'release'){'Release'}else{'Debug'}
 $opts=@('-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake','-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed','-DVCPKG_MANIFEST_MODE=OFF','-DVCPKG_TARGET_TRIPLET=x64-windows','-DCMAKE_HAVE_LIBC_PTHREAD:INTERNAL=','-DCMAKE_HAVE_PTHREADS_CREATE:INTERNAL=','-DCMAKE_HAVE_PTHREAD_CREATE:INTERNAL=',('-DPAS_PRELIVE_CORE_DONOR='+$donor.Replace('\','/')))
 if($config -eq 'asan'){$opts+=@('-DPAS_PRELIVE_ASAN=ON','-DPAS_ASAN_SUPPORT_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')}
 $adapter=Join-Path $PSScriptRoot 'cmake-stage-v2.ps1';$configure='configure-prelive-'+$config+'-'+$Attempt;$build='build-prelive-'+$config+'-'+$Attempt
 Stage $configure $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $candidateRepo 'research/windows_owner_wake/runtime');BuildRoot=$root;StageName=$configure;ToolSnapshot=$tool;GateReceipt=$gate;Operation='Configure';Configuration=$configName;Options=$opts;Inputs=@($baseInputs+$donorBinding.path+$coreInputs+$cached)}
 $buildReceipts=@()
 # Split native builds at target boundaries to keep every Job below the
 # existing 64 held-identity cap; no guard limit is enlarged or bypassed.
 $targets=@('current_chain','owner_wait_tests','producer_wait_tests','tap_reset_workload_tests','lateness_diagnose','disabled_live_seam_tests','runtime_finish_tests');if($config -eq 'release'){$targets+=@('cost_analysis','cost_analysis_tests','pas_cost_receipt_verify','pas_cost_comparison','comparison_metrics_tests')};foreach($buildTarget in $targets){
  $build='build-prelive-'+$config+'-'+$Attempt+'-'+$buildTarget.Replace('_','-')
  Stage $build $adapter @{ReceiptRepo=$repo;SourceRoot=(Join-Path $candidateRepo 'research/windows_owner_wake/runtime');BuildRoot=$root;StageName=$build;ToolSnapshot=$tool;GateReceipt=$gate;Operation='Build';Configuration=$configName;Options=@('--target',$buildTarget);Inputs=@($baseInputs+$donorBinding.path+$coreInputs+$cached+(Join-Path $root 'CMakeCache.txt')+(Join-Path $root 'build.ninja'))}
  $buildReceipts+=@(Binding (Join-Path $out ($build+'/'+$build+'-verification.json')))
 }
 $files=@(Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object {Binding $_.FullName});$seal=@{files=$files};$builds+=@(@{configuration=$config;source_commit=$commit;core_donor=$donorBinding[0];library_donors=$donorBinding;core_source_join=$true;build_receipts=$buildReceipts;configure_receipt=(Binding (Join-Path $out ($configure+"/"+$configure+"-verification.json")));build_receipt=(Binding (Join-Path $out ($build+'/'+$build+'-verification.json')));files=$files})
 $Operation='tests'
 foreach($test in @('owner_wait_tests','producer_wait_tests','tap_reset_workload_tests','disabled_live_seam_tests','runtime_finish_tests')){
  $target=Join-Path $artifactRoot ($config+'-'+$test+'.json');$stage='test-prelive-'+$config+'-'+$Attempt+'-'+$test.Replace('_','-')
  Native $stage (Join-Path $root ($test+'.exe')) @($target) @($baseInputs+$files.path) 900
  $report=Get-Content -LiteralPath $target -Raw|ConvertFrom-Json;if($report.failed -ne 0){throw 'cold-test-failure'};$rows+=@(@{configuration=$config;test=$test;report=(Binding $target);receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))})
 }
 if($config -eq 'release'){$fixture=Join-Path $artifactRoot 'cost-analyzer-fixtures';$stage='cost-unit-prelive-release-'+$Attempt;Native $stage (Join-Path $root 'cost_analysis_tests.exe') @($fixture,$candidateRepo) @($baseInputs+$files.path) 900;$rows+=@(@{configuration=$config;test='cost_analysis_tests';report=(Binding (Join-Path $fixture 'result.json'));receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))});$stage='metric-controls-prelive-release-'+$Attempt;$target=Join-Path $artifactRoot 'comparison-metric-controls.json';Native $stage (Join-Path $root 'comparison_metrics_tests.exe') @($target) @($baseInputs+$files.path) 300;$rows+=@(@{configuration=$config;test='comparison_metrics_tests';report=(Binding $target);receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))});$stage='diagnose-prelive-'+$Attempt;$target=Join-Path $artifactRoot 'aa04-lateness-diagnosis.json';$raw=Join-Path $pkg 'pipeline-release-aa04-tap1.json';Native $stage (Join-Path $root 'lateness_diagnose.exe') @($raw,($raw+'.journal.jsonl'),$target) @($baseInputs+$raw+($raw+'.journal.jsonl')) 300;$rows+=@(@{test='old-raw-diagnosis';report=(Binding $target);receipt=(Binding (Join-Path $out ($stage+'/'+$stage+'-verification.json')))})}
}
$sealPath=Join-Path $artifactRoot 'candidate-builds.json';NewJson $sealPath @{schema='pas.owner-wake-build-binding.v1';source_commit=$commit;source_manifest=(Binding $sourcePath);builds=$builds;tests=$rows;source_encoding_joins=$encodingJoins;inherited_cold=(Binding $inherited);changed_scope='new declared Tap pixel absence/reentry workload and R2 cost protocol; owner/producer deadline fixes retained; core observer/hook/owner algorithms unchanged canonical source verified';live_ready=$false;device_operations=0}
Write-Output $sealPath
