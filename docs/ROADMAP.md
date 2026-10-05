# 開發路線圖

## 2026-10-04 現行順序

產品目標為 **Chapter Legacy所有曲目解鎖IN並完整IN結算Miss=0**，HD作解鎖及回歸，P/G/B照報，不增AP前置。最新基準、逐曲證據及工作包契約見[10/4總帳](LEGACY_IN_ZERO_MISS_BASELINE_20261004.md)與[JSON](LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json)。現行章節分母未核、目前逐曲解鎖unknown，沒有全曲驗收。

main HEAD f83c7ea，**main50 50/27/11是comparison／donor、live0；C36h tint1是behavioural／experimental baseline**，Dlyrotz IN仍77Miss。X1完整fixed-pixels contact replay已於10/2獨立驗收；X10d-P冷契約已通過，X10b suppression否決／OFF，X10d-O未開始。X11-P/R1/R2/R3工程與負結果已簽收、成本not-ready，X12仍conditional。[10/1狀態](PROJECT_STATUS_NEXT_STEPS_20261001.md)按J5–J32接續，A–I及早期「最新」是歷史；[10/3計畫](C36H_FORWARD_EXECUTION_PLAN_20261003.md)最新追加段銜接下列順序。

| 優先 | 工作 | 最小交付／停止条件 |
|---|---|---|
| 包1 | 基準、逐曲證據與入口整理 | 本包開發自驗後交總控獨立驗收；不改策略或追加遊戲／成本 |
| 包2 | 選項A獨立驗收收尾 | 原reader重現與新checker分層；收斂002/003工具quiescence阻塞，193因果保留Unknown，不擴成無限平台 |
| 包3 | 量測契約B審查 | 最小publication/selection觀測、可否證假說、容量及observer effect；只審查，不自動R4/新cost |
| 包4 | X10d-O current-body ownership | 先同column兩Hold／舊tail新front／rotation/neighbor/absence真圖反例及合成控制，至多一個預聲明通用候選；不混pending-only、不復活suppression。研究不依賴193全部歸因 |
| 包5 | 候選live／全曲解鎖與IN验收 | conditional；完整行為／有效成本／exact freeze/preflight先成立，現有X12 gate未過不得自行開；同版完整IN Miss=0驗收 |

每包的輸入、可修改邊界、交付、總控驗收、停止條件及依賴以10/4總帳為準；完成後才續派。本包不建立goal/automation/下一chat、不啟動emulator／觸控／模型，不追加runtime/cost/stress/full replay。不把大型Fixture、標註平台、更多訓練或擷取重選當前置；unknown／proposed不升格gold，不偽造歷史Down。

## 已存在的成果與限制

| 項目 | 狀態／證據 |
|---|---|
| D1–D4、C0–C6 | 9/29可冷範圍已通過；線身分／關聯、旋轉Hold、混合動作、fake-clock、QPC冷鏈均已有實作。部分歷史性能子規則未過，見[冷結果](COLD_DEVELOPMENT_RESULT_20260929.md)和[合併驗收](COLD_DEVELOPMENT_MERGE_REVIEW_20260929.md) |
| 9/29–9/30 main 熱測 | observer47/24、48/25中止、49/26兩首後停止，須按版本與完成／中止分讀；不是main50效果，見[熱調試](FOUR_SONG_HOT_TUNING_20260929.md) |
| 恢復線 C36h | 同號37/19不是歷史main37/19；Dlyrotz IN13為795950／P496-G11-B0-M77，主要Miss未改善 |
| 全錄與人工選取 | 7722原生PNG、12段已填理由、3722 unique／3843 references；clip09是正常對照。131.829秒received pixels，最長capture gap443.5487ms；不是來源完全無漏幀 |
| main50離線研究／X1 | 歷史暖機observer7715次及960幀差異不換算Miss；後續完整fixed-pixels owner重播及R1/R2已獨立驗收，真觸控feedback／成本及physical gold仍缺 |
| CPU模型小試 | 4377參數、合成IoU .89267、18 native ROI proposal；真實人工pixel gold=0，真實／跨曲準確率unknown。本輪只核對既有工具與資料，未追加訓練 |
| 整合測試／整理 | 整合Release348通過／1 opt-in跳過／0失敗，CPU8/8；本輪清理後117/117相關回歸。實際清理與原檔SHA保護見[清理帳本](CLEANUP_AUDIT_20261001.md) |

## M0–M6 對應與範圍修訂

保留原[跨曲研究](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)作研究沿革。2026-10-01「模型只離線輔助」優先於舊版即時學習式觀測／GPU推論提案；選定跨曲方向不代表每個學習階段都必做。

| 階段 | 當前定位 | 尚缺證據 |
|---|---|---|
| M0 證據與根因 | X1及X2–X6／X9／X10c研究已存在；目前依10/4包1–4接續 | 物理ownership／judgment-role gold、逐Note採納、193skip因果仍Unknown；cancel／combo／ID不算Miss |
| M1 幾何與關聯 | 已有全域assignment、線局部運動、Hold接續與有界預測，繼續由反例修通用機制 | 多線／旋轉／晚對齊／反轉等實景語義；不能因形式複雜先推定需學習 |
| M2 跨曲資料pilot | 按歌曲／譜面家族切分、必要短窗與長Hold並存 | 現有18 ROI全為Dlyrotz development；不足跨曲訓練或驗收，不能湊幀數代替分布 |
| M3 單幀學習（可略） | 僅可條件式比較離線輔助標記品質與人工成本 | 未達相應資料gate，不追加無界合成選模；不再排入runtime部署 |
| M4 時序／關聯學習（可略） | 只有已證缺口需要時再研究離線用途 | 不以全曲時鐘、舊按鍵、未來幀充當正式即時策略；非本輪前置 |
| M5 有限實戰 | 凍結通用C++候選A/A、A/B，報所有曲及故障 | 既有有限授權仍受gate／總控續派約束；X12成本未過、main50 live0，不自行開 |
| M6 全曲IN／保留家族 | Chapter Legacy清單與解鎖先核，HD／IN分層；未知家族泛化另列 | 完整IN Miss=0是產品驗收，沒有AP前置；開發曲不算未知曲，資料不足只稱pilot |

## 必須保留的驗收邊界

- 旋轉時分開 Note朝向、運動方向、線法向；覆蓋遠處未對齊而接近才對齊，以及Hold按住期間線仍旋轉／平移。head接入、body接觸、tail結束各自驗證。
- 幾何過線不等於完成；返回不重播已Down／未知Down；同contact按當前支持Move，不固定初始點也不盲跟線旋轉。
- 固定gRPC fast／256KiB、latest-frame、QPC、五指指紋、單owner與硬上限。時間／排程改動需可重現合成軌跡／fake-clock，效能報完整分母、分位數與jitter。
- 兩輪歷史19HD：Miss815→818、非Perfect957→860，無AP；原始session、能力報告、獨立baseline及失败證據保留。清理不重寫實驗歷史，不把已刪ignored raw假稱可重算。
