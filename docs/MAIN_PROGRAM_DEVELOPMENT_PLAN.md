# 主程式完整開發計畫

## C++20 現行入口

T0–T5 的正式實作、逐項驗收與新數值門檻分別見 [C++ 遷移計畫](CPP_MIGRATION_PLAN.md)、[驗收矩陣](CPP_PARITY_MATRIX.md) 與 [路線圖](ROADMAP.md)。目前的 C++ observe 先擷取並只顯示診斷畫面；獨立 native Fixture 負責觸控能力測試。M3 真實簡單目標閉環與 Phigros 遊玩辨識仍屬後續工作。以下原 M0–M7 分期與 Python／process 實作敘述保留為歷史需求及來源，不應當作現行 C++ 程式已驗收的功能清單。

## 歷史 M0–M7 計畫

> 2026-09-25 後續決策已變更：使用者選定全自有 C++20、單程序多執行緒，先遷移與五種擷取技術評估，再繼續新主程式功能。本文的 Python 沿用／process 固定／後端探索前提屬舊計畫，不再約束新開發；M3–M7 功能需求與 pixels-only 原則仍保留。以 [C++ 遷移計畫](CPP_MIGRATION_PLAN.md) 為新工作入口。

> 2026-09-25 獨立驗收修正：M0–M2 未全數通過；M1 有 3 個可重現排程問題、M2 需補逐 pointer／反向軌跡驗證。以 [驗收報告](ACCEPTANCE_20260925.md) 的修正清單為下一步；文末先前實作狀態表不代表完整通過。

日期：2026-09-25。狀態：M0–M2 核心已實作並完成下述 Fixture 驗證；M3–M7 仍是目標設計。各項未測試內容以第 12 節為準。

依據：`AGENTS.md`、[架構](ARCHITECTURE.md)、[路線圖](ROADMAP.md)、[量測](MEASUREMENTS.md)、[Phigros 機制研究](PHIGROS_MECHANICS_RESEARCH.md)。遊戲機制的來源與尚未驗證的說法沿用研究文件，不重複將網路摘要當成實測規格。

## 1. 已決定的範圍

- 使用者指定：完成計畫後交給 **GPT-6 Sol，reasoning effort = xhigh** 開發。
- 使用者已選擇 **CLI＋即時診斷預覽**。完整桌面 GUI 後置。
- 主程式開發擷取基線固定為 **Android Emulator gRPC、獨立 process、payload、RGB888、top-down**。尺寸與方向啟動時確認，預期本機 1280×720；不同設定需建立對應 profile。
- 此選擇以已有功能與隔離需求為依據，不宣稱 process 比 thread 快，或已符合全部遊戲判定窗。現有 benchmark 的 thread 預設不必一起修改；新 runtime profile 明確使用 process。MMAP 不進主程式，ADB 單張截圖僅診斷。只有閉環證明擷取是阻塞瓶頸時才重開選型。
- 第一版是程式化追蹤、幾何預測、接觸狀態機及排程。視覺介面預留小模型替換；沒有 LLM 逐音符決策、強化學習或歌曲記憶。
- 第一版由使用者手動登入、選曲、選難度與處理遊戲選單；程式從畫面確認遊玩後接手。APK Fixture 安裝、啟動、觸控能力測試屬開發驗證，可直接進行；不要操作帳號、購買或修改遊戲存檔。
- 所有遊玩決策只用當前 pixels 與有限近期追蹤。外部資料僅供研究、標註、離線評估及通用模型訓練。

第一個實作交付是 M0–M2 的執行核心及觸控能力工具；第一個真實閉環里程碑是 M3；第一個遊戲里程碑是 M5 的有限場景 Tap。完整 v1 須完成 M0–M7 並明列支援範圍。不得把第一批交付稱為完整自動遊玩。

## 2. 現況與需補的缺口

| 現有元件 | 處理方式 |
| --- | --- |
| `capture_grpc.py`、`capture_process.py`、`CaptureWorker`、`LatestFrame` | 沿用；修整合所需問題，不重寫擷取 |
| `SessionController` | 沿用擷取先就緒與健康語義；抽出可供 runtime 使用的非阻塞狀態快照，避免重複啟動兩個 capture worker |
| 單點 `Observation`、`VelocityTracker`、固定線 Predictor | 保留為既有合成基線；遊戲契約新增，不把舊測試改成不同含義 |
| `Scheduler` | 現在只有 Tap 展開、contact ID 綁 track ID；新增一般動作排程，保留 Tap adapter |
| `FakeTouchBackend` | 保留；新增有 timeout、有限狀態與釋放結果的真實後端 |
| `Telemetry` | 現有同步寫檔與無檔時保留無限 events 不直接用於長時間 runtime；增加有限 runtime sink |
| CLI | 舊命令繼續可用；新命令委派給小型模組，避免把整個 runtime 塞進 `cli.py` |
| Android capture Fixture | 保留原能力；新增獨立 touch／closed-loop Fixture package，避免破壞擷取基準 |

## 3. 執行架構與資源所有權

```text
Capture process（既有）
  → capacity-1 RGB handoff
  → Perception worker → Tracker → Predictor
  → 有上限且可替換的意圖集合
  → ActionPlanner / ContactAllocator
  → Scheduler / TouchBackend（唯一觸控 owner）
  → Emulator → 新 pixels

Runtime / Session gate → epoch、arm、stop、fault
Telemetry writer / preview ← 有限診斷快照，不反向產生觸控
```

第一個核心版本：Capture 是既有 spawn process；Perception 在獨立 worker；排程、接觸配置及注入由單一專用執行緒擁有。跨執行緒只交 immutable messages，不讓兩個執行緒同時修改排程 heap 或 contacts。辨識先使用向量化像素處理；不得在遊戲熱路徑逐 RGB byte 跑 Python 迴圈。

這個 thread 配置不保證免於 GIL jitter。M3 要對辨識負載與排程尾端作配對測試；若辨識影響送出期限，移到獨立 perception process，沿用容量 1 交接和相同契約，再驗證 IPC 成本。不要把「獨立 thread」寫成實時保證。

各邊界均指定容量：frame slot=1、preview slot=1、意圖按 key 替換並有總量與時間範圍上限、排程只保留近期可執行命令。已完成 key 以 epoch／有限 TTL 回收；舊 generation 的 heap 項目需壓縮，不能在反覆修正時無限積累。

停止流程：撤銷 arm／遞增 epoch → owner 拒收新動作 → 取消未送命令 → 有期限地 release contacts → 關閉 backend → 停止 perception/capture → flush 診斷並回收資源。每步記錄結果；只清本 run 建立的資源。

## 4. 契約與識別

沿用 host `time.monotonic_ns()`，來源 Unix metadata 不與之直接相減。schema 明確 version；以下名稱可以依現有結構微調，語義不可省略。

| 契約 | 必要資料 |
| --- | --- |
| `RuntimeConfig` | profile/schema、serial、capture、touch、座標映射、期限、容量、啟用能力、預覽／日誌設定 |
| `FrameContext` | run/epoch、frame sequence、capture generation、尺寸／方向／transform version、capture_complete/pixels_ready/published 時間 |
| `SceneObservation` | context、recognition_start/end、UI 狀態與證據、多個 Note／line 候選、類型與朝向／位置／可見度 |
| `TrackSet` | 物件 ID、近期運動、線關聯候選、最後觀察時間、殘差與不確定度、存在／遮擋狀態 |
| `ActionIntent` | key/revision、來源 context、Note 類型、預測撞線時間、有效／到期範圍、可觸控區域、持續或滑動需求、可解釋依據 |
| `ContactPlan` | intent revisions、epoch、有限接觸 ID、down/move/up/batch、每步 deadline、取消／失敗策略、維持期限 |
| `TouchReceipt` | command ID、scheduled、call start/return、RPC 結果／不確定狀態；不把 RPC OK 當作 Android 已生效 |
| `ReleaseReport` | 各 contact 釋放請求結果、未知／失敗列表、開始／結束時間 |
| `CapabilityReport` | 後端／裝置／映射 fingerprint、宣稱與實測能力、contact 上限、測量設定、證據路徑 |

跨局／重新連線使用新的 runtime epoch，不依賴目前固定為 0 的 `stream_generation`。尺寸、方向、座標映射變更均停用並建立新 epoch。舊 epoch 結果即使稍後完成也不可執行。

UI PLAYING 與「這顆 Note 是否可預測」分離。看不到某條線不直接判為 MENU；但 UI UNKNOWN、來源不新鮮或傳輸錯誤必須撤銷所有遊玩資格。

## 5. 排程與觸控的關鍵規則

1. dispatch 前重新檢查 epoch、UI gate、證據年齡、有效期限、revision 與 backend 健康。只做 submit 時檢查不足。
2. 同一意圖的新 revision 取代尚未執行部分；已 down 的 Hold 不可被一次新預測直接刪掉其 up／釋放責任。
3. 過期 down 不補按；失敗的 down 不產生後續正常 move。up／故障釋放不可因原意圖過期被丟棄。
4. ContactAllocator 使用有限 ID pool，與 Note ID 分開；第一版保守分指，不依賴未實測的共用接觸技巧。
5. 同時命令可組 batch，但先驗證實際 Android 接觸回饋；一個 RPC 內有多個 Touch 不等於已證實同時生效。
6. 指令容量滿、辨識停滯、RPC 逾時或不可恢復錯誤時，進入明確 fault；不靜默丟 up 或繼續猜測接觸狀態。
7. 所有預測 deadline 來自畫面相對運動。不同音符種類的時窗獨立設定；未知的遊戲判定窗不得用 Tap 的 ±80 ms 一概代替。
8. 補償以測得的閉環偏差與分布為依據，保存校準版本。未知來源年齡保留 unknown；不得把 capture_complete 當 produced 或從 FPS 推算絕對延遲。

## 6. 首選觸控候選：Emulator gRPC sendTouch

依倉庫安裝版 proto：座標是顯示座標，identifier 持續識別接觸，pressure 非零維持／0 放開；proto 提到最多 10 個同時接觸，但實際支援數須以 Fixture 驗證。`sendTouch` 非同步排入 emulator main looper，RPC 返回僅是呼叫結果。

- 重用本機 discovery 與認證方式；獨立 input channel，不跟 screenshot RPC 生命周期綁死；不記錄 token。
- 座標映射只在一處進行，整數端點必須落在 `[0,width-1] × [0,height-1]`。目前 `CoordinateTransform` 的矩形端點語義須檢查，不能把 width/height 當有效最後一個像素。
- 以非對稱角落／格點與多指可見路徑驗證旋轉、裁切、縮放。先用 2 指，再增加到需要的數量；未通過不宣稱 10 指可用。
- RPC timeout 後將接觸狀態標為未知，不盲目重送 down。嘗試釋放所有可能 active 的 ID，回報結果；重新開始前重建已知狀態。
- 不使用 NEVER_EXPIRE 作預設；proto 的預設過期長達 120 秒，也不能當作緊急停止保證。測試 runtime 終止／失聯的最佳可行釋放，清楚記錄未能保證的情況。
- `inject`、`inject_batch`、`release_all` 的實作需保持單一 owner。測試 fake server 只證明協定編碼，Android Fixture 才證明實際能力。

## 7. CLI、設定與診斷預覽

設定檔採 JSON（相容現有 Python >=3.10，不為設定格式引入依賴）。提供有說明的 sample profiles，拒絕未知欄位、NaN/Infinity、負值、互斥組合與不合理容量；啟動前驗證，日誌保存去敏後有效設定與雜湊。

規劃命令（下列是待開發介面，不是目前可執行承諾）：

```text
pas run --config configs/avd-observe.json --mode observe
pas touch-bench --config configs/avd-fixture.json
pas closed-loop --config configs/avd-fixture.json --duration-s 60
pas run --config configs/avd-phigros.json --mode assist
```

- `observe` 不建立可注入的 backend；可顯示「原本會做的動作」，不呼叫 sendTouch。`assist` 仍需能力報告匹配和 PLAYING gate，不能用命令列強制略過 UNKNOWN。
- Ctrl+C 撤銷排程並觸發停止；preview 可提供 stop，但不作唯一停止管道。未完成能力驗證的玩法開關拒絕啟用或明示不支援。
- 預覽預設最多 10 Hz，可關閉／調整。顯示來源 frame/epoch、主機駐留時間（非來源 age）、UI 狀態、Note/line IDs、候選區域、預測倒數、contacts、拒絕／故障原因。
- 預覽是 host 視窗，不在 emulator 內畫 overlay；其快照不作 detector 輸入。GUI event loop 不阻塞 scheduler，縮放座標與演算法座標分離。
- 新增 `vision` 可選依賴（例如 numpy/OpenCV）時保持 probe／基礎測試的無 extras 路徑；版本以本機安裝測試結果記錄，不在計畫中臆測最新版。

## 8. 分期工作包與完成條件

### M0 — 固定基線與執行設定

交付：runtime config/schema、profiles、run 目錄與 manifest、capture factory、CLI 骨架、能力介面。記錄 Git revision/dirty、Python／OS／AVD、解析度／方向／設定、後端、實測與未驗證欄位。

完成：舊測試與 CLI 保持可用；無效設定在任何觸控前失敗；observe 可啟停並保留捕捉來源；程序資源能回收。先用 fake source，實機 smoke 再驗證。

### M1 — 接觸計畫、排程與真實後端

交付：ContactPlan／allocator、可替換且有容量的 scheduler owner、gRPC input、release report、fake-server 測試。Scheduler 可在無新 frame 時準時喚醒，但不得越過最後證據的期限。

完成：可重現軌跡與虛擬時鐘驗證更新、過期、取消、同時事件、Hold 維持／移動／結束、Flick 軌跡；包括 dispatch/stop 競態、RPC 失敗／逾時、部分 batch 不確定性、舊 epoch 與長時間容量上限。主機時間測試獨立報告，不把虛擬 clock 的零誤差當性能。

### M2 — Android 觸控能力 Fixture 與即時預覽

交付：獨立 Android Fixture、可重現 build/install 說明、touch-bench、能力報告、預覽。Fixture 顯示各 Android pointer ID、位置／路徑、down/up 計數及取消；Android pointer ID 不假設等於 emulator identifier。

測試包含：Tap／長按、按住 A 同時點 B、A 移動而 B 不動、交錯放開、兩指同時、Flick 多段移動與反向、取消及錯誤恢復、座標格點。短暫事件在畫面留下持續證據，避免低擷取率看不到中間狀態。

最低採樣：每種主要動作至少 30 次，雙指交錯至少 30 組；座標至少 9 個覆蓋畫面的點，端點與旋轉另測。報告失敗數、座標誤差、注入耗時、效果可見延遲。樣本不足只標 smoke，p99 在小樣本下不得誇大。

完成：每個宣稱能力有可見回饋證據；沒有非預期合指或殘留接觸，不能只靠 RPC OK 通關。沒有全數通過的能力維持 disabled。觸控能力測試可用固定測試序列；這是隔離的 Fixture 測試，不得拿來操作遊戲。

### M3 — 真實 pixels-to-touch-to-pixels 閉環

交付：Fixture 移動目標模式、向量化 detector、短期 tracker/predictor、接 runtime 的 closed-loop runner、離線摘要重算器。目標速度／方向／出現間隔由 Fixture 控制，runtime 不讀 seed、進度或目標真值；只用 pixels 找目標和判定線。

畫面用有限持續回饋顯示命中／漏失與觸控位置。可見計數／marker 僅用觀測配對及離線量測，不編碼下一次操作。Fixture 的 Android clock 與 host monotonic 分開保存，未校準不能相減；可用 Android 端相對撞線誤差作離線結果，主機端保留觀察→注入→回饋時間。

標準批次：暖機 10 秒後三批各至少 60 秒且各至少 100 個目標；未達樣本數延長窗口。正常、受控辨識延遲、主機負載分開；慢 consumer、恢復與 500 ms 擷取停頓另做故障批次。記錄 source 實際可見更新率而非只記設定 FPS。

進入 M4 的工程門檻：在事先固定的簡單場景中三批正常命中率各 >=99%、0 非目標誤點、0 gate 關閉後新 down、0 停止後未解釋 active contacts；主機排程絕對誤差 p99 研究目標 <=10 ms，Fixture 撞線絕對誤差 p99 研究目標 <=40 ms。這些是本專案簡單閉環的初始門檻，不是 Phigros 判定常數，測試前寫入 manifest，不能看完結果才改。未達標先定位捕捉／視覺／排程／注入瓶頸。

故障批次不要求相同命中率，但必須停止補按、限制記憶體、可靠取消及可解釋恢復。若實際來源更新率不足，報告測試前提不成立並保留資料；不得以跳過差批次替代通過。

### M4 — Phigros 只觀察與標註基線

依賴 M3 通過。交付：UI classifier、多 Note／多線 Observation、旋轉形狀特徵、關聯追蹤、疊圖與離線評估。由使用者手動進入可用歌曲收集畫面；缺資料時請求一次具體操作，其餘離線工作繼續。

標註：四種 Note、Hold 頭／身／尾、線姿態／可見性、重疊／遮擋、UI 狀態、可觀測／不可觀測、人工事件時間區間。不同歌曲與視覺主題切分，不能隨機拆相鄰 frames；保留未參與調參的歌曲。

首次 baseline 至少三首可用歌曲、每種 Note 至少 100 個人工確認實例；無該類型不得填零錯誤。量測 precision/recall、類型混淆、line 姿態誤差、ID switch、事件計數及時間誤差；不只報 frame accuracy。

完成：每種支援候選有實際標註成績；不支持的場景能拒絕。從此才開發／評估遊戲專用幾何，不能用一張選單截圖宣稱已完成辨識。

### M5 — 有限場景 Tap 自動接手

依賴 M2 Tap 通過、M3 通過、M4 適用場景通過。先限普通模式、可見判定線、可追蹤速度、單 Tap 到獨立多 Tap，未啟用種類明列不支援。

交付：PLAYING 確認、空拍維持、暫停／結算／UNKNOWN 停止、session epoch、動態線局部座標與區域預測、畫面結果分析。啟用前在獨立驗證資料上先確認 UI false arm 為 0、啟用範圍內 Tap precision >=99%、recall >=95%；這是進場門檻，不是泛化保證。

實機至少三個已可用簡單譜面片段／歌曲各三次；記錄可見配置、接手與停止延遲、誤點／漏點、判定統計與獨立錯誤審查。不以曲名載入觸控序列。第一顆來不及接手須明列漏接，不先盲目 arm。

### M6 — Hold／Drag／Flick、多線與複雜運動

依序啟用 Drag → Hold → Flick，再做混合與多指衝突（可依資料調整順序，但不可略過個別能力驗證）。每類動作有自己的 evidence、期限與狀態機：Hold 不因一次漏辨立即當結束；短期延續只在有界可解釋證據內，逾期釋放；Flick 測速度、方向反轉與重新觸發；Drag 避免多餘 down 誤觸 Tap。

擴充多線／旋轉／反向／變速、重疊與可短期預測的遮擋。接觸共用／換指為通過專項測試後的優化。完全不可觀測、演出混淆或未校準模式維持不支援。

完成：每一啟用能力有獨立遊戲證據與混合場景回歸；ContactPlan 可以解釋每根手指為何存在／移動／放開。報告每類的錯誤，不以總 ACC 掩蓋失敗類型。

### M7 — v1 整合與交付

交付：單一 CLI 流程、profiles、預覽、故障排查、已驗證能力與已知限制、可重算報告、資料 manifest。普通支援場景持續運作至少 30 分鐘，檢查記憶體與隊列上限；主機正常與負載條件分開報告。

測試退出、perception 停滯、capture child crash、input timeout、journal 寫入失敗、AVD 中斷、解析度／方向變化、暫停／恢復與重新開局。恢復必須新 epoch、fresh evidence、重新確認狀態；不能補放舊動作。

v1 完成的定義：支援範圍內可重現的畫面到操作閉環、可靠停止、每次操作可追溯。全曲 AP、全難度、課題模式、解謎解鎖、自動導覽與完整 GUI 不屬於本版承諾。

## 9. 機器學習的決策點

M4 有標註基線後才決定是否加入小型偵測／分割模型。若失敗主要是特效、背景、形狀泛化且幾何／觸控已穩定，開一個限定實驗；若失敗是錯時或指令狀態，不用模型掩蓋。

模型只替換 perception。比較未見歌曲的 event precision/recall、UI 誤接手、姿態誤差、推論 p50/p95/p99/max 與完整閉環結果；在固定硬體與同樣像素輸入下比較。訓練與評估分割、標註、權重及設定保存雜湊；不要讓歌名／時間查表洩漏進決策。模型與 runtime dependency 可選，規則基線仍可重現。

訓練資料不足時提供收集與標註工具，不隨便訓練一個模型宣稱泛化。外部付費算力或上傳私人畫面不是本次預設工作；本機小型實驗可按證據推進。

## 10. 日誌、驗證與性能報告

每個 run 目錄：去敏 config、環境／revision manifest、JSONL、summary、可選診斷 PNG／限額故障片段。設定磁碟／記憶體上限；熱路徑不逐張同步存 PNG。

Telemetry 使用有上限的 writer queue。關鍵控制／觸控事件不能靜默丟失：容量不足時停止新的 assist，保留 drop/fault 計數與緊急停止能力，flush 有期限。debug 級逐 frame 明細可抽樣，但摘要明示樣本截斷。worker 不因 logger 持鎖卡住釋放。

時間線至少包含：capture_complete、pixels_ready、perception start/end、prediction complete、plan accepted、scheduled、inject start/return、effect first observed、gate revoked、release start/return。預測誤差、喚醒／排程誤差、呼叫耗時、畫面回饋延遲分開報告。

所有分布保存 n、p50/p95/p99/max、失敗與丟棄；jitter 明確採用排程誤差的 p95−p5，另附絕對誤差。三批不可只挑最佳批，暖機／正式窗口分開，同一條 host 時間線；p99 小樣本限制明列。Fixture Android 真值只作離線對照，不滲入 runtime。

測試層次：純狀態／虛擬時間 → 假 RPC 整合 → 真主機時間 → Android 可見回饋 → 真實閉環 → 遊戲有限場景。回歸優先測 invariant 和真實故障，不為薄 wrapper 堆鏡像測試。

## 11. 建議檔案布局與變更批次

```text
src/pas/config.py                  設定與 profile 驗證
src/pas/runtime.py                 執行與資源所有權
src/pas/runtime_contracts.py       新契約（或合理拆分）
src/pas/input_grpc.py              gRPC 注入、有限接觸狀態
src/pas/action_planner.py          意圖到接觸計畫
src/pas/contact_scheduler.py       單一 owner、deadline、gate/epoch
src/pas/preview.py                 降頻診斷視窗
src/pas/perception/                Fixture／後續遊戲辨識
src/pas/tracking.py                多目標／線關聯
src/pas/prediction.py              動態線與區域預測
configs/                          JSON sample profiles
fixtures/touch_android/            獨立測試 APK
scripts/build_touch_fixture.py    重現本機 SDK build
tests/test_runtime*.py             生命週期／門控／設定
tests/test_input_grpc.py           協定、失敗、timeout
tests/test_contact_scheduler.py    動作與競態／容量 invariant
```

檔名是分工建議，不要求空殼檔案先全部建立。每批實作、測試、文件一起落地；已有未提交的研究文件屬本次工作，保留，不回退或覆蓋其他變更。需要建立 branch 時用 `codex/` 前綴，不自動推送或建立外部發布。

## 12. 交接指示與狀態追蹤

指定開發者先讀本計畫與 AGENTS 所需文件，然後實作 **M0–M2 的第一批核心**。其中 M2 實機通過前，不啟用 Phigros 觸控；可直接建置與安裝自己的 Fixture 做驗證。先讓可測最小垂直流程工作，再補已列故障處理，不先寫完整框架空殼。

遇到參數／技術細節依本計畫與量測做合理決定並記錄。只有缺少必要使用者操作、未解鎖資料或明確外部限制才提出具體問題；已授權的離線實作、測試和 Fixture 驗證繼續。每批回報實作、命令、樣本與限制；不得把 skipped 當 passed。第一批完成後按 M3→M7 門檻繼續安排工作包。

| 階段 | 規劃時狀態 | 完成證據 |
| --- | --- | --- |
| M0 | 已實作／驗證 | 嚴格 JSON profiles、去敏 run manifest、process/payload capture factory、`run --mode observe`、降頻主機預覽；fake 21 frames／0.5 s、實機靜態畫面 smoke、程序正常回收。2026-09-25 另發現 emulator gRPC 曾在同尺寸回傳倒轉畫面，因此 profile 增加來源方向檢查；恢復記錄見量測文件。`assist` 明確拒絕。舊 CLI 語義保留。 |
| M1 | 已實作／驗證核心，實際失聯仍需持續故障測試 | 有限 ContactPlan、ID allocator、可替換 revision、單 owner 排程、epoch／證據／遲到檢查、gRPC `sendTouch`、逾時後鎖定、release report、緊急全 ID 釋放。虛擬時鐘與假 gRPC server 故障回歸；本次 69 tests 全通過。實機 owner 計時與釋放數據見量測紀錄。 |
| M2 | Fixture 主要能力已達最低樣本；其他旋轉／真實 RPC deadline 逾時未驗 | 獨立 APK；Tap／Hold／Move／反向 Flick／交錯雙指／近同時雙指各 30 組，取消 30 組，單 RPC 雙指 batch 30 組，全部由 Android 可見計數驗證且零殘留；九點覆蓋、四角各一次 smoke、90° 映射。真實 channel 斷線後一組緊急釋放可見成功；真實 RPC deadline 逾時嘗試未重現，fake server 逾時已驗。其他裝置、解析度、旋轉和程序猝死後釋放未驗。 |
| M3 | 等待 M2 | 待真實 Fixture 閉環驗收 |
| M4 | 等待 M3 | 待遊戲畫面與標註資料 |
| M5–M6 | 等待相應視覺／觸控門檻 | 待有限場景及每類音符實測 |
| M7 | 等待支援能力整合 | 待長時間及故障回歸 |

進入 M3 前還需把簡單移動目標 Fixture、只讀 pixels 的向量化 detector／短期 tracker／幾何 predictor 接到 observe runtime，建立 PLAYING 以外的 Fixture gate 與新 epoch、注入結果回讀配對、固定三批測試 manifest 和離線重算器。M2 的 `getScreenshot` 輪詢只能證明觸控能力與可見延遲；正式 M3 應使用已選定的 process＋payload 容量 1 串流。沒有上述閉環及受控負載／停頓批次前，不宣稱觸控已適用 Phigros，也不啟用 `assist`。
