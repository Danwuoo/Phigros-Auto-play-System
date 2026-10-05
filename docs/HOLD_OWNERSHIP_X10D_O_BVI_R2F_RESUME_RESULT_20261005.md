# R2F resume結果：交易預驗通過，owned-child控制失敗後STOP

2026-10-05 Asia/Taipei。新增授權由使用者「好，派送原 task」派回本chat，具體修訂已保存於新batch [scope-amendment](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f-resume/scope-amendment.md)。唯一新attempt `bvi-r2f-20261005-02`。**工程partial：完整共用file交易預驗通過，natural及nonzero真控制通過；owned-child非預期失敗，durable STOP，Release/configure/build/BVI冷驗全未執行。** 不簽execution recovery完成、BVI cold pass、family no-go或產品採用。

原R2F `-01` STOP、來源/失敗/收據與R2E STOP永久保留，R1 repair不reset。舊state SHA35fa5fe81fdc2d0e775bcd68c5da3376f9a58d296ff0b5a4ae6dc7e7d7da7513、舊final receipt SHA11f5f0255b0eb16bdd84301cefb8cb77c0142fe6df7e099ee865bf8d7697dfb9均重核一致。本包未縮小舊請求覆寫舊attempt，而依新授權使用research/x10d_o_bvi_r2f_resume、batch hold-ownership-x10d-o-bvi-r2f-resume、out/x10d-o-bvi-r2f-resume。

## 已完成的共用交易與預驗

[transaction.ps1](../research/x10d_o_bvi_r2f_resume/transaction.ps1)是正式run與fake facts預驗的共同檔案路徑，沒有CreateProcess或native interop。command/result/checkpoints/verification/logs預開ReadWrite、FileShare.Read，保持拒絕旁路writer；全SHA與JSON reader使用Read/FileShare.ReadWrite，容納現有writer而不放寬writer本身。report/identity是限定producer的shared reservation；只有VerifyProcess核root/job/streams完成後，才關共享holder並改開Read/FileShare.Read guard，拒絕後續writer，再核consumer/hash/readback。封口後不能再透過WriteTransaction寫；READY包括全部引用及verification SHA，由同入口CheckState核下一stage。這不是抵抗惡意外部OS寫入的安全沙箱。

同一路徑涵蓋CreateNew reserve→command→durable RUNNING→共享事實/checkpoint→result→process predicate→report/identity consumer→SHA/JSON→verification→READY及下一stage。保存故障後StopTransaction保留共享退出事實；STOP保存也失敗時RUNNING仍拒絕。若RUNNING保存之前失敗留下舊READY，預開command reservation由CheckLaunchState另行拒絕重用，不能重新授予launch。

原helper只跑一次25/25，Add-Type僅編譯owned interop，不是BVI build。完整交易oracle先凍結32 cases，兩輪限額用完：第一輪30/32，T10/T12的預期故障都正確拒絕且STOP/退出事實保住，但Windows錯誤文字與凍結oracle不符。保存首輪shared source/pretest及報告後，唯一預驗repair將closed writer、missing input正規化為固定storage分類；oracle未改。第二輪**32/32、0真CreateProcess**，含writer持有與關閉後相同bytes/SHA、真JSON reader、零/非零log、report及identity分享模式、缺失/pending/SHA突變、exited-save/closed-writer/state保存失敗、READY/STOP/重複/sealed拒絕、writer guard、容量已有資料/等cap/超1B/MB與MiB及exit/isError阻止外層mutation。合計64個declared case executions、62pass、2保留失敗，不能省略第一輪。

helper S06及第二輪T15–T18另shell共用consumer均拒絕，actual native0；fake PID/15s/cleanup資料只測consumer，不證明真Windows身份/清理。成功預驗common/transaction SHA與最終freeze相同。[transaction-round-2](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f-resume/transaction-round-2.json)、[transaction-cross-shell](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f-resume/transaction-cross-shell.json)保留逐case事實。

另在第一次真launch前的stage argv readback發現Stage參數取名`args`碰到PowerShell自動變數，13個argv空。這是作者錯誤，尚未寫RUNNING/消耗launch；保存初始prepare/run/contract/freeze/state到failures/preformal-empty-argv，改為argvVector，精確重建argv並重新封存。此更正沒有第三輪交易預驗、沒有改common/transaction或candidate。最終13stage＝3 controls＋10product，argv長度3/3/7/4/4/8/7/4/4/7/4/4/7；final contract SHA140c52a35a01f8e9488e49a269be053b8b03403431428115b5c874e541eb3175，freeze SHAc64f4bf13ad3d6c5c6fd9466ae2bc5343684537d69a7071413f0475eed7a05e9。正式control開始後source/runner未變。

容量修訂已落CapacityMath：actual/protection/disk錯誤hard STOP，僅未消耗RUNNING的估算拒絕另列PREFLIGHT_REJECTED。實際各stage用有根據的增量預留，控制393216B、configure/build維護1572864B、suite6291456B，out configure4MiB、build40MiB；ASan configure另加已pin兩DLL實際bytes。沒有再請整包40MiB，也沒有正式stage估算拒絕或修正（0次）；超1B/单位等是封閉純算術oracle，不授予launch。沒有藉縮小reserve掩蓋真正超額。

## 真控制與hard STOP

| Stage | native / runner / verification | 事實與判定 |
|---|---|---|
| natural | 0 / 0 / 0 | trusted root/member identity、自然active0、streams完成、收據SHA/readback及下一stage READY；native elapsed1.3355178s |
| nonzero | 7 / 7 / 0 | 同型可信自然收尾，保留預期7且完整收據鏈成立；elapsed1.4136123s |
| owned-child | **1 / 125 / 1** | root確實launched/resumed/exited；parent stderr為child-identity-missing，非預期native1；15.0089187s末仍child/conhost在job；owned termination後job active0，但held member退出檢查2錯誤；17.8670271s，STOP |
| configure/build/wrong-contact/suite Release、Debug、ASan | null / null / null | **10產品stage全未執行**；不得補skip/pass |

第三控制root PID18540／creation FILETIME134356515630144198，完整pwsh.exe image與QPC frequency10000000。child PID23708／creation134356515643328801，conhost PID24036／creation134356515649487689，兩者root-exit及quiescence-end PID/creation/image/membership前後均可信、active/listed/assigned2。child identity JSON最終存在且與job snapshot一致，parent-identity仍pending。parent stderr從control-parent.ps1:9拋child-identity-missing，native1不符合預期parent0。

清理記錄需分讀：Result.cleanup字串為cleanup_verified，after-owned-cleanup及final-before-close完整job accounting/list均0、streams_completed=true、無overflow。但errors包含held-member-not-exited:23708及:24036，held_members_exited只有root18540。因此**job active0及管線完成有證據，完整held process退出gate未通過**，不能把字串cleanup_verified當全部程序清理驗收；不倒填後來退出、也不把parent1當合法負控制。VerifyProcess先由errors拒絕process-integrity，未進report/identity正式consumer，未放行configure。

静態可疑點與已證事實分開：parent polling在`if(-not $v.pending){break}`前沒拒絕null JSON，而producer對shared檔先SetLength(0)再寫入，存在空檔readback導致提早break的可能。此次收據沒有記每次poll，不能確證當時是哪個讀取值/競爭造成parent缺identity。owned.cs在job active0後即以WaitForSingleObject(held,0)核退出，也可能遇終止通知與handle signal的短暫差；原始資料只確證兩次wait未退出，不確證更深平台原因。**本attempt不修poll或wait、不試第二次真native**，保留為下一包需另授權的控制工程缺口。

state STOP revision8、consumed3、known launches3、成功receipts2、next仍owned-child只保留失敗前位置。另一個維護shell用CheckLaunchState要求configure-release，實際state-blocked；沒有呼叫run來試啟動。所有freeze files在STOP後SHA仍一致。[owned-child-result](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f-resume/owned-child-result.json)、verification、8 checkpoints、兩identity、stdout/stderr、state及STOP-readback完整保存。

## BVI、closure與來源保護

R1 bvi.cpp/hpp逐bytes保持，原69＋20 cases、89×4＝356、22 supplemental、4 schema controls、1128 coverage rows／2392引用及contact adapter未改。candidate實際layer-case/assertion/supplemental/schema控制全0、sizeof/metadata/probes null。沒有C++ binary或新out，compiler/include/link/STL/SDK/CRT/WIC/BVI DLL closure未驗。Debug/ASan在正式freeze前只核安裝檔存在/依賴SHA；未啟動配置，不是通過或skip。

實際執行僅PowerShell controls与managed owned interop。native結果保存root/child/conhost image，contract pin pwsh/compiler等檔；managed/Roslyn及全部實際loaded system DLL closure沒有全面採集，不能拿存在/指紋當loaded closure。source/input/dependency/binary manifests與final ledger列確切hash及限制。

原保護重建R2D2134、R2E原2145，加R2E交付/controller與原R2F完整ledger/final/依賴等後**2251唯一檔／2442引用／0不符**，原545 Git paths（含前R2F13 external paths）完整保留；旧controller532 paths/dirty指紋另核。HEAD f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c、index260c49037641b2c8c44f40e828491778779223193d91cb7cd824c8534ef7e9e1保持，正式src/include/root CMake及原apps/tests未改。未執行旧settle/check回寫。

原R2F [geometry-interface-review](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f/geometry-interface-review.json)按SHA引用，沒有重做PNG/observer/full replay：52圖合法ROI/all-line adapter仍缺，front與held patch、Note朝向/運動方向/line法向不混用，full-prefix bank不當6份90ms空history來源。PNG已用0/2、剩2。Guard.attachment_query仍fake，不建立physical owner。owner接入/機會損失/成本/live仍未驗。

## 容量與交付

carry aggregate8284363856、development7450899、controller1299900、oldout374705。原R2F156209B先佔原40MiB，新resume最多41786831B，不重置額度；newout仍134217728且加aggregate/sharedout。controller剩7088708全保留，free每phase≥5704253440；reports各≤64KiB、maintenance logs≤2MiB，所有failed snapshots/source/reports計入，無刪舊資料/PNG副本。精確final bytes/SHA/self exclusions見[final-receipt](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f-resume/final-receipt.json)與artifact-ledger。

交總控獨立驗收即止，不自簽產品採用或另派task。保留C36h tint1 behavioural baseline、main50 donor50/27/11 live0、suppression OFF、P excluded、X12 not-ready及Chapter Legacy全曲IN Miss=0目標。未emulator/ADB/觸控/manual-session、runtime/cost/stress/full replay、模型、goal/automation、commit/push；IN驗收沒有新增證據。
