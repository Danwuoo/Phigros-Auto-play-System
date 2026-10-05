# 擷取研究最後驗收與選型決定

> 2026-09-28 清理後註記：本文保留當時版本與證據界線，舊授權／待辦不作現行指令。後續方向見[跨曲學習研究](../01-project/MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)；原始資料與 binary 的現存／已刪範圍見[清理紀錄](CLEANUP_AUDIT_20260928.md)。歷史 JSON 的 hash／pass 不代表被刪的 raw 仍可重算。

本文件為擷取器現行終態與後端選型的主紀錄。2026-09-26 最終狀態：**五路徑研究已結案，gRPC 接收層優化已完成正式開發與短測；本輪沒有待續跑的擷取研究或長測。** 終態指本輪交付範圍完成，不代表遊戲閉環、長期性能或所有候選均合格。

先前依使用者「完成，請幫我做擷取器的最後驗收與決定」完成縮短範圍覆核，後續再完成 gRPC 優化。下方原驗收數據保留其版本與時間範圍，不恢復已取消的 96 批完整矩陣或長測。

## 現行交付終態

- **主用**：C++20、單程序多執行緒的 gRPC payload fast，CPU RGB24／top-down、顯式尺寸及方向。Windows gRPC 1.81.1 overlay revision 2 已完整建置，畫面 channel 預設 **256 KiB**；CLI／profile 可選 8／64／256 KiB，未指定參數的其他 channel 保持 8 KiB。8／64 KiB 是同一路徑的比較設定，不是獨立備用。
- **正式備用暫缺**：WGC 僅為 bench 備援候選；DXGI 保留受限比較用途；本次 scrcpy 軟體 H.264 配置不列主／備；MMAP 僅診斷。沒有自動切換後端的資格或實作承諾。
- **儲存與緩衝**：latest-only、有界 frame pool；處理完成釋放引用並重用像素 buffer，不逐幀存圖。bench／campaign 預設不寫 PNG；明示 `--keep-diagnostic-image` 才每批保留一張。JSONL／設定／數值證據保留供重算。
- **正式版本驗證**：Release 34／34 回歸通過；loopback／Emulator 各 9 批短測（每批 1 秒暖機＋4 秒正式窗）均有效；正式 bench、500 ms 暫停恢復及 2 秒 observe 完成。修補、版本雜湊、樣本與分布見 [gRPC 正式接入紀錄](GRPC_TRANSPORT_INTEGRATION_20260926.md)。這批測試只驗證新 gRPC 版本，不把舊五路徑數據重標為新版本測試。
- **驗收界線**：loopback 中位數與 client CPU 改善已有證據；Emulator 到達間隔 p99 未一致改善，絕對 source age 仍 unknown。`performance_pass=null` 與原 campaign 未完成旗標不改寫，長期穩定性與遊戲操作時序尚未通過。
- **2026-09-26 當時交接點（已由後續遊戲開發承接）**：observe 已採用 256 KiB，但 Session/profile 尚未接入 bench 的 250 ms 相對 lag guard。該接線、證據失效撤銷與簡單目標 pixels→touch 閉環列入下一階段 M3；不是本輪仍待完成的五路徑研究。Phigros 辨識／assist 仍未開放。

## 驗收結論

**五條擷取路徑研究在縮短範圍內驗收結案；選定 gRPC payload fast 為下一階段 M3 的主用擷取基線。正式備用暫缺，WGC 保留為備援候選。**

這是後端選型決定，取代比較報告中的「初步主用」用語。不是原完整 campaign 通過，也不是完整 Session 整合、長期可靠性或 Phigros 遊玩性能驗收。原始 `campaign_complete=false`、`all_valid=false`、`performance_pass=null` 保持不變，不回填歷史結果。

| 路徑 | 決定 | 依據與界線 |
| --- | --- | --- |
| gRPC payload fast | **主用：M3 擷取基線** | 現行接收區塊預設 256 KiB；正常來源跟隨、像素正確性與短恢復證據支持選型；bench 相對積壓保護仍須接入 Session |
| WGC | **備援候選，未取得正式備用資格** | 像素與短恢復可用，但 120 秒來源跟隨只有 71.47%；不能以「能取圖」代替穩定交付，也未接入 Session |
| DXGI Desktop Duplication | 保留受限場景比較工具，不列主／備 | 嚴格依賴前景、未遮擋、單一未旋轉 output；本版已有两次環境失效拒絕，缺少同版完整恢復驗證 |
| scrcpy v4.1 軟體 H.264 | 本固定配置淘汰於主／備選型 | 48 Hz 批次有 2.148 秒空窗；57 Hz 有積壓保護終止，qemu CPU 成本高。結論不外推至未測硬體 encoder／其他 codec |
| Emulator MMAP | 維持 diagnostic-only | producer 一致性／ownership 未證明；無法因成功取圖或 reader 校驗取得正式資格 |

gRPC legacy-rows 保留為控制配置，與 fast 同屬一條來源路徑，不算能補足 gRPC 失效的獨立備用。fast 的複製階段改善不足以證明所有條件下 CPU 或端到端都有顯著提升，主用選定也不依賴這種宣稱。

## 選型時的獨立核對（接收層優化前）

審查起點：`main` 的 `78f9fec`，程式基線 `2d43dd74811aab75c3968a2c21aaea2dc4b28dbd`。核對兩者間 `src/`、`include/`、`tests/`、`apps/`、`fixtures/`、CMakeLists 與 vcpkg manifest 無程式差異。本次只新增验收證據與文件，沒有更改被測程式，也沒有再跑實機長測。

1. 證據索引內 **810/810 檔**存在，bytes 及 SHA-256 全部相符，無缺檔／不一致。索引本身 hash 亦符合原文件。
2. 凍結與目前 Release `pas.exe` hash 相同：`2447a4143a4b35f045f22e9345b462b4ed176bee20db9a937bfc38ab7ecc0d68`。
3. 直接執行目前 Release `pas_tests.exe`，**29/29 通過**，包括 LatestFrame lease／容量、gRPC 真 loopback／取消／來源序號與時間失效、幾何邊界、Fixture identity 交叉核對、原生相對時戳及 scrcpy 封包契約。這是既有測試二進位的重新執行；本次沒有重新建置或新增 sanitizer 驗證。
4. 以凍結 C++ analyzer 重新分析 r4 campaign：仍為 **37 個有效完整正常窗口**、環境核對一致；**93,473** 張消費樣本全部可解碼。原日程未完成，3 個失敗、1 個中斷及 55 個未開始不改寫。
5. 重新分析 **6/6** 短補測，**12,102/12,102** 消費圖可解碼，幾何／counter order 有效、source-invalid 計數零；每批 raw hash、消費數及主機駐留 p99 與保存 summary 一致。
6. 抽查主用來源的格式／尺寸／方向驗證、owned RGB、序號及相對時間保護、取消與 LatestFrame lease；另核對 WGC readback／callback 生命周期與 Session 接線界線。本次不是對所有驅動行為或每條錯誤路徑的全面證明。

本次新增證據位於 `measurements/capture_final_acceptance_20260926/`：

- `evidence-verification.json`：810 檔逐項核對。
- `campaign-recomputed.json`：r4 本次 C++ 重算。
- 六份 `*-recomputed.json`：補測本次 C++ 重算。
- `decision-metrics.json`：從重算結果抽取的選型數據與一致性核對。

測試輸出：`measurements/capture_final_ctest_20260926.log`（直接執行 GoogleTest binary，非本次 CTest runner）。歷史資料索引見 證據索引（CAPTURE_EVIDENCE_INDEX_20260926.md 已清理，歷史見 Git baf3d4f），詳細測法及所有失敗見 比較報告（CAPTURE_COMPARISON_20260926.md 已清理，歷史見 Git baf3d4f）。

## 支持主用決定的數據

本機 Windows／MSVC Release、Emulator 37.1.11、AVD 5 vCPU／8 GiB、host GPU、獨立視窗、1280×720 CPU RGB24。APK、driver、視窗、codec 與環境介入均保留在比較報告與各 manifest；不外推成其他裝置保證。

| 指標 | gRPC fast | WGC |
| --- | --- | --- |
| r4 正常完整窗口 | 8 批，各 60 秒，目標 40／48／57 Hz | 8 批，各 60 秒，同目標組 |
| 正常各批來源跟隨 | 99.75–99.99% | 66.70–96.90% |
| 正常各批主機駐留 p99 | 2.20–4.24 ms | 13.75–19.67 ms |
| 120 秒短期穩定性消費樣本 | 5,323 | 4,106 |
| 120 秒可見來源／不同圖率 | 44.60／44.32 Hz | 47.87／34.21 Hz |
| 120 秒來源跟隨 | **99.36%** | **71.47%** |
| 120 秒主機駐留 p99 | **4.614 ms** | **13.684 ms** |
| 120 秒最大無新圖空窗 | 94.869 ms | 66.546 ms |

正常批次表中的 p99 是逐批區間，不是 pooled p99。上述主機駐留從各後端 `capture_complete` 到消費，時間點語義有差異，**不是 Android 產生畫面到觸控的端到端延遲**。主用決定綜合來源跟隨、像素、恢復與使用條件，不能單憑這個數字宣稱 gRPC 的絕對來源畫面較新。

兩個候選的 pause500／recover100 都完成短窗口並保留恢復像素證據。刻意暫停期間出現約 523／533 ms 空窗，以及慢消費期間跟隨下降，屬該壓力情境的預期部分，不套正常場景告警線。恢復 counter 匹配指標包含 ADB reference 等待；consumer 恢復缺少實際解除 journal 時刻，不能把排程 threshold 推導值當精確解除延遲。

WGC 的較低跟隨率尚未定位原因，沒有證據把全部 frame-sequence skip 歸為下游積壓；duplicate callback 也包含未發布的序號。這個缺口足以暫緩正式備用資格，無需再花時間跑完整長測才作決定。

## 採用設定與預警決策

現行 gRPC 基線為：payload、RGB888、top-down、1280×720、此 AVD 的 `source_rotation=1`、`optimized_rgb_copy=true`（bench CLI：`--grpc-copy-mode fast-memcpy`）、`grpc_read_chunk_kib=256`。bench 採 `max_relative_lag_ms=250`，Session 接線狀態見下節。裝置序號須即時發現／指定；尺寸與方向變動需新 profile／epoch，不能默認沿用。

採用比較報告提出的 **正常、無額外負載診斷預警線**供後續 M3 開發：主機駐留 p99 >10 ms、最大無新圖空窗 >150 ms、可見 Fixture 來源跟隨 <98% 即記異常。這些是根據已觀察基線作出的工程決策，並非事前註冊的統計性能驗收或遊戲判定窗；不能據此回填所有 raw `performance_pass=true`。

來源跟隨只在有可見 counter 的 Fixture 可量，不能假設遊戲也有這個真值。預警以完整測量窗口統計，不能當逐 frame 的安全 freshness 門控。250 ms 相對 lag guard 是已測的相對積壓保護，不是允許觸控使用 250 ms 舊圖的承諾；起始來源年齡仍 unknown。真正觸控 evidence deadline、prediction horizon 與失效撤銷須由 M3 時序驗證決定。

## 進入 M3 前必須完成的接線

**現有 observe／Session 尚未等同選定的 bench 保護設定。** `apps/pas/main.cpp` 的 `run_observe()` 已傳入尺寸、source rotation 與 `grpc_read_chunk_kib`，但沒有傳入相對 lag 值；`configs/avd-observe.json` 也沒有 lag 字段。預設 fast copy 與 256 KiB 已啟用，`max_relative_lag_ns` 仍為空值。不能直接把目前 `run` 命令宣稱為完整採用已驗證保護。

以下是 2026-09-26 當時的交接項，現已由遊戲 runtime 的 profile／相對 lag／撤銷接線承接；目前狀態以 ARCHITECTURE.md 為準，不是待重開的擷取任務：

1. 將相對 lag 配置、metadata／錯誤語義與 manifest 記錄接入正式 profile／Session，加入配置傳遞與舊圖拒絕的針對性回歸；不能只在文件填 250 而 runtime 不使用。
2. 在 detector／tracker／scheduler 間確實傳递 epoch、generation、幾何與證據期限。超時／相對積壓／斷線／方向改變時撤銷依賴該證據的觸控。
3. 核對 Android 畫面到觸控座標，先對簡單目標量測像素→預測→排程→注入→畫面回饋；只依即時 pixels 決策。
4. 本輪 **不啟用自動切換 WGC**。若將來要升格備用，先定位跟隨損失、完成同版關鍵失效短驗證與 Session／座標接線，再决定。無主用時先明確失效，不以未驗證切換維持操作。

本版長期穩定性、完整 CPU／GPU／memory 負載、所有視窗／driver fault 及遊戲閉環仍未驗證。依使用者縮短要求，不再把它們當本次研究結案的自動續跑工作；其限制在後續對應範圍內處理。
