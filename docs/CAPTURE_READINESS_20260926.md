# 五擷取路徑就緒與量測矩陣（2026-09-26）

本文件隨開發及實測更新。`development_smoke` 僅證明取圖與基本正確性；只有同一凍結版本的 `formal_campaign` 可供候選比較。正常來源研究範圍為 40–59 Hz，目標設定和實際可見更新率分別報告。性能數值門檻尚未決定，`performance_pass=null`。

使用者已授權自行關閉及重啟 AVD。gRPC 控制／優化、WGC、DXGI 與 scrcpy 已通過開發取圖檢查，待同版正式集中測試；MMAP 維持 `diagnostic-only`。目前尚無正式選型結果。

| 路徑 | 實作與輸出 | 就緒證據 | 限制／門控 |
| --- | --- | --- | --- |
| gRPC payload 控制 | 逐列 RGB888 複製為 CPU RGB24；目前來源直接為橫向，不需旋轉 | Release 成功；CTest 25/25；[獨立視窗 3 秒短測](../measurements/standalone_20260926_grpc_control_01/summary.json) 143/143 消費影格可解碼、方向正確 | 只供同 campaign 的配對基線；protobuf 內部耗時無法從 API 單獨觀察 |
| gRPC payload 優化 | top-down RGB888 單次 `memcpy` 為 CPU RGB24；其他 profile 可明示融合旋轉 | Release 成功；CTest 25/25；[獨立視窗 3 秒短測](../measurements/standalone_20260926_grpc_01/summary.json) 142/142 消費影格可解碼、方向正確 | 仍保有自有 RGB buffer；同版控制配置用 `--grpc-copy-mode legacy-rows` |
| WGC | 顯式 HWND、free-threaded frame pool、D3D11 staging readback、BGRA→CPU RGB24 | Release 成功；CTest 25/25；[5 秒短測](../measurements/standalone_20260926_wgc_01/summary.json) 204/204 消費影格可解碼，四角色差／細線位移 0 | top-level HWND，crop=(1,38)；child HWND 實測 E_INVALIDARG；尺寸／DPI 變更撤銷 profile；不接 Session |
| DXGI Desktop Duplication | 顯式 monitor／HWND、輸出座標裁切、Acquire／Release、access-lost duplication 重建 | Release 成功；CTest 25/25；[5 秒短測](../measurements/standalone_20260926_dxgi_03/summary.json) 235/235 消費影格可解碼，四角色差／細線位移 0；操作覆蓋層被拒絕 | client crop=(0,0)，monitor 0；擷取前後檢查前景、DWM 可見範圍、尺寸／DPI與受檢點；跨 monitor／旋轉拒絕；不接 Session |
| scrcpy v4.1 H.264 | 官方 server hash 固定、Android 軟體編碼、FFmpeg 9.0.2 軟體解碼、CPU RGB24 | Release 成功；CTest 25/25；[獨立視窗 5 秒短測](../measurements/standalone_20260926_scrcpy_01/summary.json) 221/221 消費影格可解碼、方向正確 | audio/control 關閉；封包 4 MiB 上限；積壓超限最多重連一次；不接 Session |
| Emulator MMAP | 有界映射、通知後 snapshot、四區 tearing 診斷 | [版本與契約證據](MMAP_FEASIBILITY_20260926.md)；[5 秒診斷](../measurements/five_20260926_smoke_mmap_01/summary.json) 239/239 消費影格可解碼 | **diagnostic-only**；proto 明示可能 tearing，無已證實 writer ownership／fence；像素通過不證明一致性 |

## 已核對的測試環境

- AVD `phigros`：設定 5 vCPU／8192 MiB／host GPU／720×1280 自然方向；Fixture 橫向輸出 1280×720。獨立視窗冷啟動後 guest 為 5 個 processor、`MemTotal` 8,130,816 kB（先前內嵌執行為 8,130,828 kB）。
- 主機顯示控制器為 NVIDIA GeForce RTX 3050 6GB Laptop GPU（driver 32.0.15.9159）與 Intel Arc Graphics（driver 31.0.101.5125）。實際 DXGI adapter／monitor、實體像素、刷新率和 DPI 由每批 manifest 的 host probe 固定記錄；不以這裡的控制器清單替代選定輸出證據。
- Emulator 37.1.11.0 build 15917651，序號 `emulator-5554`。已改成独立視窗，原資料分割區與已安裝應用保留；Quick Boot 首次出現系統 ANR，略過快照載入的冷啟動已恢復。正式環境啟動加 `-fixed-scale`；125% host DPI 下 `emulator-user.ini` 的 `window.scale=0.800000` 得到實體 1280×720 client，畫面位置 (125,163)。原視窗設定備份在 `measurements/standalone_20260926_emulator_user_before_scale.ini`。
- 已安裝 Native Fixture APK SHA-256 `1059d59875c65840db07688ce091cc30a4c2053b7f393378f7905dbe5c375e6f`，與 `measurements/fixture_cpp_v2/pas-capture-fixture-v2.apk` 相同。現有 AVD 上以 ADB PNG 確認四角識別與橫向畫面；此為開發檢查，不是候選量測。
- 官方 scrcpy v4.1 tag commit `49c9501fb26f456bbf4a341dd68879f670c67452`，server SHA-256 `deacb991ed2509715160ffdc7907e47b4160eb30d1566217e9047fd5b8850cae`。在本 AVD 以原版 server 的 `list_encoders=true` 查得 H.264 `c2.android.avc.encoder` 為軟體編碼器；候選顯式使用該名稱及固定 8 Mb/s、最高 60 fps。
- 先前內嵌視窗的 gRPC API 回傳 720×1280、rotation 0，使用明示 -90° 正規化；該資料保留為開發證據。獨立視窗旋轉為橫向後，[新 gRPC 短測](../measurements/standalone_20260926_grpc_01/summary.json) 直接回傳 1280×720、rotation 1（142/142 消費影格可解碼），正式 campaign 使用 `--source-rotation 1`，不啟用旋轉轉換。

## 集中日程與證據

`pas capture-five-campaign` 先寫 `campaign-plan.json`，再依固定輪換順序串行執行每個可行候選 40／48／57 Hz 各三批，每批 10 秒暖機加 60 秒量測。每批前的 Fixture READY 條件為 10 秒內從四區像素讀到兩個不同 counter。日程還包含慢消費、500 ms 接收暫停、CPU／256 MiB 記憶體負載、10 分鐘穩定性與 MMAP 診斷。所有批次寫出 raw JSONL、manifest、診斷 PNG 與 SHA-256；分析可從 raw 重新計算並產生 `comparison.md`。

正式開始前需凍結 source revision／diff hash、Release EXE、FFmpeg 版本、server、APK、顯示形式與裁切座標。若某候選不符合安全或幾何條件，應記為 `unsupported-with-evidence` 或 `failed-with-reproduction`，不要以缺失批次冒充成功。完整故障情境仍需在候選基本正確性通過後依 [計畫](CAPTURE_FIVE_BACKENDS_PLAN_20260926.md) 執行；未實測項目不得稱為通過。

首輪開始前固定補測規則：至多一輪正常場景配對補測，僅在環境／幾何門控失敗、像素品質異常，或同目標場景候選間可見來源率差超過 10% 時觸發。保留首輪全數原始資料與觸發原因；配置或程式修正另記 revision，重跑所有受影響的候選配對，不挑選最佳批次替換原資料。

## 首輪停止故障與修正

`measurements/five_formal_20260926_r1` 固定 `3f5671e` 後完成前兩批 gRPC；第一批 WGC 的 raw 已到 `STOPPING`，關閉時程序崩潰，未生成該批 summary，後續 73 批未執行。Windows Application Error 1000／WER 1001 記錄 `GraphicsCapture.dll_unloaded`、`c0000005`，Report ID `2e9b6647-aecf-4b94-a70d-cb8dacf70286`；原始 WER 與退出索引保留在該 campaign。首輪不得作完整候選排名。

修正以共享回呼狀態及 mutex 設定 closing，排空執行中回呼，讓撤銷後的已排定回呼先檢查 closing 才能存取 stack；關閉 session／pool 並平衡 apartment。另從 System32 載入並以 [GetModuleHandleExW PIN](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexw) 將單一系統 GraphicsCapture.dll 保留至程序退出，避免這個 Windows build 的內部 worker 在模組卸載後執行。這不保留擷取 session／texture／Frame，模組常駐成本會納入 WGC 使用過的程序資源。相同卸載問題的 [原始重現報告](https://github.com/robmikh/Win32CaptureSample/issues/99) 僅為背景，實機依據為本機 WER。

此停止故障觸發已預定的整組重測；先完成同程序反覆啟停驗證，再以新 source revision 重跑完整五路徑日程。原資料保留。

修正後 Release／CTest 25/25 通過；[同程序 WGC 啟停驗證](../measurements/wgc_stop_start_20260926_01/campaign-results.json) 完成 20 次（每次暖機 1 秒、量測 2 秒），再完成暖機 1 秒＋量測 60 秒，21/21 成功。這是停止故障的開發回歸，不能替代正式三批或十分鐘穩定性。
