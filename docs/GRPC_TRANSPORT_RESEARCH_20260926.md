# gRPC 傳輸延遲實測（2026-09-26）

後續使用者已授權正式接入，現行實作與驗證見 [接入紀錄](GRPC_TRANSPORT_INTEGRATION_20260926.md)。下文保留研究當時「未修改正式 dependency」的決定；其 prototype build 指令只對前輪凍結版本有效，現行比較改用 `pas_grpc_latency --read-size-matrix`。

## 結論與決定

**gRPC 本身有可量測的改善空間。最有證據的方向是 Windows EventEngine 的接收區塊大小，不是再加大 HTTP/2 預讀或停用 BDP。** 獨立實驗把底層 8 KiB 接收改成 64／256 KiB，同主機完整畫面的 ready→Read 中位數由 6.66–8.30 ms 降至 3.83–4.40 ms，CPU 下降；但 p99 並未每批改善，不能宣稱尾端延遲已解決。

保留 64／256 KiB 作為後續依賴修補候選，**本次不改正式 gRPC dependency、`GrpcCapture` 或 Session 預設**。若正式採用，需要將修改做成固定版本的 dependency patch，檢查多 stream／小訊息／慢讀者的記憶體及取消行為；不把實驗用的 translation-unit override 當正式供應方式。由於優先目標是尾端延遲，下一步應定位剩餘 EventEngine 排程停頓，而不是只追求更低中位數。

## 環境與界線

- Windows 11 Pro 10.0.26200、Core Ultra 5 125H（18 logical processors）、約 32 GiB RAM、Balanced 電源方案；未關閉使用者其他程序，屬日常負載短測。
- MSVC 14.51、Release、gRPC 1.81.1／protobuf 6.33.4，QPC 10 MHz；探針以 RAII 请求 1 ms timer resolution。
- Emulator 37.1.11.0／build 15917651，Android 16、x86_64、5 vCPU、MemTotal 8,130,816 KiB。原顯示 profile 720×1280／density 320；Fixture 橫向 RGB888 1280×720、rotation 1，每張 2,764,800 bytes。
- 真實 Fixture 目標維持原本的 48 Hz、freeze=0；沒有改 AVD 硬體、GPU mode、解析度、電源方案或使用遊戲輸入。實際可見更新率另量，不能以目標代替。
- 不落盤保存 pixels。每個探針只保留目前 protobuf 圖像，最多 4,096 筆小型數值紀錄；讀取期間不寫 JSONL。固定時窗、RPC deadline 與 server cancellation 都有上限。
- `ready_to_read_ms` 是同主機 QPC 下「測試 server 呼叫 Write 前」到「client typed Read 返回」，包含 serialization、gRPC／TCP、排隊、decode 與 OS 排程；**不是純網路線上時間，也不是 Android 畫面年齡**。
- 真實 Emulator 未有已校準的源端 QPC；保留原始來源 timestamp，但絕對 source age 維持 null。到達間隔及相對變化不能替代絕對傳輸延遲。

環境細節與原始證據位於 `measurements/grpc_latency_20260926/`，包含 binary hash、事先固定的 plan、每次 raw／summary／manifest、編譯及回歸紀錄。以下範圍均為三批各自統計的 min–max，不是合併樣本的 percentile，也不是信賴區間。

本輪 CPU 由 `GetProcessTimes` 的差值計算並保存百分比與量測 QPC 邊界，最初探針沒有輸出兩個原始 CPU counter，故 CPU 表可查核摘要但無法僅由 JSONL 獨立重算；收尾版已補存 counter，舊結果不回填。延遲與像素跟隨的原始數值均有保留。

## 量測設計

每批 1 秒暖機＋6 秒正式窗口，所有批次串行；三候選以 A/B/C、B/C/A、C/A/B 平衡順序。正式比較前以一秒 smoke 驗證。每個 probe 用相同像素格式與大小，只有指定的 channel argument 或 Windows 讀取常數不同。首個 channel loopback 矩陣執行時 AVD 在 launcher，後續 read-size loopback 時 Capture Fixture 已運行；只在各自矩陣內比較，不跨這兩種背景負載計算改善率。

1. channel 參數：預設、`grpc.http2.lookahead_bytes=4194304`、`grpc.http2.bdp_probe=0`；同主機 loopback 9 批、真實 Emulator 9 批。
2. loopback 對照另做 40／59 Hz，以及 48 Hz 每次 Read 後延遲 5／25 ms，各一批；這四批只作機制診斷。
3. 底層接收：重新編譯的 8 KiB 控制、64 KiB、256 KiB，loopback 9 批、真實 Emulator 9 批。每種各三批，不重跑挑最好數字。
4. 額外兩個約兩秒 trace 只驗證執行路徑與讀取次數，**不納入性能比較**。

首次 Emulator smoke 在 Activity 切換尚未完成時因缺少 Fixture pixels 失敗並非零退出；原資料保留。待畫面就緒的第二次 smoke 通過。所有 40 個正式短窗口皆有效；失敗 smoke 沒有被覆蓋或計入有效比較。這不是長期穩定性驗收。

## HTTP/2 參數結果

同主機 loopback，48 Hz，每個候選約 863 個正式樣本：

| 設定 | ready→Read p50 ms | p99 ms | 程序 CPU，單核心百分比 |
| --- | ---: | ---: | ---: |
| 預設 | 3.93–7.15 | 24.95–32.69 | 34.49–54.20 |
| lookahead 4 MiB | 4.64–7.10 | 28.56–35.46 | 24.53–56.92 |
| BDP off | 6.73–9.09 | 31.92–35.13 | 85.77–114.22 |

沒有可支持採用 lookahead 的穩定改善；BDP off 在本機增加 CPU 與傳輸成本。真實 Emulator 亦沒有足夠證據選用兩者：來源率跨批 43.44–48.09 Hz，本身已不完全配對；BDP off 部分批次的可見跟隨率下降。

獨立 `flowctl` trace 顯示預設連線的 INITIAL_WINDOW_SIZE 與 MAX_FRAME_SIZE 已從 65,535／16,384 調至 4,194,304 bytes。不能再將問題簡化成「HTTP/2 永遠只有 64 KiB 視窗」。這不表示所有可能的流控停頓都已排除。

## Windows 接收區塊實驗

核對 [gRPC v1.81.1 WindowsEndpoint](https://github.com/grpc/grpc/blob/v1.81.1/src/core/lib/event_engine/windows/windows_endpoint.cc)：接收最小配置固定 8,192 bytes，該實作忽略 EndpointConfig 與 ReadArgs；一般 TCP chunk channel arguments 不等於這裡的可調旋鈕。

使用官方 v1.81.1 source archive，僅重新編譯 `windows_endpoint.cc`，以獨立 probe 的 object 覆蓋 static library 中該 translation unit。三版用相同工具鏈；8 KiB 版也是重編控制組。link map 證實 `WindowsEndpoint::Read`／`DoTcpRead` 由實驗 object 提供。未重建整包依賴，也未覆寫 installed library。

同主機 loopback、48 Hz：

| 接收區塊 | 正式 n | ready→Read p50 ms | p95 ms | p99 ms | 程序 CPU，單核心百分比 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 8 KiB 控制 | 864 | 6.66–8.30 | 18.90–24.66 | 33.60–37.79 | 47.92–58.85 |
| 64 KiB | 866 | 3.83–4.40 | 16.86–22.97 | 26.02–40.75 | 15.36–36.35 |
| 256 KiB | 863 | 3.88–4.00 | 21.46–24.89 | 29.12–37.29 | 12.24–32.55 |

每輪中位數和 CPU 都下降，但 64 KiB 第二輪 p99 為 40.75 ms，比該輪控制的 35.00 ms 更差；256 KiB 第二、三輪也不是一致改善。最大值分別達 51.49／61.35／63.44 ms。不能只報最好的 26.02 ms。

獨立 trace：8 KiB 在 98 筆已交付訊息期間記到 33,226 次 read attempt；64 KiB 在 97 筆期间為 4,461 次，約由每訊息 339 次降至 46 次。這包含兩個 loopback endpoint、HTTP/2 控制訊息及取消尾端，並非精確的一張圖 syscall 數；但與減少小區塊讀取的機制相符。trace 沒有啟用 payload-data logging。

真實 Emulator（這裡量到達間隔，沒有絕對延遲）：

| 接收區塊 | 正式 n | 可見來源 Hz | 到達間隔 p99 ms | client CPU，單核心百分比 |
| --- | ---: | ---: | ---: | ---: |
| 8 KiB 控制 | 818 | 43.39–47.49 | 48.01–56.70 | 37.52–55.81 |
| 64 KiB | 850 | 46.61–47.86 | 46.26–52.81 | 17.42–20.64 |
| 256 KiB | 857 | 46.98–47.88 | 44.57–49.62 | 10.15–13.78 |

這九批四角色碼與独立 binary counter 均驗證，source sequence gaps 為 0。跨批來源變動使速度排名有限制；CPU 改善明顯，但不能宣稱 Emulator 畫面生成到主機收到已縮短同樣比例。loopback 的 CPU 包括 test server；Emulator 表只計 client，沒有測量其完整 producer CPU。

## 接收停頓與剩餘瓶頸

loopback 40／59 Hz 兩個控制各有 239／354 個正式樣本，ready→Read p99 為 28.92／29.41 ms。正常不停讀的應用層 read gap 很小，傳輸尾端仍存在；因此「把 normalize 搬到其他 thread」不能自動解決目前看到的全部尖峰。

Read 後延遲 5 ms 的一批仍可維持 48 Hz；延遲 25 ms 時，正式樣本降至 229，server ready→Read p99 54.41 ms，原排程→Read p99 累積至 1,433.16 ms。這個合成 server 逐次 Write、producer 被回壓後延後產生，沒有影像 FIFO；不能把結果冒充 Emulator 的實際 queue policy。它證明讀者落後時只量 Read 返回時間會漏掉 producer 停頓。

下一個優先研究點是 Windows EventEngine 的 callback／thread-pool 排程尾端，以及正式 capture callback 的實際 read gap。需用短 ETW／排程追蹤分段，避免任意提高 thread priority 或增加緩衝。若正式 callback 確實造成回壓，才加入有界的專用接收 owner。

公開 Emulator source 的 `streamScreenshot` 為等待 frame event → `getScreenshot` → `Write`；截圖 metadata timestamp 在取圖／轉換前設定。該公開分支未與安裝 build 建立一對一版本對應，因此只作調查線索，不用它校準本機絕對 source age。原始 [EmulatorService.cpp](https://android.googlesource.com/platform/external/qemu/+/refs/heads/emu-master-dev/android/android-grpc/services/emulator-controller/server/src/android/emulation/control/EmulatorService.cpp) 與 [gRPC 流控說明](https://grpc.io/docs/guides/flow-control/)可供查核。

## 重現入口

正常建置會產生 `pas_grpc_latency.exe`；它是診斷工具，不是新的 runtime 後端。

```powershell
out/release-v145/Release/pas_grpc_latency.exe --source loopback --matrix --output-dir measurements/new-grpc-loopback
# 先啟動已安裝的 Capture Fixture 並確認橫向 pixels 就緒
out/release-v145/Release/pas_grpc_latency.exe --source emulator --matrix --output-dir measurements/new-grpc-emulator
```

底層實驗需官方 `https://github.com/grpc/grpc/archive/refs/tags/v1.81.1.tar.gz`，SHA256 `48ae0d05f87206112d9e9144a923191ee1e482141a70686ec58dc86d0b40fddc`。解壓 `include`／`src` 即足以編譯本實驗，避免解壓範例內 Windows 不支援的 symlink。CMake 另檢查 endpoint 原文 SHA256 `72dff6dc4f09c5bce3b54056dafe26685f6e0db5e8cd5e338a06be14177444fc`。

```powershell
cmake --preset windows-release -DPAS_GRPC_RESEARCH_SOURCE_DIR=<grpc-1.81.1-source>
cmake --build --preset windows-release --target pas_grpc_latency_read8192 pas_grpc_latency_read65536 pas_grpc_latency_read262144 pas_grpc_read_campaign
out/release-v145/Release/pas_grpc_read_campaign.exe --source loopback --output-dir measurements/new-read-size-loopback
out/release-v145/Release/pas_grpc_read_campaign.exe --source emulator --output-dir measurements/new-read-size-emulator
```

研究 targets 預設不參與 all build，需顯式啟用；不向正式 `pas` 連結 override。原版 Apache-2.0 授權仍適用，見既有 [gRPC 授權](third_party/grpc.txt)。驗證與暫存清理結果記於本次證據目錄。

## 驗證

- 原正式回歸 31／31 通過。
- 64／256 KiB 兩版各 5／5 真 loopback 回歸通過，包含接收與合作取消、截短 payload、來源時間凍結、sequence reset、旋轉像素核對。沒有以性能閾值作不穩定單元測試。
- 原始採樣版探針與對應 source 已凍結；初版 source hash 與環境紀錄一致。後續補 CPU counter 的收尾版不冒充本輪已測 binary。
- 已驗證解壓 source 位於本研究目錄且無 reparse point，但清理約 75 MiB 解壓暫存的操作被自動審核以 `blocked by policy` 拒絕；未提供細項原因，因此該資料夾仍保留。原始 archive、source hash、數值資料與凍結二進位亦保留，沒有影像檔累積。CMake 的實驗 source 選項已清空，正常建置不依賴這份暫存。
