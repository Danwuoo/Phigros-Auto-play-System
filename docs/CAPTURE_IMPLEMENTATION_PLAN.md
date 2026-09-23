# 即時畫面擷取實作計畫

日期：2026-09-23，進度更新：2026-09-24。狀態：gRPC 實作、Chrome 與原生動態 fixture、三批原生 1280×720 基準及上游停頓測試已完成；實測未達約 60 FPS 研究目標，來源絕對年齡、其他候選比較與完整選型仍待完成。本文件的門檻是研究目標，不代表後端已選定或可支援 Phigros 判定窗。

## 目標與範圍

將目前秒級的逐張 ADB PNG 擷取替換為可持續接收最新畫面的擷取候選，完成像素、生命週期、時間語義與尾端延遲驗證。優先實作 Android Emulator gRPC 原始像素串流；只有實測顯示不可用或不適合，才依本文條件比較替代方案。

本次交付包括擷取後端、CLI／Session 整合、測試、可重現基準工具與文件。觸控注入、遊戲專用辨識、撞線預測改造、深度學習與自動選曲不在本次範圍。不可讀取遊戲記憶體、譜面、存檔或按鍵序列；連線資訊與公開顯示設定只用於擷取及診斷。

開發前依序閱讀 `AGENTS.md`、`README.md`、`docs/ARCHITECTURE.md`、`docs/ROADMAP.md`、`docs/CAPTURE_AND_VISION_RESEARCH.md` 與本文件。保留既有合成測試及 ADB 診斷路徑。

## 已知起點與待核實資訊

- 研究文件記錄 AVD 為 Android 16／API 36.1、1280×720 橫向、60 Hz；裝置序號與實際環境須重新查驗，不能固定為 `emulator-5554`。
- 既有文件記錄 ADB 呼叫與純 Python PNG 解碼均過慢，但原始 `measurements/adb_capture_phigros_20260923.jsonl` 目前不在此工作目錄；舊百分位數只能引用為歷史紀錄，不能冒稱已重算。
- `src/pas/capture.py` 的 `CaptureSource.capture()` 是同步取圖介面；`CaptureWorker` 持續取圖並發布到容量 1 的 `LatestFrame`。目前 `stop()` 無法直接取消阻塞中的來源。
- `Frame` 使用 RGB24；`capture_complete_ns` 在 ADB 路徑表示 PNG bytes 收齊，解碼時間另記。既有 benchmark 的間隔來自 bytes 收齊時間，不是解碼完成間隔。
- 本機 SDK 曾確認有 `emulator/lib/emulator_controller.proto`，包含 `streamScreenshot`、RGB888／RGBA8888、`Image.seq` 及 Unix 微秒時間戳。MMAP 是傳輸方式，不是像素格式，且協定提醒可能撕裂。
- 架構與路線圖仍有「尚無 AVD」的過時敘述；更新時清楚區分歷史基線與目前實測狀態。

## A. 能力盤點與最小連線驗證

1. 記錄 OS、Python、CPU／GPU、Emulator／ADB 版本、AVD／Android 版本、裝置序號、解析度、方向、縮放與 GPU 模式；遊戲版本若無法可靠取得則標示未知。
2. 從 SDK 設定或可配置路徑找到與本機 Emulator 對應的 proto。記錄版本／雜湊及來源，避免只依賴網路 master。採用產生的 bindings 時保留可重建指令與必要授權資訊。
3. 對照本機版本的端點發現與認證規則，建立明確的 serial → emulator instance → gRPC endpoint 對應。多裝置或對應不明時拒絕猜測；允許明確設定覆寫。
4. 使用既有本機認證端點，不為方便關閉認證。token、JWT、認證檔內容不寫入日誌、命令列範例或錯誤訊息。不要直接輸出可能含權杖的完整程序命令列。
5. 首先請求 RGB888 經一般 gRPC transport 傳輸；若該格式在本機不正常，再試 RGBA8888 並轉為 RGB24。收到有效影格後記錄尺寸、長度、序號與接收時間。
6. grpc/protobuf 依賴採可選 extras 或同等清楚的隔離方式；沒有安裝時既有合成／ADB 功能仍可用，錯誤訊息給出安裝方法。依實際 Python 相容性驗證，不預先硬編套件版本。

完成條件：能在選定 AVD 上正確認證、接收並保存少量診斷畫面；或以具體錯誤及環境證据判定此路徑不可用。不要把 proto 存在視為連線成功。

## B. 串流後端與資源生命週期

建議新增 `src/pas/capture_grpc.py`，端點／認證設定可獨立模組化；命名可依專案慣例調整。

- 維持可替換 `CaptureSource` 邊界；必要時擴充 start／close／cancel 或 context manager，確保所有既有來源同步適配。
- 一個持續讀取串流的工作者負責接收與發布。不得由慢速辨識迴圈逐次拉取網路影格，避免遠端或 gRPC 緩衝積壓。若轉換需要另一工作者，其輸入仍只能保留最新的完整影格，所有應用佇列均有明確上限。
- 區分遠端 `source_sequence`、本機發布序號與串流 generation。重連或來源序號重置時，本機排序仍須正確；不得讓 `LatestFrame.read_after()` 永久等不到下一張。
- 驗證 RGB／RGBA 通道、資料長度、像素行方向、旋轉、尺寸與黑邊。以非對稱測試圖確認，不能只靠看似正常的選單。以原始 frame 座標為基準，不在 Capture 混入遊戲座標。
- 0×0／inactive frame 不得發布為合法 `Frame`；記錄來源狀態。損毀長度、格式錯誤、連線關閉與認證失效須可辨識，不默默發布舊圖。
- 關閉時先取消串流以喚醒阻塞讀取，再回收 thread／channel；停止應冪等，測試目標為 2 秒內完成。清理失敗要回報，不以 daemon thread 掩蓋。
- 尺寸／方向變更產生明確事件，必要時重建轉換狀態；重連採有限重試與退避，或清楚停止並要求重啟，不能無限卡住。
- 靜態畫面可能沒有新事件：不得複製舊圖、更新時間戳來假造新 frame。將「未有新像素」、「來源 inactive」、「連線失效」分開處理。Session 的就緒與逾時策略需適應事件式串流，且不得誤宣稱已確認遊玩狀態。
- JSONL 串流寫入；避免把完整影像或所有事件累積在記憶體。同步日誌若影響接收，需量測並以有上限的方式處理。

## C. 明確的時間與統計契約

所有主機端時差用同一 `time.monotonic_ns()` 時域。增加欄位時優先保留既有欄位相容性並更新文件。

| 欄位／指標 | 定義 |
| --- | --- |
| `capture_complete_ns` | 相容欄位：本機已取得完整 encoded／raw payload 的時刻；不是來源產生時間 |
| `decode_complete_ns` 或 `pixels_ready_ns` | RGB24 已可供視覺模組使用的時刻；原始像素仍包含轉換／複製成本 |
| `published_ns` | 最新 frame 發布完成的時刻 |
| `source_timestamp_us` | gRPC 提供的 Unix 微秒估計值，僅保留為附時域標記的原始 metadata |
| `produced_ns` | 只有可靠映射到主機 monotonic 且交代誤差時才填；否則維持 `None` |
| 接收到達間隔 | 相鄰 `capture_complete_ns` 的差 |
| 像素就緒間隔 | 相鄰 `pixels_ready_ns` 的差，與到達間隔分開報告 |
| 轉換耗時 | `pixels_ready_ns - capture_complete_ns` |
| 主機駐留時間 | consumer 開始處理減 `capture_complete_ns`；不得標為 source frame age |
| 丟棄與失敗 | 來源跳號、應用覆蓋、消費者跳過、invalid frame、斷線分開計數，不直接相加 |

串流等待下一影格的時間不稱為「擷取處理耗時」。沒有可信來源時鐘時，不用 Unix 時間相減推導單調延遲，也不把有固定 FPS 當作畫面新鮮的證明。

本次不直接修改撞線預測器的時間基準；文件明確列出未補償的來源延遲是後續預測整合的前置問題。

## D. CLI、Session 與基準工具

- 在現有 `capture-bench`／`start-session` 加入明確 backend 選項，例如 `--capture-backend adb-png|emulator-grpc`；依能力結果再決定預設值，失敗時不靜默切成慢速 ADB。
- 端點、認證來源、解析度等由參數或設定提供，記錄有效非機密設定。保留裝置歧義檢查。
- 擴充串流 benchmark：duration、warmup、受控 consumer delay、日誌路徑、格式及取消 timeout。每次量測使用獨立 run ID／檔名，避免覆寫歷史。
- 更新現有 ADB 統計的命名，並另量測像素就緒間隔；不要改寫歷史數字為新定義。
- 建立可重現的動態 pixels fixture：非對稱色塊、角落標記、逐畫面可見計數／移動圖案。优先使用在 AVD 內執行的簡單測試頁／測試應用；其真值只供離線驗證，不接入遊戲決策。
- 動態 fixture 的來源更新率必須實測或標示未知；不能以 AVD 60 Hz 設定當作實際 60 FPS。若需 Android 時鐘或外部觸發，只報告時鐘可比的區段；主機送出觸發命令不等於畫面出現真值。

## E. 驗證矩陣與研究目標

### 自動化測試

用可控假串流／假 clock 測試：通道與旋轉轉換、空影格與錯誤長度、來源跳號及重連重置、慢 consumer 只取得最新 frame、來源中斷、阻塞讀取取消、重複停止與清理。檢查時間先後與統計定義；測試不可依賴本機 AVD 永遠存在。執行既有 `python -m unittest discover -s tests -v`，依賴可選時驗證未安裝 gRPC 的原有功能仍正常。

### AVD 實測

1. 暖機 10 秒，每批持續量測至少 60 秒，正常條件至少 3 批。記錄實際样本數、fixture 更新率、主機負載與原始日誌；必要時延長以增加尾端樣本。
2. 至少一組受控負載，以及 consumer 延遲 50／100 ms 的測試；慢 consumer 期間接收應繼續，恢復後不逐張回放舊 frame。
3. 對接收端／轉換端刻意停頓另做測試，不能只測最後一層 `LatestFrame`。觀察上游是否積壓；用可見計數、來源序號和可可靠取得的時間證據，避免只憑穩定 RSS 宣稱沒有延遲累積。
4. 測試旋轉、尺寸變更、inactive／靜態画面、停止與重新連線。若涉及使用者正在操作的 AVD，先保存設定；避免清除資料、重新安裝遊戲或中斷登入流程。需要重啟才能驗證時先完成其餘測試並在新任務提出具體必要操作。
5. 記錄 p50／p95／p99／最大值、各類失敗與丟棄數、到達間隔 p95−p5、CPU、RSS，GPU 使用可取得則記錄，否則標示缺測。本次的間隔離散程度不能冒稱觸控排程 jitter。

研究起始目標（可設定，不是遊戲合格線）：在確認有約 60 FPS 動態來源的 1280×720 配置，爭取收到至少 55 個不同來源影格／秒，到達間隔 p95 ≤ 33.4 ms、p99 ≤ 50 ms，慢 consumer 復原後 100 ms 內恢復當前畫面，且無持續增長的積壓／記憶體。無法確認來源 FPS 或當前畫面真值時，對應項目標示「未能驗證」，不得假判通過。未達目標先拆解來源、傳輸、轉換、日誌與主機負載，再決定下一個候選。

### 像素正確性與資料保存

保存少量 fixture 診斷截圖與非機密執行設定。原始量測可放 gitignore 的 `measurements/<run-id>/`，但應交付位置、檔案雜湊及足以重現的命令；精簡摘要／設定寫入版本控制。報告清楚區分歷史紀錄、本次實测、假串流測試與尚未驗證項目。

## F. 替代路徑與停止條件

1. **一般 gRPC RGB／RGBA**：先完成上述實作及量測。若可用並達研究目標，本次不必為形式完整而實作所有候選。
2. **MMAP**：只有證據顯示 gRPC payload／複製成本為主因才評估。先核對本機平台支援及共享記憶體生命週期；必須有讀寫一致性／撕裂驗證，不能當成換一個像素格式。無法證明一致性就不採用。
3. **Windows Graphics Capture**：若 gRPC 不可用或不適合，評估獨立 Emulator 視窗、frame pool 消費、GPU→CPU readback、縮放與遮擋／最小化。`SystemRelativeTime` 只代表主機合成器 frame 時刻，不等同 Android 來源時間。
4. **scrcpy**：WGC 不適合時比較連續視訊；停用控制及無關音訊，固定版本、codec、解析度與 buffering 設定。接收／解碼持續進行，只向下游發布最新已解碼圖；不可任意丟棄相依壓縮封包導致解碼損壞。

比較候選使用相同動態 fixture 與設定。任何候選若有像素錯誤、無法取消或持續積壓，不因高平均 FPS 而入選。所有候選均不合適時，交付量測與明確阻礙，不宣稱擷取問題已解決，也不轉入觸控或遊戲辨識。

## 交付與完成定義

- 可選的持續擷取後端及可重現 CLI，在真實 AVD 上有像素正確性、停止／故障、慢下游與時間分布的證據。
- 自動化測試通過；有完整的執行環境、暖機、樣本數、原始紀錄位置和可重現命令。
- `README.md` 更新安装／執行方法；`ARCHITECTURE.md` 更新資料契約、來源生命週期與有依據的候選結論；`ROADMAP.md`、`MEASUREMENTS.md` 與研究文件同步修正狀態、統計命名和 MMAP 描述。
- 開發完成報告列出已達／未達的研究目標、剩餘風險與下一階段限制。只有取得足夠 AVD 實測證據才標示本階段已驗證；外部環境受阻時照實交付已完成的實作與待驗證項目。

## 官方參考

- Emulator proto：https://github.com/google/android-emulator-webrtc/blob/master/proto/emulator_controller.proto（實作以安裝版本為準）
- WGC 時間：https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframe.systemrelativetime
- scrcpy 串流：https://github.com/Genymobile/scrcpy/blob/master/doc/develop.md
