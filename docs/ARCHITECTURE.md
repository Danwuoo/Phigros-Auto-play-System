# 架構與資料契約

## 閉環模組

| 模組 | 輸入 | 輸出 | 責任 |
| --- | --- | --- | --- |
| Capture | 模擬器畫面 | `Frame` | 取得畫面、轉為統一像素格式，只發布最新 frame |
| Perception | `Frame` | `Observation` | 偵測可見目標、Note 與判定線及其畫面座標 |
| Tracker | `Observation` | `Track` | 跨 frame 對應物件，估計位置、速度與不確定度 |
| Predictor | `Track` | `HitIntent` | 估算 Note 到達判定線的時間與觸控位置 |
| Scheduler | `HitIntent` | `TouchCommand` | 安排動作時間、處理過期與衝突動作 |
| Input backend | `TouchCommand` | `TouchReceipt` | 送出 Tap / Hold / Move / Flick / 多點觸控並回報送出狀態 |
| Telemetry | 各模組事件 | 結構化日誌 | 建立同一條時間線，計算延遲與 jitter |

第一個閉環測試可以用簡單目標偵測器取代 Note 追蹤與預測，但仍經過相同的排程器和觸控後端。

## 啟動協調與介面狀態

Session controller 是遊戲外圍的控制元件，負責一鍵啟動與安全切換，不從遊戲內部資料推斷操作時機：

1. 選定並記錄唯一的模擬器序號、解析度、方向、縮放與後端設定；確認 ADB 連線及遊戲已安裝。啟動遊戲可使用 Android 的公開啟動介面，但套件資訊只用於啟動與診斷，不作遊戲決策。
2. 先啟動 Telemetry、Capture 與最新 frame 緩衝區，確認持續收到有效畫面後，再要求模擬器開啟遊戲；記錄啟動命令與畫面首次出現的 monotonic 時間戳。啟動命令返回不代表遊戲已載入。
3. 介面辨識器只根據最新 frame 輸出 `UiObservation`：`MENU`、`LOADING`、`PLAYING`、`PAUSED`、`RESULT` 或 `UNKNOWN`，並附來源 frame、辨識完成時間與信心。畫面未穩定或信心不足時維持 `UNKNOWN`，不因固定等待秒數直接進入遊玩狀態。
4. `PLAYING` 連續通過可設定的確認條件後才啟用 Predictor / Scheduler。離開 `PLAYING`、畫面逾時、ADB 中斷或使用者停止時立即停用排程，取消未送出的觸控並釋放仍按住的接觸點。重新進入遊玩畫面需重新確認；Tracker 不沿用上一局的狀態。
5. 第一版在 `MENU` 等非遊玩狀態交由使用者手動操作。若後續加入自動導覽，應由獨立的 UI navigator 根據當前可見按鈕產生觸控意圖，每次操作後重新觀察畫面與確認轉移；不得用固定座標、延遲或預錄序列盲目走完整個選單。UI navigator 與譜面 Predictor 互斥使用觸控後端。

Session controller 的狀態可為 `DISCONNECTED → CAPTURING → LAUNCHING → NAVIGATING → ARMED → PLAYING → RESULT`，任何階段皆可轉至 `STOPPED` 或 `ERROR`。`ARMED` 表示已確認遊玩畫面、準備處理新 frame；是否有實際音符仍由 Perception / Predictor 判斷。所有狀態轉移記錄原因、來源 frame 與 monotonic 時間戳。此流程中的「同步啟動」是可驗證的就緒交接，而非假設兩個程序在同一瞬間啟動。

## 時間與座標

- 主機端時間戳統一使用 `time.monotonic_ns()` 或語義相同的 monotonic clock，以奈秒儲存。wall clock 只用於辨識日誌檔案，不參與時差計算。
- 每個 `Frame` 至少包含序號、像素、尺寸、`capture_complete_ns`。若擷取後端能提供可靠的曝光或產生時間，可額外記錄，但不能把取回時間誤寫成曝光時間。
- 每個 `Observation` / `Track` 保留來源 frame 序號。每個 `HitIntent` 包含來源、預測撞線時間、畫面座標、動作類型與信心或不確定度。
- 每個 `TouchCommand` 至少記錄 `scheduled_ns`、接觸點 ID、畫面座標與動作階段（按下、移動、放開）。`TouchReceipt` 記錄注入呼叫開始與返回時間、結果；呼叫返回不等於畫面已反映觸控。
- UI 狀態切換與啟動命令同樣使用主機 monotonic clock。`UiObservation` 及 Session controller 狀態轉移須可連回來源 frame；任何 `UNKNOWN` 狀態不得送出遊玩或導覽觸控。
- 所有視覺座標以原始 frame 像素座標為基準，透過單一明確變換映射到模擬器觸控座標。旋轉、裁切、縮放與黑邊都要納入變換。

## 最新 frame 規則

Capture 只維護容量為 1 的共享緩衝區。新 frame 覆蓋尚未處理的舊 frame；Perception 取得 frame 後才開始計算。追蹤器可以保留少量歷史狀態以估計運動，但不得要求逐一處理所有過去的 frame。落後或超過有效期限的 `HitIntent` 由 Scheduler 丟棄並記錄原因。

## 計時與量測

至少量測擷取間隔、frame 年齡（辨識開始時間減 `capture_complete_ns`）、辨識耗時、排程誤差（注入開始時間減 `scheduled_ns`）及注入呼叫耗時。簡單目標測試應另外記錄目標首次被觀察到、首次送出觸控，以及畫面首次觀察到觸控效果的時間；後者包含顯示與再次擷取的延遲，應與注入呼叫耗時分開報告。受控測試畫面可額外提供目標出現的真值時間供離線評估，但執行時決策仍只能使用 pixels。

報告樣本數、p50 / p95 / p99、最大值和誤差分布。此專案的 jitter 指相對預定動作時間的實際送出誤差分散程度，不能只以平均延遲代替。若要宣稱足以應付遊戲判定窗，須先有判定窗與端到端誤差的可比量測。

## 尚待實測的選型

擷取後端需評估輸出幀率、畫面年齡、CPU 使用量與掉幀情況。觸控後端需逐項證明 Tap、長按、連續移動、快速 Flick 以及獨立多指的能力，並量測排程誤差。後端應可替換；選型完成時在本文件補充實測環境、結果與限制。

## 目前實作與選型狀態（2026-09-23）

- `pas.contracts` 定義 `Frame`（RGB24、`capture_complete_ns` 與可選且不推定的 `produced_ns`）、`Observation`、`Track`、`HitIntent`、`TouchCommand`、`TouchReceipt`。追蹤速度以 frame 的擷取完成時間計算，不以辨識耗時推動物體。`HitIntent` 保留速度、殘差和判定線的文字依據；不確定度是像素量化的啟發式估計，尚未校準為機率區間。
- `CaptureWorker` 由可替換的 `CaptureSource` 持續取圖並寫入 `LatestFrame`，其共享緩衝區容量固定為 1。下游以序號取得最新 frame；覆蓋數只計尚未消費的 frame。執行記錄採 JSONL 串流，不在有日誌檔時把所有事件累積在記憶體。
- `AdbPngCapture` 是候選擷取路徑：`adb exec-out screencap -p`，支援非交錯 8-bit RGB/RGBA PNG。`capture_complete_ns` 是取回 PNG bytes 的時間，不是畫面曝光時間；解碼完成另記 `decode_complete_ns`。尚無 AVD，無擷取性能或幀齡結論。
- `SessionController` 已提供擷取先就緒、再發送公開應用啟動命令的交接；目前停在 `NAVIGATING`，尚無遊戲畫面分類器或 `PLAYING` 接手機制。它不注入觸控。
- `GreenTargetDetector`、`VelocityTracker`、`LineCrossingPredictor` 是簡單目標研究用，從當下 RGB pixels 取得位置，使用至多 8 筆追蹤狀態線性估速，計算判定線交會時間。`Scheduler` 可更新未送出的預測、拒絕過期／重複意圖、取消排程，依 monotonic 截止時間注入並記錄收據。
- `FakeTouchBackend` 驗證獨立接觸點的 down／move／up 狀態機和取消釋放；它不證明 Android 多指能力。`CoordinateTransform` 統一處理裁切、黑邊映射及四種直角旋轉。尚無任何已通過模擬器實測的觸控注入後端；`adb shell input tap` 僅列為單點能力候選，未宣稱支援 Hold／Move／Flick／多指。
- 合成 world 的目標出現及撞線真值只供測試 fixture 和離線摘要。執行中的 detector、tracker、predictor、scheduler 不讀真值。合成閉環與主機排程實際分布、限制見 [量測紀錄](MEASUREMENTS.md)。
