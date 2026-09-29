# 清理執行紀錄與保留範圍（2026-09-28）

使用者在盤點後明確授權：只保留重要證據（尤其最近兩輪 measurements）、重要 out 與後續開發相關檔案，其餘可刪；同步更新文件，再繼續跨曲學習架構研究。本次已執行，取代本頁先前的「僅建議、不刪除」盤點。

## 執行結果

- 刪除 254 個具名目標、13,906 個檔案，檔案 Length 合計 **8,301,196,103 bytes（7.73 GiB）**。
- 清理前後核對 **6,159 個保留檔案，SHA-256 不一致 0**，涵蓋保留 measurements 以及重要 exe／baseline library／DLL。
- C 槽可用量當時由 125,306,077,184 → 133,645,021,184 bytes，增加約 7.77 GiB。這是系統當時快照，包含同期活動，不能當作每一刪除項目的精確磁碟貢獻。
- 清理前核對無相關執行／建置程序，目標解析後均位於本專案，無 reparse point。以 PowerShell LiteralPath 逐項刪除，未刪其他工作區或系統 TEMP。
- 清理後 Windows Release CMake configure 成功，使用原有已安裝依賴；沒有重新安裝套件。全專案文件的 133 個相對檔案連結檢查無缺失，git diff --check 通過。
- 本次沒有 Git commit／push。已追蹤的旧文件與 legacy 可從清理前 HEAD `baf3d4fcbaac56ab085e19b9fba8a5ef6615d3bc` 查閱；**ignored 舊量測刪除後不能靠 Git 恢復**。
- 清理後兩項既有離線回歸通過：歷史結算／選單圖辨識，以及合併前策略 golden。沒有 skipped，沒有啟動 emulator 或注入觸控。

詳細紀錄在 [deletion-manifest.json](../measurements/maintenance-20260928/deletion-manifest.json)（逐檔路徑／bytes／hash）、[protected-manifest.json](../measurements/maintenance-20260928/protected-manifest.json)、[result.json](../measurements/maintenance-20260928/result.json) 與 [測試結果](../measurements/maintenance-20260928/post-cleanup-tests.json)。這些 maintenance 紀錄也在 ignored measurements，另行備份時須包含。

## 保留的重要證據

| 範圍 | 保留原因 |
|---|---|
| `game-assist/manual-session-54180603650300` | 第一輪舊策略前半段，含故障／中止，不只留成功結果 |
| `game-assist/manual-session-99031291652700` | 第一輪待命 session，保留完整實驗分母 |
| `game-assist/manual-session-99700620346600` | 第一輪後半段及 Credits 重跑 |
| `game-assist/manual-session-108176133899800` | 第二輪 21 結算、全部原始 events、manifest、558 RGB／186 triples |
| `hd9-cross-song-20260928`、`main-legacy-20260928` | 兩輪整理、操作者紀錄與環境 |
| `cross-version-analysis-20260928` 的分析 JSON／具名案例 | 保留逐曲診斷、clip跨度與 Pixel Rebelz 三幀線 ID 案例；只清除可重建事件串接副本 |
| `game-semantics-20260927` | 五指 capability、mapping 指紋與動作後端驗證 |
| `outline-contact-20260927` | 現有結算辨識測試輸入、Hold／線失效圖與必要歷史反例 |
| `vision-dataset-20260927`、`tracking-segmentation-20260927` | 後續標註與追蹤研究可用的 ROI、mask／QA／candidate 資料；仍非人工 gold |
| `main-merge-20260928` | 獨立合併前 main library／exe、三配置驗證紀錄 |
| `capture_final_acceptance_20260926`、`grpc_production_20260926` | 固定擷取器與正式修補的重要驗收 |
| `fixture_cpp_v2`、`fixture_cpp_position_static`、`deps` | Fixture APK／建置資源與 bench 需要的第三方 server |
| measurements 根下 `hd9-*` logs／XML | 舊策略驗證與重建來源說明 |

上表 measurement 路徑均相對 `measurements/`。研究續作新增 `line-identity-research-20260928/`，屬本輪新離線結果。

## 保留的重要 out

- 現行 `release-v145`、`debug-v145`、`asan-buildtools-v145`：供後續開發與回歸。
- `vcpkg_installed/x64-windows` 及 status／info：已安裝依賴；不是下載快取。清掉 blds／pkgs 內容後重建空目錄，維持短路徑 junction 的目標。
- `hd9-original-release/Release`：重建的舊策略基準及 pas_core.lib，不冒稱最初歷史第九輪的原 binary。
- `hd9-session-release/Release`：第一輪實戰 binary＋DLL，exe SHA-256 `483cb83887d6daad9f01569aaca8f06c40e4e91df98f393b16a9c713c2534b76`。
- `main-legacy-v145/Release`：第二輪實戰 binary＋DLL，exe SHA-256 `de22022803a00ec0fd81ba19c8575b3728cb6e3bfd007ac73a360b08e08dc601`。
- 上述三個歷史 Release build 的 cache／generated provenance 保留，只有第一層 *.dir 中間產物移除；不要在当前 checkout 重建而覆寫凍結 binary。
- `research/tracking/reference-sources`：14 份上游精選來源與 manifest 保留；未完成 ByteTrack clone 刪除。
- `line-identity-research`：清理後新增的獨立研究 probe；不覆寫正式 pas.exe。

## 已移除的歷史範圍

1. legacy Python／Java／HTML 快照、35 份過期計畫／量測文件／遷移索引。正式 C++ 原始碼、tests、fixtures、configs、第三方原文與授權保留。
2. 早期 capture／五路徑矩陣／gRPC 研究批次、其 frozen binaries，以及舊遷移／恢復／cleanup raw。現行選型與重要正式驗收保留，但完整舊矩陣已無本地 raw 可重算。
3. `game-assist/cpp-observe-*` 單曲 AP 舊日誌。相關保留 ROI／歷史報告中的 run provenance 仍表示當時来源，**不能承諾其原始 events 還存在**；最近兩輪四個 manual-session 全數保留。
4. `out/hd9-session-debug`、`out/hd9-session-asan`、沒有實戰 pas.exe 的 `out/main-legacy-release`、舊 Android build、未完成 clone、vcpkg staging／Python bytecode。HD9 evidence JSON 的 Debug／ASan binary 路徑是歷史記錄，現已移除；驗證 log／hash仍保留，不能把舊 pass 改稱本輪重跑。
5. 14 份跨版分析的事件串接副本。刪除前逐份按原 round 的 `summary.json.event_segments` 順序串接 bytes，SHA-256 **14/14 相同**；保留原始 segments、分析 JSON（含 input hash）、diagnostics-summary、clip-spans 及 pixel-rebelz-line-identity-case.jsonl。

被刪檔案清單與原 hash 以 deletion-manifest 為準；不對歷史 evidence JSON 回填或刪改原有 hash／pass。保留的歷史 Markdown 加上資料保留界線，移除指向已刪文件的失效連結。原文件可用 `git show baf3d4f:docs/<檔名>` 查閱；這僅恢復有追蹤的文字，不恢復 deleted raw。

## 文件與後續維護

AGENTS／README／ARCHITECTURE／ROADMAP 已改寫成現行入口；[跨曲學習研究](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md) 為後續主線。舊 AP goal、不按歌名調參、pixels-only、QPC、有界状态与單一 owner 的界線已明確保留。

現有 `scripts/consolidate_main_legacy_20260928.py` 僅作兩輪歷史結算／hash重現來源，不是新正式資料工具或 runtime。IDE 設定保留作本機開發環境，不因名稱含 cache 就連同使用者工作狀態刪除。

此輪研究只新增離線 C++ probe 和結果，正式遊戲策略未修改。後續模型、採樣、實戰仍依新研究的分階段 gate 推進。
