# 較低背景負載的擷取重測（2026-09-25）

結論：原生 Capture Fixture 的繪製與交付率明顯高於同日「日常負載」基線，但 **process／thread payload 的正常批次均未達原研究數值門檻，來源條件亦未全部接近 60 Hz**。主程式維持 process＋gRPC payload 開發基線；不能據此宣稱 Phigros 自動遊玩可用。來源畫面產生到主機接收的絕對年齡仍未知。

## 來源、環境與預先固定的規則

- 在獨立 worktree `C:/Users/wurre/.codex/worktrees/8bd4/Phigros-Auto-play-System` 測試；原桌面工作樹未改。兩邊 HEAD 均為 `b5da5c834d0b5b2343e83ef394697a8b235bd1be`。測前從桌面工作樹複製 30 個尚未提交的程式、設定、Fixture 與文件檔；來源和目的 SHA-256 逐檔相同，快照清單為 `measurements/capture_idle_retest_20260925T052732Z/snapshot.json`。主程式的 M1 排程問題未在本輪改動。
- Windows 11 10.0.26200、Python 3.14.7、Balanced 電源；唯一已連線 ADB 裝置由 `adb devices -l` 查得 `emulator-5554`，Android 16/API 36、`sdk_gphone64_x86_64`。Capture Fixture 已安裝，前景 1280×720、rotation=1、RGB888/top-down、scale=1；診斷 PNG 的不對稱角落與可見計數正向。實際安裝 APK SHA-256 `2f50b989c6ed48bfb804d3f5a9c6a289db49b6325a813608a373fd7358f25e23`，與前次基線的安裝檔相同；本 worktree 無舊 build APK，不臆稱 build 二進位驗證。
- 切換 Fixture 前，10 秒全機 busy 為 19.50%；九個正常正式窗口各自 1 Hz 全機 busy p50 為 20.12–37.22%，前次日常負載為 55.91–65.56%。GetSystemTimes 包含 AVD 與測試程序，不能拆出背景應用成本；關閉大部分程式不等於完全閒置。
- `campaign/manifest.json` 在第一批前保存順序、門檻、設定、APK 與受測程式 SHA-256。正常九批按 T/P/M、P/M/T、M/T/P 串行，各就緒後暖機 10 秒、正式 60 秒；其餘十批暖機 10 秒、正式 30 秒。T=thread payload，P=process payload，M=process MMAP 診斷。正式窗口內不並行其他測試，不重跑挑選結果。九個正常來源跨度率 max/min ≤1.05 才視為近似可比；57–63 Hz 為預定「接近 60 Hz」範圍；原研究數值門檻為不同 counter ≥55/s、到達 p95≤33.4 ms、p99≤50 ms。沒有事後調低。
- 同一主機 `time.monotonic_ns()` 定義 `[MEASURING, STOPPING)`。擷取按 `capture_complete_ns`，消費按 `consume_ns` 各自過濾；主機駐留是兩者差。process JSONL 的 `capture` 只含 child 交付到 parent 的影格，不能代表 child 收到的全部 frame；`ipc_published` 等 child 計數是另一次資源快照窗口估計。CPU：thread 是正式窗口 process time；process 父／子為各自取樣區間中點估計，含偏差界限，不能直接當同一精確窗口 CPU 排名。來源 Unix metadata 不映射到主機 monotonic，也不用來算絕對影格年齡。

## 正常九批

全部 19/19 批退出碼 0、尺寸／方向／計數解碼成功，總計 43,548 張交付影格；正常九批 28,989 張。完整每批的 n、p50/p95/p99/max、主機負載、駐留、掉幀、故障及 SHA-256 見 `measurements/capture_idle_retest_20260925T052732Z/campaign/tables.md`、`analysis.json` 和各批原始 JSONL／PNG。受測程式 SHA-256 在矩陣前後一致，所有 process child 正常回收，沒有擷取錯誤。

| 批次 | 來源跨度 Hz | 不同 counter/s | 到達間隔 n；p50 / p95 / p99 / max ms | 主機駐留 p50 / p95 / p99 / max ms | 全機 busy p50 % |
| --- | ---: | ---: | --- | --- | ---: |
| 1 T | 44.66 | 44.38 | 2675；20.49 / 43.59 / 61.37 / 109.79 | 3.12 / 5.56 / 9.87 / 17.20 | 37.22 |
| 2 P | 50.74 | 50.34 | 3025；17.07 / 41.58 / 55.49 / 100.12 | 8.34 / 12.53 / 16.00 / 21.30 | 33.01 |
| 3 M | 55.89 | 55.82 | 3349；16.69 / 31.73 / 36.52 / 54.46 | 4.92 / 7.46 / 8.69 / 15.51 | 22.49 |
| 4 P | 55.20 | 55.05 | 3302；16.03 / 37.10 / 47.87 / 69.70 | 7.98 / 13.66 / 18.30 / 26.12 | 28.94 |
| 5 M | 57.45 | 57.36 | 3443；16.60 / 29.44 / 34.79 / 69.69 | 5.02 / 7.64 / 8.23 / 13.65 | 23.72 |
| 6 T | 53.72 | 53.43 | 3210；16.20 / 39.92 / 52.94 / 111.58 | 3.16 / 6.68 / 11.67 / 18.99 | 29.36 |
| 7 M | 56.95 | 56.91 | 3414；16.16 / 31.13 / 35.25 / 65.79 | 4.47 / 7.00 / 7.88 / 17.73 | 20.12 |
| 8 T | 53.91 | 53.61 | 3228；16.01 / 39.20 / 52.96 / 78.43 | 2.80 / 5.08 / 8.56 / 17.03 | 27.98 |
| 9 P | 55.89 | 55.46 | 3334；15.51 / 37.98 / 50.79 / 72.65 | 7.67 / 12.68 / 17.55 / 25.91 | 26.91 |

九批來源 max/min=1.2866，故跨批來源不可視為相同。payload 數值門檻 **0/6**，接近 60 Hz **0/6**；MMAP 診斷數值門檻 **3/3**，但接近 60 Hz 僅 **1/3**，而且整張像素快照一致性未證，不能進 Session。前次日常負載九批來源 21.10–30.47 Hz、不同 counter/s 21.06–30.30、到達 p99 75.96–126.45 ms、數值門檻 0/9；同一已安裝 Fixture APK 與較低全機 busy 伴隨本次改善。前後負載與時間均未隨機控制，且本次 thread 停用每張 RSS 查詢，不能將改善量全歸因於關閉背景程式。process 路徑的像素處理程式未因本次 thread 量測修正而改變。

## 壓力、停頓與穩定性

- 50 ms 慢 consumer：T/P 分別交付 1688/1625 張、消費 588/592 張，consumer skip 1098/1055；T 最新緩衝未讀覆蓋 1100，P 的 IPC 覆蓋 24（不同層，不能加總）。駐留 p99 40.91/49.73 ms，沒有持續增長的消費佇列。
- 100 ms consumer 在正式第 15 秒恢復：P/T 分別消費 987/988 張、正式 skip 各 712；首張恢復後主機駐留 24.08/10.80 ms、該次序號跳過 4/6。這是主機駐留，不是來源絕對年齡。
- 無保護 500 ms 停頓：T/P 實際 500.12/500.32 ms；恢復後首張於 3.59/8.40 ms 交付，但相對錨點仍落後 497.13/491.03 ms。啟用 100 ms 相對保護時 P/T 各丟棄 10/3 張，恢復後首張於 67.23/23.03 ms 交付，相對落後 0.05/−1.56 ms。負值只表示比錨點稍新，不是負的絕對延遲；保護增加了空窗，仍不保證來源絕對新鮮度。
- 修正前的 thread 故障注入在 `collection_lock` 內睡眠，consumer 被連帶卡住，曾有約 504 ms 主機駐留。本次在鎖外停頓並記實際 `ended_ns` 後，無保護／保護 thread 批駐留最大僅 18.14/17.67 ms；到達間隔最大仍 507.73/526.22 ms，符合只暫停接收 callback 的預期。離線回歸直接驗證 consumer 不隨鎖停住，並檢查實際停頓時間。
- 受控 parent GIL 忙碌：T/P 的可見來源跨度仍 55.28/52.88 Hz，交付卻只剩 17.33/30.25 張/s；來源序號跳號 1138/669，P 的 IPC 覆蓋 13。這支持 Python 接收與排程競爭會影響交付，process 有隔離作用，但兩批來源率仍不同，不能據收到率給後端作普遍排名。
- 另做 process payload **10 秒暖機＋180 秒正式窗口**：9847 張交付、不同 counter 54.65/s、可見來源跨度 55.97 Hz；到達間隔 n=9846，p50/p95/p99/max 15.97/39.04/52.02/77.19 ms；駐留 n=9846，6.02/9.42/12.07/25.72 ms。全機 busy n=178，p50/p95/p99/max 25.82/36.96/40.69/44.23%。擷取錯誤 0、child 正常回收，數值門檻仍未達。

## 成本定位與功能 smoke

固定順序的 2 秒暖機＋15 秒分段診斷，在 Fixture 約 54.83–56.99 Hz 的條件下，payload／protobuf／RGB／Fixture 模式各交付 55.72／56.49／55.40／首尾 56.80、54.86 張/s；單核心 CPU 約 56.4／67.4／76.8／首尾 68.7、80.5%，後接收工作 p99 0.015／1.98／5.69／首尾 3.17、6.80 ms。這顯示 gRPC 原始接收本身已耗費相當 CPU，解析與轉換另有成本；各模式來源率和時序不同，不能把 CPU 數字逐項相減為精確成本。

再以受控 Fixture 的 `dumpsys gfxinfo` 繪製計數作一次擷取關／開／關對照：20 秒區段分別 56.45／55.14／56.29 Hz；「開」段完整落在另一批 process payload 正式窗口內，該批可見計數跨度 55.39 Hz。來源 Fixture 即使不擷取亦未達 57 Hz；本輪的近 60 Hz 前提不足，瓶頸至少包含上游繪製／AVD／主機條件，不能只歸咎於 parent IPC。這只有一組 A/B/A，約 1 Hz 的差別未驗證為穩定擷取效應。前次 21–30 Hz 條件沒有同步的獨立 Fixture 繪製真值，無法更精確回溯其瓶頸。

`python -m unittest discover -s tests -v`：**75/75 通過**，含 thread 停頓鎖與恢復 timestamp 回歸。實機 observe 10 秒消費 516 張、1280×720/rotation 1、無 journal fault、`input_created=false`、child 正常回收；Fixture Session 5 秒 `NAVIGATING`、`frame_fresh=true`、未啟用遊玩觸控。Session 對 MMAP 明確以 exit 2 拒絕。可逆關閉／開啟 AVD 螢幕後有新影格，主動關閉客戶端 gRPC 得 `CANCELLED`，新認證來源可重取圖。這些操作沒有產生 `0×0` inactive，也未測整台 AVD 失聯；不把未重現項算通過。沒有操作 Phigros、帳號或登入。

## 可重現與下一步

在倉庫根目錄、Capture Fixture 前景執行（序號必須先以 `python -m pas.cli probe` 查明）：

```powershell
$env:PYTHONPATH='src'
python -m unittest discover -s tests -v
python scripts/unified_capture_bench.py --serial emulator-5554 --include-diagnostic-mmap --output measurements/<全新目錄>
python scripts/summarize_unified_capture.py measurements/<全新目錄>
```

本次所有原始資料位於 `C:/Users/wurre/.codex/worktrees/8bd4/Phigros-Auto-play-System/measurements/capture_idle_retest_20260925T052732Z/`，被 Git 忽略：`snapshot.json`、`preflight.json`、矩陣 manifest／每批 invocation、stdout、stderr、JSONL、PNG、完整重算與雜湊、分段診斷、180 秒穩定性、繪製率 A/B/A、observe／Session／故障 smoke 均在此。`artifact_hashes.json` 對 164 個檔案保存 SHA-256 與大小，`final_audit.json` 保存批次、程式與原目錄未變的稽核結果。移除 worktree 前須另行保存。前次日常負載原始資料只讀保留於桌面工作樹的 `measurements/unified_capture_20260925/`，未複製或覆寫。

下一輪先在相同 Fixture 和真正接近 60 Hz 的穩定來源下做配對成本改善，優先量測 Emulator/gRPC 讀回與 Python RGB 正規化／拷貝的尾端與 CPU；另以獨立 Fixture 繪製計數驗證來源是否維持 57–63 Hz。若優化 payload 後仍未過門檻，才按像素一致性和時序證據評估下一個正式擷取候選。MMAP 必須先證明整張像素一致性；遊戲來源絕對年齡、真實 0×0 inactive、完整 AVD 斷線與畫面到觸控閉環仍待驗證。原 M1 排程與 M2 細分軌跡證據問題見 [獨立驗收](ACCEPTANCE_20260925.md)，本輪未處理。
