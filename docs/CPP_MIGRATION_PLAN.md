# 全 C++ 遷移與擷取候選開發計畫

日期：2026-09-25。狀態：T0 來源已逐 hash 核對並凍結；T1–T5 正依此計畫實作與驗收，逐項實證狀態另見 [C++ 驗收矩陣](CPP_PARITY_MATRIX.md)。新基線後的性能數值門檻仍須詢問使用者。盤點依據見 [盤點報告](CPP_MIGRATION_AUDIT.md) 與 [逐檔 hash](CPP_MIGRATION_INVENTORY.json)。本計畫中的目標目錄、命令、類別與測試要求是開發規格；不能僅憑此文件視為已完成。

## 1. 使用者決策與適用範圍

| ID | 決策 | 狀態 |
| --- | --- | --- |
| D1 | 所有自有正式執行邏輯採 C++，包括主機 runtime、測試、分析、批次工具、Android Fixture；不採 Python hybrid | 已確認 |
| D2 | 新任務只完成既有功能 C++ 遷移、缺陷修正與驗收（T0–T5）；五候選新增開發與比較另開下一任務（T6–T7） | 已確認 |
| D3 | Windows x64、C++20、MSVC、CMake、vcpkg manifest；可安裝缺少的工具鏈 | 已確認 |
| D4 | 正式核心採單一程序、多個專用執行緒；取消既有獨立擷取 child／IPC 作為正式要求 | 已確認 |
| D5 | 舊 Python／Java／HTML 凍結於 legacy 作對照；保留 CLI 語義與舊日誌重算能力；正式流程不依賴 Python | 已確認 |
| D6 | 40–57 Hz 為正常來源範圍；先固定測法並取得新基線，再把延遲／掉幀門檻交使用者決定 | 已確認 |
| D7 | 第三方依賴允許原語言，包括 NDK C glue、FFmpeg C、scrcpy 官方 Java server；自有正式邏輯全 C++ | 已確認 |
| D8 | CLI＋Win32／D3D11 輕量診斷預覽；完整 GUI 後置 | 已確認 |
| D9 | AVD 5 vCPU、8 GB RAM，其餘固定；先核對實際套用狀態 | 已核對：guest online processors 5，`MemTotal` 8,130,828 KiB；見 C++ 驗收紀錄 |
| D10 | 由新的 GPT-6 Sol、xhigh 任務開發，在隔離 worktree 工作 | 已指定 |

本計畫覆蓋舊文件的 Python／混合語言、process runtime、必須接近 59–60 Hz，以及「只有既有 gRPC 失敗才探索候選」的未來工作限制。歷史測試結果、AGENTS 的 pixels-only／最新 frame／monotonic／閉環先行原則仍有效。

「全 C++」不要求重寫 OS、第三方庫、編譯器或 Android framework。允許 CMake、JSON/XML、vcpkg manifest、必要的建置設定；自有測試編排與分析不可偷偷委派 Python／JavaScript。若工具鏈間接使用外部 runtime，記錄 build-time 依賴，與正式程式的 runtime 依賴分開。

本次範圍終點 T5：遷移既有能力、修正既有契約缺陷與驗收。既有 gRPC payload／ADB 診斷／MMAP 診斷仍須移植；WGC／DXGI／scrcpy 新後端及完整五候選比較不在本次範圍。本任務不開發新 Phigros Note／判定線分類、自動選曲或遊戲 assist；既有簡單合成 detector／tracker／predictor 必須移植。新的真實移動目標 M3 閉環在後續擷取候選評估後的階段。

## 2. 遷移來源與凍結

1. 讀取本計畫、盤點、README／ARCHITECTURE／ROADMAP／AGENTS；取得本次提供的來源快照。新 worktree 只含 HEAD 時不得直接移植舊提交。
2. 來源以桌面工作區為主，四個重測修正版程式與低負載報告採使用者提供的 `C:/Users/wurre/Documents/Codex/2026-09-25/new-chat/outputs/recovered-8bd4`。原工作區消失後由封存紀錄重建；本次逐hash比對66／70檔相同，四個關鍵程式及報告全部符合消失前hash。詳見 [核對清單](CPP_RECOVERY_VERIFICATION.json) 與 [復原交接紀錄](CPP_RETEST_RECOVERY_NOTES.md)。其餘文件使用桌面最新計畫，不用重建版整包覆蓋。raw164檔仍未恢復，不假稱已重算消失資料。來源變動先列diff並避免覆盖未知並行工作。
3. 建立 `legacy/python/`、`legacy/android-java/`、`legacy/web-fixture/` 與 `legacy/README.md`；保留程式、測試、設定、版本與執行说明。凍結不代表它們全部通過驗收。
4. 不刪原工作區，不移除 ignored measurements；建立 evidence index，核對原始資料存在與 hash。必要檔案做副本保管後才可考慮工作區退役，這不在本次開發的自動清理範圍。
5. 過渡期可以執行凍結 Python 作 differential test；這是驗證工具，不是最終 C++ 命令的實作依賴。最終驗收必須在無 Python PATH／套件的乾淨程序環境執行正式命令。
6. Git 分批提交於 `codex/` 分支，說明 source snapshot；不得把原目錄未提交檔誤認為可丟棄內容。不要自動 merge 回主目錄，不上傳量測／帳密。

## 3. 建置、依賴與目錄

建議目錄（可調檔名，不可省略責任）：

```text
CMakeLists.txt / CMakePresets.json / vcpkg.json
apps/pas/                    CLI、campaign、analyze
include/pas/                 版本化資料與模組介面
src/core/                    clock、config、frame、latest、status
src/platform/windows/       QPC、process launch、handle／timer／window
src/capture/                 grpc payload、mmap、wgc、dxgi、scrcpy
src/runtime/                 supervisor、session、bounded telemetry
src/vision/                  已有簡單目標 detector／tracker／predictor
src/input/                   fake、grpc、mapping、capability
src/scheduling/              plans、single owner、revision／epoch
src/preview/                 Win32／D3D11
src/bench/ / src/analysis/   fixtures、矩陣、重算與報表
tests/unit/ / integration/ / acceptance/
fixtures/android/capture/ / touch/ / common/
proto/                      原 emulator proto、C++ generated build outputs
legacy/ / docs/ / configs/
```

- C++20，禁用未確認可用的 C++23 特性；MSVC `/std:c++20 /permissive- /W4 /EHsc`，自有程式警告處理，第三方警告獨立。Release／Debug／可用的 ASan preset；性能測試只採固定 Release preset。
- CMake presets＋vcpkg manifest pin 到實際 commit；記錄 MSVC、Windows SDK、CMake、Ninja／generator、NDK、protoc／grpc codegen 與所有依賴版本。不可只寫 latest；同一工具鏈編译 ABI 相容的 protobuf／gRPC。
- 原則採 gRPC C++／Protobuf、CLI11、nlohmann-json、GoogleTest；PNG 可用 Windows WIC，hash 可用 BCrypt。具體版本由 configure 驗證後鎖定；若替換依賴會改授權／支援平台／接口，先回報決策。
- 五候選階段加入 FFmpeg 的必要解碼／像素轉換組件與固定版本 scrcpy server；不預設所有 Android encoder 或 Windows hardware decoder 可用。保留 licenses／third-party notices，記錄 build features。
- Win32／D3D11 預覽不引入 Qt。OpenCV、模型 runtime 不因「以後可能用」就加入；遷移簡單 detector 可先直接 C++ 實作。
- Android NativeActivity＋NDK C++、x86_64，使用 SDK／NDK 提供的生命周期／input API；優先 C++ EGL／GLES 渲染與 AChoreographer 對齊刷新。自有邏輯不寫 Java。Manifest 指向 framework NativeActivity；第三方 native_app_glue 原版可保留。
- Fixture APK build／package／align／sign 可由 CMake targets 呼叫 SDK 工具；必要自有輔助邏輯寫 C++。不寫 Python 打包腳本；SDK 簽章工具的 JVM 是外部 build-time dependency，明列。

## 4. 單程序架構與時間契約

```text
Capture backend callback／worker → 最新有效 frame → 簡單 perception worker
                                                     ↓
                                             bounded intent mailbox
                                                     ↓
                                         scheduler／touch 唯一 owner
Runtime／Session supervisor ──────────────── revoke／epoch／stop
Preview、Telemetry、Resource sampler ← 只讀且有界診斷
```

- 主執行緒持有 Win32 message loop／UI；capture worker、perception worker、scheduler/input owner、telemetry writer 各有清楚 owner。gRPC／FFmpeg 庫內部 threads 要盤點，不宣稱全程只有這幾條 threads。
- Capture callback 只作必要驗證、擷取時間戳、取得完整像素與發布；不可做重算、同步檔案寫入、預覽縮放或觸控 RPC。
- 用 Windows QPC 作唯一 host monotonic domain，保存 QPC frequency／run epoch 與整數換算；所有執行緒共用 Clock interface。Windows capture timestamp 在核對時域／單位後才映射。來源 Unix、Android frame time、FFmpeg PTS 各標 domain，未校準時不能直接相減。
- 保留 `capture_complete`、`pixels_ready`、`published`、`consume`、`recognition_complete`、`scheduled`、`injection_start/return`；加 backend-specific 原始時間點。`produced` 可為 unknown，不把收到時間改名為產生時間。
- 所有時間戳以 64-bit integer 序列化，不經 double；算 durations 時檢查 overflow／負值。新增 `schema_version`／`clock_domain`／backend timestamp semantics。
- 所有跨執行緒訊息帶 run／epoch／capture generation／frame sequence／geometry version；新 epoch 明確清理舊狀態。

## 5. Frame、緩衝與像素

- 邏輯只公開一個最新 frame，不建立逐張待處理 FIFO。可用固定數量物理 buffer slots 保證讀者生命週期；明確列出 max buffers／max bytes／max readers。
- 已交付 frame 在 reader lease 結束前不可被寫入。`shared_ptr` 不能當作無上限持有許可；讀者超期／pool 耗盡要有丟棄／fault 計數，不能臨時無限 allocate。
- 先用可證明的短鎖／ownership 建立正確性；不為「無鎖」採未驗證 memory ordering。縮放與格式轉換不可持有發布鎖。
- FrameView 明列 width、height、stride、RGB/BGRA/RGBA 等格式、orientation／crop transform、像素一致性資格。提供完整 CPU RGB24 相容視圖；後端原格式與 CPU 正規化成本分開計時。
- GPU surface 必須在後端 frame lease 有效時完成複製／GPU 同步；關閉 frame pool 後不得仍引用其 texture。先做明確所有權，再研究 GPU buffer 轉交。
- callback 次數、來源序號、Fixture 可見 counter、不同 pixels、consumer skip 分開；桌面動了滑鼠或重複同圖不能冒充遊戲新畫面。一般遊戲的完全相同 pixels 不能簡單等同擷取故障。
- 先保留源端整圖；ROI／降解析度是顯式新 profile，須重新驗證可見計數與幾何，不在比較中偷偷減少工作量。

## 6. Session、停止與故障

- observe 不建立 input backend，assist 明確拒絕。Fixture touch 由前景 package＋可見簽名雙檢查允許；capture-only candidate 不可取得觸控控制。
- READY／WARMUP／MEASURING／STOPPING、未知／靜態 degraded、inactive、stream fault 分開。健康 probe 返回後重新檢查最新有效 frame／inactive／stop，保留已修正競態的語義。
- 單程序不再有 child heartbeat／IPC cleanup；不能宣稱仍具 crash isolation。C++ exception／library callback error 轉成明確 fault；memory corruption／整體 crash 的釋放結果可能未知。
- 有期限的 stop：先 revoke／禁止新 down，再取消計畫與嘗試 release，取消 capture RPC／socket／frame pool callback，等待 in-flight ownership 結束，再銷毀資源及 flush。
- 不使用 TerminateThread 或 detach 還引用已銷毀資源的 worker；所有阻塞操作要有 cancel／timeout 策略。無法 cooperative 停止時記錄 timeout／unknown，必要 fail-stop 整個本程序，不假稱乾淨停止或觸控已放開。
- scheduler 不等待預覽或 writer 的鎖。debug log 可有界丟棄並計數；重要控制／觸控紀錄無法保存時撤銷操作資格，保留 fault／緊急釋放結果。停機 flush 有期限。
- 認證沿用明確序號與本機 discovery，不把 token 放 CLI／manifest／錯誤文字。公共 gRPC capture/input channels 生命週期分離。

## 7. 既有排程與觸控缺陷必修

以 `ACCEPTANCE_20260925.md` 的具名重現作回歸起點；不是把舊錯誤翻譯成 C++。

1. gate／plan 證據到期均列入 owner 下一次喚醒期限，即使下一個動作尚未到期也撤銷資格並釋放 Hold。邊界定義為 `now >= deadline` 到期；對恰好到期測試。
2. 每次 dispatch 與 active contact 維持獨立驗證 gate 與 plan 證據；新 UI frame 不刷新舊 target。revision 的延續證據必須有界、可追溯。
3. completed intent 不因新 revision 重觸。設計 key／epoch／birth identity 與遲到 horizon；有界 tombstone 淘汰不能復活舊意圖。以單調 intent identity／拒收 watermark 或等價可證明方案處理，寫清楚契約與測試，不只調大 cache。
4. 注入與 stop 必須有線性化點；已 down 的 revision 保留 contact ID／釋放責任。down timeout 不重試；結果未知則 input faulted，後續 move 不執行。
5. Touch Fixture 新版以 pixels 保留每 pointer、每段位置與階段的有界可讀紀錄，含 sequence／overflow／schema；checker 驗證保持指漂移、移動指路徑、反向、交錯 up。歷史事件合併要明示；證據 overflow 或缺段不能判成功。
6. 加錯誤路徑負例：兩指同漂、反向錯誤、missing up、pointer 混淆、漏段都必須失敗。更新 capability report schema，舊 APK／mapping／ABI 的報告不得自動 arm。

## 8. Android Fixture 遷移

- 使用新的 package／版本識別，保留舊 APK 可作 A/B 對照；避免覆蓋舊測試來源後失去可重现性。
- Capture Fixture v2 以可見 metadata 編碼 frame counter、區塊校驗／frame identity，非對稱角落、移動矩形、細線與色塊；可明確選 40／48／57 Hz 目標場景及隨裝置刷新場景。以時間推進動畫位置，不能只用 frame counter 推進導致負載時移速改变。
- 設定目標 Hz 不等於實際 Hz。實際繪製與可見更新分開量測；fixture-only renderer 統計可用於離線評估，不能接入遊戲決策。
- Touch Fixture 使用 NDK pointer ID／action index 與 cancel lifecycle，保留 Android 觀察到的事件，不把主機送出序列畫回去當接收證據。
- 新舊 Fixture 的 renderer 不同；先用固定 legacy APK 比較 Python／C++ host，再用固定 C++ Fixture 比較全部新後端，避免同時改語言、renderer、AVD 後歸因錯誤。
- Android 時間戳只屬測試來源 domain；可見編碼與校準需另驗證，不能直接與 host QPC 相減。多區塊一致性測試可發現 tearing，但沒有發現不等於證明 MMAP 一致性。

## 9. CLI、設定與舊資料相容

- `pas.exe` 對應原 `python -m pas.cli`；保留 11 個命令的用途，具體 migration table 要逐 flag 審核。
- 新 profile schema 明確 `execution=thread`／native single process。可由 `pas config migrate` 轉 schema 1，但輸出新檔、不覆寫舊 profile；process flags 明確告知已退役，不做 silent substitution。
- `offline-capture-bench` 新 native 版本是 fake source 的單程序測試，與 legacy process 量測種類分开；同名保留用途不能沿用錯的 CPU／IPC 欄位。
- `pas analyze` 以 streaming／有界記憶體重算 JSONL，保留半開窗口、warmup 邊界、counter wrap、分層 drops、CPU 取樣 brackets、缺值 null、實測／推算 pause end。
- 對舊樣本同樣公式與 percentile 定義應產生相同結果；metadata／精度容差事先列出。不能把舊 57–63 Hz pass 欄位改寫為新正常範圍而宣稱歷史通過。
- source snapshot、測試輸入與原始日誌僅用於 offline command；正式 runtime 不接受錄影／譜面／舊觸控作決策。

## 10. 五種擷取路徑（D2 決定本任務是否執行）

順序：C++ gRPC payload 基線 → WGC → DXGI → scrcpy → MMAP 可行性收尾。ADB PNG 僅診斷基線，不算第六條高頻候選。

| 路徑 | 實作與驗證重點 | 禁止誤判 |
| --- | --- | --- |
| gRPC payload | 舊 proto、認證、首張／動態／靜態、取消；比較 RGB888／RGBA8888、接收／解析／轉換／copy 成本 | payload 接收時間不是產生時間；相對 lag guard 不是絕對新鮮度 |
| WGC | 指定 emulator window、D3D11 free-threaded frame pool、CPU readback、尺寸變更、DPI／黑邊、遮擋／最小化 | OS capture QPC 不是 Android render time；不可假定最小化仍繪製 |
| DXGI | 指定 monitor／adapter、Desktop Duplication、裁切 transform、旋轉／多螢幕／access lost | 桌面遮擋不能變成有效遊戲 frame；desktop updates 不等於目標更新 |
| scrcpy | 官方固定版 server、control/audio disabled、直接讀串流＋FFmpeg 解碼；先 H.264，其他 codec 作有界選項 | 不任意丟 inter-frame 壓縮封包；解碼依賴與所有 queue 都要有界；raw_stream 不代表 RGB raw pixels |
| MMAP | 相符 emulator source／協定研究、生產端 ownership／fence／ack 證據、metadata／pixels 對應與 cleanup | reader 自己加鎖、CRC、連讀相同都不能修復未同步 writer；沒有證據就 diagnostic-only |

WGC／DXGI 可能需要獨立 emulator window。若目前嵌在 Android Studio，列出可逆啟動方式及其設定差異，先讓使用者決定是否改啟動形式；不能暗中改 GPU、解析度、顯示更新率或重啟其他工作負載。

各候選結果只能是 implemented-and-tested／diagnostic-only／unsupported-with-evidence／failed-with-reproduction／waiting-user-decision。完成研究不等於每條都可進 Session。重建整套 Emulator 以修 MMAP producer 不在默認範圍，若必要另提決策。

## 11. 測法與未定數值門檻

### 11.1 已固定規則

- 來源 40–57 Hz 屬正常；>57 Hz 不判異常，<40 Hz 保留並分類低來源條件。正常範圍不是自動通過性能的條件。
- AVD 5 vCPU／8192 MiB 記憶體需核對 unit／實際設定，其餘用 preflight 固定。SDK／GPU mode／APK／尺寸／縮放／方向／電源／監視器 refresh／encoder／decoder／codec 皆入 manifest。
- 第一批前固定測試順序、樣本窗、負載、分析版本與 source hash。每後端至少 3 批正常（10 s 暖機＋60 s 正式），串行且平衡順序。無預覽作主比較，預覽 on 另測。
- 壓力場景：50 ms slow consumer、100 ms consumer 後恢復、500 ms receiver pause、可適用的相對保護、受控 CPU／記憶體負載；GPU 負載用獨立明確場景。C++ 無 GIL，原 parent-gil 必須標為 legacy，不以同名 CPU stress 假稱完全等價。
- 同條件 10 分鐘單後端穩定性；選定正式候選再延伸 30 分鐘，所有結果保留，不挑最佳批次。長測實際時長先入 manifest。
- 預覽關閉、來源靜態、視窗 resize／遮擋／最小化、斷線／重連、停止／反覆啟動、超容量／invalid pixels／timestamp reset 都有實測或可重現替代；真實未重現項明列，fake 不冒充 emulator 驗收。
- source callback、來源可見 counter、不同交付、source skips、publish drops、consumer skips 分開；來源率由樣本跨度推估不能假稱逐 frame 真值。
- 硬條件：有效幾何／像素／生命週期、不持續積壓、不失去取消／釋放責任、不把 stale／unknown frame 判新鮮、report 可由 raw 重算。數值性能另列。

### 11.2 使用者尚需在新基線後決定

第一批 C++＋5 核／8 GB 基線完成後，開發任務提出：到達 p95／p99、主機駐留 p99、最大無圖空窗、跟隨來源比例、正常及負載失敗容許值。附樣本／分布／可達成本與建議，不自行套用舊 60 Hz 門檻或預設全部合格。

使用者回覆前，可以繼續後端實作、正確性／故障驗證與預先定義的資料蒐集；不能宣告性能通關或最終選型。不以等待數值決策為由放棄其他獨立工作。

比較來源 max/min 差異與各批分布；可沿用 1.05 作「配對來源近似相同」的研究標籤，不以此否定 40–57 Hz 的真實使用範圍。差異超出時不宣稱速度排名，可補事先定義的配對輪次，禁止無限重跑挑數字。

## 12. 工作包與交付門檻

| 工作包 | 實作 | 驗證／交付 |
| --- | --- | --- |
| T0 來源凍結 | 整理雙工作區增量、legacy、evidence index、工具鏈 | hash 一致；未覆寫原目錄；可追溯新旧來源；列缺失資料 |
| T1 C++ 基礎 | CMake／依賴、Clock、Frame、latest、config、CLI、telemetry、analyze | Debug／Release build、單元測試、舊 JSONL golden 重算、無 Python 命令可執行 |
| T2 既有擷取／runtime | gRPC payload、ADB 與 MMAP 診斷、單程序 worker、Session、Win32 預覽 | loopback 真 RPC、取消／probe race／stale、observe 實機、固定 APK 新舊 host 比較；MMAP 不進 Session |
| T3 既有合成／觸控 | detector／tracker／predictor、plans／scheduler 修正、fake／grpc input | synthetic deterministic、三 P1 回歸、停止與 unknown state、mapping／batch tests |
| T4 C++ Android Fixture | native capture／touch、建置包裝、像素格式、逐指路徑 | 新 APK 版本與 hash、同時／獨立／反向／交錯／取消、負例必敗、各能力正式樣本 |
| T5 遷移驗收 | CLI／舊 schema、量測／分析／工具覆蓋、文件整理 | 功能矩陣每項有證據或決策性退役；無 Python 正式依賴；未通過能力不宣稱完成 |
| T6 五候選開發 | 依第 10 節，全部接入同一 Frame／bench 契約 | 每候選正常／壓力／故障／長測與可行性結論；所有數據保留 |
| T7 比較與交接 | 新門檻使用者決策、候選選型建議、剩餘限制 | 使用者確認數值後才性能驗收；更新 roadmap，下一步才 M3 真實簡單目標閉環 |

T2 的 native gRPC 新環境基線已足以發出第一次數值門檻決策；不必等待 T6 全部結束。T3 是移植既有測試能力，不是提前做 Phigros 或新的 M3。

使用者已選僅遷移，任務終點 T5；T6–T7 保留為下一任務計畫，不在本次實作。T0–T5 需持續完成，不在 T1 scaffold 或 T2 可取圖時宣稱完成。性能數值等待使用者確認時，完整呈現已完成的功能／資料與尚未驗收項，不把等待當作性能通過。

## 13. 測試映射與完成定義

- 建立 `docs/CPP_PARITY_MATRIX.md`：每個原模組、11 CLI、11 scripts、8 tests 的舊測試案例 → 新對應 → 狀態／執行命令／證據。退役只允許已確認架構差異（如 process IPC），功能責任仍需覆蓋。
- GoogleTest／CTest 跑 fake clock 和 loopback RPC；不要用不穩定 wall-time assertions 代替確定性 owner／deadline 測試。資源失效、invalid sizes、integer overflow、malformed JSON／proto、取消期間 callback 均有負例。
- ASan／可用分析工具驗證生命週期；Windows 工具不支援的 sanitizer 明列，不寫成已通過。不要用 sanitized build 做性能結論。
- Android 能力至少每種 30 組，逐段／逐 ID checker，保留失敗與 overflow；四角及斷線單次只叫 smoke。新 Fixture 能力 report 不沿用舊 APK 的合格標籤。
- 用固定測試輸入驗證幾何／預測容差與觸控序列；float 容差必須解釋，不使用 fast-math 改動已驗證時序語義。
- `pas --help`、`probe`、離線／合成、舊日誌分析、observe、Fixture 能力命令均有可執行證據；shell 只作命令入口，主要邏輯不得藏在 Python fallback。
- 更新 README／ARCHITECTURE／ROADMAP／MAIN_PROGRAM_DEVELOPMENT_PLAN，將歷史與目前分開。移除正式命令的 pip／Python requirements，legacy 說明保留。
- 最终摘要：完成工作包、commit、build／test 命令、樣本與分布、原始資料絕對路徑、保留／退役功能、未重現 fault、MMAP／source age 限制、下一階段。不能只報「測試全綠」。

## 14. 需回到使用者的決策

以下會改變已同意範圍，需在獨立工作繼續時具體詢問：新性能門檻；WGC／DXGI 所需 emulator 啟動形式變更；改 CPU／RAM 以外固定設定；改單程序架構；新 GUI／平台；引入自有非 C++ 邏輯；為 MMAP fork／重建 Emulator；無法保留的 CLI／歷史分析能力。普通類別命名、檔案拆分、已列依賴版本鎖定與必要錯誤修正由開發任務執行並記錄。

不得為決策中的待定項默認選答案，也不反覆詢問本表已確認選項。所有新問題附具體取捨與推薦；完成可獨立進行的工作再回報。

## 15. 技術參考

- [gRPC C++](https://grpc.io/docs/languages/cpp/quickstart/)：client／server／codegen。
- [vcpkg manifest](https://learn.microsoft.com/en-us/vcpkg/concepts/manifest-mode)：版本化依賴；[CMake 整合](https://learn.microsoft.com/en-us/vcpkg/consume/manifest-mode)。
- [Windows capture](https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/screen-capture)：frame pool、surface、QPC timestamp 與尺寸／device lost。
- [Desktop Duplication](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api)：desktop surface／dirty regions／rotation。
- [scrcpy develop](https://github.com/Genymobile/scrcpy/blob/master/doc/develop.md)：串流／解碼／官方 server；實作必須 pin tag／commit，不引用浮動 master 作版本。
- [NDK NativeActivity](https://developer.android.com/ndk/samples/sample_na)：native lifecycle／input；[NDK concepts](https://developer.android.com/ndk/guides/concepts)。
- Emulator 以倉庫 `proto/emulator_controller.proto` 與安裝版雜湊為準，不用不相符的網路 proto 覆蓋。
