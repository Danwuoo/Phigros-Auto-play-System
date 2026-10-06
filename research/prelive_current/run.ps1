param([ValidateSet('release','debug','asan')][string]$Mode='release',
 [ValidateSet('build','tests','pixels','pipeline','analysis','bindings')][string]$Operation='build',
 [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$BuildAttempt='01',
 [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$Attempt='01',
 [string]$Selection='', [ValidateSet('A','B')][string]$Variant='B',
 [ValidateSet('tap','hold','dense','slow','writer','rpc','fault')][string]$Scene='tap',
 [ValidateRange(1000,10000)][int]$Frames=1000,[int]$ExpectedNative=0,
 [ValidateSet('aa','run')][string]$Analysis='run',[string]$AnalysisInput='',
 [string]$ClipRoot='C:/Users/wurre/Desktop/Phigros-Auto-play-System/measurements/game-assist/manual-session-3906621440800/pixel-clips',
 [string]$CoreDonor='')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out=Join-Path $repo 'out/windows-handoff'
$pkg=Join-Path $repo 'out/prelive-20261006'
$pwsh='C:\Users\wurre\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe'
$gate=Join-Path $out 'native-qualification-01/native-gate.json'
$tool=Join-Path $out 'msvc-isolated-probe-01/tool-snapshot.json'
$root=Join-Path $repo ('out/prelive-current-'+$Mode+'-'+$BuildAttempt)
function NewJson($path,$value){$bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 40)+"`n");$s=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read);try{$s.Write($bytes);$s.Flush($true)}finally{$s.Dispose()}}
function CheckEntry($f){if((Get-Item -LiteralPath $f.path).Length -ne $f.bytes -or (Get-FileHash -LiteralPath $f.path).Hash.ToLowerInvariant() -cne $f.sha256){throw ('input-sha '+$f.path)}}
function Capacity($stage,$phase){$roots=@(Get-ChildItem -LiteralPath (Join-Path $repo 'out') -Directory|Where-Object Name -like 'prelive*')+@(Get-ChildItem -LiteralPath $out -Directory|Where-Object Name -like '*prelive*');$rows=@($roots|ForEach-Object {$sum=(Get-ChildItem -LiteralPath $_.FullName -File -Recurse|Measure-Object Length -Sum).Sum;@{path=$_.FullName;bytes=[long]$sum}});$total=[long](($rows|Measure-Object bytes -Sum).Sum);$free=[long](Get-PSDrive C).Free;$reserve=if($phase -eq 'before' -and $Operation -in @('pipeline','pixels')){1073741824}elseif($phase -eq 'before' -and $Operation -eq 'build'){if($CoreDonor){536870912}else{1610612736}}else{0};$valid=$total+$reserve -le 12884901888 -and $free-$reserve -ge 21474836480;NewJson (Join-Path $pkg ($stage+'-capacity-'+$phase+'.json')) @{schema='pas.prelive-capacity.v1';stage=$stage;phase=$phase;roots=$rows;bytes=$total;reserve=$reserve;free=$free;cap=12884901888;minimum_free=21474836480;pass=$valid;metadata_seal_cap=67108864};if(-not $valid){throw 'capacity-gate'}}
function Receipt($stage){$dir=Join-Path $out $stage;$v=Get-Content -LiteralPath (Join-Path $dir ($stage+'-verification.json')) -Raw|ConvertFrom-Json;$r=Get-Content -LiteralPath (Join-Path $dir ($stage+'-result.json')) -Raw|ConvertFrom-Json;
 if($v.pending -or $v.native_exit -ne $ExpectedNative -or $v.runner_exit -ne $ExpectedNative -or $v.verification_exit -ne 0 -or $r.facts.active_final -ne 0 -or -not $r.facts.held_all_signaled -or -not $r.facts.streams_completed -or -not $r.facts.identity_trusted){throw ('receipt-not-verified '+$stage)};foreach($f in $v.entries){CheckEntry $f}}
function Stage($stage,$script,$arguments){Capacity $stage 'before';$json=Join-Path $pkg ($stage+'-params.json');NewJson $json @{script=$script;arguments=$arguments};try{& $pwsh -NoProfile -File (Join-Path $out 'withdrawal-review-01/stage.ps1') -Parameters $json;if($LASTEXITCODE -ne 0){throw ('stage-wrapper '+$stage+' '+$LASTEXITCODE)};Receipt $stage}finally{Capacity $stage 'after'}}
if(-not(Test-Path (Join-Path $pkg 'initial-audit.json'))){throw 'initial-audit-required'}
if((Get-PSDrive C).Free -lt 21474836480){throw 'minimum-free'}
$buildStage='build-prelive-'+$Mode+'-'+$BuildAttempt
$source=@(Get-ChildItem -LiteralPath $PSScriptRoot -File|ForEach-Object FullName)
if($Operation -eq 'build'){
 $oldFreeze=Join-Path $out ('build-bridge-withdraw-'+$Mode+'-01/freeze.json')
 $oldFiles=(Get-Content -LiteralPath $oldFreeze -Raw|ConvertFrom-Json).files
 $changed=(Get-Content -LiteralPath (Join-Path $pkg 'formal-source-before.json') -Raw|ConvertFrom-Json)
 foreach($f in $oldFiles){if($f.path -in $changed.path){$prior=@($changed|Where-Object path -eq $f.path)[0];if($prior.sha256 -cne $f.sha256){throw 'before-source-freeze'}}else{CheckEntry $f}}
 $inputs=@($oldFiles.path|Where-Object {$_ -notmatch '-inputs\\.*\.cmd$'})+$source+@(Join-Path $pkg 'initial-audit.json')+@(Join-Path $pkg 'formal-source-before.json')+@(Join-Path $repo 'docs/research/zero-miss-20261006/windows/PRELIVE_CURRENT_COST_PROTOCOL.md')+@(Get-ChildItem -LiteralPath (Join-Path $repo 'tests') -File|ForEach-Object FullName)+@(Get-ChildItem -LiteralPath (Join-Path $repo 'include') -File -Recurse|ForEach-Object FullName)+@(Join-Path $repo 'src/runtime.cpp')+@(Join-Path $repo 'proto/emulator_controller.proto')+@(Get-ChildItem -LiteralPath (Join-Path $repo 'out/win-r4-ninja-release-01/generated') -File|ForEach-Object FullName)
 $options=@('-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake','-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed','-DVCPKG_MANIFEST_INSTALL=OFF','-DVCPKG_TARGET_TRIPLET=x64-windows')
 if($CoreDonor){$CoreDonor=[IO.Path]::GetFullPath($CoreDonor);$donorName=Split-Path $CoreDonor -Leaf;if($donorName -notmatch ('^prelive-current-'+$Mode+'-([a-z0-9-]+)$')){throw 'donor-mode'};$donorStage='build-prelive-'+$Mode+'-'+$Matches[1];Receipt $donorStage;$donorFreeze=Join-Path $out ($donorStage+'/freeze.json');$donorFiles=(Get-Content -LiteralPath $donorFreeze -Raw|ConvertFrom-Json).files;foreach($f in $donorFiles){if($f.path.StartsWith((Join-Path $repo 'src'),[StringComparison]::OrdinalIgnoreCase) -or $f.path.StartsWith((Join-Path $repo 'include'),[StringComparison]::OrdinalIgnoreCase) -or $f.path -eq (Join-Path $repo 'CMakeLists.txt')){CheckEntry $f}};$deps=Join-Path $out ('bindings-deps-prelive-'+$Mode+'-final/'+ 'bindings-deps-prelive-'+$Mode+'-final.stdout.log');$previousRoot=if($Mode -eq 'release'){'prelive-current-release-10'}elseif($Mode -eq 'debug'){'prelive-current-debug-03'}else{'prelive-current-asan-02'};$extraDeps=@();foreach($line in Get-Content -LiteralPath $deps){if($line -match '^    (.+)$'){$relative=$Matches[1];$path=[IO.Path]::GetFullPath((Join-Path (Join-Path $repo ('out/'+$previousRoot)) $relative));if(Test-Path -LiteralPath $path -PathType Leaf){$extraDeps+=@($path)}}};$donorBinding=Join-Path $pkg ('core-donor-'+$Mode+'-'+$BuildAttempt+'.json');$extraFiles=@($extraDeps|Sort-Object -Unique|ForEach-Object {@{path=$_;bytes=(Get-Item -LiteralPath $_).Length;sha256=(Get-FileHash -LiteralPath $_).Hash.ToLowerInvariant()}});NewJson $donorBinding @{donor=$CoreDonor;files=$extraFiles;formal_sources=$donorFiles|Where-Object {$_.path.StartsWith((Join-Path $repo 'src'),[StringComparison]::OrdinalIgnoreCase) -or $_.path.StartsWith((Join-Path $repo 'include'),[StringComparison]::OrdinalIgnoreCase)}};$inputs+=@($donorBinding,$donorFreeze,(Join-Path $CoreDonor 'formal_core.lib'),(Join-Path $CoreDonor 'CMakeCache.txt'),(Join-Path $CoreDonor 'build.ninja'),$deps);$options+=@('-DPAS_PRELIVE_CORE_DONOR='+$CoreDonor)}
 if($Mode -eq 'asan'){$options+=@('-DPAS_PRELIVE_ASAN=ON','-DPAS_ASAN_SUPPORT_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')}
 $configuration=if($Mode -eq 'release'){'Release'}else{'Debug'}
 foreach($op in @('Configure','Build')){$stage=$op.ToLowerInvariant()+'-prelive-'+$Mode+'-'+$BuildAttempt;$args=@{SourceRoot=$PSScriptRoot;BuildRoot=$root;StageName=$stage;ToolSnapshot=$tool;GateReceipt=$gate;Operation=$op;Configuration=$configuration;Inputs=($inputs|Sort-Object -Unique);TotalSeconds=900};if($op -eq 'Configure'){$args.Options=$options};Stage $stage (Join-Path $PSScriptRoot 'cmake-stage.ps1') $args}
 if($CoreDonor){foreach($f in $extraFiles){CheckEntry $f}}
 exit 0
}
Receipt $buildStage
$freeze=Join-Path $out ($buildStage+'/freeze.json');$files=(Get-Content -LiteralPath $freeze -Raw|ConvertFrom-Json).files;foreach($f in $files){CheckEntry $f}
$inputs=@($source+$freeze)+@(Get-ChildItem -LiteralPath $root -File|Where-Object Extension -in '.exe','.lib','.dll'|ForEach-Object FullName)
$inputs+=@(Join-Path $root 'build.ninja')+@(Join-Path $root 'CMakeCache.txt')+@(Join-Path $root 'runtime_contract.cpp')+@(Join-Path $repo 'docs/research/zero-miss-20261006/windows/PRELIVE_CURRENT_COST_PROTOCOL.md')
if($Operation -eq 'bindings'){
 foreach($kind in @('deps','commands')){$stage='bindings-'+$kind+'-prelive-'+$Mode+'-'+$Attempt;Stage $stage (Join-Path $repo 'tools/zero_miss_windows/native-stage.ps1') @{StageName=$stage;Executable='C:/Users/wurre/AppData/Local/Programs/Python/Python310/Scripts/ninja.exe';Arguments=@('-C',$root,'-t',$kind);GateReceipt=$gate;Inputs=$inputs;TotalSeconds=300;ExpectedNative=0}}
 exit 0
}
if($Operation -eq 'analysis'){
 if(-not $AnalysisInput){throw 'analysis-input-required'};$j=Get-Content -LiteralPath $AnalysisInput -Raw|ConvertFrom-Json
 $inputs+=@($AnalysisInput);if($j.runs){$inputs+=@($j.runs.path)}elseif($j.path){$inputs+=@($j.path)}
 $stage='analysis-prelive-'+$Mode+'-'+$Attempt;Stage $stage (Join-Path $repo 'tools/zero_miss_windows/native-stage.ps1') @{StageName=$stage;Executable=(Join-Path $root 'cost_analysis.exe');Arguments=@($Analysis,$AnalysisInput,(Join-Path $pkg ('analysis-'+$Mode+'-'+$Attempt+'.json')));GateReceipt=$gate;Inputs=$inputs;TotalSeconds=300;ExpectedNative=0};exit 0
}
if($Operation -eq 'tests'){
 $clipIndex=Join-Path $ClipRoot 'index.jsonl';$clipFiles=@(Get-Content -LiteralPath $clipIndex|Where-Object {$_}|ForEach-Object {$_|ConvertFrom-Json});foreach($c in $clipFiles){$p=Join-Path $ClipRoot $c.path;if((Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant() -cne $c.sha256){throw 'clip-input-sha'}}
 $inputs+=@($clipIndex)+@($clipFiles|ForEach-Object {Join-Path $ClipRoot $_.path});$env:PAS_RGB_CLIP_ROOT=$ClipRoot
 foreach($kind in @('prefix_preservation','front_preservation','own_prefix','current_tests','formal_affected')){$stage=$kind.Replace('_','-')+'-prelive-'+$Mode+'-'+$Attempt;$nativeArgs=if($kind -eq 'formal_affected'){@('--gtest_output=json:'+(Join-Path $pkg ($kind+'-'+$Mode+'-'+$Attempt+'.json')))}else{@((Join-Path $pkg ($kind+'-'+$Mode+'-'+$Attempt+'.json')))};Stage $stage (Join-Path $repo 'tools/zero_miss_windows/native-stage.ps1') @{StageName=$stage;Executable=(Join-Path $root ($kind+'.exe'));Arguments=$nativeArgs;GateReceipt=$gate;Inputs=$inputs;TotalSeconds=300;ExpectedNative=$ExpectedNative}}
 foreach($c in $clipFiles){$p=Join-Path $ClipRoot $c.path;if((Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant() -cne $c.sha256){throw 'clip-input-sha-after'}}
 exit 0
}
if($Operation -eq 'pixels'){
 if(-not $Selection){$Selection=Join-Path $out 'real-pixels-selection-02/selection.json'}
 $selected=Get-Content -LiteralPath $Selection -Raw|ConvertFrom-Json
 foreach($f in $selected.frames){CheckEntry $f}
 $inputs+=@($Selection,$selected.index_path,(Join-Path $selected.record_root 'manifest.json'))
 $stage='pixels-prelive-'+$Mode+'-'+$Attempt
 $arguments=@('pixels',$Selection,(Join-Path $pkg ('pixels-'+$Mode+'-'+$Attempt+'.json')))
}else{
 $stage='pipeline-prelive-'+$Mode+'-'+$Attempt
 $arguments=@('pipeline',$Variant,$Scene,[string]$Frames,(Join-Path $pkg ('pipeline-'+$Mode+'-'+$Attempt+'.json')))
}
Stage $stage (Join-Path $repo 'tools/zero_miss_windows/native-stage.ps1') @{StageName=$stage;Executable=(Join-Path $root 'current_chain.exe');Arguments=$arguments;GateReceipt=$gate;Inputs=$inputs;TotalSeconds=900;ExpectedNative=$ExpectedNative}
if($Operation -eq 'pixels'){foreach($f in $selected.frames){CheckEntry $f}}
