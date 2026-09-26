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

## 第二版完整日程及暫停後舊圖修正

`five_formal_20260926_r2` 凍結 `6fa6dbde65e488986c8ca969994426649e797aa6`，76 批全部執行：69 成功、7 失敗。DXGI 兩批 48 Hz 因前景被其他視窗取代而拒絕；scrcpy 四批正常及十分鐘長測因相對 PTS 積壓保護終止。gRPC 控制／優化、WGC、DXGI 各完成十分鐘；該版所有原始資料與生成比較表保留，不能把失敗刪去再聲稱整體通過。舊版 EXE／DLL 另存 `measurements/frozen_6fa6dbde_release/`。

暫停後離線稽核發現 WGC 第一張發布圖相對暫停前時域落後增加 500.7144 ms；未啟用相對保護的 gRPC 控制／優化第一張分別增加 494.3589／493.7612 ms。DXGI 的第一張增加為 -0.2882 ms。這是主機／來源**增量**的差，沒有 Android 絕對來源年齡校準。第二版的幾何有效不代表恢復行為符合最新圖契約。

新 WGC 在容量二的 pool 最多取兩張、明確 Close 被替換與已用 frame，先檢查 SystemRelativeTime 增量再 readback／發布，超過 250 ms 相對積壓增加即丟棄，並記錄 `pool_older_frame_drop`／`relative_stale_drop`。來源或主機時戳倒退終止來源。250 ms 是有界積壓保護配置，不是已確認的性能通過門檻。新的五路徑日程也固定 gRPC 兩配置 `max_relative_lag_ms=250`；原控制與優化只有複製策略不同。WGC 單獨 bench 預設相同保護，gRPC 單獨 bench 需顯式啟用。

新 Fixture 保留 native-v2 四區格式，增加獨立 binary 移動 X 真值、低對比區與紅／藍邊緣，以及只限 Fixture 的 `debug.pas.fixture_freeze`。每 100 ms 查一次 property；凍結不 swap、不增加 counter，恢復繼續繪圖。新 APK 位於 `measurements/fixture_cpp_position_static/pas-capture-fixture-v2.apk`，SHA-256 `195b6a0673c5d2182fe9b9face60cbfae8457dcf4c46bb82a873ad99d35fe964`。原 APK 不覆寫，重測所有候選，避免混合 Fixture 成本與品質版本。ADB shell 沒有 su，原 package 不可 run-as，未用未經證實的外部 SIGSTOP 冒充靜止來源。

新版集中日程為 96 批：45 正常、45 壓力（每配置新增三秒靜止、獨立 GPU 負載、preview off/on）、五次十分鐘及 MMAP 診斷。GPU 負載是同程序專用 thread 的 D3D11 512×512 float4 compute、每 pixel 128 iterations、最多一個 dispatch 在途、完成期限二秒；adapter LUID、shader 與 dispatch 數記錄在 raw。PDH GPU Engine utilization 原始 instance、percent 與 QPC brackets 同時採樣；每 engine 分別歸一，不能把所有 engine 加總當整卡百分比。preview 使用不啟用焦點的明示位置 (1460,125)、client 400×225，RGB endpoint 仍為 1280×720，本位置只適用本機桌面 profile。

暫停／靜止恢復有獨立 ADB pixels reference，包含呼叫前後 QPC bracket；分析尋找第一張消費 counter 不早於該 reference 的圖。該指標包含診斷呼叫等待，不稱為精確的來源年齡或瞬時恢復下限。靜止的兩個 reference counter 必須相同，恢復 reference 必須更新。

開發回歸：Release／CTest **27/27**；WGC 暫停短測丟棄一張、gRPC 丟棄三張，所有消費圖可解碼；WGC／gRPC 靜止測試的兩個獨立 reference 相同、恢復更新；WGC GPU＋preview 開發測試完成 570 dispatch、9 個 GPU 採樣，280/280 消費圖可解碼。DXGI preview 開發短測 133/133，移動左右誤差 0；scrcpy 短測 124/124，左邊緣誤差 0–1 px、右邊緣 0 px。上述短測不作性能排名。接續新 revision 完整重測及依選型規則的主用／備用三十分鐘驗證。
