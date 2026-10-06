param([ValidateSet('release','debug','asan')][string]$Mode='release',
 [ValidateSet('build','tests','pixels','pipeline')][string]$Operation='build',
 [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$BuildAttempt='01',
 [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$Attempt='01',
 [string]$Selection='', [ValidateSet('A','B')][string]$Variant='B',
 [ValidateSet('tap','hold','dense','slow','writer','rpc','fault')][string]$Scene='tap',
 [ValidateRange(1000,10000)][int]$Frames=1000,[int]$ExpectedNative=0)
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
function Receipt($stage){$dir=Join-Path $out $stage;$v=Get-Content -LiteralPath (Join-Path $dir ($stage+'-verification.json')) -Raw|ConvertFrom-Json;$r=Get-Content -LiteralPath (Join-Path $dir ($stage+'-result.json')) -Raw|ConvertFrom-Json;
 if($v.pending -or $v.native_exit -ne $ExpectedNative -or $v.runner_exit -ne $ExpectedNative -or $v.verification_exit -ne 0 -or $r.facts.active_final -ne 0 -or -not $r.facts.held_all_signaled -or -not $r.facts.streams_completed -or -not $r.facts.identity_trusted){throw ('receipt-not-verified '+$stage)};foreach($f in $v.entries){CheckEntry $f}}
function Stage($stage,$script,$arguments){$json=Join-Path $pkg ($stage+'-params.json');NewJson $json @{script=$script;arguments=$arguments}; & $pwsh -NoProfile -File (Join-Path $out 'withdrawal-review-01/stage.ps1') -Parameters $json;if($LASTEXITCODE -ne 0){throw ('stage-wrapper '+$stage+' '+$LASTEXITCODE)};Receipt $stage}
if(-not(Test-Path (Join-Path $pkg 'initial-audit.json'))){throw 'initial-audit-required'}
if((Get-PSDrive C).Free -lt 21474836480){throw 'minimum-free'}
$buildStage='build-prelive-'+$Mode+'-'+$BuildAttempt
$source=@(Get-ChildItem -LiteralPath $PSScriptRoot -File|ForEach-Object FullName)
if($Operation -eq 'build'){
 $oldFreeze=Join-Path $out ('build-bridge-withdraw-'+$Mode+'-01/freeze.json')
 $oldFiles=(Get-Content -LiteralPath $oldFreeze -Raw|ConvertFrom-Json).files
 foreach($f in $oldFiles){CheckEntry $f}
 $inputs=@($oldFiles.path|Where-Object {$_ -notmatch '-inputs\\.*\.cmd$'})+$source+@(Join-Path $pkg 'initial-audit.json')
 $options=@('-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake','-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed','-DVCPKG_MANIFEST_INSTALL=OFF','-DVCPKG_TARGET_TRIPLET=x64-windows')
 if($Mode -eq 'asan'){$options+=@('-DPAS_PRELIVE_ASAN=ON','-DPAS_ASAN_SUPPORT_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')}
 $configuration=if($Mode -eq 'release'){'Release'}else{'Debug'}
 foreach($op in @('Configure','Build')){$stage=$op.ToLowerInvariant()+'-prelive-'+$Mode+'-'+$BuildAttempt;$args=@{SourceRoot=$PSScriptRoot;BuildRoot=$root;StageName=$stage;ToolSnapshot=$tool;GateReceipt=$gate;Operation=$op;Configuration=$configuration;Inputs=($inputs|Sort-Object -Unique);TotalSeconds=900};if($op -eq 'Configure'){$args.Options=$options};Stage $stage (Join-Path $PSScriptRoot 'cmake-stage.ps1') $args}
 exit 0
}
Receipt $buildStage
$freeze=Join-Path $out ($buildStage+'/freeze.json');$files=(Get-Content -LiteralPath $freeze -Raw|ConvertFrom-Json).files;foreach($f in $files){CheckEntry $f}
$inputs=@($source+$freeze)+@(Get-ChildItem -LiteralPath $root -File|Where-Object Extension -in '.exe','.lib','.dll'|ForEach-Object FullName)
if($Operation -eq 'tests'){
 foreach($kind in @('prefix_preservation','front_preservation','own_prefix','current_tests')){$stage=$kind.Replace('_','-')+'-prelive-'+$Mode+'-'+$Attempt;Stage $stage (Join-Path $repo 'tools/zero_miss_windows/native-stage.ps1') @{StageName=$stage;Executable=(Join-Path $root ($kind+'.exe'));Arguments=@((Join-Path $pkg ($kind+'-'+$Mode+'-'+$Attempt+'.json')));GateReceipt=$gate;Inputs=$inputs;TotalSeconds=300;ExpectedNative=$ExpectedNative}}
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
