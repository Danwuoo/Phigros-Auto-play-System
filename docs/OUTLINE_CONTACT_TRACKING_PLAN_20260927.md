# 外框追蹤與持續接觸方案（2026-09-27）

狀態：使用者停止兩輪測試後的具體設計，尚未修改runtime或啟用光流。使用者指出Drag漏接、Hold未結束即放開、線／Note移動不能跟隨，並建議Hold以外框續接、Drag模擬手指持續放在範圍內。本方案沿用五指、C++20單程序、固定擷取與pixels-only；不再啟動遊戲測試。

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
