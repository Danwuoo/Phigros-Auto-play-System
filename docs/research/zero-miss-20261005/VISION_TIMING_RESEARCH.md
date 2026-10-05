# Zero miss 視覺、關聯與時間研究

日期：2026-10-05。研究基準：`74e54437d4a3ad2b2bd1a3b09312211a92f2359e`。本文件只研究 pixels-to-touch 的觀測、line/note 身分、note→line、時間與 BVI 資格；沒有修改正式 source、既有 tests、分支或 commit，沒有啟動遊戲、模擬器、裝置、模型訓練、付費或發佈。

## 0. 先講結論

1. **優先拆解「沒形成正確可執行目標」與「正確目標打晚／打錯位置」。** 現有來源不能把全部 Miss 歸給延遲；也不能因 scheduler p99 約 1ms 就排除時間問題。歷史 C36h Dlyrotz IN 的 77 Miss 是完整結算，root、cancel、ID 或 fake Down 都不是逐 Note Miss 真值。
2. **現有 `uncertainty_ns` 是局部距離擬合誤差÷速度，不是端到端時間置信區間。** `capture_complete` 實際是 gRPC Read 返回主機時間；來源絕對年齡、遊戲接受觸控時間仍未識別。本輪獨立公式實驗產生 residual=0、uncertainty=3.75ms 卻 root 偏差 95ms 的合法算術反例。不能靠調 lead 或擴 uncertainty gate 修這個識別缺口。
3. **note→line 已有相當多非學習幾何實作，不應重寫成「尚無多線／旋轉支援」。** 但幾何線不等於 judgment-role 真值；初配、confirmed 保留、原線缺失後重接是三個不同故障面。歷史 X4 已支持「早期初配足以改變固定影像鏈」，X2 已反對直接移除 preserve。
4. **本輪新 cold finding：BVI 的內容變化去重阻擋了所測靜止端部的新接入資格。** 原樣 R1/build 核心的 typed harness 中，四份新鮮來源、60ms、當前支持且 RGB signature 改變，但端部 descriptor 不變時，independent 永遠 1、new-demand opportunity 永遠 false；端部每幀移動 1px 的對照到第 3 幀可用。已 Down 的 current-supported Move 兩組都可用。另以獨立合成 RGB 的「線追靜止 Tap」實跑 extract→relate→constrain，第3/4幀 current contact 已支持，仍不能取得新接入資格；移動1px對照可取得。這是 conditional ROI/all-lines 的冷反例，不是物理 owner 或遊戲 Miss 的證明。
5. **不把 ML、大型標註平台、全錄重跑或所有 skip 歸因列為前置。** 先做最小可否證的時間／慢速離線關聯／靜止端部反例；以一個小原生影像事件窗補觀測真假。BVI 必須通過真實 ROI/all-lines 來源橋接，才能談實景 RGB 能力。

產品驗收仍為 Chapter Legacy 全曲解鎖 IN、同版逐曲完整 IN Miss=0；HD 只作解鎖／回歸，P/G/B 照報，沒有 AP 前置。章節分母與當前解鎖 unknown。本研究不取得 live-ready 資格。

## 1. 證據層級與本輪確實做了甚麼

| 層級 | 本輪可說的話 | 不能升格成甚麼 |
|---|---|---|
| 本輪 source 核讀 | 核對 `src/game.cpp`、`game_motion.cpp`、`game_tracking.cpp`、`emulator.cpp`、`core.cpp`、`manual_session.cpp`、`game_session.cpp` 與相關 tests、BVI 原碼；HEAD 相符，未見 `.agents/skills` | Source 存在不等於 Windows build、完整 suite 或遊戲有效 |
| 本輪公式小試 | GCC 14.2.0、C++20 執行本報告隔離 `formula_probe.cpp`；只復算已核公式與 preserve predicate | **沒有執行正式 observer／tracker**；不算真來源 jitter、延遲量測或正式回歸 |
| 本輪 BVI typed 小試 | 原樣編譯 `research/x10d_o_bvi_r1/bvi.cpp`，8 個 frame-case、assertions 完成 exit0；該檔與最新 `x10d_o_bvi_build`、R2E/R2F/control/resume 核心 SHA 完全一致 | 沒跑 RGB extract、正式 owner、全部 BVI suite、Windows target 或 live |
| 本輪 BVI synthetic RGB 小試 | 另以最新 build 原碼跑8個frame-case，獨立 raster→extract→relate→constrain，signature由extract實算，assertions exit0 | conditional幾何ROI/all-lines；不是完整GameObserver、真圖、正式owner或遊戲效果 |
| 歷史冷證據 | X1 fixed-pixels contact replay、X2/X3/X4、既有 RGB/fake-clock tests 及 BVI 設計／修補歷史按文件引用 | 本輪未取得 ignored raw，**沒有重算、重跑或重新驗其 SHA** |
| 歷史實戰 | C36h tint1 Dlyrotz IN 795950、P496/G11/B0/M77；lead35→40 的四首 HD 各一輪小幅改善 | 不是全曲驗收、逐 Note 歸因、重複 A/B 或本輪新 live |

已讀 AGENTS 所列 README、ARCHITECTURE、ROADMAP、MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH；最新日期入口採 `docs/status/DEVELOPMENT_STATUS_AND_ISSUES_20261005.md`。舊 README/路線圖中的「O 未開始」是歷史時點，不能覆蓋後來 BVI source 與工具工作。`measurements/`、`out/` 在 `.gitignore:5–6`，本 checkout 沒有它們，raw 缺席是實際限制。

本輪小試原碼與輸出：
- `evidence/vision-timing/formula_probe.cpp`、`formula_probe.csv`：公式輸出分兩段表頭，不是單一 schema 數據集。
- `evidence/vision-timing/bvi_stationary_probe.cpp`、`bvi_stationary_probe.csv`、`bvi_compile.log`。
- `evidence/vision-timing/bvi_rgb_stationary_probe.cpp`、`bvi_rgb_stationary_probe.csv`、`bvi_rgb_compile.log`。
- 詳細命令／hash／限制見 `evidence/vision-timing/README.md`。

## 2. 實際實作盤點

### 2.1 RGB 提取與可見性

- `src/game.cpp:39–45` 以固定 RGB predicate 區分彩芯、黃、紅及白；白要求各通道>195、色差<35。`game.cpp:51–60` 對連接白 component 作至多4096點／4支線分解；`704–799` 的 row／column／spanning 路徑保留最多16線；column tint recovery 需當前純白種子、局部對比，不能由全暖色特效造線。
- Hold 有當前 rails/body/outline 恢復；`game_motion.cpp:237–357` 需要近期 anchor 加當前雙側支持、接觸附近 attachment、可見 closure 才提 tail。歷史 anchor 不是當前像素。Note orientation、line normal、motion direction 有分離欄位／運算。
- 仍有值得量而非直接改的偏差：`game.cpp:1022–1037` 的一條 fallback body seed 路徑先選長度≥0.8×寬的單一高信心 `main` line，再叫 `current_hold_body`。**不是所有 Hold 路徑都只處理單線**；應用相同 current candidates、不同 line order／獨立多線的反例，確認這條路徑的漏失量。
- 視覺漏辨不能以 `targets=0` 算 recall：分母應是獨立覆核的可見物件機會。假線、看得到但未過色閾值、Note/特效融合、短 Hold 被當 Tap、同一 body 多描述、無法看清等類別要分開。

歷史實例很具體：X3 報告的6211–6214九點亮度由195.74降到171.67，6212起九點都不過 white predicate，而畫面仍有對比；這支持「bank missing 不等於 RGB absent」，尚未證成單一漏辨根因或應全面放寬閾值。來源：`LINE_ROLE_VALID_REASSOCIATION_X3_HANDOFF_20261002.md:114–127`。本輪沒有原 PNG，沒有重驗該九點。

### 2.2 line identity 已有保護，不是只有 greedy

`src/game_motion.cpp:143–235`：
- 最多16線，按角度／局部法向位置／長度 gate，做全域 assignment；最佳解與移除某對應後的替代解相差≤3則 contested。
- 競爭落選候選不立即生出新 ID；可分離近鄰需連續3幀才出生，exact duplicate 不得增加 lineage。
- 歧義可保留舊 ID 作診斷，但不更新 measured lifetime 或 motion fit；90ms 到期、100ms/context gap reset。
- motion fit 有≤6份／90ms／10ms bucket，至少3份／30ms，線是無方向幾何：以當前 reference point 的線方程擬合，避免把沿線 crop center 當物理點速度（`106–141`）。

這已處理舊 Pixel Rebelz 的 duplicate birth 機制類型。仍要測真正相鄰線、合併／分裂、同方向跨越與暫時缺線；改善 IDSW 若同時增加 unknown/FN 並不等於改善。線角色正確性是另層，不能由 track ID 穩定推出。

### 2.3 Note identity 與 note→line 是不同層

`src/game_tracking.cpp:118–198` 的 Note identity 是候選距離 greedy 加 isolated two-note swap 檢驗，較大可交換 component 保守歧義；不是和 line tracker 同一全域 solver。`recent_identity`/rail claims 在此優先使用，可能形成上游候選與舊軌跡相關性；不把 baseline-guided bank 當無偏 detector 評估。

note→line（`235–358`）使用：當前 association-valid 線、沿線 extent、跨線距離、弱外觀 orientation、bounded relative-trend，加 `same_recent` 的120分優待；best/second差<8時 unknown。這不是可校準 posterior。需要區分：
1. **初配偏差**：近的裝飾／掃過線先勝出。
2. **已 confirmed 的保留偏差**：被保留關係自行產生後續歷史。
3. **原線缺失／新 ID 重接**：old-visible、tangent dot≥.97、prior hit normal gap≤36，通常兩份新鮮樣本相隔≥12ms；近線 Drag／held body 有當前支持例外。

`preserve_confirmed` 的實際 predicate（`284–306`）容許 `abs(d_now) ≤ abs(d_previous)+max(12,width×0.10)`，並 bypass best/second ambiguity。它**不是嚴格接近**。既有 `RecedingConfirmedLineDoesNotOverrideCurrentCrossing`（`tests/game_tests.cpp:2490–2506`）以單步49px後退驗拒絕，未覆蓋每步12px而長期離線。本輪公式例只證這個 predicate，不宣稱整個 tracker 或 owner 必然注入錯觸。

歷史 X2 已示只移除 winner assignment會產生6200水平 overlap Down，仍被6194/6214 conflict擋住，並破壞原 crossing-line保護測試；X4只介入未confirmed初配，能恢復D的 vertical relation→root→fake Down。兩者分別支持「不可簡單刪 preserve」與「初配值得優先研究」，不提供 judgment-role gold。來源：X2 handoff:3–5、20–30；X4 handoff:5–19、44–62。

### 2.4 旋轉、多線、晚對齊、線追 Note 與返回

已存在的 source/tests 不能忽略：
- moving line追靜止Tap：`tests/cold_scenario_tests.cpp:445–470`。
- irregular dt、遠處未對齊到近線對齊：`:472–539`，同時有 RGB 與獨立候選 oracle 層。
- 多線交叉、V分支、完全重疊後分開：`:1829–2034`。
- Hold持續期間旋轉／平移、可見tail同contact：`:2047–2106`；雙Hold不同旋轉線 `:2306–2404`。
- 幾何連續換line ID需兩份新鮮樣本及新fit：`:2560–2588`。
- 缺線40ms內Tap/Flick投影：`game_tracking.cpp:12–91`；current Note仍需可見，line motion合格、extent/局部競爭/誤差皆過。`game.cpp:1705–1710` 明確禁止投影為已開始contact Move／續命。

本輪只核讀以上 tests，未重跑。它們不提供所有實景 judgment-role、旋轉Hold採納或同色物體物理身份真值。任何新方案應沿這些組合做對照，不能為了修一個平行線例增加「遠處外觀必須與線同法向」，也不能由返回過線重播 completed/unknown Down。

## 3. 時間 domain、frame age 與不可識別性

### 3.1 名稱與真正可量的事件

| 欄位／domain | Source 位置 | 可解釋範圍 |
|---|---|---|
| host QPC ns | `core.cpp:15–29` | 主機單調時鐘整數換算；不是 Unix、Android時間，也不等於已量 precision |
| `receive_start_ns` | `emulator.cpp:219–221,247` | 下一個 blocking Read 的開始；可能含等新frame，不能全部稱網路傳输 |
| `capture_complete_ns` | `emulator.cpp:157,221,245` | payload路徑的 Read 返回 arrival；不是遊戲 render 或掃描完成時間 |
| `pixels_ready_ns` | `emulator.cpp:158–196` | normalize/copy結束；arrival→ready可直接量這條copy路徑 |
| `published_ns` | `core.cpp:107–117` | frame mutex內 pointer swap後讀值；不是 decision publication |
| recognition start/end | `game.cpp:677–680`、`include/pas/game.hpp:75–83` | perception業務clock；另有host compute timing；fake replay業務clock不可混為真成本 |
| decision publication/selection | `manual_session.cpp:169–170,233–253` | latest packet操作存在；原流程未有足以解全部P/S壽命的成對界限 |
| due / injection start / return | `game.cpp:1844–1852`、`emulator.cpp:395–406` | 主機排程／同步RPC占用；success明寫 effect_unverified |
| `source_timestamp_us` | `emulator.cpp:258–287` | emulator Unix metadata獨立domain；未有與QPC／render-age的校準映射 |
| `source_sequence` / host frame / decision / ordinal | `emulator.cpp:249–295`、`game.cpp:676–678` | 四種key不同；可用來join及分層drop，不能替代时间或物理Note ID |
| replay rebased fake ns | `contact_replay.cpp:114–121` | 對原capture/ready同量平移；和原QPC不可直接相減當延遲 |

Frame age至少分成：
- **已量主機駐留**：`now − capture_complete`。
- **來源到arrival的相對累積延遲變化**：`(arrival−anchor_arrival)−1000×(source_us−anchor_source_us)`。
- **真正像素年齡**：`now−render_in_host_domain`。目前unknown；不能把前兩者換名成第三者。

`GamePlanOwner`、SessionPerception、manual dispatch 的100ms gate都是第一種 age（`game.cpp:1342–1346`、`game_session.cpp:120–122`、`manual_session.cpp:150–160,224–225`）。250ms relative-lag guard只擋第二種正向增長，且以第一份arrival作anchor；穩定但已很舊的畫面可以有relative lag≈0。source停住、倒退、跳變有獨立檢查（`emulator.cpp:263–291`）。這些是安全邊界，並非來源新鮮度已校準。

### 3.2 為何 low residual 不保證 root 準

令真像素時間為 `s`、arrival為 `c=s+A(s)`，真線局部距離 `d(s)=v(s−T)`。現行fit把 `c` 當量測時間（`game_tracking.cpp:103,390–464`）。
- 若 `A(s)=A0`，線性fit完全正確但預測root是 `T+A0`。
- 若 `A(s)=A0+k×s`，fit仍可 residual=0，估速 `v/(1+k)`，root變 `A0+(1+k)T`。
- 因此 residual／current mismatch 都小，也可能存在很大時間偏差；目前 `max(2,residual,mismatch)/abs(v_est)` 不含來源年齡、clock映射誤差、未來加速度或遊戲判定窗。

本輪隔離公式數值（px/s、ms）：

| 場景 | true root | 估root | 偏差 | RMS | 程式式樣 uncertainty | relative lag最後值 |
|---|---:|---:|---:|---:|---:|---:|
| age=0 | 150 | 150 | 0 | 0 | 2.5 | 0 |
| 固定age20 | 150 | 170 | 20 | 0 | 2.5 | 0 |
| age=20+0.5×source_ms | 150 | 245 | 95 | 0 | 3.75 | 50 |

輸入為每20ms的800px/s、6份量測；依現行90ms history裁切後做OLS，皆跨≥30ms且bucket不合併。第三例最新arrival170ms時真crossing已過20ms，估root仍在75ms後；lead35會安排40ms後，relative lag50ms仍小於250ms。這是**可產生偏差的模型反例，不是實測遊戲延遲，也未執行owner**。

既有 `TimingUncertaintySeparatesFastCaptureJitterFromSlowAmbiguousMotion`（`tests/game_tests.cpp:1131–1152`）用規則20ms時間、位置±12px jitter驗空間誤差除速度；`ShortCaptureBurst…`（1098–1118）擋3.6ms假速度；它們不等價於把來源render時間與arrival時間分開變動的實驗。`SixtyNinetyAndHundredMsUseActualSourceTime` 名稱裡的Source實際餵的是`context.capture_ns`（`tests/game_tracking_tests.cpp:115–117`），不能拿名稱宣稱校準source timestamp。

### 3.3 lead 可調，但不能把它當純輸入延遲

以可正確追蹤、未clamp的單Note簡化：`action_error ≈ source_age + fit_bias + dispatch_lateness + input_to_game_delay − lead`。同樣的效果可由不同項組成；僅有P/G/B/M和RPC return無法唯一分離它們。lead35→40四首HD每首一次，Miss分別−1/−1/−2/−1，只是歷史描述（`FOUR_SONG_HOT_TUNING_20260929.md:16–29`），不是5ms固定gRPC latency已量得。

還有非線性混淆：late due被clamp到now，Drag另加15ms；`capture→Down`包含計畫等待；早期predicted_due超過60ms不進plan；expired或根本沒形成target的Note不在Down latency樣本分母。先分「可排程且未clamp」「late-clamped」「資格不足」「未檢出」再評估lead。AP不是目標，不能把Good優化掩蓋Miss。

若要量絕對來源age，需獨立可見時標／已知呈現事件與host對映及其誤差界；單純以最小arrival-source差估offset仍混有最小transport delay。若要量觸控接受，需可核对的可見fixture回饋或其他合法外部觀测。兩者都不能從當前raw補算出不存在的事件；不用遊戲內部状态、歌曲時鐘或讀譜當捷徑。

## 4. 缺幀必須分層，不能只看平均FPS

管線分母應依序保留：來源sequence→received valid/invalid→publish attempt／pool drop→latest overwritten／consumer skip→perception產物→decision publication→owner selection／reject→plan→due dispatch／receipt→可見遊戲結果。

`LatestFrame`目前一個latest、三個物理buffer（constructor也容許受限slots），leased buffer不可覆寫、耗盡drop（`core.cpp:32–68,107–130`）。慢消費者跳過舊圖是設計，不是應全部取消的失敗。需要問「skip是否抹去某個唯一可見接入／tail／端部」；單純提高FPS、保留所有舊圖會增加age並違反latest契約。

歷史 C36h owner-consumed playing間隔p99=43.946792ms、max433.1117ms；較窄consecutive-playing scope max77.3507ms不能替代前者。歷史來源：`DLYROTZ_FRAME_ANALYSIS_C36H_20261001.md:141–150`。在那樣的長gap中，90ms motion／100ms gate可能依法失效；是否剛好遮掉可判機會尚unknown。Full-recording錄全received不證source未漏，recording也獨立於owner skip（`manual_session.cpp:138–139`）。

runtime B已提出publication/selection bounds、全分母、censor/Unknown與trace observer-effect設計；本轮只讀，不重算193 skip。既有raw缺P/S時，換更多parser無法補出時間。見 `RUNTIME_DECISION_SKIP_B_CONTRACT_REVIEW_20261004.md:21–27,51–81,99–119`。只在其答案能改變具體工程選擇時做最小插樁，不把A/B整套完成當視覺研究前置。

## 5. BVI：可見支持與物理身份必須拆開

BVI的合理任務是：當前body、contact-zone、雙側端部、背景gap/occlusion＋≤6份/90ms的observed-piece關聯，輸出same-contact Move或independent opportunity限制。相同RGB與有界歷史仍可能來自不同physical世界，任何演算法（包含ML）都不能由完全相同合法輸入唯一識別不同答案。這是資訊界線，並非宣稱所有pixels方法無效。

核對結果：
- `research/x10d_o_bvi/CONTRACT.md:5` 明示RGB extractor仍接收外部當前測得ROI/all-lines，typed geometry不是全note discovery。
- 最新核心仍以固定藍/白/黃/黑predicate、body兩flank、端部transition與line-contact probes工作：R1/build `bvi.cpp:13–55`。它不會自行得到任意真圖的合法ROI。
- `bvi.cpp:87–97` 的 `Guard.attachment_query` 是fake既有contact掛接條件，不是physical owner sensor；「Move=true」不能當接上正式owner或真採納。
- 原52圖packet缺合法current ROI/all-lines（`docs/status/DEVELOPMENT_STATUS_AND_ISSUES_20261005.md:71–72`）。H/K/A/D已由歷史AI看過不代表human gold；本輪未拿到圖，未重新視察。

### 新小試：靜止端部與新鮮來源被內容去重綁在一起

核心 `signature`（`:16`）含front、width、angle、measured_depth及端部flags，不含line pose；`relate`（`:69–82`）在任一歷史RGB hash**或**descriptor hash相同時不加independent；usable要≥3且跨度≥30ms。`constrain`新opportunity要usable（`:95`），活動同contact current support則不必（`:94`）。

本輪直接編譯原核心，typed對照如下：

| typed輸入 | 4幀時間 | descriptor / RGB | 第4幀結果 |
|---|---|---|---|
| 靜止端部、同body、current contact唯一；hit可隨線移動 | 10/30/50/70ms，各新key | descriptor相同；RGB各不同 | independent1、span0、usable false、新opportunity false、active Move true |
| 其他相同，端部每幀移1px | 同上 | descriptor與RGB各不同 | independent4、span60ms、usable true、新opportunity true、active Move true |

上述只驗 `relate→constrain` 資格，**typed樣本不是RGB提取或真世界證據**。為檢查該輸入是否只是手填signature的產物，本輪另寫獨立640×640 synthetic raster：藍Tap核心79×9px、中心(320,500)，白線y520→510→504→500，4份新key、時間10/30/50/70ms；query與all-lines是renderer已宣告的current幾何。最新build原碼實跑extract→relate→constrain，未手填signature：

| RGB情境 | 第3/4幀當前contact | 實算descriptor與RGB | 第3/4幀資格 |
|---|---|---|---|
| Tap固定，只有線移近 | supported、line_unique=true | descriptor相同，RGB均不同 | independent1、span0、usable0、new opportunity0 |
| 其他相同，Tap每幀橫移1px | 同上 | descriptor與RGB均不同 | independent3/4、span40/60ms、usable1、new opportunity1 |

兩情境共8個frame-case，assertions exit0；這仍是**conditional ROI/all-lines synthetic RGB**，不是完整note discovery或真機採納。後續應把新鮮但同內容長靜止body、重複舊source key、同key篡改timestamp、遮擋後同外觀replacement分開，不能只把signature每幀強改來讓測試綠燈，也不能解除unknown/completed Down保護。內容不同不是統計獨立；內容相同也不自動代表來源重播。將「來源新鮮度」和「端部移動足以辨識時序關係」用不同語義記錄，才知道這是必要保守拒絕還是丟失可接入機會。

## 6. Miss 根因子樹與最小區分實驗

每個事件先建立可見物件機會，不以runtime ID作物理分母；若無足夠觀測，直接Unknown。各枝可能交互，不能把同一事件重複算多顆Miss。

| 子樹 | 最小區分證據／介入 | 若結果成立能說甚麼 |
|---|---|---|
| A. 沒收到必要像素 | source／capture／publication/selection key、gap與可見機會窗；同RGB序列只改drop mask的fake-clock對照 | 缺幀足以消除觀測機會；不證歷史Miss由此造成 |
| B. 收到但提取不到 | 小窗當前原圖＋獨立部位/線覆核；同frame對照color mask／ridge／candidate原因 | detector FN／false line，與association錯分開；不可用自己候選當gold |
| C. 有候選但identity錯／歧義 | permutation、近鄰、合併分裂、two-note swap與>2鏈；記所有assignment成本與拒絕 | 指出identity guard瓶頸；IDSW下降須同報FN/unknown與機會損失 |
| D1. 初配錯線 | 保留已有X4結論；新最小獨立typed/RGB幾何，初見無motion後第二幀可辨；加入線追Note/晚對齊反例 | provisional選擇如何帶入錯誤確認；不把oracle變runtime規則 |
| D2. 錯confirmed自我維持 | 每幀慢速離線+競爭線；不變更候選/identity，逐步記pre/post winner、preserve、conflict | 本輪predicate反例能否延伸到正式tracking結果；保持原crossing保護 |
| D3. 原線消失／換ID後卡住 | 同pose換ID vs真正neighbor vs正交新線，缺線39/40/41/89/90/91ms | 區分量測缺失、identity更換與role轉換；不轉移舊root／完成intent |
| E. root在錯誤時間或位置 | 已知source-motion與獨立arrival軸，固定/斜率/jitter/dropped-frame矩陣；linear及旋轉/加速 | root誤差、first usable lead、錯過機會比例；uncertainty校準缺口 |
| F. plan尚未形成／過期／clamp | 完整current/target/plan/due timeline＋Owner FakeTouch；含未排程分母 | 何處首次不再可救；不能以只看已Down延遲報成功 |
| G. 已Down但Hold接續／tail有誤 | 當前body/rails/contact支持、既有cursor、每Move、可見tail及Up責任 | 支持lost contact或body alias假說；沒有current body就不靠grace續租 |
| H. RPC返回但遊戲沒採納 | capability/座標＋可見輸入效果與同源結算；未知收據另列 | 输入語義／位置／時窗檢驗；RPC success本身不夠 |
| I. 標成Miss的診斷其實不是 | 對照獨立可見判定／結算；cancel、combo失色、root0、ID更換另列 | 去除錯誤歸因；不拿Unknown當成功 |

優先不是把整棵樹都做完，而是選能使下一步決策不同的最小枝。BVI只覆蓋B/G的一部分，不能包辦A/D/E/H；lead只可能影響E/F/H且對未形成目標無能為力。

## 7. 建議順序、效益、風險與退出條件

| 優先／實驗 | 最小投入與量測 | 效益／風險 | 依賴與退出條件 |
|---|---|---|---|
| P0 時間雙軸冷契約 | 約12個事前宣告軌跡：constant age、affine lag、非仿射jitter、burst、drop；速度慢/快各一，主機與source軸分欄；真root由renderer輸入算 | 確認uncertainty coverage、first usable lead、late-clamp可救性；風險是把合成age誤稱實機量測 | 本輪公式已示問題；下一步用正式tracking/owner隔離build。若未知age無上界，報不可識別，不能靠參數掃描偽造校準 |
| P0 BVI來源資格反例 | 新鮮靜止、線追Note、舊key replay、遮擋replacement、moving positive五個RGB/typed控制 | 防止「只會在端部移動時有資格」；風險是放寬去重導致假確認 | 本輪typed與conditional synthetic RGB線追Tap已驗；真圖仍需合法ROI/all-lines。若相同合法觀測仍無法區分身份，保留Unknown並停止此identity主張 |
| P1 原圖最小事件窗 | 先取K2865–2889或D6158–6220其中一窗、前≥90ms觀測暖機、必要後文，配index/events/manifest；另保留1個正常對照 | 把可見缺口、提取／關聯與source時間分開；風險是未含接觸前綴造成假owner結論 | 小窗只作observer/觀測，不當完整contact重播；若raw缺或真值不可判，停止量化而保留proposed |
| P1 慢速離線preserve | 正式tracker typed序列：12px/步、不同dt/寬度、近鄰競爭、原crossing/晚對齊對照 | 判別積累離線是否固定錯line；風險是刪保護讓交叉線偷走正確relation | 不重做X2整體移除；若只改局部閾值才過，尚不能採用。需不依歌曲的機制及反例 |
| P2 小型合法RGB→BVI橋接 | current ROI/all-lines皆帶同frame來源；固定query bound；同column雙Hold/舊tail新front/rotation/absence/小Hold vsfragment | 驗候選真有觀測輸入，避免typed假通 | 不讓full-prefix bank、手述parts、未來frame充當current detector；無合法橋接就維持not-ready |
| P2 視需要P/S插樁 | 只有待解工程選擇確實依賴decision skip時，沿B的bounds/全分母/ON-OFF影響檢查 | 找短snapshot是否在pre-accept期間失去；風險是量測本身製造skip | 無可識別結果、未影響工程選擇或trace有副作用即停，不無限加runner/trace |
| P3 有限實機lead／觀測候選 | exact source/binary/profile/fingerprint與成本gate後，固定通用候選、先A/A再配對A/B，全部曲及失敗照報 | 確認實際Miss/副作用；風險是noise/同曲過擬合 | 本輪無授權/未執行；零Miss驗收不可由cold結果替代，不增加AP gate |

上述是研究優先序，不是開啟裝置、延長既有goal或移植BVI的授權。先保留已有成功控制，避免再建完整恢復平台；新觀測須有明確答案與停點。

## 8. 最小需要哪些 gitignore 資料

### 本輪程式研究必需

**不需要額外ignored資料。** 本checkout已有程式、tests、歷史文件；本輪可完成的source分析、公式probe、BVI typed核心probe都已做。不要為本輪打包整個`measurements/`、7722 PNG、`out/`或套件cache。

### 真像素／時間核驗：按問題只取一種小包

1. **優先一個觀測事件窗**：以原根
   `measurements/game-assist/2026-09-30-m0-manual-continue/full-recording36g-01/sessions/manual-session-22885039263800/`
   的 `full-recording/index.jsonl` 及其 `path` 指向原PNG取子集。二選一：
   - D6158–6220：初配→缺線→重接／role；前綴按capture時間補至少90ms而非固定幀數。
   - K2865–2889：同column分離body→舊接觸區缺支持→incoming；補同樣觀測暖機。
   可加clip09正常control的5287附近少量幀。不要一次索取全部兩窗或全錄，先由要否證的假說決定。
2. **同包index與manifest**：每張保留原ordinal、source_frame、source_sequence、capture_complete、pixels_ready、source_timestamp及domain、尺寸/rotation、PNG SHA；原run/profile/source/binary SHA和選窗理由。子集另列原index SHA與切片規則，不改成「原完整index」。
3. **匹配事件**：原`round-1/summary.json`所列event_segments中的同窗原序列、精確source_frame join；事件無source_frame時保留「journal-order近鄰」而非精確。只要觀測／描述性時間分析，可提供帶來源hash的窗口excerpt與必要prefix/state說明；它不能冒充完整journal雜湊一致的原segment。
4. **要看歷史X4而不重新跑**：索取 `preconfirmation-role-x4/causal-report.json`、選窗trace／oracle-attempts及input/query manifest與所引hash；先核結論，不用重新索取7722PNG。X3同理取`line-role-x3/causal-features-1verified.json`的有界對應輸出和manifest。
5. **要研究來源delay變化**：需要同窗連續received index（包含未進perception/owner者）、source timestamp/sequence以及原capture統計；只有owner events無法重建接收分母。這只量relative variation，絕對age仍unknown。

**重要更正：90ms暖機只覆蓋有界觀測／局部motion，不恢復長Hold的已Down/contact/completed前綴。** 現有X1完整contact入口讀完整index與journal，從earliest available preroll到EOF，且重建lifecycle及owner（`apps/frame_review/contact_replay.cpp:67–112,114–121,316–330`；X4 handoff:44）。不能說把小窗塞進現有CLI就能等價重跑完整contact。若未來真要完整反事實owner驗收，應在有原full-prefix資料的原環境執行現有工具，或先另驗合法checkpoint能力；本輪不為此索整包、不偽造historical Down，不把小窗結果當contact驗收。

### 本輪不需要

整個`out/`、舊LibTorch/GPU/CPU runtime、模型權重／訓練資料、整套vcpkg/SDK、所有failed-run收據、遊戲APK／譜面／音訊／帳密／內部狀態、全曲按鍵序列。只為同版Windows replay需要的exact binary／frozen source／依賴closure，應由該驗證任務另定，不能在這次source研究先要求全部上傳。

## 9. 驗收紀錄與仍未知

- 本輪公式反例、BVI typed及conditional synthetic RGB對照有可重現C++原碼／輸出，未改正式程式。BVI compile有一條既有misleading-indentation warning（`bvi.cpp:97`），編譯成功且assertions exit0，warning保存。
- 本輪沒有重跑原GTest/Windows建置，沒有RGB原圖、完整raw replay、來源age或RPC採納測量。歷史數字均標為文件引用。
- 仍未知：各曲真可見Note recall、judgment-role gold、實際物理Hold歸屬、來源絕對年齡與clock映射、input-to-game延遲、各種可判窗、各Miss子因比例、BVI完整分層suite／RGB橋接、正式成本、Chapter Legacy現行分母／IN解鎖／全曲zero miss。
- 本報告的新資格缺口值得以小反例修清語義；不足以宣稱需某個ML模型、需全面改擷取、需全域放寬grace，或可直接上機。
