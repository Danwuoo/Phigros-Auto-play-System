# 開發路線圖

## 2026-10-01 當前研究方向

使用者要求整合全部 worktree 至 main，再由 Astra／xhigh 新 task 閱讀理解、研究下一步並整理清洗。新候選 observer50／planner27 沒有新實戰。恢復分支 C36h 與原全錄均 77 Miss，主要問題未改善；CPU 合成小試已有工具與提議，但真實人工 pixel gold=0。研究優先核對既有連續像素的 Hold 支持／身分／關聯／owner 失效，評估完整暖機 FakeTouch 反事實重播及小量针对性標註；不要把所有框架或模型訓練作前置。模型僅辅助離線優化。詳見[當前交接](MAIN_INTEGRATION_HANDOFF_20261001.md)，下列各日期路线保留歷史含義。

2026-09-29 四首熱測後補充：observer48／planner25 未完成四首複測，使用者已停止該輪實測；其一輪中止資料不可當成分數驗收。缺線時原線位移追蹤未接動作資格的問題改以 observer49／planner26 離線修正。2026-09-30 使用者明確要求重啟，完成 Glaciaxion HD6、Dlyrotz HD9 兩首後又要求停止；程序已退出，保存結算與原始事件。後續仍需按結果檢查真實像素與判定語義，不自動恢復未完成的兩首。

2026-09-28：後續方向以使用者指定的 [跨曲學習研究](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md) 為準。舊單曲 AP 路線與擷取重選已退出主線；保留已完成實作和重要基準，沒有宣稱新模型已完成。

2026-09-29 最新安排：[非學習式冷開發計畫](NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md)先執行P0–P5，涵蓋七輪診斷、線／Note觀測與關聯、旋轉Hold及混合動作、預測、完整時序與瓶頸優化。最新實戰為observer37／planner19的六HD＋光IN，Dlyrotz退步須保留追查。本輪禁止啟動emulator；通過冷驗收後再由使用者安排熱測，不因舊操作命令或先前實戰授權自行恢復。

2026-09-29 第一輪冷開發結果（歷史）：當時 source 為 observer38／planner21／diagnostics7，已完成七輪分段追溯、65組RGB冷重播與三項先紅後綠修正；預配置A/B未改善 tail，故未啟用。[結果與未解項](COLD_DEVELOPMENT_RESULT_20260929.md)列明當時P1–P5缺口，冷驗收部分未達，仍停止於 emulator 啟動之前。

同日使用者要求持續goal續作：沿用原GPT-6 Sol／xhigh task，依[同一計畫§8–9](NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md)完成C0–C6；先凍結38/21，以既有像素提出可核對標記及小額通用參數實驗，補RGB真值組合、Hold／排程與整鏈效能。部分未達報告不作goal完成；可冷驗項全部完成才交熱測議程，熱測仍需使用者另行啟動。OpenCV等傳統影像方法可作離線工具，proposed不升格人工真值，本輪不引入模型訓練。

續作結果：observer38／planner21／diagnostics7配套已凍結；現行observer47／planner23／diagnostics11的既有RGB索引核對753張／736 unique SHA、18家族，選30clip並產生90幀proposed標記，仍無人工gold。G1–T2九組和R1–R7七個交叉各有合成RGB／oracle／fake-clock正反代表例，機讀矩陣32／32列已由C++ validator核對；包括線獨立旋轉／交叉、晚對齊與瞬移、縱連、Drag分合／Flick串、旋轉Hold head→body→tail、混合五指與容量／故障。C1的固定三候選共用線缺口實驗維持4px，未按曲家族調參。六區塊預檢在原始RGB微基準及正式三執行緒記憶體FakeCapture冷鏈Tap場景超出預凍A/A噪聲；密集128 Note逐批加速實驗未過，但不超原計畫非退步容忍。Tap／dense各10,000幀長跑、writer滿／慢fake RPC及完整QPC join已驗；`c5-global-gate.json`依原§8.6規則passed。Release／Debug／ASan各309／309通過，最終來源與設定hash、容量帳本及熱測待驗清單已保存；C0–C6可冷範圍通過。合成接觸不代表遊戲採納；詳見[結果續作章節](COLD_DEVELOPMENT_RESULT_20260929.md)。

## 階段

2026-09-29 main整合驗收：冷測量器發布時間race修正後，Release／Debug／ASan各310／310及1,000幀正式冷鏈通過，既有753RGB相對交付版語義差異0。效能主張與待熱驗邊界見[合併驗收](COLD_DEVELOPMENT_MERGE_REVIEW_20260929.md)；本次不啟動熱測。

| 階段 | 交付 | 驗收／限制 |
|---|---|---|
| M0 證據與根因 | 凍結兩輪證據；30 個 clip 的標註小試；線 ID 重生的合成前置狀態與真實摘錄對照 | 分開可見觀測、關聯失效與未知；AI 提議不算人工雙標 |
| M1 幾何策略、關聯與採樣 | greedy／全域 assignment／歧義出生策略比較；斜向、旋轉、線追 Note、突現及往返的短小合成回歸；有界兩秒診斷 buffer | 真兩線不可合併；反轉撤銷與去重正確；測 off／ring／writer 的 tail、容量、drop |
| M2 跨曲資料 pilot | 按歌曲家族分組；均衡短窗與長 Hold／旋轉必要片段；人工 instance／ID／relation QA | 資料達相應能力門檻才 training-ready；不湊影格數 |
| M3 有條件的單幀學習 | 確認 A 的觀測瓶頸後，才比較小型 CNN、單一小 DETR；固定 split、3 seeds、C++ 輸出等價 | 漏辨、錯配、危險 FP 與整鏈 tail 同時可接受；可不做 |
| M4 必要的時序／關聯學習 | 僅在 M3 證明需求後，加因果短窗或關聯頭 | 不用未來幀、全曲時鐘或無界 hidden state |
| M5 Shadow → 有限 live → 開發集 | 固定版本 A/A、A/B；19 HD 與 IN 分層比較 | 無安全回歸；完整報退步曲與故障，不能只挑最高分 |
| M6 新曲保留驗收 | 模型凍結後解封新歌曲家族，HD／IN 各自評估 | 開發歌不算未知曲；沒有足量證據就只稱 pilot |

詳細容量、標註成本、停止條件與建議數值以研究文件為準；那些門檻仍須在新實驗前凍結，不能事後調整來通關。AP 是另行全曲結果，階段通過不等於 AP。

[判定線形式](判定線形式.md)作為場景覆蓋清單。先驗證幾何與動作策略，形式多不等於需要學習；若 A 已達相應能力門檻，可跳過 M3／M4 進 M5／M6，仍須跨曲真值、固定測試資料與相同安全／延遲驗收。

M1旋轉專項：Note遠處不對齊、接近才對齊時的候選關聯；Hold按住期間線旋轉／平移時的body觀測、同指Move及tail結束。加入干擾線、掉幀、反轉與同時Hold，分別驗證錯配、接觸位置、連續性及Up時機；不能只放寬角度門檻當完成。

本次開發按研究 §5.1.1 的 D1→D4：線身分／出生 → Note與旋轉線的關聯 → 旋轉Hold持續接觸 → 分段相對運動與預測撤銷。先用合成及fake-clock完成可測實作，完整人工標註集不作前置阻塞；再按實際缺口做針對性標註。兩秒採樣ring與模型訓練不混入本次改動。

## 當前基準

- 現行開發版 observer47／planner23，尚無實戰；最近七輪是 observer37／planner19。observer36／planner18 為前兩輪實戰基準，另保留 observer33／planner13 作歷史比較。
- 同 19 首 HD，Miss 815→818、非 Perfect 957→860，尚無 AP。兩首 IN 各一輪，不構成 IN 版本配對。
- 558 RGB＝186 triples；沒有獨立人工真值，不能直接啟動正式模型驗收。
- 固定 gRPC fast／256 KiB、latest-frame、QPC、指紋五指、單一 touch owner。
- 兩輪原始 session、能力與必要回歸資料保留；已刪歷史資料的範圍見 [清理紀錄](CLEANUP_AUDIT_20260928.md)。

線 ID 異常的合成因果鏈與 D1–D4 幾何／關聯程式回歸見[實作紀錄](JUDGMENT_LINE_M1_IMPLEMENTATION_20260928.md)。最新七輪分析與下一步以冷開發計畫為準；先由現有證據建立反例並修通用機制，必要的局部標註保持proposed／人工覆核之別，不以大量標註阻擋冷開發。有限診斷ring可冷測負載，不採新實戰資料；不恢復無限AP或模型訓練。
