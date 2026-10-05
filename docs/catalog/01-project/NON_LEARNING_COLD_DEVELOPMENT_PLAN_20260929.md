# 非學習式 observer／planner 冷開發計畫

2026-09-29，持續開發修訂。Planner依使用者最新指示擴充原計畫；沿用 GPT-6 Sol／xhigh 開發 task「非學習式跨曲能力與延遲冷開發」（`01a0eac0-381c-77c3-9ef1-87e3bf8b5d71`），在該 task 建立持續 goal。**本輪只做正式C++20開發、既有資料分析、像素離線重播、合成／fake-clock驗證及性能實驗，不啟動emulator、遊戲、實際觸控或manual-session。** 熱測試另階段，由使用者選曲並按Play；本文件不是現在開始熱測試的授權。

長期目標是HD全曲AP或至少zero Miss；冷開發只能交付已驗證的軟體能力與限制，不能宣稱達成遊戲結果。「所有可能組合」落實為有限的代表性覆蓋矩陣、邊界與錯誤處置，不能以無限排列作完成條件，也不能把unknown算成功。

依據：[工作準則](../../../AGENTS.md)、[README](../../../README.md)、[架構](ARCHITECTURE.md)、[路線圖](ROADMAP.md)、[主研究](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)、[判定線形式](判定線形式.md)、[Note形式](note形式.md)、[M1實作](../04-offline-research/JUDGMENT_LINE_M1_IMPLEMENTATION_20260928.md)。兩份形式文件維持使用者原文，作場景與操作需求，不讀譜面欄位作runtime輸入。既有M3／M4學習分支本輪不啟用。

### 本次續作起點與 goal

[上一輪結果](../04-offline-research/COLD_DEVELOPMENT_RESULT_20260929.md)是階段紀錄，**不是冷開發已完成**：observer38／planner21／diagnostics7，Release／Debug／ASan各230項通過；已修Note占用替代配對假歧義、邊界Flick與pending Flick路徑。65組／195RGB已可驗hash冷重播，三幀不能還原長Hold。預配置A/B未穩定加速，正式預設未啟用。G1–T2完整pixels-to-FakeTouch矩陣、Hold完整生命週期、整鏈QPC與瓶頸改善尚未達標。

**持續 goal：在完全不啟動emulator或真觸控、不導入模型訓練的前提下，完成本計畫§8的C0–C6：可追溯既有像素與輔助標記、具獨立真值的代表性組合回歸、線／Note／關聯與Hold通用修正、時間預測及排程正反例、正式pipeline的離線完整鏈與有證據的延遲改善；通過§5與§8.7冷驗收，交付可重跑版本及熱測待驗清單。** HD AP／zero Miss與Dlyrotz恢復是後續實戰目標，不是本goal可宣稱的結果。

本goal沒有使用者指定token預算。由原開發task先查自身goal；不存在或已完成才建立上述新goal，已有同範圍未完成goal則續作並記錄。不得因做完一批測試、寫出部分未達報告、缺人工雙標或缺新實戰就把goal標完成。冷開發還能完成的程式、反例、測量持續推進；真正需要新pixels／人工裁決／遊戲語義的項目分列，不能以猜測填補，也不能阻塞其他可做項。停止、阻塞與完成依goal工具契約處理；不自行把goal設為paused。

## 1. 七輪資料與版本基準

七輪是**六個HD譜面＋光IN12**，六個不同曲名；不是七個獨立歌曲家族，也不是原訂十輪都完成。資料來源：

- 新測試：[啟動索引](../../../measurements/game-assist/2026-09-29-line-m1-ten-song/index.json)、[離線結算抄錄](../../../measurements/game-assist/2026-09-29-line-m1-ten-song/completed-rounds.json)。原始session為 `measurements/game-assist/manual-session-140156097343000/`，round-1…7有manifest／events分段／summary／result.png，另有pixel-clips/index.jsonl及RGB。
- 對照：`measurements/game-assist/manual-session-108176133899800/` 的round **3、6、7、15、9、11、19**，依序對應本輪1…7。[既有結算與provenance](../02-game-results/MAIN_LEGACY_HD_RESULTS_EVIDENCE_20260928.json)。IN獨立比較，不混入HD成果。
- 本輪binary SHA-256：`d9fa50034d1935b87ee4a1ab1f73ec762fa63b0e0f3835ffe4778f3e0fd8e780`。source base `baf3d4fcbaac56ab085e19b9fba8a5ef6615d3bc`、dirty=true，策略observer37／planner19；實際source.patch及source-hashes.json在啟動索引目錄。不能用base commit單獨重建本輪策略。
- 前版binary：`de22022803a00ec0fd81ba19c8575b3728cb6e3bfd007ac73a360b08e08dc601`，observer36／planner18，source `cd0ec437f495ee73e92a1adaccd097a86eeb91ac`。
- **已證實的metadata錯誤**：新manifest策略字串已37／19，但 `game_observer_version/game_planner_version`仍36／18；`src/manual_session.cpp`有硬編碼。P0必須改成單一版本來源並測一致性，不改寫歷史manifest。
- 本轮環境：Windows 11 Pro build26200、Core Ultra5 125H；Android16／SDK36、AVD phigros、x86_64、guest processors5；固定1280×720／rotation1、gRPC fast RGB888 top-down／256KiB、五指lead35ms、uncertainty30ms、無preview、有既有三幀pixel clips。capability fingerprint matches=true。來源絕對年齡unknown；GPU／driver／電源／AVD競爭負載未完整記錄，後續補provenance，不能假設兩輪環境完全相同。
- 本輪七份summary皆result_confirmed，round release_failed／unknown為空。規劃時未發現pas／emulator程序，但session根沒有最終summary；只能確認七輪完整，**不能聲稱整個session正常STOPPED或全程無故障**。

Planner用當前凍結C++ `pas analyze game` 逐輪按summary順序暫時串接events，分析後移除新建暫存，不改raw。[audit.ps1](../../../measurements/cold-plan-20260929/audit.ps1)、[audit.json](../../../measurements/cold-plan-20260929/audit.json)保存14輪分析、來源分段及hash。已核對253個raw檔（兩版相應round的manifest／events／result，以及新195張RGB）；另核對七份summary及patch／source hashes／config／capability的11項hash，见[supplemental-hashes.json](../../../measurements/cold-plan-20260929/supplemental-hashes.json)。未宣稱對舊版所有RGB再次全面核對。

已另凍結 `out/observer37-frozen-20260929/Release/` 的pas.exe、pas_tests.exe、pas_core.lib及DLL，15檔120,207,026 bytes全部copy hash相符，見[frozen-baseline.json](../../../measurements/cold-plan-20260929/frozen-baseline.json)。後續不覆寫此處；新建置用開發輸出。舊兩輪與M0／M1資料同樣保留。

## 2. 逐曲結果與可證實的追查入口

以下old→new指36／18→37／19；每版每譜面僅一次，不是受控因果A/B。分數與判定來自既有人工抄錄、同源PNG及hash；Planner另目視核對本輪Dlyrotz結果圖。

| 新round／譜面 | old P/G/B/M → new | old分數 → new（差） | 優先追查與限制 |
|---|---|---|---|
| 1 Dlyrotz HD9 | 452/0/0/6 → 450/0/0/8 | 961572→910044（−51528） | max combo336→118；Hold Down43→28；查身分／body失去支持與觸發缺口。不能把2個新增Miss定位成某兩個Hold |
| 2 光 HD7 | 312/1/0/2 → 311/3/0/1 | 943762→948429（+4667） | Miss減1但Good增2；作簡單場景時序與辨識回歸，不能僅以分數提高判全改善 |
| 3 光 IN12 | 443/7/0/67 → 427/4/3/83 | 799603→756170（−43433） | Miss增16且Bad增3；保留高密度混合／身分／容量反例，IN不作本輪HD驗收分母 |
| 4 Pixel Rebelz HD9 | 412/1/0/92 → 429/8/0/68 | 751257→787089（+35832） | Hold Down15→46；改善仍剩68Miss；查線ID修正後的部位／關聯，不把更多Down直接解讀正確命中 |
| 5 混乱-Confusion HD10 | 376/1/0/101 → 443/0/0/35 | 743692→860879（+117187） | Hold Down46→79；辨識耗時p99反而下降，須保留這個反例避免一律歸因新版較慢 |
| 6 FULL AUTO SHOOTER HD9 | 295/1/0/93 → 310/9/0/70 | 704846→742327（+37481） | contact_conflict拒絕1→4；PLAYING已消費畫面間隔max約963.76→962.10ms；查間隔兩端與接觸佔用，不先歸因新擷取延遲 |
| 7 -SURREALISM- HD9 | 465/2/0/104 → 533/3/0/35 | 764396→862443（+98047） | Hold身分歧義取消3→10；有2筆target_evidence_or_window_expired；辨識p99升高，查body搜索與負載、保留新改善 |

### 2.1 Hold診斷：取消不等於失敗

原C++分析器統計與按 `contact_started` 分組的raw核對見[hold-cancel-counts.json](../../../measurements/cold-plan-20260929/hold-cancel-counts.json)。計數是intent／event，不是譜面Hold數、tick數或Miss數。

| 新round | 實際Hold Down | Hold取消事件（已開始／未Down） | 可見tail確認 | 主要取消原因（事件數） |
|---|---:|---:|---:|---|
| 1 | 28 | 29（28／1） | 0 | missing/region23、identity4、held unsupported2 |
| 2 | 13 | 13（13／0） | 0 | missing/region11、identity2 |
| 3 | 34 | 34（34／0） | 0 | missing/region26、geometry5、identity2、held unsupported1 |
| 4 | 46 | 48（46／2） | 0 | missing/region41、identity6、geometry1 |
| 5 | 79 | 79（79／0） | 0 | missing/region77、geometry2 |
| 6 | 57 | 56（56／0） | 1 | missing/region45、geometry6、identity3、held unsupported2 |
| 7 | 95 | 94（91／3） | 1 | missing/region76、identity10、geometry7、held unsupported1 |

大部分已開始Hold走取消路徑，tail正常結束極少，值得最高優先追查。**也可能包含真實結束後body消失而走fallback，不能全稱提前Up。** P0要逐intent連接Down→body evidence→Move→cancel／Up→tail及可見回饋，分清接入失敗、持續丟失、正常尾端缺標、未知。

Dlyrotz已有可追溯例子（新round-1/events-1.jsonl）：

- frame9485、note911／intent335：`identity_ambiguous`，contact_started=true、executed_steps=1，cancel_ns=140352251282100。證實已開始接觸因身分歧義取消；不證明音符真實身分或遊戲Miss。
- frame9493、note906／intent334：`current_object_missing_or_region_lost`，executed_steps=16；最後證據140352356950900、取消140352418889700，約61.94ms。符合missing期限的軟體路徑，不能透過放寬期限當修正。
- frame9688、note922／intent336：`current_held_region_unsupported`，executed_steps=40。須回查之前body／線／Move及當前目標，不能只看cancel標籤認定detector漏辨。
- 新round1共有83次target `motion_discontinuity`、64次 `near_line_appearance_unqualified`、180次 `association_ambiguous`；未Down預測取消中motion_discontinuity只有3次。兩者分母不同，不得將每幀target狀態數當獨立失敗數。

新Dlyrotz只有9組三幀／27RGB；clip7為frame9514–9516，未覆蓋9485／9493當下。**既有採樣不能補回前述cancel的pixels真值。** 開發端從現有可見片段提取有因果可辨性的例子，欠缺部分保留unknown並用機制合成測試，不虚構影片或人工標註。

### 2.2 已量到的延遲與尚未量到的部分

數值單位ms，格式 `n；p50／p95／p99／max`。這是本輪原始診斷重算／round summary，沒有新實機量測。完整old/new各指標見audit.json，樣本上限100000且本輪以下n未達上限。

| round | PLAYING owner消費画面間隔 | recognition耗時 | RPC呼叫耗時 |
|---|---|---|---|
| 1 | 7329；16.554／33.578／46.593／127.007 | 7632；4.629／7.978／10.894／21.834 | 1624；0.729／1.218／1.771／12.015 |
| 2 | 7798；16.612／32.954／44.314／150.604 | 8079；4.483／7.035／9.226／24.553 | 778；0.737／1.320／1.910／2.701 |
| 3 | 7751；16.613／33.091／47.006／148.497 | 8094；4.642／8.560／12.786／42.965 | 1404；0.721／1.275／1.735／2.501 |
| 4 | 8453；16.472／33.884／46.355／151.019 | 8758；4.708／8.282／11.137／22.461 | 1365；0.738／1.299／1.684／2.450 |
| 5 | 7584；16.477／33.072／45.087／96.564 | 7864；4.706／8.837／11.263／16.809 | 1403；0.737／1.252／1.895／3.345 |
| 6 | 7436；16.535／33.720／45.706／962.096 | 7913；4.817／8.347／10.909／32.257 | 1019；0.755／1.351／2.125／2.801 |
| 7 | 8985；16.539／33.821／46.508／114.909 | 9401；5.259／9.901／13.336／28.016 | 1986；0.750／1.293／1.798／2.754 |

注意分母：recognition含該輪非PLAYING畫面；summary的playing interval是先前合格PLAYING到下一份合格PLAYING，可能跨過gate失效；原analyzer另有「連續兩份gate=true」分布。不得混用這兩種scope。`host_residency_ms`是在owner記錄decision時 `now-capture_complete`，還在accept之前；不是完整pixels-to-touch。計畫中的intentional等待也不是處理負載。

Dlyrotz旧→新：PLAYING間隔p99 **42.526→46.593**；recognition p99 **10.647→10.894**；capture→owner記錄p99 **14.122→15.132**。新owner全部command schedule lateness為 **n1624；0.172／0.602／1.017／1.991**；RPC最大12.015ms，但沒有證明該outlier對應Miss。原analyzer的predicted_deadline_down_lateness是相對plan的predicted_down（已含lead／Drag提前），**不是遊戲撞線誤差**。來源render time未知，不能量出絕對端到端輸入延遲。

七輪分析的unknown_contact_receipts及contact_history_resets皆0、runtime_revokes計數0，但仍有UI門控、release與session終止等獨立事件；不把「某欄為0」概括為無故障。FullAuto四筆contact_conflict需逐筆看在用指、需求是否真同時、active Hold是否錯誤佔指。

### 2.3 分層歸因與優先級

| 層 | 現已支持的事 | P0／後續尚須證明 |
|---|---|---|
| 線／Note觀測 | runtime有無線輸出及body不支持事件；使用者觀察辨識失效 | 同幀RGB確認可見線／Note而漏辨、誤分類、誤body／tail，才列已證實觀測錯誤；目前無逐曲recall真值 |
| 身分／關聯 | Dlyrotz已開始Hold遭identity_ambiguous取消；Note仍採greedy及替代pair歧義檢查 | 是否為已占用替代pair的假歧義、同Note碎裂或合理拒絕；不能把線M1已修等於Note身分亦已修 |
| 撞線預測 | discontinuity取消及near-line拒絕有原始紀錄 | 正確觀測下是否誤斷軌跡、等待30ms錯失可操作窗口；幾何root未必遊戲判定時刻 |
| 動作規劃 | 多數Hold從取消退出，FullAuto有指衝突 | 正常結束或提前Up、重複Down、錯線Move、Drag覆蓋／Flick爭用，須intent／receipt鏈及當前支持 |
| 觸控執行 | 有RPC收據／耗時、命令scheduled/start/return | RPC成功不等於遊戲採納；同時Down偏斜、Move實效與接觸狀態仍需匹配證據 |
| 延遲 | 分布與長gap已重算；部分曲recognition變慢，部分變快 | capture delivery、consumer skip、UI gate、perception、owner／writer阻塞的分解；缺欄位先補instrumentation，不先重選擷取 |

優先級：**P0可追溯分析與量測 → P1當前觀測／身分契約 → P2 Hold與混合動作 → P3相對運動與資格 → P4有證據的效能優化 → P5整體冷驗收**。每階段可先交小修正，不做一次性全重寫；Hold反例與Dlyrotz追查貫穿P0–P3。允許證據支持的架構重設，保留舊版獨立對照。

## 3. 開發工作包與可替換架構

### P0：凍結、重算、反例及量測契約

1. 先讀本計畫及audit原始輸出；核對dirty source是否仍與測試snapshot相符。保存開發前核心headers／source與baseline library配套，禁止用新header錯接舊ABI。修manifest硬編碼，所有入口／decision／候選版本使用同一來源，增加一致性回歸。
2. 寫／擴充C++分段journal分析器：直接讀summary segment清單、驗hash與順序，跨segment維持狀態，不依賴永久串接副本。輸出每round層別計數、分母、限制與machine-readable追溯鏈；兼容舊schema，缺欄位unknown。特別區分accepted-before-Down取消、active contact取消、正常tail、過期釋放及RPC未知。
3. Dlyrotz優先建立事件窗口：上述三例＋所有Hold已Down取消＋短接觸／身分轉接＋83次discontinuity＋最大gap/RPC outlier。對照36同譜面時只能按可見機制比較，不把不同run的QPC／note_id當對齊真值。其餘六輪各抽代表事件；全部七輪皆有結果行及缺口，不能只報改善曲。
4. 重播本輪65組／195RGB與之前相關片段；分「冷啟動片段」和「有足夠暖機歷史」兩種，不把斷續triples接成連續影片。可用人工／AI提議overlay整理支持區，但proposed與human-reviewed分開。先定位有證據的錯誤，再建oracle候選與RGB fixture雙層回歸。
5. 用frame/epoch/geometry＋note/relation/intent/revision/contact建立QPC追溯：capture_complete、pixels_ready/publish、perception取用／start/end、decision_publish／owner_consume、accept、scheduled_due、injection_start/return；另記predicted_crossing、predicted_down與clamp。優先由舊receipt.source_frame join既有decision計算可觀測區段；無法唯一join須記missing，不用最近frame硬湊。
6. 診斷要能拆label／component／line／note／association／body／prediction成本、frame skip／pool drop、writer enqueue等待、decision覆寫及owner遲到。記完整鏈的直接分布，不加各段p99；Down/Move/Up及future／already-past計畫分開，planned waiting不算可優化compute。觀測證據至Down及最新revision至Move/Up各自定義。

P0交付：七輪逐曲報告、Dlyrotz具體反例、可重跑分析命令、baseline snapshot、量測欄位及缺口。初步分類不必湊成結算Miss總數。若raw不足，明列不足而繼續有確定測試基礎的通用修正。

### P1：當前場景觀測、身分與關聯

建議資料流：`FrameEvidence → CurrentScene → Line/Note hypotheses → RelationSet → Crossing/ContactEvidence → Intent → 唯一Owner`。這是責任拆分，不要求為每框新增執行緒。CurrentScene可在單perception worker產生完整不可變快照；身份假設不污染同幀幾何。

- 線：可見support segments＋方向mod π＋裁切／遮擋標記，允許多條短線／斜線／垂直線；分析現有全寬／height比例／去重門檻的漏線。以方向無關的ridge／連通片段幾何提出候選，當前像素驗證；線與Hold外框／特效的區分有負例。框、V、放射等仍由獨立線實例組合，不建歌別圖案辨識器。
- Note：保留instance及Tap／Drag／Hold／Flick類型證據；Hold的head/body/front/tail分離。兩側、旋轉下的局部部位定義不使用螢幕固定上下；方向暫不一致仍可觀測。密集連通區需有界分拆，同色相鄰／重疊不同Note不可因靠近就合併，Hold rails不可誤作判定線。
- Track：把line↔line、note↔note、note→line分開。針對Note greedy歧義建立獨立反例：已占用替代pair、縱連同位置、交叉、同時同形、多線重合、重複候選。比較可行global assignment／有界局部候選圖；不必機械照搬16線演算法到128Note並放大成本。unknown有期限、出生不自我增殖、completed/touched身份不因重接消失。
- Relation：上游先保留有限候選與unknown，角度／距離都是支持之一；遠處未對齊→近線才對齊、線追Note、較近但錯線皆有負例。已有Hold關係不能盲目鎖死，也不能無當前支持换線；可辨重接維持contact，無法辨識則安全撤銷而不延長evidence。
- UI區域限制需明確獨立於幾何檢測；現有height*.12等門檻若保留，標出無法覆蓋範圍，不靜默把全畫面旋轉能力宣稱完成。

P1交付：真值可知合成場景的漏檢／假陽性／誤合併／IDSW／關係coverage＋unknown，既有RGB對照overlay及限制；所有容量不超界。不能只以ID穩定或候選變多判成功。

### P2：Hold與通用多指動作

以明確狀態分開 `approaching → eligible → down-issued/unknown → sustained → tail-confirmed/revoked → released`；這是設計建議，可另選等價機制，但須保留已執行prefix、release責任及不重播保證。

- 接入：head當前支持＋線關係＋可支持時機；不能把body重現當新head重按。短Hold／長Hold與近線突現有不同觀測充分性，但共同期限不放寬。
- 持續：當前body/rails與所屬線形成接觸候選區，保持同一contact；既有接觸仍在有效區時可保持，有必要才Move。驗證搜索是否仍假設body同法向、舊anchor距離過小、只能單側；旋轉／平移／反轉時用當前pixels定位，不剛體轉舊Down座標、不將理想直線交點直接視為有效接觸區。
- 結束：tail可见穿越、body消失但tail不明、證據到期／RPC未知各自標原因。先消除tail與body狀態互相覆寫、mod π假過線、換線誤tail；保留失效Up。目標是區分真正常結束與提前失效，不能以少報cancel或無限按住改善統計。
- Tap縱連：同位置不同Note各需獨立意圖，Up→下一Down的最小序列與deadline可重現；空間去重不得吞縱連，已完成note又不能重新按。
- Drag：按使用者Note文件，當前判定區的持續接觸可覆蓋；以區域／時間支持有限共用，不要求每顆重按，也不把相鄰Drag串預先錄成路徑。合流／分流／交叉及與Hold共存須檢查覆蓋，不搶走活動Hold的指。
- Flick：按使用者文件**不要求指定滑動方向**；舊主研究「無箭頭方向不能自造」不可成為拒絕任意方向Flick的理由。辨認Flick類型後依可用螢幕空間／接觸衝突選有限滑動方向與位移／速度。現行固定向下、height*.94截斷可在邊界縮短，建立反例；冷測只驗觸控路徑與契約，遊戲有效速度／窗口留熱驗。不得從視覺箭頭推強制方向。
- 多押／混合：先預留活動Hold；對其餘需求做有界contact指派、有限Move／Up序列及同期限偏斜計量。1…5指能力內滿足合法組合；超過5個互斥需求明確capacity拒絕，不假稱支援任意多押。任何接觸共用須有動作語義與區域／時間支持，不能僅按近距離合併。

P2交付：完整intent/contact生命週期回歸、當前支持的Move／無支持不得Move、提前Up反例、正常tail、未知Down不重試、Hold與Tap/Drag/Flick/其他Hold的衝突測試。正常tail率提高必須有正確性證據，不能只改事件名稱。

### P3：相對運動、突現與可撤銷預測

- 以線局部有號距離與沿線座標 `d,s`、實際dt，包含線平移／轉動；方向符號連續。局部常速、分段常速與必要的常加速／轉動模型用同觀測比較，選最簡單且可驗的模型。
- 區分真反轉、角度mod π、定位抖動、換錯線、瞬移；不可每次抖動清空歷史導致永遠不足3點。段切換、等待／拒絕均有原始原因；不把未知來源延遲當速度噪聲全部吸收。
- 多root、掠過返回及線追Note：保留有限可支持候選／無root，未知不硬選；未Down可撤銷再計畫，已執行／未知Down不重播。手動假時鐘驗證取消與到期的等號邊界。
- 突現不能永遠只靠等30ms，也不能一幀重疊就亂按；依Note類型及當前支持建立明確的late/overlap策略與必要拒絕。目前欠缺遊戲窗口真值時，完成隔離介面及合成資格測試，標記需熱驗，不放大所有deadline或按歌調lead。

P3交付：已知軌跡的時間／位置誤差、可支持root比例、拒絕及誤觸數，按速度／角速／dt／可見性分層；沒有校準就叫誤差界或區間，不稱機率。

### P4：依實測瓶頸降低成本與抖動

- P0 instrumentation先於優化。特別檢查全frame重複掃描、每個Hold的大範圍逐像素搜索、候選全pair分派、JSON序列化／archive工作是否占owner臨界路徑、鎖持有、allocator及單次RPC阻塞。以同一資料集逐項profile，不從7曲總分選優化。
- 可比較一次建立共用pixel分類／梯度圖、方向化有界ROI、空間索引／候選裁剪、預配置固定buffer、確定性的成本上限、writer資料ownership移交；每個快取附frame key，不能讀到上一幀卻當當前支持。ROI優化仍需週期／當前全圖新生發現，不能永久漏出界新線／Note。
- 保留gRPC fast／256KiB、三buffer／latest1與單owner，不靠增加排隊、放寬期限、讀譜或提高backend併發來換數字。若改thread安排，先證明frame原子性、單一owner與有界負載；多worker不是預設更快。
- 冷測用凍結RGB／確定dt軌跡＋FakeTouch重播正式路徑（不建立emulator endpoint）。業務時間用fake-clock，CPU成本用獨立QPC測；不能將虛擬時間推进當實際效能。模擬固定capture cadence、掉幀、consumer慢、RPC延遲／未知等負載，另記simulate標籤。
- baseline／candidate同硬體、Release、相同輸入／採樣／並發負載交錯至少3批；每場景暖機後至少1000測量，另報unique frames／clips、loop數，不能把重播次數當實戰樣本。小樣本保留完整max/outliers，禁止刪最慢一批或平均每曲p99。

P4交付：CPU各段、capture-complete→decision／owner／FakeTouch的直接分布、owner lateness及同時Down skew、alloc／RSS／drop分布；可對舊journal量到的host鏈另附old分布。冷測不能量出真emulator render→遊戲生效延遲，明列待熱驗。

### P5：整體冷驗收與後續採樣準備

所有工作包接回同一正式pipeline，版本化輸出；新增診斷欄位兼容舊資料，工具不能連真backend。完成逐曲離線差異／代表性矩陣及未解案例清單，更新架構／路線圖／操作說明。

三幀片段對長Hold及故障前因不夠，允許在功能完成後冷開發有界事件前後診斷ring，沿用主研究512MiB／有限片段設計並測off/ring/writer負載、滿額drop、容量上限與source frame provenance。它只為之後熱測收證據，本輪不採新遊戲資料；若負載未通過，標示未就緒，不能偷偷啟用。不要將它與核心修正混在同一性能消融。

## 4. 代表性測試矩陣

以固定seed／可重現生成器、已知ground truth建立測試；按能力組與風險pairwise覆盖，不窮舉。每例標 `RGB觀測測試／oracle候選測試／fake-clock動作測試／現有實景片段`，不能只餵完美候選就宣稱像素偵測成功。

| 組 | 必含組合 | 必驗負例／斷言 |
|---|---|---|
| G1 基本幾何 | 單線水平／垂直／斜線，Tap/Drag/Hold/Flick，雙側／斜向進入 | UI／背景直線、裁切、光效；法向距離正負與螢幕方向無關 |
| G2 多線 | 平行近線、十字/X、V、框／放射代表、多中心；同時／獨立旋轉 | 真兩線不合併、單線碎片不多生、ID順序交換、重合unknown可恢復 |
| G3 運動 | 平移、旋轉＋平移、線追Note、Note近靜止、接近才對齊、反轉／瞬移／角度切換 | 較近／方向較像錯線；mod π不製造假反轉，舊root按時撤銷 |
| N1 密集Note | 等速／加減速縱連、樓梯／交互、同色相鄰、疊鍵、多押1…5 | 同位置新Note不被tombstone誤吞；完成Note不重播；>5互斥需求可解釋拒絕 |
| N2 Drag/Flick | Z/V/X／波浪Drag、分流合流、Flick串／Tap交替、四邊近邊界 | Drag共用有當前區域證據；Flick任意方向選路不被裁成零位移 |
| H1 持續Hold | 短／長、head消失、body旋轉／變形、線追body、tail未知／進場／遮擋 | 同contact續接、僅投影無pixels不得Move、body不造Down、tail不因錯線／符號翻轉假結束 |
| H2 多指混合 | Hold+Tap/Drag/Flick、2…5Hold、同時tail＋新Tap、雙側異類同時進入 | 不偷指、不提前Up騰空位、Down/Move/Up順序及期限，同時偏斜有界 |
| T1 退化與故障 | 規則／不規則dt、burst、掉幀、過期、零／近零速度、out-of-order、epoch/geometry變更 | latest-only、不接受舊輸出、source/target獨立expiry、unknown RPC不重試、所有contact可安全釋放 |
| T2 高密度／長時間 | 16線／128Note邊界、稠密候選、長Hold、多輪reset、writer滿／慢RPC | 記憶體／mailbox／分支有界、無跨round殘留或飢餓，拒絕原因與樣本分母完整 |

高風險交叉必測：**旋轉Hold＋同位置Tap縱連、雙側Hold＋Flick、多線交叉＋兩Hold、突現＋指滿、反轉＋已送Down、角度跳變＋tail剛出現、RPC拖延＋多押同deadline**。每個場景必有正常與干擾版本，不能只驗「不crash」。

## 5. 冷開發完成門檻與停點

1. P0完整七輪報告有版本／環境／結算、trace ID、分類及unknown；Dlyrotz下降保持可見。新manifest版本與binary／source一致，保留對照可重跑。
2. G1–T2矩陣及風險交叉有實作、測試與結果；未具真實pixels／語義證據者明列待熱驗，不能空白或聲稱覆蓋所有形式。已證實錯誤均有先失敗後通過的反例，必要變更不能靠改golden掩蓋。
3. 安全契約：零未支持新Down、零unknown Down重播、零重複完成intent、零錯誤跨contact佔用、零超額／無界增長；檢查不只看測試數量。時間／排程更動每項有合成／fake-clock案例及對應負例。
4. 效能：先凍結同機同corpus baseline；各場景完整鏈p95/p99不超出事前由A/A重複量測決定的噪聲容忍，且至少一個**已證實瓶頸**的p95/p99有超過噪聲的下降並傳遞到整鏈。只省均值、用更多drop丟難例或拒絕更多需求換低延遲不算通過；max、coverage、late/drop/expiry需一起報。若功能改善但tail成本上升，明列取捨與未達，不宣稱延遲已下降。
5. 最終Release／Debug完整相關專案回歸，ASan覆蓋變動及owner／scheduler；固定隨機seed與長時間容量測試。歷史golden若因刻意改進應不同，先保留baseline證據與差異理由，再新增策略測試，不一鍵覆寫舊golden。
6. 開發端在每個里程碑回報「通過／部分未達」及剩餘工作，部分未達是進度而非goal完成；依§8.7完成全部可冷驗工作後才標complete。**任何狀態均不跨入emulator啟動**。需要實機語義才能決定的事列入熱測議程；不能為湊冷完成而猜遊戲窗口，也不能因此停掉其他可完成工作。

## 6. 後續熱測設計（本輪不執行）

之後使用者確認進入熱測才用同一manual-session，由使用者選曲／Play，結算釋放、保存並待命。先Dlyrotz及穩定HD回歸，再本輪其餘HD與旋轉Hold代表；光IN保持額外壓力組，不混HD。

固定baseline37／19與新candidate、相同採樣／環境／配置，以A/A重複建立波動，再AB/BA平衡順序，各HD至少3完整輪作首批對照；保存全部重跑／abort／fault，不挑最高分。與36／18歷史結果可並列但不當同時期控制組。報每run P/G/B/M、分數／combo、已證實Hold失敗及unknown、錯接觸／故障、n/p50/p95/p99/max與最差曲。

Dlyrotz恢復須看重複完整結果及trace，不以單次超过910044就宣稱修復，也不保證立刻回到歷史961572。zero Miss須完整結算M=0；AP須完整P/G/B/M及該譜面全部判定證據支持，不能以synthetic全通或0取消代替。沒有測到的HD與場景不宣稱全曲泛化。

## 7. 開發端交付方式

開發端負責完整執行P0–P5及本次C0–C6續作與更新結果，Planner負責計畫和審查。沿用原task操作local工作樹，保留既有未提交清理／source／使用者文件；不git reset/clean、不自動commit/push，不移除原始證據。使用者提供的兩份形式文件不覆寫，新增機制設計記入架構與本計畫進度。維持GPT-6 Sol／xhigh，不另開競爭寫入同一source的task。

每個工作包交付：改動／具體原因、前後反例、命令與source/config hash、分布與分母、失敗／unknown、下一依賴。允許有證據的創新架構，但逐層替換、同資料消融，不能把整體重寫後一次總分比較當因果證明。數據處理正式邏輯保持C++；PowerShell僅維護／編排，第三方原版工具可維持原語言。

## 8. 持續冷開發工作包與資料回饋（2026-09-29追加）

本節將P0–P5未達項拆成可追蹤的續作，不重做已完成的七輪結算分析。允許重構觀測／關聯／planner邊界，不把目前類別切分當限制；維持固定capture、QPC、單owner、有界狀態與當前像素契約。優先順序為C0 → C1/C2 → C3 → C4 → C5 → C6；C5子段instrumentation可先完成，效能A/B在功能及輸入凍結後執行。

### 8.1 C0：凍結續作基準與驗收帳本

- 續作前核對observer38／planner21／diagnostics7的source、設定與exe，新增獨立baseline快照，保留其配套headers／library／DLL。上一輪報告exe SHA為`190996dfebd58b8e09d186bd5002df8e135dc4921b0ec8633f70bc1d90452353`；不一致先列實際差異，不假稱仍是同版。既有37凍結binary及36歷史比較保留。
- 在本文件§9維護C0–C6狀態；另存機讀coverage manifest，每列包含case ID、形式文件條目、G1–T2、風險組合、輸入hash／seed、normal或negative、RGB／oracle／fake-clock層級、期待結果、測試名、結果路徑及限制。不得以「已有230 tests」代替對照。
- 新資料使用同一`measurements/cold-goal-20260929/`；建立容量帳本。派生資料根硬上限8GiB，不複製整套raw；來源索引引用舊RGB。binary／source凍結存量另計bytes；合成長序列按seed串流生成，失敗只保存有界窗口。上限不足先減少可再生輸出或報阻礙，不換根繞過額度、不刪舊證據。
- 冷命令入口拒絕emulator啟動、device discovery、ADB、live capture及真touch backend；測試直接注入FakeCapture／FakeTouch。沿用正式observer／planner／owner，不另寫會「比較容易通過」的影子策略。加入配置誤用負例，證明離線入口不會意外走到裝置。

### 8.2 C1：既有記錄 → 可核對標記 → 有限通用調參

資料與分組：核對既有558RGB／186triples與新195RGB／65triples；753張／251組只是假定無重複時的名義和，實際unique數以hash及索引驗證為準。缺檔列missing，不從被清除raw重建。相同曲家族的HD／IN與不同版本同組；不得把同clip三幀分到不同split。全部看過的歌曲只能稱development／開發驗證，不能聲稱未知曲test。

先凍結30個不同clip（不足時記實際n與原因），涵蓋可見Hold body/tail、密集或零候選、旋轉／斜線／多線若存在，以及容易場景／背景控制；Dlyrotz不占全部。每種場景寫有／無資料。用所有可用家族建立fit／validation分組，對少數家族可預先固定逐家族留出診斷；所有候選共用split，不看validation逐曲改參。已用於改設計的留出組下輪轉development，不再宣稱獨立。

**OpenCV或現有C++影像運算都可用，目的是非學習式候選標記。** 目前vcpkg manifest沒有OpenCV；先復用既有`dataset rasterize/validate/export`、PNG／overlay及C++像素工具。若方向化邊緣、線段、連通區／輪廓、局部光流等具體實驗需要OpenCV，採鎖版本的最小CPU依賴、更新授權與建置說明，保留不用它也能build正式runtime的路徑；不為標記引入DNN、權重或CUDA。引入前記錄所要解決的反例與成本，不以換函式庫當作能力提升。

標記流程與信任邊界：

1. 原圖hash、frame key、QPC、geometry與crop transform先固定。線支撐片段、可見head/body/tail、Note類型、遮擋、候選關係與clip-local ID依研究§3.2擴充現有schema；runtime ID只作獨立診斷欄，不填ground truth。
2. 閾值／edge／輪廓／線段／局部追蹤輸出一律`proposed`，即使多演算法一致或AI看過仍不是`human_reviewed`。未標區是ignore／unknown，不能當負背景。每個提議記算法／參數／source hash、支持frames及版本；線／Note關係允許候選集合或unknown。
3. 先看source及支持mask，再對照診斷。產出30clip的contact sheet與選定overlay；不強迫每個部位都標有答案。驗證型別、ID引用、mask邊界、hash、同家族split、當前／跨幀支持來源。C++ validator拒絕未覆核標籤進人工gold評估分母；目前人工reviewer不足不冒充雙標，也不等人完成全套才做C2–C5。
4. 既有RGB上只能報可核對的反例、候選差異、proposed一致性與unknown；沒有人工truth不報真實precision/recall或IDSW準確率。量化正確性用C2的獨立合成truth；oracle只在離線測試注入並明示使用標籤，不能接回正式runtime。
5. 以一個可證偽假說為一輪，先固定參數範圍、目標和停止條件，再最多24組共用參數候選；正式搜尋／統計邏輯C++。本goal最多三輪此類調參，保留全部負結果。可調的是幾何／可見部位／匹配成本等可解釋參數，不搜尋歌別閾值、不擬合歷史觸控序列，不把結果分數當objective。
6. 先通過安全硬約束，再比較已知truth上的漏檢／假陽性、錯配、拒絕／unknown及時間誤差，最後看成本。不能以全部拒絕、縮ROI漏難例、延長missing／source expiry或擴大觸控資格換改善。lead／遊戲判定窗口／安全到期契約不作本次調參變量。每家族列差異，保留退步案例；沒有可靠改善就不採用該候選。

這是離線工程校準：選定的通用常數隨source/config版本交付；runtime不讀標註、片段ID、家族、曲名、診斷歷史或未來frames。禁止用observer自身輸出自標後又以同輸出評分宣稱準確度提高。

### 8.3 C2：有真值的組合生成器與正式鏈回歸

擴充C++測試場景描述／rasterizer，輸出RGB及獨立truth。truth來自生成器的幾何／部位／接觸支持定義，不能由受測detector反算。像素測試僅把RGB和時間送入正式observer；oracle觀測／關係消融另跑，不能混算pixels成功率。合成接觸語義是測試契約，不能等同已驗遊戲引擎。

以§4九組G1–T2及七項高風險交叉為必交清單，每組至少一個完整正常案例及一個不同成因的負例；每個細項在機讀manifest對映實例或具理由的資訊不足／物理容量拒絕。單純尚未實作不能標成不可觀測。G1覆蓋四Note類、水平／垂直／斜線與兩側；N1/H2覆蓋1…5指及第6互斥拒絕；G2覆蓋近線、X、V、框／放射、多中心及重合恢復；N2覆蓋四邊Flick與Drag分合。

分開Note外觀朝向、實際運動向量、線法向；掃過mod π、角速／dt／速度邊界、近線才對齊、線追近靜止Note、瞬移、反轉及可見掠過返回。先凍結至少三個seed與分層取值，測解析度/rotation變更應撤銷，測UI／Hold rails／特效相似線负例。完全隱藏或兩幀間無足夠資訊的過程期待unknown，不虛构root。

RGB → scene → track → relation → crossing → intent → owner → FakeTouch逐層記錄差異。除確切ID相等外，加入可見幾何等價、切向符號翻轉、物件輸入順序置換、合理座標旋轉等metamorphic檢查；UI／screen clipping等非對稱契約單列，不能假設全畫面完全旋轉不變。每個時間／排程更動必有fake-clock重現、到期等號及拒絕負例。

### 8.4 C3：優先完成Hold，再解觀測與關聯缺口

從上一輪未達項建立先失敗案例。優先通過連續RGB的head接入 → 多次當前body支持的同contact Move → 可見tail結束；包括線旋轉／平移、Note晚對齊、body雙側、局部被擋、兩個同形Hold交錯。若沒有相應實景長窗，以合成證明機制，實景保留缺口，不把三幀串接成長影片。

明確檢查：沒有提早Up、沒有重複Down、沒有換錯線／偷指、沒有只看tail預測就延長body、沒有current mask消失後仍盲Move；失效釋放是期待安全行為，與正常tail分開。容忍期內的遮擋與超过到期分別測，不能改成永不撤銷。把sustained body的搜尋／關聯成本獨立量測。

線觀測優先處理方向偏置與Hold框誤線；Note優先處理相鄰／同位置縱連的錯合併、同時Hold部位身份與交叉的可行替代匹配。只在新RGB／oracle反例能隔離根因時替换對應模組。允許採用有界候選圖／全域指派／少量多假設，但事前記nodes、edges、history、分支與運算上限；保留已Down／未知Down／completed intent的身份墓碑及有限生命週期。

每個可觀測正常例應有預期動作；不能全部變ambiguous來通過安全測試。真正不可辨識的重合可保留等價集合，解除後依當前證據恢復；不能以生成器秘密ID幫runtime解答。相關修正用相同raw集生成前後差異，改善合成能力與實景效果unknown分開報告。

### 8.5 C4：預測、混合需求與排程機制

- 相對距離模型以C2已知軌跡測root位置／時間區間、無root、反轉後新段與多root截斷；報全部可判定機會、實際觸發、拒絕、過期及誤觸分母。真RGB只用可見前後幀夾住的區間，不把中點或舊deadline當精準truth。
- 混合動作把「當前接觸需求」與「可用contact資源」分開；檢查預留活動Hold、Drag共用資格、Tap縱連Up→Down及Flick完整Move鏈。可比較有界到期優先指派／預留設計；不得以排程優先權覆蓋像素資格，也不得同指同時承諾互斥位置。
- 同deadline多押測兩種模式：確定性fake-clock語義與實際QPC工作耗時。若backend只能順序送，記錄偏斜及不可達deadline，不假稱同時注入；不能新增第二touch owner解決。
- 假RPC立即／慢／未知／失敗、取消與revision競爭、source新但target舊、排程前後epoch切換、已執行prefix後新計畫、指滿與釋放失敗都要可重現。成功return不等於遊戲採納，未知Down不retry。
- 所有等待、期限、revision及過期分支以fake-clock驅動；CPU效能另用QPC。避免以wall-clock sleep的偶然通過充當時間邏輯證明。加seed固定的狀態序列測試，終止時contact／plan狀態可解釋且容量不增長。

### 8.6 C5：量完整冷鏈，針對已量瓶頸優化

上一輪195幀observer p95/p99為14.841/18.135ms，base_scene p99約11.089ms、held_recovery max6.864ms；它們是冷啟動CPU成本線索，不能直接當當前同機比較基準。先拆base_scene內部line掃描／Note解讀／重複pixel訪問，補publish／consume／owner臨界區／writer enqueue/drop，再選成本最高且不減品質的一處優化。預配置已失敗的結果保留，沒有新假說不重跑同一實驗。

建立兩種分開命名的測量：一是離線observer微基準；二是記憶體FakeCapture以固定及抖動cadence餵正式latest mailbox、正式perception/owner/scheduler到FakeTouch的完整主機鏈。後者包含消費跳幀與競爭，模拟RPC等候另列；使用真QPC報CPU／排程分布，fake業務時間不冒充量測時間。source render age、真gRPCtransport與遊戲生效仍unknown。

凍結baseline38與candidate、輸入、模式、優化開關、編譯flags、電源／CPU／背景負載記錄；先做baseline A/A至少三批，固定各場景p95/p99噪聲容忍，再做交錯A/B至少三批。不以候選結果回改容忍。每模式每場景至少1,000樣本；long run至少10,000 frames或10分鐘fake業務時間，另報實際wall time與RSS採樣頻率。T2測16線／128Note、超界拒絕及writer滿載，all attempts的drop、unknown、reject、late完整記錄。

同時交付：直接capture_complete→decision／owner／injection_start/return分布，evidence→Down及最新revision→Move/Up，planned wait、lateness、同deadline skew，p50/p95/p99/max、n、unique frames、clips、家族數、loop數、失敗數。没有動作的frame仍進observer／drop分母；不能只報成功排程子集。端到端樣本必須同一frame/intent/revision join，不相加子段分位數。

至少一個已量瓶頸的p95/p99改善超過A/A容忍，且反映在可控冷整鏈對應分布；各凍結場景品質與安全不退步，完整鏈p95/p99、drop/expiry/late不超容忍。若新能力必需成本增加，保留baseline、把功能與優化各自消融並如實記未達；不能直接放寬門檻。優化未成功時goal保持未完成，繼續其他有依據的瓶頸方案；不能承諾任意毫秒或以均值加速結案。

### 8.7 C6：goal完成條件與熱測邊界

以下全部成立才可將冷goal標complete：

1. C0來源／baseline／容量帳本可驗；C1索引、30clip或實際不足原因、proposed標記／validator與有限調參結果可重跑，人工truth狀態誠實。不以等人工雙標作整個goal的必要條件；未覆核資料不參與gold準確率。
2. C2矩陣所有組及七個風險交叉有對應正式pipeline正常／負例；C3可觀測Hold head/body/tail長序列及混合contact正常運作，C4時間／排程每項改动有獨立回歸。每例有期待動作或可證的unknown／拒絕理由；無已知可修但尚未實作的清單項。
3. 安全零違反以固定矩陣與狀態序列為範圍，不能宣稱全遊戲零風險；已知truth的品質達事前凍結門檻，不犧牲正常可判定例來降低誤觸。新改動接回正式runtime而非只存在research工具。
4. C5完整冷鏈測量、至少一項超噪聲且傳遞到整鏈的改善達§5要求；不把單純量到瓶頸當降低延遲完成。最終Release／Debug／ASan與長跑回歸通過，全部輸出具版本／source/config hash及重跑命令。
5. 更新本plan帳本及原結果文件的續作章節，保留上一輪數字與負結果，同步README／ARCHITECTURE／ROADMAP。交付逐家族離線差異、所有退步／unknown、未量的遊戲效果、後續有限熱測清單。Dlyrotz恢復、真實Hold命中及HD AP／zero Miss明示待熱驗。

真實長Hold pixels缺席、人工語義裁決、遊戲有效Flick速度、真render-to-effect等無法冷證的項目，從一開始就列「熱測／資料缺口」，不把它們偽裝成冷驗收已通過，也不要求為了本goal啟動emulator。若仍有可冷修問題，goal持續；若真的無法推進，按工具的阻塞契約記錄具體外部依賴，不偷偷轉向學習或熱測。

## 9. 續作驗收帳本

下表是本次續作的最新冷證據；後面的交接表保留當時起點與負結果，不能當成目前狀態。全部位置均在`measurements/cold-goal-20260929/`，結果細節見[最新續作紀錄](../04-offline-research/COLD_DEVELOPMENT_RESULT_20260929.md)。

| 工作包 | 目前冷狀態 | 主要證據與保留邊界 |
|---|---|---|
| C0 來源／帳本 | 通過 | `baseline38/snapshot.json`獨立凍結；753張索引逐檔hash、8GiB派生額度及冷CLI無裝置守門保留；`candidate47-23-final-provenance.json`與最終`capacity-ledger.json`已刷新 |
| C1 標記／有限調參 | 通過可冷部分 | 30clip／90幀proposed且拒絕gold；預凍`c1-line-gap-sweep-plan.json`三候選，合成truth僅4px無FN／FP，3px在已驗RGB之Cereris fit家族有1幀線數及3幀語義差異，5px短clip零變動但合成有3假線；保留4px，不宣稱真實precision／recall |
| C2 代表矩陣 | 32／32代表列通過 | `coverage-validation.json`驗九組G1–T2及七交叉各正常／負例；獨立RGB／oracle／fake-clock與限制列在`coverage-manifest.json`；非全遊戲形式窮舉 |
| C3 Hold／身份 | 合成可觀測例通過 | head→多幀當前body→可見tail、旋轉／平移、雙側、多線交叉及混合指；消線錯接與tail誤判保留先紅後綠反例；真實長Hold pixels／採納待熱驗 |
| C4 root／排程 | 合成時間與故障例通過 | 不規則dt與500／750／1000px/s雙側root、靜止／遠離無root；R1–R7 fake-clock含同deadline慢／未知RPC、到期／revision、取消與不重試；真判定窗口未知 |
| C5 整鏈／改善 | 原§8.6跨場景冷規則通過 | `c5-global-gate.json`：Tap三批ABBA的recognition及capture→owner p95/p99超預凍A/A容忍；dense三批未超非退步容忍，但更嚴格的逐批加速實驗failed。兩場景各10,000 attempts、writer滿／慢fake RPC、RSS與全分母QPC均有檔；來源render age、真transport／效果unknown |
| C6 整合 | 可冷範圍通過，熱測另行安排 | Release／Debug／ASan各309／309；source/config與binary hash、容量帳本、32／32矩陣、C5跨場景gate及熱測待驗清單已重核；未啟動emulator，遊戲效果仍unknown |

以下為Planner交接時的歷史起點表：

| 工作包 | 交接狀態 | 完成證據／剩餘重點 |
|---|---|---|
| C0 基準與goal | 已完成起始凍結／帳本，隨續作維護 | 使用者已續作，冷goal仍未完成；`measurements/cold-goal-20260929/baseline38/snapshot.json`凍結134檔／161,007,847 bytes，逐檔hash相符，pas.exe SHA `190996df…52353`；`capacity-ledger.json`區分baseline與8GiB派生額度，派生資料持續更新。`candidate47-23-provenance.json`記本輪source及Release／Debug／ASan binary hash，Release／Debug／ASan三種binary與source hash已重核。`coverage-manifest.json`列九組＋七交叉的32個正反例，C++ `coverage-validation.json`目前核對16列通過、16列待驗；`offline-entry-reject-serial.log`證明冷CLI拒絕裝置參數。後續每項新增證據持續更新帳本 |
| C1 像素標記與校準 | 進行中 | `measurements/cold-goal-20260929/corpus-index-verified.json`核對兩批原始RGB與session方向指紋：251 clips／753 indexed frames／736 unique hashes、18有RGB家族，固定30clip檢視集；`proposals30-observer42/`重新產生90幀source/proposed、validator驗hash且拒絕gold升格。`replay38-to-41-diff.json`原有9幀額外線候選；`replay41-to-42b-diff.json`新增37幀線候選數變化及2幀Note數變化，Class Memories側軌假線已修，其他差異無人工真值。仍須有限通用參數實驗，人工gold無 |
| C2 組合矩陣 | 部分已有 | `coverage-validation.json`目前G1及R1–R7正反16／32列通過，16列待驗；G1獨立合成三seed的72種RGB靜態幾何、四類動態接觸、oracle關聯及兩種不同成因負例。G2平行／X、V／框／放射、多中心與側軌負例已有RGB測試；G3線追靜止Tap、待執行Down後反轉取消有RGB與oracle；切向mod π交替先敗後修，但旋轉／速度分層尚缺。N1的1至5指及第6拒絕、N2四邊Flick完整路徑及失去新鮮像素釋放、H2 Hold加Tap／Drag／Flick另有部分正式鏈。R2雙側Hold＋Flick三指正常／兩指拒絕與R5已Down Drag線反轉後有／無當前支持，均分開驗RGB、oracle及fake-clock。T1補正式latest mailbox正常逐幀與burst skip=2、無Note當前快照取消未執行Down，先紅後綠。R1旋轉Hold＋同位置Tap、R3交叉雙線雙Hold、R4滿指時突現Drag、R6角度跳變與tail剛入線、R7同deadline多押與fake RPC延遲的RGB／oracle正反已驗；縱連、Drag分合、獨立旋轉、其餘八組矩陣仍待完整正反驗收；總測試數不能替代矩陣 |
| C3 Hold／觀測／關聯 | 部分已有 | 35幀固定線及36幀旋轉兼平移線的合成Hold：同contact接入／body、多幀可見tail確認後Up；無tail且body失去支持時不Move、有限grace安全Up。舊灰色body前緣／遮擋回歸仍通過，Hold同指期間稍後Tap另用第二指；R1旋轉Hold同位置Tap的側軌續接及可見tail結束已驗；R3雙線交叉Hold消線後不錯接、可辨換ID重接與R6 tail跳角正反已驗；更多雙側遮擋、Hold+Drag/Flick及真實長窗仍待驗，H1/H2保持pending |
| C4 預測與排程 | 部分已有 | planner23修正較新完整像素快照缺少目標時，未執行Down立即撤銷；後續新鮮Drag返回可建立不同intent，已執行contact保留有限missing grace。observer45以同線歷史正規化切向mod π，使等價翻轉不假造運動反轉；`g3-oracle-initial.log`保存先敗、`g3-modpi-final.log`驗線追Tap與真反轉。仍須補root分層、混合資源與完整故障／revision序列 |
| C5 完整鏈與效能 | observer微基準已驗，整鏈量測器與A/A已建立，正式A/B未達 | 預配置A/B無穩定收益；QPC五子段定位逐列線掃描為主要成本。observer44六區塊預檢在兩批原始RGB的三批ABBA與完整decision語義比對均通過，舊558幀每模式n=3348的observer p95／p99由15.40／18.12降至7.31／9.57ms，新195幀每模式n=1170由14.46／16.83降至7.88／9.97ms。`game-cold-pipeline`現用正式latest-frame→observer→owner→FakeTouch／Journal三執行緒量QPC、skip／writer／late分母與每100幀RSS；1000幀全掃描A/A三批容忍已在`pipeline-aa-tolerance.json`先凍結，量測器漏排診斷與跳幀後序號混用兩個失敗已修。另有observer45及observer46各一輪10,000幀8ms抖動探索性長跑；後者消費8,982／skip1,018、Down／Up各829、觸控失敗0、RSS峰54.16MiB，兩輪並非配對效能A/B；功能凍結後仍須正式交錯A/B、多場景長跑、writer／RPC負載與品質門檻；不能標C5通過 |
| C6 整合驗收 | 未達 | 全部冷gate完成才標goal complete；熱測未授權 |
