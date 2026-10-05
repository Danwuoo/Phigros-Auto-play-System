# 第二輪獨立核驗與重現邊界

日期：2026-10-05；研究父commit：`ebe955a6286ca1bb767b631948714a5545b7c174`。

## 已獨立重做

1. 使用者ZIP實際bytes／SHA、28唯一entries／安全相對路徑／無symlink、26份derived檔的bytes／SHA及README SHA全部核對。原包只放repo外。
2. 原解析度查看C4 PNG，直接讀到Dlyrotz、IN13、795950、P496/G11/B0/M77、Combo120、Accuracy86.16%；與另一分支的圖面覆核及tracked `/runs/88`一致。這不是獨立的人類gold，亦不是新實戰。
3. 重新執行 [Windows receipt audit](evidence/windows-receipts/audit_receipts.sh)，產出的JSON與保存的 [receipt-audit.json](evidence/windows-receipts/receipt-audit.json) 逐byte相同。沒有執行Windows命令。
4. 使用最終 [run_frozen_suite.sh](evidence/bvi-frozen-suite/run_frozen_suite.sh)，另建 `independent-release` 新根，從原core/driver與相同frozen inputs重新編譯、重新執行wrong-contact及完整suite；全輸入前後SHA保持。

獨立完整suite native exit1、runner aggregate exit1，89×4 layer-cases、3938 assertions、54fail，與canonical報告逐byte相同。

- [獨立run完整紀錄](evidence/bvi-frozen-suite/independent-release/run.log)
- [獨立suite](evidence/bvi-frozen-suite/independent-release/suite.json)
- [獨立摘要／全部failed rows](evidence/bvi-frozen-suite/independent-release/summary.json)
- [canonical suite](evidence/bvi-frozen-suite/typed-singleton-release-verified/suite.json)

上述suite及Debug／sanitizer解壓後的共同SHA：

`8fbc37e8db4afe1d8736b2f89c17f25dc91d2213581d241279c6ab2dbda38611`

一致的失敗是可重現負結果，**不是全部測試通過**。54個failed assertions的分類仍完整保留；沒有把fixture問題從frozen總數裡刪掉。

## 不隱藏原失敗或未完成輸出

- `release/`及`diagnostics/`是未加shape adapter的原reader結果：成功編譯、wrong-contact正確拒絕，完整suite以type305／exit2中止。其suite.json仍是pending reservation，不當作完整結果。
- 這兩根的 `post-audit-summary.json` 是0-byte stdout捕獲，因外部audit拒絕未完成suite而沒有JSON輸出；對應stderr／exit保留。**它們不是有效summary，不能宣稱全部JSON都有完整結果。** 文件QA只對非空JSON解析；兩份空捕獲檔另列預期未產生輸出。
- 初期collector只保存各native exit、最後shell回0；最終runner已改成以完整結果核驗回傳aggregate非零。舊logs保持，變更見 [collector-protocol-history.json](evidence/bvi-frozen-suite/collector-protocol-history.json)。
- Debug／ASan+UBSan與Release有相同54fail；ASan/UBSan沒有額外診斷，不把它說成契約通過。LSan明示關閉。

## 交付檢查

- 正式 `src/`、`include/`、`configs/`、`tests/`、`fixtures/`、原 `research/`、`tools/`、根CMake及presets對父commit零diff；只新增本輪文件／隔離工具／衍生結果，更新兩份導覽README。
- 第一輪 `evidence/research-files.sha256` 所列檔保持原樣；新一輪另有自己的 `research-files.sha256`，不回寫舊證據。
- 非空JSON語法、研究相對連結、shell語法、SHA清單、staged whitespace均檢查；空的失敗stdout例外如上。原始run log的命令列尾空白與官方patch的context空白原樣保留，未為消除格式warning改寫證據bytes；文件／source／shell的獨立whitespace檢查通過。細項在 `delivery-checks.txt`。
- 檔案中未納入原證據ZIP、PNG、private manifests、Library參照、原JSON header、可執行binary／object／cache；沒有憑證或原username路徑。一般匿名路徑／Windows公共系統路徑只用來解釋收據。
- 重跑需原repo及使用者提供的小包、GCC／bash／jq，以及報告所pin的官方JSON header。下載依賴與讀取私有包不是腳本的隱藏副作用，prepare腳本只對已取得檔案套官方patch與驗SHA。

完整 Windows／PowerShell預驗／原process controls／MSVC／owner／真ROI或events replay／runtime成本／裝置與遊戲均未跑，不能由這些Linux結果代簽。
