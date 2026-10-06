# 文件總目錄

**最新範圍：2026-10-06 Windows 離線接手及隔離橋接里程碑；原完整冷契約仍36fail，正式 owner／scheduler 前綴113/113，256張真圖沒有 eligible action。正式runtime與實機尚未接入，成本與全曲驗收未成立。**

先讀[10/6撤回驗收與自行開發](research/zero-miss-20261006/windows/WITHDRAWAL_ACCEPTANCE.md)、[Windows 收據與限制](research/zero-miss-20261006/windows/README.md)，再讀[10/5 第四階段候選與交接](research/zero-miss-20261005/round4/README.md)。[第三階段](research/zero-miss-20261005/round3/README.md)保留54→43fail，[第二輪](research/zero-miss-20261005/round2/README.md)保留首次54fail及早期Windows收據，[首輪](research/zero-miss-20261005/README.md)保留架構盤點。較早[開發現況](status/DEVELOPMENT_STATUS_AND_ISSUES_20261005.md)是歷史停止快照；舊BUILD／STOP沒有重開。

## 建議閱讀順序

0. [10/6 Windows 里程碑](research/zero-miss-20261006/windows/README.md)／[目標與狀態](goals/LEGACY_IN_ZERO_MISS_GOAL.md)：核驗、實作、原套件負結果、真圖阻塞、完整成本與裝置授權界線。
1. [10/5 第四階段交接](research/zero-miss-20261005/round4/README.md)：凍結v3及原36fail；[較早開發現況](status/DEVELOPMENT_STATUS_AND_ISSUES_20261005.md)保留當時停止點。
2. [Chapter Legacy 目標與逐曲基準](catalog/01-project/LEGACY_IN_ZERO_MISS_BASELINE_20261004.md)／[逐曲 JSON](catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json)：IN 解鎖與 Miss=0 的產品分母與限制。
3. [歷史 BUILD 結果](catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_BUILD_RESULT_20261005.md)／[交接](catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_BUILD_HANDOFF_20261005.md)：當時預驗18/20；10/6另在新root達20/20及native控制，不改舊收據。
4. [R2F control 結果](catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_R2F_CONTROL_RESULT_20261005.md)：三真控制通過，但 configure 因 Windows 語法錯誤停止。

## 依主題查找

下列八類完整收錄整理前的141檔（根目錄120檔、第三方授權21檔），每份原檔只歸一類。

| 分類 | 原檔數 | 用途 |
|---|---:|---|
| [目標、架構與路線](catalog/01-project/README.md) | 13 | 產品方向、架構及不同時點的規劃入口。本機僅獲離線開發授權；舊文件中的續派與最新字樣不代表裝置授權。 |
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
- 使用者最新授權決定範圍；10/6為本機離線接手，裝置讀取／Fixture／遊戲另須相應授權。不得由歷史派送或研究建議自行啟動實機。

## 整理與保全

原141份文件已實際搬入八個分類資料夾；Markdown導覽連結與README／AGENTS入口已更新。授權原文與JSON保持bytes；歷史收據、封存腳本中的原路徑與SHA仍代表當時快照，不回寫成新證據。新舊路徑及搬移前後SHA見 [搬移對照與核驗](status/DOCUMENT_RELOCATION_20261005.md)。

維護查核與新增容量見 [本次整理收據](../measurements/documentation-organization-20261005/receipt.json)。這是文件維護，不是新候選驗收或開發授權。

## 2026-10-05 第四階段雲端開發完成／交接留存

[第四階段結果](research/zero-miss-20261005/round4/README.md)與[自足Codex接手prompt](research/zero-miss-20261005/round4/CODEX_HANDOFF_PROMPT.md)。候選c2dda1d；原3938仍36fail（原54修18、新0），新／獨立必要三配置回歸完成。正式主鏈與實機未驗收，停在真圖／Windows門檻，產品Goal未完成。
