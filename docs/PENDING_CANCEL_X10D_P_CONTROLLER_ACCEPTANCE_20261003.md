# X10d-P 總控獨立驗收

2026-10-03。**工程與冷候選生命週期契約驗收通過，允許進入X11成本／版本凍結準備；尚未採為產品baseline或證明遊戲改善。** X10b suppression仍OFF、否決狀態不變。X10d-O的body ownership是另一個研究問題，不以本驗收替它背書。

本次main HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、唯一registered worktree。保留原dirty；正式src/include/root CMake對HEAD無差異。新驗收只新增維護script、文件與獨立結果根，無build／完整replay／emulator／live／commit／push。

## 1. 實際獨立執行

原交付：[研究結果](PENDING_CANCEL_X10D_P_RESULT_20261003.md)、[預先protocol](PENDING_CANCEL_X10D_P_PROTOCOL_20261003.md)。總控讀取owner caller、missing與cancel_contact分支、scheduler cursor／cancel／同due排序、完整新測試、reader timing及三個audit程式；不是只讀自驗summary。

新證據根：`measurements/game-assist/2026-09-30-m0-manual-continue/pending-cancel-x10d-p-controller`。入口：[accept-pending-cancel-x10d-p.ps1](../tools/accept-pending-cancel-x10d-p.ps1)，已有輸出時拒絕覆寫，不對frozen batch增加replay。

- 重新核對**226個source/export/binary/snapshot綁定及207個artifact-ledger項目**；原report、XML、commands、source、events及digest SHA相符。唯一export差異是variant的game.cpp；inverse patch還原parent，新增8行與main50 donor完全相同。
- 核對三run分母、原input binding、reference與X10c ON/OFF bridge，以及candidate ON/OFF events/state-digests相同；原ledger也綁定完整summary及semantic值。沒有重跑三次full replay，不把hash核對冒稱重新執行全部PNG解碼。
- **實際重跑六套既有test binary**，新XML/logs/commands/exits留在驗收根。未重build，原source/test期待全部保留，原27張RGB opt-in已啟用。

| 獨立重跑 | 結果 |
|---|---|
| Release reference原suite | 207/207 |
| Release reference新契約 | 8通過／5預期失敗，與先紅清單完全一致 |
| Release candidate原suite | 206通過／1刻意契約失敗 |
| Release candidate新契約 | 13/13 |
| Debug-ASan candidate原suite | 206通過／同1刻意契約失敗 |
| Debug-ASan candidate新契約 | 13/13 |

全部errors／disabled／skip為0。candidate唯一失敗仍是`SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt`的兩個pending_count期待：新規則取消，所以0取代舊期待1。這是事先聲明的契約變更，不能報成原207項全綠；新增fresh-return測試另外驗證合法新intent。没有新的非預期回歸。

- **重新執行三個C++分析工具**：full action audit、lifecycle audit、causal review；解析後完整JSON與原封存報告相同。不是只核summary數字。
- 驗收輸出≤8MiB、campaign+prior≤8GiB；實際容量、SHA、命令及失敗清單以新根`final-summary.json`為準，原X10d-P batch/out仍frozen。

## 2. 判定及邊界

**Verified：**當較新完整snapshot已被accept且目標缺席，新hook只取消submitted且**absolute cursor確知為0**的plan；不將缺cursor、completed、unknown injection或修訂後非零prefix當成可重試。due早於新snapshot ready時仍可先Down；沒有future pixels回溯禁止。active Hold／alias／shared Drag及原grace由原路徑處理。測試覆蓋四Note種類、fresh-return、missing／no-new-frame、due前後及同時、gate/context/reset、rootless旋轉Hold、非零prefix及unknown/completed。

**Verified全分母：**2165 reference／2131 candidate actions，2121個normalized matches、44／10 unmatched，EOF前全部稽核。observer scene/bank/note-history在7722幀全部相同。首owner差1976、首action差1978；28個hook取消均為known-zero且該intent此前無receipt。14組reference曾Down但candidate未Down；5組依新current target重建。H/K原行為保持；A的Drag重建使一個Move消失，另有相同Move因既有map-key等時排序而在全序列位置改變。持續Hold的normalized action signature保持。

**Strong inference：**總控逐組核對19個受影響local ID的取消、返回／新plan與receipts，與單一hook及既有scheduler排序一致，未發現需另一策略改動才能解釋的差異。`causal-review`自動的`explained`條件主要是hook join與Down數分類，本身不是一般因果證明；此簽收還依據唯一patch、全observer相同、逐組時間鏈與fake-clock回歸。不能把該boolean泛化為任意候選自動驗收器。

**Unknown：**14組機會損失是否曾是合法遊戲命中、physical note真值、真實recognition/RPC延遲與閉環feedback、77 Miss改善、跨曲泛化。它們不是14個已避免的ghost，也不是14個新增Miss。固定recording不會隨counterfactual touch改變後續pixels，這個限制不能靠更多相同replay消除。

沒有發現阻止本單機制冷契約簽收的問題；**產品效果與live readiness仍未驗收**。機會損失是實機比較必須報告的取捨，不以減少Down／取消數判成功。

## 3. 下一步

1. **X11-P成本與候選凍結優先。** 從C36h正式來源建立baseline與僅有此hook的候選完整runtime，先定版本／來源／依賴／profile／binary綁定與成本預算；原research replay EXE不可當成live runtime。驗新snapshot與between-frame排程、latest-frame跳幀／延遲分布／容量，保留已知契約測試差異及所有失敗。現有replay host samples排除了每幀額外digest/lifecycle，且含instrumentation，不作正式latency gate；summary沿用的`with digest`字樣不足以描述實際計時範圍，新成本報告須按meter邊界明列。
2. X11通過才進**X12有限emulator baseline A/A與A/B**，沿用使用者既有授權及總量最多6輪，預檢、完整失敗分母、固定candidate，未知注入先釋放／停止。這時使用emulator是必要的：要判定觸控是否被遊戲採納及漏觀測時撤銷pending的實際利弊。不能直接推定Miss下降；若效果退步就保留結果並撤回候選，不混入第二策略救分數。
3. **X10d-O另案保留。** 兩Hold／tail與incoming歸屬尚未解決，但不是本pending-only候選進X11的前置。若後續做O，從未加X10b suppression的baseline另立反例和獨立實驗；不在這次比較中混改。

本輪止於獨立驗收與handoff更新，沒有啟動X11、新chat或實機。正式baseline仍C36h tint1；原開發報告的「待總控驗收」保留為封存時狀態，最新結論以本頁及status J22為準。
