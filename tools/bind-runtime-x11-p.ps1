$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$batchPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p"
$outPath="$repoPath/out/x11-p"
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Save($name,$value){if(Test-Path "$batchPath/$name"){throw 'existing binding'};[IO.File]::WriteAllText("$batchPath/$name",($value|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))}
$dumpbin='C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/dumpbin.exe'
$closure=@();$metadata=@()
foreach($role in @('B0','B1')){
 $release="$outPath/build-$role/Release"
 $bins="$outPath/runtime-$role";if(Test-Path $bins){throw 'new closure required'};New-Item -ItemType Directory $bins|Out-Null
 $todo=[Collections.Generic.Queue[string]]::new();$todo.Enqueue('pas.exe');$seen=@{};$rows=@()
 while($todo.Count){$name=$todo.Dequeue();if($seen.ContainsKey($name)){continue};$seen[$name]=$true
  $path="$release/$name"
  if(!(Test-Path -LiteralPath $path)){throw "runtime closure missing $path"}
  Copy-Item -LiteralPath $path -Destination $bins
  $dep=@(& $dumpbin /DEPENDENTS $path);if($LASTEXITCODE){throw 'dumpbin'}
  [IO.File]::WriteAllLines("$batchPath/import-$role-$name.txt",$dep)
  $dependencies=@();foreach($l in $dep){if($l -match '^\s+([^\s]+\.dll)\s*$'){$d=$Matches[1];$system=!(Test-Path -LiteralPath "$release/$d");if($system -and $d -notmatch '^(api-ms-|ext-ms-)' -and !(Test-Path -LiteralPath "$env:windir/System32/$d")){throw "missing OS import $d"};$dependencies+=@{name=$d;system=$system};if(!$system){$todo.Enqueue($d)}}}
  if($dependencies.name -match 'torch|c10'){throw 'model linked'}
  $rows+=@{name=$name;bytes=(Get-Item $path).Length;sha256=(Hash $path);dependencies=$dependencies}
 }
 $prov=& "$bins/pas.exe" x11-provenance|ConvertFrom-Json
 if($LASTEXITCODE -or $prov.variant -ne "C36h-tint1-X11-P-$role" -or $prov.parent_saved_commit -ne '98169a575bd7c3b7503849ceb4b91a934d50ce0f'){throw 'metadata'}
 foreach($p in $prov.compiled_source_sha256.PSObject.Properties){if((Hash "$outPath/$role/$($p.Name)") -ne $p.Value){throw 'compiled source hash'} }
 $metadata+=@{role=$role;runtime_path="$bins/pas.exe";sha256=(Hash "$bins/pas.exe");metadata=$prov}
 $closure+=@{role=$role;root=$bins;files=$rows}
}
Save 'runtime-closure.json' $closure
Save 'runtime-metadata-verified.json' $metadata
$files=@()
foreach($r in @('B0','B1')){
 foreach($root in @("$outPath/$r/src","$outPath/$r/include","$outPath/$r/apps/pas","$outPath/$r/proto","$outPath/$r/cmake","$outPath/$r/tests","$outPath/$r/third_party")){
  foreach($f in Get-ChildItem -LiteralPath $root -File -Recurse){$files+=@{path=$f.FullName;sha256=(Hash $f.FullName);bytes=$f.Length}}
 }
 foreach($p in @('CMakeLists.txt','x11.cmake','vcpkg.json','vcpkg-configuration.json')){$f=Get-Item "$outPath/$r/$p";$files+=@{path=$f.FullName;sha256=(Hash $f.FullName);bytes=$f.Length}}
 foreach($root in @("$outPath/build-$r/Release","$outPath/runtime-$r")){
  foreach($f in Get-ChildItem $root -File | Where-Object Extension -In '.exe','.dll','.lib'){$files+=@{path=$f.FullName;sha256=(Hash $f.FullName);bytes=$f.Length}}
 }
 foreach($p in @('CMakeCache.txt','generated/session_build_provenance.hpp','generated/emulator_controller.pb.cc','generated/emulator_controller.grpc.pb.cc')){$f=Get-Item "$outPath/build-$r/$p";$files+=@{path=$f.FullName;sha256=(Hash $f.FullName);bytes=$f.Length}}
}
foreach($root in @("$repoPath/apps/runtime_x11_p","$outPath/aux/Release","$outPath/aux-asan/Debug")){
 foreach($f in Get-ChildItem -LiteralPath $root -File -Recurse|Where-Object Extension -In '.cpp','.cmake','.txt','.exe','.dll'){$files+=@{path=$f.FullName;sha256=(Hash $f.FullName);bytes=$f.Length}}
}
foreach($p in @("$batchPath/profile-parent.json","$batchPath/capability-reference.json","$outPath/asan-core/Debug/pas_core.lib","$repoPath/out/vcpkg_installed/vcpkg/status")){$f=Get-Item $p;$files+=@{path=$f.FullName;sha256=(Hash $f.FullName);bytes=$f.Length}}
Save 'source-binding-before-cost.json' @{files=$files;full_replay_attempts=0;core_is_uninstrumented=$true;baseline_synthetic_bridge_equal=((Hash "$batchPath/bridge-B0/public-events.jsonl") -eq (Hash "$batchPath/bridge-old-B0/public-events.jsonl"));candidate_synthetic_bridge_equal=((Hash "$batchPath/bridge-B1/public-events.jsonl") -eq (Hash "$batchPath/bridge-old-B1/public-events.jsonl"));binding_scope='runtime/core/dependencies/compiled inputs/harness; scripts and reports in final ledger'}
Save 'environment.json' @{cpu=(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);os=(Get-CimInstance Win32_OperatingSystem|Select-Object Caption,Version,TotalVisibleMemorySize);timezone='Asia/Taipei';compiler='MSVC v145 14.51.36231 / cl 19.51.36256.0';generator='Visual Studio 18 2026; BuildTools 18.9.12105.275';dependency='fixed out/vcpkg_installed patched gRPC1.81.1; no reinstall';capture='offline synthetic memory RGB; live profile gRPC payload fast RGB888 top-down 256KiB';conditions='sequential QPC cost runs; no emulator/ADB/model; no priority/affinity override; OS background load unisolated'}
