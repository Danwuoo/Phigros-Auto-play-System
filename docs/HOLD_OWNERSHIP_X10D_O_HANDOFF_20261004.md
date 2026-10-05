# X10d-O 交接：負結果，待總控

2026-10-04 Asia/Taipei。先讀 [protocol](HOLD_OWNERSHIP_X10D_O_PROTOCOL_20261004.md)及 [result](HOLD_OWNERSHIP_X10D_O_RESULT_20261004.md)，再看batch `final-receipt.json`、`delivery-binding.json`、`protection-after.json`、`capacity.json`。本包沒有可採用runtime candidate；BCC-v1在tail/incoming貼合反例錯給唯一owner，已停止family及full replay。C36h tint1 baseline／main50 donor/live0／suppression OFF／P與B不混入／X12 not-ready均維持，gameplay Unknown。

## 精確交付與限制

- `research/x10d_o/contract.hpp`、`reference.cpp`、`candidate.cpp`、`contract_tests.cpp`：先紅8項與唯一family，Release candidate10 pass/1 fail完整保存。reference adapter不是原observer的實作，不能把8個adapter fail說成已證C36h實景錯誤。
- `owner_controls.cpp`：8/8，未改C36h owner/scheduler/FakeTouch controls；沒有接resolver，不能簽為end-to-end ownership candidate。
- `packet_audit.cpp`及 `grounding.json`：196真PNG/clock/source-frame join、195 consumed/1原skip，七窗全部bank物件納入；10張直接目視，physical gold0，所有ownership仍null。
- 兩份新export runtime sources原樣複製，`runtime-policy.patch`空，沒有策略接線。compiled core只有reference lineage；candidate export是未改runtime容器，實驗candidate helper獨立連同一core。
- Release原207 tests pass，含27RGB。ASan build quiescence失敗，Debug tests0，不能以EXE存在或compiler stdout當ASan通過。單次執行工具修補後遇此停止線；沒有平台續修。
- 新full replay0，action分母及ON/OFF比較not_run/null，不報contacts_at_exit=0的假replay。原baseline ON/OFF exact bridge是重新hash歷史artifact，非重跑。

## 獨立驗收可重現入口

**現有batch/out已封存；禁止prepare/build/finalize重做。** 原tool scripts是實際執行紀錄，不是對本batch追加測試/容量或replay授權。交付SHA／snapshot先核再用。總控可在32MiB預留內另建review根，執行frozen Release binaries，所有XML/log使用新review路徑：

```powershell
$repoPath='C:/Users/wurre/Desktop/Phigros-Auto-play-System'
$batchPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o"
$reviewPath="$batchPath/controller-review" # 總控新增；本包未建立
$env:PAS_RGB_CLIP_ROOT="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
& "$repoPath/out/x10d-o/build/Release/original_tests.exe" --gtest_brief=1 "--gtest_output=xml:$reviewPath/original.xml"
& "$repoPath/out/x10d-o/build/Release/reference_contract.exe" --gtest_brief=1 "--gtest_output=xml:$reviewPath/reference-red.xml" # 預期exit1，3pass/8fail
& "$repoPath/out/x10d-o/build/Release/candidate_contract.exe" --gtest_brief=1 "--gtest_output=xml:$reviewPath/candidate-negative.xml" # 預期exit1，10pass/1fail
& "$repoPath/out/x10d-o/build/Release/owner_controls.exe" --gtest_brief=1 "--gtest_output=xml:$reviewPath/owner.xml"
& "$repoPath/out/x10d-o/build/Release/packet_audit.exe" "$batchPath/rgb-packet.json" "$reviewPath/pixel-support-audit.json"
```

packet reader要求新output，重算JSON可與原完整相同；没有touch backend。確認所有failed assertions、red預聲明時序、single/touching render逐byte相同、helper未被接進runtime，以及原pre-existing435檔＋frozen/input保護scope。未知source render age、錄影前state、真tail/head語義、incoming是否physical新Note、同版完整IN／跨曲改善都不能因驗收工具結果而消除。

資料缺口與未做冷項在RESULT明列；本包不自行開第二family、ASan修補、B量測、live或新goal。僅等待總控獨立決定是否簽收此negative/partial工程交付及下一個工作包。
