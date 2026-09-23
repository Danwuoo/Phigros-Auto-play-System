# 2026-09-23 基線量測與限制

## 環境與重現

- 主機：Windows 11 build 26200、AMD64、Python 3.14.7；時間來源為同一程序的 `time.monotonic_ns()`。CPU 型號、主機背景負載與使用率未控制或記錄，故主機排程數值僅是這台機器的初步基線。
- 此行原為 AVD 尚未建立時的歷史盤點；其後使用者已建立並啟動 `phigros` AVD。2026-09-23 本次重新查驗：`emulator-5554` 已連線，Emulator 37.1.11.0，遊戲選單畫面 1280×720。觸控能力與遊戲判定效果仍未驗證。
- 指令見 [README](../README.md#執行與重現)。暖機：主機排程每批先執行 10 次，不納入 100 次統計；合成與緩衝區測試沒有暖機。原始 JSONL 位於本工作樹的 `measurements/`（不納入 Git）；新 gRPC 基準每次使用獨立檔名，舊檔同名重跑可能覆寫。統計採線性插值百分位數；此處排程 jitter 為預定觸控與實際注入開始之誤差的 p95−p5，並另記 p95 絕對中位偏差。擷取間隔 p95−p5 不等於觸控排程 jitter。呼叫返回不代表畫面已反映觸控。

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

此節是本次**新量測**，與上面的歷史 ADB 摘要分開。主機 Windows 11 build 26200，Intel Core Ultra 5 125H（18 邏輯處理器）、Intel Arc／NVIDIA RTX 3050 Laptop GPU；Python 3.14.7，grpcio 1.84.0、protobuf 7.36.2、Pillow 12.3.0。ADB 37.0.1、Emulator 37.1.11.0 build 15917651。`phigros` AVD：Android 16／SDK 36、x86_64、4 vCPU、6144 MB、host GPU、物理顯示 720×1280／320 dpi、目前遊戲選單橫向輸出 1280×720，顯示模式 60 Hz。裝置序號本次為 `emulator-5554`，gRPC 在 127.0.0.1:8554，以本機 discovery 權杖認證。遊戲版本與背景 GPU 使用率未可靠取得；主機其他負載未固定。安裝版 `proto/emulator_controller.proto` SHA-256 為 `1D62C6BCAD5F06621F90EC2BF26C661BA769CCD0F1416B5314D25A68E04EEE5F`。

使用一般 gRPC transport 的 RGB888，請求原尺寸。每批暖機 10 秒、正式收集 60 秒，畫面為已有的遊戲選單，含部分動畫但沒有可驗證的每幀真值；60 Hz 是顯示設定，**不是已確認的來源更新率**。到達間隔按相鄰 `capture_complete_ns` 計，像素就緒間隔另按 `pixels_ready_ns` 計；來源 Unix 微秒原值不與主機 monotonic 直接相減。百分位數採線性插值，CPU 是本程序 process time 除正式量測時間，以單一核心百分比表示；RSS 是本程序 Working Set。原始 JSONL 與診斷 PNG 在下表所列的本工作樹 `measurements/`，被 Git 忽略。

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

正常第 1 批的 2,026 個相鄰計數差全為 `+1`；第 2 批僅有 1 次 `+2`；第 3 批有 2 次重複、2 次 `+2`、1 次 `+3`。這表示實際收到的來源畫面大多連續，但**原生畫面在擷取期間的繪製率也只約 34–35 Hz**；不能從此判定 gRPC 穩定漏掉 60 FPS 來源。縮小請求解析度後收到率約 41.3 Hz、CPU 明顯下降，顯示像素量／主機處理成本有貢獻；這是不同尺寸的單批診斷，不是 1280×720 合格結果。1280×720 三批均未達每秒 55 張不同畫面、到達間隔 p95 ≤33.4 ms、p99 ≤50 ms 的研究目標。沒有可靠的來源產生時間與主機時鐘對齊，絕對 source frame age 仍未知。

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
