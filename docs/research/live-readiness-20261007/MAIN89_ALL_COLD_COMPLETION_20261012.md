# main89 必要冷測試完整收尾

本輪依使用者「一次測完全部」完成同一候選的 Normal3、AA12、noise18、ABBA36 及最後只讀核對。**成本 NOT_READY、live UNQUALIFIED，尚未到只差啟動 emulator 的階段。**
實際編譯及測量來源都是 main89／`0486ed3127acfa1b237186ba56ba3b60958604c1`，build attempt 為 `main89-perf-e2`。後續這份文件的 HEAD 不是 binary 來源。

## 本輪修補與範圍

- 冷 AA／ABBA 將固定 Frame metadata 在測量 epoch／producer 啟動前準備一次；每筆只更新 sequence 與 capture／pixels-ready 時間。原 pool 深拷貝、像素及 publication 行為保持。
- `src/game.cpp` 第二個 note decode loop 提前跳過原本最後也會跳過的白色元件，省去無結果用途的幾何計算。第一個 judge/PCA loop、像素策略與門檻保持；沿用 main80 motion delta。
- 必要來源 overlay、兩個 owned build batches、actual member／source／Native8 joins 明列。舊 22-member archives 及 base55 libraries 仍是原來源，沒有冒稱 fresh 全包。
- builder／loader 的空 prior、encoding 判讀與 build-group JSON 身分問題已修復；原失敗與 STOP 保留。main89-e1 的 root-exit 身分拒絕沒有改成 PASS，fresh main89-e2 才取得完整收據。

新候選有 16 個必要 compile occurrences、13 個 build／control Native 執行。原 affinity／reader／score controls、motion26、Dense12、Frame8 與必要 pre-cost sections 通過。原75、production9、prefix3063、fault4 沿用原 PASS 來源，未全盤重跑。
Normal status 的 `source_commit` 保留 base55／`12ec055` 宣告；`execution_source_commit`、`delta_build` 與 actual Native8 才是 current main89 執行身分。保存索引分列兩者，沒有把 base55 libs／原 PASS 證據改稱新來源。
沒有啟動 emulator／device／Computer Use、新 sidebar thread、額外 ETW、模型訓練或遠端 push。原 R2、20ms／2560／warm256／measurement2304、數值 gates、48GiB／minimum-free20GiB 與 tests1GiB reserve 保持。

## 完成數量與原結果

| 項目 | 實際結果 |
| --- | --- |
| 必要 build／controls | fresh16 TUs、13 Native，全部 Native0／runner0／verification0 |
| pre-cost readonly | 七個必要 sections PASS；成本未就緒，原 Expected2 |
| Normal3 | 三項 full raw PASS；Tap、Dense 原數值 PASS，Hold NOT_READY |
| AA12 | 十二項 full raw PASS；6 validity PASS、6 NOT_READY；Tap、Hold 各1／4 PASS，Dense4／4 PASS |
| noise18 | 原 analyzer 完整計算；11 adequate、7 inadequate，ready=false |
| ABBA36 | Tap／Hold／Dense 各12，三批 A／B／B／A；36 full raw audits PASS |
| 原 formal comparator | Native2／runner2／verification0；`complete unstopped same-candidate AA required`，沒有 comparison.json |
| 最後 readonly | fresh `full-r2`，Native2／runner2／verification0；七個原必要 sections PASS，整體 NOT_READY |
| collection／實際裝置收據 | NOT_RUN；冷成本前提未過 |

Normal 的原 recipe 在 Hold STOP；只補原未執行 Dense 一筆，沒有重測 Tap／Hold。Dense-only clone 的 inherited console 字樣不能當成 Normal3 整體 PASS。
AA 原 recipe 在首 Tap NOT_READY STOP；後十一筆在獨立 diagnostic completion root 補完，同一 frozen plan／source／workload，原 status 與 STOP 沒有覆寫。數值 NOT_READY 不阻止本輪剩餘收集，但 Native identity／cleanup／raw integrity 仍是停止邊界。
AA completion v1 在所有十二筆已完成後遇 PowerShell selector aggregation 錯誤；保留原腳本／STOP，以 v2 核對既有十二筆後只執行原 noise 收尾，沒有重新啟動 producer／auditor／gate。

最後原 readonly recipe 的 canonical AA root 仍只有原 STOP、沒有正式完整 aa-input；因此其 aa／abba 欄為 NOT_RUN、noise NOT_READY。這是正式 gate 尚未接受的狀態，**不是**實際 diagnostic AA12／ABBA36 沒有執行。完整測量由獨立 completion 索引明列，不用診斷資料冒充正式 PASS。
第一次 `full` launch 留下 input／closure leaves／direct spec，tool session 遺失，無 final Native／capacity／receipt；退出及具體原因 UNKNOWN。原檔與 fresh-output refusal log 保留。不同 fresh output `full-r2` 才取得上述完整 Native 收據，沒有重跑量測。

## 尚未修好的長空窗

Normal Hold：producer5／callback6 超期、schedule lateness max59.0737ms；pool0、raw integrity PASS。AA producer／publication 合計297／305，ABBA 合計1052／1058；這些可包含同一長窗後的 absolute-epoch catch-up，不能當成相同數目的独立卡頓。

| 同一筆原 attempt | start lateness | render | render end→publication return | 判讀 |
| --- | ---: | ---: | ---: | --- |
| AA Hold1／2423，measurement | 265.5677ms | 0.3151ms | 0.1175ms | 長窗已在 render start 前 |
| ABBA Hold batch2 A offset0／2311，measurement | 379.6099ms | 0.3430ms | 0.1025ms | 長窗已在 render start 前 |
| ABBA Dense batch1 B offset1／2001，measurement | 0.0004ms | 0.0003ms | **95.9341ms** | publication span 仍有長窗 |
| ABBA Tap batch3 A offset3／1，warmup | 458.3910ms | 0.2244ms | 0.1781ms | 全輪最大 lateness 位於 warmup，不能稱為 measurement 最大值 |

以上保留同 attempt 的原 QPC 邊界，沒有將不同 maxima 相加。排程／publication 仍未穩定；端點不能分辨 slot lock、copy、notify 與被排程中斷的各自成本，根因 UNKNOWN。
舊 main80 Hold3 的33.9µs負結果仍保留；本輪沒有單獨追修該微小差值、加 tolerance 或移除原負結果。這次未通過的重點是上述毫秒級空窗。

獨立 main83 三個 phase diagnostic runs 曾全部通過 raw／原數值，沒有重現巨大空窗；它們是另一個 diagnostic binary，不能證明 main89 長窗已修好。producer ideal-processor 偏好草案只保存在 ignored planning，**未實作、未 query、未編譯、未測試**；它不是已確認的根因或修復。

## noise 與 ABBA 描述性判讀

原 AA-only 固定配對計算；不刪掉失敗 run、不用 B 重估噪聲、不放寬 floor。

| inadequate metric | 原 d（ms） | 原 T（ms） |
| --- | ---: | ---: |
| Dense capture_to_owner.p99 | 8.234181 | 16.468362 |
| Dense observer.p99 | 6.149210 | 12.298420 |
| Hold capture_to_owner.p99 | 5.836094 | 11.672188 |
| Tap capture_to_owner.p95 | 10.850375 | 21.700750 |
| Tap capture_to_owner.p99 | 10.487686 | 20.975372 |
| Tap observer.p95 | 8.911175 | 17.822350 |
| Tap observer.p99 | 10.740988 | 21.481976 |

ABBA 固定54 cells 中52在本輪原 T 內、2超出，兩項原 AA noise 都 adequate：Hold batch1 observer.p99 的 B−A **0.829758ms／T0.5ms**；Hold batch1 capture_to_owner.p95 的 B−A **3.100105ms／T3ms**。
Dense 的18 cells 沒有超出本輪 T；observer.p95 三批 B−A 為 **+0.4213575／−4.5476300／−14.3541575ms**，本輪 T0.99392ms。這是本輪 gate 門檻下未再出現原 Dense p95 退步，不能以不同 binary／noise T 宣稱跨版本因果改善，Dense p99 noise 仍不足。
Dense batch2／3 有 paired bottleneck＋capture improvement，九批共2；沒有三批穩定 C5 收益。C5 只描述，不增 readiness gate。
ABBA16／36筆 report-only 必要條件檢視有負結果，24／36筆有至少一次 producer／publication overrun；Dense 原 gate 不要求 zero overrun，仍照報其所有次数。這些 metadata 算術不替代原 C++ load_run 或正式比較器的拒絕。

## 保存與容量

完整[保存索引](MAIN89_ALL_COLD_COMPLETION_20261012.json)分列 current131個 Native8、report／audit／gate、原 source／binding、attempts 與原 journal SHA、原 STOP／失敗、診斷計算 recipe、capacity 與 archive restore proof。Normal3 加 AA12／ABBA36 共51項 full raw audits；原資格與診斷結果分列。

先前 inactive main80 completion journals、inactive compiler／source payloads，以及本輪已完成且 invalid 的 AA12 journals 均依 original SHA→decoded ZIP SHA→original再次核對、durable proof／restore index 與 LiteralPath containment 核對後回收。current AA 十二個 journal 原 bytes 共2,003,737,221B，ZIP95,372,557B，净回收1,908,364,664B；原 reports／audits／attempts／gates／STOP／Native／noise 仍在原位置，journal 可 exact restore。
ABBA36 journals 仍保留原位置。舊 source／compiler path 重驗須先依索引恢復 exact bytes；封存不授任何資格。
最後 readonly after-capacity 為 **49,645,010,532B** retained、free205,839,822,848B；原 cap51,539,607,552B／minimum-free21,474,836,480B 保持，after reserve0 PASS。下一次 build／tests 仍須用各自原 before reserve 核對。

本輪一次完整收集與盤點完成。實機 ready 目標尚未達成；剩餘修補重點是 producer／publication 長空窗、Hold 成本退步與七項 noise 不足。後續必要修補須以明確來源與 fresh 收據驗證，不從這批負結果直接取得裝置資格。
