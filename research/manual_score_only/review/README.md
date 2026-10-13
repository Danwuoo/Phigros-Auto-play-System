# 現行版本與測試紀錄

專案入口見 [根 README](../../../README.md)。目前預設為完成 Chapter Legacy 本輪 20 首可用 IN 分數收集的 **baseline／35 ms／score-only**；診斷 JSON 省略版 `lazy` 保留為明選實驗。原 cost `NOT_READY`／live `UNQUALIFIED`、歷史負結果與 STOP 不變。

## 版本與原碼

| 版本 | 用途 | 保留的測試範圍 | 原碼 |
| --- | --- | --- | --- |
| baseline | 預設，35 ms、最多 20 首 | 20 首分數覆蓋；92 項 budget 與 5 項入口拒絕控制有既有通過記錄 | [manual_session.cpp](../../../src/manual_session.cpp) |
| lazy | 明選實驗 | 16 項診斷省略軟體檢查、3 首實跑；沒有完整 20 首測試 | [保留原碼](../variants/lazy/manual_session.cpp) |

兩者共用 main80 保存元件及 score-only 入口，每首結算後釋放觸控，達首數限制後停止。Lazy 省略 score-only 中不使用的診斷 JSON 建立；三首分數有升有降，尚未證明穩定升分或延遲改善。

[版本與 SHA 索引](current-approach.json) 保存來源、建置身分和證據 pins。其 publication 欄位是原文件建立時的歷史文字，不是 GitHub 即時狀態；不要為更新文字而改寫來源或原始結果指紋。

## Session 控制

[CLI](../../../apps/pas/main.cpp) 與 [runtime 宣告](../../../include/pas/runtime.hpp) 提供 `manual-session --score-only --rounds 1..20`，拒絕錄影／clips 及不相容選項。[Budget](../../../include/pas/manual_score_budget.hpp) 定義單首 10 分鐘 watchdog、曲間待命 30 分鐘與總 wall-time 5 小時上限；取消或故障仍停止，未完成曲目不計入結算數。

[Launcher](../standby.ps1) 和 [建置腳本](../build.ps1) 預設 baseline。Lazy 使用獨立來源與 [diagnostic helper](../../../include/pas/manual_score_diagnostics.hpp)，對應 [16 項軟體檢查](../lazy_archive_tests.cpp)。比較來源可用：

```sh
git diff --no-index -- src/manual_session.cpp research/manual_score_only/variants/lazy/manual_session.cpp
```

此命令從 repository 根目錄執行；有差異時 Git 回傳 exit 1。

## 分數與建置資料

| 資料 | 筆數 | 限制 |
| --- | ---: | --- |
| [Baseline 主表](evidence/chapter-legacy-in-20261013.json) | 20 | 前五首補測；「光」取自使用者先前操作後的當前結算畫面。是本輪分數覆蓋，不是新 formal qualification。 |
| [Baseline lead35 重複](evidence/chapter-legacy-aa-20261013.json) | 1 | Winter 重複分數為 799,236，保留同設定波動。 |
| [Baseline lead40](evidence/chapter-legacy-lead40-20261013.json) | 3 | 調參樣本，未採為預設。 |
| [Lazy lead35](evidence/chapter-legacy-lazy-lead35-20261013.json) | 3 | 兩升一降，未完成完整章節比較。 |

檔案是原 `measurements/game-scores` 的 exact-SHA 副本，只包含曲名、IN 等級、分數與簡短狀態。沒有以歷史最高分替代，也沒有挑最好分數覆蓋主表；未附影片、逐幀、touch 或 trace，因此不支持 AP／零 Miss 等判定。

[Baseline build](evidence/baseline-build.json)、[lazy build](evidence/lazy-build.json) 與 [lazy contract log](evidence/lazy-contract.log) 保留原 bytes。兩個入口各 fresh 兩個 TU，其餘重用 main80 objects／libraries，不是完整 fresh build。[原完成紀錄](../result-20261013.json) 的 LINK_FAILED 及較早 lazy 預設保持歷史原貌。

## 唯讀核對與本機條件

```powershell
& ./research/manual_score_only/verify-review.ps1
# 僅在保留原執行檔的 Windows workspace 使用：
& ./research/manual_score_only/verify-review.ps1 -LocalArtifacts
```

核對只讀來源和證據，不啟動程式或遊戲。原 binary、emulator、token gRPC 及能力指紋檔是本機執行前提；`standby.ps1 -Variant lazy` 或 `-LeadMs 40` 都是明選比較設定。

重建使用 `build.ps1 -Variant baseline` 或 `-Variant lazy`，並指定新的 `-OutputRoot out/<fresh-name>`。需要本機保存的 Windows toolchain、main80 libraries／overlay objects 與 DLL；乾淨 clone 不能僅靠此腳本重建原 binary。新產物不繼承旧實跑身分。本文件整理未追加遊玩、cold readiness 測試或研究結果。
