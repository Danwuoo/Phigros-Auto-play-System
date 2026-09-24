# Phigros Auto-play System

以 **Android 模擬器的即時畫面** 為唯一遊戲狀態來源，研究從畫面辨識、時間預測到虛擬觸控的完整閉環。目前已有合成畫面／假觸控閉環、主機排程基線、ADB 擷取候選與擷取優先的啟動協調器，並完成一台 AVD 的連線與擷取基線。**目前擷取速度不足，且尚無多點觸控後端或 Phigros 辨識；程式不會操作遊戲譜面。**

## 目標與邊界

- 只根據即時 pixels 決定操作；不讀譜面、遊戲記憶體或其他可直接取得 Note 資訊的資料。
- 不以預先錄製的按鍵序列操作。允許記錄觸控事件、執行日誌與畫面，供離線分析延遲和辨識錯誤；回放資料不得成為執行時的決策來源。
- 永遠優先處理最新 frame。處理來不及時丟棄舊 frame，不排隊補做過期辨識。
- 預測 Note 到達判定線的時間，再排程觸控；不以「已撞線」作為唯一觸發條件。
- 每個 frame、辨識結果、排程動作和實際注入事件都附上時間戳。評估時同時看延遲分布和 jitter。

## 預計資料流

```text
Android Emulator
    ↓ 畫面擷取
最新 frame 緩衝區
    ↓
OpenCV / 視覺辨識
    ↓
Note 與判定線追蹤
    ↓
撞線時間預測
    ↓
觸控排程器
    ↓
虛擬多點觸控 → Android Emulator
```

目前的合成測試完整經過畫面 pixels、辨識、追蹤、預測、排程、假觸控與再次觀察畫面。真實模擬器上的 Tap / Hold / Move / Flick / 多點觸控能力均未驗證；確認其能力與延遲後才接入 Phigros Note 與判定線辨識。

## 執行與重現

需要 Python 3.10 以上。以下命令在 PowerShell、倉庫根目錄執行；不需要額外 Python 套件。

```powershell
$env:PYTHONPATH='src'
python -m unittest discover -s tests -v
python -m pas.cli probe
python -m pas.cli synthetic --count 30 --fps 60 --log measurements/synthetic_60fps.jsonl
python -m pas.cli synthetic --count 30 --fps 60 --recognition-delay-ms 5 --log measurements/synthetic_delay5ms.jsonl
python -m pas.cli synthetic --count 30 --fps 60 --recognition-delay-ms 25 --log measurements/synthetic_delay25ms.jsonl
python -m pas.cli buffer-bench --duration-s 1 --capture-interval-ms 2 --consumer-delay-ms 20 --log measurements/buffer_slow_consumer.jsonl
python -m pas.cli schedule-bench --samples 100 --warmup 10 --interval-ms 10 --log measurements/scheduler_idle.jsonl
python -m pas.cli schedule-bench --samples 100 --warmup 10 --interval-ms 10 --load --log measurements/scheduler_load.jsonl
```

`synthetic` 使用虛擬 monotonic clock；其零排程誤差和零注入耗時不是實機性能。`schedule-bench` 使用主機 `time.monotonic_ns()` 和假觸控，量測主機喚醒與 Python 排程的基線。兩者的 JSONL 僅供離線診斷，不會回饋為執行時按鍵序列。數據見 [量測紀錄](docs/MEASUREMENTS.md)。

## 模擬器準備與下一步

本機已有 `phigros` AVD（Android 16／API 36.1、標準 4 KB Google Play x86_64 映像），目前 ADB 序號為 `emulator-5554`，遊戲已安裝且使用者可手動進入選曲畫面。序號可能在重啟後改變，先以 `adb devices -l` 確認；程式不預設特定型號或版本。使用者自行安裝正版 Phigros、自行登入已完成新手教學的全新帳號，並手動處理選單與選曲；程式不接收帳密或驗證碼，也不操作登入或教學。實測擷取性能與限制見 [量測紀錄](docs/MEASUREMENTS.md)。

### 持續 gRPC 畫面串流

一般合成與 ADB 路徑不需額外 Python 套件；gRPC 後端需安裝可選依賴：

```powershell
python -m pip install -e '.[emulator-grpc]'
$env:PYTHONPATH='src'
python -m pas.cli capture-bench --serial emulator-5554 --capture-backend emulator-grpc --duration-s 60 --warmup-s 10 --ready-timeout-s 15 --fixture-scale 1
python -m pas.cli capture-bench --serial emulator-5554 --capture-backend emulator-grpc --duration-s 60 --warmup-s 10 --consumer-delay-ms 50
python -m pas.cli capture-bench --serial emulator-5554 --capture-backend emulator-grpc --duration-s 30 --warmup-s 5 --consumer-delay-ms 100 --consumer-recover-after-s 15
python -m pas.cli capture-bench --serial emulator-5554 --capture-backend emulator-grpc --duration-s 30 --warmup-s 5 --receiver-pause-ms 500
python -m pas.cli capture-bench --serial emulator-5554 --capture-backend emulator-grpc --duration-s 30 --warmup-s 5 --receiver-pause-ms 500 --max-relative-lag-ms 100
```

每次量測自動建立獨立的 `measurements/<run-id>/capture.jsonl` 和一張診斷 PNG。CLI 從模擬器的本機 discovery 檔以選定 ADB 序號找出唯一執行個體、端點與權杖；不會在日誌輸出權杖。若 discovery 無法使用，可同時提供 `--grpc-endpoint 127.0.0.1:<port>` 和 `--grpc-token-file <本機私有檔案>`。格式可選 `--image-format rgb888|rgba8888`，寬高 0 表示使用原尺寸。`--row-order` 預設 `top-down`，在本機 Emulator 37.1.11 與 ADB 畫面對照正確；其他版本須用非對稱畫面確認。gRPC 中斷會報錯，不會靜默退回 ADB。

量測先進入 `CONNECTING`，首張有效圖到達後才開始 `WARMUP`，暖機完成後才進入完整 `MEASURING` 窗口。`--ready-timeout-s` 控制就緒期限；未就緒、正式窗口無法證明動態 Fixture 計數、或擷取失敗均以非零退出碼回報。摘要分開列出正式 monotonic 牆鐘時長、首末有效影格跨度、無影格停頓與同窗口 CPU／丟棄計數。正常靜態畫面可用於 Session，不能當成動態性能基準。

`--max-relative-lag-ms` 是可選的上游暫存保護：只比較同一串流內「主機到達時間差」與「來源 Unix 時戳差」，超過指定差值時丟棄該影格。它不測量絕對來源年齡；來源時戳若失準，也可能誤丟影格。基準測試預設停用，啟用時會分開計數與記錄 `relative_stale_drops`。
來源時間缺失、倒退、明顯前跳、停止前進或持續丟棄超過 1 秒時，保護會明確停止串流並要求重建來源；不重新設錨後接收積壓圖。來源 Unix metadata 不能用於絕對影格年齡或觸控排程。

`start-session` 也支援 `--capture-backend emulator-grpc`。事件式串流的就緒條件是一張有效圖像；靜態畫面不會偽造新影格。此命令只啟動應用並監控 `NAVIGATING`／`DEGRADED` 等擷取狀態，不判斷遊玩狀態或注入觸控。
事件式串流逾期時，Session 以獨立的 `getScreenshot` 探測傳輸及可見 pixels。探測返回後會重新讀取最新串流影格、inactive、worker 錯誤與停止狀態；若探測期間已有未逾期的新有效影格，恢復 `NAVIGATING`。相同靜態 pixels 只標為 `DEGRADED`，`frame_fresh=False`；探測失敗或探測到變化但串流仍未送達時轉為 `ERROR`。未確認的新鮮度不供未來遊玩門控使用。

`fixtures/capture/index.html` 是可重現的動態像素測試頁，含非對稱角落色塊、可見計數與位元編碼。其計數只供測試統計，不接入遊戲決策。可在倉庫根目錄執行 `python -m http.server 8765 --bind 127.0.0.1 --directory fixtures/capture`，由使用者在 AVD 瀏覽器開啟 `http://10.0.2.2:8765/`。先看輸出的診斷 PNG，依瀏覽器工具列位置與像素縮放設定 `--fixture-x`、`--fixture-y`、`--fixture-scale`、`--fixture-scale-y`，再跑長批次；本機 Chrome 直向 720×1280 的校準值為 `--fixture-y 162 --fixture-scale 2 --fixture-scale-y 2.25`。實際 callback 更新率由可見計數的首末值估計，不能由 AVD 60 Hz 設定推定。

另有不需網路的原生橫向測試畫面原始碼 `fixtures/capture_android/`。可在已安裝 Android SDK build-tools 36.0.0、platforms android-37.0 與 Android Studio JBR 的環境執行 `python scripts/build_capture_fixture.py`，產生 `measurements/fixture_android/pas-capture-fixture.apk`。以 `adb -s emulator-5554 install -r measurements/fixture_android/pas-capture-fixture.apk` 安裝後，從 AVD 主畫面開啟「PAS Capture Fixture」。此 APK 僅繪圖，沒有讀取遊戲或注入觸控；開啟後仍須用診斷 PNG 驗證實際畫面與計數座標。本機橫向 1280×720 使用 `--fixture-scale 1`。

分段性能診斷可執行 `python scripts/profile_grpc_stages.py --serial emulator-5554 --mode payload --duration-s 10 --warmup-s 2 --log measurements/<新 run id>/summary.json`，`--mode` 依序可改為 `protobuf`、`rgb`、`fixture`。四種模式會分別量測只收 payload、protobuf 解析、RGB 正規化及可見計數解碼，僅作受控成本比較，不發布給遊戲決策。原生 Fixture 方向須用診斷 PNG 確認；Android Activity 橫向不保證 Emulator gRPC 輸出本身已是正向 1280×720。

`proto/emulator_controller.proto` 複製自本機 Android Emulator 37.1.11.0（build 15917651）的 `emulator/lib`，SHA-256 `1D62C6BCAD5F06621F90EC2BF26C661BA769CCD0F1416B5314D25A68E04EEE5F`，原始檔附 Apache 2.0 授權標頭。更新 SDK 後可用 `python -m pip install grpcio-tools` 與 `python -m grpc_tools.protoc -I proto --python_out=src/pas/emulator_proto proto/emulator_controller.proto` 重建 bindings，並重新驗證端點、像素行方向與色彩。

AVD 啟動後先執行 `python -m pas.cli probe` 取得序號。多裝置時必須明確指定 `--serial`；擷取候選的基線命令如下：

```powershell
python -m pas.cli probe --serial emulator-5554
python -m pas.cli capture-bench --serial emulator-5554 --samples 30 --warmup 3 --log measurements/adb_capture_phigros_20260923.jsonl
```

若已知安裝套件名稱，可執行 `python -m pas.cli start-session --serial emulator-5554 --package <套件名稱> --duration-s 30 --log measurements/session.jsonl`。此命令先確認連續有效擷取，再發送公開的啟動命令並持續擷取；目前**不會**判定 `PLAYING` 或注入遊玩觸控。套件名稱僅用於安裝查驗與啟動，不參與遊戲決策。

## 啟動與遊戲介面

規劃中的完整協調器負責選定模擬器、啟動擷取與日誌、開啟已安裝的遊戲，並根據即時畫面切換介面狀態。選單、載入、暫停與結算畫面不會觸發譜面操作；只有持續確認進入遊玩畫面後，才啟用預測與觸控排程。第一版將由使用者手動選曲，程式再依畫面辨識接手。**目前只實作擷取先就緒與開啟遊戲的交接，畫面分類／自動接手尚未實作。**詳見[架構與資料契約](docs/ARCHITECTURE.md#啟動協調與介面狀態)。

## 文件

### 獨立擷取程序（2026-09-24 冷開發）

`emulator-grpc` 新增可選 `--capture-execution process`；預設仍是原本的 `thread`。程序模式以 Windows `spawn` 建立子程序，子程序持有認證串流，經固定 16 MiB 上限的共享像素區把最新 RGB24 畫面交給父程序；父程序複製為不可變 `Frame.rgb`。正常 Session 僅允許 `--grpc-transport payload`。子程序阻塞時停止期限為 2 秒 cooperative 等待，之後僅強制回收本次建立的子程序。靜態畫面沒有新串流回覆時，Session 會退到 `DEGRADED`，不把 heartbeat 當新圖。

```powershell
$env:PYTHONPATH='src'
python -m pas.cli offline-capture-bench --duration-s 3 --warmup-s 1 --width 1280 --height 720
python scripts/offline_capture_comparison.py --duration-s 2 --warmup-s 0.5 --width 1280 --height 720 --output-dir measurements/another_offline_run
python scripts/recompute_offline_capture.py measurements/another_offline_run/process-payload.jsonl
# 本機已使用前景 PAS Capture Fixture 實測；以下命令可重跑：
python -m pas.cli capture-bench --serial emulator-5554 --capture-backend emulator-grpc --capture-execution process --grpc-transport payload --duration-s 60 --warmup-s 10 --fixture-scale 1
python -m pas.cli start-session --serial emulator-5554 --package org.pas.capturefixture --capture-backend emulator-grpc --capture-execution process --grpc-transport payload --duration-s 5
```

`--grpc-transport mmap` 只能在 `capture-bench --capture-execution process --diagnostic-mmap` 明確啟用。本機 Emulator 37.1.11 的 MMAP 診斷還須指定 `--width 1280 --height 720`；預設 `0×0` 不交付影格，CLI 現在會先報錯。其 `consistency=unverified`：Emulator 的 MMAP 生產端沒有已證明的讀取同步，通知和像素可錯配，診斷 PNG 與速率均不能視為有效 Session 畫面。`start-session` 直接拒絕 MMAP。`--max-rgb-bytes` 上限為 16 MiB；超容量會失敗並要求以新程序重建，不能在串流中縮放映射。詳見 [低延遲擷取計畫](docs/CAPTURE_LOW_LATENCY_PLAN.md) 和 [量測紀錄](docs/MEASUREMENTS.md)。

- [AGENTS.md](AGENTS.md)：後續開發者與自動化代理的工作準則。
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)：模組邊界、時間戳與資料契約。
- [docs/ROADMAP.md](docs/ROADMAP.md)：第一版里程碑與驗收方式。
- [docs/MEASUREMENTS.md](docs/MEASUREMENTS.md)：本機環境盤點、合成與主機時序基線。
- [docs/CAPTURE_AND_VISION_RESEARCH.md](docs/CAPTURE_AND_VISION_RESEARCH.md)：擷取候選、量測方法與混合式視覺決策方案。
- [docs/CAPTURE_IMPLEMENTATION_PLAN.md](docs/CAPTURE_IMPLEMENTATION_PLAN.md)：持續擷取的驗證計畫與研究門檻。

## 目前狀態

合成閉環和主機排程基線可重現。ADB PNG 的歷史基線約每 3 秒取得一張解碼畫面；原始 JSONL 目前缺失，不能重算。gRPC 原始畫面串流已能在本機 AVD 正確認證與取圖。先前原生 1280×720 動態 Fixture 三批各 60 秒約每秒取得 35.4–35.9 個不同畫面；在後續約 59 Hz 的 Fixture 繪製條件下，乾淨提交的三批各取得 3,509–3,559 個不同畫面／60 秒，到達間隔 p95 為 29.58–32.48 ms、p99 為 38.71–40.67 ms，符合該條件的研究門檻。兩組繪製條件不同，差異原因尚未證實；受控停頓仍顯示短暫舊圖，可選相對落後保護在此條件下丟棄舊圖。詳見[量測紀錄](docs/MEASUREMENTS.md)。來源絕對年齡與遊戲判定窗適用性未證明；真實 `0×0` inactive 仍未在 AVD 重現。已驗證可逆的客戶端 gRPC 斷線與重新連線，但未測整台 AVD 斷線。遊戲用擷取後端與觸控後端均未選定；須先完成擷取能力及時序，再做遊戲專用辨識。

2026-09-24 使用者啟動模擬器後，獨立程序 payload 在原生 Fixture 上完成三批各 60 秒及 5 秒 Session；thread payload 也做三批對照。MMAP 指定 1280×720 後完成三批診斷取圖，子程序 CPU 較低，但共享像素一致性仍未證明，不能供 Session。這次 Fixture 可見更新率約 39–44 Hz，與上述約 59 Hz 的歷史條件不同，沒有選定新預設後端。批次分布、負載結果與原始檔雜湊見[量測紀錄](docs/MEASUREMENTS.md)。
