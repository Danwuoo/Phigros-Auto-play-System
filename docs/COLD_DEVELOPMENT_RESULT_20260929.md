# 非學習式 observer／planner 冷開發結果

> 2026-09-29合併審查：原交付SHA與冷結果已獨立核對；修正離線量測器發布時間欄位的資料競爭，並補建置指紋與一項回歸。修補後Release／Debug／ASan各310／310，753幀重播相對交付版語義差異0。原有效能數字仍屬凍結meter，限制及main整合範圍見[驗收紀錄](COLD_DEVELOPMENT_MERGE_REVIEW_20260929.md)。以下保留各版歷史結果，不回填成修補版實測。

## observer47／planner23 本次續作（2026-09-29，最新）

現行冷開發只用既有RGB、獨立合成像素、oracle消融及FakeTouch，未啟動emulator、真觸控或模型訓練。`coverage-manifest.json`經C++ `coverage-validation.json`核對，九組G1–T2及七個高風險交叉的正常／負例代表列 **32／32 passed、0 pending**；這是固定合成矩陣的程序行為，不是實戰命中率或所有形式的窮舉。新增G2獨立旋轉X線、消線／ID交換、單線碎片不多生，G3晚對齊與瞬移撤銷、N1完成後同位置閃片不重播、H1/H2連續Hold及混合指、T1不規則新鮮時間與unknown Down、T2容量與writer飽和等RGB／oracle正反例；N2另驗Z／V／波浪單指Drag串的同contact Move、雙股合流分流不補送Down、Tap後Flick、四邊Flick完整Down／四Move／Up。`c2-n2-t2-c4-verified.log`的15項專項均通過。高密度RGB 1,000幀每幀16線／128 Note且track ID有界；第129顆關門、三個epoch reset無舊接觸、2槽writer確定性滿載時debug drop可計數且critical fault。

C1先凍結`c1-line-gap-sweep-plan.json`的共用幾何參數3／4／5px、三seed正負真值與停止條件。正式C++合成結果`c1-line-gap-synthetic.json`：3px真線漏2／9，4px漏0且假線0，5px在6個負例中造3條假線；因此維持runtime 4px。兩個來源根的753幀／736 unique RGB逐張驗SHA後，3px比4px在Cereris fit家族少1幀線候選、3幀完整語義不同；5px在這批短clip零變動（`c1-line-gap-4-to-{3,5}-diff.json`）。18家族fit／future_validation分組固定，既有RGB無人工truth，零變動不推翻合成假線，也不作真實precision／recall。30clip／90幀仍只有`proposed`，validator拒絕升格人工gold。

C4新增不規則12／19／16／21／22ms間隔下500／750／1000px/s、雙側的幾何root測試，對可判定時刻誤差小於1ms；靜止與遠離速度不造root。R7兩個同deadline Tap以fake RPC返回5／45ms驗順序owner的獨立到期，45ms第二Down拒絕且不重試。這些是合成時間／排程契約，沒有推斷遊戲判定窗口。

C5先前12ms交錯ABBA三批中，預檢雖改善延遲，但第二批因少跳27.5幀而多2.5個Down，超過先凍結的差1容忍；`pipeline-ab-analysis.json`保留`failed`。改用observer47凍結量測器、同一12張RGB／25ms固定cadence，三批全掃描A/A後依預先寫定公式凍結`pipeline47/pipeline-aa-tolerance.json`，再做三批ABBA、每模式每批2,000 attempts。`pipeline47/pipeline-ab-analysis.json`三批均`passed`：recognition p95改善6.74–10.64ms、p99改善7.62–11.24ms；同frame的capture→owner p95改善6.65–10.80ms、p99改善7.53–11.24ms。各批Down同為83、skip均0–0.5、failed／unknown／writer drop均0，RSS每100幀抽樣未超容忍。這只證明固定合成Tap／記憶體FakeCapture的可控冷鏈。

另以16線／128 Note靜態RGB凍結獨立A/A與三批ABBA。`pipeline47-dense/pipeline-ab-analysis.json`較嚴格的「每批都須加速」規則**failed**：第一批略退、第二批改善、第三批改善未超噪聲；每批p95/p99、skip及RSS的退步均未越原§8.6非退步容忍。`c5-global-gate.json`按原計畫「一個已量瓶頸超噪聲整鏈改善、其餘凍結場景不退步」跨場景規則為`passed`，並保留密集場景的嚴格失敗。兩場景各跑10,000 attempts／約80.1s：Tap消費9,043／skip957、Down／Up各834、RSS峰54.46MiB；dense消費5,528／skip4,472、Down0、RSS峰57.67MiB；pool／writer drop、failed／unknown均0，無留指。受控writer30ms／8槽加fake RPC10ms的1,000次負載造成3,097筆可計診斷drop與752次首筆enqueue拒絕，Down／Up各83、failed／unknown0、writer未fault、收尾無留指。dense完整鏈分段診斷另顯示逐列掃描仍最貴；單次full／prescreen線掃描p95為13.62／8.02ms，但不得替代正式密集ABBA失敗。

Release、Debug、ASan全套`ctest-*-47-final*.log`各309／309通過；Debug用169.17s，ASan用393.74s。第一次Release 309項的唯一失敗是舊最小corpus索引沒有`family_splits`而新報表誤要求必填；修為`not_frozen`相容後先紅後綠（`ctest-release-47-final.log`、`replay-family-compat-focused.log`）。`candidate47-23-final-provenance.json`記86個source／config、7個binary及16份關鍵輸出的SHA-256與重跑命令；`capacity-ledger.json`逐檔重算派生容量。C++重新驗`coverage-validation.json`為32／32，`c5-global-gate.json`為passed。按原計畫§8.7，C0–C6的**可冷完成範圍通過**。source render age、真gRPC、實際Hold採納、Flick有效速度、Dlyrotz恢復及HD AP／zero Miss皆未知，必須留待使用者另行安排有限熱測；此處不恢復manual-session。

後續有限熱測／資料缺口清單（須另由使用者安排，不屬本輪執行）：先取得可逐幀覆核的真實長Hold head／body／tail pixels，含按住期間旋轉、Note近線才對齊、遮擋、換線與同時Tap／Drag／Flick；再核對觸控是否被遊戲採納、Flick實際有效速度與失敗釋放。量測真gRPC傳輸、source render age及render→遊戲生效的時鐘關係，不能把本輪host QPC當成它們。有限手動場次先以Dlyrotz及穩定HD作版本與原始證據配對，再測代表性旋轉Hold；光IN單列壓力組。HD AP／zero Miss、Dlyrotz恢復及未知曲泛化均維持未驗，若需人工線／Note／關係裁決，須把`proposed`與人工覆核分開保存。

## observer47／planner23 續作檢查點（2026-09-29）

R6新增36幀旋轉Hold在tail剛入線時角度額外跳變0.30 rad的RGB／oracle正反例。當前連續兩幀有附著tail封口時維持原contact後正常Up；沒有附著tail而只有一幀白色閃片時不假確認tail、不重播Down，失去body支持後安全釋放（`r6-rgb-oracle-initial.log`）。合成像素可證程序行為，實戰tail語義未知。

R3新增X形雙線、兩個各屬一線的Hold之RGB／oracle／FakeTouch正常例與「第二線先可見再消失」負例。未修前，已關聯第二線的Hold改接剩餘交叉線並送出第二個Down（`r3-rgb-line-disappear-red.log`）。observer47要求同一Note至少三次、跨30ms的測量確認line ID；不連續的換線使關聯及root未知。若新ID在原接觸區延續原線局部幾何，一般Hold以兩張新鮮影像重接；已有近線Drag或當前可見Held body支持時可即時重接，兩者均重新累積速度而不沿用舊root。第一版守門使反轉時換ID的活動Drag提前Up（`ctest-release-47-23-11.log`），加入當前接觸支持條件後R3及R5六項專項通過（`r3-r5-all-focused-final.log`）。其餘遮擋、線ID交換與實戰關係真值仍待驗。

R7以相同預測deadline的兩個Tap建立RGB／oracle／FakeTouch序列。第一個Down的fake RPC返回延遲5ms時兩指皆Down；延遲45ms時第二計畫先到自身`valid_until`，排程報`target_evidence_or_window_expired`且不送第二個Down，之後不重播、收尾無留指（`r7-rgb-oracle-verified.log`）。初測把`enabled_types=2`誤設為Tap並錯期待`down_too_late`先發生，保留`r7-rgb-oracle-initial.log`及`r7-rgb-oracle-bit-fixed.log`；修正的是測試設定與到期斷言，正式排程未改。這是fake-clock回傳延遲，未測真RPC併發。

R4以Hold已占一指、Drag到判定線邊才突現的29幀RGB與獨立oracle正反例驗資源守門：首幀無Drag Down，第二張新鮮像素仍支持時，容量二指會送一次Drag Down並保留Hold原指；容量一指則報`contact_conflict`，不搶Hold或補送（`r4-rgb-oracle-initial.log`）。這只證明近線Drag在當前區域策略下的冷行為，不宣稱突現Tap／Flick能在遊戲窗口內成功。

Release、Debug、ASan完整回歸各286／286通過（`ctest-*-47-23-11-286.log`）。observer46→47在兩批逐幀驗SHA的753張／736 unique既有RGB中，候選數及完整語義決策均零變化（`replay46-to-47-semantic-diff.json`）；每組最多三幀，無法證明實景長Hold或換線安全。`candidate47-23-provenance.json`保存17個source／binary hash並重核零差異；`capacity-ledger.json`記baseline資料夾161,034,194 bytes、派生資料持續更新且未複製原始RGB。機讀矩陣G1及R1–R7正反16／32列通過、16列待驗，C5正式完整鏈A/B與多場景長跑尚未完成，整體cold goal仍未達。

observer46另完成一輪10,000幀、8ms確定性抖動的探索性完整冷鏈（`pipeline-46-long-10000-jitter.json`）：發佈10,000、消費8,982、skip1,018、pool／writer drop 0、Down／Up各829、觸控failed／unknown 0、scheduler reject2、收尾零留指；wall80.1秒、RSS每100幀採樣峰54.16MiB，capture→recognition p95／p99 7.04／9.79ms、capture→owner 7.30／10.13ms。這是單一重複12幀Tap場景，與observer45長跑非配對A/B，不用其差異宣稱效能改善；source render age及真gRPC不在冷量測內。

## observer46／planner23 續作檢查點（2026-09-29）

R1新增旋轉兼平移的36幀Hold與同線座標Tap疊近的獨立RGB及幾何oracle正反例。初版RGB案例在前景Tap分割後讓新生Hold片段搶同一組側軌，舊Hold於body仍可見時因missing grace到期Up；一指與二指皆失敗（`r1-rgb-initial-red.log`）。將當前側軌的續接claim先給已有`held_body_evidence`／`head_on_line`的有界歷史，並禁止次級前緣從同一對側軌刪去該body後，兩指能同時保持Hold與Tap，一指則保留Hold且拒絕Tap。更強的正常tail斷言又先失敗：tail過線時窄搜尋窗漏掉第二份新鮮證據（`r1-rgb-tail-gate.log`）；僅當先前已見tail且接近線時，封口搜尋局部窗由12px擴到36px，仍須當前封口／雙側側軌及兩份新鮮證據。修後`r1-rgb-oracle-initial.log`的RGB／oracle兩項通過，並驗同contact、沒有重複Down、沒有失去支持取消、可見tail正常結束。這是合成語義，真實Hold接觸區及Tap採納未知；較寬tail搜尋的整鏈成本尚未完成正式A/B。

Release、Debug、ASan各276／276通過（`ctest-*-46-23-11-276.log`）。observer45→46在兩批已驗SHA的753張／736 unique既有RGB中，候選數及完整decision均零變化（`replay45-to-46-semantic-diff.json`）；三幀冷啟動不能證明實景長Hold安全。機讀矩陣目前G1、R1、R2、R5正反8／32列通過、24列待驗，整體冷goal仍未完成。

## observer45／planner23 續作檢查點（2026-09-29）

G3 的獨立oracle反例把同一已追蹤判定線的切向逐幀交替寫為`u`／`-u`，未修前在第13幀把線追靜止Tap錯判為`motion_discontinuity`並丟失可判定root（`g3-oracle-initial.log`）。observer45只在同一非零line ID的Note歷史內選連續的等價法向；原始當前線候選不改寫。修後該例、真正線反轉撤銷pending Down、RGB線追Tap與既有跳變回歸六項通過（`g3-modpi-final.log`）。用observer44凍結binary與observer45對兩批已驗SHA的753張原始RGB重播，`replay44-to-45-semantic-diff.json`顯示候選數及完整decision均零變化；三幀資料未包含此反例，真實準確率與遊戲效果仍unknown。

R5新增「線反轉＋已送Drag Down」獨立生成RGB正反例：持續有當前Drag支持時，同contact維持至既定`due+75ms` Up；反轉遠離但Note與線仍可見時，舊Down不重播且安全釋放。另一個不經RGB detector的oracle幾何／相對運動序列驗同一語義，三項見`r5-rgb-oracle-final.log`。正常例最初把合法的到期Up誤作提前釋放（`r5-normal-initial.log`），修的是測試期待，不是延長動作期限。機讀manifest現G1、R2、R5正反6／32列passed、26列pending；G3雖已有RGB／oracle／fake-clock正反例，旋轉／速度／Note晚對齊等子項仍pending。合成接觸語義不等於遊戲判定。

離線`analyze game-cold-pipeline`以12張有界預產合成RGB、固定或確定性抖動cadence，讓記憶體FakeCapture經正式LatestFrame、GameObserver、GamePlanOwner／scheduler、FakeTouch與Journal三執行緒執行；用同一frame／intent的host QPC量capture complete→recognition／owner／Down，並報publish、skip、writer、reject、late、每100幀RSS及所有1000次嘗試分母。量測器先因未排空accepted-plan診斷在256筆停止，後因跳幀時混用來源frame與決策序號而無法收尾；已依正式runtime消費診斷並分開兩種序號，失敗輸出保留。未使用ADB、emulator、真touch或模型。

修後探索性1000幀固定12ms鏈：全掃描984張消費／16 skip，capture→recognition p95／p99 14.60／20.31ms、capture→owner 16.27／25.11ms；預檢968張消費／32 skip，對應8.45／11.17ms與8.77／11.61ms。此對照早於最終量測器RSS／輸入hash固定，且drop不同，**不能作C5 A/B驗收**。之後凍結`pas-cold-pipeline-meter.exe` SHA `f07c1d09…2cf05c`和12張RGB合併SHA `5ab18476…09c6c62`；三批全掃描A/A、每批兩次各1000 attempts：消費980–992、skip 8–20、Down 83–84、writer drop／觸控失敗／unknown均0，recognition p95 18.30–18.96ms、p99 20.40–21.30ms，owner p95 18.82–19.36ms、p99 21.40–24.46ms，RSS每100幀採樣峰53.25–53.35MiB。`pipeline-aa-tolerance.json`在正式A/B前凍結各分布與skip／late容忍。另有observer45一輪10,000幀、8ms抖動的探索性長跑（`pipeline-45-long-10000-jitter.json`）：發佈10,000、消費8,831、skip 1,169、pool／writer drop 0、Down／Up各833、touch failed／unknown 0、一筆scheduler reject、結束無留指；wall 80.1秒，RSS每100幀採樣峰54.18MiB，recognition p95／p99 10.33／12.29ms、owner 10.70／12.91ms。單一重複合成場景有大量skip，不能當多場景或observer46長跑驗收。功能矩陣尚未凍結，因此正式交錯A/B、多場景長跑、writer／RPC負載與品質門檻仍待做，C5及整體cold goal保持未達。source render age、真gRPC、遊戲效果均unknown。

## observer44／planner23 續作檢查點（2026-09-29）

正式 observer 新增 host QPC 的 `base_scene` 五子段：combo glyph、逐列線掃描、連通區線解讀、Note 解讀與線追蹤。兩輪原始RGB共251組／753幀逐幀驗SHA後，未優化時舊558幀的逐列線掃描 p95／p99 為8.65／10.03 ms（`replay42-22-10-old-timing.json`），新195幀為8.03／9.77 ms（`replay42-22-10-new-timing.json`）；base scene 的其餘子段各自遠低於這段。子段和等於 base scene，並由合成測試核對。這是離線冷啟動 CPU 分段，不是實際capture→touch延遲。

依該瓶頸新增長線逐列預檢。既有掃描允許最多四個連續非線像素且要求跨度超過畫面寬32%；因此每個完全落在該長run內的五像素區塊必有支持。在1280寬每64像素取五像素，任何合格run至少含六個完整區塊；少於六個才安全跳過整列。`BoundedRowPrescreenMatchesFullScanWithMaximumPermittedGaps`用採樣位置恰有四像素缺口的獨立RGB反例檢查，並同時檢查base scene子段和。預檢與原路徑可由離線benchmark切換；正式runtime預設啟用六區塊版本。它只讀當前pixels，不讀譜面、歌曲或舊動作。

先用三批ABBA、每批兩次／模式的原掃描A/A凍結容忍，舊集每模式n=3348、新集n=1170；按每批p95／p99最大絕對差向上取至0.1ms，舊集為0.2／1.3ms，新集0.3／0.6ms（`row-prescreen-aa-tolerance.json`）。初版只要求一個區塊亮像素，舊集有改善，但新集合併p99僅19.26→19.10ms，且首批退步19.08→21.02ms；保留 `row-prescreen-ab-old.json` 與 `row-prescreen-ab-new.json` 負結果。六區塊版本沿用先凍結容忍重新交錯A/B，對每次重播驗SHA、每幀比對去掉QPC計時欄後的完整decision語義，兩組皆`coverage_equal=true`：

| 原始RGB集 | 每模式n／批次 | 舊掃描observer p95／p99／max ms | 六區塊預檢 p95／p99／max ms | 逐批結果 |
|---|---:|---:|---:|---|
| 舊558幀／186 clips／553 unique SHA | 3348／3 | 15.40／18.12／21.80 | 7.31／9.57／13.01 | 三批p95、p99皆超過原容忍下降 |
| 新195幀／65 clips／195 unique SHA | 1170／3 | 14.46／16.83／21.54 | 7.88／9.97／11.98 | 三批p95、p99皆超過原容忍下降 |

此處微基準在observer44／planner22 binary完成；後續planner23只修改owner的未執行Down撤銷，仍須做新版完整鏈量測。重播循環樣本不能視為不同實戰畫面；沒有量到source render age、gRPC、實際觸控或遊戲效果。效能C5仍缺正式latest-frame／owner／FakeTouch完整鏈、publish／consume／writer及drop／late分母，不能單靠observer微基準驗收。G3另新增線追靜止Tap的14幀RGB→observer→owner→FakeTouch正例，及已確認待執行Down後判定線反向遠離、舊期限到仍零Down的15幀負例；角速、dt、近線對齊及oracle順序變形尚待補。

T1以六張移動Tap RGB經正式`LatestFrame`→observer→owner→FakeTouch驗最新畫面與一次Down，並用三張burst確認只讀第九張、skip=2。負例先紅：新當前RGB已無Note，未執行Tap Down卻因舊40ms失蹤寬限仍到期注入（`t1-initial.log`）；planner23讓未執行Down在較新完整快照缺少目標時立即撤銷，已執行接觸保留原有有限寬限。舊`SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt`因期待舊像素Down而在完整Release先敗（`ctest-release-44-23-10.log`）；改為驗撤銷舊intent、Drag重新可見時由新鮮證據建立不同intent，專項及Release全套262／262通過（`t1-regression-revised.log`、`ctest-release-44-23-10-final.log`）。這是有意的安全契約變更，不是刷新舊golden；Debug／ASan全套亦各262／262通過（`ctest-debug-44-23-10.log`、`ctest-asan-44-23-10.log`）。T1其他掉幀、epoch／geometry、慢RPC與writer故障仍待矩陣驗收。人工遊戲語義與真實Hold命中仍unknown，冷goal保持active。

`candidate44-23-provenance.json`保存本版source與Release／Debug／ASan binary SHA、測試及先前observer微基準來源；`capacity-ledger.json`目前派生資料144,102,597 bytes，未複製兩批原始RGB。未啟動emulator、真觸控或模型訓練。

H2後續用同一長Hold RGB加入較晚的Drag與Flick：兩指時各得獨立接觸，Flick保有完整Down／四Move／Up；一指時已具crossing資格的Flick不搶活動Hold。R2另以兩個相向Hold與Flick做獨立生成像素，初版三個Down但峰值僅兩指，receipt證實下方Hold比Flick Up晚才進來；將生成器下方Hold起始相位提前四幀後，兩Hold先Down且三指峰值成立。相同幾何限兩指時兩Hold各Down一次、Flick零Down。分開的oracle幾何／關聯輸入與FakeTouch容量重跑也通過，`coverage-validation.json`目前G1與R2正反4／32列passed、28列pending；Release `ctest-release-44-23-10-r2.log` 269／269。這些是合成接觸語義，未證明遊戲Hold／Flick判定；本新增測試的Debug／ASan待重跑。

## observer42／planner22 續作檢查點（2026-09-29）

G2 已用當前 RGB 合成像素補 V、連通框、放射、分離雙交叉中心及完全重合後分離的線候選；X 的雙 Note 分線接觸與等距 unknown 仍通過。連通白色區的有界局部線段分割初版讓舊 Hold 回歸把白色內部裝飾誤認為第二條線，先紅後加入沿線相鄰 body 像素守門。舊 RGB 的 Class Memories 一幀又揭露長 Hold 側軌被誤認為兩條直線並新增兩個 Note 候選；加上 8–48 px 的長向彩色／灰色 body 支撐檢查後，該張候選差異消失，獨立合成側軌負例通過。`replay41-to-42b-diff.json`對同一753張已驗 SHA 畫面仍有37張線候選數變化、2張Note候選數變化，線差異分布於七個家族；Dlyrotz clip 8 的直向白線目視存在，但其遊戲語義及其餘變化沒有人工真值，均不報作準確率提升。[PRAW] clip 4 的2張Note數變化落在旋轉方框特效邊；選定三幀的完整`decision-*.json`中新增的邊緣Drag片段均為`insufficient_history`，沒有cold crossing，但實戰warm歷史與遊戲語義仍unknown，不據此宣稱安全完成。

H1 新增36幀判定線持續旋轉兼平移、Hold head/body/tail 同向變動的 RGB→observer→owner→FakeTouch 測試；排程在兩幀之間的預測到期時以 fake-clock 執行。一次 Down、同 contact Move、兩份當前可見 tail 支撐後的正常 Up 與專屬 `game_hold_tail_confirmed` 事件通過。測試曾誤把合法 contact ID 0 當成無 contact，修正測試狀態為 optional 後通過；其餘雙側、局部遮擋、交錯雙 Hold 與 oracle 消融仍待驗。

N1 使用同一RGB線與1至5顆獨立移動Tap，正式鏈各自取得contact；第6顆同時到達時只拒絕新需求，不偷換已開始五指。H2 的35幀長Hold同指續接期間，較晚到線的Tap另用第二指。N2 四邊Flick各經當前RGB與fake-clock執行完整Down／四Move／Up，位置保持畫面內；初版測試缺後續新鮮幀，先紅在證據到期時安全釋放，補正常持續幀後通過，另保存缺幀負例。N1縱連／交互、N2 Drag分合、H2 Hold+Drag/Flick與各自oracle消融仍缺，因此這六列保持pending。

現行 source 為 observer42／planner22／diagnostics9；選定clip的三幀離線重播現在另寫完整`decision-*.json`供關聯與crossing追查。`candidate42-provenance.json`記 source／Release、Debug、ASan binary SHA；最終擴充後的`ctest-release-42-22-9-expanded.log`、`ctest-debug-42-22-9-expanded.log`、`ctest-asan-42-22-9-expanded.log`各257／257通過。`proposals30-observer42/`同30 clip／90幀重新產生，C++ validator通過而 `gold_eligible=false`。`coverage-validation.json`仍只有G1正反2／32列通過，其餘30列待驗；C1有限通用參數實驗、C2其餘矩陣、C3混合Hold及C4–C6尚未達冷goal門檻。容量帳本另記派生資料，未複製原始RGB。未啟動emulator、真觸控或模型訓練；新版的遊戲效果仍unknown，goal保持active。

## observer41／planner22 先前檢查點（2026-09-29）

上一輪結論「部分未達」是里程碑，現已建立 active goal，依[同一計畫 §8–9](NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md)續做 C0–C6；後文上一輪數字原樣保留。C0 已把 observer38／planner21／diagnostics7 的 134 個 source／設定／Release 配套檔保存到 `measurements/cold-goal-20260929/baseline38/`，逐檔 SHA 相符，內容 161,007,847 bytes；`capacity-ledger.json` 獨立記新派生資料（目前約115 MB，硬上限8 GiB），不複製原始RGB。

C1 的正式 C++ `pas analyze game-corpus` 核對兩批 index／RGB SHA、session manifest 1280×720／rotation1 及離線結算的歌曲家族：251組／753張索引RGB、736個不同SHA、18個有RGB的家族；舊資料只留20個round的片段，不能把缺片段的歷史第21輪算進家族數。凍結30組檢視集，每個可用舊家族與七個新round均有入口；選擇依錄製trigger／候選數，不能由此宣稱類型真值。`proposals30-verified/`是observer38的來源與提議contact sheet，現行observer41在`proposals30-observer41/`重新產生同30組／90幀且通過C++ validator：線只保存當前亮色像素支持片段，Note head/body/tail與關係各自可為unknown，候選body框明示不是可見mask。`human_reviewed_truth=false`、`gold_eligible=false`；不能做precision／recall或遊戲結果判定。`indexed_rgb_frames`與`unique_rgb_frames`誤混、重播方向缺欄、session方向指紋未檢三項均有先失敗後通過反例。`replay38-to-39-diff.json`驗753個frame key／hash與兩版輸出：各家族線及Note候選數皆無變化，僅表示當時的observer39在舊RGB沒有候選數差異，不表示觀測正確率或動作效果不變。

C2 G1使用獨立幾何／像素生成器，固定1101／2203／3307三seed，72種四Note類×三線角×兩側RGB正常例，另以頂部UI近似線與近線初現無時序作兩種負例。移動Tap三角度兩側、水平四類Note都走正式observer→owner→FakeTouch，測一次Down及安全釋放；oracle候選四類三角度雙側檢查關聯／root，負例不能造Down。`c2-g1-g2-observer40.log`含G1七測通過，機讀`coverage-validation.json`記2列passed／30列pending。垂直線在原低信心全寬要求下漏檢，先失敗再採沿線方向長度門檻；初版造成Hold rails假Drag，已以方向權重與現有Hold負例修正。G2平行及X交叉雙線有獨立RGB truth，X由一條變兩條的先敗後過測試、兩顆分屬不同線的FakeTouch及等距unknown負例均通過；V／框／放射和重合恢復仍缺。`replay38-to-41-diff.json`驗753幀來源與計數，9幀線候選由1→2（Credits 3、Pixel Rebelz 6），Note候選數不變；針對這三個clip產生source／overlay，目視有X形白線，但未經人工gold，不將它計入真實precision。

C3的35幀連續合成Hold先揭露tail已離線卻以可見遠端body續接；無tail安全失效版本修正後，加入當前橫向tail邊緣與同線確認，區分正常tail Up與缺少tail時的grace Up。`c3-long-hold-tail-positive-negative-final.log`正反例通過，既有灰色body前緣移動／遮擋案例一度因守門過緊失敗，保留`c3-hold-existing-regression-diagnostic.log`後修正，`c3-hold-tail-and-existing-guards.log`七測通過。現行observer41／planner22／diagnostics8；Release `ctest-release-41-22.log` 246／246。實景三幀片段無法提供長Hold真值；旋轉／平移同段、雙側及混合contact仍待冷驗，遊戲命中效果仍unknown。C1有限通用參數實驗、C2其餘矩陣與C4–C6冷門檻仍在進行中，goal保持active。

2026-09-29。範圍是[冷開發計畫](NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md)所定的既有 journal／RGB、合成像素、FakeClock／FakeTouch 與離線 CPU 量測。本輪沒有啟動 emulator、Phigros、real touch 或 manual-session，也沒有採集新遊戲資料。**結論：部分未達冷驗收。** observer38／planner21／diagnostics7 已接回正式 C++ pipeline；P0 追溯、三個具體反例及部分 P2 安全路徑通過，但 G1–T2 全矩陣、完整時序鏈、已證實的效能改善均未完成。遊戲成績仍是歷史 observer37／planner19 的結果，不能歸給新版。

## 1. 固定證據與可重跑入口

- 開發前 53 個 dirty source 檔逐一符合七輪索引的 `source-hashes.json`；開發前核心 source／headers／tests 備份在 `measurements/cold-dev-20260929/source-before/`，再次核對 53/53（`source-before-verification.json`）。獨立的 observer37 binary 凍結與 hash 見[冷開發計畫 §1](NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md)。新 source／Release exe hash 在 `measurements/cold-dev-20260929/development-hashes.json`；其中 `pas.exe` SHA-256 為 `190996dfebd58b8e09d186bd5002df8e135dc4921b0ec8633f70bc1d90452353`。保留原 dirty tree，未 commit／stage／reset／clean。
- 新命令 `pas analyze game-round <round-dir>` 直接依 `summary.json` 讀取分段，核對 `events-0...` 順序及各 SHA-256，跨段維持 analyzer 狀態；拒絕竄改。十四份機讀輸出是 `measurements/cold-dev-20260929/round-1..7-analysis.json` 和 `baseline-round-{3,6,7,15,9,11,19}-analysis.json`。舊欄位仍可讀；新 QPC 欄位缺席就是 unknown，不以零代替。
- `pas analyze game-clips <pixel-clips-dir>` 核對 index、每幀 RGB SHA／大小／幾何／三幀順序，且每組重新建立 observer；`pixel-clips-cold-replay-instrument.json` 記錄 65 組、195 個獨特 RGB 的冷啟動結果。冷啟動每組最多三幀，並非連續實戰 warm track。六張 proposed overlay 在 `measurements/cold-dev-20260929/dlyrotz-clip7/`；無人工 truth 標註。
- 統一 `strategy_version.hpp` 提供 strategy 字串與 numeric 版本，修正舊 manifest 字串 37／19、numeric 36／18 的矛盾；舊 manifest 不回寫。新增 `components/base_scene/held_recovery/tracking` QPC 計算欄位；FakeClock 決策 golden 不寫 host QPC 數值。仍缺 capture delivery、writer enqueue／drop、owner 臨界路徑與 inject 後遊戲採納時間。

在 workspace 根目錄可用已建好的 Release CLI 重算，例如：

```powershell
& 'out/release-v145/Release/pas.exe' analyze game-round 'measurements/game-assist/manual-session-140156097343000/round-1'
& 'out/release-v145/Release/pas.exe' analyze game-clips 'measurements/game-assist/manual-session-140156097343000/pixel-clips'
& 'out/release-v145/Release/pas.exe' analyze game-clips-bench 'measurements/game-assist/manual-session-140156097343000/pixel-clips' --batches 3 --replays 6
```

## 2. 七輪事件追溯

下表的 round 1…7 依序為 Dlyrotz HD9、光 HD7、光 IN12、Pixel Rebelz HD9、混乱-Confusion HD10、FULL AUTO SHOOTER HD9、-SURREALISM- HD9。原始結算與 old→new 分數見[計畫 §2](NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md)，本表只計事件；Hold cancel 不等於 Miss 或提早 Up。`Down p99` 是 `capture_complete → injection_start` 的直接 join（ms），包含計畫等待，來源 render age unknown。每輪 source_frame join missing／ambiguous 皆 0。

| round | frames／events段 | Hold取消：已Down／未Down | 可見tail確認 | Down p99 | 額外事件 |
|---|---:|---:|---:|---:|---|
| 1 | 7,632／2 | 28／1 | 0 | 48.41 | Dlyrotz 下降；三個 trace 見下 |
| 2 | 8,079／2 | 13／0 | 0 | 40.02 | 光 HD 的歷史 Miss 2→1 |
| 3 | 8,094／2 | 34／0 | 0 | 47.79 | 光 IN 的歷史 Miss 67→83；額外壓力組 |
| 4 | 8,758／2 | 46／2 | 0 | 46.86 | Pixel Rebelz 的歷史 Miss 92→68 |
| 5 | 7,864／2 | 79／0 | 0 | 48.40 | Confusion 的歷史 Miss 101→35 |
| 6 | 7,913／2 | 56／0 | 1 | 45.58 | 四筆 Flick `contact_conflict`，各有五個活動指；不能證明譜面要求 >5 |
| 7 | 9,401／3 | 91／3 | 1 | 48.82 | Hold identity 取消增多；歷史 Miss 104→35 |

Round 1 可按 `hold_cancel_traces` 的 note／intent／plan frame 查原始 event：note911／intent335 在 frame9485 因 `identity_ambiguous` 取消，最後 evidence 到取消約 57.01 ms；note906／intent334 在 frame9493 因 `current_object_missing_or_region_lost` 取消，約 61.94 ms；note922／intent336 在 frame9688 因 `current_held_region_unsupported` 取消，約 60.10 ms。三者均已開始接觸。這些是軟體取消路徑，不足以判定哪顆 Note 造成結算 Miss。Dlyrotz clip7 僅 frame9514–9516，不能補回 frame9485／9493 像素；clip7 可見大型 Hold body 與水平線，runtime warm 與冷啟動目標數不同屬歷史長度差異，不視為真值錯誤。195/195 冷重播的 line 數與錄製 warm 數相同，target 數有 28 幀不同，未經人工覆核。

## 3. 改動、反例與限度

| 變更 | 可重現反例與結果 | 尚未證明 |
|---|---|---|
| Note 身分歧義 | `OccupiedAlternativeIsNotAnIdentityAmbiguityWithoutFeasibleSwap`：舊 greedy 把已占用替代 pair 當成兩顆 Note 都 ambiguous，測試先紅後綠；現在檢查完整雙 pair 交換成本，較大複雜 component 保守維持 ambiguous | Dlyrotz 該三顆真實身分、整體 IDSW／Miss 降幅 |
| Flick 近螢幕邊界 | `FlickNearLowerBoundaryKeepsFourFiniteMovesInsideScreen`：舊向下路徑被截成幾乎零位移，先紅後綠；現在依當前 hit 選完整 80px 可用基數方向，不要求箭頭方向 | Phigros 有效滑速／窗口 |
| 待執行 Flick 修訂 | `PendingFlickRevisionRebuildsFullPathFromCurrentHit`：舊通用 `.94*height` 限制扭曲 Down 和首個 Move，先紅後綠；現在以最新當前 hit 重建未執行路徑，已執行 prefix 不重播 | real touch 採納 |
| 指數與混合動作 | FakeTouch 測試五個 Hold 佔滿時第六個明確 `contact_conflict`，既有 contact 不被偷；旋轉 Hold 與同位置 Tap 分別保持 contact | >5 互斥需求不可執行；真遊戲接觸效果 unknown |

其餘 G1–T2 及高風險交叉有大量既有合成測試（見 `tests/game_tests.cpp`），包括線旋轉、局部 Hold rails／tail、Drag 共用、未知／過期 Down、16 線等；本輪新增測試只覆蓋上表和分析器契約。沒有建立每類的 RGB truth、漏檢／假陽性／IDSW 指標，也沒有完成全部正常／干擾 pairwise 組合，故 P1–P3 矩陣仍未驗收。`held_recovery` 的既有支持規則未因歷史取消數直接放寬；正常 tail 只有 round6、7 各一例，不能靠改事件名提高率。

| 組 | 已有代表性合成／FakeTouch 檢查 | 仍缺的驗收證據 |
|---|---|---|
| G1 | `SlantedLineSurvivesIntersectionAndMatchesNoteOrientation`、`OnePixelLineJoinedToHoldBorderStaysObservable` | 全方向與背景／UI 負例的標註 RGB coverage |
| G2 | `IndependentLinesKeepIdentityAcrossReorderingMotionAndTangentSign`、`CloseRealLinesStayDistinctAndExpiryDoesNotBridgeNinetyMilliseconds` | V／框／放射、多中心及重合後恢復的成組真值 |
| G3 | `RotatingLineEquationsIgnoreAlongLineCropCenterDrift`、`RelativeApproachSeparatesEquidistantUnrelatedLines`、`JumpAndReversalRequireFreshMeasuredSegments` | 速度／角速／dt 分層的 root 誤差與拒絕分母 |
| N1 | `NestedRibbonsMergeButEqualWidthOverlapsRemainDistinct`、`SixthSimultaneousContactIsRejectedWithoutStealingFiveActiveFingers` | 密集縱連、交互、1–5 多押的完整像素到 contact 正反例 |
| N2 | `DenseColocatedDragsShareOneActiveContactAndKeepTapIndependent`、兩項近邊界 Flick 修正 | Z／V／X 路徑與四邊 Flick、分流合流的完整 RGB／觸控驗收 |
| H1 | `RotatingCurrentHoldKeepsItsFingerAndMovesOnlyWithCurrentBody`、`RotatedTailNeedsTwoCurrentSamplesOnItsOwnLine` | 連續長 Hold 的 head→body→tail 像素／receipt／遊戲效果鏈 |
| H2 | `FiveHoldsKeepIndependentContactsUntilCurrentTailsActuallyPass`、旋轉 Hold＋同位置 Tap 新反例 | 雙側 Hold＋Flick、多線交叉＋兩 Hold、RPC 延遲＋同期限 skew |
| T1 | `ContextAndDispatchGuardRevokePendingAndActive`、`MissingAfterStartedOrCompletedDownCannotRetryReturnedPixels` | 全鏈 owner／writer drop／unknown 收據負載分布 |
| T2 | `SixteenLinesRemainBoundedAndContextGapStartsNewLineage`、archive 上限測試 | 128 Note 密集長跑、記憶體／RSS 與 ring／writer 滿載測量 |

## 4. QPC 成本與被否決的優化

195 張獨特 RGB、Release、冷啟動三幀片段重播，直接 QPC 計算（ms）：

| 階段 | n | p50／p95／p99／max |
|---|---:|---:|
| 全 observer | 195 | 8.382／14.841／18.135／20.046 |
| component | 195 | 2.444／4.478／4.939／5.064 |
| base scene | 195 | 5.053／10.809／11.089／11.221 |
| held recovery | 195 | 0.052／2.317／4.319／6.864 |
| tracking | 195 | 0.020／0.045／0.062／0.117 |

`base_scene` 含全行線掃描及當前 Note／線解讀；此數據指出候選瓶頸，未隔離其內部子段。對 component mask／queue 預配置做同機交錯 ABBA 三批、每模式每批 1,170 次、總 n=3,510／模式（只是 195 張反覆重播）。最終 binary 同 corpus、每次 hash 驗證、output equality=true：原 local allocation p95／p99／max=12.469／15.103／21.549 ms；reuse=12.680／15.338／21.481 ms。三批各自 p99 的 reuse 差為 +0.01／−0.70／+0.59 ms，方向不一致。前一次獨立執行也無改善（`ab-scratch-bench.json`）；最終結果在 `ab-scratch-bench-final.json`。正式 runtime 預設維持原 local 路徑。沒有把重播 3,510 次當成 3,510 個不同實戰樣本；沒有調高 drop／expiry 或放寬時限。完整 capture→owner→FakeTouch 鏈與真正增益尚未量成，P4 效能門檻未達。

## 5. 驗證、容量與後續停點

- Release、Debug、ASan `ctest --preset windows-{release,debug,asan} --output-on-failure` 各 230/230 通過，紀錄 `ctest-release-instrument.log`、`ctest-debug-final.log`、`ctest-asan.log`。其中 ASan 使用獨立建置；測試沒有替代熱測。
- 新 analyzer 的 bounded trace 最多 128 筆、recent released intent 32 筆、source_frame decision join 512 筆；clip replay 600 frames／200 clips、index 16 MiB 上限，核對 RGB bytes／hash。既有 runtime 三 buffer／latest1、單 touch owner 及期限未改。新增冷開發資料約 6.8 MB（報告寫入前），既有 clip corpus 196 檔約 539 MB；新程式未寫入新的遊戲 capture。
- 本輪停止於 emulator 啟動之前。待完成：G1–T2 有真值的代表性 RGB／oracle／fake-clock 正反例；Hold 接入、body、tail 的完整事件／像素鏈；capture／writer／owner 完整 QPC 鏈及丟幀／分母；針對 base scene 已證實且超出 A/A 噪聲的整鏈效能改善。下一階段若要判定真實 Miss 原因與 Flick 有效性，需使用者另行安排 manual-session 熱測，不從歷史結算反推新策略成績。
