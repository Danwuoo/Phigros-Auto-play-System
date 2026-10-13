# Phigros 現行基準與審查說明

專案目前採用完成 Chapter Legacy 20 首可用 IN 分數收集的 **baseline／35 ms**。最近的診斷 JSON 省略版 **lazy** 保留為明選的實驗版本。這次採用依據是已完成的章節測試範圍；原 cost NOT_READY／live UNQUALIFIED、歷史負結果與 STOP 保持。

## 採用版本與保留版本

| 版本 | 目前用途 | 實際測試範圍 | 原碼 |
|---|---|---|---|
| baseline | 預設 approach，35 ms、score-only、最多20首 | 20首可用IN分數已記齊；92項budget及5項入口拒絕控制有既有通過記錄 | [src/manual_session.cpp](../../../src/manual_session.cpp) |
| lazy | 保留實驗，須明選 | 16項診斷省略軟體檢查、三首IN；尚無完整20首測試 | [保留的manual_session.cpp](../variants/lazy/manual_session.cpp) |

兩個版本共用 main80 保存元件與 score-only 入口；每首結算後釋放觸控，達設定首數後停止。新版省掉 score-only 中會被丟棄的診斷 JSON 建立，三首分數有升有降，尚未證明穩定升分或延遲改善。原碼已還原至 baseline 建置時的 exact SHA；保留的 lazy 原碼亦符合其原建置 SHA。

「完整20首」指本輪可用IN的分數覆蓋：前五首因先前分數未記錄而補測；光由使用者先前操作後的當前結算畫面讀取。結果沒有以選曲畫面的歷史最高分替代，也沒有挑最好分數覆蓋主表。這不是新版lazy的20首測試，也不授新的formal資格。

## 審查內容

先讀 [採用決定與SHA索引](current-approach.json)，再對照以下變更。

1. [CLI入口](../../../apps/pas/main.cpp) 與 [runtime宣告](../../../include/pas/runtime.hpp)：`manual-session --score-only --rounds 1..20`，拒絕錄影／clips及不相容選項。
2. [session budget](../../../include/pas/manual_score_budget.hpp) 與 baseline manual session：單首10分鐘watchdog、多首曲間待命30分鐘、20首總wall上限5小時；曲間計時重設，取消／故障仍停止，未完成首不計入結算數。
3. [launcher](../standby.ps1) 與 [建置腳本](../build.ps1)：兩者預設baseline，lazy有獨立原碼路徑；重建使用新的output root，保存的已測binary不被替換。一般CMake沿用baseline，來源manifest亦收錄budget header。
4. [lazy診斷helper](../../../include/pas/manual_score_diagnostics.hpp) 與 [16項檢查](../lazy_archive_tests.cpp)：保留作實驗，預設baseline不引用helper。

比較實驗的純診斷差異可執行以下唯讀命令。Git會以exit1表示兩個檔案不同。

```powershell
git diff --no-index -- src/manual_session.cpp research/manual_score_only/variants/lazy/manual_session.cpp
```

## 分數與建置證據

| 證據 | 筆數 | 狀態 |
|---|---:|---|
| [baseline章節IN主表](evidence/chapter-legacy-in-20261013.json) | 20 | 完整本輪分數覆蓋 |
| [baseline lead35重複](evidence/chapter-legacy-aa-20261013.json) | 1 | Winter再次得799,236，保留波動反例 |
| [baseline lead40比較](evidence/chapter-legacy-lead40-20261013.json) | 3 | 調參樣本，未採為預設 |
| [lazy lead35比較](evidence/chapter-legacy-lazy-lead35-20261013.json) | 3 | 兩升一降，未升格為完整測試基準 |

這些是原 `measurements/game-scores` 檔案的 exact SHA 副本，僅含曲名、IN等級、分數與簡短狀態。沒有新增截圖、影片、逐幀、touch或trace。原件與原binary保持在本機。

[baseline建置身分](evidence/baseline-build.json)、[lazy建置身分](evidence/lazy-build.json) 與 [lazy軟體檢查輸出](evidence/lazy-contract.log) 同樣保留原bytes。兩個入口各fresh兩個TU，重用main80已保存的objects／libraries；不宣稱whole fresh build。先前validation runner選到空白link行造成的LINK_FAILED仍記於 [原完成紀錄](../result-20261013.json)，沒有把該負結果改成PASS。

原完成紀錄中「預設lazy」是這次使用者改選baseline之前的歷史決定。現行決定以本頁與 `current-approach.json` 為準。07:10提醒已完成停止核對，程式未在這次整理中啟動。

## 唯讀核對與本機操作

```powershell
& ./research/manual_score_only/verify-review.ps1
# 保存原binary的本機可另核對三個exe SHA
& ./research/manual_score_only/verify-review.ps1 -LocalArtifacts
```

核對只讀取source／證據、檢查SHA／分數欄位／腳本預設，不執行程式或測試遊戲。

人工啟動時，`& ./research/manual_score_only/standby.ps1` 使用已完成章節測試的原baseline binary。`-Variant lazy` 明選保留版，`-LeadMs 40` 明選原比較參數。啟動仍需原本的emulator、token gRPC與能力指紋檔。

重建可用 `build.ps1 -Variant baseline` 或 `-Variant lazy`，並以 `-OutputRoot out/<fresh-name>` 指定新目錄。腳本依賴本機保存的Windows toolchain、main80 libraries／overlay objects與DLL；clean clone可審查source及分數副本，不能直接依此腳本重建原已測binary。重建產生新artifact，不沿用原實跑身分。

本次交付是本機未提交的審查差異，未commit／push或建立PR；沒有追加實機遊玩或cold readiness循環。
