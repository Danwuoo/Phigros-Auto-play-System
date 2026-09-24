# 2026-09-23 基線量測與限制

## 2026-09-24 獨立程序／MMAP 冷開發離線結果

本輪**未連線、操控或啟動 emulator**。環境：同一 Windows 11 build 26200 主機、Python 3.14.7、1280×720 RGB888、每 16.67 ms 嘗試產生一張的本機 loopback 假 gRPC server；每配置就緒後暖機 0.5 s、正式窗口 2 s，依序跑同程序 payload、spawn 程序 payload、spawn 程序 MMAP 診斷。來源 server 也在父程序內，因此「parent load」同時影響假來源，**不能由此推斷真實 Emulator 隔離改善**。下表是原始 JSONL 以 `scripts/recompute_offline_capture.py` 對 `[measurement_start, measurement_end)` 重算的擷取事件數和相鄰 `capture_complete_ns` 間隔；不是來源畫面年齡，也不是實機效能。

payload 的 `capture_complete_ns` 是完整 gRPC bytes 到達，MMAP 的是映射區一次複製完成；兩者起點不同，表中間隔只顯示各模式自身交付節奏，不能當作相同事件的 transport 延遲差或 MMAP 優勢。

| 條件 | thread payload n / p95 ms | process payload n / p95 ms | process MMAP 診斷 n / p95 ms |
| --- | ---: | ---: | ---: |
| 正常 | 80 / 44.12 | 69 / 55.46 | 95 / 23.50 |
| consumer 50 ms | 80 / 46.37 | 82 / 37.21 | 101 / 21.91 |
| consumer 100 ms | 81 / 42.23 | 84 / 40.79 | 102 / 21.94 |
| parent Python GIL 負載 | 27 / 121.40 | 25 / 121.45 | 26 / 120.22 |
| child Python GIL 負載 | 81 / 50.23 | 32 / 79.21 | 29 / 112.49 |

逐批 p50／p95／p99／最大值、讀取數、consumer skip、IPC 覆蓋、父子 CPU 與 RSS、時鐘快照偏差與同步 JSONL 寫入成本在各自 `summary.json` 和 JSONL；百分位數為線性插值，日誌成本抽樣有 100,000 筆上限，可用 `--no-log-cost` 作停用對照。慢 consumer 100 ms 下，process payload 收到 84 張但只讀 20 張；process MMAP 收到 102 張、只讀 20 張。這是固定容量 latest 交接的預期現象。MMAP 數值不表示安全、零複製或真實 Emulator 加速；loopback server 的寫入／通知行為不能證明安裝版 Emulator 的上游同步。原生 helper 未加入：目前證據顯示要先做實機 profiling 與來源一致性確認，不能只憑假 server 將 Python 接收器改寫成原生。

原始資料只在本 worktree 的被 Git 忽略 `measurements/offline_final3_{normal,slow50,slow100,parentload,childload}/`，不隨提交或 merge 帶走。每個目錄有 `thread-payload.jsonl`、`process-payload.jsonl`、`process-mmap.jsonl` 及 `summary.json`。三配置各自 SHA-256（順序 thread／process payload／process MMAP）：

| 條件 | SHA-256 |
| --- | --- |
| normal | `EE5797F90A5F5B26FBFF1F75AF386C230C38A3BDAAA9245183BD51F4D145C89A` / `DD1F6C3815B5C514A90CE76D0FD212B3CBBD0BA347A5DE54B8A27D686252004D` / `5693B91484160FEB83D14471C4FDCD747103CD636EA1ED4C759EAFAAA8EFB7B1` |
| slow50 | `B53DE4120D6D3F65714435F5D1C541052355B7F1A02636C6DCECD23FCF1ECBBF` / `4FE5FFE6F1C1A0A047199D9F09046D7A9B277B9EA005D87FAC7A1B4DABF245FF` / `878AC3FFDB230AA020715B172737E5E1CBC2E80821F7E0E2DA8EC65423256192` |
| slow100 | `E8565533623C8170975366E52C02284D6A2F75DA637D9AEBF1FFE183B0B87DFB` / `D0D13566DE33A43C491DB564C5B1833E2D4DD784CE328A33F5943CE7C5D37A77` / `E14911BD0271ED42F2799F899BD0FB1C10F74AE88935AE4F936668A1D01EE6EC` |
| parentload | `FE36442DB0BB1A3BC17B7827A717D1A67038708AD7AF5629E7729BD7BFDB9932` / `CA8CD7350F3F7B0A79880DCDEA5C7EE73305777E6CAD1792DA8A78834512D727` / `76F413BD96944C3190AA835D8869461A739701B11C01B890F384B877C96ECE44` |
| childload | `968A82EBC0404F4705F1AF9123150ECB52622900F434B9A6C02A195246532FDE` / `C90997E3B221344038DDCC939067CFAC430FDA14FEB010B3BD38133D4447514A` / `3F2760DA051438C1AA1787097495C8DE0BD4F5B76A6114983E3C03877B32DB95` |

重算例：`$env:PYTHONPATH='src'; python scripts/recompute_offline_capture.py measurements/offline_final3_normal/process-payload.jsonl`。此前失敗批次 `offline_final_*` 保留在 ignored 目錄，失敗原因是 Windows 子程序 RSS 讀值的 ctypes 指標型別錯誤；已修正並重跑上表，沒有將失敗批次混入結果。

## 環境與重現

- 主機：Windows 11 build 26200、AMD64、Python 3.14.7；時間來源為同一程序的 `time.monotonic_ns()`。CPU 型號、主機背景負載與使用率未控制或記錄，故主機排程數值僅是這台機器的初步基線。
- 此行原為 AVD 尚未建立時的歷史盤點；其後使用者已建立並啟動 `phigros` AVD。2026-09-23 本次重新查驗：`emulator-5554` 已連線，Emulator 37.1.11.0，遊戲選單畫面 1280×720。觸控能力與遊戲判定效果仍未驗證。
- 指令見 [README](../README.md#執行與重現)。暖機：主機排程每批先執行 10 次，不納入 100 次統計；合成與緩衝區測試沒有暖機。第二輪原始 JSONL 位於此工作樹的 `measurements/`（不納入 Git）；初版歷史原始資料位於下文列出的舊工作樹。gRPC 基準每次使用獨立檔名且拒絕覆寫。統計採線性插值百分位數；此處排程 jitter 為預定觸控與實際注入開始之誤差的 p95−p5，並另記 p95 絕對中位偏差。擷取間隔 p95−p5 不等於觸控排程 jitter。呼叫返回不代表畫面已反映觸控。

## 容量 1 的最新 frame 緩衝區

`buffer-bench` 以主機 clock 執行 1 秒，合成 producer 預定每 2 ms 發布 1×1 RGB24 frame，consumer 每讀 1 張後延遲 20 ms。結果：發布 357 張、讀到 48 張、未消費即被覆蓋 308 張；consumer 序號總共跳過 301。兩個計數因量測起訖與讀取邊界不同，不應相加。已讀 frame 的 age（讀取時間減擷取完成時間，n=48）為 p50 1.180、p95 2.899、p99 3.218、最大 3.232 ms；0 次錯誤。這驗證了下游落後時讀到最新 frame，不建立持續成長的 frame 佇列。原始檔：`measurements/buffer_slow_consumer.jsonl`。

## 合成 pixels → touch → pixels 閉環

合成畫面 64×64 RGB24、目標 1 秒出現一次、500 ms 後到達 y=48 判定線、60 fps、30 個目標；真值只在測試 world 與離線摘要使用。目標到達時間與偵測座標的量化誤差可重現；時間由虛擬 monotonic clock 推進，假觸控無 OS／模擬器注入延遲。

| 額外辨識耗時 | 處理 frame | 跳過擷取時限 | 命中／目標 | 預測誤差 p50 / p95 / p99 / 最大 (ms) | 預定到注入誤差 | 觸控到效果被看見 |
| --- | ---: | ---: | ---: | --- | --- | --- |
| 0 ms | 1,800 | 0 | 30/30 | −4.386 / −4.385 / −4.385 / −4.385 | 0 ms（虛擬 clock） | p50/p95/p99/max 4.386 ms |
| 5 ms | 1,800 | 0 | 30/30 | −4.386 / −4.385 / −4.385 / −4.385 | 0 ms（虛擬 clock） | p50/p95/p99/max 4.386 ms |
| 25 ms | 900 | 900 | 0/30 | 無可用注入樣本 | 0 次觸控 | 無效果 |

前兩批各 30 次觸控、30 次效果觀察、0 次誤點／漏點；擷取間隔 n=1,799，p50/p95/p99/max 均 16.667 ms。預測誤差為最後送出的計畫時間減測試真值；前兩批 p95−p5 約 0.0005 ms，幾乎完全由固定影格週期的小數捨入造成。辨識耗時 0／5 ms 是注入的虛擬延遲，不是 Python 影像處理的實際耗時。25 ms 的同步處理超過每影格 16.667 ms，跳過一半擷取時限；預測到期後排程器拒絕過晚動作，故 30 次全漏，沒有盲目補按。這暴露了目前同步 runner 的負載瓶頸；真實擷取及排程須獨立執行並重新量測。原始檔：`measurements/synthetic_60fps.jsonl`、`measurements/synthetic_delay5ms.jsonl`、`measurements/synthetic_delay25ms.jsonl`。

## 主機 monotonic 排程與假觸控

每批 100 個預定 down 事件，相隔 10 ms，前 10 次暖機排除。`--load` 在另一個 Python thread 持續執行 CPU 運算，只是受控擾動，不等於可比的 CPU 使用率。下表單位 ms；誤差 = `injection_start_ns − scheduled_ns`。每批 0 次假後端失敗。

| 條件／批次 | n | p50 | p95 | p99 | 最大 | jitter p95−p5 | 原始 JSONL |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| idle 1 | 100 | 0.688 | 4.352 | 24.814 | 34.652 | 4.100 | `measurements/scheduler_idle.jsonl` |
| idle 2 | 100 | 0.620 | 2.917 | 5.494 | 12.014 | 2.698 | `measurements/scheduler_idle2.jsonl` |
| load 1 | 100 | 7.716 | 14.884 | 15.554 | 16.140 | 13.695 | `measurements/scheduler_load.jsonl` |
| load 2 | 100 | 7.808 | 14.439 | 15.368 | 15.536 | 13.754 | `measurements/scheduler_load2.jsonl` |

假後端注入呼叫耗時：idle 1 的 n=100、p50 0.006、p95 0.009、p99 0.012、最大 0.117 ms；load 1 的 p50 0.002、p95 0.005、p99 0.009、最大 0.009 ms。排程尾端遠大於假注入呼叫，且 idle 批次間差異大；目前無法推斷遊戲觸控時效或判定窗是否可達標。需在 AVD 準備後量測擷取、真正觸控、畫面效果及整體誤差，再比較候選後端。

## AVD 驗收進度與後續需求

1. 使用者已建立並啟動 AVD，自行安裝遊戲並進入選曲畫面；`probe` 與首批 `capture-bench` 已完成，結果如下。CPU 使用與掉幀尚未量測；沒有可靠畫面產生時間時，不宣稱知道真實 frame age。
2. 建立可見回饋的簡單 Android 測試畫面及可維持獨立接觸點的觸控候選。逐項測 Tap、Hold、Move、Flick、同時按下、交錯放開與取消，量測座標誤差及觸控效果延遲。`adb shell input tap` 不足以完成此驗收。
3. 在同一實機配置重跑完整簡單目標閉環和正常／受控負載多批次量測。只有完成第 1–4 階段的實機驗收，才開始 Phigros 介面交接與專用辨識。

## 2026-09-23 `phigros` AVD 首次擷取量測

- AVD 設定：Small Phone、Android 16／API 36.1、Google Play x86_64 標準 4 KB 映像；4 vCPU、6 GB RAM、20 GB 內部儲存、host GPU、60 Hz 顯示。使用者已自行安裝遊戲並進入選曲畫面。ADB 序號在本次量測為 `emulator-5554`；`getconf PAGE_SIZE=4096`。實際擷取像素尺寸為橫向 1280×720。AVD 設定由使用者提供，連線、頁面大小、遊戲安裝與擷取尺寸由 ADB 驗證。
- 測試命令：`$env:PYTHONPATH='src'; python -m pas.cli capture-bench --serial emulator-5554 --samples 30 --warmup 3 --log measurements/adb_capture_phigros_20260923.jsonl`。主機為 Windows 11 build 26200、AMD64、Python 3.14.7；未控制背景負載。計時使用同一主機 `time.monotonic_ns()`。3 張暖機影格不納入統計，正式樣本 30 張，全部成功。
- `adb exec-out screencap -p` 呼叫耗時，n=30：p50 681、p95 901、p99 951、最大 960 ms。純 Python PNG 解碼耗時，n=30：p50 2,499、p95 2,799、p99 2,902、最大 2,909 ms。相鄰 PNG bytes 收齊間隔，n=29：p50 3,156、p95 3,556、p99 3,688、最大 3,733 ms；p95−p5 為 808 ms。這些是歷史摘要；原始 `measurements/adb_capture_phigros_20260923.jsonl` 目前缺失，不能重算，也不能稱為像素就緒間隔。
- 另以 5 次未壓縮 `adb exec-out screencap` 呼叫做初步診斷，呼叫耗時 665、1,017、596、720、588 ms。這不是正式候選後端基準，未解碼或驗證像素；它僅顯示避開 PNG 解碼仍不足以接近 60 Hz 影格週期。`screencap` 不提供可靠的畫面產生時間，因此無法量測真實 source frame age。
- 結論：目前 ADB PNG + Python 解碼路徑不能支撐遊戲即時接手。下一步須實作並比較更快的持續擷取路徑，記錄像素正確性、擷取間隔、frame age 可觀測性、CPU 負載和 p50／p95／p99／最大值；之後才進入觸控後端與簡單目標閉環驗收。此結果不代表遊戲判定窗或未來後端的能力。

## 2026-09-23 Emulator gRPC 串流實測

**歷史數據更正（2026-09-24）：** 本節至「原生橫向像素 fixture」的舊 gRPC CPU 欄位把暖機與停止成本納入 process time，卻以宣告的正式 duration 為分母，因此不代表正式窗口 CPU 使用率；沒有保存兩端 CPU 快照，不能從舊 JSONL 補算可靠修正值。舊批次的來源跳號、inactive、相對舊圖丟棄與覆蓋也是生命週期累積值，不應解讀為正式窗口計數。部分負載批次正式影格跨度明顯短於宣告時長；例如 `grpc_fixture_load` 的正式樣本只跨 11.5515509 秒，且第一張就是正式樣本。下列舊 FPS／到達分布只描述實際已收樣本；對長時間穩態與 CPU／負載的結論應以第二輪重測取代。舊原始日誌在 `C:/Users/wurre/.codex/worktrees/f1a5/Phigros-Auto-play-System/measurements/`，此工作樹僅唯讀引用，原始 SHA-256 不更動。

此節是本次**新量測**，與上面的歷史 ADB 摘要分開。主機 Windows 11 build 26200，Intel Core Ultra 5 125H（18 邏輯處理器）、Intel Arc／NVIDIA RTX 3050 Laptop GPU；Python 3.14.7，grpcio 1.84.0、protobuf 7.36.2、Pillow 12.3.0。ADB 37.0.1、Emulator 37.1.11.0 build 15917651。`phigros` AVD：Android 16／SDK 36、x86_64、4 vCPU、6144 MB、host GPU、物理顯示 720×1280／320 dpi、目前遊戲選單橫向輸出 1280×720，顯示模式 60 Hz。裝置序號本次為 `emulator-5554`，gRPC 在 127.0.0.1:8554，以本機 discovery 權杖認證。遊戲版本與背景 GPU 使用率未可靠取得；主機其他負載未固定。安裝版 `proto/emulator_controller.proto` SHA-256 為 `1D62C6BCAD5F06621F90EC2BF26C661BA769CCD0F1416B5314D25A68E04EEE5F`。

使用一般 gRPC transport 的 RGB888，請求原尺寸。舊命令宣告暖機 10 秒、正式收集 60 秒，畫面為已有的遊戲選單，含部分動畫但沒有可驗證的每幀真值；60 Hz 是顯示設定，**不是已確認的來源更新率**。到達間隔按相鄰 `capture_complete_ns` 計，像素就緒間隔另按 `pixels_ready_ns` 計；來源 Unix 微秒原值不與主機 monotonic 直接相減。百分位數採線性插值。下表舊 CPU 欄位是混合窗口的 process time 除宣告的正式秒數，**不可作正式窗口 CPU 結論**；RSS 是本程序 Working Set。原始 JSONL 與診斷 PNG 被 Git 忽略，存放位置見上段更正。

| 條件 | 收到影格／60 s | 到達間隔 p50 / p95 / p99 / 最大 (ms) | 像素就緒 p95 (ms) | 來源跳號 | CPU 單核心 | RSS p50 / 最大 (MiB) |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| 正常 1 | 1,296 | 44.11 / 83.34 / 118.24 / 198.54 | 84.72 | 0 | 43.3% | 57.69 / 59.32 |
| 正常 2 | 1,498 | 39.87 / 70.36 / 85.57 / 182.31 | 70.34 | 0 | 47.1% | 57.80 / 59.63 |
| 正常 3 | 1,575 | 37.47 / 65.97 / 82.64 / 117.79 | 66.03 | 0 | 47.8% | 57.31 / 59.46 |
| consumer 延遲 50 ms | 1,312 | 44.91 / 79.82 / 96.07 / 176.62 | 79.81 | 0 | 42.2% | 58.00 / 61.44 |
| consumer 延遲 100 ms | 1,514 | 38.91 / 70.23 / 86.75 / 122.38 | 70.29 | 0 | 43.2% | 57.43 / 61.64 |
| 額外 Python CPU 忙碌 thread | 730 | 62.43 / 94.03 / 109.59 / 124.80 | 94.50 | 458 | 130.9% | 62.36 / 66.81 |

每批到達間隔的樣本數是收到影格減一。正常三批的 RGB888 轉換 p95 分別為 2.15、1.61、1.44 ms；這不是串流等待時間。50 ms 慢 consumer 讀到 1,134 張、覆蓋 178 張未讀圖、讀取序號跳 177；100 ms 條件讀到 596 張、覆蓋 919 張、跳 917。兩類數字邊界不同，不相加。另做 30 秒測試，前 15 秒延遲 consumer 100 ms、後 15 秒恢復，收到 717 張、讀到 504 張、覆蓋 214 張；恢復後首張讀取的主機駐留時間 77.46 ms。這驗證應用的容量 1 交接；沒有來源畫面真值時不能把 77.46 ms 稱為來源影格年齡。

`--load` 是同程序另一 Python thread 持續做整數運算，會競爭 GIL，並非標準化主機負載。該批收到影格少於正常條件且來源跳號 458，顯示此接收實作對同程序 CPU／GIL 競爭敏感；不能由此推定 AVD 實際產生 FPS。

接收端獨立停頓測試：暖機 5 秒、正式 30 秒，在 `on_frame` 暫停 500 ms，收到 768 張、來源序號跳 11。恢復後先收到 3 張相對較舊的圖；第 4 張在暫停結束約 36.58 ms 後收到，來源時戳的**相鄰差值**與主機到達時間差值重新接近暫停前基線。見 `python scripts/analyze_capture_pause.py measurements/grpc_receiver_pause/capture.jsonl`；這沒有做 Unix 與 monotonic 絕對值相減，也未證明所有動態負載下均不積壓。該批到達間隔 p50 / p95 / p99 / 最大為 38.21 / 65.85 / 82.00 / 509.78 ms。短測顯示一般 gRPC 在這個場景下會暫時交付舊圖，之後以來源跳號追上；若未來要用於決策，仍需有可信新鮮度策略。

另以相同停頓設定啟用可選 `--max-relative-lag-ms 100`，正式 30 秒收到並發布 782 張，分開記錄來源跳號 16 與相對落後丟棄 3 張；暫停結束後首張發布於 33.45 ms。此保護只用兩個時域各自的**差值**辨識額外積壓，不能補償首次接收前已有的來源延遲，也不等於絕對新鮮度。這是單次遊戲選單場景結果，尚未用每幀可見計數驗證。

像素與故障：在同一選單場景將 gRPC RGB888 與 ADB PNG 的 25 個分散採樣像素對照，修正行序後樣本完全相同。安裝版 proto 註解說像素 bottom-up，但本機 RGB888／RGBA8888 實測為 top-down；兩格式的診斷圖文字與角落方向正確。RGBA8888 轉 RGB24 的優化後 3 秒短測收到 82 張，轉換 p50 / p95 = 7.03 / 15.82 ms，故優先 RGB888。錯誤權杖得到 `UNAUTHENTICATED`，日誌沒有權杖；真實阻塞串流停止呼叫約 1.92 ms。尺寸變更、inactive 與來源序號重置有假串流測試，尚未在這台 AVD 上切換方向或尺寸。

可重現命令：`$env:PYTHONPATH='src'; python -m pas.cli capture-bench --serial emulator-5554 --capture-backend emulator-grpc --duration-s 60 --warmup-s 10 --log measurements/<新資料夾>/capture.jsonl`。慢 consumer 加 `--consumer-delay-ms 50` 或 `100`；接收端停頓用 `--duration-s 30 --warmup-s 5 --receiver-pause-ms 500`。`--log` 指向已存在檔案會拒絕覆寫。原始 JSONL SHA-256：

| 本工作樹相對路徑 | SHA-256 |
| --- | --- |
| `measurements/grpc_normal_1/capture.jsonl` | `137BAC46A8BBBA59F17698924B602B92F49CC2D2291DFCC4921EBE1BC93ACFCF` |
| `measurements/grpc_normal_2/capture.jsonl` | `887D2AEEE1E1EB1C263BC7BC03AB9ACB3B4DC4DD424DB7E7C0C5F0CEACCAA7F5` |
| `measurements/grpc_normal_3/capture.jsonl` | `27D0E5A06675787BA423A9EF8B97F45B42881C3D90EEE17C1393B6F09EE53A86` |
| `measurements/grpc_consumer_50/capture.jsonl` | `8963C1C983B21D2C3F2B46D503F525B962B0FBAEFB5B3970012C75D08B4CCB6F` |
| `measurements/grpc_consumer_100/capture.jsonl` | `728F6C15DCA4B57E5A83068429F3C8DAD2FA1789BE162588304E984736C8A321` |
| `measurements/grpc_receiver_pause/capture.jsonl` | `734413A5CA70C7124AFE4482C7A4EB9B0CCD1C7B7BE5625DFC1323EE2A793293` |
| `measurements/grpc_consumer_recover/capture.jsonl` | `DB50BAFDA72510B11047D205A1A9373DD095D400C13DBE45597AB06482501097` |
| `measurements/grpc_receiver_guard/capture.jsonl` | `169FDDE7763BDFED18D97FE74982023841B42E6668BC5CD6C75FD53BFCF626E2` |
| `measurements/grpc_rgba_optimized/capture.jsonl` | `A2453FF6DF3CD12FEC28BEBDF4B159D1BD31962B8364D6C3D93B6A5299FD6964` |
| `measurements/grpc_load/capture.jsonl` | `C8565DC2D1FF875CA659B6968C14AC448D145AA41341BCEAFBFAA9B5CD3EC665` |

研究目標中的 1280×720、約 60 FPS 動態來源條件**仍未確認**。遊戲選單場景的觀測接收率僅約 21.6–26.3 張／秒，間隔 p95、p99 高於目標，但不能區分來源只更新較慢、傳輸或主機負載的比例。動態 fixture 位於 `fixtures/capture/index.html`。自動執行審查拒絕 `adb shell am start -a android.intent.action.VIEW ...` 以及啟動本機伺服器，理由均為 `blocked by policy`；使用者其後手動啟動伺服器並在 AVD Chrome 開啟測試頁。

Chrome 轉成直向 720×1280；診斷圖中的兩排位元標記已校準為水平 2.0、垂直 2.25 倍與 y=162 偏移。初次載入時頁面間歇空白，短測曾只有 0–3 張；使用者重新整理後持續繪製。以下新批次均用 RGB888、`--fixture-y 162 --fixture-scale 2 --fixture-scale-y 2.25`，其餘環境與上段相同。正常批次每批暖機 10 秒、正式 60 秒；擾動批次暖機 5 秒、正式 30 秒。可見計數每次 `requestAnimationFrame` 繪製加一，表中 callback 更新率由首末計數差除接收時間跨度估計，不等於 60 Hz 設定。

| Chrome 動態畫面條件 | 收到張數 | 計數更新率估計 (Hz) | 到達間隔 p50 / p95 / p99 / 最大 (ms) | 來源跳號 | CPU 單核心 | RSS p50 / 最大 (MiB) |
| --- | ---: | ---: | --- | ---: | ---: | ---: |
| 正常 1，60 秒 | 1,637 | 27.28 | 34.58 / 68.34 / 90.45 / 142.84 | 0 | 62.9% | 57.93 / 60.18 |
| 正常 2，60 秒 | 1,639 | 27.30 | 33.86 / 69.43 / 87.65 / 166.62 | 1 | 62.5% | 57.87 / 59.85 |
| 正常 3，60 秒 | 1,576 | 26.24 | 36.88 / 68.18 / 88.54 / 161.82 | 0 | 62.0% | 57.84 / 60.09 |
| consumer 延遲 50 ms | 881 | 29.36 | 32.64 / 63.30 / 83.41 / 110.20 | 0 | 64.6% | 54.17 / 61.49 |
| consumer 延遲 100 ms | 877 | 29.28 | 32.62 / 65.70 / 81.26 / 163.97 | 1 | 65.2% | 53.58 / 60.80 |
| 500 ms 接收暫停，無保護 | 761 | 25.54 | 37.82 / 71.90 / 93.33 / 515.75 | 10 | 67.8% | 52.96 / 58.94 |
| 500 ms 接收暫停，相對落後上限 100 ms | 790 | 27.05 | 35.63 / 69.78 / 93.31 / 539.37 | 17 | 67.6% | 52.95 / 59.21 |
| 額外 Python CPU 忙碌 thread | 178 | 35.06 | 62.45 / 94.32 / 109.21 / 136.77 | 229 | 124.4% | 61.28 / 64.58 |

正常 3 批的可見計數差分分別是：1,636 次 `+1`；1,637 次 `+1`、1 次重複；1,569 次 `+1`、4 次重複、2 次 `+2`。因此來源確實持續更新，實際 callback 約 26–27 Hz，沒有足夠證據在這個直向 Chrome 設定宣稱「60 FPS 來源下擷取 55 張／秒」已通過。到達間隔 p95 均超過 33.4 ms，p99 均超過 50 ms；來源更新率低與接收路徑成本的貢獻仍需用橫向原生 fixture 分解。受控 CPU 負載使接收張數和來源序號連續性大幅惡化，與先前遊戲選單測試方向相同，但這只是同程序 GIL 競爭。

慢 consumer 50／100 ms 各讀到 578／296 張，未消費即被容量 1 緩衝區覆蓋 302／580 張；接收仍持續進行。另在 30 秒測試中以前 15 秒 100 ms 延遲、後 15 秒恢復，收到 947 張、覆蓋 307 張，恢復後首張已接收畫面的主機駐留時間 18.15 ms。這是主機接收後的時間，不是 Android 畫面產生後的絕對年齡。

接收端暫停 500 ms 的原始畫面證據更直接：無保護時，暫停前可見計數為 18,026；恢復後於 14.78、20.61、26.31 ms 收到 18,027–18,029 三張較舊畫面，再於 62.49 ms 收到 18,040。來源 Unix 時戳**差值**與主機 monotonic **差值**顯示前三張額外落後約 494／461／428 ms，後一張約 27 ms；來源序號同時由 420 跳至 431。啟用 `--max-relative-lag-ms 100` 的另一批丟棄 3 張相對舊畫面，暫停前可見計數 19,157，恢復後首張 19,178 於 34.57 ms 交付；後續無持續回放。這驗證了短暫上游緩衝會發生，且此條件下保護能擋下；初始來源延遲、時戳準確度與長時負載下的積壓仍未證明。

可重現命令範例：`$env:PYTHONPATH='src'; python -m pas.cli capture-bench --serial emulator-5554 --capture-backend emulator-grpc --duration-s 60 --warmup-s 10 --fixture-y 162 --fixture-scale 2 --fixture-scale-y 2.25 --log measurements/<新資料夾>/capture.jsonl`。其他批次加表列的 `--consumer-delay-ms`、`--receiver-pause-ms`、`--max-relative-lag-ms` 或 `--load`；詳情可讀各 JSONL 的 `run_config`。暫停分析可用 `python scripts/analyze_capture_pause.py measurements/grpc_fixture_receiver_pause/capture.jsonl`。原始 JSONL SHA-256：

| 本工作樹相對路徑 | SHA-256 |
| --- | --- |
| `measurements/grpc_fixture_dynamic_1/capture.jsonl` | `825E66B845BC3C54397E98082A5B54B7FF6F5AE2F9CE390DCBFAA0996C7E378C` |
| `measurements/grpc_fixture_dynamic_2/capture.jsonl` | `77241CB365ECAF19E99D4DD7A6CA0755AB9E793B40D7ACC453C5514905B5A46E` |
| `measurements/grpc_fixture_dynamic_3/capture.jsonl` | `2A30E9172DF618178CE1E9D6B0752DA2A134ED398D6D0D4EDE463C6B1389B61B` |
| `measurements/grpc_fixture_consumer_50/capture.jsonl` | `38F2E1FD096DC3D1175625A8DB353BEE23D258DC8F6D0F93CF72579E6B362168` |
| `measurements/grpc_fixture_consumer_100/capture.jsonl` | `E43113652E82C0D5F93F8904AD57239BC37B6EB4390ECF092157D4DE6E30E983` |
| `measurements/grpc_fixture_consumer_recover/capture.jsonl` | `03751455F0B2774EF319F2559CB4B19F355C927065DCCDCC9719006953FA3517` |
| `measurements/grpc_fixture_receiver_pause/capture.jsonl` | `AD5FF5A24749D8B594A48E865D9C06403E4A06E9F44FD9FE67695CD29E235A67` |
| `measurements/grpc_fixture_receiver_guard/capture.jsonl` | `4E7F63F7FF914D58C4785015BD4A63F98B1C9C7027AF925869209D3B1973EC4F` |
| `measurements/grpc_fixture_load/capture.jsonl` | `4AE048D7A5AFEADEAC9AD7144895A1E391F84B2A2A998AD77DED6A8419235BA8` |

### 原生橫向像素 fixture

為排除 Chrome 直向、工具列與 canvas 排程因素，另以 `python scripts/build_capture_fixture.py` 從 `fixtures/capture_android/` 建立單一原生繪圖 Activity，APK 位於 `measurements/fixture_android/pas-capture-fixture.apk`，SHA-256 `2F50B989C6ED48BFB804D3F5A9C6A289DB49B6325A813608A373FD7358F25E23`。已在同一 AVD 安裝，使用者手動開啟；無網路、遊戲資料讀取或觸控。RGB888 診斷圖確認 1280×720 橫向、非對稱四色角落與兩排位元計數，解碼參數為 `--fixture-scale 1`。以下正常批次各暖機 10 秒、量測 60 秒；其餘各暖機 5 秒、量測 30 秒。環境、時間定義與 CPU／RSS 計法同上。

| 原生 fixture 條件 | 收到張數／不同計數 | 可見繪製率估計 (Hz) | 到達間隔 p50 / p95 / p99 / 最大 (ms) | 來源跳號 | CPU 單核心 | RSS p50 / 最大 (MiB) |
| --- | ---: | ---: | --- | ---: | ---: | ---: |
| 1280×720 正常 1 | 2,027 / 2,027 | 33.79 | 28.97 / 50.69 / 63.90 / 82.61 | 0 | 61.7% | 58.09 / 60.31 |
| 1280×720 正常 2 | 2,077 / 2,077 | 34.64 | 28.40 / 47.86 / 63.55 / 78.07 | 0 | 68.9% | 58.22 / 60.04 |
| 1280×720 正常 3 | 2,123 / 2,121 | 35.42 | 27.96 / 48.93 / 61.69 / 98.32 | 3 | 68.4% | 58.36 / 60.30 |
| 640×360，30 秒 | 1,239 / 1,239 | 41.30 | 24.12 / 39.24 / 43.18 / 52.08 | 0 | 23.8% | 46.32 / 50.04 |
| 1280×720，500 ms 接收暫停 | 1,076 / 1,075 | 36.63 | 27.12 / 47.87 / 56.54 / 505.23 | 23 | 65.2% | 57.36 / 60.56 |
| 1280×720，500 ms 暫停且相對落後上限 100 ms | 1,011 / 1,011 | 34.41 | 28.39 / 52.42 / 64.32 / 548.88 | 18 | 67.6% | 57.09 / 59.06 |

正常第 1 批的 2,026 個相鄰計數差全為 `+1`；第 2 批僅有 1 次 `+2`；第 3 批有 2 次重複、2 次 `+2`、1 次 `+3`。這表示實際收到的來源畫面大多連續，但**原生畫面在擷取期間的繪製率也只約 34–35 Hz**；不能從此判定 gRPC 穩定漏掉 60 FPS 來源。縮小請求解析度後收到率約 41.3 Hz；舊 CPU 數值窗口不一致，只能把這批視為尺寸成本線索，需重測才能定量比較。這是不同尺寸的單批診斷，不是 1280×720 合格結果。1280×720 三批均未達每秒 55 張不同畫面、到達間隔 p95 ≤33.4 ms、p99 ≤50 ms 的研究目標。沒有可靠的來源產生時間與主機時鐘對齊，絕對 source frame age 仍未知。

原生畫面的暫停測試也證實短暫上游積壓。無保護時，暫停前計數 17,062；恢復後於 3.82／8.93／14.65 ms 先收到 17,063–17,065，直到 36.40 ms 才跳到 17,089。前三張的來源與主機時戳**差值之差**約 481／455／445 ms，追上的一張約 1 ms。啟用相對落後上限 100 ms 的另一批丟棄 4 張，暫停前計數 18,615，恢復後首張 18,637 於 47.84 ms 交付。這滿足本次受控暫停後 100 ms 內恢復較新畫面的觀察，但不代表所有場景的絕對當前畫面都可判定。

重現原尺寸正常批次：`$env:PYTHONPATH='src'; python -m pas.cli capture-bench --serial emulator-5554 --capture-backend emulator-grpc --duration-s 60 --warmup-s 10 --fixture-scale 1 --log measurements/<新資料夾>/capture.jsonl`。縮圖批次另加 `--width 640 --height 360 --fixture-scale 0.5`。原始 JSONL SHA-256：

| 本工作樹相對路徑 | SHA-256 |
| --- | --- |
| `measurements/grpc_native_dynamic_1/capture.jsonl` | `5899D68149835611DFE621F976131DECBE6793A82B3ECDD857C83ADCA95E8694` |
| `measurements/grpc_native_dynamic_2/capture.jsonl` | `0744656BD66E0FC234E2FBEDB7796146688EEC4CA6115AE57C9B83149289A2D9` |
| `measurements/grpc_native_dynamic_3/capture.jsonl` | `6C4F4455F220030AC1F55B7EA3DFCC0F157CE9ABD347950821EAEF65D44BAFEE` |
| `measurements/grpc_native_half_30/capture.jsonl` | `D41CD59FCFD946A668F3000E371CE03C451B189D37F85E3F3513E224480B8843` |
| `measurements/grpc_native_receiver_pause/capture.jsonl` | `E9E1EE1C9B96CEAFFDBF131771D68FA803AF890DD08B4DD31C1846B28CD977F4` |
| `measurements/grpc_native_receiver_guard/capture.jsonl` | `9B39A794DFED3A3E1D43BB0AE1D78F01D2D51117BE40D5CDC9D577EC10B2E08B` |

MMAP 傳輸目前沒有證據是必要且有撕裂風險，WGC／scrcpy 也尚未進入同條件比較。此階段僅確認 gRPC 可用與已列出的性質，未選定遊戲用擷取後端。

## 2026-09-24 第二輪窗口修正與 AVD 重測

本輪在 `c38fe00` 後的獨立工作樹修正 R1–R4，執行時工作樹尚未提交。環境延續同一 Windows 11 build 26200、Emulator 37.1.11、Python 3.14.7、grpcio 1.84.0、protobuf 7.36.2、Pillow 12.3.0、`phigros` AVD 與原生 `PAS Capture Fixture`。序號 `emulator-5554`、RGB888 原尺寸 1280×720、`source_rotation=1`、top-down；實體模擬器以 `adb emu rotate` 校正，診斷 PNG 的四色角落與可見計數均正向。除表列差異外，首張有效影格後暖機 10 秒、正式 monotonic 窗口 60 秒；對照批次暖機 5 秒、正式 30 秒。每批 `--ready-timeout-s 15 --fixture-scale 1`，來源 Unix metadata 僅保留診斷，不計算絕對影格年齡。主機其他背景負載未固定；CPU 是正式窗口 process time／實際 monotonic 牆鐘，單核心百分比。RSS 是進程 Working Set。JSONL 事件在半開區間 `[measurement_start_ns, measurement_end_ns)` 重算正式來源跳號、丟棄、發布、覆蓋與消費；兩端快照另存。百分位數採線性插值，本表樣本均少於 100,000 筆上限，因而使用完整正式樣本。初始化、暖機、停止、PNG 輸出均不在正式 CPU 窗口。

| 條件 | 正式秒數 | 收到／不同可見計數 | 可見計數率 Hz | 到達間隔 n；p50 / p95 / p99 / 最大 ms | 來源跳號／相對舊圖丟棄／未讀覆蓋 | CPU 單核心 | RSS p50 / 最大 MiB |
| --- | ---: | ---: | ---: | --- | ---: | ---: | ---: |
| 原生正常 2 | 60.007 | 2,142 / 2,142 | 35.71 | 2,141；26.85 / 51.98 / 65.37 / 96.85 | 0 / 0 / 0 | 58.6% | 58.6 / 60.3 |
| 原生正常 3 | 60.003 | 2,224 / 2,222 | 37.10 | 2,223；26.31 / 47.31 / 61.07 / 83.14 | 0 / 0 / 0 | 57.7% | 58.2 / 61.9 |
| 原生正常 4 | 60.001 | 2,182 / 2,181 | 36.39 | 2,181；27.00 / 47.32 / 60.37 / 75.29 | 2 / 0 / 0 | 55.0% | 58.4 / 60.5 |
| consumer 延遲 50 ms | 30.041 | 883 / 882 | 29.40 | 882；33.43 / 60.51 / 73.29 / 81.26 | 0 / 0 / 298 | 64.2% | 54.2 / 60.9 |
| consumer 延遲 100 ms | 30.097 | 849 / 849 | 28.24 | 848；35.04 / 64.56 / 75.43 / 83.70 | 0 / 0 / 550 | 56.8% | 53.9 / 61.2 |
| 100 ms consumer，15 秒恢復 | 30.005 | 1,076 / 1,076 | 35.87 | 1,075；26.64 / 50.58 / 64.12 / 79.93 | 0 / 0 / 369 | 59.8% | 57.8 / 59.7 |
| 接收暫停 500 ms | 30.008 | 1,119 / 1,116 | 37.74 | 1,118；25.41 / 48.53 / 60.44 / 505.62 | 13 / 0 / 0 | 62.5% | 57.2 / 76.7 |
| 接收暫停，落後上限 100 ms | 30.003 | 1,074 / 1,072 | 36.54 | 1,073；27.17 / 49.04 / 64.04 / 531.97 | 20 / 3 / 0 | 56.6% | 57.2 / 59.2 |
| 同程序 Python CPU 忙碌 thread | 30.027 | 461 / 454 | 37.97 | 460；62.47 / 94.23 / 109.29 / 140.92 | 678 / 0 / 5 | 113.3% | 61.6 / 65.8 |
| 640×360 尺寸診斷 | 30.007 | 1,271 / 1,271 | 42.38 | 1,270；23.66 / 39.23 / 45.58 / 53.45 | 0 / 0 / 0 | 15.9% | 47.5 / 49.8 |
| 1280×720 RGBA8888 診斷 | 10.009 | 360 / 359 | 36.08 | 359；26.94 / 45.76 / 54.13 / 61.62 | 1 / 0 / 0 | 98.0% | 60.4 / 63.8 |

上表前三批是修正過程中的工作樹量測；完整正式窗口與 CPU 計法已修正，然而當時尚無乾淨提交版本。完成可靠性程式後，以**乾淨提交 `e2444f3fd1d3c77f925da7a76274346081c1e9d4`** 再跑三批相同的就緒後暖機 10 秒、正式 60 秒。每批 `run_config` 均記錄完整 `command_argv`、該提交與 `git_dirty=false`，原始 JSONL 可按事件時間重算：

| 乾淨提交正常批次 | 正式秒數 | 收到／不同計數；計數率 Hz | 到達間隔 n；p50 / p95 / p99 / 最大 ms | 來源跳號 | CPU 單核心 | RSS p50 / 最大 MiB |
| --- | ---: | --- | --- | ---: | ---: | ---: |
| final 1 | 60.001 | 2,154 / 2,153；35.93 | 2,153；27.33 / 49.01 / 63.11 / 91.55 | 2 | 58.3% | 58.3 / 60.7 |
| final 2 | 60.005 | 2,150 / 2,147；35.86 | 2,149；27.41 / 50.76 / 64.85 / 82.76 | 1 | 57.2% | 57.9 / 60.2 |
| final 3 | 60.009 | 2,126 / 2,124；35.40 | 2,125；27.75 / 51.23 / 64.77 / 81.61 | 1 | 58.1% | 58.0 / 59.9 |

乾淨提交三批收到約 35.4–35.9 張／正式秒，皆未達 55 個不同畫面／秒，也未達 p95 ≤33.4 ms、p99 ≤50 ms。收到影格幾乎都帶不同可見計數；在此擷取條件下，Fixture 自身可見更新率亦僅約 35–36 Hz，不存在已證實的 60 FPS 來源。未擷取時用受控 Fixture 的 `dumpsys gfxinfo org.pas.capturefixture` 累積 `Total frames rendered` 差值作三個 10 秒離線觀察，得到 517 / 10.200 s（50.69 Hz）、444 / 10.229 s（43.41 Hz）、427 / 10.297 s（41.47 Hz）；測量工具與主機／AVD 負載未固定，因此只能證明未擷取時也沒有穩定 60 Hz，不能把差值全部歸因於 gRPC。此 gfxinfo 僅量測專用 Fixture，不作遊戲決策。

慢 consumer 的 `frames_read` 分別 584／298，跳過 297／547 個本機序號；恢復批次的第一次恢復讀取主機駐留 30.85 ms。覆蓋、跳過與收到數是不同事件，不能相加。接收端暫停無保護時，暫停結束後 4.57–47.85 ms 先交付來源序號 599–607 的相對舊圖，55.58 ms 才跳到 621；來源與主機相鄰差值之差由約 +467 ms 回到約 −12 ms。啟用保護的獨立批次丟棄 3 張，暫停結束後 31.04 ms 首張交付序號 654，來源與主機差值之差約 −0.86 ms；沒有觀察到持續回放。這只驗證受控停頓下的相對新鮮度，不是絕對 source frame age。額外的 10 秒最終 watchdog 驗證批次見原始日誌。

同程序負載將收到數降到 461／30 秒並出現 678 個來源跳號，但可見計數跨度約 38 Hz，顯示 Python/GIL 競爭確實阻礙接收，與 AVD 自身繪製率必須分開解讀。640×360 在更低 CPU 下收到約 42.4 張／秒，證明像素量／讀回成本有影響，卻不能代替 1280×720 驗收。RGBA8888 的像素轉換 p95 為 16.20 ms、正式 CPU 約 98.0%，RGB888 仍是較合理配置。主機／AVD 外部負載未單獨施加或控制，不從同程序負載推論其效果。

為拆解 Python 成本，另以 `scripts/profile_grpc_stages.py` 在同一 Fixture 跑四個 10 秒診斷窗口（各暖機 2 秒）：只收原始 gRPC payload 343 張／34.19 Hz、p95 50.24 ms、CPU 46.4%；protobuf 解析 378 張／37.69 Hz、p95 47.38 ms、CPU 60.0%；再加 RGB 正規化 364 張／36.31 Hz、p95 48.14 ms、CPU 62.3%；再加可見計數解碼 359 張／35.90 Hz、p95 51.27 ms、CPU 65.5%。這些是連續不同時間的短批，受來源變動影響；沒有證據顯示 Python 轉換／計數是約 35 FPS 的主要限制。一般 RGB888 gRPC payload／模擬器繪製與讀回鏈仍是優先瓶頸，MMAP 只會改傳輸共享區，且有撕裂風險，暫無理由實作。下一個具體候選是 Windows Graphics Capture：它可繞開 Emulator gRPC readback，使用主機 QPC 時戳；目前 AVD 嵌在 Android Studio，需獨立模擬器視窗才能驗證視窗內容、縮放與遮擋，尚未比較，也沒有選定遊戲用後端。

實機功能方面，已驗證同一 AVD 的認證、原生像素方向與可見計數、500 ms 暫停與保護、橫直方向切換（gRPC 影格 1280×720／rotation 1 轉 720×1280／rotation 0，無 worker error），阻塞來源停止 4.68 ms，並還原至 1280×720／rotation 1。另在 Fixture 已非前景時短暫切到 Android Home 作靜態測試：1.2 秒監控、`max_frame_age_s=0.25`，6 張過渡影格後狀態為 `DEGRADED`、`frame_fresh=false`、無錯誤；其後已恢復 Fixture 前景。inactive→active、來源時戳不連續、序號重置與逾期串流以假來源驗證。真實 AVD 的 inactive／斷線未以中斷裝置或清除資料方式製造，故這兩項實機功能仍待驗證。可靠性測試全綠不代表實機功能與性能目標全完成。

重現範例：`$env:PYTHONPATH='src'; python -m pas.cli capture-bench --serial emulator-5554 --capture-backend emulator-grpc --duration-s 60 --warmup-s 10 --ready-timeout-s 15 --fixture-scale 1 --log measurements/<新 run id>/capture.jsonl`。慢 consumer 加 `--consumer-delay-ms 50|100`，恢復加 `--consumer-recover-after-s 15`，接收暫停加 `--receiver-pause-ms 500`，保護加 `--max-relative-lag-ms 100`，負載加 `--load`。各批獨立資料夾，未覆寫歷史日誌；所有 `summary` 均含窗口、分布與缺測來源年齡標記。原始 JSONL SHA-256：

| 本工作樹 `measurements/` 下路徑 | SHA-256 |
| --- | --- |
| `round2_native_normal_2/capture.jsonl` | `02B5C55356C9B5E560B1C3EAF02A58B1F791EA7388CC9A8A9049259A4DF96AAE` |
| `round2_native_normal_3/capture.jsonl` | `3014A1F31DA25F6987D3F1EF7D5357B364A30DAB63CD3C851003B96E235E96B5` |
| `round2_native_normal_4/capture.jsonl` | `4858C3645F6504A25BAF3C6AA124A52DC7B69E1C35698628875E2A873B811068` |
| `round2_final_normal_1/capture.jsonl` | `00DF6AB7CA194177047E2D217B2F337E167543C22251CAD3AA8B04159ED438A3` |
| `round2_final_normal_2/capture.jsonl` | `5EEB38A3198755CAFECD46DB5856B8DFEE2E3E43FF4A27A74A6BCF9F9428B738` |
| `round2_final_normal_3/capture.jsonl` | `88915CC59600189BDE06193DC8F16948BD7545FE8689ADF42563690A898DA740` |
| `round2_native_consumer50/capture.jsonl` | `2C0612E15C55BAEC3F29F360635D3789F30949919F960BCB69DF75D6C427B2B0` |
| `round2_native_consumer100/capture.jsonl` | `209E0323295FEA3A1F044C95585FBD6B7A4CE46AE4F6D4B49B05E1A3B6A07EBF` |
| `round2_native_recover/capture.jsonl` | `61B21EE4BA416284F4266F5969B3304C4202AD0C899903780BB7A978CB10E4B7` |
| `round2_native_pause/capture.jsonl` | `43C4BBDE4719099BA5E2F3B2FAD10F63A4348825D71A0CB7B4496F83818296B7` |
| `round2_native_guard/capture.jsonl` | `E3898FE150A941906F1A5FC02C144297B4DD51589A6D5FCABEC36E95504374B4` |
| `round2_native_load/capture.jsonl` | `4A8FF3BD876B4D13223CE3049A0CCE05B5244717AA09556358F3FFE6C5E1279D` |
| `round2_native_half/capture.jsonl` | `9771218A188C2D7B03614C834775DF92B98F1F9BD1A588E6EE3902412DCF0FE9` |
| `round2_native_rgba/capture.jsonl` | `3C8763BA652647CC6F45CB379C19E70FAF5F871E9A36E71BC99BC8D818BFEBB3` |
| `round2_guard_final/capture.jsonl` | `0930B1F1F05C7DFA28BBFDB0F3B28566A6654A5186C3665FC1E8167385540E03` |
| `round2_geometry/events.jsonl` | `1BFE5F927DDB83B62E9512227D8BAFB69E7DC1C0E0EFD37506E66D8BBA967B3B` |
| `round2_static/events.jsonl` | `AEBD26CDE46BB773065441C54D48EE7246942B5A93248472C3F21834F1D297C4` |
| `round2_profile_payload/summary.json` | `93200B602922E35E6980F7E43968CC5F25DA196A58B6C0558A94125BF972DF0E` |
| `round2_profile_protobuf/summary.json` | `2F94CEBCB8E03409E58D68F1FFAB9A7D0195C7E4FE9F1512A08C1CF3FA8CE0A4` |
| `round2_profile_rgb/summary.json` | `807197BD9BB32D3920E786B45D84E32930D883BF343C61D0306CAC3F2E7880BE` |
| `round2_profile_fixture/summary.json` | `660391CDEA4C10D9CB84AC18DBA2DE6271457FF35A9AC258C807001AB951790E` |

## 2026-09-24 後續 R2／R3 回歸與原生 Fixture 條件變化

此節是上一節後續；保留所有約 35 Hz 的原始結果與解讀。程式提交 `e92d03e671db0a6d71530ac62d71a50a2d9a9798` 修正兩個可重現缺陷：事件式健康探測進行期間收到新有效串流影格時，不再依探測開始時的舊影格誤轉 `ERROR`；首次正式 consumer skip 以最後一個暖機影格序號為基準，包含窗口邊界後才完成的暖機 callback。假來源回歸涵蓋新影格、真正卡住、inactive、取消及故意延遲第一筆正式消費。安裝 gRPC extras 時以 `python -m unittest discover -s tests -q` 執行，36 項單元測試通過；設定 `PYTHONPATH=src` 的 `python -S` 模式 28 項通過、8 項依賴 gRPC 的測試跳過。真實 AVD 搭配受控探測回傳值時，等候新 gRPC 串流影格後再回傳 `changed`，觀察 `new_valid_frame_during_probe`、`NAVIGATING`、沒有 monitor 錯誤；此探測回傳值由測試控制，不能稱為真實 `getScreenshot` RPC 競態的直接重現。

環境延續上一節 Windows 11、Emulator 37.1.11、Python 3.14.7、`phigros` AVD、`emulator-5554`，原生 `PAS Capture Fixture` 前景、gRPC RGB888、top-down、1280×720／`source_rotation=1`。後續量測時 Fixture 可見繪製率約 59 Hz，明顯高於前一約 35–36 Hz 條件。停止擷取程序後，用只讀 `dumpsys gfxinfo org.pas.capturefixture` 的累積 `Total frames rendered` 差值獨立觀察 606 張／10.2723452 秒（58.99 Hz），且確認 Fixture 前景。這支持來源此時確有接近 60 Hz 的畫面更新，卻**不能證明**為何與前一條件不同：螢幕／視窗狀態、模擬器排程及主機負載未受控，不把提升歸因於 R2／R3 修正。

以下正式基準在乾淨提交 `e92d03e` 執行，`run_config.source_revision.git_dirty=false`。正常批次各就緒後暖機 10 秒、正式 60 秒；停頓及負載批次暖機 5 秒、正式 30 秒。均加 `--ready-timeout-s 15 --fixture-scale 1`，每批獨立 JSONL。百分位數是正式窗口全部相鄰 `capture_complete_ns` 間隔的線性插值；正式秒數含停頓，CPU 是同一正式窗口 process time／monotonic 時長，RSS 是程序 Working Set。`收到／不同計數` 的每秒值用不同計數除正式牆鐘，來源可見計數跨度率另由首末計數和影格跨度估計。主機其他負載未固定；來源 Unix 時戳不與主機 monotonic 直接相減。

| 條件 | 正式秒數 | 收到／不同計數；不同計數／秒 | 可見計數跨度率 Hz | 到達間隔 n；p50 / p95 / p99 / 最大 ms | 來源跳號／相對丟棄／未讀覆蓋／consumer skip | CPU 單核心 | RSS p50 / 最大 MiB |
| --- | ---: | --- | ---: | --- | ---: | ---: | ---: |
| 正常 1 | 60.002 | 3,557 / 3,557；59.28 | 59.32 | 3,556；16.25 / 32.48 / 40.67 / 54.48 | 3 / 0 / 1 / 0 | 74.2% | 59.68 / 63.29 |
| 正常 2 | 60.006 | 3,559 / 3,559；59.31 | 59.45 | 3,558；16.08 / 29.80 / 38.71 / 59.39 | 7 / 0 / 1 / 0 | 79.2% | 58.93 / 62.68 |
| 正常 3 | 60.010 | 3,509 / 3,509；58.47 | 58.48 | 3,508；16.39 / 29.58 / 39.59 / 61.82 | 1 / 0 / 1 / 0 | 84.9% | 58.95 / 63.28 |
| 接收暫停 500 ms，無保護 | 30.006 | 1,762 / 1,761；58.69 | 59.73 | 1,761；16.43 / 28.65 / 36.58 / 509.27 | 29 / 0 / 1 / 0 | 76.5% | 58.04 / 61.55 |
| 接收暫停 500 ms，相對落後上限 100 ms | 30.012 | 1,754 / 1,754；58.44 | 59.55 | 1,753；16.32 / 29.29 / 36.76 / 546.49 | 28 / 4 / 1 / 0 | 72.2% | 58.21 / 63.37 |
| 同程序 Python GIL 忙碌 thread | 30.004 | 510 / 510；17.00 | 59.93 | 509；61.78 / 92.99 / 96.33 / 140.60 | 1,289 / 0 / 8 / 7 | 109.2% | 62.22 / 66.50 |

三批正常結果在此約 59 Hz 來源條件下，皆通過研究門檻：1280×720 至少 55 個不同畫面／秒、到達間隔 p95 ≤33.4 ms、p99 ≤50 ms。這只是**條件性的擷取到達驗收**；前一約 35 Hz 條件並未通過，絕對 source frame age、端到端觸控時序與遊戲判定窗仍未知。受控同程序 GIL 忙碌 thread 讓收到率降至約 17.00 張／秒，來源序號跳 1,289，儘管 Fixture 可見計數跨度仍約 59.93 Hz；這只描述同程序爭用，不代表主機或 AVD 外部負載性能。

接收暫停無保護時，停頓前錨點來源序號 1016；停頓結束後 7.38／13.60／21.60 ms 先收到 1018–1020，兩個時域各自差值再相減顯示額外相對落後約 499／488／480 ms；30.23 ms 到達 1048 時差值回到約 3 ms。啟用相對落後上限的另一批丟棄 4 張，停頓前來源序號 1008，停頓結束後 44.96 ms 首張交付 1041，差值約 7.83 ms；後續沒有持續回放。來源序號跳號、丟棄與容量 1 覆蓋是不同計數，不互相相加；保護仍不能提供絕對新鮮度。

另在修正尚未提交的工作樹做 100 ms 慢 consumer 30 秒批次：正式 30.027 秒收到 1,779 張／1,779 個不同計數、可見計數跨度 59.30 Hz；間隔 n=1,778，p50／p95／p99／最大為 16.06／31.08／38.93／53.15 ms；CPU 96.9%，RSS p50／最大 58.79／62.82 MiB。容量 1 緩衝區覆蓋 1,483 張、consumer 讀 296 張、正式 skip 1,478。最後暖機序號為 299，第一張正式消費序號為 300，首筆 skip=0；因此暖機影格沒有混入正式 skip。此批 `git_dirty=true`，主要用於邊界與慢 consumer 驗證，不能代替乾淨提交的正式正常三批。

可逆故障檢查只針對 Fixture：關閉 AVD 螢幕 2 秒時 `dumpsys power` 顯示非 Awake，`inactive_frames=0`、worker 無錯誤；開啟並恢復 Fixture 前景後收到新 1280×720 影格。Android Studio 視窗最小化 2 秒再恢復期間，同樣收到有效影格、`inactive_frames=0`、worker 無錯誤，Studio 可見狀態已恢復。主動關閉**客戶端**已認證 gRPC source 得到 `gRPC screenshot stream failed: CANCELLED`，重新建立已認證來源後取得 1280×720 影格。這驗證客戶端中斷與重建，不代表整台 AVD 斷線／重連；兩種安全的可視性操作皆未觸發 `0×0` inactive，真實 inactive 路徑尚未實機驗證。假來源的 inactive→active 回歸仍通過。未清除裝置或遊戲資料，也未中斷登入。

可重現正常命令：`$env:PYTHONPATH='src'; python -m pas.cli capture-bench --serial emulator-5554 --capture-backend emulator-grpc --duration-s 60 --warmup-s 10 --ready-timeout-s 15 --fixture-scale 1 --log measurements/<新 run id>/capture.jsonl`。對照批次改 `--duration-s 30 --warmup-s 5`，另加 `--receiver-pause-ms 500`、`--max-relative-lag-ms 100` 或 `--load`；consumer 批次加 `--consumer-delay-ms 100`。競態與故障檢查可用 `python scripts/verify_probe_race.py --serial emulator-5554 --log measurements/<新 run id>/events.jsonl`、`python scripts/verify_capture_faults.py --serial emulator-5554 --screen-off-s 2 --log measurements/<新 run id>/capture.jsonl`，前提是 Fixture 前景；安全可視性檢查的結果見原始日誌。每次新路徑不可覆寫。下列檔案存於此工作樹、被 Git 忽略；SHA-256 供核對，移除 worktree 前須另行保存。

| 本工作樹 `measurements/` 下路徑 | SHA-256 |
| --- | --- |
| `round2_followup_consumer100/capture.jsonl` | `60DAAE4B236314B78502A0D0E09BA079E0CA9B6D2F759426B9991DBDBDBC593A` |
| `round2_followup_clean_normal_1/capture.jsonl` | `BD6D15775D0ED70163614DF4DC2A417EA115AD7C4934206FA133B6C4AF23D8A1` |
| `round2_followup_clean_normal_2/capture.jsonl` | `9B88513502359F9FF2296922CAB618F38185B4A889E02AC860FA439F83BFC14C` |
| `round2_followup_clean_normal_3/capture.jsonl` | `675E5A3CA9AD5EA0A0EC276C9DBF49138C229885EC7400BA111D3A6430DC1D6A` |
| `round2_followup_clean_pause/capture.jsonl` | `DC8088313B820097EF41AEF02790958466C44EE0BA4406E16A5C49266C43A5C5` |
| `round2_followup_clean_guard/capture.jsonl` | `2012268A0266D70AF9A4EC479AA0A305705688C53656EC64732957B51C580A28` |
| `round2_followup_clean_load/capture.jsonl` | `B59477F35A13922E85FEA0878EFB441F4FE184D60426852B6D83130B0F90E395` |
| `round2_followup_gfxinfo/summary.json` | `C8FB83268868C690C36EE78EB418BF96A44B5C684781D4EDA168628D8429EA7B` |
| `round2_probe_followup_2/events.jsonl` | `296F999D854A6554CE3B4B646AD9DC54CACA3C687250D2EAD5BF4F2C5348957F` |
| `round2_fault_followup/capture.jsonl` | `12980ACAB41A577F0A5E810E6BF4A5DDFDE188ECB00645BF99C2D41525EECD60` |
| `round2_visibility_followup/events.jsonl` | `2F03B89A57DD98CFFBA2153C6DC51E43B23781866B39E88ADD238C446BE72516` |
