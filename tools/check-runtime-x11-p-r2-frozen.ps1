. "$PSScriptRoot/r2-common.ps1"
$oldPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p-r1"
$checks=@()
foreach($name in @('source-binding-before-cost.json','compiled-dependency-freeze.json','negative-tool-freeze.json','artifact-ledger.json')){
 $j=Get-Content -LiteralPath "$oldPath/$name" -Raw|ConvertFrom-Json
 foreach($f in $j.files){if((R2Hash $f.path) -ne $f.sha256 -or (Get-Item -LiteralPath $f.path).Length -ne $f.bytes){throw "old freeze mismatch $($f.path)"}}
 $checks+=@{name=$name;sha256=(R2Hash "$oldPath/$name");entries=$j.files.Count}
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
$hook=$hook.Replace("`r`n","`n")+"`n";$count=0
foreach($f in Get-ChildItem "$repoPath/out/x11-p-r1/B0/src","$repoPath/out/x11-p-r1/B0/include" -File -Recurse){
 $other=$f.FullName.Replace('\B0\','\B1\');$a=[IO.File]::ReadAllText($f.FullName).Replace("`r`n","`n");$b=[IO.File]::ReadAllText($other).Replace("`r`n","`n")
 if($f.Name -eq 'game.cpp'){if(!$b.Contains($hook)){throw 'missing hook'};$b=$b.Replace($hook,'')}
 if($a -cne $b){throw "inverse $other"};++$count
}
$before=Get-Content "$batchPath/workspace-before.json" -Raw|ConvertFrom-Json
foreach($f in $before.formal_files){if((R2Hash $f.path) -ne $f.sha256){throw 'formal changed'}}
foreach($f in $before.dirty_files){if((R2Hash "$repoPath/$($f.path)") -ne $f.sha256){throw "prior dirty changed $($f.path)"}}
R2Save 'old-freeze-verified-before-stress.json' @{checks=$checks;inverse_files=$count;formal_files_unchanged=$before.formal_files.Count;prior_dirty_unchanged=$before.dirty_files.Count;head=(git rev-parse HEAD);worktrees=@(git worktree list --porcelain);only_decision_delta='unchanged 8-line known absolute cursor0 pending missing hook';new_runtime_exe=$false;profile_reference="$oldPath/x12-profile-preview.json";profile_sha256=(R2Hash "$oldPath/x12-profile-preview.json")}
Write-Output "old freeze and inverse=$count verified"
