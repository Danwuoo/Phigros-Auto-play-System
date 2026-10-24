# Dlyrotz IN13 連續影格分析與 C36h

2026-10-01。使用者完成十二段理由，授權逐幀研究、開發優化候選，再進行一輪manual測試。
修改只在baseline36-recovery工作樹；主checkout工作不覆寫，原12輪保持closed。
不恢復AP goal、訓練或無限實戰。

最新結論：C36h tint1單輪已完成、自動STOPPED，795950分、77 Miss。前次全錄同樣77 Miss，
使用者回報「主要的問題還是沒有改善」。主要問題改善未通過驗收；下述冷回歸與
root恢復不能代替此實戰結果。凍結binary不變，沒有啟動下一輪或直接放寬失支持門檻。

## 輸入、逐幀方法與標註保存

資料根：主checkout `measurements/game-assist/2026-09-30-m0-manual-continue/`。
原全錄 `full-recording36g-01/sessions/manual-session-22885039263800` 保存7722張
無損1280×720未畫框PNG；C36g-rec1結算778185、485/16/6/77、max combo86。
播放器零秒是第一張pre-roll，與round_start差0.9602991秒。
index SHA `0bd8ec74b6bc19d1f8080c064453a052095e328e31d2759f12d6d04d78500f8d`。
使用者REASONS.md SHA `a01ebaf9e989ab09537882f8bf41dae1e6d48bba315be0857c3c6ad00fa73ee9`。
原文字不修改；snapshot、selection-r3分別保存觀察／選取原因／標註需求／推測。
凍結clip package的revision2 snapshot不改寫；clip09依使用者更正作無問題對照。

正式離線分析全C++20、無真觸控backend：

1. `pas_frame_review <session-root> <selected-root> <new-output> replay|recorded`
   逐一核對7722張PNG/index、兩段journal SHA，再對3722張不同選取影格（3843次
   片段引用，含context）join原線pose、Note/line ID、root、取消與收據。
   六張沒有對應owner decision，明列缺口。全讀21,184原journal rows。
2. source_frame事件精確join；缺此欄的事件只附到journal-order最近decision，
   `join_basis`明列非精確。不拿撤銷、root_past、沒有Down當遊戲Miss。
3. 基線／候選從full recording起點重播，保留原辨識幀節奏、QPC及32張pre-roll
   暖機，7715次process。此前standby state未保存；新replay不冒稱原實戰decision。
4. 十二段皆檢視原圖contact sheet，重要幀另看原解析度。第3963–4026幀有0／1起算
   歧義，因此涵蓋ordinal3962–4026共65張，逐張檢视連續縮圖；保存每tile的原PNG、
   source_frame、QPC映射。沒有把3722張都宣稱已人工逐物件標gold。
5. `pas_frame_review digest <frames.jsonl> <new-report.json>` 提取1161個runtime Note ID
   的狀態轉換、172次取消及1274個receipt事件。ID不是物理Note真值。
   `compare <a-replay.jsonl> <b-replay.jsonl> <new-report>`只量行為差異，不量命中率。

輸出：`frame-analysis36g-v1/`存凍結基線replay、原events join和理由snapshot；
`frame-analysis-recorded-v2/target-histories.json`存digest及65張連續contact sheets。
初版report取消aggregate因event名稱漏了`led`而為空，不可使用該空欄作零取消；
以digest完整`game_contact_cancelled`事件與最終候選report為準。
來源絕對年齡、逐Note遊戲判定、物理身分遮擋等unknown；所有assistant role提案非human gold。

## 分段發現

|段|像素與journal支持的路徑|處置／限制|
|---|---|---|
|01、07|薄藍核心被命中特效包圍；原journal有短歷史／少量競爭、特效候選|保留遮擋與receipt時間線，尚不能將使用者所見Miss逐一歸因延遲|
|02|放射白線、抖動與Hold front/body交替。ordinal2479/source14246，同一可見body有560與542兩描述；560因identity_ambiguous撤銷，542仍有支持|候選重複不等於兩個物理Hold；不放寬未知Down重試或identity gate|
|03|短Hold、暖色特效與閃爍白線；有已開始接觸的競爭／失支持取消|仍須逐物件body/rail/tail覆核；取消不是獨立Miss證據|
|04|3495–3497 patch y553→549；3498–3501 front y605→611。Hold975/line139有新鮮rails/body，原owner48px點距離拒絕位置語義切換，3501已Down contact取消|C36h修同ID／同line當前body支持的patch/front接續，不延長missing grace|
|05|斜線、掠過白線與Drag核心同時可見，有線競爭／root缺失|補正交current ridge後仍用相對運動關聯，不把每條垂直白線當可執行線|
|06|3962–3963兩長Hold可見；3964長Hold已消失，畫面上方白線與特效仍在，後續才出現多水平線／黃色核心及藍Note與線相向／線追Note|65張全覆核；接入、body、tail及新Note分開，COMBO35→37→39不是逐Note完成gold|
|08|Hold1562已有accepted plan與Down；84.99s/ordinal4986原圖Hold與線均不可見，失支持64ms後撤銷|不是從頭沒Down；閃爍遮擋維持unknown，不用歷史影像續租|
|09|使用者更正無問題，完整前後context保留|正常對照，不刪除或強制製造失敗原因|
|10、11|垂直Flick中央箭頭分割紅核心；如5520/source17287，一個可見圖形被描述為1606、1607|C36h用Note局部方向合併，必須有當前中央箭頭，無箭頭分段維持獨立|
|12|6214/source17981/105.6875651s同時有y576橫線、x989垂直線，原decision只留橫線；側向Drag1662被配328，13張root0且無accepted plan|交叉component破壞PCA，舊split在legacy line_count=1關閉；補獨立current列ridge。普通Flick／上移假線仍分開核對|

不是77個Miss全部定位。假線語義分類、特效核心、長Hold切分及突現多線仍保留研究缺口。

## C36h 契約與回歸

隔離候選observer37/planner19、variant `C36h-orthogonal-ridge-patch-recovery`；號碼只
適用A36派生候選，不是歷史main observer37/planner19的相同binary，依SHA／variant辨識。

- 列擷取：current細線長run，gap≤4px，跨度>max(32%寬,55%高)，band≤6px；
  run至少一半為當前純白像素。命中特效染色部分可由低色差（≤100）、亮度≥120、
  雙側±4px對比≥18的當前ridge連接；預篩與完整掃描使用相同ridge資格。
  全暖色特效不能單獨產生新線；沒有用歷史白線續租。
  9點中≥7有雙側contrast、≥3點相鄰藍／紅body則拒絕；最多width個暫存run、16線。
  原split旗標仍可作消融；舊單線fallback不遮掉獨立垂直current觀測。
- Flick：沿Note切向gap與法向差，modπ方向相容、局部厚度≤20px，gap10–4%寬，
  總長≤18%寬；13個當前箭頭採樣≥8點白／黃。沒有箭頭不合併。
- Hold：同已提交Note ID／同非零line ID，兩份rails/body支持、寬度和切向資格維持；
  patch flag切換、法向位移≤128px且落在body支持範圍。放寬不給新ID／alias，
  同contact按current hit Move；不復活已Up contact、不延長缺像素grace。
- `manual-session --one-round`在一次完成／中止後釋放並停止，與full recording獨立；
  使用者選曲按Play，lead35ms、uncertainty30ms、五指和相同preflight指紋。

三個positive synthetic／fake-clock在修改前失敗。新增Hold側軌／寬白區、無箭頭雙Flick、
換Note／換line、body不覆蓋、無當前rails反例；完整Release回歸、最終7715幀重播另存報告。
最終核對新增6214原圖的特效染色案例：純白長run被中途染色截斷；補上有純白種子的
染色ridge與全暖色反例。原圖單幀診斷已同時看到x988.5垂直線和y576水平線；
最終v3連續重播的6214幀也恢復側向Drag的line關聯和root（prediction_observe_only、
uncertainty約6.01ms）。這是觀測／預測路徑恢復，不是已執行或命中證據。
5520幀的分段Flick則合併為一個132.02px核心。clip09無問題對照仍保留，
replay的runtime ID不可跨版本直接比大小或當物理Note數。
初步列擷取pass在3722張增加185個root出現、移除66個、增加445個垂直線出現，只是行為差異。
該pass與建置負載並行，observer p99為baseline15.29ms、candidate18.45ms，不宣稱性能改善
或非退步；完整n/p50/p95/p99/max/jitter保留，最終實戰另量，不相加各階段p99。

## 下一輪驗收與容量

`acceptance36h-01`獨立一輪；原12輪closed、原全錄／clips／binary全保留。
8GiB campaign按每個hard-link目錄entry計算保守logical sum，先量帳本後凍結runtime。
這輪原有有界pixel-clips最多30張RGB，完整有界journal與結果圖合計預留650MiB；
不再開5GiB full recording，不能把三幀採樣稱全錄。先過冷回歸／指紋再進STANDBY。
結算未發生前不宣稱改善；結算後另核對版本、原圖、P/G/B/M、receipt/release未知與失敗、
capture skips/gap與完整階段分布。更細後續標註仍沿用本批無損PNG和逐幀映射。

2026-10-01最終tint1候選：Release列出238項，236通過、2個opt-in跳過、0失敗，
18.47秒；`acceptance36h-01/c36h-tint1-full-tests.log`保存完整清單。
read-only preflight指紋相符、mismatches空、1280×720 rotation1及五指映射相同；
`real_input_created=false`。final replay、binary/source SHA、容量、manual launch與
STANDBY證據各自保存；以上是結算前的準備紀錄，實際單輪結果見下一節，不作實戰驗收通過。

最終`frame-analysis36h-v3`再次核對7722張SHA、21,184 journal rows，3722張join輸出
SHA仍為`e54f2744e58767073efb01349fc83c24cf5a676eda869a843786c6fd79a53195`。
7715次重播的observer ms：p50 3.3429／p95 9.39837／p99 14.181976／max 25.1434；
jitter p95−p5 7.60583。Windows Release、同主機1280×720，重播初段與ctest並行；
不是受控性能比較。3722張候選比基線增加234次root出現、移除303次、增加625次垂直線，
764張root／垂直線數量不同；Flick合併會減少描述數，仍不能把差值換算命中改善。
clip09指定90秒的ordinal5287：除runtime ID外，line pose、兩個target幾何、root及
理由與基線相同；只核對該指定幀，不把它擴張為整首非退步證明。

## C36h tint1 實戰結果與下一步驗證

session `acceptance36h-01/sessions/manual-session-16174738262800`；binary SHA
`61ecfacd2cc48ed270506719a1089be646d915c64e9026706ac1654e0d4bbce9`。
模式manual、使用者選曲／Play；1280×720 rotation1、五指、lead35ms、uncertainty30ms。
原12輪保持closed。此獨立一輪結果由同源result.png人工讀數，584個judgments；
逐Note遊戲判定仍unknown，不能以總數反推哪個ID漏接。

|數值|前次C36g-rec1全錄|C36h tint1|差值|
|---|---:|---:|---:|
|Score|778185|795950|+17765|
|Perfect|485|496|+11|
|Good|16|11|−5|
|Bad|6|0|−6|
|Miss|77|77|0|
|Max combo|86|120|+34|
|Accuracy|84.83%|86.16%|+1.33百分點|

這是一對實戰觀察，沒有重複測試的統計保證。分數／Bad改進，但Miss未減，
不能宣稱主要故障修復。所有compiled source的21個SHA和凍結manifest核對；
round summary及兩段原journal SHA另驗；結果圖SHA
`d85240867fa6386538076fb556c2f995f82e1e887d7675d64ab789a69c4271af`。
stdout為STANDBY→STARTING→PLAYING→RESULT→STOPPED，PAS已不存在。

`round-analysis-v1.json`由凍結C++分析器讀取按原segment次序的byte concat產生，
concat provenance保存兩段SHA，不排序或刪事件。7753個decision、1770個命令；
unknown contact receipt=0、最後release failed/unknown列表空。
GameAction owner的命令start相對due ms：n1770，p50 .2167455／p95 .7216517／
p99 1.04650092／max 1.527382，jitter p95−p5 .7205517。這只量主機排程，
不是遊戲接受時間或來源絕對年齡；不能因排程數值小就排除pixel時機／落點誤差。
recognition ms n7753：p50 2.867／p95 6.3021／p99 9.443968／max 18.4328，
jitter p95−p5 4.5284。owner消費frame間隔的完整分布保存在round summary；
playing區間n7284、p99 43.946792ms、max433.1117ms。分析器另有「consecutive
playing_gate=true」的不同scope，不能把它的max77.3507ms取代前述433.1117ms。

已Down／尚未Down合計的Hold取消：missing/body region lost59、held region unsupported8、
geometry unsupported3、identity ambiguous4。前次分別55／6／0／10。
這些是owner撤銷事件，非Miss歸因；自然消失、描述碎裂、真失支持及可能斷觸仍須原圖覆核。
Drag missing110、Flick identity ambiguous3等亦保留在分析器，不忽略反向變化。

`miss-review-v1`核對27張RGB與27個exact recorded decision join，9組triples，
跨度合計294.5815ms。232個診斷事件（193個已開始contact撤銷、37個combo visibility loss、
2個Hold evidence expired），其中只有9個在±500ms內有稀疏圖，223個沒有附近圖，
事件source_frame精確圖0。diagnostic event不是遊戲Miss，nearby radius也不是歸因窗。
這輪不能冒稱已做77個Miss的連續影格分析；此前7722張全錄仍完整保留。

下一個離線驗證針對完整接觸鏈，不以增加root數作acceptance：

1. 使用已保留全錄從起點暖機，沿原decision cadence重跑候選observer→GamePlanOwner→
   scheduler→FakeTouch；保存原recorded receipts與counterfactual fake receipts為兩個來源。
   fake-clock明列capture、消費時間與between-frame due派發假設，不能替代實戰注入延遲。
2. 對使用者選取的Hold／多線／Flick片段，逐個proposed物件串接current pixel部位、
   note ID／line ID、root、accepted、Down、每次Move的body支持、tail確認與Up／cancel。
   重點是當前body仍可見時在哪一步消失／改ID／配錯線／owner拒絕；假線角色仍需覆核。
3. 每個修正須保留一個從真實像素暖機到owner失敗的red case，及新Note、body不存在、
   假線／不相容幾何的反例；成功指標是該物件contact連續且Move有當前支持，
   不是少取消、較多Down或合成測試總數。合成／fake-clock只補組合與時序契約。

此完整owner反事實重播與逐物件覆核是下一步設計，尚未實作或驗收；不把本輪失敗
推導為已需模型接入，不用延長missing grace掩蓋、亦不自動追加實戰。
使用者後續另授權[離線CPU學習視覺小試](../07-data-learning/OFFLINE_CPU_VISION_PILOT_20261001.md)，
限定輔助優化，不進主程式迴圈；此新授權不把上述owner重播設計回填為已完成。
