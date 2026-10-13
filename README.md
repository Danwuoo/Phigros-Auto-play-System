# Phigros Auto-play System

以 C++ 建置的即時畫面處理與觸控控制研究系統，將畫面擷取、RGB／幾何辨識、時間處理及 session 控制串成可檢查的軟體流程。研究重點是辨識與輸入時序、資源管理、逾時停止，以及實際遊玩紀錄和受控測試之間的區別。

## 目前版本

現行預設為 **baseline／35 ms／score-only**，單輪上限 20 首；以 main80 保存元件搭配 manual-session 入口。`lazy` 是省略部分診斷 JSON 建立的明選實驗版本，不是預設，也尚未證明穩定升分或延遲改善。

版本與原碼指紋見 [current-approach.json](research/manual_score_only/review/current-approach.json)，結果與核對方式見 [現行版本與測試紀錄](research/manual_score_only/review/README.md)。現存正式狀態仍為 **cost `NOT_READY`／live `UNQUALIFIED`**；有限遊玩紀錄不改寫原本門檻或歷史負結果。

## 原碼閱讀路線

```text
畫面輸入 → 像素／幾何處理 → 時間與動作決策 → 觸控輸出
                         ↘ session budget / watchdog / stop
```

| 入口 | 內容 |
| --- | --- |
| [apps/pas/main.cpp](apps/pas/main.cpp)、[runtime.hpp](include/pas/runtime.hpp) | CLI、runtime 宣告與 `manual-session --score-only --rounds 1..20` 的輸入限制。 |
| [game.cpp](src/game.cpp)、[game_motion.cpp](src/game_motion.cpp) | 畫面與運動處理；以實際 RGB／幾何邏輯理解方法，不將其誤稱為已訓練的深度學習模型。 |
| [manual_session.cpp](src/manual_session.cpp) | Baseline session 流程、結算後釋放觸控與停止處理。 |
| [manual_score_budget.hpp](include/pas/manual_score_budget.hpp)、[budget tests](research/manual_score_only/budget_tests.cpp) | 單首、曲間與總 wall-time 限制及控制測試。 |
| [lazy source](research/manual_score_only/variants/lazy/manual_session.cpp)、[diagnostic helper](include/pas/manual_score_diagnostics.hpp) | 與 baseline 分開保留的診斷省略實驗。 |
| [CMakeLists.txt](CMakeLists.txt)、[vcpkg.json](vcpkg.json) | 一般建置設定與依賴入口。 |

## 保留的遊玩紀錄

| 組別 | 筆數 | 來源與解讀 |
| --- | ---: | --- |
| Baseline／35 ms | 20 | [Chapter Legacy IN 主表](research/manual_score_only/review/evidence/chapter-legacy-in-20261013.json)，本輪可用曲目的分數覆蓋。 |
| Baseline／35 ms 重複 | 1 | [A/A 紀錄](research/manual_score_only/review/evidence/chapter-legacy-aa-20261013.json)，用於觀察同設定波動。 |
| Baseline／40 ms | 3 | [Lead 比較](research/manual_score_only/review/evidence/chapter-legacy-lead40-20261013.json)，未採為預設。 |
| Lazy／35 ms | 3 | [Lazy 比較](research/manual_score_only/review/evidence/chapter-legacy-lazy-lead35-20261013.json)，兩升一降，樣本不足以建立穩定改善。 |

20 首是分數覆蓋，不是一次無人介入的連續完整測試：前五首因舊分數未記錄而補測，「光」來自使用者先前操作後的當前結算畫面。資料只保存曲名、IN 等級、分數與備註，沒有逐幀影像、影片、touch trace 或完整判定明細；不能据此宣稱 AP、零 Miss 或完整自動選曲能力。

## 不啟動遊戲的核對

Windows PowerShell 的唯讀入口如下，會核對 source／evidence SHA、分數欄位與預設參數，不啟動 runtime 或遊戲：

```powershell
& ./research/manual_score_only/verify-review.ps1
```

只有保留原執行檔的本機環境，才可加上 `-LocalArtifacts` 核對 binary SHA。可直接比較兩個 session 版本：

```sh
git diff --no-index -- src/manual_session.cpp research/manual_score_only/variants/lazy/manual_session.cpp
```

`git diff --no-index` 發現差異會回傳 exit 1，這不是測試失敗。既有 budget／入口拒絕控制只支持各自軟體範圍，不等於完整建置、辨識正確率或裝置時序驗證。

## 建置與執行前提

[build.ps1](research/manual_score_only/build.ps1) 和 [standby.ps1](research/manual_score_only/standby.ps1) 預設 baseline，lazy 須明確指定。現有建置會重用本機保存的 Windows toolchain、main80 libraries、overlay objects 與 DLL；已測入口僅 fresh 編譯部分 translation units，不能稱為 whole fresh build。

一般 clone 可閱讀原碼與分數副本，但**不能只靠該腳本重建原已測 binary**。執行還需要既有 emulator、token gRPC 與能力指紋檔。新建置應指定新的 output root，不覆寫已保存的原執行檔，也不沿用舊實跑身分。

## 歷史與證據保存

逐日開發、STOP、失敗與回復紀錄原文保留於 [DEVELOPMENT_HISTORY_20261013.md](DEVELOPMENT_HISTORY_20261013.md)，它是舊首頁的原樣快照，不是現行操作指令。原 manifest 中「本機未提交」等 publication 字串描述建立當時的狀態，不能用來判定目前 GitHub 分支是否已提交。

[2026-10-13 完成紀錄](research/manual_score_only/result-20261013.json) 中的 lazy 預設，是較晚改選 baseline 之前的歷史決定。[main80／main89 比較](docs/research/live-readiness-20261007/MAIN80_ROLLBACK_AND_MAIN89_COMPARISON_20261012.md) 與其他原始測量保持不變；本次文件更新沒有啟動遊戲、重建 binary、續跑 cold readiness 或新增資格。
