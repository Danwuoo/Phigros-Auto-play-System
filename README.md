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

AVD 啟動後先執行 `python -m pas.cli probe` 取得序號。多裝置時必須明確指定 `--serial`；擷取候選的基線命令如下：

```powershell
python -m pas.cli probe --serial emulator-5554
python -m pas.cli capture-bench --serial emulator-5554 --samples 30 --warmup 3 --log measurements/adb_capture_phigros_20260923.jsonl
```

若已知安裝套件名稱，可執行 `python -m pas.cli start-session --serial emulator-5554 --package <套件名稱> --duration-s 30 --log measurements/session.jsonl`。此命令先確認連續有效擷取，再發送公開的啟動命令並持續擷取；目前**不會**判定 `PLAYING` 或注入遊玩觸控。套件名稱僅用於安裝查驗與啟動，不參與遊戲決策。

## 啟動與遊戲介面

規劃中的完整協調器負責選定模擬器、啟動擷取與日誌、開啟已安裝的遊戲，並根據即時畫面切換介面狀態。選單、載入、暫停與結算畫面不會觸發譜面操作；只有持續確認進入遊玩畫面後，才啟用預測與觸控排程。第一版將由使用者手動選曲，程式再依畫面辨識接手。**目前只實作擷取先就緒與開啟遊戲的交接，畫面分類／自動接手尚未實作。**詳見[架構與資料契約](docs/ARCHITECTURE.md#啟動協調與介面狀態)。

## 文件

- [AGENTS.md](AGENTS.md)：後續開發者與自動化代理的工作準則。
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)：模組邊界、時間戳與資料契約。
- [docs/ROADMAP.md](docs/ROADMAP.md)：第一版里程碑與驗收方式。
- [docs/MEASUREMENTS.md](docs/MEASUREMENTS.md)：本機環境盤點、合成與主機時序基線。

## 目前狀態

合成閉環和主機排程基線可重現。ADB PNG 擷取已在 `phigros` AVD 上量測，約每 3 秒取得一張解碼畫面，不能作為即時遊玩擷取後端；需改用更快的擷取路徑並重新量測。觸控後端尚未選定；須先在測試畫面驗證，再做遊戲專用辨識。
