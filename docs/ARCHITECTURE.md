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
- 每個 `Frame` 包含本機遞增序號、RGB24 像素、尺寸、`capture_complete_ns`（本機收到完整 payload）、`pixels_ready_ns`（可供視覺模組使用）和發布前記錄的 `published_ns`。gRPC 另記 `source_sequence`、`stream_generation`、`source_rotation` 與 `source_timestamp_us`（Unix 微秒原值）。未校準時 `produced_ns=None`；絕不可直接用 Unix 時戳與主機 monotonic 相減。ADB 的 `capture_complete_ns` 是 PNG bytes 收齊，`pixels_ready_ns` 是解碼完成。
- 每個 `Observation` / `Track` 保留來源 frame 序號。每個 `HitIntent` 包含來源、預測撞線時間、畫面座標、動作類型與信心或不確定度。
- 每個 `TouchCommand` 至少記錄 `scheduled_ns`、接觸點 ID、畫面座標與動作階段（按下、移動、放開）。`TouchReceipt` 記錄注入呼叫開始與返回時間、結果；呼叫返回不等於畫面已反映觸控。
- UI 狀態切換與啟動命令同樣使用主機 monotonic clock。`UiObservation` 及 Session controller 狀態轉移須可連回來源 frame；任何 `UNKNOWN` 狀態不得送出遊玩或導覽觸控。
- 所有視覺座標以原始 frame 像素座標為基準，透過單一明確變換映射到模擬器觸控座標。旋轉、裁切、縮放與黑邊都要納入變換。

## 最新 frame 規則

### 獨立程序交接（已做 Fixture 實機測試）

`ProcessCaptureSource` 是既有 `CaptureSource`／`CaptureWorker` 的相容 adapter。Windows `spawn` 子程序擁有 gRPC channel、token 讀取、stream 和來源 MMAP；父程序擁有固定共享像素區與子程序生命週期。IPC schema 2 的 256-byte header 含 magic、schema、隨機 generation、發布／來源序號、尺寸、RGB24 長度、row order、rotation、CRC32，以及來源通知、來源快照複製、像素就緒、IPC 發布等 monotonic 時間。實體像素只有一個容量受限的 slot，沒有 FIFO；父子使用有期限的跨程序 lock 防止這一段半寫半讀，父程序在 lock 內複製出 immutable `bytes`。CRC 是損壞偵測，lock 才是本段同步依據。死鎖超時後回報錯誤並回收本次子程序，不接受半張圖。輪詢共享序號最多等待 5 ms，避免子程序崩潰時 Windows Event 內部鎖留在被殺程序而造成停止卡死。

`capture_complete_ns` 在 payload 仍是完整 gRPC bytes 到達；MMAP 診斷路徑代表映射區的**一次複製完成**，不等於已證實一致快照。`notification_received_ns`、`snapshot_copy_started_ns`、`snapshot_copy_complete_ns`、`ipc_published_ns`、`parent_snapshot_complete_ns` 分別保留，`produced_ns` 仍為 `None`。所有主機時戳使用同機 `time.monotonic_ns()`；來源 Unix 微秒保持未映射，不作主機時差。Session 拒絕 MMAP，因為來源端寫入與通知並無 reader acknowledgment、fence 或其他足以排除 tearing／下一張覆寫的證據。程序 heartbeat 只證明 child alive，不刷新舊 frame。程序模式對靜態串流回報 `DEGRADED`，不將未知的新鮮度作為日後觸控資格。

資源 owner：父程序在 spawn 前建立來源 MMAP 私有目錄，子程序在該目錄建立映射檔並持有映射與 gRPC。獨立取消執行緒每 20 ms 檢查停止旗標及 parent alive，呼叫只取消 RPC、不釋放像素的 `cancel()`；擷取返回後才在 finally 解除映射、關閉檔案。父程序 join 後回收目錄中本來源的殘留映射檔，Windows server 尚未解除映射時最多重試 0.75 秒；不掃描其他 task 的檔案。2 秒 cooperative 期限後才 terminate，再以各 1 秒 join/kill 為備援；強制停止標 `FORCED_STOPPED`，清理失敗標 `FAILED` 並報錯。`capture_process_shutdown` 事件保留 forced、child_reaped、exitcode、mmap_cleanup_complete；正常取消不累計 child_failures。父程序異常退出時 child 自行取消、解除映射及清理已知私有目錄。IPC SharedMemory 仍由父程序建立／unlink。上述清理不構成生產端像素同步證據，MMAP Session 門控維持。

程序 benchmark 的影格窗口仍使用 `[measurement_start_ns, measurement_end_ns)`，晚完成的暖機 callback 與 consumer 收到的暖機圖都會更新序號基準。資源讀取與影格邊界不可能由這個 API 原子化：`resource_windows.parent_cpu/child_cpu` 保存每次讀取的 before_ns、after_ns、value，以起訖讀取中點之差估計分母，另附最短／最長可能時長及相對影格邊界的 offset bounds。父 CPU 起點在慢速 child 取樣之後、終點在末次 child 取樣之前，排除這兩次量測成本；child CPU 則明確屬於自己的取樣窗口。`snapshot_start_ns` 是整批開始讀取前、`snapshot_end_ns` 是整批結束後，不再漏算取樣耗時。計數差值也是帶有時間括號的資源快照差，不能聲稱精確落在影格窗口；原始取樣記入 phase 事件供重算，缺測仍為 null。

2026-09-24 本機 Emulator 37.1.11／1280×720 動態 Fixture 實測：process payload 三批完整 60 秒、Session 5 秒均能取得有效畫面；MMAP 診斷使用預設 `width=height=0` 時無影格，明確指定 `1280×720` 後能取圖，CLI 因此要求 MMAP 診斷指定正尺寸。MMAP 三批正式窗口及 payload/thread 對照見 [量測紀錄](MEASUREMENTS.md)。MMAP 子程序 CPU 較低，但來源一致性尚未證明；來源可見更新率在批次間下降，無法據此選定低延遲遊戲擷取後端。正常 Session 仍只准 payload，預設執行方式維持 thread。

Capture 只維護容量為 1 的共享緩衝區。新 frame 覆蓋尚未處理的舊 frame；Perception 取得 frame 後才開始計算。追蹤器可以保留少量歷史狀態以估計運動，但不得要求逐一處理所有過去的 frame。落後或超過有效期限的 `HitIntent` 由 Scheduler 丟棄並記錄原因。

## 計時與量測

至少量測相鄰 payload 到達間隔、相鄰像素就緒間隔、主機駐留時間（消費開始減 `capture_complete_ns`）、辨識耗時、排程誤差（注入開始時間減 `scheduled_ns`）及注入呼叫耗時。主機駐留時間不是來源影格年齡；來源時鐘未映射時來源影格年齡未知。簡單目標測試應另外記錄目標首次被觀察到、首次送出觸控，以及畫面首次觀察到觸控效果的時間；後者包含顯示與再次擷取的延遲，應與注入呼叫耗時分開報告。受控測試畫面可額外提供目標出現的真值時間供離線評估，但執行時決策仍只能使用 pixels。

報告樣本數、p50 / p95 / p99、最大值和誤差分布。此專案的 jitter 指相對預定動作時間的實際送出誤差分散程度，不能只以平均延遲代替。若要宣稱足以應付遊戲判定窗，須先有判定窗與端到端誤差的可比量測。

## 尚待實測的選型

擷取後端需評估輸出幀率、畫面年齡、CPU 使用量與掉幀情況。觸控後端需逐項證明 Tap、長按、連續移動、快速 Flick 以及獨立多指的能力，並量測排程誤差。後端應可替換；選型完成時在本文件補充實測環境、結果與限制。

目前擷取路徑的量測、下一批候選及 Phigros 視覺架構見 [畫面擷取與視覺決策技術研究](CAPTURE_AND_VISION_RESEARCH.md)。候選優先序不是後端選定結論；實際採用仍以同一 AVD 上的功能、像素正確性和尾端延遲量測為準。

## 目前實作與選型狀態（2026-09-23）

- `pas.contracts` 定義 `Frame`（RGB24、`capture_complete_ns` 與可選且不推定的 `produced_ns`）、`Observation`、`Track`、`HitIntent`、`TouchCommand`、`TouchReceipt`。追蹤速度以 frame 的擷取完成時間計算，不以辨識耗時推動物體。`HitIntent` 保留速度、殘差和判定線的文字依據；不確定度是像素量化的啟發式估計，尚未校準為機率區間。
- `CaptureWorker` 由可替換的 `CaptureSource` 持續取圖並寫入 `LatestFrame`，其共享緩衝區容量固定為 1。下游以本機序號取得最新 frame；覆蓋數只計尚未消費的 frame。gRPC 來源在停止時取消阻塞 RPC，再回收非 daemon thread 與 channel。來源跳號、應用覆蓋、消費者跳過與 inactive／invalid 各自記錄。執行記錄採 JSONL 串流，不在有日誌檔時把所有影像或事件累積在記憶體。
- `AdbPngCapture` 保留為診斷路徑：`adb exec-out screencap -p`，支援非交錯 8-bit RGB/RGBA PNG。`capture_complete_ns` 是 PNG bytes 收齊，`pixels_ready_ns` 是解碼完成。歷史 AVD 基線非常慢，原始 JSONL 目前缺失，不能重算其百分位數。
- `EmulatorGrpcCapture` 使用安裝版 proto 的 `streamScreenshot`、一般 gRPC transport 和本機 discovery 檔的端點／權杖。先請求 RGB888，可選 RGBA8888，驗證長度後輸出 RGB24；0×0 inactive 影格不發布。串流斷線或來源序號重置會停止並要求重啟，不回放舊圖。`Image.seq` 與本機 `Frame.sequence` 分離；`stream_generation` 在每個新來源物件中為 0，未在同一 worker 內自動重連。動態 fixture 實測發現接收端暫停 500 ms 後，gRPC 暫時交付 3 張較舊畫面；可選的相對落後上限可丟棄該批舊畫面，但不是絕對來源年齡證明。
- 可選 `max_relative_lag_ms` 只比較串流內來源 Unix 時戳差與主機 monotonic 到達時間差，丟棄接收端停頓後相對落後的上游圖；它是不可信的相對診斷，並非絕對影格年齡。基準預設停用。啟用時固定首張錨點；時間缺失、倒退、明顯前跳、凍結或超過 1 秒無有效發布會明確失敗，要求重建來源，不能重設錨點洗掉積壓。來源序號重置同樣失敗；`stream_generation=0` 只代表這次未重連的來源物件。
- 現有 Tracker／Predictor 仍以 `capture_complete_ns` 作時間基準；它沒有補償來源產生到本機接收的未知延遲。原生 1280×720 動態畫面先前三批約 35.4–35.9 張不同畫面／秒、p95 49–51 ms；後續約 59 Hz Fixture 條件下三批約 58.5–59.3 張不同畫面／秒、p95 29.6–32.5 ms。後一條件符合擷取到達研究門檻，繪製率改變的原因未證實。建立可信來源時鐘映射、量測端到端誤差之前，不把 gRPC 影格直接視為可滿足遊戲判定窗的預測輸入。
- 安裝版 Emulator 37.1.11 的 RGB888／RGBA8888 原始行序經 ADB 同場景截圖驗證為 top-down，雖然 proto 註解宣稱 bottom-up。後端提供 `row_order` 明確設定。像素以已定向的原始 frame 座標輸出，保留 `source_rotation` metadata；尺寸或方向變化記錄事件，不在 Capture 加入遊戲座標。
- `SessionController` 已提供擷取先就緒、再發送公開應用啟動命令的交接；事件式 gRPC 以一張有效圖像就緒。`frame_fresh` 僅在最近有效串流影格未逾期時為真。逾期後獨立 `getScreenshot` 探測返回時，在同一狀態鎖內重新檢查最新串流序號／年齡、worker 錯誤、inactive 與停止狀態。探測期間若已有未逾期的新有效串流影格，恢復 `NAVIGATING`，而非用舊探測結果誤報 `ERROR`；新圖若已逾期則維持 `DEGRADED`。相同靜態 pixels 僅標 `DEGRADED` 並撤銷新鮮度；探測失敗或看到變化但串流仍未送達才轉 `ERROR`。協定無法單靠相同 pixels 證明事件式串流仍會送出新畫面，因此 `DEGRADED` 不可作遊玩門控。inactive 短暫轉 `DEGRADED`，逾期報錯，新有效影格可恢復 `NAVIGATING`。可逆的 AVD 客戶端斷線／重連已驗證；螢幕關閉及 Android Studio 最小化均未產生真實 `0×0` inactive，裝置斷線亦未測。目前沒有遊戲畫面分類器或 `PLAYING` 接手機制，也不注入觸控。
- `capture-bench` 的 `CONNECTING → WARMUP → MEASURING → STOPPING` 以主機 monotonic 標記。首張有效影格前的連線／認證不計入暖機與正式窗口；暖機後 CPU process time 與累積計數在兩端各取快照。正式計數再由 JSONL 事件時間的半開區間 `[measurement_start_ns, measurement_end_ns)` 重算，來源接收、發布與消費各依自身事件時間歸屬，可在邊界不同而不強求相等。首次正式消費前以最後一個暖機影格序號作 skip 基準；晚完成的暖機 callback 也會更新該基準，不把暖機影格算成正式 consumer skips。窗口內無影格停頓保留在牆鐘分母；摘要百分位數是至多最近 100,000 筆的線性插值，完整事件保留在串流 JSONL。來源 Unix metadata 不參與窗口或 CPU 計算。
- `GreenTargetDetector`、`VelocityTracker`、`LineCrossingPredictor` 是簡單目標研究用，從當下 RGB pixels 取得位置，使用至多 8 筆追蹤狀態線性估速，計算判定線交會時間。`Scheduler` 可更新未送出的預測、拒絕過期／重複意圖、取消排程，依 monotonic 截止時間注入並記錄收據。
- `FakeTouchBackend` 驗證獨立接觸點的 down／move／up 狀態機和取消釋放；它不證明 Android 多指能力。`CoordinateTransform` 統一處理裁切、黑邊映射及四種直角旋轉。尚無任何已通過模擬器實測的觸控注入後端；`adb shell input tap` 僅列為單點能力候選，未宣稱支援 Hold／Move／Flick／多指。
- 合成 world 的目標出現及撞線真值只供測試 fixture 和離線摘要。執行中的 detector、tracker、predictor、scheduler 不讀真值。合成閉環與主機排程實際分布、限制見 [量測紀錄](MEASUREMENTS.md)。
