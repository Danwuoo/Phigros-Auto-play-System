$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$batchPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p"
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Text($p){[IO.File]::ReadAllText($p).Replace("`r`n","`n")}
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
$hook=$hook.Replace("`r`n","`n")+"`n"
$rows=@();$research=@()
foreach($f in Get-ChildItem "$repoPath/out/x11-p/B0/src","$repoPath/out/x11-p/B0/include" -File -Recurse){
 $path=$f.FullName.Substring(("$repoPath/out/x11-p/B0").Length+1).Replace('\','/')
 $a=Text $f.FullName;$b=Text "$repoPath/out/x11-p/B1/$path"
 $inverse=if($path -eq 'src/game.cpp'){$b.Replace($hook,'') -ceq $a}else{$a -ceq $b}
 if(!$inverse){throw "additional core decision change $path"}
 $rows+=@{path=$path;B0_sha256=(Hash $f.FullName);B1_sha256=(Hash "$repoPath/out/x11-p/B1/$path");inverse_hook_equal=$inverse}
 if(Test-Path "$repoPath/out/x10d-p/baseline/$path"){
  $old=Text "$repoPath/out/x10d-p/baseline/$path"
  if($old -cne $a){$research+=@{path=$path;difference='read-only X1/X10c instrumentation/getters removed; manual_session has common new metadata'}
   & git diff --no-index -- "$repoPath/out/x10d-p/baseline/$path" $f.FullName *> "$batchPath/research-removal-$($path.Replace('/','-')).patch"
  }
 }
}
$metadata=@();foreach($p in @('CMakeLists.txt','cmake/session_build_provenance.hpp.in','apps/pas/main.cpp','x11.cmake')){
 $a=Text "$repoPath/out/x11-p/B0/$p";$b=(Text "$repoPath/out/x11-p/B1/$p").Replace('C36h-tint1-X11-P-B1','C36h-tint1-X11-P-B0')
 if($a -cne $b){throw 'metadata asymmetry'};$metadata+=@{path=$p;equal_except_variant_label=$true}
}
$oldFreeze=Get-Content "$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/pending-cancel-x10d-p/source-binding-before-replay.json" -Raw|ConvertFrom-Json
$oldChecked=@();foreach($f in $oldFreeze.export_files){$p="$repoPath/out/x10d-p/$($f.role)/$($f.path)";if((Hash $p) -ne $f.sha256){throw 'old export altered'};$oldChecked+=@{path=$p;sha256=$f.sha256}}
foreach($f in $oldFreeze.binaries){$p="$repoPath/$($f.path)";if((Hash $p) -ne $f.sha256){throw 'old binary altered'};$oldChecked+=@{path=$p;sha256=$f.sha256}}
$result=@{core_files=$rows;common_metadata=$metadata;research_removed=$research;old_frozen_checked=$oldChecked;baseline_public_bridge_equal=((Hash "$batchPath/bridge-B0/public-events.jsonl") -eq (Hash "$batchPath/bridge-old-B0/public-events.jsonl"));candidate_public_bridge_equal=((Hash "$batchPath/bridge-B1/public-events.jsonl") -eq (Hash "$batchPath/bridge-old-B1/public-events.jsonl"));formal_checkout_diff=@(git diff --name-only HEAD -- src include CMakeLists.txt);original_live_unlisted_inputs='not retrospectively SHA-certified; saved full archive and preserved recovery patch establish reconstructable lineage, not original binary identity'}
if($result.formal_checkout_diff.Count){throw 'formal checkout touched'}
if(Test-Path "$batchPath/source-inverse-audit.json"){throw 'existing'}
[IO.File]::WriteAllText("$batchPath/source-inverse-audit.json",($result|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))
