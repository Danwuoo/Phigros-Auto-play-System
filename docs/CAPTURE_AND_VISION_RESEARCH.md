# 畫面擷取與視覺決策技術研究

2026-09-24 更新：新增獨立 Python spawn + gRPC payload 作冷開發可用的隔離方案；MMAP 已有 file URI 映射與 loopback 診斷實作，但仍有上游 tearing／metadata 錯配風險，因此只准 opt-in benchmark。離線 loopback 的數據僅衡量本機 harness/IPC，不可替代真實 Emulator 更新率、來源延遲或性能選型。需要真實同條件比較，並先證明 MMAP 生產端同步，才可更改候選結論。詳見 [低延遲計畫](CAPTURE_LOW_LATENCY_PLAN.md)。

研究日期：2026-09-23。本文區分已量測的基線、可用的技術介面與尚待實測的選型。執行時的遊戲決策仍只能來自即時畫面；記錄資料只能用於離線分析與訓練通用視覺模型，不能作為譜面或固定按鍵序列回放。

## 目前已量測的瓶頸

`phigros` AVD：Android 16／API 36.1、4 KB page、x86_64、4 vCPU、6 GB RAM、host GPU；遊戲橫向畫面 1280×720、顯示 60 Hz。Windows 11 build 26200、Python 3.14.7。歷史記錄的 `adb exec-out screencap -p` + 專案純 Python PNG 解碼 30 張樣本（3 張暖機、0 失敗）：ADB 擷取呼叫 p50／p95／p99 = 681／901／951 ms；PNG 解碼 = 2,499／2,799／2,902 ms；相鄰 payload 收齊間隔（29 個樣本）= 3,156／3,556／3,688 ms，最大 3,733 ms。原始 `measurements/adb_capture_phigros_20260923.jsonl` 目前缺失，因此這些數字僅引用歷史摘要，不能重算或稱為像素就緒間隔。這個路徑不能用於即時遊玩。

另以 5 次未壓縮 `adb exec-out screencap` 做診斷，單次呼叫約 588–1,017 ms；即使省掉 PNG 解碼，逐張啟動 ADB 命令仍是明顯瓶頸。這 5 次未驗證像素或畫面年齡，不能視為正式後端基準。

## 擷取候選與順序

| 優先順序 | 候選 | 研究理由 | 主要驗證風險 |
| --- | --- | --- | --- |
| 1 | Android Emulator gRPC `streamScreenshot`，先試 `RGB888`／`RGBA8888` | 模擬器直接推送新影格，協定支援原始像素與序號；本機 SDK proto 與權杖 discovery 已確認可用。 | 先前乾淨提交的原生 1280×720 Fixture 三批僅 35.4–35.9 個不同畫面／秒、到達 p95 49.0–51.2 ms；後續約 59 Hz 繪製條件的乾淨提交三批為 58.5–59.3 個不同畫面／秒、p95 29.6–32.5 ms、p99 38.7–40.7 ms，該條件達研究門檻。條件變動原因未證實；500 ms 接收停頓仍會暫交付舊圖，保護在受控測試中可丟棄。絕對來源年齡與遊戲判定窗適用性仍未知。 |
| 2 | Windows Graphics Capture 擷取獨立模擬器視窗 | Windows 可對單一視窗以 Direct3D frame pool 收取畫面；影格附 QPC 時間，適合與主機計時整合。 | 目前模擬器嵌在 Android Studio 且以 `-qt-hide-window` 啟動；需改為獨立視窗並校正視窗客戶區、縮放、黑邊及觸控座標。 |
| 3 | scrcpy H.264 持續串流，停用控制 | 已有成熟的 Android 畫面串流路徑；可控制畫質與最大幀率，並可取得其伺服器串流協定。 | 編碼、傳輸、解碼與可能的緩衝都會增加延遲；此 AVD 上的性能尚未量測。 |
| 基線 | 每張 `adb screencap` | 已有可重現的正確性與計時基線。 | 本機量測遠慢於 60 Hz，保留作診斷，不進入遊玩路徑。 |

模擬器協定列明串流在新影格產生時推送，`ImageFormat` 支援 PNG、RGBA8888、RGB888；`MMAP` 是 `ImageTransport` 的傳輸通道，不是像素格式，且 proto 警告共享區域可能撕裂。只有一般 gRPC payload／複製成本被證明是主要瓶頸，且有一致性驗證方法時才考慮 MMAP。`Image.seq` 可用於辨識跳號。`Image.timestampUs` 是模擬器估計的 Unix 時間，不能直接與主機 `time.monotonic_ns()` 相減。若要量測來源影格年齡，須另建立可靠的時鐘對齊或受控可見事件測試。本機 proto 的像素行方向註解與實際 Emulator 37.1.11 的 RGB888／RGBA8888 輸出相反；已用 ADB 同場景截圖對照確認 top-down。實作以安裝版 proto 為準，雜湊見 [量測紀錄](MEASUREMENTS.md)。[Google 模擬器協定](https://github.com/google/android-emulator-webrtc/blob/master/proto/emulator_controller.proto)、[gRPC 安全與設定](https://android.googlesource.com/platform/external/qemu/+/686efa16baf59d776cadc3f975d12570fe44bbb9/android/android-grpc/docs/README.md)

第二輪修正把連線、暖機、正式量測分開；歷史 CPU 與負載窗口有偏差，不能用舊數據選型。後續修正首次正式 consumer skip 的暖機邊界，以及事件式健康探測期間新影格到達卻誤報失敗的競態。事件式串流沒有新影格時，獨立畫面探測只能確認同一靜態 pixels 與傳輸可回應，不能證明串流一定會在未來送出新圖，因此新鮮度資格必須撤銷。相對來源時戳差值保護遇到時鐘不連續會停止而不重設錨點；它仍不提供可信的絕對影格年齡。先前約 35 Hz 與後續約 59 Hz 的原生 Fixture 條件須分開比較；條件變動原因未證實。重測與診斷結果見 [量測紀錄](MEASUREMENTS.md)。

Windows Graphics Capture 的 `FrameArrived` 提供持續影格，`SystemRelativeTime` 是合成器產生影格時的 QPC 時間；應在同一主機 QPC 時域內計時，並驗證視窗擷取的內容與觸控座標一致。[Microsoft 畫面擷取文件](https://learn.microsoft.com/en-us/windows/uwp/audio-video-camera/screen-capture)、[單視窗擷取介面](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow)

scrcpy 官方文件指出它以連續視訊串流顯示畫面，預設 H.264，可設定 `--max-fps`，也能以 `--no-control` 停止輸入控制；其公布的性能數字屬跨裝置描述，不能當作本機 AVD 的驗收結果。[scrcpy 視訊文件](https://github.com/Genymobile/scrcpy/blob/master/doc/video.md)、[唯讀控制選項](https://github.com/Genymobile/scrcpy/blob/master/doc/control.md)、[串流協定](https://github.com/Genymobile/scrcpy/blob/master/doc/develop.md)

### 同條件基準程序

1. 在同一 AVD、解析度、遊戲／簡單動態測試畫面、主機負載下比較候選；記錄映像版本、GPU、方向與實際縮放。靜態選單不足以測連續影格輸出。
2. 暖機後收集足以觀察尾端的連續影格，保留原始資料與樣本數；量測到達間隔、接收／解碼耗時、p50／p95／p99／最大值、跳號或丟幀、CPU／GPU 與記憶體。串流應只把最新 frame 發布給下游，不能累積待處理佇列。
3. 以可見且可控制的測試事件量測「事件出現 → 影格觀察 → 觸控送出 → 效果再次被看見」；沒有可比時鐘時只報可可靠取得的主機單調時間差，不臆測 source frame age。
4. 驗證橫直切換、視窗縮放、黑邊、連線中斷、最小化或遮擋、遊戲載入與高動態畫面的像素完整性。只有在功能和尾端延遲都通過同條件測試後，才正式選定擷取後端。

## 主程式：混合式視覺與幾何預測

建議延續目前 Python 的模組契約作協調與測試；耗時的擷取、像素處理或觸控段落在量測證明必要時改為原生模組。遊戲推論分成四層，不能把整局交給單一模型直接輸出按鍵：

1. **畫面狀態門控**：只從當前畫面及短期穩定度判定選曲、載入、遊玩、暫停、結算與未知。未知或畫面逾時時不送出遊玩觸控。
2. **視覺觀測**：先以 OpenCV 的顏色、邊緣、線段與局部形狀建立可解釋基線；判定線可試 Hough 線段與幾何一致性，跨影格運動可試光流。若特效、遮擋與主題變化使手工特徵失效，再以輕量物件偵測／分割模型輸出 Note 類別、位置與判定線遮罩及信心。[OpenCV Hough 線段](https://docs.opencv.org/4.x/d9/db0/tutorial_hough_lines.html)、[OpenCV 光流](https://docs.opencv.org/4.x/d4/dee/tutorial_optical_flow.html)
3. **短期追蹤與預測**：把 Note 位置轉入隨判定線移動／旋轉的局部座標，維護物件 ID、相對位置與速度、判定線姿態、不確定度；可用 Kalman／其他狀態估計器。依最近幾張有效畫面估算撞線時間，附來源 frame 與可解釋的軌跡，再交給既有排程器。[OpenCV KalmanFilter](https://docs.opencv.org/4.13.0/dd/d6a/classcv_1_1KalmanFilter.html)
4. **有限適應與記憶**：保留短期追蹤狀態、當局畫面推得的速度／旋轉與延遲估計，以便應付譜面變化。切歌、暫停或失去畫面時重置相應狀態；不保存歌曲對應的 Note 時間序列，也不由過去日誌、歌曲名稱或譜面識別查表決定下一次觸控。離線資料可用來訓練一般化的視覺權重，正式運行仍必須由即時 pixels 觸發每次決策。

### 何時加入深度學習

先建立人工標註的畫面驗證集，依歌曲與視覺主題切分訓練／驗證資料，避免相鄰影格洩漏造成虛高分數。對比傳統視覺與小型偵測／分割網路的 Note 漏辨、誤辨、類型混淆、判定線誤差，以及推論 p50／p95／p99 和整條閉環延遲；只有改善整體可靠度且沒有破壞時間預算，才加入模型。可用 PyTorch 離線訓練、匯出 ONNX，執行時比較 ONNX Runtime CPU 與 NVIDIA CUDA provider；本機有 RTX 3050 Laptop GPU，但 GPU 路徑仍需實測。[PyTorch ONNX 匯出](https://docs.pytorch.org/tutorials/beginner/onnx/export_simple_model_to_onnx_tutorial.html)、[ONNX Runtime 執行後端](https://onnxruntime.ai/docs/execution-providers/)、[CUDA 後端需求](https://onnxruntime.ai/docs/execution-providers/CUDA-ExecutionProvider.html)

首版不以端到端強化學習或長期譜面記憶作為觸控決策。它們無法替代尚未完成的擷取、獨立多點觸控和簡單目標閉環驗收；是否探索學習式策略應待可重現基線與足夠跨歌曲資料後再決定。
