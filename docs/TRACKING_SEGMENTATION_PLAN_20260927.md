# ByteTrack／OC-SORT 追蹤對照與小型分割資料準備計畫

日期：2026-09-27。狀態：**使用者已授權開發；本文件為待實作規格，不代表 tracker、資料集或模型已通過驗收。** 交由既有 GPT-6 Sol／xhigh task「繼續優化 Phigros：修正 Hold 身分與 HD AP」續作。工作區沿用桌面專案，先完成本計畫的兩條工作線，再依證據決定是否替換正式追蹤器。此輪準備模型資料，不開始模型訓練或導入推論服務。

## 1. 目標、範圍與起點

本輪交付：可替換的 C++ 追蹤介面、現行基線與 ByteTrack／OC-SORT 思路的可重現對照、有界同源像素採樣、可檢查的標註格式與首批資料、比較報告及下一步決策。追蹤與資料工作可交錯進行，但正式遊玩時不得並行 build、test、第二擷取或重度離線分析。

起點 `2752734`（main），production source 修正提交 `81f29b0`，observer29／planner8／diagnostics2。現有 Release／Debug／嚴格 ASan 各 113/113 是歷史基線，本輪修改後須重新驗證。v75 Glaciaxion HD 為 356 Perfect／1 Good／0 Bad／36 Miss，未 AP。兩張曲中 PNG 已存在並重新核對 hash；沒有1191–1196完整逐幀原圖，不能用事件 JSON 假造該段像素回放。

- 現存 run：`measurements/game-assist/cpp-observe-17904849394349323/`，含 events、manifest、summary、兩張診斷圖與 result-manual。
- Hold PNG SHA256：`71a08f064f7fed821dcc8f456755129dd7f285f4914cb426a381d4a31a85cb29`。
- combo PNG SHA256：`79bc66feba6d77489f158990886541bc41e3ae637c724ca5623cd17a0d6a33c7`。
- 背景與限制：[視覺輔助研究](VISION_ASSIST_RESEARCH_20260927.md)、[HD 交接](HD_ACCEPTANCE_HANDOFF_20260927.md)。36 Miss 的逐音符成因仍 unknown。
- 舊4c3c source已在main祖先，舊ignored measurements未確認恢復。不要刪除、封存或清理現有工作區／量測，也不要把Git commit當成圖像備份。

不重開 capture 選型、不新增大型 Fixture 階段、不改 lead35ms／兩指、不調寬 source／target100ms、rail anchor90ms、missing60ms，不修改 completed intent 防重播、停止釋放與單一 touch owner。正式流程全 C++20、單程序多執行緒；legacy凍結。第三方原版程式可研究，不能用Python實作自有正式分析／資料工具。

使用者此輪授權開發、離線驗證及資料準備。需要新遊戲畫面時，沿用使用者自行登入／選曲／重試並告知已就緒的流程；不要自行導覽或反覆重開曲目。就緒前先完成所有不依賴新畫面的工作。HD首次AP才進IN，不能為擴充資料提前切IN。

## 2. 已核對的上游來源

已透過官方GitHub取得固定commit的核心檔、README及LICENSE，僅供閱讀，未執行上游install或下載權重／MOT資料。完整shallow clone傳輸未完成，已停止；可用的是下列**14檔選取快照**，不是完整clone。位置為 `out/research/tracking/reference-sources/`，逐檔URL／SHA256見[來源索引](TRACKING_REFERENCE_SOURCES_20260927.json)。缺檔可依索引重取，不能依賴未完成的 `out/research/tracking/ByteTrack/`。

| 來源 | 固定版本 | 本輪借用及限制 |
| --- | --- | --- |
| [ByteTrack](https://github.com/FoundationVision/ByteTrack/tree/d1bf0191adff59bc8fcfeaa0b33d3d1642552a99) | `d1bf0191adff59bc8fcfeaa0b33d3d1642552a99` | 高品質候選先關聯，再以低品質當前候選續接；低品質不能直接建立新目標。參考 `yolox/tracker/byte_tracker.py`、`kalman_filter.py`、`matching.py`，及ncnn C++版。 |
| [OC-SORT](https://github.com/noahcao/OC_SORT/tree/8462e7e729a93ccd3bd995c0a79a890336cb3a0b) | `8462e7e729a93ccd3bd995c0a79a890336cb3a0b` | 觀測方向關聯、依最後實際觀測重接及遮擋後狀態修正；參考 `trackers/ocsort_tracker/{ocsort,association,kalmanfilter}.py`。 |

原版ByteTrack含frame-rate換算lost buffer與固定步長Kalman；OC-SORT亦有以frame age取觀測、內部虛擬軌跡與累積history。本專案需改為QPC實際dt、嚴格有界歷史，虛擬點只可修正估計器內部狀態，絕不可變成觸控證據或回填過去指令。OC-SORT官方repo的C++部署範例可參考介面，但README示例含需要修正的迴圈／邊界處理，不能整段照貼。

兩repo根LICENSE目前皆MIT；若複用實作，逐檔核对來源／內嵌授權（含Kalman、LAP等），保留原文與copyright，更新 `docs/THIRD_PARTY_NOTICES.md`。本輪優先採C++實作適配後的思路，不引入YOLOX、ReID、TensorRT、ncnn、完整OpenCV等與追蹤對照無關依賴。確有矩陣／assignment需求時採最小且鎖版本的方案。名稱用 `byte_association`／`oc_observation`，明示是本專案適配版，不聲稱重現論文MOT分數。

## 3. 現行程式的切入點

| 現行位置 | 責任／本輪處置 |
| --- | --- |
| `include/pas/game.hpp` | `NoteCandidate`、`GameTarget`、`DecisionSnapshot`、`GameObserver::History`；加入可追溯觀測與tracker契約，保持planner adapter邊界。 |
| `src/game.cpp` | UI、line／note pixels、以歷史引導的rails修復、greedy association、相對撞線擬合集中於此；先抽離追蹤切面，保留原版可選基線。 |
| `src/runtime.cpp` | capture／perception／action分工、當前兩張diagnostic；新增有界採樣與dry對照，真實input權限不交給shadow。 |
| `tests/game_tests.cpp` | 已有相鄰Hold、薄Tap、暖色特效、稀疏幀、線移動及期限負例，作為不可退步案例。 |
| `apps/pas/main.cpp`、`src/config.cpp` | 新離線比較／資料驗證入口、嚴格參數與版本化manifest。 |

抽出的模組可命名為 `game_tracking`、`game_dataset`（include/src/tests對應），不要把新功能繼續堆進單一巨型 `game.cpp`。先以既有113項及像素序列比對驗證無行為改動，再加新方法；不要凍結第二份production observer source長期分叉。

**重要：現行候選提取會讀取tracks／rail anchor。** 相同像素不必然等於相同候選。對照分成兩層：

1. **關聯隔離比較**：一次產生不可變 `CandidateBatch`，餵給所有tracker；批次包含提取版本／歷史來源。若保留baseline引導的候選，標為 `baseline_guided`，結果只支持「在這組候選上」的結論。不能把它當各自完整pipeline的比較。`recent_identity`屬提取器的hint ID，不能硬套進其他tracker的ID空間；用共同幾何／證據，或有明確映射且標註的hint對照。
2. **完整pipeline比較**：同一像素序列，各候選用自己的有界歷史驗證同一套rails規則；測候選提取與tracking互相影響。一次只改一層，報告分開列。若抽離guided detector尚未完成，標 pending，不把第一層結果宣稱可直接換正式版。

當前 `.55`、`.65` 等手工confidence不是校準過的機率，不能直接照抄上游 `.1/.5` 切分。新增強／弱當前證據分類，先以規則特徵與標註development集定義；不足以辨認的特效／背景候選必須可拒收。tracker無法解決「完全沒產生候選」的灰色Hold漏辨，此項單列為觀測覆蓋缺口。

## 4. 觀測、追蹤與執行資格契約

### 4.1 輸入與輸出

`CandidateBatch`至少記錄：schema、extractor／quality rule版本、SceneContext、capture_ns、提取start/end、UI/source/capacity資格、line候選與穩定candidate ID、Note類型與部分幾何、原圖坐標、品質特徵、evidence origin、可見head／tail／左右rail／body支持、裁切／遮擋／unknown欄位。歷史hint記錄其真正來源frame／age，不能改名成當前證據。

`TrackedObservation`至少記錄：track ID／birth identity／revision、tentative/observed/lost/retired、匹配candidate ID／stage／cost／競爭差距、最近實測時間、最近充分支持時間、估計中心／速度／不確定度、line ID、當前支持部位、`prediction_only`、`identity_supported`、`action_evidence_valid`及拒絕理由。未知head／tail不填猜測值。全部時間使用整數QPC ns。

三種資格獨立：

- `strong_current`：可建立新track；建立可執行plan仍須原有UI、歷史跨度、撞線與owner檢查。
- `weak_current`：僅與既存track續接；身分續接不等於足以更新plan。只有當前完整可解釋的rails／body／head證據通過專用驗證，才可更新相應action evidence；body支持不能刷新看不見的tail或過期rail anchor。
- `prediction_only`：只有估計，允許短期保留身分供之後比較；不出現在可執行完整target集合、不更新evidence／expiry、不避免missing取消，不建立或重播Down。

不得用弱候選跨接兩個已執行／完成的Note身分。completed intent的記憶不能由track重新編號绕過；重接超過missing期限的Hold，僅可恢復診斷身分，不能重啟已完成接觸。存在無法解開的競爭時輸出ambiguous，不能為降低ID switch隱藏歧義。

### 4.2 幾何與預測

Hold以leading edge、body rails與可見tail分開表示；不把bbox中心當作頭部。matching使用類型相容、頭部距離、寬度／方向、body重疊、line-relative幾何等有解釋的項目；IoU只能是其中一項，薄Tap与縮短Hold需有特別負例。先做hard gate，再做有界一對一assignment；一個candidate不可同時續接兩track。

初輪保留現行撞線擬合與命中投影，tracker只影響身分／歷史選擇，避免同時替換時間模型。Kalman／運動估計以QPC `dt`更新transition及process noise；NaN／非正dt／倒退／epoch／geometry重置均有拒絕路徑。長間隔不得用clamp後的小dt偽裝連續。現行≥30ms跨度、最多6點／90ms／10ms bucket繼續適用於可執行的撞線證據。

line tracking為獨立可消融選項：最多16條、最多6個當前實測點／90ms，處理theta modulo pi、切向符號一致及多線競爭。基準先保持現行line selection；後續才比較line ID／innovation／方向連續性。不能因平滑而延遲真正移動／旋轉，也不能讓無當前像素支持的線啟用touch。物理移動與量測噪聲在合成已知真值中分開測。

### 4.3 有界狀態

- 每方法最多128個Note tracks，active/lost/tentative合計；每track最多6個實測點／90ms。身份最後實測達100ms即退休，不照搬上游數十幀lost buffer。
- 強弱候選合計最多128個；達上限明確記錄overflow，沿用capacity失效路徑，不能靜默擴容。assignment matrix最多128×128，臨時buffer、成本及最壞迭代有界。
- OC內部修正最多6個虛擬點，只限最近90ms內的估計器修正，另標synthetic，不計入實測samples／history span，不改寫已發布snapshot或owner歷史。
- 全部ID皆在run/context命名空間，ID不重用；完整snapshot語義保留，跳幀不丟失取消。
- 離線各方法串行讀相同資料；線上一次僅一個候選shadow，容量一mailbox，落後丟掉舊批次並記錄skip。不讓shadow持有capture pool lease、等待真實owner或共享可改track state。

## 5. 追蹤對照矩陣

| 方法 | 改動 | 要回答的問題 |
| --- | --- | --- |
| T0 legacy | 原greedy／現有歷史與line／predictor | 無行為回歸的基線。 |
| T1 byte_association | 同樣幾何gates；強候選assignment後，以弱當前候選續接未配對既存tracks | 部分觀測是否減少fragment／ID switch，而未增加false continuation。 |
| T2 oc_observation | 觀測方向成本、最後實測重接、可開關的有界observation-centric修正 | 遮擋後與非等間隔運動是否更穩；沒有足夠跨度時方向項必須失效。 |
| T3 combined | T1低品質續接＋T2觀測方向；僅T1/T2有支持後評估 | 兩種收益是否互補，避免無理由增加複雜度。 |

先用同一候選bank、同一預測／planner fake backend做T0–T2；T3及line tracking各自消融，不能一次開完後聲稱找到原因。OC re-update關閉／開啟亦分列；若內插只增加成本而無收益可不採用，保留對照與理由。

資料分成deterministic合成像素序列、實際ROI標註序列、有限live shadow。合成只是短回歸，不新增大型遊戲模擬器或Fixture平台。v75兩張單圖僅測候選覆蓋，不可測ID switch或追蹤恢復率。ROI片段的完整scene／UI不在圖內時，只測區域tracker，不假造整張HUD、不能用ROI通過來宣稱完整observer gate已驗證。

必要場景：單Hold清楚→暖色遮擋→灰色body；完全消失／重現；相鄰兩Hold；同一Hold內部分片；薄Tap穿過Hold；同時黃色highlight；短尾／裁切tail；note交叉；多線／假垂直線；真實line平移與旋轉；純line量測噪聲；錯誤類型；候選重複；dt=10/16/33/51ms及60/90/100ms邊界；441ms來源空窗；倒序／重播／epoch／geometry變更；128上限與停止。

預先固定development參數及holdout資料清單後才比較。每種方法先報：

1. 按可標註實例／frame的候選召回、誤檢、重複、ID switch、track fragment、錯誤合併／續接、歧義拒絕、重接耗時及不足資料數。每項有分母／定義；不可只報tracks數或cancel下降。
2. fake owner：每真實可見實例的Down數、duplicate Down、missing後Up期限、source失效／stop釋放及completed不重播；遊戲judgment仍unknown。
3. 已知合成真值的撞線時間／hit幾何誤差。真實影片只能報畫面可觀察的區間與不確定度，不能把Perfect／Miss反推為逐Note真值。
4. Release成本：提取、association、prediction、copy、host residency、shadow skip，均記n／p50／p95／p99／max；可用資料少於尾端分位所需時明示稀疏，不把重播次數當獨立場景數。

離線性能依同序列輪換T0/T1/T2，至少3批、每方法至少10,000次更新（短序列可重播但標出唯一frame／clip數），熱身排除方式固定，記工具鏈／binary hash／主機／負載。正確性統計只算唯一資料一次。offline decode與tracker計時分開，不把離線吞吐等同live延遲。

## 6. 同源有界採樣：診斷與資料集

保留預設不存圖；新選項及上限進manifest。先發布正常decision，再從**同一張已消費frame**複製獨立像素buffer；不可在capture callback編碼、不可建立待辨識影格佇列。shadow／資料功能都沒有input backend。

### 6.1 小型失效診斷

沿用研究文件規格：最多2個事件×4張ROI＝8張／run。事件可由Hold候選消失、identity競爭、line innovation、combo字形消失觸發；記錄原因，不當作Miss真值。優先前1張＋當前＋後2張實際消費frame；若缺圖保留實際張數與QPC gap，不補插值圖片。新契約升級diagnostics版本，舊flag行為變化寫README。

### 6.2 額外的模型資料採樣模式

只有異常8張不足以建立資料，也有選樣偏差。新增獨立opt-in dataset profile，明示本輪額外保留額度；不可暗中擴大原diagnostic flag。

- 起始上限：每run最多16 clips×8張＝128 ROI，ROI最多640×384、原生像素1:1、RGB888。選擇一個固定ROI涵蓋整段clip，clip內坐標一致；物件出框標truncated，不暗中換中心。
- 每clip為前2張、trigger當前、後5張實際消費frame，最長250ms窗口；間隔按實際QPC記錄，跨epoch／geometry即截斷。不足8張不能用複製填滿。
- 前置歷史只留2張完整已消費frame的**獨立診斷副本**，不參與runtime決策；每一時刻最多一個clip正在採集，其餘trigger記dropped reason。完成128張不再採集。
- 128張最大RGB約90MiB，加2張1280×720約5.3MiB；含診斷8張約5.6MiB、metadata／buffer overhead後，新增診斷／資料raw配置總上限128MiB，逐byte檢查，不能以vector reserve當保證。更大source geometry先按總上限縮減額度／拒絕配置，不擴大記憶體。PNG逐張編碼需另列scratch peak。
- 全部PNG／mask編碼與磁碟寫入在touch停止後進行。停機flush有期限與partial manifest；空間不足、overflow、崩潰遺失有計數，不阻塞觸控或宣稱資料完整。磁碟每run額度256MiB、整個本輪dataset cap2GiB；達額度即停止採樣、保留已有檔，禁止自動刪舊資料。
- 分配起始16 clips：8個異常／難例、4個正常Note／line、4個不依賴detector命中的空白／背景／裝飾窗口。依elapsed host QPC的有界分層抽樣或固定seed採樣，只决定保留圖片；不能用曲名、歌內時間／frame序列腳本決定觸控。候選完全漏辨仍有機會進入資料。
- 每張保留run／clip／frame／capture QPC、geometry、ROI原點／尺寸、是否裁切、採樣原因、source／binary／config hash及像素SHA256。runtime Note ID只作diagnostic hint，不當標註ground truth。

資料採樣模式與純效能run分開報告；至少用合成／離線A/B測copy overhead，live有明顯尾延遲或skip增加則停止新增採樣，保持原owner期限。勿為了收滿quota持續重試遊戲。

## 7. 小型分割資料規格與品質

### 7.1 標註的最小可用格式

使用原生ROI PNG、8-bit semantic mask、16-bit instance mask及JSON sidecar。先建立版本化schema與C++ validator／exporter，不要求本輪做完整GUI。可使用已存在的第三方標註工具輸出polygon/RLE，再由C++轉換與校驗；未人工／逐圖核對的預標註保持 `proposed`，不進gold set。

| semantic ID | 類別 | 規則 |
| --- | --- | --- |
| 0 | background | 已檢查且確定無目標的可見背景；不是未標註區。 |
| 1 | judge_line | 可見判定線；裝飾線不自動歸此類，語義不明則ignore。 |
| 2 | tap | 可見芯／輪廓，與Hold內薄Tap獨立標。 |
| 3 / 4 / 5 | hold_head / hold_body / hold_tail | 同一Hold共用instance ID；看不見的head/tail不補畫。頭尾無明確分界時只標可確定區，交界ignore。 |
| 6 / 7 | drag / flick | 保留可見類型標誌；不從預期動作猜分類。 |
| 8 | hit_effect | 可辨識的可見命中特效。混合像素無法唯一歸屬時ignore。 |
| 255 | ignore | 未標註、被遮擋、邊界不確定或語義無法確認區，不算background。 |

instance 0為背景／無實例，65535保留unknown；Note實例在同一clip內連續，跨clip不強行連接。另有line instance及可見centerline／endpoints標記，方便量測線位置。sidecar記visible／occluded／truncated、type uncertainty、gray／warm／adjacent／overlap等tags、annotation author/reviewer／版本／來源hash。只標可見表面，不做amodal補全；被特效完全遮掉的Note像素不畫不存在的mask。

單一semantic mask在重疊處標可見上層，無法分辨用ignore；各Hold部位由instance ID串聯，避免分割訓練資料把相鄰Hold當一塊。runtime evidence與人工標籤分離，不能用模型／規則自己輸出的ID作正確答案。annotation view可展示前後pixels，隱藏預測類型／觸控結果後再覆核，減少先入為主。

### 7.2 收集與切分

先把v75兩張圖建立來源索引、少量確認過的visible標註及unknown清單，作為格式pilot。新資料第一輪目標為200–400張經核對ROI、至少3個獨立run；此數量是可行性pilot，不代表足以训练或驗收模型。之後視coverage才擴至約800–1280張／至少8個run，本輪硬cap1280張（10個run×128）及2GiB，若需要超出另在報告提出理由；不能只為湊張數重複相鄰圖。

分層覆蓋：正常四種Note、Hold灰化、暖色遮擋、內部碎片、相鄰Hold、薄Tap重疊、真實線運動／旋轉、多線／裝飾、空白負例、裁切。先列coverage count，以有效實例／clip數判斷是否缺資料。HD未出現的場景保留 `absent_in_HD`，可有獨立合成challenge，不能冒充實戰樣本。

train/validation/test以run為最小分組，全部同clip／近鄰／增強版本留同split。8個run時起始5/1/2，按coverage調整但先凍結manifest。另做跨run的原圖hash及近似重複檢查；同一首歌重打可能像素近乎相同，不能只靠run不同宣稱無洩漏。近似重複群整群移到同側或從test排除，導致test不足就報不足；禁止用歌曲時間／譜面對齊作為標註來源。

只有Glaciaxion HD時，test僅支持這首HD的保留場景，不能聲稱跨曲／IN泛化。完整資料太少時僅做development＋鎖定小challenge，`training_ready=false`，列缺口；不假造大規模train/test。

### 7.3 QA與交付

C++ validator檢查image/mask尺寸與hash、class／instance範圍、part-instance一致、polygon界限、ROI反變換、QPC順序／epoch、重複與split洩漏、unknown不被默認background。每個小pilot樣本逐圖看原圖及overlay；正式資料至少覆核全部困難例、validation/test、其餘train抽20%，記review status，不把單一自動overlay當獨立人工審查。

overlay／contact sheet由C++本地工具產生，保留lossless原圖不覆寫。資料根建議 `measurements/vision-dataset-20260927/`；原圖／mask不上傳、不進Git，Git只放schema、標註規範、聚合coverage及不含敏感資訊的相對路徑hash index。資料來源含帳號UI時裁切排除。必要備份在有足夠空間且可核對hash的第二實體位置作副本，記實際路徑；沒有第二副本就明示single-copy，不能宣稱已備份。

未來模型評估需保留per-class IoU／Dice、薄邊界與line位置誤差、instance誤併／分裂、可見Hold召回及p99成本，不只平均mIoU。模型架構、訓練框架、GPU provider、ONNX／quantization屬下一輪決策；本輪不下載大模型、不排cloud job、不新增自有Python training code。資料格式避免綁定某一模型。

## 8. 執行順序與完成條件

| 工作包 | 內容 | 可驗收交付 |
| --- | --- | --- |
| P0 基線保全 | 讀AGENTS／五份核心文件／本計畫；核對HEAD、dirty、上游索引、v75檔；建立實作分支與evidence index | 基線hash、缺失清單、分批commit計畫，不覆寫其他修改。 |
| P1 契約與採樣 | 抽離候選／tracker接口且T0行為等價；完成診斷8張及dataset有界採樣；schema／validator與v75 pilot | 重構回歸、記憶體／上限／stop／partial dump證據、可檢視首批overlay。 |
| P2 追蹤對照 | 實作T1／T2、獨立line可選接口；短合成像素與fake owner；離線paired runner／report | 失敗先重現、負例不可放寬；T0/T1/T2同input報告，分離觀測缺失。 |
| P3 真實資料 | 使用者就緒後同capture採樣；標註／QA／split去重，與P2缺例交錯補充 | 200–400 ROI pilot或清楚的實際數／缺口、可驗證格式與coverage、無偽造連續原圖。 |
| P4 決策 | development調參、凍結holdout；必要T3／line消融；三配置測試、成本對照、有限shadow | 候選採用／保留baseline／資料不足的具體結論及報告；不保證必須有winner。 |
| P5 可選正式接線 | 僅候選證據足夠且既有資格不弱化時，接回既有owner；固定35ms／兩指HD有限驗證 | 完整結算與raw/hash；AP與否按可見結果，不把shadow等同遊戲收益。 |

必要新CLI（名稱可依現有CLI調整，文件與help一致）：`pas analyze tracking <dataset> --methods ...`、`pas dataset validate <root>`、`pas dataset export <root>`；新live配置用明確的shadow／dataset opt-in，不接受離線檔路徑作assist決策來源。離線runner只可建立FakeTouch/DryTouch，測試證明它無法建立GrpcTouch。

P1/P2功能完成即繼續P3所能完成的資料準備，不在空介面／空schema停工。若缺使用者選曲就緒，清楚報告已完成軟體與待取實戰資料；保持工具、測試、已有PNG標註與文件可交付，不能宣稱資料目標達標。此task不因模型尚未訓練而擴大本輪範圍。

## 9. 驗收、性能與正式替換界線

必過不變式：prediction-only／低品質新候選不能Down；無當前充分支持不能續命；假tail不能延長Hold；60/90/100ms邊界及epoch／geometry重置；相鄰Hold仍獨立、薄Tap不被吞；未知線不能啟用touch；completed不重播；停止／來源失效及input未知狀態仍撤銷釋放；資源有界且shadow不影響owner權限。

完整Release／Debug／嚴格ASan回歸在最後source版本各跑一次並留log；中途只跑相關短測。ASan不關閉必要檢查，第三方未插樁限制明示。不要用sanitized數據評性能，不重跑無關擷取矩陣。

候選需要在預先凍結的困難集減少fragment／ID switch，且無新增錯誤合併／false continuation／duplicate Down；逐例列得失。若樣本不足、只改善synthetic或提高recall卻增加危險續接，保持T0預設。可觀察的拒絕有其用途，不以取消數變少直接排名。

性能為相同輸入／相同負載的對照，正式 `performance_pass` 仍pending，不能擅自取代既有尚未決定的數值門檻。報告新增association／copy成本、p95/p99差、最壞更新、跳幀與資源峰值，提出可行預算建議。對觸控期限有負面影響則候選只留shadow／offline；不能以提高期限掩蓋。

新正式接線之前先完成對應不變式與三配置測試，再使用者準備好的HD上做有限驗證。遊玩中凍結binary/config、停build/test／第二capture，記錄source cadence以避免把環境差異歸因tracker。若通過開發仍不足以採用，交付有證據的negative result亦是有效成果；HD AP是後續主線目標，不是本輪每項研究必须達成的承諾。

## 10. Git、文件及交接

規劃文件由本task提交；開發task在桌面同一checkout以 `codex/` 分支分批提交，開始前再次確認沒有其他task寫入。此輪沒有要求新worktree，沿用現有build與ignored資料；不自動merge／push或清理舊worktree。這不推翻先前已完成的main merge。

開發完成同步README／ARCHITECTURE／ROADMAP／視覺研究及HD交接；新增 `docs/TRACKING_COMPARISON_20260927.md`、`docs/VISION_DATASET_20260927.md`（或等價清楚名稱），包含：實際版本／改動範圍、命令、source與binary hash、環境／n／分布、資料QA／split／缺口、raw絕對位置、通過／未通過／未測清單、是否保留T0、下一階段模型所需條件。不要把本計畫的預期數量或門檻填成已實測。

## 11. 開發task首輪指令

依P0→P1→P2開始，資料schema／既有PNG pilot同步準備；不需再問是否可以開始這些已授權工作。把進度拆成可review提交，避免只回覆另一份計畫。先保留production T0及所有期限；完成契約／採樣／tracker／分析與必要測試，再處理需使用者準備的live資料。模型訓練與推論接入另案，不能以「需要模型」跳過本輪追蹤對照，也不能用tracker預測遮蔽觀測缺口。
