param([string]$Phase='before')
. "$PSScriptRoot/r3-common.ps1"
$checks=@();$exceptions=@()
foreach($batch in @('runtime-x11-p','runtime-x11-p-r1','runtime-x11-p-r2')) {
 $names=if($batch -eq 'runtime-x11-p-r2'){@('source-binding-before-stress.json','compiled-dependency-freeze.json','artifact-ledger.json')}elseif($batch -eq 'runtime-x11-p-r1'){@('source-binding-before-cost.json','compiled-dependency-freeze.json','negative-tool-freeze.json','artifact-ledger.json')}else{@('source-binding-before-cost.json','compiled-dependency-freeze.json')}
 foreach($name in $names){$path="$campaignPath/$batch/$name";$j=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json
  foreach($f in $j.files){$actual=$f.path
   if($batch -eq 'runtime-x11-p-r2' -and $name -eq 'artifact-ledger.json' -and ($actual.Replace('\','/') -in @("$repoPath/docs/PROJECT_STATUS_NEXT_STEPS_20261001.md".Replace('\','/'),"$repoPath/docs/C36H_FORWARD_EXECUTION_PLAN_20261003.md".Replace('\','/')))) {
    $actual="$campaignPath/runtime-x11-p-r2-controller/pre-review-$([IO.Path]::GetFileName($f.path))";$exceptions+=@{workspace_path=$f.path;historical_preimage=$actual;sha256=$f.sha256}
   }
   if((R3Hash $actual) -ne $f.sha256 -or (Get-Item -LiteralPath $actual).Length -ne $f.bytes){throw "old freeze mismatch $actual"}
  };$checks+=@{batch=$batch;name=$name;entries=$j.files.Count;sha256=(R3Hash $path)};Write-Output "$batch/$name verified=$($j.files.Count)"
 }
}
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
$hook=$hook.Replace("`r`n","`n")+"`n";$inverse=@()
foreach($f in Get-ChildItem "$repoPath/out/x11-p-r1/B0/src","$repoPath/out/x11-p-r1/B0/include" -File -Recurse){
 $p=$f.FullName.Replace('\B0\','\B1\');$a=[IO.File]::ReadAllText($f.FullName).Replace("`r`n","`n");$b=[IO.File]::ReadAllText($p).Replace("`r`n","`n")
 if($f.Name -eq 'game.cpp'){if(!$b.Contains($hook)){throw 'missing hook'};$b=$b.Replace($hook,'')};if($a -cne $b){throw "inverse $p"};$inverse+=@{B0=$f.FullName;B0_sha256=(R3Hash $f.FullName);B1=$p;B1_sha256=(R3Hash $p);inverse=$true}
}
$original=[IO.File]::ReadAllText("$repoPath/out/x11-p-r1/B0/src/session_archive.cpp")
if($original.Replace('16384','65536') -cne [IO.File]::ReadAllText("$repoPath/apps/runtime_x11_p_r3/session_archive.cpp")){throw 'archive has extra change'}
$before=Get-Content "$batchPath/workspace-before.json" -Raw|ConvertFrom-Json
foreach($f in $before.formal_files){if((R3Hash $f.path) -ne $f.sha256){throw "formal changed $($f.path)"}}
R3Save "old-freeze-verified-$Phase.json" @{checks=$checks;historical_exceptions=$exceptions;inverse=$inverse;archive_common_delta='only three literal16384->65536 replacements: reserve serialize/write and fail limit; same class/header/ABI';core_recompiled=$false;formal_unchanged=$before.formal_files.Count;original_live_compiled_SHA='Unknown';capacity=(R3Capacity)}
