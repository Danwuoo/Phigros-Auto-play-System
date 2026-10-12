# 回復 main80 與 main89 結果比較

依使用者「那還是回到舊版吧，上一版結果跟這版比較」，當前 main 的程式與建置／驗證工具已回復到 **main80／`3ee69edc85acb6b9b968424374a376f351d0e2dd` 的原碼**。main89／`0486ed3` 改為歷史候選。
這次是 source rollback 與既有結果比較；沒有重新編譯、重跑量測、啟動 emulator／device／CU、新 thread、ETW 或遠端 push。新 rollback commit 不是舊 binary 的編譯來源；歷史 binary／收據不能直接改掛新 HEAD 的資格。

| 原測試項目 | 上一版 main80 | main89 |
| --- | ---: | ---: |
| Normal 原數值通過 | **3／3** | 2／3，Hold 未過 |
| AA validity 通過 | **11／12** | 6／12 |
| noise 足夠 | **15／18** | 11／18 |
| AA＋ABBA 完整 raw audit | 48／48 | 48／48 |
| ABBA 完成收集 | 36／36 | 36／36 |
| ABBA 描述性必要條件有負結果的 run | **11／36** | 16／36 |
| ABBA 有任一 producer／publication 超期的 run | **15／36** | 24／36 |
| ABBA 原 producer／publication 超期次數 | 945／949 | 1052／1058 |
| 描述性比較在各自原 T 內 | 52／54 | 52／54 |
| 原 formal comparator／cost／live | REJECT／NOT_READY／UNQUALIFIED | REJECT／NOT_READY／UNQUALIFIED |

main80 的有效性、noise 足夠數與較少的 ABBA 負結果，支持此次回復基準的選擇。兩輪在不同時間量測，噪聲與 T 分別由各自 AA 固定配對取得；**這是既有結果對照，不能證明 main89 的兩項改動造成退步，也不能保證回復後結果必定相同。**
超出各自 T 的兩個比較項目，main80 在 Dense observer.p95 的 batch1／2，main89 在 Hold batch1 的 observer.p99／capture_to_owner.p95。main89 Dense18 cells 都在其本輪 T 內；但其 Dense observer.p95 T 為0.99392ms，main80 為0.5485ms，且兩版都有 Dense p99 noise 不足，不能當成共同門檻下的因果改善。

main80 仍保留原 Hold3 publication 超期33.9µs、三項 noise 不足及 ABBA 較大空窗（已核對的 measurement start lateness1770.799ms、publication span440.7352ms）。main89 的已核對 measurement lateness379.6099ms、publication span95.9341ms也未消除問題。舊版不是每個空窗指標都優於新版；此次依整體有效性與穩定性結果回復，**兩版都尚未實機 ready**。不刪除負結果、加 tolerance、放寬 R2 或把回復當作 gate PASS。

## 實際回復內容與驗證

- 12 個既有 tracked files 恢復舊版 exact bytes，包括 `src/game.cpp`、cold chain、建置／loader／reader／admission 工具；metadata reuse 與白色元件提前 skip 都已撤回。
- 15 個實驗新增的 tracked files 從現行原碼移除，包括額外 diagnostic 工具及 Dense／Frame delta helpers／controls。其所有內容仍保存在既有 Git commits 與 source snapshots／可恢復 archives。
- `research`、`src`、`include`、`tests` 與 root CMake 的 tracked 內容已逐 Git blob／SHA 核對，与 main80 完全一致。保留 main80 原 motion delta、producer eligibility 與既有 gates。
- 原 main80／main89 歷史文件、STOP、Native8、原始測量、archive／restore 索引均保留。沒有 reset／改寫 Git 歷史或覆寫原測試結果；沒有删除歷史 binary／raw payload。

本輪只驗 exact source 回復、必要 PowerShell syntax 與文件／比較資料一致性，沒有以新量測宣稱 readiness。後續若執行仍須用正確 source／binary／build 身分與既有門檻；已封存的原 path 重驗需先 exact restore，不能直接套用 main89 bindings 到回復原碼。

[回復逐檔 SHA 與比較 JSON](MAIN80_ROLLBACK_AND_MAIN89_COMPARISON_20261012.json)保存兩版原證據 pins、18 項各自 noise／T、描述性負結果與回復清單。
原[main80 完整結果](MAIN80_AA_ABBA_COMPLETION_REVIEW_20261012.md)及[main89 完整結果](MAIN89_ALL_COLD_COMPLETION_20261012.md)保持原件。
