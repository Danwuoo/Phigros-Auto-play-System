# 文件總目錄

**最新範圍：2026-10-05 新授權的有界雲端盤點、研究與隔離冷實驗已完成；正式功能開發與實機未恢復。**

先讀[雲端多分支 zero-miss 研究](research/zero-miss-20261005/README.md)與[ignored 最小檔案清單](research/zero-miss-20261005/IGNORED_INPUTS.md)。本次 BVI 核心 Linux 小型 probe 已編譯／通過，完整 Windows harness、原 frozen suite 與真機仍未驗。較早的[開發現況與問題總整理](status/DEVELOPMENT_STATUS_AND_ISSUES_20261005.md)保留當時停止快照；最新歷史 BUILD 包仍為 STOP，未續跑。舊 README／路線圖中的「O 未開始」等文字依其日期解讀。

## 建議閱讀順序

0. [10/5 雲端研究總結](research/zero-miss-20261005/README.md)：四分支查核、新冷實驗、Miss根因樹、優先實驗與逐曲驗收；不是遊戲改善宣稱。
1. [目前開發現況與問題](status/DEVELOPMENT_STATUS_AND_ISSUES_20261005.md)：已有能力、逐包停止點、真實阻塞、剩餘驗收階段。
2. [Chapter Legacy 目標與逐曲基準](catalog/01-project/LEGACY_IN_ZERO_MISS_BASELINE_20261004.md)／[逐曲 JSON](catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json)：IN 解鎖與 Miss=0 的產品分母與限制。
3. [最新 BUILD 結果](catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_BUILD_RESULT_20261005.md)／[交接](catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_BUILD_HANDOFF_20261005.md)：新增診斷預驗18/20，真native前停止，原建置封裝仍未定位。
4. [R2F control 結果](catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_R2F_CONTROL_RESULT_20261005.md)：三真控制通過，但 configure 因 Windows 語法錯誤停止。

## 依主題查找

下列八類完整收錄整理前的141檔（根目錄120檔、第三方授權21檔），每份原檔只歸一類。

| 分類 | 原檔數 | 用途 |
|---|---:|---|
| [目標、架構與路線](catalog/01-project/README.md) | 13 | 產品方向、架構及不同時點的規劃入口。開發目前暫停；舊文件中的續派與最新字樣不代表本輪授權。 |
| [實戰、基準與逐曲結果](catalog/02-game-results/README.md) | 14 | 历史HD／IN、熱調試與比較證據。依版本與日期分讀；不是目前解鎖清單，也不是新BVI的實戰驗收。 |
| [Ownership 與 BVI 工作包](catalog/03-ownership-bvi/README.md) | 29 | X10d-O、4R、4I與後續BVI恢復／建置交付。BCC-v1負結果、BVI未編譯、工具控制通過三種狀態必須分開。 |
| [幾何、Hold 與離線回歸](catalog/04-offline-research/README.md) | 21 | M0/M1、冷開發、X1–X10及pending研究；合成／fixed-pixels證據不等於遊戲對新觸控的回饋。 |
| [成本、runtime 與 A／B 量測](catalog/05-runtime/README.md) | 26 | X11-P/R1/R2/R3及A/B的protocol、結果、交接與驗收。X12仍not-ready；設計簽收不等於量測已實作。 |
| [擷取、工程整合與資料保全](catalog/06-engineering/README.md) | 10 | 擷取選型、觸控能力／語義、全錄、合併及清理紀錄。操作指令是歷史參考，不授予本輪執行。 |
| [資料、標註與離線模型](catalog/07-data-learning/README.md) | 6 | 視覺資料、追蹤比較、proposed標註與離線CPU小試。模型不進正式即時閉環，unknown不升格人工gold。 |
| [第三方依賴與授權](catalog/08-licenses/README.md) | 22 | 第三方聲明與原始授權文字，保留授權原文bytes。分類不修改授權內容。 |

## 文件用途與狀態

- RESULT 是該包結果；HANDOFF 是接手說明；PROTOCOL／DISPATCH／PLAN 是當時契約或規劃。
- CONTROLLER_ACCEPTANCE／REVIEW 的驗收範圍可能只到保全、設計、工程控制或冷契約；不能只看檔名就當作產品已驗。
- 日期相同不代表同一來源／binary；source SHA、attempt 與收據才是辨識依據。
- 使用者最新授權決定範圍；本次僅有界雲端研究，不得由歷史派送或研究建議自行恢復功能開發或實機。

## 整理與保全

原141份文件已實際搬入八個分類資料夾；Markdown導覽連結與README／AGENTS入口已更新。授權原文與JSON保持bytes；歷史收據、封存腳本中的原路徑與SHA仍代表當時快照，不回寫成新證據。新舊路徑及搬移前後SHA見 [搬移對照與核驗](status/DOCUMENT_RELOCATION_20261005.md)。

維護查核與新增容量見 [本次整理收據](../measurements/documentation-organization-20261005/receipt.json)。這是文件維護，不是新候選驗收或開發授權。
