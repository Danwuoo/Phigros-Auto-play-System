# 現行架構與資料契約

2026-09-29。現行開發版 observer47／planner23／diagnostics11，單程序 C++20；最近七輪實戰仍是 observer37／planner19，先前兩輪為舊版基準。判定線修正見[實作紀錄](JUDGMENT_LINE_M1_IMPLEMENTATION_20260928.md)，後續設計見 [跨曲學習研究](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)。本文區分已存在的執行契約與未實作的學習式方案。

2026-09-29補充：observer37／planner19已完成七輪實戰（六HD＋光IN）；原始結果見[冷開發計畫](NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md)。[冷開發結果](COLD_DEVELOPMENT_RESULT_20260929.md)記錄新版的分段SHA驗證、source_frame事件join、QPC子段、Note已占用替代配對與Flick路徑修正。固定擷取／時鐘／latest-frame／單owner契約不變；本輪只離線開發。下述為當前實作，不代表所有判定線與Note形式已驗收。

續作驗收以[同一冷計畫§8](NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md)為準，C0–C6可冷範圍已通過；這是離線工程證據，尚無新版遊戲效果驗收。資料回饋限定離線標記／驗證與通用常數修訂；runtime仍只用當前pixels與有界追蹤，不讀標籤、歌曲家族或舊觸控。完整冷鏈已以FakeCapture／FakeTouch連正式pipeline，時序語義用fake-clock、CPU與排程成本另用QPC，沒有量到emulator或遊戲生效延遲。

observer44起`base_scene`另有host QPC五子段，顯示逐列線掃描主導兩批原始RGB的冷啟動CPU時間。每64像素抽五個當前RGB像素；在1280寬，少於六個有線色支持的區塊不可能形成舊規則所需「跨度>32%且連續缺口≤4」的長run，故可跳過完整掃描。離線ABBA保留全列路徑作對照，逐幀完整decision語義相等；兩批各三批observer p95/p99皆超過事前A/A容忍下降。此項只證明離線observer微基準，完整鏈效能與實戰效果須各自驗。

planner23將「已Down接觸的短暫missing grace」與「尚未執行的Down」分開：新完整快照中目標不可見時，後者立刻取消，返回後只可用新鮮像素提交新intent。正式LatestFrame的burst測試保存了舊行為先敗、修正後只讀最新frame並零舊Down的FakeTouch證據；此測試尚不代表完整多執行緒效能鏈。

observer45在同一非零line track ID的Note歷史內，將新切向與上一筆切向點積為負的線局部法向翻到等價方向，然後才計算有號距離／root。原始當前線候選不改寫；真正反轉或跳變仍由測量段守門。切向正負交替的獨立oracle案例先失敗為`motion_discontinuity`，修正後與反轉負例皆通過。兩批既有753張三幀RGB的完整cold decision與observer44相同；這不是實景準確率。

observer46在同一組當前可見Hold側軌上，優先讓已建立的線上body取得續接候選，再處理新生前景片段；後者不得擦掉既有body候選。已觀測tail接近判定線時，後續新鮮像素可在有界的36px局部窗尋找第二份tail支持，仍須當前封口及兩側側軌，並維持兩份新鮮證據才正常Up。旋轉Hold與同位置Tap的36幀合成RGB及獨立oracle一／二指正反例通過；既有753張三幀RGB相對observer45無完整decision變化。這些片段沒有實景長Hold真值，擴張搜尋的整鏈成本尚待正式A/B核對。

observer47在同一Note經至少三次、跨30ms的當前像素測量確認line ID後，拒絕直接改接局部幾何不連續的另一條線；原線消失時該Note的關聯與root變unknown，已提交意圖由owner撤銷或按現有接觸證據續接。若新ID在上一測量接觸區延續原線（切向mod π點積至少.97、局部法向差不超過36px），一般Hold需兩張間隔至少12ms的新鮮影像；近線且當前支持的Drag／Held body可立即重接以保留活動contact。兩者均清空舊線速度擬合，不把舊root搬到新ID。追蹤狀態仍隨Note在100ms未觀測後清理。此守門由交叉雙Hold消線負例與反轉Drag換ID回歸約束；更多遮擋／旋轉換ID仍待驗。

離線`analyze game-cold-pipeline`用有界預產RGB、正式LatestFrame三實體buffer、正式observer／owner／scheduler與FakeTouch、Journal三執行緒跑QPC鏈；每個來源frame分別記capture complete、publish、recognition、owner accept，receipt依source frame／intent join。輸出包含全部publish嘗試、skip、pool drop、writer drop、late／失敗和每100幀RSS；source render age、真gRPC、遊戲採納未知。全掃描A/A先凍結容忍，再以三批ABBA驗正式Tap與密集場景；原計畫跨場景冷gate通過，密集場景另設的逐批加速規則失敗，詳見[結果](COLD_DEVELOPMENT_RESULT_20260929.md)。

量測入口另有固定Tap循環及16線／128Note的靜態密集RGB場景；`--writer-capacity`／`--writer-delay-us`只對離線Journal寫執行緒施加有界負載，`--rpc-delay-ms`只延後FakeTouch receipt return。負載時仍從實際QPC記injection start／return，並把所有1000／10000次publish嘗試保留在分母；writer的debug drop是單向診斷損失，不回饋owner策略。正式runtime的Journal預設容量與真輸入backend未受這些離線參數改動。

合併審查修正冷鏈診斷的發布時間所有權：perception只讀Frame lease內在LatestFrame鎖下發布的`published_ns`，不再讀producer在`publish()`返回後另寫的未同步欄位；producer的publish呼叫成本只在join後彙總。詳見[合併驗收](COLD_DEVELOPMENT_MERGE_REVIEW_20260929.md)。observer47／planner23／diagnostics11的遊戲策略不變，冷鏈輸出另記`publication_time_basis`辨識修補；歷史meter不回寫。

相向雙側Hold與Flick的合成RGB已能在同一線上保持三個獨立接觸；容量改為兩指時保留兩個先開始的Hold，Flick無Down。對應oracle消融使用獨立生成的幾何與關聯，隔離觀測及資源規則；這不代表實景Hold部位或遊戲判定已驗。

## 即時閉環

```text
gRPC capture → LatestFrame → SessionPerception / GameObserver
                              → 最新完整 DecisionSnapshot
                              → SessionGameOwner / GamePlanOwner
                              → ContactScheduler → GrpcTouch
supervisor ───────────────────→ revoke / stop / release
只讀旁路：preview、journal、結果圖、pixel clips、shadow tracker
```

capture、perception、action owner、supervisor、診斷 writer 的責任分離；模型／採樣不得直接取得觸控 backend。主程式不讀譜、遊戲內部狀態、音訊節拍或歷史按鍵。曲名／難度只屬離線報告資料。

## 固定擷取、時間與所有權

- gRPC payload fast、RGB888 top-down、1280×720、source rotation 1、read chunk 256 KiB。profile 的相對 lag guard 為 250ms；它不是絕對來源年齡。
- 邏輯 latest-frame 容量 1，固定三個物理 buffer；reader 持有時不可覆寫，耗盡 drop 並計數，不無限配置。
- host 使用 QPC nanoseconds。Frame 保留 sequence、epoch、generation、geometry、capture_complete、pixels_ready、published；消費／辨識／排程／RPC 時刻另記。
- source Unix／Android／WGC／PTS 保持獨立 domain。source age 未校準；receive time 不改稱 render time。
- frame context、尺寸／方向改變或來源失效撤銷資格。WGC／DXGI／scrcpy 仍僅 bench，MMAP diagnostic-only，無正式自動備援。

## 觀測、身分與預測

GameObserver 使用當前像素的 HUD、線、色芯與 Hold 外框候選；GameLineTracker 維護獨立線 ID／有界 pose。GameTarget 與 Note identity、intent identity、contact ID 分離。
observer42 的跨畫面像素掃描與有界連通區局部線段分割可保留最多16條具獨立支持的線，避免X／V／框／放射的連通白區被單一PCA方向吞掉；相近方向／法向位置先去重，局部線段另查沿線相鄰彩色或灰色Hold body像素，避免側軌／內部裝飾假線。既有兩批RGB中，observer41與凍結38版相比有9幀額外線候選；observer42相對41另有37幀線候選數及2幀Note候選數變化。這只支持候選差異，沒有人工逐像素真值或實戰動作證明。

線至多 16、note／track 至多 128，近期 pose 至多 6 點／90ms；跨 epoch／geometry、非遞增 QPC 或過大 gap 清理。線用至多16×16的全域可行配對；競爭未分派不即刻出生，近鄰可分離候選需連續三幀才出生，暫存至多16筆。輸出線幾何仍僅來自當前量測；track運動只供關聯預測，歧義線不供新Down。

note→line 拒絕 association_invalid 的線，以局部距離、沿線範圍、近期相對接近趨勢、線ID及弱外觀方向分數選有限候選；次佳分數過近則保持關係 unknown，不建立撞線擬合。歷史趨勢只是軟分數，遠處外觀方向不合不硬拒絕；候選存在不等於Down資格。Hold已有關係時優先維持同線，當前body仍須重新由pixels支持。

撞線使用 note 與線的局部法向距離、近期表觀相對運動。反轉或跳變斷開舊擬合，至少三點／30ms的新量測段才再輸出root；近線首見Tap／Hold／Flick明示未取得時間資格。空間殘差及時間不確定性分開；30ms uncertainty 上限與 35ms lead 屬基準設定，後者不是已量得的 gRPC 固定延遲。

後續設計依[判定線形式](判定線形式.md)及[研究 §2.7](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md#27-判定線形式與策略優先的運動處理)：目前先按線局部座標的有號距離，對反轉／跳變做有界分段，使用最新可支持的單root；斜向與線追Note可由相對距離涵蓋。多root且幀間不可見的過程仍是unknown，未建立校準的機率分布。幾何過線不是完成音符的充分依據，返回也不授權重播已執行／未知 Down。

## 動作與失效契約

- 唯一 action owner 修改 scheduler／發送觸控。gate 與每個 plan 的證據獨立檢查，now >= deadline 到期。
- source／target 證據上限 100ms；missing 容忍 Tap／Drag 40ms、Hold 60ms、Flick 75ms。UNKNOWN UI／context／input fault 撤銷全部，不延長期限掩蓋漏辨。
- Hold 以當前 body／rails 支持續接同指；可見 tail 結束才正常 release。tail 預測不能單独維持接觸；停止／失效仍釋放。
- 旋轉 Hold 的續接搜索不再用舊anchor切向硬拒絕當前線；同線且當前body／rails驗證時，可維持contact並同指Move。可見tail必須配對當前線ID及時間，連續兩份新鮮證據才正常結束。接入、持續body及tail是獨立檢查；旋轉／移動時觸點由當前body支持，不由舊Down位置剛體旋轉。合成回歸已通過，遊戲接觸區語義仍未實戰驗收。
- observer42／planner22 的長序列回歸區分當前 body 仍支持的前緣移動、可見 tail 過線及無 tail 的證據失效。成對側軌與前緣可驗證當前body，兩側末端都已越過所屬線時不再以它們續命；當前橫向tail邊緣可在兩份新鮮幀確認後由owner正常Up。旋轉兼平移的36幀RGB序列亦已驗同contact的Down／Move與可見tail Up。無tail時不可再Move，維持有限missing grace後安全釋放。正反例只代表合成像素與FakeTouch契約。
- Drag 的當前區域覆蓋可在已驗語義下共用活動接觸，歧義或不同目標不可強行合併。
- planner19 在新量測判定 `motion_discontinuity` 時撤銷尚未執行的Down；只有能確知 scheduler cursor=0 時，後續完整有效 pixels 才能建立新 intent。未知／已開始／已完成的 Down 不重播。
- RPC success 只代表呼叫返回；未唯一配對的遊戲效果保持 unknown。未知 Down 不重試，釋放責任保留。
- 五指 capability 需匹配裝置／APK／幾何／mapping 指紋；Fixture 多指成功不等於 Hold／Flick 的完整遊戲驗收。

## 多輪生命週期

manual-session：STANDBY → STARTING → PLAYING → RESULT → STANDBY。使用者選曲及按 Play；當前 HUD／observer gate 才可觸控。六個英文結算文字須三個不同新鮮 frame、跨度至少 60ms；第一份結算證據即關閉新 Down。空白、無候選、HUD 消失或暫停不算結算。

真正新 round 重設 observer／owner；曲中 HUD／source 撤銷不清除已完成 identity。geometry／generation 改變、未知注入／釋放結果為 FAULT。finalize 保存首份 release report。watchdog 只管停止，不能當歌曲時鐘或結算。

run --manual-play 是另一個有限等待入口：預設 wait-play-s 60，首次像素確認 playing 才啟動 duration 預算；重複 playing 不重設。它不等同無限待命的 manual-session。

## 診斷容量與資料用途

| 元件 | 現行上限／用途 |
|---|---|
| SessionArchive | mailbox 8192 事件且 16MiB；每輪 16MiB×32 段，超額 FAULT；最多一張待編碼結算圖 |
| 待命 journal | 1MiB×4 輪替；不累積全歷史 round vector |
| 每輪統計 | 四個 vector 各 100,000 樣本；完整 raw 與有界統計範圍分開 |
| Pixel clips | opt-in；20輪×每輪最多30張全RGB；writer mailbox 4；採樣失敗只減少診斷、不回饋 owner |
| 舊 ROI／candidate bank | 有界離線／shadow 工具；proposed masks 未經人工核對不算 gold |
| 預览 | 降頻只讀；不等待 writer、不作決策來源 |

新的兩秒診斷 ring、學習式觀測、relation head 及推論 runtime 尚未接入。新研究中的 512MiB 診斷 arena、模型延遲／VRAM預算與 data gates 是待驗設計，不取代以上現行上限。

## 量測與維護

報告 n／p50／p95／p99／max、complete／abort／fault、逐曲與最差曲。owner lateness、RPC 時間、預測偏差及像素首次可辨效果是不同量測；分位數由完整 raw 或明示有界樣本重算，不平均每曲 p99。

兩版實戰來源與 hash 見 [舊版結果](HD9_LEGACY_HD_RESULTS_20260928.md)、[新版結果](MAIN_LEGACY_HD_COMPARISON_20260928.md) 和 [合併紀錄](MAIN_MERGE_20260928.md)。清理後可用資料與被刪的歷史範圍見 [清理紀錄](CLEANUP_AUDIT_20260928.md)。正式實作修改需同步更新契約、相關合成回歸與能力限制。
