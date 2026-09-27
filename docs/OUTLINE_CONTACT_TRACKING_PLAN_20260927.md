# 外框追蹤與持續接觸方案（2026-09-27）

最新狀態：自主開發與Computer Use實戰持續，HD同版本至少連續三次AP後進IN。observer33第九輪369／2／0／22、868,880分，仍未AP。observer34／planner14修正前端被觸控特效遮擋時的body續接與色塊錯誤投影Move；diagnostics6有界採樣。已有灰階雙側外框、短尾端、接觸重新關聯、Drag同指Move及獨立line ID；光流／模型未接入。

## 已實作資料契約

### 首輪實戰與後續修正

planner11／source `f0aa05b` 第四輪 `cpp-observe-17905056643080469`：352／3／0／38、825,585分、Late3、max combo59，未AP，STOPPED／exit0。35次重關聯、contact conflict0、Hold Down112／missing取消93／geometry13／ambiguous6／tail確認0，source expiry3；raw SHA256 `baf9fdb89a347ffb65d12c46b9a1e85386aaf1a042e3386f5c5a633abfecf91e`。不能把Down减少當成命中改善。

frame410將候選46接回原Hold35／intent6；411原35仍有當前152×314外框，但46變為遠離line的歧義core。舊別名無條件把46重命名為35，先取消同一contact；后續原35不能復活。planner12改為每張驗證外框、samples、當前evidence、line及幾何，優先原身分的有效支持；有效alias可替代失效原描述，無效alias不刷新／取消原手指，也不能成新Down。別名last_seen只作退休抑制，最久無新candidate100ms即清理；目的owner消失／完成但candidate仍出現時只忽略，不能復活。附兩種snapshot排序、反向有效替代、無支持60ms釋放及500ms後仍不復活回歸；此版仍待完整配置／實戰核對。

第二／三輪source `2807f2e`／binary `9b3036c0...`：診斷輪 `cpp-observe-17905045465138818` 啟動晚於重試，324／0／0／69、757,252分，排除穩定性與直接分數比較；完整重跑 `cpp-observe-17905047888987862` 的 Computer Use 結算358／4／0／31、840,560分、Late4／max combo58，未AP。兩輪均STOPPED／exit0、185秒、相同五指與取樣配置、contact conflict0；完整輪139 Hold Down、Hold missing取消124／geometry7／ambiguous3、tail確認1、重關聯0、local Drag coverage10。完整輪raw SHA256 `55b3510e5302c1222f886598ead880b2ed7d571acef0ee0c2c15bd73b9c27db9`，辨識n10663 p50／p95／p99／max=4.9125／6.96306／8.25246／19.2555ms，source expiry1。少12 Miss只是兩個完整run的結果，不是多輪稳定改善結論。

完整輪frame344–355暴露owner準入缺口：原Hold32／intent7在line上仍有310px body，候選42早先已出現但未注入；352起以同位置152px rails取代32，owner因`identities_.contains(42)`跳過重關聯，354便missing取消原contact，363仍可見160px gray body。下一版planner11允許未submitted的占位候選接回唯一active Hold，移除無手指占位，不復活submitted／completed或過期contact。附現有占位／RootPast／持續同指及停止回歸。diagnostics4只收head_on_line且距線≤8px的長Hold，避免frame119距線30px的短暫approach gap先花掉有限held-loss窗口；這是只讀取樣篩選，不改觸控期限。

source `fe0c85e`／binary `e2cab0ac...`，五指／lead35、185秒、單擷取、無preview／shadow／dataset、2個診斷clip共8 native ROI：run `cpp-observe-17905036167552410`，STOPPED／exit0。Computer Use 結算核對350 Perfect／0 Good／0 Bad／43 Miss、817,048分、max combo61，未AP。停止後另存結算PNG；raw SHA256 `146c9d650b8bbb553c69fd04654d13c679bd2bab8fec5594db934a0a477cdfe9`。

C++分析10720 frames，辨識時長n10720 p50／p95／p99／max =4.7794／6.999555／8.526006／22.7867ms；capture間隔n10719 =16.5907／32.83405／46.153474／266.6157ms，source expiry2。真實401 Down／518 Move／401 Up，6個contact conflict；Hold down148、missing取消140、ambiguous3／geometry3，tail確認0／contact重關聯0，Drag已知local coverage9。這些是本機身份與注入統計，不是393個譜面音符真值；不能把Hold missing取消全部視為提前Up。測試環境及固定參數見metadata。

conflict事件顯示同一x約639／y576位置有五個不同Hold intent持續刷新；新外框路徑可能讓多個舊anchor借同一當前rail刷新。後續改為每組當前相同外框只能支持一個身分，附40幀改變內部core的持續回歸。另保存第一個灰化失聯frame540 ROI：舊anchor core寬136.7604px、當前外框153px，原10%寬差門檻拒絕。當前成對寬差改為最多20%，保留雙側／近線連接／多截面一致與相鄰負例；離線C++ WIC probe僅讀保存pixels及當時anchor，舊source回傳no_current_outline，新source回傳x350.062／y576／width153／height233／tail未知。crop以上仍未知，不以補黑區域創造支持；此局部A／B不是遊戲命中驗收。原始probe source／log／settlement／分析保留於raw root。

首次凍結版本的 Release／Debug／無 suppressions ASan 各149／149通過，時長11.76／31.32／59.90秒；build、所有失敗與修正回歸原始紀錄已保留。五指 preflight fingerprint_matches=true。[驗證 metadata](OUTLINE_CONTACT_EVIDENCE_20260927.json)記錄環境、設定及 binary SHA256；實戰仍待測，不能據此宣稱性能改善或 AP。

- `src/game_motion.cpp` 的 line tracker 上限16，90ms 歷史、100ms／場景切換清空，方向反轉正規化；近似競爭不強配。實際當前幾何不低通，只有法向平移可從無紋理線段識別。CandidateBatch extractor30 新增 line_id、observed_ns、velocity、angular_velocity、association_valid；讀取仍相容29。
- 既有90ms Hold anchor只作搜尋初值；當前成對亮度 ridge、寬度、至少兩截面及近線連接才續接。不要求填色，不以歷史或光流刷新證據。尾端需當前封口，裁切／未見保持未知；短尾需兩側在封口下方延續且上方不延續，排除內部特效封口。兩次當前尾端過線、跨度至少10ms才鎖定20ms正常 release。
- 既存 Hold 唯一外框匹配可將新候選接回原 contact，不重複 Down、不復活完成手指。Drag 同 line_id 的當前區域可持續接觸與移動；沿線死區最多48px、法向2px，真正離開區域時同 contact Move。新 Down 仍要求預測準入，五指容量與60／40／100ms期限保持。
- 新增 `game_contact_cancelled`、`game_contact_up`、`game_hold_contact_reassociated` 診斷，分析另計尾端確認與釋放原因；均只表示本機觀测／執行，不代表遊戲判定。
- 原始失敗、建置與回歸保留 `measurements/outline-contact-20260927/`。合成回歸新增灰色雙框／單側負例、短尾／內部封口負例、平移旋轉、多線重排、同指 Drag 跟隨、Hold 換候選不換手指與完成不復活；未將舊圖 proposed mask 當真值。

## 歷史停止點與後續實戰

### 自主迭代補充：第五輪與接線過渡（2026-09-27）

第六／七輪同一 source `5f1c710`／binary `58bafae6...`、五指／lead35／185秒／相同dataset採樣，均STOPPED／exit0。Computer Use 核對第六輪365 Perfect／2 Good／0 Bad／26 Miss、852,341分、max combo53／Late2；第七輪359／7／0／27、862,583分、max combo118／Late7。兩輪都非AP，不能據此升IN或宣稱穩定。兩輪contact conflict0；Hold Down105／112、重關聯10／9、tail確認0／1，geometry取消5／7、ambiguous取消0／1。第六辨識n10640 p50／p95／p99／max=4.6658／6.57259／7.87322／13.6668ms；playing interval n9450=16.67975／33.32927／46.549711／90.4836ms。第七辨識n10278=5.08525／7.50676／9.445496／16.8118ms；playing interval n9103=16.8445／35.77646／48.321038／90.1227ms。原始hash及結算PNG／json見metadata及raw root；只報分布，不將不同行程分數差當單一修正收益。

第七輪frame740–760，Hold98／intent15的當前藍色本體前端由line576逐步往上離開，current外框仍存在；舊路徑卻只在line附近續接，749起只剩新候選103，751以missing取消原contact，候選不能復活它。保存的四張ROI顯示可見前端正在移動，這不是單純顏色消失。observer32新增`observe_moving_held_front`：僅90ms近期已on-line／已有當前held_body_evidence的成對anchor，局部沿／法向各±48px、固定上限搜尋；當前亮度前緣至少5／7內部點、外側暗區、三個一致成對rail截面、两側至少96px連續rail、前方不繼續同rail，才續接。近線≤12px仍走原on-line路徑；tail保持未知，不用截斷長度猜尾端。內部暗條、缺rail、無當前fill、未曾接線與空frame均有拒絕回歸。

NoteCandidate新增`held_body_evidence`，與`head_on_line`分開；GameTrackPoint記錄該current前端是否作實際touch point。Current前端續接可更新90ms rail anchor，Hit／Move採当下量測前端內側，不再投影到原line；owner接觸別名仍需唯一、當前、未完成與≤48px步進。新字段只由專用當前觀測產生，不作新灰色Hold出生或延長60／100ms期限。CandidateBatch extractor32、quality1／schema1不变，讀取29–31缺字段預設false，字段true需Hold／outline／rails同時成立。影子方法仍無真實backend。diagnostics5允許當前moving held body的消失採樣；純approaching未接線仍不消耗held窗口。續接plan basis明示`live_pixels_held_body_continuation`，原首次Down預測仍保留。

新增C++ pixels／fake-clock整合測試：灰色body從line移到上方116px及橫移66px，保持同contact／單Down、每幀Hit是當前body前端，missing仍按60ms釋放；序列化正／負例及新版diagnostic回歸同步加入。最初focused3項有1失敗，原因是136px出生core與152px外rail不一致；改為當前三截面各自量測並核對20%寬度與4px一致性，未放寬預期。修正後focused3／3、完整Release159／159先通過；新增diagnostic後Release／Debug／無suppressions ASan各160／160通過，11.39／28.58／65.28秒。保存frame749 ROI的current moving-front probe定位x638.493／y513／width152，crop內兩側支持176px，tail未知。binary aee65d0be78b82cf2febc2c33535da1329e7e02428aa8ac5348f0efa8380a897。

### 第八輪退步與當前特效回退修正

observer32／source3747854，cpp-observe-17905096397278472：356／2／0／35，829,949分，maxcombo46、ACC90.92%、Early1／Late1，185秒STOPPED／exit0。原始SHA256 25d32a142c763ee073a34ebad8d4b9877e1329dbbfc070a2f9d5f7338694563b。recognition n10824，p50／p95／p99／max=4.8017／7.50175／8.842214／19.2853ms；playing interval n9630=16.56395／32.17922／44.347327／81.2392ms。同環境與五指35ms；曲中無build／test／額外擷取。接觸衝突0、Hold Down107／alias8／tail0，missing96／geometry7／ambiguous3；source expiry1、UI lost1。更差仍需逐段取證，不能把9個額外Miss全歸給單一事件。

829／830的Hold119當前前端548→535；831移動前端失敗後回退舊outline，位置跳回575.9，832／833仍在線上，834改為其他候選，836 missing取消已執行6步的intent15。此輪因全域10run採樣額度已滿，sampling_manifest=null、ROI0；日誌只證明幾何跳回，特效像素原因由先前同類ROI及合成負例核對，未假稱有第八輪ROI。

observer33：若已由current held body離線，重新接回line需line後6／12px兩排各5／7內部亮度支持；只有兩側框線不能接回。moving front的前方rail連續亦須24px外側有5／7body fill才判為內部暗條，保留真正內部暗條負例。粗略±4px亮暗對比先找前端，再以當前±1px終止邊的中位位置量測觸點，放在內側3px；避免cost將位置逐幀拉深。已held_body不走純approaching快捷路徑。新增特效框線正／負回歸，交替延伸外框的整合序列仍要求一個Down／同指Move／60ms缺失釋放。首次focused16項1失敗（觸點深入6px），修正實際终止邊後16／16，不放寬預期。extractor33讀取29–32，字段契約不變。採樣上限有界調為20run／2560 images，另測較小額度的拒寫、partial及舊檔保留；2GiB／256MiB／128MiB與停止後編碼不變。

第五輪 source `5a8fe34`／observer30／planner12／diagnostics4，run `cpp-observe-17905069580837787`，185秒 STOPPED／exit0。Computer Use 結算353 Perfect／4 Good／0 Bad／36 Miss、825,038分、max combo42、accuracy90.48%、Late4；未AP。contact conflict0、Hold Down112、重關聯33、tail確認0；Hold missing取消98／geometry8／ambiguous5。raw SHA256 `7634176a0cebe773e0a3b1edfd14ca1ad9c973fb2908ab3e934c6cd61ec372cc`。本輪另開有界dataset：18 clips／136 native ROI、首2048候選bank、无shadow／模型、停止後WIC編碼，不能把分數差全歸planner12。copy n10614 p50／p95／p99／max=.22465／.3175／.409387／1.5115ms；辨識4.61515／6.65461／7.960794／19.6307ms。playing capture interval n9440=16.70585／32.563705／44.882484／102.3034ms，source expiry2、UI lost1；global capture max322.3705ms含非playing區域。停止後結算PNG及讀值另存raw root。

frame725–728 的另一個提前Up可重現：Hold99／intent13已Down，舊前端受特效影響從542回退到524／527／549px，仍被當approaching；728多描述競爭令samples0，取消contact。ROI中的白外框仍接到線，後續gray body存在。observer31允許90ms內最近成對rails、前端距當前線≤48px但尚未head_on_line的anchor搜尋當前外框；首次接線兩側必須在距線2–4px都有實際ridge，純投影不供證據。已有完整當前前端則保留其幾何，避免拉到線上破壞速度及讓內框寬度變成外框anchor。既有held路徑與所有期限不變。

最初完整Release156項有7失敗，收緊首次接線後剩1（完整前端被覆寫、偏寬anchor容許錯pair）；保留完整當前前端後156／156通過。保存frame727 ROI以C++ WIC probe核對：當時anchor x353.089566／y548.9625／width139.04745／非online，当前量測x350.09／y576／width152／height240；tail未知，crop以上不作真值。新增pixels＋fake-clock整合回歸確認同contact只有一Down、gray持續8幀、真正missing按60ms釋放；分離前端與遠於48px anchor負例仍拒絕。最終Release／Debug／無suppressions ASan各156／156通過，11.35／29.78／67.73秒；binary 58bafae6f5917a4239cdf4f4f6249ba3b8b4f8665fafca2ae89952a238bac505，實戰待續。

第一輪已完成185秒、STOPPED／exit0／playing_seen=true，run `cpp-observe-17905001386033080`；9293消費frames／999 gameplay commands。使用者隨後停止測試，第二輪未開始，当前沒有pas程序。結算擷取命令在建立capture前因無效options退出，沒有結算PNG／人工判定；不能填分數、Miss或宣稱改善。原始資料與C++分析保留於 `measurements/game-semantics-20260927/live-two/` 及原run。

同一已驗證source59c92bf／planner9／observer29、五指、lead35ms，未啟用dataset／diagnostics／shadow／preview。C++分析沒有contact conflict、9次Drag coverage、6次source expiry與1次UI gate revoke、4次target evidence/window rejection。這些是本機執行證據，不是逐Note遊戲判定。raw SHA256 `47602f32d9f25c2359b582dacfbeaf6c437b81c2dce81172b2a8cd16718838e5`。沒有`game_hold_tail_confirmed`記錄；不能据此斷定每次Hold的真實尾端位置或個別Up原因。

| 程式確定缺口 | 對應位置／影響 |
| --- | --- |
| 既有Hold rails路径仍要求blue／gray填色、近期anchor、固定附近搜尋；不是獨立的外框物件追蹤 | `src/game.cpp` 的body重建、`hold_fill_near_head`與`visible_hold_rails`呼叫；灰化／特效／橫移可能使可見外框無法產生有效觀測 |
| owner仍直接以observer Note ID管理接觸 | 新舊描述競爭／ambiguous／missing會取消；正常尾端完成修正不涵蓋這些提前取消 |
| active Drag共用只接受原contact已在新區域內 | `drag_can_cover`限制法向2px，`refresh_drag`只更新evidence／Up，沒有Move；線或黃鍵移走便無法續接 |
| LineCandidate沒有穩定line ID，部分Hold路徑只接受長度≥80%畫面的高信心線 | 每幀選線及線相對幾何不足以代表真正平移、旋轉、多線關聯 |

## 1. Hold：顏色確認出生，外框維持身分

未按下前以可見類型／頭部確認Hold，避免把裝飾矩形當新Hold。按下後以同一組左右邊緣、寬度、方向、外側輪廓連續性及尾端幾何續接，顏色變化只降低相應品質，不直接令接觸消失。左右rail採局部亮度梯度與邊緣方向，不強制固定RGB／純白閾值。

外框匹配使用兩側成對支持、合理寬度、當前body重疊、線相對位置與實際QPC dt；一對一關聯並保留競爭差距，防止把相鄰Hold合併或借另一條rail。頭、body、左右rail、tail的可見性與evidence時間分開。當前兩側／body可以維持既存接觸；不能把看不見的tail填成觀測。特效內部亮邊不能當尾端。

外框身分與手指contact／intent分開。只有唯一、當前外框匹配證明是原物件，才讓更換的candidate描述接回原contact；不重新Down，也不因candidate編號改變自行Up。無當前支持或兩個物件同樣可能時仍走明確失效路徑。

正常完成需有可解釋的當前tail與所屬line幾何；以短小的連續當前觀測確認尾端通過，避免單張錯尾即終止。缺tail表示未知，不能與完成混為一談。100ms source／target、90ms anchor、60ms Hold missing維持；新外框觀測通過專用驗證才更新action evidence，不以純預測掩蓋缺圖。

## 2. Drag：管理一根持續接觸的手指

將Drag的物件描述與持續gesture分開：首次可靠近線Drag安排一次Down；之後每張當前畫面檢查有效黃鍵區域與實際接觸位置。接觸已在區域內便保持；區域移動時用同一contact ID送Move，將手指帶入當前可見區域。先以區域內部中心作目標，保守內縮邊界，精確遊戲hitbox仍需實測。

已按下的Drag續接不再要求每個黃鍵各自具有可供新Down的完整線性撞線擬合。分類／區域／所屬line／當前像素時效仍須有效；新Down保留既有預測準入。連續可見黃鍵之間以有界一對一gesture關聯接續，不因每個黃鍵新ID而Up／Down。手指與區域已分離時，先確定唯一可跟隨區域，更新Move及證據，再判斷舊成員是否消失；不能要求Move前已在新區域，形成循環門控。

多個同時黃鍵不能一律併為一指：只有位置／時間相容、單一路徑可覆蓋才接續；分叉／遠距同時目標使用其他可用指。Hold的contact不被Drag任意移走。沒有當前有效支持時沿用有界失效／40ms missing與stop release；不從曲名、歌曲時間或整段未見序列預排觸控。

## 3. 移動追蹤：先穩定線身分，再在它的座標系追Note

每條線建立line ID、方向一致的單位切向u／法向n、當前支持範圍及位置／角度速度。角度按模pi處理；以當前長線邊緣、多位置一致性及運動連續性作有界關聯，不每幀任意選最亮／最高分線。Note以所屬line的局部座標 `s=dot(p-c,u)`、`d=dot(p-c,n)`追蹤，分開辨識線自身運動與Note相對滑動。

判定線改用方向無關的局部邊緣擬合，不保留「只能接近水平且跨80%畫面」作所有線的先決條件。支持範圍、連續性及Note關聯仍要排除Hold rail／特效／裝飾線；多線競爭不強配。

在局部ROI以稀疏Lucas–Kanade光流估计輪廓點位移，作下一張當前邊缘搜尋的初值。只從線端點、交點、Hold角點／rail有紋理區取固定上限特徵；直線純沿切向位移不可由無紋理線段可靠推得。前後向一致性、當前邊緣方向／幾何重合與多點共識過濾特效污染。線与Note各自估計局部運動，不能把整張圖的一個變換套到所有物件。光流失敗時回到當前ROI重偵測，純光流／運動預測不刷新action期限。[OpenCV官方C++光流API](https://docs.opencv.org/4.13.0/dc/d6b/group__video__track.html)

Hold的接觸目標為當前body可見區域與所屬line的交會內部；Drag為當前有效黃鍵區域。依量測運動作短期注入延遲補償，送Move時保留原contact ID；記錄目標、速度、預測時域、像素支持與誤差。小抖動採區域內死區，真正平移／旋轉及離開區域則及時Move，避免固定低通濾波拖慢跟隨。

資源：仍一個capture／最新frame；光流最多前一張＋當前有界灰階buffer、不保留capture lease；16 lines、128物件、5 contacts、每物件固定特徵上限與ROI像素總預算。epoch／geometry／過期即清空。新增OpenCV依賴／模型不在本次設計討論中直接安裝或啟用。

## 實作順序與驗證

1. 先補每次Up的原因（tail完成／外框missing／身分競爭／source過期／UI／stop），將當前像素支持及最後觀測QPC留在有界診斷；不能把所有提前Up當尾端預測錯誤。
2. 實作接觸持續狀態與Drag Move，用當前區域正／負例核對只一Down、同ID多Move、離開區域能重入；不先換T1／T2。
3. 實作外框續接與contact身分關聯，使用已保存灰色／暖特效／相鄰Hold ROI作development，另留獨立holdout；原資料未人工覆核不能當真值。
4. 加入獨立line tracking／局部座標，再評估有界光流。逐層消融，不能同時換所有方法後把收益歸某項。
5. 短小pixels＋fake-clock回歸涵蓋gray／特效、tail未完、相鄰Hold、薄Tap穿越、Drag間隔／分叉、line平移／旋轉／噪聲、多線、過期及停止。量測區域外接觸時間、ID switch／誤併與錯誤提前Up，並記環境、分母及p95／p99／max；三配置回歸通過後，遊戲實戰須等待使用者另行恢復測試與準備PLAY。

這份方案已完成程式核對，效果尚未驗證；目前不宣稱修復、Hold全曲維持或HD AP。

observer33最終三配置各163／163通過：Release11.50s／Debug28.07s／ASan65.32s，無suppressions；binary f25d4774cc29c3c8fef029ed1f92535801f946879f800070c1e1fbebbbea0ca4。新版实战待测。

### 第九輪及前端遮擋的當前 body 續接

observer33／source1636519，cpp-observe-17905107612608769：369／2／0／22，868,880分，maxcombo82、ACC94.22%、Early0／Late2，185秒STOPPED／exit0。raw SHA256 fad0a66fad967173b1dd91650208391a46d40ce5d312684e9b99c958fc45a5cf。18clips／136 native ROI已存、無partial／truncated，與第八輪缺ROI分開。recognition n10811，p50／p95／p99／max=4.5795／7.2929／8.7228／18.5836ms；playing interval n9627=16.6027／32.05132／44.62404／76.1829ms。同五指35ms環境，曲中無build／test／额外capture。接觸衝突0、alias4／tail0，Hold missing103／geometry6／ambiguous3。比先前最佳26 Miss少，但單輪不能宣稱穩定改善，更非AP。

846–853同一Hold118／intent17已從539跟到498，沒有此前反覆跳回line；854–856前端被Gold觸控特效覆蓋，current rails辨識退為一般色塊，原owner仍對有samples的同ID更新plan，Hit回到576。clip2四張ROI直接顯示body仍可見，與journal的錯誤Move吻合。

observer34新增observe_held_body_patch，僅近期已接線／已held_body的成對anchor，沿線±32px、法向−48..16px局部搜尋；觸點所在及其後8／24／40px四個截面量測同一雙側ridge，每截面寬度與位置一致；0／8／24px三排各6／7body fill，兩側96px支持，才選當前內部觸點。不要求前端可見、不猜尾端；held_body_patch=true、head_on_line=false、tail=null。一般line reattachment與實際moving front優先，front重現即回原路徑。patch影像body高度只表示觸點後方支持，不表示完整音符長度。未接線、單側rail、空fill及空frame均拒絕；沒有新歷史推算點或更長缺失期限。

extractor34解析29–33缺字段預設false；patch需held_body／outline／rails，且不得有head_on_line或tail。candidate head_visible=false／origin=current_body_patch_recent_anchor；Hit採當前body點，patch清除crossing／tail-crossing、reason=held_body_touch_only。planner14不允許held_body_evidence建立新Down；既有moving body的同ID一般色塊不更新evidence、不Move回line，60ms未回復即取消。alias仍需唯一當前幾何與既有活動手指。

新增pixels整合序列：遮擋兩幀後恢復前端，保持同ID／同finger／單Down與當前body內觸点；owner反例重現一般色塊投影，不產生錯Move，證明60ms仍有效；patch不能新Down及序列化／無新撞線預測測試。首次focused9項1失敗（只檢查觸點後方外框，點本身落入特效遮區），增加觸點所在截面後focused11／11，不放寬預期。clip2-frame1的原生ROI、prior x638.5/y498/w152重跑C++ probe量測current x638.5/y494/w152、crop內rail188px，tail未知、body_patch=true；不宣稱crop外完整長度。Debug建置與測試曾重疊造成LNK1168，該舊163項結果排除，完整重建後才採計新版。

observer34最終Release／Debug／ASan各168／168：16.15／25.86／72.57秒，無suppressions；binary 428ab1bb4f4afe42582ca38726f2c6d6f5339981d5ff3f205719f9c8a7c7e130。实战續測。

### 第十輪與 pending Hold Up 提前的修正

observer34／planner14／source7687584，cpp-observe-17905118952553270：361／1／0／31，843,982分，maxcombo62、ACC92.02%、Early0／Late1，185秒STOPPED／exit0。raw SHA256 25aacd2ff8c50831f4da738cdef046303ede30fd415bcec1f12ff434b9680d6c。18clips／136ROI，無partial／truncated。recognition n10868，p50／p95／p99／max=4.53555／7.713485／9.868346／21.9077ms；playing interval n9676=16.57005／31.426525／45.010425／78.9958ms。0接觸衝突、alias8／tail0，Hold missing90／geometry4／held-region-unsupported8／ambiguous2。同五指35ms，曲中無build／test／額外capture。退步不能宣稱body patch有效，也不能把8次unsupported取消當成8個遊戲Miss；551的例子已近可見尾端消失，後續只剩膨脹特效框，沒有逐音符真值。

另查到確定的排程bug：842的Hold124／intent13原Up=40361309004700，pending Down連續改早，將舊Up一併移動；845最新capture=40361258011400，但Up竟=40361298733654，只有40.72ms有效區間。Down 40361264463900注入，約34ms後Up；846當前frame於40361297548000擷取、recognition／owner完成時已超過Up，既有完成intent不能復活。當前body一直存在，856才記錄combo消失。不能用此單段推導全部9個額外Miss。

planner15在cursor0的Hold revision重新設定Up=最新evidence+100ms，與active body lease一致；Down预测與原valid_until仍獨立更新，tail預測不作提早Up。無更長source／target／missing期限，無重播已完成Down。C++ fake-clock回歸以兩次提前Down deadline重現舊Up=70ms，再延遲下一frame至80ms；接觸須仍在，最新capture74ms更新Up174ms，最遲174ms釋放，证明不是now+100ms。observer34與body契約不變；完整回歸／實戰續測。

planner15最终三配置各169／169：Release13.93s／Debug27.36s／ASan66.48s、無suppressions。binary 0ee882ae3ee14826c954f34969c7e1a76a51981bfd617b333def723e4d16e5eb；第十一輪HD續測。

第十一輪observer34／planner15，source ac375a6，cpp-observe-17905129001142547：361／0／0／32，838,677分，maxcombo47、ACC91.86%、Early0／Late0，185秒STOPPED／exit0。raw SHA256 5f85c4a442521a999257d0b93c7e983f93c18fd6e1ceb00b93153c9bc4d8920f。18clips／136ROI，無partial／truncated；recognition n10829 p50／p95／p99／max=4.8498／7.65434／9.434284／20.4639ms，playing interval n9641=16.5734／32.5069／45.1418／69.1985ms。Hold沒有contact_window_completed Up，已修正的提前Up在此輪未重現，但總成績未改善、不能宣稱AP或穩定。

planner16開發：離線C++關聯已成功Down／Drag coverage／Hold alias後，第十一輪有8個Drag ID曾在当前line附近且至少兩個樣本，但沒有已提交接觸；第九輪為7個。這是診斷候選，不是逐音符Miss真值。新增current_drag_overlap只供Drag：目前完整snapshot中的同ID、同capture判定線，association_valid、confidence≥.8、length≥畫面寬*.5、單位tangent與core對齊≥.95；彩色core至少兩樣本且跨度10ms、width為畫面寬5–22%、height4px至width*.35、confidence≥.5，禁止outline／rails／held body。用当前幾何而非fitted distance核對法向距離≤min(8px,height/2+2px)，Hit需距当前投影≤2px且落在線段內。通過則立即Down、Up=最新capture+100ms，basis=live_pixels_current_drag_overlap，predicted_down_ns=null；沒有偽造crossing。尚未Down的舊Drag fit可改為此当前接觸；已完成Down不復活。相鄰黃鍵沿用唯一相容手指及既有Move／40ms missing期限，coverage的crossing可null。Tap／Hold／Flick維持預測排程。四個C++回歸覆蓋移動line／非線性fit／單樣本拒絕、15種無效幾何／時序、pending轉換／不能重播、無crossing的連續黃鍵同指。尚待完整測試與HD實戰。

planner16最终Release／Debug／ASan各173／173：23.86／51.72／120.58秒，三配置並行測試、各自編譯完成後執行、無遊戲重疊、無suppressions。Drag專項11／11；binary 4fb6800d8f9212505fbea1174cb44d7daa637bd59b6694e7a77f05326183c216。第十二輪改由使用者按PLAY。

第十二輪source027f01e／planner16／cpp-observe-17905141849635372：214／59／3／117，582,990分，maxcombo20、ACC64.21%、Early2／Late57，185s STOPPED／exit0。原生結算及使用者截圖對得上；raw d0a62dfbe49f431af98d776f2e22d88a6670b57f3f4c61c3daf87028814143cc。17個初始spatial Drag intents，離線未處理近線Drag候選0，但不是遊戲逐音符真值。playing interval n4687 p50／p95／p99／max=34.9333／72.20709／111.652938／225.6488ms；recognition n5088=6.3246／13.90527／21.278326／35.3848ms。source_evidence_expired撤銷66，UI_gate_lost2；18clips／136ROI無partial，local contact conflicts0。另一遊戲前景曾被CU截入，重啟用已核對模擬器window後讀到結算。測試條件異於先前，不能只歸因Drag或稱有改善。亦發現assist原本預設auto PLAY，第3frame已自動請求，違反本輪通知的手動啟動安排；保留失敗證據，補--manual-play，下一輪先驗證菜单不注入PLAY。

手動PLAY修正不改observer34／planner16策略：run_assist新增manual_play=false參數，CLI --manual-play只接受assist；auto_play=false時assist仍核對capability並取得獨立touch endpoint，避免意外繞過能力或空endpoint。manifest／summary記automatic_play_enabled，policy=manual_PLAY_and_gated_gameplay。C++負例確認自動／手動assist均在擷取與輸入前拒絕none touch profile；CLI observe --manual-play在配置／input前退出1。Release／Debug／ASan各174／174，19.90／43.28／88.54秒，三配置並行、無遊戲重疊、無suppressions，binary ea1d7a614d189ea6fcfef421083cb1205541a964e948931b73d0bbc9ca2b1914。下一輪使用者已停止其他遊戲，菜单短驗證續測。
