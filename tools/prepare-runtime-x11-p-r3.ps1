. "$PSScriptRoot/r3-common.ps1"
if((Test-Path -LiteralPath $batchPath) -or (Test-Path -LiteralPath $outPath)){throw 'new R3 roots required'}
$c=(R3Bytes $campaignPath)+(R3Bytes "$repoPath/measurements/research-next-20261001")
if($c -gt 8GB -or $c+2GB -gt 10GB -or (Get-PSDrive C).Free -lt 3GB+2GB+5GB){throw 'initial capacity reserve'}
New-Item -ItemType Directory $batchPath,$outPath | Out-Null
$formal=@();foreach($f in Get-ChildItem "$repoPath/src","$repoPath/include" -File -Recurse){$formal+=@{path=$f.FullName;bytes=$f.Length;sha256=(R3Hash $f.FullName)}}
$formal+=@{path="$repoPath/CMakeLists.txt";bytes=(Get-Item "$repoPath/CMakeLists.txt").Length;sha256=(R3Hash "$repoPath/CMakeLists.txt")}
$dirty=@();foreach($f in @((git ls-files -m);(git ls-files --others --exclude-standard))){if(Test-Path -LiteralPath "$repoPath/$f" -PathType Leaf){$dirty+=@{path=$f;bytes=(Get-Item -LiteralPath "$repoPath/$f").Length;sha256=(R3Hash "$repoPath/$f")}}}
foreach($n in @('PROJECT_STATUS_NEXT_STEPS_20261001.md','C36H_FORWARD_EXECUTION_PLAN_20261003.md')){Copy-Item -LiteralPath "$repoPath/docs/$n" -Destination "$batchPath/pre-delivery-$n"}
R3Save 'workspace-before.json' @{head=(git rev-parse HEAD);branch=(git branch --show-current);worktrees=@(git worktree list --porcelain);status=@(git status --short);formal_diff=@(git diff HEAD -- src include CMakeLists.txt);strategy=(Get-Content "$repoPath/include/pas/strategy_version.hpp" -Raw);formal_files=$formal;dirty_files=$dirty;capacity=(R3Capacity);initial_old_bytes=$c}
R3Save 'capacity-allocation-before-engineering.json' @{normal_run_limit_bytes=80MB;normal_run_max=24;normal_reserve_bytes=1920MB;engineering_stress_limit_bytes=96MB;controller_reserve_bytes=32MB;total_limit_bytes=2GB;build_limit_bytes=3GB;disk_free_additional_reserve_bytes=5GB;old_limit_bytes=8GB;combined_limit_bytes=10GB;raw_references_only=$true;failed_data_counts=$true;no_compression=$true;capacity=(R3Capacity)}
Write-Output "R3 initialized; prior=$c"
