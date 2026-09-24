# 擷取低延遲開發計畫：獨立程序與 Emulator MMAP

日期：2026-09-24。狀態：已規劃，待冷開發。本輪同時實作獨立擷取程序與 gRPC MMAP 傳輸；實機驗證另行集中安排。

## 1. 授權範圍、基線與開始方式

- 使用者要求先冷開發。本輪不得啟動、重啟、操控或連線測試 emulator；即使它已在執行，也不自動進行 ADB、gRPC 或 fixture 實機測試。允許讀取本機 SDK/proto、既有日誌、官方原始碼，執行本機假來源、loopback 假 gRPC server、程序與共享記憶體測試。假 server 必須自行建立測試端點，不讀真實裝置 discovery。
- 在新 task 的獨立 worktree 工作，不修改其他 task 的工作樹，不自行合併 main 或推送遠端。開始先讀 AGENTS.md、README.md、ARCHITECTURE.md、ROADMAP.md、CAPTURE_IMPLEMENTATION_PLAN.md、CAPTURE_AND_VISION_RESEARCH.md、MEASUREMENTS.md 與本計畫。
- 目前 main 的程式基線仍是 c38fe00；上一輪已驗收修正位於本機分支 `codex/capture-round2-reliability`，提交 `8477aec`。新 task 先在自己的分支合併此提交，保留本計畫，再實作本輪功能；不得從舊 main 忽略既有修正。確認提交包含 e2444f3、e5aa0aa、e92d03e 等修正。若 ref 不存在先查本機 Git，不憑空重建修正。
- 上輪 36 項測試通過；在約 59 Hz 原生 fixture 下，三批各 60 秒、每批約 3,500 張，1280×720 RGB888 約 58.47–59.31 個不同畫面/秒，到達間隔 p95 29.58–32.48 ms、p99 38.71–40.67 ms。同程序 GIL 忙碌批次 30 秒、510 張，約 17 fps、p99 96.33 ms。這些是歷史條件結果，不是本輪性能承諾，也不是絕對來源延遲。
- 歷史原始資料在 `C:/Users/wurre/.codex/worktrees/0999/Phigros-Auto-play-System/measurements/`，被 Git 忽略；只讀引用，不覆寫、不搬移，也不假設新 worktree 自帶資料。此前約 35 Hz 與後續約 59 Hz 的環境差異原因仍未知。
- 僅處理畫面擷取、交接、健康狀態、CLI、量測與文件。保留即時 pixels 為唯一遊戲決策來源；不涉及遊戲狀態記憶體、譜面、觸控注入、辨識模型、WGC 或 scrcpy。此處共享記憶體只傳公開擷取 API 輸出的像素。

## 2. 目標與可比較配置

將「執行方式」與「像素傳輸」分開選擇，避免綁死：

| 配置 | 用途 |
| --- | --- |
| 同程序 + gRPC payload | 保留既有行為作回歸與性能對照 |
| 獨立程序 + gRPC payload | 必須完成、可正常供現有 Capture/Session 使用的隔離方案 |
| 獨立程序 + gRPC MMAP | 必須實作的實驗候選；可否供正式 Session 使用取決於來源一致性證據 |

同程序 MMAP 可作診斷選項，但不是必交配置。以 Python spawn 子程序先完成；只有離線 profiling 顯示接收器本身或必要同步操作有明確瓶頸/能力缺口，才加入小型 C++/Rust 等原生元件。原生改寫不是先決條件，不因「可能較快」全面重寫。

成功定義分為：冷開發與離線功能驗收完成、真實 Emulator 相容性驗收、性能改善驗收。後兩項本輪保持待測，不宣稱零複製、無撕裂或更低絕對延遲。

## 3. 架構與介面

資料流：Emulator → payload/MMAP transport → 擷取子程序的穩定像素快照 → 有界 IPC 最新影格交換區 → 父程序 Capture/Session/未來視覺消費者。

- 分開封裝來源 transport、程序監督、IPC 與既有 Frame 契約。父程序可透過相容 adapter 接既有 CaptureWorker；不得把遊戲邏輯寫入子程序。
- 子程序擁有 gRPC channel、stream、來源 MMAP handle 與取消操作。Windows 使用 spawn，入口可 import，所有配置可序列化；不能把已開啟的 channel、thread lock 或任意 closure 當跨程序契約。
- IPC 不逐幀 pickle 大型 bytes，不使用無上限 Queue。採固定容量共享像素區與有界控制訊息；通知可合併，只代表「可能有更新」，不代表必須依序處理每張圖。
- 邏輯上始終只公開最新 frame。可有固定數量的物理 slot 防止讀寫衝突，但不形成 FIFO backlog；慢 consumer 不得無限阻塞 source。指定最大配置容量、記憶體上限及超出容量的失敗/重建方式。
- 父程序取得的既有 `Frame.rgb` 必須是持有期間不被覆寫的 bytes 快照；不要把可變共享 memoryview 偽裝成 immutable Frame。若另引入 lease API，明確定義取得/釋放與停止後行為，且保留舊 API 相容性。
- IPC header 至少含 schema/version、generation、發布序號、來源序號、尺寸、格式、行序、rotation、有效 payload 長度及各時間點。驗證乘法/大小上限、格式、序號、generation，拒收半初始化、過期或損壞 header。

## 4. 兩段共享記憶體一致性必須分別解決

### 4.1 Emulator → 擷取子程序的 MMAP

- 以倉庫及本機 Emulator 37.1.11 proto 為依據，實作 `ImageTransport.channel=MMAP` 與客戶端建立/擁有的 handle。確認 Windows 的 file URI、映射方式、容量、權限、檔案鎖與解除映射；不可把 Python SharedMemory 的名稱直接假定為 Emulator 可用 handle。
- 讀取相符版本的官方實作或文件，留下版本/commit、來源連結及寫入/通知順序的分析。proto 明確警告 tearing，單次 RPC 通知不能直接證明讀取期間不會被下一張覆寫。
- 區分 notification_received、snapshot_copy_started、snapshot_copy_complete 與 pixels_ready；metadata 的 seq/time 不得無證據地配到已被覆寫的新像素。
- 若生產端沒有 reader acknowledgment、鎖、generation/sequence fencing 等可證明協定，不能靠客戶端自行加鎖、兩次讀取相同、CRC 相同、copy 很快或看起來正常，宣稱一致性保證。客戶端的雙緩衝只能保護下一段 IPC，不能修補上游未同步寫入。
- 若現有協定不能保證一致快照：仍完成 MMAP transport、資源管理、假 server 與故障測試，作明確 opt-in 的診斷/benchmark 模式；記錄 `consistency=unverified`，禁止其進入正式 Session 的有效 Frame 路徑。以獨立程序 payload 作正常可用方案；交付具體限制及後續可行的生產端同步提案，不假裝完成安全 MMAP。不要因此停下其他已授權開發，也不要擅自 fork/重建整套 emulator。
- 假來源可在畫面多個區塊寫相同 frame ID 及校驗資訊以揭露混幀；只能用於測試驗證，不能要求真實遊戲附帶標記。
- MMAP reply 的 image bytes 預期為空；拒絕不符合所選 transport 的回覆，或明確記錄並執行使用者選擇的 fallback。不得靜默改 transport 後把結果標成 MMAP。
- 預先限制映射大小，處理 inactive、無效尺寸、超容量、rotation、格式錯誤、來源重置、timestamp 不連續及取消。重建時先停止 producer 寫入，再解除/重建 mapping，generation 必須更新；不得對仍被 server 使用的區域直接縮放或刪除。

### 4.2 擷取子程序 → 父程序的 IPC

- 這段的兩端皆由本專案控制，必須有明確同步與穩定快照證明。可使用短臨界區/固定多 slot ownership 協定；若選 seqlock，必須提供跨程序 atomic 與 memory ordering 的依據，不能把 Python 普通整数讀寫視為充分保證。
- 同步等待有期限；consumer 卡死、reader 持鎖死亡或 child 在發布途中退出時，不能讓其他程序永久等待或把半張圖當成功。重試次數有上限，失敗回報原因並清理。
- 通知清除與發布競態不能導致漏醒後永久停住；重讀序號/有期限等待與狀態查驗須涵蓋靜態、inactive、無新圖及 child 死亡。
- 獨立計數：來源跳號、來源相對舊圖丟棄、來源一致性拒收、IPC 覆蓋、IPC 讀取重試/拒收、consumer skip、telemetry drop。勿相加當總丟幀。

## 5. 程序生命週期、健康与安全

- 狀態至少能區分 STARTING、READY、STOPPING、STOPPED、FAILED；來源 active/inactive、新鮮度與程序 alive 分開。child heartbeat 不可刷新舊 frame 的時間，也不能證明來源有新畫面。
- 首張有效圖的 READY、有期限健康請求、統計快照及停止使用有界控制協定。保留上一輪「probe 等候期間新圖到達」的 recheck 行為，避免跨程序重引入競態。
- Ctrl+C、來源阻塞、啟動失敗、parent 正常/異常退出、child crash、反覆 start/stop 都有回收路徑。先 cooperative cancel/join；超時才對本 task 建立的 child 做有界強制回收，記錄強制停止。正常停止目標 2 秒，異常總期限也須明確，不允許孤兒程序或無限 join。
- 規定每個 mapping/file/process 的 owner 與清理順序。不能掃除其他 task 的共享檔案；崩潰殘留的回收只認領本程式可驗證 ownership 的資源。
- 認證沿用 loopback、token 與 discovery。token 不放命令列、repr、traceback、JSONL；不要因 multiprocessing 方便而關閉認證。來源初始化在 child，配置與例外須脫敏。
- 保留可選 gRPC 依賴。無 extras 時核心、IPC 與 fake-source 測試能執行；依賴型測試明確 skip，不能把所有新增測試一併 skip。

## 6. 時間、統計與 CLI

- 同機父子程序使用可比較的 host monotonic clock；在 Windows spawn 測試中驗證時域相容性，記錄 clock 實作。禁止用 Unix/wall clock 補出來源年齡。保留 `produced_ns=None` 直到另有可信校準。
- 明確區分 stream notification/payload received、穩定快照完成、pixel normalization 完成、IPC publish、parent snapshot 完成與 consumer start；不可把 IPC 到達冒充 Emulator 來源產生時間。
- 保持現有 `capture_complete_ns` 的可解釋性：payload 為完整 payload 收齊；MMAP 必須明定穩定快照取得的語義及獨立 notification 時間。變更 Frame 欄位/契約時同步文件及使用端，不混合兩種測量點直接比較。
- 保留就緒 → 暖機 → 完整正式窗口 → 停止的量測分界。父子使用同一組正式起訖 monotonic 邊界，不能因 parent GIL 忙碌，延後取樣卻仍宣稱精確窗口。快照取得偏差及缺測需揭露。
- 分別報 parent/child CPU 同窗口差值、RSS、IPC 成本與日誌成本；只有可比時才另列合計，不沿用單程序 process_time 假裝全程 CPU。child 提前退出也要保留已有統計並標記未完成。
- JSONL 以串流或有界管線寫入；大量事件不經無上限 IPC。日誌慢時有明確上限、丟棄/失敗政策與計數。壓力測試需區分開啟/關閉詳細 instrumentation。
- 提供清楚 CLI 選項，例如 `--capture-execution thread|process`、`--grpc-transport payload|mmap`、容量/啟動/停止期限；名稱可依現有風格調整。驗證不支援組合，記錄 requested/effective 模式及 consistency；保留現有預設直到實機驗收再評估切換。
- 新模式串接 capture-bench 與 start-session；一致性未證實的 MMAP 只准診斷入口。提供離線 bench 入口/腳本，使用假來源，不會不小心發現或連線真實 Emulator。

## 7. 冷開發順序與必要測試

1. 整合 8477aec、重跑既有測試、確認不依賴 Emulator。梳理現有 health/counter/Frame 介面，必要調整保持相容。
2. 實作 spawn supervisor 與固定容量 IPC，用獨立假 producer 完成讀寫/停止/故障驗證，再接既有 payload source。
3. 研究並實作 MMAP lifecycle/transport，使用本機假 gRPC server 或等價可注入 transport 模擬 server 寫檔與通知。落實來源一致性判定及模式門控，再與 process 模式整合。
4. 完成同條件離線 benchmark、CLI、文件與結果重算。根據 profiling 決定是否需要小型原生 helper；若無證據則保留 Python 子程序並說明。

必要測試包含：

- 1280×720 RGB/RGBA、行序、像素圖樣/角落/rotation、無效長度、超容量、inactive、來源序號與 Unix 時戳異常。
- 真正 Windows spawn 的跨程序讀寫，metadata 與 pixels 配對、已交付 Frame 不變、50/100 ms 慢 consumer、恢復時取最新、固定記憶體容量。
- parent CPU/GIL 忙碌時 producer 能持續前進；另測 child CPU 負載，兩者不混為一談。可使用可見圖樣 ID/真值只作離線測試。
- 用同步 barrier/event 可重現半寫入、快寫慢讀、通知競態、換 generation、尺寸變更、child 中途退出、parent 離開及 lock owner 死亡；不要只靠 sleep 的碰運氣測試。
- MMAP 假 server 延遲通知、連續覆寫、混幀與 metadata 錯配；不能用假 server 比真實 Emulator 更強的同步能力，反向聲稱實機一致性已證明。
- 準備 timeout、零有效圖、就緒後暖機、完整正式窗口、窗口邊界回調、父子 CPU 分母、停止清理成本排除、child 提前失敗，報告可由 JSONL 重算。
- 探測期間新圖抵達、靜態來源無法驗證、取消、連線失敗/拒絕、gRPC 缺失、token 脫敏、反覆啟停且無殘留。

離線性能樣本使用固定設定與 seed，記錄 Windows/Python/CPU、commit、命令、樣本數與 p50/p95/p99/max。至少比較 thread payload-equivalent fake、process payload-equivalent fake、process MMAP fake，在正常/parent 負載/慢 consumer 條件下的交接成本。這僅衡量 harness/IPC，不模擬得出 Emulator 真實 latency。性能統計不使用脆弱的機器速度斷言代替功能測試。

## 8. 後續集中實機驗收清單（本輪只準備，不執行）

- 三個可比較配置使用相同 AVD、版本、前景原生 fixture、1280×720、RGB888、來源更新率及主機負載；若 MMAP consistency 未證實，只能作診斷資料，不可列為 production 合格配置。
- 就緒後暖機 10 秒、正式 60 秒，正常每配置至少 3 批；以交錯順序降低環境漂移，保留獨立 run ID、設定、原始 JSONL、雜湊與診斷像素。
- parent GIL 負載、child 負載、慢 consumer、接收停頓 500 ms、取消/崩潰、尺寸/方向及安全故障集中測試，保留所有失敗。
- 既有研究門檻保留：已證實約 60 Hz 來源時至少 55 個不同畫面/秒，到達間隔 p95 ≤33.4 ms、p99 ≤50 ms，停頓後恢復另測。這不是低來源延遲的證明。
- 新方案是否採用看同條件尾端表現、資源消耗、像素一致性與恢復能力；不能只看平均 FPS。可見事件延遲需標明起點及 clock mapping；來源年齡未知時仍標未知，不能用 MMAP notification、WGC 時間或到達間隔代替。

## 9. 交付與停止點

- 在自己的分支提交完整程式、必要測試、離線 benchmark 與重算工具，確保 git diff --check 及適當既有/新測試通過。
- 更新 README、ARCHITECTURE、ROADMAP、研究文件及 MEASUREMENTS；保留歷史量測，清楚區分「已完成離線」「待實機」「MMAP 一致性限制」。CAPTURE_IMPLEMENTATION_PLAN.md 保留上一輪成果與範圍，新增本計畫連結，不把舊驗收洗掉。
- 本計畫加入實際完成狀態、命令、schema/配置決策、測試數及限制；新 offline artifacts 有路徑與雜湊，提醒 Git ignored 資料不隨 worktree/merge 帶走。
- 完成後回報提交、測試結果、payload process 可用性、MMAP 實作與安全門控狀態、原生 helper 採用/未採用理由，以及一份可一次執行的實機清單。不要自動開始實機測試，也不要因缺 emulator 停下冷開發可完成的部分。

## 參考

- [Emulator 協定](https://github.com/google/android-emulator-webrtc/blob/master/proto/emulator_controller.proto)：以倉庫/安裝版 proto 與對應實作為準，重點是 ImageTransport、streamScreenshot 及 tearing 警告。
- 上一輪 [擷取實作與驗收計畫](CAPTURE_IMPLEMENTATION_PLAN.md)、[量測紀錄](MEASUREMENTS.md)。
