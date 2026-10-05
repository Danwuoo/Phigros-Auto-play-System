# Chapter Legacy 全曲 IN／zero miss：雲端重新盤點與多分支研究

日期：2026-10-05（UTC；既有本機紀錄多為 Asia/Taipei）。

**結論：現有系統已有完整 pixels→touch 架構與歷史遊玩證據，但沒有全章節 IN 解鎖或完整 IN Miss=0 的驗收證據。現在應優先讓「可重現實驗能回答失效原因」成立，再以同一通用候選逐曲擴展；不應先重寫整套程式、堆模型或重開擷取選型。**

本次使用新授權進行有界雲端研究，已實際讀程式、核 JSON、做隔離 C++20 冷實驗。未改正式策略、既有測試或程序控制；未啟動遊戲／模擬器／真觸控、訓練、付費運算或使用使用者電腦。本地研究 commit 不等於 push、PR 或 live-ready。

## 1. 固定基準與閱讀入口

- 本次 Git 基準：[`74e54437d4a3ad2b2bd1a3b09312211a92f2359e`](https://github.com/Danwuoo/Phigros-Auto-play-System/commit/74e54437d4a3ad2b2bd1a3b09312211a92f2359e)，tree `e6880d268f6eaadc43717be0bdb3942c7b9a5c4a`。
- 10/5 三次變更為文件盤點、離線研究/BVI source 保全及文件搬移。比對 `f83c7ea…` 到本次基準，`src/`、`include/`、`configs/`、根 CMakeLists／presets 沒有差異；**研究工具增加不等於正式遊玩策略變好**。
- 正式 main50：observer50／planner27／diagnostics11，comparison／donor，historical live0。C36h tint1：另有 frozen source/binary 的 behavioural／experimental baseline；同號 37/19 不等於歷史 main 的 37/19。
- 歷史 [10/5 停止時盤點](../../status/DEVELOPMENT_STATUS_AND_ISSUES_20261005.md) 保持原文，不把本次授權回填成舊工作包已可續跑，也不改其 STOP。
- 雲端環境：Debian 13 x86_64、g++ 14.2.0；未見 Windows／MSVC、CMake、PowerShell、ADB 或 emulator 可執行檔。Git 643 個 tracked 檔，`measurements/` 與 `out/` 未隨 Git 提供。[基準收據](evidence/baseline-audit.txt)

| 分支 | 詳細交付 |
|---|---|
| 現況、建置與測試阻塞 | [BUILD_TEST_AUDIT.md](BUILD_TEST_AUDIT.md) |
| 視覺、關聯、時間校準與延遲 | [VISION_TIMING_RESEARCH.md](VISION_TIMING_RESEARCH.md) |
| 多指排程、Hold／Drag／Flick／旋轉 | [TOUCH_SCHEDULER_RESEARCH.md](TOUCH_SCHEDULER_RESEARCH.md) |
| 全曲分母、IN 解鎖與端到端驗收 | [LEGACY_ACCEPTANCE_RESEARCH.md](LEGACY_ACCEPTANCE_RESEARCH.md) |
| 哪些 ignored 檔案值得補 | [IGNORED_INPUTS.md](IGNORED_INPUTS.md) |
| 本輪執行、獨立重跑與交付核驗 | [VALIDATION.md](VALIDATION.md) |

## 2. 產品目標與證據層級

### 2.1 分母和成功判準

令 `N` 為**已凍結遊戲版本、當前 Chapter Legacy 完整列表**的曲目數。現在 `N = unknown`；20 個歷史曲名只是已記錄集合，不是現行完整分母。

兩個目標分開計：

1. 解鎖覆蓋：`目前 IN 可選曲數 / N`。
2. zero-miss 覆蓋：`在同一凍結候選下符合完整 IN 驗收的曲數 / N`。

`N` 未核時，不報全曲完成百分比。HD 只在實際需要解鎖或回歸時使用，不以 HD 分數代推現在可選 IN；一般解鎖背景不替代當前畫面。運行時仍只用當前 pixels 和有界近期追蹤，歌曲標籤只用於離線驗收管理，不給動作策略。

**zero miss 是完整 IN 結算 `Miss=0`。** P／G／B／M、分數與完整性照實報，Good 或 Bad 非零不自動否決本次明確目標，也不能叫 AP／Full Combo。基礎全曲達成要求每曲至少一次同一最終 freeze 的完整 IN M0。另建議穩定性驗收採每曲同版、同有效設定、預先指定的連續 3 個 attempt 全部完整 IN `Miss=0`，跨至少 2 個 session；失敗／aborted／unknown 不刪除，不能挑三次最佳成績或無限刷到成功。這是待使用者凍結的額外重現性建議，不偷偷改掉基礎產品目標。一次成功先記「單次達成」，不冒稱穩定。

### 2.2 本報告的標籤

| 標籤 | 能說的話 |
|---|---|
| SOURCE | 本次讀到此實作／測試 source；不代表測試已跑 |
| CLOUD-PASS | 本次在明列 Linux/compiler/shim/input 下實際通過；不轉作 Windows、成本或遊戲效果 |
| HISTORICAL | 既有報告／tracked JSON 所載，來源可追，但本次無全部 raw 重算 |
| LIVE-EVIDENCE | 歷史確有遊戲結算的紀錄；此處如未親自重讀原圖，仍標其為歷史轉錄 |
| UNKNOWN／PROPOSED | 尚無證據或實驗建議；不填成成功、不當人工 gold |

## 3. 目前究竟完成多少

| 項目 | 本次判定 | 不能宣稱 |
|---|---|---|
| pixels→touch 閉環 | SOURCE：latest-frame、observer/tracking、note→line、owner、scheduler、gRPC touch、manual-session 都存在；歷史有 HD/IN 結算 | main50 或 BVI 已實戰合格 |
| 線幾何與旋轉 Hold | SOURCE＋HISTORICAL：全域線 assignment、有界關聯、head/body/tail、同 contact Move、fake-clock 與混合動作回歸已有 | 此次完整 Windows suite 通過；實景每種 Hold 語義都已驗 |
| 既有主線 Release 測試 | HISTORICAL：整合時 348 pass／1 opt-in skip；CPU 工具 8/8 | 本次雲端重跑以上 suite，或所有新研究 source 都包含其中 |
| 逐曲紀錄 | CLOUD 檢查 tracked JSON：89 唯一 run、33 sessions、70 HD＋19 IN、20 歷史曲名；引用與每輪 P+G+B+M 分母一致 | 89 輪都是獨立曲目或當前存檔已全開 IN |
| IN 實績 | HISTORICAL 19 輪只有 Dlyrotz 11 輪／光 8 輪；全部 M>0。最近 C36h tint1 Dlyrotz IN13 795950／P496-G11-B0-M77 | 任一完整 IN 已 zero miss；IN 僅有兩首已解鎖 |
| X1 fixed-pixels replay | HISTORICAL 已獨立驗收；SOURCE 是完整 perception/owner/scheduler/FakeTouch，不應仍稱只有 observer | 新動作對遊戲的反饋、真 RPC 成本、物理 owner gold |
| X10d-P／suppression | pending-missing hook 有歷史冷契約通過；suppression 仍 OFF；14 組機會損失取捨未知 | 應自動上機或取消越少越好 |
| BVI | SOURCE：bounded-current 候選與 fake constraints 存在；本次核心已 Linux 編譯和小型研究 probe，詳分支 | 原 356 layer-cases、22 supplemental、4 controls 完整套件已過；接上真 owner 或真機 |
| 程序控制 | HISTORICAL control-03 的 natural／nonzero／owned-child 控制已有可信成功；SOURCE 保留 | 任意新 wrapper／根綁定／Windows closure 都自動合格 |
| 原 build 阻塞 | SOURCE：D19/D20 slash adapter 與 root prefix 不符可核；Windows configure 的 offending step 仍 UNKNOWN | 修 source 字串即代表已修 Windows；雲端 probe 取代真控制 |
| 成本與候選 live | X11/R1/R2/R3 歷史工程和負結果在；X12 仍 not-ready | 用少量平均耗時或其他 candidate 成本替新候選簽字 |
| 原始畫面可用性 | Git 有索引／報告，無 ignored 原圖、原 events 或 frozen binary | 本次重驗 7722 PNG、原結算圖或全部 DLL SHA |

### 3.1 本次新實驗，與歷史測試分開報

| 本次實際執行 | 結果與限制 |
|---|---|
| 原封 BVI core＋新合成/typed smoke | GCC編譯，28/28 assertions；GCC ABI metadata 185904 bytes。不是原356-case套件或最壞成本量測 |
| 原封 scheduler＋Linux clock shim | 13 cases／66 assertions；五指deadline、capacity、排序、unknown Down、stop與mapping等。額外 Flick backlog case 通過，所以共14個不同合成case；沒有owner／真RPC |
| GCC ASan＋UBSan | BVI 28 assertions與scheduler 13 cases在關閉LSan後通過；最初LSan因ptrace環境限制失敗，保留原log，不宣稱leak pass |
| 時間與preserve公式 | 可重跑算術反例；未執行正式observer/tracker、不量真機jitter |
| BVI靜止端部對照 | typed與後續synthetic RGB→extract→relate→constrain都重現新需求資格差異；既有source原封，仍沒有正式GameObserver或真圖gold |
| tracked總帳／模板 | 89run／33session／20歷史曲名的一致性與模板語法；不等於原結算圖重驗 |

以上不可合成一個「全部測試通過率」，也不可與歷史348+1、CPU8/8相加。[可重跑命令與核驗](VALIDATION.md)保留這些分母。

## 4. 四分支整合：現在最值得驗證的機制

### 4.1 工程阻塞可分層，不是一切歸零

舊 BUILD 包的「native0／candidate 未編譯」仍是該次 Windows 事實。本次證實 BVI core 不依賴 Windows／JSON，能隔離编譯；它的完整 harness/CMake 卻硬綁 MSVC 與本機依賴，frozen input 也在 ignored 目錄。應把核心冷契約、完整 fixture suite、Windows wrapper／控制和候選效果四層拆開驗。

保留已驗 `owned.cs`、transaction、PID/creation/image/job identity、held handle、durable STOP 與原 oracle。D19/D20 修正應在**新且有授權的測試根**產生 canonical scratch，再跑必要完整交易回歸，不修改舊 STOP、不移除 root guard，也不將所有一般作者錯誤擴張為重建整個程序平台。

### 4.2 低殘差不等於低總時間誤差

`capture_complete` 是 gRPC Read 返回的 host 時間，不是 render time；250ms guard 只約束相對 lag。source age 未校準時，35／40ms lead 只是綜合設定，不能拆成「gRPC 固定延遲」。本次獨立公式 probe 已展示：相同真軌跡，在人工指定的 arrival-age 漂移下可以 residual=0、fit uncertainty 很小，卻相對 source crossing 偏離很多。

所以先同步記 frame arrival／pixels_ready／decision／accept／due／injection start-return，再區分 source age、observer/queue、owner late、RPC duration、可辨遊戲反饋。不能把各階段 p99 相加，也不能用提高 lead 治療所有漏辨／身份／生命周期問題。公式反例不證明真機存在相同 delay。

### 4.3 安全拒絕與機會損失必須同時計

main50 的 confirmed association 有每幀距離容差，並非嚴格單調接近；緩慢遠離可能一直保留。這需和真多線、反轉、接近才對齊一起做反例，而不是直接把容差刪除。

另一方面，保守取消／到期／capacity drop 可防重複 Down，也可能丟掉仍有效機會。原 scheduler 的 isolated fake-clock 已顯示串行 RPC 對同時多指的累積延遲，以及 intent 被移除後 cursor 不存在的語義；完整 owner 是否因此退休且無法安全恢復，只能按其回執契約另驗。unknown Down 永遠不能藉此重試。

### 4.4 不能從可見 body 推出不可見的物理 ownership

BCC 同 RGB 卻要求不同隱藏 owner 的矛盾，只否定那組不可辨識要求，不代表一切 pixels 方法無效。BVI 應只主張它當前觀測、短期連續性與 fake constraints 能支持的內容；`attachment_query` 是 harness 提供，不是真實 ownership 感知。head 新接入、已有 Down 的 body 接觸與 tail 結束要分開；少 Down／少 cancel／穩定 ID 都不能單獨當 Miss 下降。

本次 typed source-level 反例還顯示：四個新鮮幀跨 60ms、current RGB signature 變化但端部 descriptor 不動時，BVI `independent` 一直為 1，新 Down 機會為 0；端部每幀移 1px 的對照則在第三幀取得資格，兩組已有 Down 的 Move 都可持續。這只證明現行 hash 去重與新需求確認的機制，不證明真圖抽取或物理 owner；值得針對「線追靜止 note」驗證，不能直接放寬防重播規則。它與 28 項 smoke 全過沒有矛盾：smoke 驗既有規則，反例問規則是否漏掉合法需求。

後續最小 RGB 對照以固定藍 Tap、白線移近，實際執行 `extract→relate→constrain`，沒有手填 signature，仍重現相同差異。這加強 source-level 機制證據，但 ROI／all-lines 是合成 fixture 提供，不是正式 observer，也不是實際歌曲採納證據。

### 4.5 小片段與完整回放不可混用

視覺／關聯實驗可從短窗與近期暖機開始；長 Hold 的已執行 Down、retired identity 或 Drag leader 卻可能早於 90ms。沒有可信 prefix／checkpoint，短片段不能等價完整 owner replay。現有 X1 全輪模式依原輸入規格重播；fixed pixels 也永遠不包含新觸控造成的新遊戲畫面。

## 5. Miss 根因樹

此樹是可否證的分類，不是已把 M77 分配成各原因。先排除錯曲／錯難度／部分結算／OCR與版本配對錯誤，再逐層建立「首次失效位置」。每一層都允許 UNKNOWN。

```text
完整 IN 結算 M > 0
├─ A. 判定需求沒被及時看到
│  ├─ source age、capture gap、重複／skip、pool/drop、診斷競爭
│  └─ 當前像素漏線／漏note／type／head-body-tail不可辨或遮擋
├─ B. 已觀測但關係／資格未成立或成立錯誤
│  ├─ line/note ID churn、真雙線誤合併、錯note→line
│  ├─ 靜止物件／線追note與獨立樣本判準矛盾
│  └─ 多root／反轉／跳變、確認窗來不及、錯誤preserve
├─ C. 計畫沒有及時成為可執行觸控
│  ├─ gate、stale、missing、pending取消與誤退休
│  └─ contact capacity、due排序、逐RPC串行、deadline／late
├─ D. 已發出觸控但未形成正確遊戲動作
│  ├─ mapping/geometry/rotation、RPC unknown、release fault
│  ├─ Hold接入／同contact Move／錯續接／過早Up／tail
│  └─ Drag接觸共享／Flick位移速度／遊戲實際採納窗口
└─ E. 記錄不足，無法區分 A–D
   ├─ 缺raw／prefix／source time domain／exact binary
   └─ 把combo/cancel/ID數當Miss、只看成功計畫子集
```

## 6. 最小實驗清單與先後

具體 source 行號、輸入、probe 輸出和每分支更細的 case 見各分支文件。以下數量是起步的**有界研究設計**，不是官方判定窗，也不是自動執行授權。

| ID／優先 | 區分的假說與最小實驗 | 觀測／驗收 | 依賴、風險與預期效益 |
|---|---|---|---|
| E00／P0 | 凍結 baseline、解析既有 ledger；另查當前版本／完整列表／IN狀態 | repo/hash、唯一run、PGBM分母；`N` 和每曲unlock有畫面來源 | 本次 Git/JSON 已做；當前畫面仍缺。避免錯誤全曲分母和不必要HD |
| E01／P0 | canonical scratch 的完整診斷交易＋既有控制；marked wrapper逐步 probe | D19/D20抵達 intended transaction；原負例仍拒；vcvars/argv/CMake哪步先失敗明確 | Windows新授權／原收據；保留安全核心。解除「永遠跑不到候選」而不重寫平台 |
| E02／P0 | 同一 BVI source 的 core probes，接著原 frozen 分層suite | 新probe與原356/22/4分母分列；wrong-contact負例不可改oracle取pass | 核心已可雲端；原JSON依賴／fixtures缺。通過仍只到冷契約 |
| E03／P0 | 靜止端部＋移動線、相同source重播、相同內容新source、已有Down對照 | independent/span/usable、Down機會／Move、invalid；真重播不得累積資格 | 小型C++反例；不從hash同值推出來源重播。定位可觀測qualification缺口 |
| E04／P1 | 真圖片段先人看pixels再對照trace；同輸入分別替換「正確觀測」「正確relation」的離線oracle | visible FN、錯配、unknown、首次失效層；gold／proposed分列 | 精選raw＋prefix界線；oracle只離線，不能放runtime。避免盲猜應改detector或tracker |
| E05／P1 | 控制source age/jitter與幾何motion；再以真host鏈時間查各候選 | root bias/coverage、capture gap、age不可辨項、全stage n/p50/p95/p99/max | 公式小試已做；真source age需合法可校準來源。不要用lead掃描掩蓋不可識別性 |
| E06／P1 | 五Down同due、Up/Down交錯、不同RPC delay與deadline邊界 | 每個intent的due/start/return、rank、expiry、late、capacity、unknown與release | 原scheduler假時鐘已做；真RPC分布另需裝置。定位串行尾端，不預設batch已整合 |
| E07／P1 | 原owner對capacity/late/expiry掉計畫後，cursor缺失與已知0、unknown分開 | 是否可安全恢復、retirement原因、重複Down=0、全需求保留率 | 需完整owner依賴；先反例後決定改法。不能拿安全性換召回或全部拒絕求安全 |
| E08／P1 | Hold head→body→tail、旋轉／反轉／線追note／晚對齊；同時第二Hold/Flick | 同contact、觸點當前支持、過早/晚Up、錯續接、missing/alias、完成不復活 | 先既有fake-clock/RGB，再精選真圖與未來受控實機；物理owner unknown不硬標 |
| E09／P1 | 同一exact候選對diagnostics off/on及背景noise做有界A/A | 完整publish/skip/drop/accept/action分母、尾端與observer effect | 成本gate仍必需；不用先解完193 skip或建大型平台才動其他研究 |
| E10／P2 | 候選安全／全部相關回歸＋有效runtime成本→exact freeze/preflight | source/binary/dependency/profile/mapping一致；錯誤、漏樣本、容量都可審 | Windows／裝置階段，未授權執行。任一安全故障撤銷候選資格 |
| E11／P2 | 小型有限pilot：開發難例IN、不同動作組合IN、必要HD回歸；固定A/A後A/B | 所有run完整PGBM/score、故障及最差曲，不以均值掩蓋大退步 | 需使用者選曲/Play與live授權；只證明pilot，不是全曲或未知曲泛化 |
| E12／P3 | 凍結通用候選，按全部N曲逐曲IN；必要才HD解鎖；每曲重複成功 | 每曲完整IN M0證據、3連續成功建議、全曲覆蓋與全部失敗索引 | N/版本/解鎖先核；改策略後舊pass屬舊候選，不混成同版全曲完成 |

### 推薦路線與可並行的工作

1. **先把版本、分母與執行阻塞拆開處理。** E00 的畫面盤點可與 E01/E02 的工程恢復並行。既有 process-control 是資產；不是為追求新平台而重寫。
2. **用最小反例排序候選。** E03/E05/E06 可在無遊戲雲端先完成機制研究；E04/E07/E08 才決定要修觀測、關聯、owner或輸入時序。沒有證據顯示必須用新模型。BVI 也是待驗分支，不是 zero miss 唯一路徑。
3. **一次只改可解釋的一組機制。** 對同一exact source跑安全回歸與有效成本，再走有限實機。固定擷取、語言、五指能力與資料用途不變，禁止歌別調參。
4. **先得到可重複單曲，再擴場景與全部N曲。** 開發歌改善不能當未知曲泛化；全章節達成也不自動證明任意新曲通用。零Miss是逐曲結果，不是平均Miss下降。

沒有可信資料可估「幾天能zero miss」或每個分支能降低多少Miss。工程效益先以可觀測的阻塞消除／機會恢復／錯誤減少表示；真Miss效益要等同版實機結果。

## 7. 每輪最低觀測欄位

- provenance：baseline/candidate source SHA、binary/dependency SHA、compiler/OS、有效profile、遊戲版本、geometry/rotation/mapping、run/round ID。
- frame：epoch/generation/sequence、source sequence/time/domain、capture_complete、pixels_ready、publish、consume、drop/skip及其分母；未知source age留unknown。
- perception：當前可見與預測分列、line/note identity、候選/overflow、relation資格/次佳差距、root/uncertainty及其假設、拒絕原因。
- action：intent/contact/leader/alias、revision、cursor/executed prefix、deadline/due、cancel/retire理由、injection start/return/status、release責任。
- result：完整/aborted/unknown、曲名/難度的可見證據、P/G/B/M/score、結果圖SHA、同輪manifest與events、所有重試索引。

報告所有 eligible、rejected、cancelled、expired、capacity、fault；不要只計最後成功送出的動作。量測欄位可增收據，不能反向把診斷／曲名／長歷史餵給策略。新增量測本身亦需容量上限与 observer-effect 對照。

## 8. 尚需決策及可立刻做的最小下一步

本次研究可交付，不需要現在取得帳號／存檔、整包raw或使用者電腦。

後續真正需要決定的是：

1. **證據輸入**：是否先提供 [ignored 最小清單](IGNORED_INPUTS.md) 的 BVI frozen JSON＋configure 收據與一輪 C36h 結果包？先小包就能消除許多未知，不必傳全部原圖。
2. **下一個實作範圍**：推薦先授權 E01/E02 的 Windows 建置診斷最小修復，以及由 E03/E06 明確反例導出的冷測補強；是否實作策略修正，應在反例與影響審查後另定。此次研究沒有全面恢復功能開發。
3. **驗收標準與實機時段**：是否在基礎每曲一次 M0 外，另採每曲預先指定3個連續attempt／跨2 sessions 的穩定zero-miss標準，以及何時由使用者提供當前版本／完整列表並安排有界實機？未有回答前，不把它們當已准執行。

本輪沒有重啟舊 STOP、啟動 live、push、開 PR 或 merge。雲端通過的實驗、未跑的完整套件及需裝置才能查的事項必須一起交付。
