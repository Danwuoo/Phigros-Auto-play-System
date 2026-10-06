param([ValidateSet('release','debug','asan')][string]$Mode='release',
 [ValidateSet('build','tests','pixels','audit')][string]$Operation='build',
 [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$Attempt='01',
 [ValidatePattern('^[a-z0-9-]{1,24}$')][string]$BuildAttempt='01',
 [string]$ReviewTrace='', [int]$ExpectedNative=0)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out=Join-Path $repo 'out/windows-handoff'
$pkg=Join-Path $out 'hold-front-package-01'
$pwsh='C:\Users\wurre\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe'
$gate=Join-Path $out 'native-qualification-01/native-gate.json'
$tool=Join-Path $out 'msvc-isolated-probe-01/tool-snapshot.json'
$root=Join-Path $repo ('out/hold-front-'+$Mode+'-'+$BuildAttempt)
function NewJson($path,$value) {
 $bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 35)+"`n")
 $s=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
 try{$s.Write($bytes);$s.Flush($true)}finally{$s.Dispose()}
}
function CheckEntry($f){
 if((Get-Item -LiteralPath $f.path).Length -ne $f.bytes -or (Get-FileHash -LiteralPath $f.path).Hash.ToLowerInvariant() -cne $f.sha256){throw ('input-sha '+$f.path)}
}
function Entry($path){$f=Get-Item -LiteralPath $path;@{path=$f.FullName;bytes=$f.Length;sha256=(Get-FileHash -LiteralPath $f.FullName).Hash.ToLowerInvariant()}}
function Receipt($stage,[int]$expected=0){
 $dir=Join-Path $out $stage
 $v=Get-Content -LiteralPath (Join-Path $dir ($stage+'-verification.json')) -Raw|ConvertFrom-Json
 $r=Get-Content -LiteralPath (Join-Path $dir ($stage+'-result.json')) -Raw|ConvertFrom-Json
 if($v.pending -or $v.native_exit -ne $expected -or $v.runner_exit -ne $expected -or $v.verification_exit -ne 0 -or
    $r.facts.active_final -ne 0 -or -not $r.facts.held_all_signaled -or -not $r.facts.streams_completed -or -not $r.facts.identity_trusted){throw ('receipt-not-verified '+$stage)}
 foreach($f in $v.entries){CheckEntry $f}
}
function Stage($stage,$script,$arguments,[int]$expected=0){
 $json=Join-Path $pkg ($stage+'-params.json')
 NewJson $json @{script=$script;arguments=$arguments}
 # Separate pwsh process prevents stale globals/LASTEXITCODE between owned jobs.
 & $pwsh -NoProfile -File (Join-Path $out 'withdrawal-review-01/stage.ps1') -Parameters $json
 if($LASTEXITCODE -ne 0){throw ('stage-wrapper '+$stage+' '+$LASTEXITCODE)}
 Receipt $stage $expected
}
if(-not(Test-Path $pkg)){throw 'capacity-start-package-required'}
if((Get-PSDrive C).Free -lt 21474836480){throw 'minimum-free'}
$buildStage='build-hold-front-'+$Mode+'-'+$BuildAttempt
$prior='build-bridge-withdraw-'+$Mode+'-01'
$oldRoot=Join-Path $repo ('out/bridge-win-withdraw-'+$Mode+'-01')
$closure=Join-Path $pkg ('closure-'+$Mode+'-'+$BuildAttempt+'.json')
$source=@(Get-ChildItem -LiteralPath $PSScriptRoot -File|ForEach-Object FullName)
if($Operation -eq 'build') {
 Receipt $prior
 $oldFreeze=Join-Path $out ($prior+'/freeze.json')
 $oldFiles=(Get-Content -LiteralPath $oldFreeze -Raw|ConvertFrom-Json).files
 foreach($f in $oldFiles){CheckEntry $f}
 $externalPath=Join-Path $repo 'docs/research/zero-miss-20261006/windows/withdrawal-acceptance-evidence-01/binaries-external.json'
 $external=Get-Content -LiteralPath $externalPath -Raw|ConvertFrom-Json
 $libs=@{}
 foreach($name in @('formal_core','exact_bvi_v3','current_bridge','pixel_diagnostics')){
  $path=Join-Path $oldRoot ($name+'.lib');$known=@($external|Where-Object path -eq $path)
  if($known.Count -ne 1){throw 'closure-external-manifest'};CheckEntry $known[0];$libs[$name]=Entry $path
 }
 $configuration=if($Mode -eq 'release'){'Release'}else{'Debug'}
 NewJson $closure @{schema='pas.verified-linked-front-closure.v1';configuration=$configuration;asan=($Mode -eq 'asan');
  libraries=$libs;source_commit='774f3e7020818e94c508cc1481dd0986ebe417a0';prior_build_stage=$prior;
  prior_build_freeze=(Entry $oldFreeze);external_binary_manifest=(Entry $externalPath);prior_source_entries_rechecked=$oldFiles.Count}
 $inputs=@($oldFiles.path|Where-Object {$_ -notmatch '-inputs\\.*\.cmd$'})+$source+@($closure,$externalPath)+@($libs.Values.path)
 $options=@('-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake',
  '-DVCPKG_INSTALLED_DIR=C:/Users/wurre/Desktop/Phigros-Auto-play-System/out/vcpkg_installed',
  '-DVCPKG_MANIFEST_INSTALL=OFF','-DVCPKG_TARGET_TRIPLET=x64-windows',('-DPAS_CLOSURE_MANIFEST='+$closure))
 if($Mode -eq 'asan'){$options+=@('-DPAS_FRONT_ASAN=ON','-DPAS_ASAN_SUPPORT_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include')}
 foreach($op in @('Configure','Build')) {
  $stage=$op.ToLowerInvariant()+'-hold-front-'+$Mode+'-'+$BuildAttempt
  $args=@{SourceRoot=$PSScriptRoot;BuildRoot=$root;StageName=$stage;ToolSnapshot=$tool;GateReceipt=$gate;Operation=$op;
   Configuration=$configuration;Inputs=($inputs|Sort-Object -Unique);TotalSeconds=900}
  if($op -eq 'Configure'){$args.Options=$options}
  Stage $stage (Join-Path $repo 'tools/zero_miss_windows/cmake-stage.ps1') $args
 }
 exit 0
}
Receipt $buildStage
$freeze=Join-Path $out ($buildStage+'/freeze.json')
$buildInputs=(Get-Content -LiteralPath $freeze -Raw|ConvertFrom-Json).files
foreach($f in $buildInputs){CheckEntry $f}
$inputs=@($source+$freeze+$closure)+@(Get-ChildItem -LiteralPath $root -File|Where-Object Extension -in '.exe','.lib','.dll'|ForEach-Object FullName)
if($Operation -eq 'pixels') {
 $stage='pixels-hold-front-'+$Mode+'-'+$Attempt
 $args=@{StageName=$stage;BuildStage=$buildStage;BuildRoot=$root;Selection=(Join-Path $out 'real-pixels-selection-02/selection.json');
  Report=(Join-Path $pkg ('pixels-'+$Mode+'-'+$Attempt+'.json'));GateReceipt=$gate;TotalSeconds=900}
 Stage $stage (Join-Path $repo 'tools/zero_miss_windows/pixel-stage.ps1') $args
 exit 0
}
if($Operation -eq 'tests') {
 foreach($kind in @('front','prefix')) {
  $stage=$kind+'-hold-front-'+$Mode+'-'+$Attempt
  $exe=if($kind -eq 'front'){'front_tests.exe'}else{'prefix_preservation.exe'}
  Stage $stage (Join-Path $repo 'tools/zero_miss_windows/native-stage.ps1') @{
    StageName=$stage;Executable=(Join-Path $root $exe);Arguments=@((Join-Path $pkg ($kind+'-'+$Mode+'-'+$Attempt+'.json')));
    GateReceipt=$gate;Inputs=$inputs;TotalSeconds=300}
 }
 exit 0
}
if($Operation -eq 'audit') {
 $selection=Join-Path $out 'real-pixels-selection-02/selection.json'
 $selected=Get-Content -LiteralPath $selection -Raw|ConvertFrom-Json
 $annotation=Join-Path $repo 'docs/research/zero-miss-20261006/windows/HUMAN_REVIEW_20261006.json'
 $reference=Join-Path $out 'pixels-withdraw-release-01.json.rows.jsonl'
 if(-not $ReviewTrace){$ReviewTrace=Join-Path $pkg ('pixels-'+$Mode+'-'+$Attempt+'.json.rows.jsonl')}
 # PNG leaves are already checked before/after pixel-stage. Recheck all for audit.
 foreach($f in $selected.frames){CheckEntry $f}
 $stage='audit-hold-front-'+$Mode+'-'+$Attempt
 $native=@{StageName=$stage;Executable=(Join-Path $root 'front_audit.exe');
  Arguments=@($reference,$ReviewTrace,$selection,$annotation,(Join-Path $pkg ('audit-'+$Mode+'-'+$Attempt+'.json')));
  GateReceipt=$gate;Inputs=($inputs+@($selection,$annotation,$reference,$ReviewTrace,$selected.index_path,(Join-Path $selected.record_root 'manifest.json')));
  TotalSeconds=600;ExpectedNative=$ExpectedNative}
 Stage $stage (Join-Path $repo 'tools/zero_miss_windows/native-stage.ps1') $native $ExpectedNative
 foreach($f in $selected.frames){CheckEntry $f}
 exit 0
}
