# 主程式邏輯與實戰開發計畫

原規劃日期：2026-09-26。2026-09-27 已開始 G0／G1 開發、UI-only 自動 PLAY，並依後续授權接入 G2 真實 assist；全曲能力及 AP 尚未驗收。現況及證據見文末與 [開發紀錄](GAME_RUNTIME_DEVELOPMENT_20260927.md)，下列未落地的設計仍不能當作已有 API。

依使用者最新方向，以 **Chapter Legacy → Glaciaxion HD 同版本至少連續三次 All Perfect → Glaciaxion IN 穩定 AP** 推進。使用者已授權自主實作、測試、除錯與 Computer Use 遊戲導航，不需每輪等候手動準備。主程式直接用正版遊戲即時畫面研究，不另開大型簡單目標 Fixture 開發／量測階段。短小合成回歸與必要的既有觸控能力核對仍保留。

本計畫取代舊 M3–M7 的順序、三首歌曲前置要求、獨立 Fixture 閉環門檻及「AP 不在目標內」的範圍。原文保存於 [歷史計畫](MAIN_PROGRAM_DEVELOPMENT_PLAN_LEGACY_20260925.md)。[擷取最後驗收](CAPTURE_FINAL_ACCEPTANCE_20260926.md)的歷史結果不改寫；其主程式接線缺口併入 G0。以下類別、參數與介面均為設計，不是現有 API 或已通過能力。

## 1. 目標與界線

- 第一目標：Glaciaxion HD 整首由主程式依即時 pixels 操作，至少同一凍結版本連續三次結算全 Perfect；單次成功不算穩定。
- 第二目標：同一套邏輯研究 Glaciaxion IN，繼續以 AP 為目標；IN 改善需回歸 HD，不拆成兩套歌曲腳本。
- 曲名、章節及難度只作 run 標籤與結果核對，不輸入逐音符決策。不能讀譜面、遊戲記憶體、內部狀態、預錄按鍵，或以歌曲時間、BPM／節拍表、音訊補足操作。
- 使用者手動登入、選曲、選難度及重新開始；主程式從畫面辨認遊玩與結算。自動導覽、解鎖、課題模式、完整 GUI 不在本輪範圍。
- 2026-09-27 使用者新增要求：選曲後保留 PLAY，由程式啟動後依即時 pixels 自動點一次 PLAY，不使用 computer-use；此 UI-only 入口不擴大為自動導覽或曲中遊玩能力。
- 2026-09-27 後續授權：繼續 G2–G4 真實遊玩試驗；使用者休息後允許 computer-use 做模擬器選曲／重試等少量導覽。這覆蓋先前導覽禁令，Note 操作仍只由 C++ 即時 pixels 決定。
- 使用者回來後表示可代為點擊，改以assist --manual-play停用自動PLAY。能力核對與playing gate仍有效；先驗證菜單0觸控、PLAY未被自動啟動，再通知使用者開始。第十二輪漏加此選項而仍自動PLAY已記為操作失敗，不能稱該輪為手動啟動驗證。
- 第十三輪發現session_start計時會耗掉人工等待時間而截斷曲尾，該輪excluded。手動模式另設最多60秒waiting；像素首次確認playing後才起算完整duration，有限預算只能arm一次。不是逐音符歌曲計時。
- 自有正式邏輯採 C++20、單程序多執行緒、CLI＋Win32／D3D11 診斷預覽。先建立可解釋的幾何視覺方法，沒有逐音符 LLM 或歌曲記憶。
- 尚未觀察本機 HD／IN 的完整遊玩畫面，不預設音符構成、線運動或可觀測性。未見的能力標為未驗證；AP 是研究目標，規劃不等於證明 pixels-only 必然可達。

## 2. 現有程式盤點

已讀 `apps/pas/main.cpp`、`include/pas/core.hpp`、`src/core.cpp`、`include/pas/emulator.hpp`、`include/pas/config.hpp` 及現有 profiles。下表是程式盤點，不是重新執行實機驗收。

| 現有元件 | 新階段處理 |
| --- | --- |
| `GrpcCapture`、`LatestFrame` | 沿用 payload fast／RGB888／top-down／256 KiB、三個物理 buffer／一個邏輯最新槽；不重開擷取選型 |
| `run_observe()` | 現在只有擷取、健康探測及預覽；抽出 runtime，新增遊戲分析但保持 observe 無真實輸入 |
| `RuntimeConfig` | 尚無相對 lag 欄位；新增嚴格配置、有效值 manifest 及實際傳遞回歸 |
| `GreenTargetDetector`、`VelocityTracker`、固定線 predictor | 保留合成測試意義；遊戲另增多音符／多線契約，不把單綠點類別改名當遊戲辨識 |
| `ContactScheduler` | 保留單 owner、epoch、期限、取消／釋放原則；補 live owner 接線及遊戲規劃能力 |
| `GrpcTouch`、`PixelCoordinateMap` | 沿用獨立 channel／未知結果語義；核對當前畫面與觸控映射及能力指紋 |
| `Journal`、分析器、預覽 | 沿用有界寫入；新增決策事件、結果核對、錯誤分類與只讀疊圖 |

排程整合不能直接把每個偵測結果轉成完整 ContactPlan，須處理五個具體缺口：

1. **手指配置太早。** 現在 `submit()` 就占用 contact ID；較遠音符可能占滿手指。由 planner 保留有界未來意圖、檢查接觸占用區間，接近可執行時才提交。若仍需更改 scheduler 配置時機，獨立改契約與回歸。
2. **ID 順序限制。** scheduler 的新 intent ID 必須大於 high watermark。Track／Note ID 與提交 ID 分離，owner 按首次接受順序配置 intent ID，revision 沿用同 ID；避免亂序到達而永久拒絕合法 Note。同時防止已完成 Note 重新辨識後取得新 ID 再次觸發。
3. **active revision 進度。** 現在 active plan 更新將 step index 重設為 1。遊戲版只替換未執行的未來部分，保留已 down 的 contact、已執行進度與 up 責任，不重播 Hold／Flick 的舊 move。
4. **目標與全局失效。** 現在任一 plan 過期會使整個 scheduler fault。新契約區分單目標失效與全局 gate／input 失效：前者撤銷並釋放該目標，後者撤銷全部。共享接觸暫不啟用，責任保持可分離。
5. **batch 尚未接入共通排程。** `GrpcTouch` 有 batch 方法，但一般 `TouchBackend`／scheduler 仍逐筆注入。先量測多押送出偏差；需要 batch 時補共通介面、逐命令 receipt 和部分未知結果語義，不假稱已完整支援。

## 3. 每一幀的決策流程

```text
gRPC capture worker → 最新有效 Frame
                         ↓
               Perception worker
       UI 識別 → 線／Note 候選 → 跨幀追蹤
                         ↓
       相對運動預測：撞線區間、觸控區域、證據期限
                         ↓
                有界 DecisionSnapshot
                         ↓
         單一 action／scheduler／touch owner
      接觸規劃 → deadline 執行 → receipts／釋放責任
                         ↓
                     遊戲新畫面

Runtime supervisor → epoch、全局撤銷、停止
Preview／Journal／Result analyzer ← 只讀快照與事件
```

1. 讀最新 frame，核對 run／epoch／generation／geometry；落後就跳幀，不補辨識舊圖。
2. 辨認 UI；遊玩畫面與目標可觀測性分開。沒有音符或暫時看不到線，不能直接推成選單。
3. 找線／Note 候選與不確定度，不一開始強行完成所有配對。
4. 用有限近期觀察更新線姿態、Note 身分、種類、線關聯與運動；保存小型座標／特徵歷史，不持有長串原始 frames。
5. 為證據足夠的 Note 預測短期撞線區間與觸控區域；其餘標等待、不可觀測、過晚或拒絕。
6. 發布最新的完整有界決策快照，列出可執行、短期延續及撤銷狀態。owner 對照完整集合取消已消失的未執行意圖；跳過中間快照也不能漏掉取消責任。
7. owner 根據現有接觸狀態規劃動作。每次 dispatch 重查 gate、版本、目標證據、revision 與輸入健康。
8. 下一張圖修正預測；可見判定及結算另供診斷。Note 消失、RPC OK 或分數增加均不能獨自證明某顆命中。

### 執行緒與容量

- 主執行緒處理啟停與 preview message loop；supervisor 撤銷不可等待視覺、預覽或日誌。停止／fault 必須能直接喚醒 owner。
- capture worker 沿用現有有界 buffer，不在 callback 辨識或同步存檔。
- perception worker 依序對同一 frame 做 UI、偵測、追蹤及預測；第一版不為每層各開 thread。
- action／scheduler owner 是唯一能修改 plans／contacts 及注入的執行緒；等候新快照、停止或最近期限。無新 frame 時仍喚醒處理過期及 release。
- Journal writer 維持有界；preview 使用低頻快照並及時放掉 frame lease，不讓預覽占滿 capture pool。
- 最新 frame／decision snapshot 各一個；tracks、歷史點數、線關聯候選、plans、steps、preview bytes 及 journal 都配置硬上限。超量有明確原因，不無限擴充或靜默丟掉 up。
- 健康 probe 只作診斷，不能阻塞 owner 到期撤銷；static probe 成功不延長遊玩證據期限。

## 4. 視覺與追蹤

### UI 與場景

輸出 `MENU / LOADING / PLAYING / PAUSED / RESULT / UNKNOWN`，附 frame、時間與可見依據。先在 observe 確認開局、空拍、暫停、恢復及結算。進 PLAYING 需多張不同來源 frame 的一致證據，確認窗口在 G1 固定；重複讀同一張圖不算多次確認。

第一版選單由使用者操作；畫面不明時停用，不以「按 PLAY 後等固定秒數」接手。曲名／難度標籤與遊玩 gate 分離。

### 線與 Note 候選

先以色彩、形狀、邊緣及幀間連續性建立基線，不能只用單一顏色或固定螢幕 Y 座標。線候選含中心、方向、可見範圍與信心；Note 候選含輪廓、種類候選、朝向、中心、Hold 頭／身／尾證據。以運動與配對證據排除背景／裝飾線。

先全畫面找候選，再以追蹤區域加速；定期全域重找，避免 ROI 永久漏新目標。縮放、裁切及觸控映射有明確版本。配線結合朝向、相對運動、歷史一致性及可見性，不只取最近線。重疊候選不能因中心接近就合成一顆。

第一版不預先引入新模型依賴。若證據顯示主要瓶頸是視覺混淆，再以固定樣本比較 C++ OpenCV 方法／小型模型的誤差與尾端耗時；若失敗來自排程，不以更大模型掩蓋。

### 近期身分與遮擋

track 狀態為 `tentative / confirmed / temporarily_unobserved / retired`；動作進度分開記，不以已送 down 替代視覺成功。

類型／線關聯有歧義時保留候選，新觀察只修正未執行部分。短暫遮擋只延續到已核定期限，不能用另一顆 Note 或新 UI frame 刷新該目標證據。跳變、重新開局與幾何改變清空 track epoch。已完成 Note 的短期去重資料有界，不靠無限 tombstone 保存整首歌。

## 5. 預測與時間模型

以線中心 `c(t)`、切向 `u(t)`、法向 `n(t)`、Note 判定錨點 `p(t)` 定義本專案的幾何模型：

```text
d(t) = dot(p(t) - c(t), n(t))     相對線的法向距離
s(t) = dot(p(t) - c(t), u(t))     沿線位置
估計短期 d(t + τ) = 0 的根，並估計該時候的觸控區域。
```

這不是遊戲內部公式；Note 錨點及有效區域需由實機畫面／結果校準，不能把可見寬度當完整可觸控寬度。線旋轉時法向改變也影響距離導數，不能只相減螢幕 Y 速度。

第一版對有限近期樣本作局部線性擬合，輸出殘差及時間／位置不確定度；只有線性模型持續留下可解釋誤差才加更複雜運動模型。相對速度近零、反向、跳變、配線不明、預告不足或根在短期範圍外時，不硬算觸控時間。

- host 一律用同一 QPC monotonic domain；分別記擷取完成、pixels ready、辨識／預測完成、plan 接受、預定注入、注入開始／返回、首次可見回饋。
- 以 `capture_complete_ns` 擬合只得到主機到達時域的表觀撞線估計；畫面產生時刻仍 unknown，不能稱為真實遊戲判定時刻。
- 注入期限可寫作「表觀撞線估計 − 校準的綜合提前量」。此量包含未分離的顯示／傳輸／輸入偏差，不宣稱等於任何單段延遲；RPC 返回耗時也不等於觸控生效延遲。
- 第一版校準在局與局之間做，固定 profile、版本及範圍，保留所有嘗試。不由 AP／Miss 反推精確毫秒，不在局內依總分盲調。
- 預測區間、動作窗口與證據期限分開。未經本機驗證的 Perfect 時窗、Hold 寬限、Flick 速度等不寫成常數。250 ms 相對 lag guard 不等於允許 250 ms 舊圖觸控；現有 150 ms evidence／2000 ms horizon 也不是已驗證遊戲設定。
- G2 前由 G1 到達／處理分布及合成回歸固定保守試驗參數、依據與環境；後續變更版本化，不事後改門檻把失敗變通過。

## 6. 動作規劃與手指管理

2026-09-27 使用者確認四／五指可用、Hold須到整條結束、連續黃色Drag可視作持續接觸。planner9依當前visible tail決定正常Hold結束、以當前Drag覆蓋區域及窗口銜接共用接觸，另實作5-contact profile與實測容量門控；source／界線見[動作語義紀錄](GAME_ACTION_SEMANTICS_20260927.md)。不把兩指profile當遊戲限制，亦不將無畫面期間或整段未来黃色序列預排成Hold；原失效撤銷條件維持。

預測層回答 Note 的時間、區域及動作需求；planner 再將其轉成觸控。以下為待實機核對的保守策略，[機制研究](PHIGROS_MECHANICS_RESEARCH.md)只提供歷史假說，不作本機已驗規格。

| 類型 | 規劃策略 | 必須辨認的失敗 |
| --- | --- | --- |
| Tap | 預測窗口內新 down，短接觸後 up；同 Note 不重觸 | 重複 down、相鄰 Note 誤觸、偏早／偏晚 |
| Hold | 頭部觸發後維持獨立接觸；新證據更新位置與剩餘計畫，尾部證據支持才正常結束 | 頭漏失、持續偏離、提早釋放、過期未放開 |
| Drag | 安排窗口內覆蓋候選區域的接觸，按需移動 | 未覆蓋、額外 down 干擾別的 Note |
| Flick | 有持續時間、多個採樣點的滑動；速度／位移／方向／重置由實機確認 | 位移不足、滑動未生效、連續觸發未重置 |

contact 狀態為 `free → reserved → down → maintaining/moving → release_pending → free`。注入不確定則轉 `unknown`，停用新動作並處理釋放，不自動重送 down。

- Note ID 不等於 contact ID；占用區間由 down 到 up。先保護 active contacts 的維持／釋放，再安排新 down；同時事件依期限與可行性採確定性規則。
- 初版各進行中動作獨立分指；2026-09-27 planner6另允許新鮮可靠、同位置≤2px且時間重疊的Drag沿用已active的Drag接觸，原down／停止責任不重播。pending down及Tap／Hold／Flick仍獨立，不做Hold換指；max contacts以當前已驗能力決定，不採協定理論上限。詳細契約與回歸見[開發紀錄](GAME_RUNTIME_DEVELOPMENT_20260927.md)。
- 未來意圖留在有界集合，只提交近期可執行部分，不預排整首歌或長段軌跡。revision 不累積舊 heap。
- 多押以有界 group 表達共同截止區間；batch 或逐筆策略留下實測偏差。部分結果未知時保留所有可能 active ID 的釋放責任。
- 拒絕原因明列證據不足、預告不足、能力未啟用、contact 衝突、容量或過期，不偷偷忽略而宣稱整首全支援。
- 觸控區域排除可見 UI 控件與不可用邊界，再用單一版本化映射轉 Android 座標；超界拒絕，不硬裁到邊緣掩蓋錯誤。

## 7. Session 與停止

UI 觀察與控制狀態分開。控制流程為 `CAPTURING → OBSERVING → READY → ACTIVE → RESULT`；任何階段可到 `SUSPENDED / STOPPED / FAULT`。

| 條件 | 動作 |
| --- | --- |
| observe | 辨識、預測及模擬規劃；不建立真實 touch backend |
| assist 進場 | 核對主用 profile、觸控能力、幾何、校準及 G0／G1 證據；G2 的針對性回歸通過後，只在有效 PLAYING gate 下執行受限能力 |
| 新一局／暫停後恢復 | 新 epoch、清舊 Note／意圖、重取視覺證據；不補舊動作 |
| 單目標失效 | 撤銷該目標，已按住則釋放；其他有效目標可繼續 |
| UI UNKNOWN／PAUSED／RESULT、畫面停滯／積壓、幾何失效 | 優先撤銷全局 gate，取消未送 down 並釋放，不等健康 probe |
| input 未知／逾時、關鍵日誌丟失、容量完整性失效 | fault，禁止新 down，記錄釋放結果，不默默重新 arm |
| 使用者停止 | 喚醒 owner，禁止新動作，有限時間內嘗試 release，再取消 capture／收束 workers 與 journal |

撤銷時間須量測；in-flight RPC 必須有界，stop／dispatch 排序須回歸。不能承諾取消 Android 已接受動作，或宣稱程序崩潰後一定釋放；release RPC 成功仍與可見零接觸證據分開。

## 8. 新資料契約

保留合成契約，新增遊戲契約，以小型值資料跨執行緒，不傳無上限像素引用。

| 契約 | 必要資料 |
| --- | --- |
| `FrameContext` | run／epoch／generation／sequence／geometry、host 時間；來源時間分域保存 |
| `SceneObservation` | context、UI／可見依據、線／Note 候選、辨識起訖、不確定度 |
| `TrackedScene` | Note／line IDs、有限運動歷史、關聯候選、最後真實觀察時間、遮擋／退役 |
| `HitPrediction` | Note ID／revision、表觀撞線區間、觸控區域、種類需求、證據到期、校準及幾何依據 |
| `DecisionSnapshot` | 單調 sequence、context、UI gate evidence、完整有界目標集合與狀態；失效目標不隱性續命 |
| `ContactPlan` 擴充 | intent／Note 對應、revision、context、執行進度、未來 steps、占用區間／group、期限與 release owner |
| `TouchReceipt`／`ReleaseReport` | 身分、預定／呼叫起訖、結果／unknown IDs；batch 保留每項對應 |
| `ResultObservation` | 結算 frame、可見曲名／難度／判定統計、信心及人工核對；不供下一 Note 排程 |

gate 新鮮度、目標證據期限及來源有效性獨立驗證。資料改動升 schema，保留舊 JSONL 語義。新 profile 欄位列明單位、範圍、預設／必填，拒絕未知欄位及未通過能力。

## 9. 工作包與完成條件

G0–G6 為新階段，不回填舊 M3 Fixture 的通過狀態。各包一起交付可執行流程、針對性回歸與文件，不先建大量空殼。

| 階段 | 交付 | 完成條件 |
| --- | --- | --- |
| G0 執行接線 | runtime 抽離、profile 相對 lag、owner／撤銷通路、當前幾何與能力核對 | observe 無輸入；配置確實到 capture；版本／過期／停止回歸通過；映射與能力有證據 |
| G1 HD 即時觀察 | UI、線／Note、多目標追蹤、短期預測、疊圖與拒絕原因 | 至少完整觀察一輪 HD，盤點實際類型／歧義；核對開局、空拍、暫停／恢復、結算；人工核對預測樣本，尚不宣稱命中 |
| G2 HD 有限觸控 | 意圖／手指生命週期、遊戲 gate、從可靠目標開始的 pixels→touch→pixels | 短合成及停止／過期回歸後進場；固定試驗配置，真實觸控與可見結果可對照；未支援類型明列 |
| G3 HD 全曲能力 | 補 HD 實際需要的類型、重疊、多押與線運動 | 整首無人工遊玩觸控介入，結算可核對；按類型報錯誤／unknown，所需能力均有證據 |
| G4 HD AP 收斂 | 按失敗類型改辨識／預測／動作／時序，固定版本重跑 | 一次有效完整 run 的可見結算確認全 Perfect、無 Good／Bad／Miss；保存版本、配置、所有嘗試及核對方式，達標轉 IN |
| G5 IN 擴充 | 同一邏輯觀察 IN、補新增場景及實戰 | 先列與 HD 的實際差異，改動附 HD 回歸；IN AP 為目標，未達時報瓶頸，不能以 HD 結果代替 |
| G6 整理交付 | CLI／profiles、摘要、已驗能力與限制 | 區分已達 AP、可重現程度、未支援場景及故障結果；不新增強制長測或泛化宣告 |

G0 與 G1 的只觀察工作可交錯推進；G2 觸控必須先有必要接線及對應視覺／動作證據。能力依實際 HD 需要逐項加入，不強迫先完成所有 IN 複雜場景。

第一批範圍為 **G0＋G1**：即時預覽能看見程式辨認內容、預測區間及拒絕原因，再接 G2。2026-09-26 原規劃任務沒有實作或遊戲操作；2026-09-27 開發與新增 UI-only 自動 PLAY 的進度另記於文末，不回填原任務驗收。

## 10. 驗證、診斷與 AP 證據

### 最小自動回歸

- 合成 pixels／軌跡＋fake clock：水平／移動／旋轉線、變速／跳變、遮擋、同時 Note，檢查預測修正與拒絕。真值只在測試端。
- 狀態／排程：epoch、亂序 revision、Note 去重、提交 ID 順序、contact 容量、active 更新、恰好到期、stop／dispatch、未知輸入與釋放。
- 整合：perception 停滯、舊快照晚到、跳過中間撤銷、journal 滿、無新圖時到期、batch 部分未知。不為薄 wrapper 寫鏡像測試。
- 既有 Fixture 能力只在裝置／版本／映射等指紋相符時沿用；觸控語義、映射或 batch 改動才做針對性既有 Fixture 核對，不重開大型 campaign。

### 遊戲研究

每次先寫問題、改動與預期證據，再保留全部結果。錯誤分類包括 UI、漏／誤辨、類型混淆、配線、ID 切換、預測不穩、排程遲到、contact 衝突、input 未知及不可觀測／預告不足。

遊戲回饋無法唯一配對 Note 時標 unknown。總分、combo 與結算不冒充逐 Note 真值；沒有精確判定時間就不報精確遊戲撞線誤差，可報人工標註畫面區間或表觀估計並附不確定性。

HD AP 以同一次完整 run 的結算中可辨識 Perfect／Good／Bad／Miss 或等價完整資訊交叉核對；單一線色、評級符號、預測命中數不足。自動讀取不可靠時可人工核對同一次結果並記方法，不以 OCR 完整化阻塞研究；unknown 欄位不得用預測數補填。

性能摘要保留 n、p50／p95／p99／max、失敗／拒絕／丟棄；排程 jitter 定義為 signed schedule error 的 p95−p5，另報絕對誤差。小樣本不誇大 p99；遊戲無可見 counter 就不報來源跟隨真值。

### 記錄與儲存

- 每 run 保存去敏配置、Git revision／dirty、遊戲／AVD／工具鏈版本、尺寸／方向／縮放、可見遊戲設定、capture/input/校準版本、QPC 時域、停止原因與判定摘要。
- 預設不逐幀存圖；處理後釋放引用重用，保存有界記憶體狀態及必要 JSONL。故障影像／結算截圖須明確開啟限量模式，記錄上限及保留數，不恢復全程錄影。
- 預覽顯示 frame、UI、Note／line IDs、預測區間、contacts、拒絕原因及主機耗時；降頻只影響預覽。
- 離線圖像、標註、舊操作只供診斷／回歸；正式 assist 只接受即時 capture，不提供回放到真實 touch 的路徑。

## 11. 模組落點與待量測參數

依需要在 `include/pas/`／`src/` 新增 runtime、game_contracts、game_perception、game_tracking、game_prediction、action_planner、scheduler_owner、game_results；小模組可合理合併，不先搭空框架。

`apps/pas/main.cpp` 保留 CLI 委派；共通 clock／frame／合成工具保留語義；capture/input 不知道歌曲及音符。遊戲規則止於 perception、prediction、planner，scheduler 執行有版本、有期限、有釋放責任的計畫。

未來 `run --mode observe` 可選遊戲分析且無輸入；`run --mode assist` 等 G2 才開放。新 profile/schema 附遷移說明，不把今天的設定檔說成已有新欄位。曲名／難度不作按鍵索引。

待實機決定：特徵／模型選擇、UI 確認窗口、每類證據期限／預測 horizon、綜合提前量、動作時長／位移／速度、contact 數與 batch 策略。由 G1–G3 固定試驗、量測、版本化；普通命名與拆檔由開發自行處理。

2026-09-27 首批進度：G0 runtime／scheduler 接線、指紋核對與 G1 開發版 observer／短期預測／dry 排程及 pixels 自動 PLAY 已實作，commit `1969bf3`。後續已接入真實 assist：首輪有限 Tap 的同一完整 run 結算 Perfect 109／Good 15／Bad 0／Miss 269，故有 pixels→touch→pixels 可見命中證據，沒有 AP。全曲四類能力正在實測；G1 暫停／恢復與完整場景分類、G3 所需能力及 G4–G6 尚未驗收。實機批次、回歸與未驗項見 [開發紀錄](GAME_RUNTIME_DEVELOPMENT_20260927.md)；擷取研究不重開，歷史 C++ 核心／Fixture 驗收保持原範圍。
