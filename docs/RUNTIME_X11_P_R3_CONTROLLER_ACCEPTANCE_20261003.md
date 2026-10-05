# X11-P-R3 總控獨立驗收

2026-10-03。**簽收長窗 collector 工程、原始證據與覆蓋率負結果；成本資格仍 not-ready。** 本次沒有發現需要退回修補才能成立的 R3 負結果。沒有候選性能退步結論，也沒有 X12 資格。

## 獨立驗證範圍

main HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，唯一 worktree，既有 dirty 保留。正式 86 檔（src/include/root CMake）SHA 與 R3 接手快照一致，正式 metadata 仍50/27/11。C36h tint1 為 behavioural baseline；main50 為 donor/control；B1 的 pending-only 八行仍為唯一策略差異，suppression OFF。沒有新 runtime pas.exe 或正式採用。

總控閱讀 R3 protocol/result/handoff、collector/main/contract/gate/audit/archive verifier/touch/tests/CMake 與 freeze/run/accept 腳本，核對 R1 normal gate、SessionArchive 共用來源及 R2 總控停止線。實跑與只讀核對明確分開：

| 本次驗證 | 結果 |
|---|---|
| 新 R3 artifact ledger | 809 項逐檔 SHA/檔長及 ledger root 符合 |
| R3 來源及實際依賴 | 51 source/binary/method、575 compiled dependencies、另2 input bindings 符合；六張 link maps 核對新 archive object，未抽取舊 library archive object |
| 舊 frozen 證據重 hash | X11 274/1490；R1 305/1580/376/2059；R2 38/562/707 全部符合。R2 兩個可變文件僅以已封存 controller pre-review 副本核歷史 SHA |
| 隔離 source inverse | 57 對 src/include 移除唯一 pending hook 後一致；新 archive 僅三處16384→65536，共同 instrumentation 差異，不改舊 core |
| 新 tests 實跑 | B0/B1 Release 各8/8；B1 ASan10/10；fail/error/skipped/disabled0。Release 明確排除兩個大型 archive 邊界 fixture，ASan 實跑完整套件 |
| 原11套 XML | 重新解析逐 testcase 與核 SHA，含已聲明 historical failures；**沒有重跑原238/pending13/R1/R2 suites** |
| 五筆 C++ raw audits | 重新輸出至 controller root，與開發版逐 byte 一致；14 metric families、全窗/暖機/測量/四 blocks、完整 receipt/release/archive 核對 |
| 額外獨立維護核對 | 直接重讀五筆 frames/receipts，重算分母、全窗與測量 recognition/owner/capture→owner，以及全窗 lateness 的 n/quantiles/jitter；各 archive segment SHA、五個 gate exit 符合 |
| 兩版 streaming bridge 實跑 | 各512 inputs；summary bytes 與 R3 frozen reference 相同。B0 14,668,947B/7931 rows/177 receipts；B1 15,043,186B/9562 rows/127 receipts；contacts exit0 |

本次 new build/cost/stress/full-PNG replay/live 均0；ASan 是有界契約測試，不是新 OS 並行成本 run。R2 active 並行覆蓋仍只引用歷史證據。未啟動 emulator/ADB/manual-session/真觸控/模型、goal、chat 或 automation，未 commit/push。

## 決定性結果：有效負結果，但不是候選退步

五筆實際 normal runs 全為 **B0**，依次四筆 RGB、第一筆 owner。四筆 RGB 僅通過各自 normal/integrity gate，不能稱完整 A/A noise 已通過。

`aa-owner-1` 的原始2560 attempts 全發布，2475被 perception 消費、2282到達 owner。分解為 **85 consumer skips＋193 decision skips**，完整保留於分母。原 R1 門檻要求 owner≥2304，實際少22：**2282/2560=89.140625%<90%**。總控以整數比較 `owned*10 >= attempts*9` 獨立核對，gate CLI 亦 exit2。

固定 measurement 的 recognition n2231、owner/capture→owner n2052，均達≥2000。全窗 capture→owner p99/max=11.5068/17.5312ms，lateness p99/max=5.8618/8.6554ms，符合100/250及15/100ms界限。raw 完整性、收據/釋放安全計數、contacts exit、worker/archive fault 與每 run80MiB容量均過。

所以本次已解決「長窗樣本/容量不夠而根本不能開始」的阻塞；現在的停止原因是 **instrumented baseline 在固定正常負載下的 owner 完整分母覆蓋率不足**。90%是本比較的資格條件；latest-only 設計本來允許跳過舊資料，不能只因未達90%便另宣稱正式 runtime 發生安全契約錯誤。

R3 額外加入的 measurement90% 是成本前凍結的較嚴條件，**不是 R1 原門檻**。交付已補記此來源差異；總控不回改 frozen method。即使不使用新條件，原 full90% 已足以停止，因此不影響本次負結果簽收。`gate_threshold_changed:false` 只能按「成本開始後未改」理解，不能解讀為 R3 與 R1 方法完全相同。

驗到 run 目錄與 qualification 恰為 AA5/ABBA0/stress0，無 owner2–4、noise-frozen 或 cost-evaluation 產物。`noise_gate:false` 是尚未取得資格，**noise 未評估**；不能說本輪又測得 noise fail。B1 未進新成本 run，不能歸因於 pending hook，也不能以低於90%的差距小為理由補跑到過關。

## 證據分級與未覆蓋

- **Verified：** 上述 SHA、source inverse、測試、bridge、五run raw/分母/分位數、舊門檻負結果及依序停止。新版 collector 的增量界限有測試支持；本次 source/正式策略未變。
- **Strong inference：** 在已測512input public bridge上，共同插樁保持策略可見輸出；不能外推所有輸入或原 live timing。
- **Hypothesis：** action thread 的完整診斷序列化/入列、OS scheduling 或 producer 到達群聚可能促成 decision overwrites。`owner` metric 主要涵蓋 `game.accept`，未涵蓋整個 action iteration；低 owner p99 不能排除其他服務時間影響。未取得直接因果隔離。
- **Unknown：** 193 decision skips 的完整因果分配、有效獨立樣本數、完整 AA noise、B1 成本差、真 capture/RPC/game adoption、原 live 未列 compiled SHA、14 lost opportunities 利弊、77 Miss 及跨曲改善。

逐 event timing 的窗口依最後 accepted attempt 錨定，不是每項實際服務時刻。因此不能把某 block writer 高值直接指派為同 block 某個 lost decision 的根因。原始 scheduler cancellation/contact 數同樣不是遊戲 Miss。

## 總控決策與下一步

**停止目前 pending-only 候選的成本資格追加測試及升格，不自動派 R4/R5、不啟動 X12。** 這不撤銷 X10d-P 冷驗證，也不代表已證明該策略較差；是尚無有效比較可支持採用。保留原 C36h baseline 與全部負結果，不能藉改分母、降低90%、減少完整 JSON 或原樣重跑取得 pass。

若再投入本方向，應先提出一個使用**既有 owner raw**即可回答的窄問題：跳過的193個 decision 是否集中在已可觀測的 producer/recognition 到達群聚、owner accept 之前或之後的服務缺口？沿 frames 的 capture/recognition/owner 時戳及現有事件核對已知部分，餘下標 Unknown；沒有細粒度 actor 時戳就不可硬拆 JSON/OS 成本。只有形成具體可證偽原因、確認最小必要插樁及新的有界實驗價值後，再由總控決定是否值得另一包。這是後續決策條件，**本次沒有開始新研究包或授予額外成本輪次**。

## 重現與封存

新增總控證據：`measurements/runtime-cost-x11-p-r3/controller-review/`。舊 bindings 另寫 `old-freeze-verified-controller-independent.json`；兩者合計計入預留32MiB。最終檔長、SHA及實際命令在 controller `final-summary.json` / ledger，原 R3 ledger/final-summary/raw 不改。

```powershell
# 使用新的不存在目錄；不要覆寫本次 controller-review。
./tools/accept-runtime-x11-p-r3.ps1 -ReviewRoot '<new-review-root>'
./tools/review-runtime-x11-p-r3-controller.ps1 -ReviewRoot '<new-review-root>'
# 若需要再次完整核舊來源，Phase 必須是從未使用的新名稱。
./tools/check-runtime-x11-p-r3-frozen.ps1 -Phase '<new-review-phase>'
```

以上是重現入口，不是自動追加執行授權；重跑前仍須計算32MiB驗收剩餘配額。總控維護 cross-check 首次因 PowerShell `-join` 比較括號不足誤拒 run list，核對實際五目錄後只修正括號重跑；沒有因此改動開發工具、raw、測量門檻或增加任何成本 run。該維護修復與 R3 pre-cost 修復額度分開記錄。
