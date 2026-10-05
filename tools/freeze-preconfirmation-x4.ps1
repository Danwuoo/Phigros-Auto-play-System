. "$PSScriptRoot/x4-budget.ps1"
$tools=Join-Path $script:X4Repo 'out/x4/tools'
if(Test-Path -LiteralPath $tools){throw 'Frozen tools must be new'}
New-Item -ItemType Directory -Path $tools|Out-Null
foreach($name in 'pas_frame_review.exe','x1_tests.exe','x4_tool.exe','z.dll','gtest.dll','gtest_main.dll') {Copy-Item -LiteralPath (Join-Path $script:X4Repo "out/x4/build/Release/$name") -Destination $tools}
$binaries=@();foreach($f in Get-ChildItem -LiteralPath $tools -File){$binaries+=@{path=$f.FullName;bytes=$f.Length;sha256=(X4-Sha $f.FullName)}}
$sources=@()
foreach($f in Get-ChildItem -LiteralPath "$script:X4Repo/apps/frame_review","$script:X4Repo/tools" -File -Recurse) {
  if($f.FullName -like '*\tools\*' -and $f.Name -notlike '*x4*'){continue}
  $rel=$f.FullName.Substring($script:X4Repo.Length+1).Replace('\','/')
  $snapshot=Join-Path $script:X4Batch "tool-source/$rel"
  X4-Write $snapshot ([IO.File]::ReadAllText($f.FullName))
  $sources+=@{path=$rel;snapshot=$snapshot;sha256=(X4-Sha $f.FullName);snapshot_sha256=(X4-Sha $snapshot)}
}
foreach($rel in 'tests/x4_oracle_tests.cpp','tests/contact_replay_tests.cpp') {
  $snapshot=Join-Path $script:X4Batch "tool-source/$rel";X4-Write $snapshot ([IO.File]::ReadAllText((Join-Path $script:X4Repo $rel)))
  $sources+=@{path=$rel;snapshot=$snapshot;sha256=(X4-Sha (Join-Path $script:X4Repo $rel));snapshot_sha256=(X4-Sha $snapshot)}
}
$exports=@();foreach($f in Get-ChildItem "$script:X4Repo/out/x4/main50-provisional" -File -Recurse){$exports+=@{path=$f.FullName;bytes=$f.Length;sha256=(X4-Sha $f.FullName)}}
$different=@();foreach($f in Get-ChildItem "$script:X4Repo/out/x1/main50-v2" -File -Recurse){$rel=$f.FullName.Substring(("$script:X4Repo/out/x1/main50-v2").Length+1);$new=Join-Path "$script:X4Repo/out/x4/main50-provisional" $rel;if((X4-Sha $new) -ne (X4-Sha $f.FullName)){$different+=$rel.Replace('\','/')}}
if($different.Count -ne 1 -or $different[0] -ne 'src/game_tracking.cpp'){throw 'Unexpected export strategy change'}
X4-Json (Join-Path $script:X4Batch 'tool-freeze.json') @{source=$sources;binaries=$binaries;exports=$exports;export_only_changed_file=$different;formal_git_diff=@(git diff -- src include CMakeLists.txt);hook_default='off';X2_ablation_combined=$false;strategy='50/27/11';root_runtime_links_hook=$false}
Write-Output 'Frozen X4 tools/source; original exports preserved.'
