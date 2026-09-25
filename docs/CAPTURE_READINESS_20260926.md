# 五擷取路徑就緒與量測矩陣（2026-09-26）

本文件隨開發及實測更新。`development_smoke` 僅證明取圖與基本正確性；只有同一凍結版本的 `formal_campaign` 可供候選比較。正常來源研究範圍為 40–59 Hz，目標設定和實際可見更新率分別報告。性能數值門檻尚未決定，`performance_pass=null`。

目前 gRPC 控制／優化及 scrcpy 已完成開發短測，仍待正式集中測試；WGC／DXGI 處於 `waiting-user-decision`，需先取得可見且足夠大小的 Emulator 視窗，再做 Fixture 裁切 QA。MMAP 維持 `diagnostic-only`。目前沒有路徑可標為 `implemented-and-tested`，也沒有主用／備用選型。

| 路徑 | 實作與輸出 | 就緒證據 | 限制／門控 |
| --- | --- | --- | --- |
| gRPC payload 控制 | 逐列 RGB888 複製，再將來源直向畫面逆時針旋轉成 CPU RGB24 | Release 成功；CTest 25/25；[Fixture 5 秒短測](../measurements/five_20260926_smoke_grpc_control_02/summary.json) 241/241 消費影格可解碼、方向正確 | 只供同 campaign 的配對基線；protobuf 內部耗時無法從 API 單獨觀察 |
| gRPC payload 優化 | 一般 top-down RGB888 單次 `memcpy`；本機直向來源則融合旋轉直接寫入橫向 RGB24 | Release 成功；CTest 25/25；[Fixture 5 秒短測](../measurements/five_20260926_smoke_grpc_fast_02/summary.json) 239/239 消費影格可解碼、方向正確 | 仍保有自有 RGB buffer；同版控制配置用 `--grpc-copy-mode legacy-rows` |
| WGC | 顯式 HWND、free-threaded frame pool、D3D11 staging readback、BGRA→CPU RGB24 | Release 成功；CTest 25/25；視窗取圖與裁切 QA 待完成 | 視窗需可見且未最小化；尺寸變更撤銷 profile，重選裁切；不接 Session |
| DXGI Desktop Duplication | 顯式 monitor／HWND、輸出座標裁切、Acquire／Release、access-lost duplication 重建 | Release 成功；CTest 25/25；前景／遮擋／裁切實測待完成 | 僅適用前景可見且無上層視窗相交的指定 monitor；跨 monitor／旋轉拒絕；不接 Session |
| scrcpy v4.1 H.264 | 官方 server hash 固定、Android 軟體編碼、FFmpeg 9.0.2 軟體解碼、CPU RGB24 | Release 成功；CTest 25/25；[Fixture 5 秒短測](../measurements/five_20260926_smoke_scrcpy_03/summary.json) 232/232 消費影格可解碼、方向正確 | audio/control 關閉；封包 4 MiB 上限；積壓超限最多重連一次；不接 Session |
| Emulator MMAP | 有界映射、通知後 snapshot、四區 tearing 診斷 | [版本與契約證據](MMAP_FEASIBILITY_20260926.md)；[5 秒診斷](../measurements/five_20260926_smoke_mmap_01/summary.json) 239/239 消費影格可解碼 | **diagnostic-only**；proto 明示可能 tearing，無已證實 writer ownership／fence；像素通過不證明一致性 |

## 已核對的測試環境

- AVD `phigros`：設定 5 vCPU／8192 MiB／host GPU／720×1280 直向；Fixture 實際橫向輸出 1280×720。guest `/proc/cpuinfo` 有 5 個 processor，`MemTotal` 為 8,130,828 kB。
- 主機顯示控制器為 NVIDIA GeForce RTX 3050 6GB Laptop GPU（driver 32.0.15.9159）與 Intel Arc Graphics（driver 31.0.101.5125）。實際 DXGI adapter／monitor、實體像素、刷新率和 DPI 由每批 manifest 的 host probe 固定記錄；不以這裡的控制器清單替代選定輸出證據。
- Emulator 37.1.11.0 build 15917651，序號 `emulator-5554`。現時顯示於已最小化的 Android Studio 內嵌視窗，WGC／DXGI 需要有效的視窗畫面與相同 1280×720 可見區，啟動形式變更仍待具體使用者決定。
- 已安裝 Native Fixture APK SHA-256 `1059d59875c65840db07688ce091cc30a4c2053b7f393378f7905dbe5c375e6f`，與 `measurements/fixture_cpp_v2/pas-capture-fixture-v2.apk` 相同。現有 AVD 上以 ADB PNG 確認四角識別與橫向畫面；此為開發檢查，不是候選量測。
- 官方 scrcpy v4.1 tag commit `49c9501fb26f456bbf4a341dd68879f670c67452`，server SHA-256 `deacb991ed2509715160ffdc7907e47b4160eb30d1566217e9047fd5b8850cae`。在本 AVD 以原版 server 的 `list_encoders=true` 查得 H.264 `c2.android.avc.encoder` 為軟體編碼器；候選顯式使用該名稱及固定 8 Mb/s、最高 60 fps。
- 本機 gRPC API 在 Fixture 橫向時回傳來源 720×1280、rotation metadata 0；加 `--grpc-rotate-ccw --source-rotation 0` 後，manifest 保留原始尺寸與 -90° 正規化，對外輸出 1280×720。先前直接請求 1280×720 的開發嘗試得到 405×720，是 API 等比縮圖，不可作候選比較。

## 集中日程與證據

`pas capture-five-campaign` 先寫 `campaign-plan.json`，再依固定輪換順序串行執行每個可行候選 40／48／57 Hz 各三批，每批 10 秒暖機加 60 秒量測。每批前的 Fixture READY 條件為 10 秒內從四區像素讀到兩個不同 counter。日程還包含慢消費、500 ms 接收暫停、CPU／256 MiB 記憶體負載、10 分鐘穩定性與 MMAP 診斷。所有批次寫出 raw JSONL、manifest、診斷 PNG 與 SHA-256；分析可從 raw 重新計算並產生 `comparison.md`。

正式開始前需凍結 source revision／diff hash、Release EXE、FFmpeg 版本、server、APK、顯示形式與裁切座標。若某候選不符合安全或幾何條件，應記為 `unsupported-with-evidence` 或 `failed-with-reproduction`，不要以缺失批次冒充成功。完整故障情境仍需在候選基本正確性通過後依 [計畫](CAPTURE_FIVE_BACKENDS_PLAN_20260926.md) 執行；未實測項目不得稱為通過。
