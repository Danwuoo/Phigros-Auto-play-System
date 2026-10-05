. "$PSScriptRoot/x2-budget.ps1"
if(Test-Path -LiteralPath $script:X2Batch){throw 'X2 batch must not exist'}
if(Test-Path -LiteralPath (Join-Path $script:X2Repo 'out/x2')){throw 'out/x2 must not exist'}
$campaignBefore=X2-Bytes $script:X2Campaign;$priorBefore=X2-Bytes $script:X2Prior
if($campaignBefore+$priorBefore+67108864 -gt 8589934592){throw 'Cannot reserve full X2 budget'}
$x1=Join-Path $script:X2Campaign 'contact-replay-x1'
$expected=@{'input-manifest.json'='99e8a364ea9f8575d67fbe39b1b76cccd6820816bc4d999d1b1bdfcfec2dd955';'five-cases-verified.json'='37d469caa0017be00fb78736fd69fb5cc87585c40b9fd671bb7e8397c420e7f7';'acceptance-r1-r2-20261002/acceptance-summary.json'='53338cb1b4f5910399d15366db2d661567e4fc834578f78145dc6eeb6bf7cb83'}
foreach($p in $expected.Keys){if((X2-Sha (Join-Path $x1 $p)) -cne $expected[$p]){throw "X1 hash mismatch $p"}}
$tracked=@(git -C $script:X2Repo ls-files -m);$untracked=@(git -C $script:X2Repo ls-files --others --exclude-standard)
$before=@();foreach($p in @($tracked+$untracked)){if(Test-Path -LiteralPath (Join-Path $script:X2Repo $p) -PathType Leaf){$before+=@{path=$p;sha256=(X2-Sha (Join-Path $script:X2Repo $p))}}}
$protected=@();foreach($root in @($x1,(Join-Path $script:X2Repo 'out/x1/c36h-v3'),(Join-Path $script:X2Repo 'out/x1/main50-v2'),(Join-Path $script:X2Repo 'out/x1/verified-tools36'),(Join-Path $script:X2Repo 'out/x1/verified-tools50'),(Join-Path $script:X2Repo 'out/x1/repair-tools36'),(Join-Path $script:X2Repo 'out/x1/repair-tools50'))){foreach($f in Get-ChildItem -LiteralPath $root -File -Recurse){$protected+=@{path=$f.FullName;bytes=$f.Length;sha256=(X2-Sha $f.FullName)}}}
$formal=@();foreach($f in Get-ChildItem (Join-Path $script:X2Repo 'src'),(Join-Path $script:X2Repo 'include') -File -Recurse){$formal+=@{path=$f.FullName;sha256=(X2-Sha $f.FullName)}}
X2-Json (Join-Path $script:X2Batch 'workspace-before.json') @{head=(git rev-parse HEAD);branch=(git branch --show-current);worktrees=@(git worktree list);status=@(git status --short);strategy=(Get-Content include/pas/strategy_version.hpp -Raw);uncommitted_files=$before;protected=$protected;formal=$formal;campaign_before_bytes=$campaignBefore;prior_before_bytes=$priorBefore;x1_before_bytes=(X2-Bytes $x1);x2_limit_bytes=67108864;estimate_five_runs_and_reports_bytes=48000000;live_rounds=0}
X2-Write (Join-Path $script:X2Batch 'status-before-append.md') ([IO.File]::ReadAllText((Join-Path $script:X2Repo 'docs/PROJECT_STATUS_NEXT_STEPS_20261001.md')))
Write-Output "Reserved X2 64MiB, campaign=$campaignBefore prior=$priorBefore; X1 protected."
