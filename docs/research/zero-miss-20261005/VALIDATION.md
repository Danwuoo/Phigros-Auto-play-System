# 本輪實驗與交付核驗

基準：`74e54437d4a3ad2b2bd1a3b09312211a92f2359e`。日期：2026-10-05。

## 1. 可以重跑的內容

從完整 repo root、Linux＋GCC C++20 環境執行：

```sh
bash docs/research/zero-miss-20261005/evidence/verify-cloud-probes.sh
bash docs/research/zero-miss-20261005/evidence/verify-ledger.sh
```

此 shell 只編譯／執行隔離研究 probe，產物放新的 `/tmp/phigros-cloud-review.*`；不下載依賴、不連網、不建立 emulator／transport／遊戲。它直接編譯原有 BVI core／scheduler，scheduler 使用[明示的 Linux clock shim](evidence/touch-scheduler/shim/windows.h)。這不是 Windows clock／MSVC／完整正式 runtime 的移植或驗證。

輸出：[整合獨立重跑 log](evidence/cloud-probes-verification.log)、[tracked JSON 查核 log](evidence/ledger-verification.log)。

| 檢查 | 實際範圍與結果 | 不是甚麼 |
|---|---|---|
| BVI core | 28/28新研究 assertions，含合成RGB與typed／fake constraint | 原89×4＝356層cases、22supplemental、4controls完整suite |
| Scheduler | 13個fake-clock cases／66 checks，全部通過 | 真RPC延遲、owner全鏈、真五指採納 |
| Flick scheduler | 額外1個case，原4個過期Move在同一poll送出；assertions通過 | Flick因此Miss或遊戲速度窗口已量測 |
| 時間/preserve公式 | 獨立C++算術輸出如期，來源與限制在log及分支報告 | 正式observer/tracker運行、已校準source age |
| BVI typed靜止端部 | 兩組各4幀，原core的新Down資格差異如期 | 物理owner正確性、真圖extract |
| BVI synthetic RGB靜止Tap | 原core extract→relate→constrain與移動Tap對照如期 | 正式GameObserver、真實ROI/all-lines、遊戲Miss |

Probe 按現有 source 與契約撰寫，不是獨立人工 gold。通過既有 guard 的 smoke 和揭露 guard 機會缺口的反例可以同時成立；不改 oracle 或正式source 來讓結果好看。

分支各自的編譯命令、warning、binary/source SHA及詳細結果：

- [BVI建置報告](BUILD_TEST_AUDIT.md)，`evidence/build-audit/`；結構化結果 [probe-results.json](evidence/build-audit/probe-results.json)。
- [視覺/時間證據說明](evidence/vision-timing/README.md)。
- [觸控排程報告](TOUCH_SCHEDULER_RESEARCH.md)，`evidence/touch-scheduler/`。
- [逐曲驗收報告](LEGACY_ACCEPTANCE_RESEARCH.md)，包括總帳解析與模板查核命令。

## 2. Sanitizer 與保留失敗

- BVI core probe 與13-case scheduler probe均另以 GCC AddressSanitizer＋UndefinedBehaviorSanitizer 執行；**明示關閉 LeakSanitizer 後通過**。
- 最初啟用 leak detection 的執行被當前環境的 ptrace 限制中止，exit1／stderr保留在各分支log。這是環境阻塞，不是leak-free證據；不能宣稱LSan或MSVC ASan通過。
- GCC 的原source warning保留，主要為既有 condensed code 的 indentation／unused參數；本次沒有為消warning修改正式程式。
- 整合第一次重跑漏給額外Flick probe的include/link參數，在編譯時失敗，**沒有執行該case**。原log保留為 [cloud-review-initial-command-error.log](evidence/cloud-review-initial-command-error.log)；只修新研究驗證shell的參數後重跑成功，未改source／fixture／期待值。這是研究命令作者錯誤，不歸因為產品缺陷。

## 3. 資料／文件／程式邊界

- 本次只改兩份導覽 README，新增 `docs/research/zero-miss-20261005/` 研究文件、JSON模板、小型C++ probe、shim、shell及文字收據。未修改 `src/`、`include/`、`configs/`、`tests/`、`fixtures/`、`research/`、`tools/`、根 CMake 或 presets。
- 新JSON均可解析；總帳完整引用與PGBM一致性另列查核，不把歷史 `exists:true` 當作此雲端有原檔。
- 研究資料夾中 Markdown 相對連結作存在性檢查。歷史文件中指向未隨Git提供的 `measurements/` 仍保持原指向，不造假檔補洞。
- 新檔案作不必要個資／secret形態掃描；原始歷史JSON保留不動，新模板若遇本機username絕對路徑改成明示的衍生相對路徑，仍可由原JSON索引追溯。
- 所有小型研究交付的SHA列在 `evidence/research-files.sha256`（不自含此manifest）；它校驗交付bytes，不替原實戰資料作驗證。

## 4. 完全沒有跑的層次

完整根專案CMake／Windows MSVC Release/Debug/ASan、PowerShell診斷20項、真process controls、新Windows wrapper、原frozen BVI整套JSON cases、原7722 PNG replay、raw result圖覆核、實機capability/五指mapping、模型訓練、runtime成本、遊戲或全章節IN。

其中原JSON harness實際語法編譯嘗試停在缺nlohmann header；CMake命令不可用。這些阻塞及原始exit在建置報告，不寫成pass。未啟动部分則是未跑，不寫成failed或skip綠燈。

研究已完成不等於產品目標已達成；原Windows syntax failure、當前章節分母、逐曲解鎖與真實Miss根因仍有明列未知。
