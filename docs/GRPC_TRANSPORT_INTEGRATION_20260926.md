# gRPC 接收區塊正式接入

> 2026-09-28 清理後註記：本文保留當時版本與證據界線，舊授權／待辦不作現行指令。後續方向見[跨曲學習研究](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)；原始資料與 binary 的現存／已刪範圍見[清理紀錄](CLEANUP_AUDIT_20260928.md)。歷史 JSON 的 hash／pass 不代表被刪的 raw 仍可重算。

狀態：revision 2 正式接入完成；Release 34／34 回歸通過，18 批傳輸短測、正式擷取、接收暫停與 observe 驗證完成。擷取 channel 預設 256 KiB，保留 8／64 KiB。此交付不等同遊戲閉環或長時間性能驗收。

使用者在 短測研究（GRPC_TRANSPORT_RESEARCH_20260926.md 已清理，歷史見 Git baf3d4f） 後授權完成開發。本輪將 256 KiB 畫面 channel 接收區塊接入正常建置，保留 8／64 KiB 的配置與比較能力。選擇依據是前輪模擬器 client CPU 改善較穩定；p99 並未一致改善，因此本次不宣稱端到端或尾端延遲已通過數值門檻。

## 交付內容

- `vcpkg-configuration.json` 自動選用 `third_party/vcpkg-overlay/grpc`。以原 baseline port 為基礎，保留原七個 patches，再加 Windows endpoint／Connect 修補。版本為 gRPC 1.81.1、port revision 2，source archive 雜湊由原 port 鎖定。
- dependency 整包重建，所有使用 WindowsEndpoint layout 的 object 一致；不採前輪單一 object 覆蓋方式。舊實驗 CMake／campaign runner 從開發入口退役，其原檔與二進位仍保留在前輪 frozen evidence。
- 私有 channel argument 僅接受 65,536／262,144 bytes；其他值及未指定採原 8,192 bytes。對外 profile／CLI 嚴格驗證，只接受 8／64／256 KiB。Capture 預設 256，Touch 不設定此 argument。
- `Connect` 在呼叫當下讀取整數並存入 `ConnectionState`；非同步完成後再傳入 endpoint。既有 listener 保持預設 8 KiB；endpoint 不讀取 listener 保存的 `EndpointConfig` 參照。
- 安裝 header 與 library revision symbol 雙重約束。連到 stock library 會因缺少 symbol 而失敗；版本不符也明確拒絕，不會報告未生效的優化。
- manifest 分別保存 `package_version=1.81.1` 與 gRPC Core 回報的 `client_version`；後者是 Core 版本字串，不應當成 C++ 套件版本比較。
- `capture-bench`、兩種 campaign 及 observe 都保存實際傳輸配置。舊 profile 預設升至 256 KiB；需要控制組時明示 8 KiB。新 campaign 分析驗證 plan／manifest 的 transport 設定一致，歷史沒有欄位的證據沿用原契約。
- 持續只保留最新 frame，不新增 image queue。接收 slice 原有的 spare-slice 捐回邏輯保留；每次 Read 準備交給 WSARecv 的可用長度合計小於兩個 chunk。這是當次接收範圍，並非 retained allocation 或整個程序的記憶體上限；gRPC 訊息、HTTP/2 buffer 與仍被引用的 backing allocation 另計。
- 預設不存 PNG 的政策保留。vcpkg 建置／打包暫存由既有 clean 選項於成功安裝後自動清除；正常 presets 加入已核對的短路徑，避免重新建置 gRPC 時碰到 Windows 長路徑問題。

## 使用方式

```json
"capture": {
  "kind": "emulator-grpc",
  "execution": "thread",
  "transport": "payload",
  "image_format": "rgb888",
  "row_order": "top-down",
  "width": 1280,
  "height": 720,
  "source_rotation": 1,
  "grpc_read_chunk_kib": 256
}
```

`pas capture-bench --grpc-read-chunk-kib 8 ...` 可取得控制組，64／256 為另兩個明示設定。`pas_grpc_latency --read-size-matrix --duration-s 4 --output-dir <新目錄>` 在同一個正式 dependency 上按平衡順序比較三個大小；不再依賴 research source 目錄或多套修改過的 executable。Loopback test server 保留 upstream 8 KiB，只有 client 依 case 設定；Emulator 服務端接收區塊未知，manifest 記為 null。

首次建置需要重編 patched gRPC（Debug 與 Release），時間比應用增量建置長；後續由 vcpkg ABI cache 重用。短路徑 `C:/pas-bld-9408`／`C:/pas-pkg-9408` 的 junction 指向本 workspace 的 `out/vcpkg_installed/vcpkg/blds`／`pkgs`，新 checkout 須依 README 調整本機路徑。

## 驗證紀錄

第一版的 34 個 Release 回歸有 6 個 gRPC 通訊案例在接收第一張圖前崩潰，沒有進入性能量測。`crash-stack.log` 定位到 endpoint constructor 的 `EndpointConfig::GetInt`；上游 client Connect 原先會忽略設定並另建空 config，listener 則保存參照，不能在非同步 accept 時新增讀取。第二版改成在同步 Connect 呼叫中複製所需值，不保留設定物件。此失敗與修正保留供追溯，第一版不算可用版本。

第一輪依賴建置約 57 分鐘，主要為 Debug／Release 冷編譯。第二輪在確認可用 RAM 約 22 GiB 後，僅依賴重建採 12 個編譯工作，約 37 分鐘，另花 4.8 分鐘保存 ABI cache。正式性能測試均在建置與快取作業結束後執行；每批性能量測仍僅 1 秒暖機＋4 秒正式窗。

本輪證據目錄：`measurements/grpc_production_20260926/`，事前設定見 `validation-plan.json`。以下均為正式 revision 2 新量測，不沿用前輪 prototype 的結果。

### 功能與建置

- `regression-tests-v2.log`：34／34 通過，2.40 秒。含各區塊大小的 1280×720 RGB 全 payload 檢查、同 channel 小 RPC、慢 stream 取消後再呼叫，以及未指定參數的小 channel。
- `endpoint-trace-v2.log`：實際 endpoint 的 8192／65536／262144 bytes 分別出現 10／1／1 次。trace 僅在功能測試開啟，不混入性能量測。
- config 接受 8／64／256，拒絕錯誤型別與其他大小；CLI 128 回傳 105，未建立輸出目錄。campaign 的 plan／manifest transport 不符或缺失會拒絕，歷史 plan 保留相容。
- 正常 Release link flags 已還原為 `/INCREMENTAL:NO`，`VCPKG_MANIFEST_INSTALL=ON`，最終 configure 成功。依賴完整建置，無研究 object 覆蓋。

### 環境與比較方法

Windows 11 Pro build 26200、Core Ultra 5 125H（14 實體／18 邏輯核心）、32 GiB、AC／Balanced、MSVC 14.51.36231、gRPC 1.81.1 overlay #2。QPC 10 MHz；probe 要求 1 ms timer resolution。一般背景程序仍存在，非隔離效能主機。

兩個 matrix 各三輪，順序固定為 8→64→256、64→256→8、256→8→64 KiB，每批 1＋4 秒、目標 48 Hz、RGB888 1280×720（2,764,800 bytes）。同一 executable 與 dependency，JSONL、設定與 SHA-256 均保存。所有 18 批 `valid=true`，意義為資料／時窗有效，並非通過遊戲延遲門檻。

Loopback 執行時 AVD 已離線；完成後才以原設定啟動唯一 `phigros` AVD 做 Emulator matrix。因此只在各自 matrix 內比較，不將 loopback 與 Emulator 數字視為同一環境。AVD：5 vCPU、8 GiB、host GPU、Android 16 x86_64、720×1280／320 DPI、輸出 landscape 1280×720。Fixture 設定 48 Hz，APK SHA-256 `195b6a0673c5d2182fe9b9face60cbfae8457dcf4c46bb82a873ad99d35fe964`，正式 bench 已驗證安裝版本。測後核對自啟動的 launcher／qemu PID，回到 HOME 並正常關閉，恢復實測前離線狀態。

### Loopback 傳輸路徑

`ready→Read` 是同主機 QPC 下 server 呼叫 Write 前到 client typed Read 完成，包含序列化、gRPC、TCP、OS 排程與解碼，不能稱為純網路延遲。CPU 包含此測試程序內的 client＋server，以單核心 100% 計。

| 批次 | 樣本 | p50 ms | p95 ms | p99 ms | CPU 單核心 % |
|---|---:|---:|---:|---:|---:|
| r1 8 KiB | 192 | 9.70 | 29.28 | 38.29 | 69.40 |
| r1 64 KiB | 192 | 5.68 | 21.15 | 24.19 | 33.59 |
| r1 256 KiB | 193 | 3.45 | 19.27 | 33.07 | 9.33 |
| r2 64 KiB | 192 | 5.43 | 18.80 | 20.44 | 38.71 |
| r2 256 KiB | 191 | 4.56 | 18.13 | 19.07 | 19.53 |
| r2 8 KiB | 193 | 8.30 | 27.02 | 37.16 | 27.73 |
| r3 256 KiB | 193 | 3.57 | 17.61 | 18.50 | 28.16 |
| r3 8 KiB | 192 | 8.55 | 21.83 | 32.75 | 69.64 |
| r3 64 KiB | 192 | 5.05 | 18.92 | 32.43 | 32.30 |

8／64／256 KiB 合計樣本各為 577／576／577。本輪 256 的 p50 與 p99 三輪均低於 8，支持此傳輸路徑有改善；前輪研究的 p99 並未一致改善，不能把這次短窗當作普遍尾端延遲保證。

### Emulator 傳輸與可見更新

CPU 僅 client probe，不包含模擬器及正式擷取的像素複製／consumer 工作。下表延遲欄是**相鄰到達間隔**，不是 source→client 延遲；Emulator 時鐘未校準，絕對 source age 保持 null。

| 批次 | 樣本 | client CPU % | 接收 Hz | 到達 p50 ms | p95 ms | p99 ms | 可見追隨率 |
|---|---:|---:|---:|---:|---:|---:|---:|
| r1 8 KiB | 193 | 45.20 | 48.09 | 17.11 | 39.68 | 44.94 | 1.0000 |
| r1 64 KiB | 193 | 18.27 | 48.12 | 17.78 | 41.71 | 46.27 | 1.0000 |
| r1 256 KiB | 193 | 4.29 | 48.21 | 17.70 | 36.83 | 43.86 | 1.0000 |
| r2 64 KiB | 192 | 10.13 | 48.20 | 17.63 | 41.37 | 47.13 | 1.0000 |
| r2 256 KiB | 191 | 3.15 | 48.26 | 17.16 | 44.26 | 50.08 | 1.0000 |
| r2 8 KiB | 192 | 28.58 | 48.04 | 17.03 | 35.04 | 50.33 | 1.0000 |
| r3 256 KiB | 193 | 5.82 | 48.01 | 17.97 | 43.27 | 46.33 | 1.0000 |
| r3 8 KiB | 192 | 32.13 | 48.08 | 17.13 | 35.25 | 42.61 | 1.0000 |
| r3 64 KiB | 191 | 12.11 | 47.91 | 17.32 | 43.00 | 58.72 | 0.9948 |

8／64／256 KiB 合計樣本各 577／576／577。Fixture 像素 counter 推估 source 48.01–48.26 Hz；r3 64 KiB 有 1 次 source sequence gap，其餘為 0。256 的 CPU 三輪均低於另外兩組，到達間隔 p99 則沒有一致改善。這是選用 256 的資源成本依據，不能取代遊戲觸控閉環。

### 正式流程、恢復與儲存

- `production-normal`：不明示區塊參數，manifest 確認預設 262144 bytes／revision 2。4.009 秒內接收 192 張（47.89 Hz），消費 177 張不同 Fixture 畫面（44.15 Hz，追隨率 0.9228）；consumer skips 14，符合 latest-only。消費端像素全數可解碼，geometry 正確，線／方塊位置誤差 0。接收完成→消費 host residency p50／p95／p99 為 1.011／1.565／2.266 ms（n=177），不是畫面絕對年齡。程序 CPU 約單核心 23.11%，包含正式擷取／分析與量測工作，不能與 probe CPU 直接比較。
- `production-pause`：4 秒正式窗內暫停接收約 503 ms；接收 167、消費 155 張。恢復後 17.74 ms 收到第一張，額外相對 lag 3.06 ms；後續第一秒 47 個樣本均在 20.61 ms 內。此次沒有 stale drops，故只能證明本次恢復正常，不能聲稱 250 ms lag guard 已被觸發驗證。完整相對時間在 `pause-analysis.json`；暫停窗接收率下降屬預期。
- 舊 `configs/avd-observe.json` 未指定新欄位，2 秒 observe 確認自動預設 256／revision 2、消費 95 張、正常 STOPPED、`input_created=false`、0 pool drops。run：`measurements/runtime/cpp-observe-17904192926357155`。
- 本輪證據目錄 PNG 數量為 0，正常及暫停 bench 均 `diagnostic_image_retention=none`。僅保存小型設定、日誌與量測資料。
- 已清除失敗 revision 1 的約 1.62 GB ABI archive 及兩個臨時偵錯 PDB，共 1,898,343,868 bytes（約 1.77 GiB），紀錄見 `removed-build-temporaries.json`；保留可重用的 revision 2 cache。vcpkg 已清理成功建置的 buildtrees／package staging。先前被自動審核拒絕刪除的研究 source 目錄未再刪除。

## 決定與界線

正式預設採 256 KiB；需要重現 upstream 接收大小時使用 8 KiB，64 KiB 留作比較。此次完成的是 Windows gRPC 接收層的正式開發與短窗驗證。現有 gRPC 主用擷取決策維持；尚未補齊的 M3 lag guard 與簡單目標觸控閉環仍依 ROADMAP 推進，不因本次 CPU／loopback 結果宣稱 Phigros 操作時序已合格。
