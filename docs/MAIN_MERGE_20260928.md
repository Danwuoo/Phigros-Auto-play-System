# Legacy HD 跨曲分支合併紀錄

> 2026-09-28 清理後註記：本文保留當時版本與證據界線，舊授權／待辦不作現行指令。後續方向見[跨曲學習研究](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)；原始資料與 binary 的現存／已刪範圍見[清理紀錄](CLEANUP_AUDIT_20260928.md)。歷史 JSON 的 hash／pass 不代表被刪的 raw 仍可重算。

日期：2026-09-28。使用者完成 Chapter Legacy 跨曲測試後授權合併並研討學習式方法。本次不恢復舊 AP goal，不啟動遊戲、採樣或模型訓練。

## 版本與資料保留

- 合併前 main：`5ad759ef5004ab89be8f1a326e75fb96e7cb9a0e`，observer36／planner18。
- 來源分支：`codex/hd9-manual-rounds`，`c89d906d00f0da224f5e17d8cf58c529e8b05038`；由最高分第九輪 `1636519b9e3ddd472fad1d7494f5bc7525cc5fc1` 建立，observer33／planner13。
- 合併採雙親 merge，保留 main 的辨識、追蹤、預測、planner／scheduler，加入多輪手動待命、結算紀錄及跨曲證據。README、ARCHITECTURE、ROADMAP 的衝突保留歷史並新增當前版本說明；runtime.hpp 同時保留 GameRunBudget 與新入口宣告。
- 合併後 `manual-session` 使用 observer36／planner18，manifest 與 CLI 說明已更正，不冒稱 HD9。此次20張結算均不是合併後 main 的實戰成績。
- HD9 分支及 `out/hd9-session-release/Release/pas.exe` 保留，其 SHA-256 為 `483cb83887d6daad9f01569aaca8f06c40e4e91df98f393b16a9c713c2534b76`。本次沒有刪除或封存工作樹／原始量測，也未覆寫該建置目錄。
- 本機 `measurements/` 為 ignored；Git merge 不等於備份原始證據。本次重新核對索引內41份 round summary／result檔案雜湊，均相符（21份摘要含1次中止，20張結算）。

## 合併驗證

`src/game.cpp`、`src/game_motion.cpp`、`src/game_tracking.cpp`、`src/core.cpp`、`include/pas/game.hpp` 相對合併前 main 無差異。歷史 HD9 golden 保留，新增獨立 main golden；測試同時比較直接呼叫策略與包入新生命週期的結果。

main golden 使用合併建置前保存的 Release `pas_core.lib`，而非新編譯策略自產期望值。保存路徑為 `measurements/main-merge-20260928/premerge-main-pas_core.lib`，SHA-256 `c83c67db704cc9bcab13f1873803a1f4be3775c6d6eb5b50fb2d7a1e603882b7`。同時保存原 main 執行檔，SHA-256 `d7ce474576a0283711b046b82720f9c10e8de0bb92203eb999a6c8decc157162`。

以 `PAS_HD9_TRACE_BASELINE_LIBRARY` 指向上述獨立 library，建置 `pas_hd9_original_trace` 產生6種情境、108筆固定 pixels／QPC 的 decision、plan 與 receipt；工具名稱沿用原分支。新檔 `tests/data/main-strategy-golden.json` SHA-256 為 `83f7f3710ec05ae62b44577ad78f537f8de623f7b174b8a0b1a89b83fceca8e9`。正常建置已清空此選項，無需本機歷史 library。

Windows x64／VS2026 v145／既有 vcpkg 環境下，Release、Debug、嚴格 ASan 建置及 CTest 各 **206／206通過**，測試執行時間依序15.61／49.25／75.76秒（不是效能比較）。ASan 未設定放寬的 `ASAN_OPTIONS`；第三方依賴未全面插樁的既有限制仍適用。原始 log 位於 `measurements/main-merge-20260928/`。本次合成／離線驗證不代表合併後版本的跨曲實戰能力。

## 研究方向與證據界線

[跨曲報告](HD9_LEGACY_HD_RESULTS_20260928.md) 有20張完整結算、19個不同曲名（Credits兩次），另有先前獨立 Glaciaxion 基準。光5 Miss，Credits兩次133／130 Miss，顯示相同策略跨曲表現落差；結算與事件不能單獨證明每個 Miss 的成因。處理間隔仍有未歸因的尾端異常。

下一階段建議學習式模型提供當前音符 head／body／tail 與多判定線觀測；追蹤層維護各線身分及位置／方向的時間關係，再將音符關聯到線，於該線局部座標預測撞線。模型輸出與跨幀預測仍須區分當前可見支持、遮擋假設及不確定性，不能把預測當成新的像素證據。既有 ByteTrack／OC-SORT 候選對照無法補回候選提取器完全漏掉的物件。

本次只有結算圖與事件，不是20首完整訓練影片。後續需有限、多曲短片段採樣，標註音符與判定線的實例身分、幾何及關聯；按整首歌曲分割訓練／驗證／保留測試集，避免相鄰影格或同曲洩漏。比較應使用固定版本及統一參數，列出每曲與最差曲表現，而非按歌名選模型、閾值或播放腳本。正式推論保持 C++20、pixels-only、latest-frame 與有界狀態；本次僅研討，未選定或實作模型。
