# 開發路線圖

## 2026-10-01 現行順序

main 已整合 tracked 工作；候選 **observer50／planner27／diagnostics11，尚無實戰**。C36h tint1 與前次 C36g 全錄皆 77 Miss，使用者回報主要問題未改善。模型只辅助離線標記和主程式優化；不進即時迴圈。本輪完成研究、既有資料重播與實際清理，沒有恢復 emulator／真觸控、AP goal、無限實戰或模型訓練。

可核對的現況、main50 重播新差異與詳細交付標準見[現況與下一步研究](PROJECT_STATUS_NEXT_STEPS_20261001.md)；來源與 binary 對應見[整合交接](MAIN_INTEGRATION_HANDOFF_20261001.md)。下一步建議如下，尚未實作的項目不標完成：

| 優先 | 工作 | 最小交付／停止条件 |
|---|---|---|
| P0 | 真實全錄的完整接觸重播 | 沿用現有 C++ reader，補 SessionPerception／reset／gate、GamePlanOwner、FakeTouch、幀間 scheduler；先取兩個 Hold 窗＋clip09正常對照＋5520 Flick＋6214側向 Drag。不能用原按鍵或 runtime ID 當 oracle；早期狀態不足明示 unknown |
| P1 | 用反例定位與修正通用機制 | 分離当前 body提取、身分、Note→line、owner取消。main50 的6214 Drag仍有垂直線卻綁水平線，是優先關聯反例；Hold3498有body但無root不能直接算漏接。每次只改一個已定位機制，以先紅後綠與跨形式負例約束 |
| P2 | 小量針對性標記 | 沿用7722原圖、12段理由及18 ROI，補最必要的body／tail／干擾線／同物件連續性覆核；proposed、unknown、人工gold各自保留。缺人工gold不阻塞可冷驗的owner／scheduler契約 |
| P3 | 凍結候選再提有限熱驗方案 | 先保留所有已知反例、失敗與baseline；量完整冷鏈分布和資料界限，再由使用者另行安排固定版本跨曲比較。不得把離線root數或FakeTouch當命中提升 |

不把大型新 Fixture、完整標註平台、更多合成訓練或擷取重選當前置。重播先補能回答問題的最小功能；若原始記錄不足以重建可信接觸狀態，保存 unknown 與缺口，用獨立合成／fake-clock 驗契約，不偽造歷史 Down。

## 已存在的成果與限制

| 項目 | 狀態／證據 |
|---|---|
| D1–D4、C0–C6 | 9/29可冷範圍已通過；線身分／關聯、旋轉Hold、混合動作、fake-clock、QPC冷鏈均已有實作。部分歷史性能子規則未過，見[冷結果](COLD_DEVELOPMENT_RESULT_20260929.md)和[合併驗收](COLD_DEVELOPMENT_MERGE_REVIEW_20260929.md) |
| 9/29–9/30 main 熱測 | observer47/24、48/25中止、49/26兩首後停止，須按版本與完成／中止分讀；不是main50效果，見[熱調試](FOUR_SONG_HOT_TUNING_20260929.md) |
| 恢復線 C36h | 同號37/19不是歷史main37/19；Dlyrotz IN13為795950／P496-G11-B0-M77，主要Miss未改善 |
| 全錄與人工選取 | 7722原生PNG、12段已填理由、3722 unique／3843 references；clip09是正常對照。131.829秒received pixels，最長capture gap443.5487ms；不是來源完全無漏幀 |
| main50離線研究 | 新暖機observer重播7715次、核對全部PNG；3722選段中960幀root／垂直線數改變，不能換算準確率或Miss。完整owner重播仍缺 |
| CPU模型小試 | 4377參數、合成IoU .89267、18 native ROI proposal；真實人工pixel gold=0，真實／跨曲準確率unknown。本輪只核對既有工具與資料，未追加訓練 |
| 整合測試／整理 | 整合Release348通過／1 opt-in跳過／0失敗，CPU8/8；本輪清理後117/117相關回歸。實際清理與原檔SHA保護見[清理帳本](CLEANUP_AUDIT_20261001.md) |

## M0–M6 對應與範圍修訂

保留原[跨曲研究](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)作研究沿革。2026-10-01「模型只離線輔助」優先於舊版即時學習式觀測／GPU推論提案；選定跨曲方向不代表每個學習階段都必做。

| 階段 | 當前定位 | 尚缺證據 |
|---|---|---|
| M0 證據與根因 | 現行P0–P2主線；既有原圖、選片、proposal、合成反例可用 | 真實全錄owner因果鏈、針對性人工覆核、可證偽根因；cancel／combo／ID不算逐Note判定 |
| M1 幾何與關聯 | 已有全域assignment、線局部運動、Hold接續與有界預測，繼續由反例修通用機制 | 多線／旋轉／晚對齊／反轉等實景語義；不能因形式複雜先推定需學習 |
| M2 跨曲資料pilot | 按歌曲／譜面家族切分、必要短窗與長Hold並存 | 現有18 ROI全為Dlyrotz development；不足跨曲訓練或驗收，不能湊幀數代替分布 |
| M3 單幀學習（可略） | 僅可條件式比較離線輔助標記品質與人工成本 | 未達相應資料gate，不追加無界合成選模；不再排入runtime部署 |
| M4 時序／關聯學習（可略） | 只有已證缺口需要時再研究離線用途 | 不以全曲時鐘、舊按鍵、未來幀充當正式即時策略；非本輪前置 |
| M5 有限實戰 | 驗證凍結後的C++候選，固定版本A/A、A/B並報所有曲與故障 | 需另行安排；main50 live=0，歷史開機授權不自動延續 |
| M6 新曲保留驗收 | 凍結後解封未用歌曲家族，HD／IN分層 | 開發曲不算未知曲；沒有足量資料只稱pilot；AP另行全曲驗收 |

## 必須保留的驗收邊界

- 旋轉時分開 Note朝向、運動方向、線法向；覆蓋遠處未對齊而接近才對齊，以及Hold按住期間線仍旋轉／平移。head接入、body接觸、tail結束各自驗證。
- 幾何過線不等於完成；返回不重播已Down／未知Down；同contact按當前支持Move，不固定初始點也不盲跟線旋轉。
- 固定gRPC fast／256KiB、latest-frame、QPC、五指指紋、單owner與硬上限。時間／排程改動需可重現合成軌跡／fake-clock，效能報完整分母、分位數與jitter。
- 兩輪歷史19HD：Miss815→818、非Perfect957→860，無AP；原始session、能力報告、獨立baseline及失败證據保留。清理不重寫實驗歷史，不把已刪ignored raw假稱可重算。
