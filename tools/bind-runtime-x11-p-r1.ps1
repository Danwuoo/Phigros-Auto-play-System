$ErrorActionPreference='Stop';$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$outPath="$repoPath/out/x11-p-r1";$batchPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-r1"
function Hash($p){(Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant()}
function Text($p){[IO.File]::ReadAllText($p).Replace("`r`n","`n")}
function Save($n,$v){if(Test-Path "$batchPath/$n"){throw 'existing freeze'};[IO.File]::WriteAllText("$batchPath/$n",($v|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))}
$hook=@'
        // A newer complete snapshot with no current Note must withdraw a
        // pending Down immediately. Missing grace applies only after a
        // contact has started; it cannot license a new touch from old pixels.
        if(identity.submitted) if(const auto cursor=scheduler_.executed_steps(identity.intent);
           cursor&&*cursor==0) {
            cancel_contact(id,identity,"pending_down_current_object_missing");
            continue;
        }
'@
$hook=$hook.Replace("`r`n","`n")+"`n";$core=@();$changes=@()
foreach($f in Get-ChildItem "$outPath/B0/src","$outPath/B0/include" -File -Recurse){
 $rel=$f.FullName.Substring(("$outPath/B0").Length+1).Replace('\','/');$a=Text $f.FullName;$b=Text "$outPath/B1/$rel"
 $same=if($rel -eq 'src/game.cpp'){$b.Replace($hook,'') -ceq $a}else{$b -ceq $a};if(!$same){throw "inverse failed $rel"}
 $core+=@{path=$rel;B0_sha256=Hash $f.FullName;B1_sha256=Hash "$outPath/B1/$rel";inverse_equal=$same}
 if(!(Test-Path "$repoPath/out/x11-p/B0/$rel") -or (Text "$repoPath/out/x11-p/B0/$rel") -cne $a){$changes+=@{path=$rel;old_sha256=if(Test-Path "$repoPath/out/x11-p/B0/$rel"){Hash "$repoPath/out/x11-p/B0/$rel"}else{$null};new_sha256=Hash $f.FullName}}
}
$oldManual=Text "$repoPath/out/x11-p/B0/src/manual_session.cpp";$b=$oldManual.IndexOf('class Wake {');$e=$oldManual.IndexOf('struct Packet :',$b)
if(!(Text "$outPath/B0/include/pas/action_wake.hpp").Contains($oldManual.Substring($b,$e-$b))){throw 'Wake extraction not exact'}
if($changes.path.Count -ne 5){throw 'unexpected common core changes'}
foreach($r in @('B0','B1')){if(!(Get-Content "$batchPath/bridge-$r/summary.json" -Raw|ConvertFrom-Json).comparison.equal){throw 'bridge'}}
Save 'source-inverse-audit.json' @{core_files=$core;common_changes=$changes;wake_original_class_exact=$true;only_decision_delta='8-line pending hook; known absolute cursor zero';suppression=$false;formal_checkout_diff=@(git diff --name-only HEAD -- src include CMakeLists.txt);old_live_unlisted_compiled_SHA='unknown';call_chain='manual_session and meter -> shared Wake/action_wait_delay/session_events -> SessionGameOwner -> GamePlanOwner/ContactScheduler; SessionArchive common compiled source'}
$dumpbin='C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/dumpbin.exe'
$closure=@();$metadata=@()
foreach($r in @('B0','B1')){
 $release="$outPath/build-$r/Release";$bins="$outPath/runtime-$r";if(Test-Path $bins){throw 'new closure required'};New-Item -ItemType Directory $bins|Out-Null
 $todo=[Collections.Generic.Queue[string]]::new();$todo.Enqueue('pas.exe');$seen=@{};$rows=@()
 while($todo.Count){$name=$todo.Dequeue();if($seen.ContainsKey($name)){continue};$seen[$name]=$true;$p="$release/$name";Copy-Item $p $bins
  $dep=@(& $dumpbin /DEPENDENTS $p);if($LASTEXITCODE){throw 'dumpbin'};[IO.File]::WriteAllLines("$batchPath/import-$r-$name.txt",$dep);$ds=@()
  foreach($l in $dep){if($l -match '^\s+([^\s]+\.dll)\s*$'){$d=$Matches[1];$sys=!(Test-Path "$release/$d");if($sys -and $d -notmatch '^(api-ms-|ext-ms-)' -and !(Test-Path "$env:windir/System32/$d")){throw "OS DLL absent $d"};if(!$sys){$todo.Enqueue($d)};$ds+=@{name=$d;system=$sys}}}
  if($ds.name -match 'torch|c10'){throw 'model dependency'};$rows+=@{name=$name;bytes=(Get-Item $p).Length;sha256=Hash $p;dependencies=$ds}
 }
 $prov=& "$bins/pas.exe" x11-provenance|ConvertFrom-Json;if($LASTEXITCODE -or $prov.variant -ne "C36h-tint1-X11-P-R1-$r"){throw 'provenance label'}
 foreach($p in $prov.compiled_source_sha256.PSObject.Properties){if((Hash "$outPath/$r/$($p.Name)") -ne $p.Value){throw "compiled source SHA $($p.Name)"}}
 $metadata+=@{role=$r;binary_sha256=Hash "$bins/pas.exe";metadata=$prov};$closure+=@{role=$r;files=$rows}
}
Save 'runtime-closure.json' $closure;Save 'runtime-metadata-verified.json' $metadata
$files=@();foreach($r in @('B0','B1')){
 foreach($dir in @('src','include','apps','proto','cmake','tests','third_party')){foreach($f in Get-ChildItem "$outPath/$r/$dir" -File -Recurse){$files+=@{path=$f.FullName;bytes=$f.Length;sha256=Hash $f.FullName}}}
 foreach($p in @('CMakeLists.txt','x11.cmake','vcpkg.json','vcpkg-configuration.json')){$f=Get-Item "$outPath/$r/$p";$files+=@{path=$f.FullName;bytes=$f.Length;sha256=Hash $f.FullName}}
 foreach($dir in @("$outPath/build-$r/Release","$outPath/runtime-$r")){foreach($f in Get-ChildItem $dir -File|Where-Object Extension -in '.exe','.dll','.lib'){$files+=@{path=$f.FullName;bytes=$f.Length;sha256=Hash $f.FullName}}}
 foreach($p in @('CMakeCache.txt','generated/session_build_provenance.hpp','generated/emulator_controller.pb.cc','generated/emulator_controller.grpc.pb.cc')){$f=Get-Item "$outPath/build-$r/$p";$files+=@{path=$f.FullName;bytes=$f.Length;sha256=Hash $f.FullName}}
}
foreach($dir in @("$repoPath/apps/runtime_x11_p_r1","$outPath/build-asan/Debug")){foreach($f in Get-ChildItem $dir -File -Recurse|Where-Object Extension -in '.cpp','.hpp','.inc','.cmake','.txt','.exe','.dll','.lib'){$files+=@{path=$f.FullName;bytes=$f.Length;sha256=Hash $f.FullName}}}
foreach($p in @("$repoPath/apps/runtime_x11_p/pending_tests.cpp","$batchPath/profile-parent.json","$batchPath/x12-profile-preview.json","$batchPath/capability-reference.json","$repoPath/out/vcpkg_installed/vcpkg/status")){$f=Get-Item $p;$files+=@{path=$f.FullName;bytes=$f.Length;sha256=Hash $f.FullName}}
Save 'environment.json' @{cpu=(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);os=(Get-CimInstance Win32_OperatingSystem|Select-Object Caption,Version,TotalVisibleMemorySize);timezone='Asia/Taipei';compiler='MSVC v145 14.51.36231 / cl19.51.36256';dependency='existing patched gRPC1.81.1/vcpkg; no installation';capture='synthetic memory; real profile fast/RGB888 top-down/256KiB unchanged';owner_options=@(15,35000000,30000000);conditions='OS background load unisolated; no priority/affinity/power override; builds finish before costs'}
Save 'source-binding-before-cost.json' @{files=$files;bridge_old_new_equal=$true;full_recorded_replay_runs=0;core_strategy_uninstrumented=$true;archive_optional_instrumentation=$true;B0B1_symmetric=$true;original_live_SHA='61ecfacd2cc48ed270506719a1089be646d915c64e9026706ac1654e0d4bbce9'}
