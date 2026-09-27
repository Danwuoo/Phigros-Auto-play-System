# 外框追蹤與持續接觸方案（2026-09-27）

最新狀態：使用者已重新授權自主實作、迭代、Computer Use 遊戲操作與實戰；HD 同版本至少連續三次 AP 後才進 IN。observer30／planner10 已加入當前灰階雙側外框、短尾端、接觸重新關聯、持續 Drag Move 與獨立 line ID；其餘配置回歸與遊戲驗證正在進行，尚未 AP。以下停止點是歷史紀錄，光流段仍為待評估設計，不能視為已接入。

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

## 本輪停止點與確定缺口

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
