$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue"
$batchPath="$campaignPath/runtime-x11-p-r1";$outPath="$repoPath/out/x11-p-r1"
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$v){[IO.File]::WriteAllText("$batchPath/$name",($v|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))}
if((Test-Path $batchPath) -or (Test-Path $outPath)){throw 'new roots required'}
$total=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001");$free=(Get-PSDrive C).Free
if($total+128MB -gt 8GB -or $free -lt 3GB+128MB+5GB){throw 'reserve failed'}
New-Item -ItemType Directory $batchPath,$outPath|Out-Null
$dirty=@();foreach($p in @((git diff --name-only HEAD),(git ls-files --others --exclude-standard))|ForEach-Object {$_}){if(Test-Path -LiteralPath "$repoPath/$p" -PathType Leaf){$dirty+=@{path=$p;sha256=(Hash "$repoPath/$p")}}}
Save 'workspace-before.json' @{head=(git rev-parse HEAD);branch=(git branch --show-current);worktrees=@(git worktree list);status=@(git status --short);dirty_files=$dirty;strategy=(Get-Content "$repoPath/include/pas/strategy_version.hpp" -Raw);campaign_plus_prior_bytes=$total;free_bytes=$free}
Copy-Item "$repoPath/docs/RUNTIME_X11_P_R1_PROTOCOL_20261003.md" "$batchPath/protocol-before.md"
Copy-Item "$repoPath/docs/PROJECT_STATUS_NEXT_STEPS_20261001.md" "$batchPath/status-before.md"
Copy-Item "$repoPath/docs/C36H_FORWARD_EXECUTION_PLAN_20261003.md" "$batchPath/forward-before.md"
$checked=@();foreach($name in @('source-binding-before-cost.json','compiled-dependency-freeze.json')){
 $binding=Get-Content "$campaignPath/runtime-x11-p/$name" -Raw|ConvertFrom-Json
 foreach($f in $binding.files){if((Hash $f.path) -ne $f.sha256){throw "old freeze altered $($f.path)"};$checked+=@{path=$f.path;sha256=$f.sha256;bytes=(Get-Item $f.path).Length}}
}
Save 'old-binding-verified.json' @{files=$checked;scope='actual old source/binary and compiled dependencies; raw final ledger SHA reference';old_final_sha256=(Hash "$campaignPath/runtime-x11-p/final-summary.json");old_artifact_ledger_sha256=(Hash "$campaignPath/runtime-x11-p/artifact-ledger.json")}
foreach($r in @('B0','B1')){
New-Item -ItemType Directory "$outPath/$r"|Out-Null
 foreach($entry in Get-ChildItem "$repoPath/out/x11-p/$r" -Force|Where-Object Name -ne 'measurements'){Copy-Item -LiteralPath $entry.FullName -Destination "$outPath/$r" -Recurse}
 New-Item -ItemType Directory "$outPath/$r/measurements"|Out-Null
 New-Item -ItemType Junction -Path "$outPath/$r/measurements/outline-contact-20260927" -Target "$repoPath/measurements/outline-contact-20260927"|Out-Null
 $path="$outPath/$r/CMakeLists.txt";$s=[IO.File]::ReadAllText($path).Replace("`r`n","`n").Replace("C36h-tint1-X11-P-$r","C36h-tint1-X11-P-R1-$r")
 [IO.File]::WriteAllText($path,$s,[Text.UTF8Encoding]::new($false))
 $manualPath="$outPath/$r/src/manual_session.cpp";$m=[IO.File]::ReadAllText($manualPath).Replace("`r`n","`n")
 $b=$m.IndexOf('class Wake {');$e=$m.IndexOf('struct Packet :',$b);if($b -lt 0 -or $e -lt 0){throw 'wake anchors'}
 $wake=$m.Substring($b,$e-$b)
 [IO.File]::WriteAllText("$outPath/$r/include/pas/action_wake.hpp","#pragma once`n#include `"pas/core.hpp`"`n#include <algorithm>`n#define NOMINMAX`n#define WIN32_LEAN_AND_MEAN`n#include <windows.h>`nnamespace pas {`n"+$wake+"inline Nanoseconds action_wait_delay(std::optional<Nanoseconds> due,Nanoseconds now) {`n return due?std::clamp(*due-now,Nanoseconds{0},Nanoseconds{10'000'000}):10'000'000;`n}`n}`n",[Text.UTF8Encoding]::new($false))
 $m=$m.Remove($b,$e-$b)
 $m="#include `"pas/action_wake.hpp`"`n#include `"pas/session_events.hpp`"`n"+$m
 $b=$m.IndexOf('json release_json(');$e=$m.IndexOf("`n}`n}",$b);if($b -lt 0 -or $e -lt 0){throw 'release anchors'}
 $m=$m.Remove($b,$e+2-$b)
 $b=$m.IndexOf('                    ++commands;record({{"event","game_touch_receipt"}');$endMarker='{"real_input",true}});';$e=$m.IndexOf($endMarker,$b)+$endMarker.Length-1;if($b -lt 0 -or $e -lt 0){throw 'receipt anchors'}
 $m=$m.Remove($b,$e+1-$b).Insert($b,'                    ++commands;record(receipt_json(r,true));')
 $b=$m.IndexOf('                                json steps=json::array();');$e=$m.IndexOf("`n                            }",$b);if($b -lt 0 -or $e -lt 0){throw 'plan anchors'}
 $m=$m.Remove($b,$e-$b).Insert($b,'                                record(plan_json(plan,clock.now_ns(),true));')
 $m=$m.Replace("wake.wait(due?std::clamp(*due-clock.now_ns(),Nanoseconds{0},Nanoseconds{10'000'000}):10'000'000);",'wake.wait(action_wait_delay(due,clock.now_ns()));')
 [IO.File]::WriteAllText($manualPath,$m,[Text.UTF8Encoding]::new($false))
 Copy-Item "$repoPath/apps/runtime_x11_p_r1/session_events.hpp" "$outPath/$r/include/pas/session_events.hpp"
 Copy-Item "$repoPath/apps/runtime_x11_p_r1/session_archive.hpp" "$outPath/$r/include/pas/session_archive.hpp"
 Copy-Item "$repoPath/apps/runtime_x11_p_r1/session_archive.cpp" "$outPath/$r/src/session_archive.cpp"
 Copy-Item "$repoPath/apps/runtime_x11_p_r1/x11.cmake" "$outPath/$r/x11.cmake"
 & git diff --no-index -- "$repoPath/out/x11-p/$r" "$outPath/$r" *> "$batchPath/old-to-r1-$r.patch"
}
Copy-Item "$campaignPath/runtime-x11-p/profile-parent.json" "$batchPath/profile-parent.json"
Copy-Item "$campaignPath/runtime-x11-p/x12-profile-preview.json" "$batchPath/x12-profile-preview.json"
Copy-Item "$campaignPath/runtime-x11-p/capability-reference.json" "$batchPath/capability-reference.json"
Save 'capacity-before.json' @{batch=Bytes $batchPath;out=Bytes $outPath;campaign_plus_prior=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001");free=(Get-PSDrive C).Free;batch_limit=128MB;out_limit=3GB;campaign_limit=8GB;reserve=5GB}
