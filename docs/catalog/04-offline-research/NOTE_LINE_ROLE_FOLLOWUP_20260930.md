# Note 朝向／運動與真線角色：本批後冷修正

2026-09-30，第十輪 C36f 結算後使用者提出，並明確補充「兩種都要考慮」：
Note 移動方向垂直於線，以及 Note 長條圖形朝向垂直於線，都應保留為
可觸控候選。另需區分原判定線仍明確可見時，向外移動且略微變色、
並非閃爍的近似假線；避免非法線／裝飾／Hold 邊緣干擾 Note→line。
這是使用者確認的功能要求與整輪觀察，尚無逐 frame 人工 gold。

第十輪 C36f 783793 分／81 Miss，有線而全線 association invalid 僅 2
frames。ID 出生循環的修正不等同真／假線角色辨識已完成。剩餘第十一／
十二轮沿凍結 C36f→A36 同譜面比較，不把下述新規則混入本批版本。

## 邊界與當前缺口

- Note 長軸、Note 在主機時間上的位移、線切向／法向各自保存和檢驗。
  接近時才對齊的 Note 不能因遠處朝向不符而失去有界追蹤。
- 垂直關係保留為候選；真正 Down 仍需當前 Note 接觸區域、線支持、
  一致的有界相對運動／當前交疊與 owner 資格。不能把垂直本身當 Down。
- `track_legacy_batch` 多線選擇目前有 abs(dot(note.tangent,line.tangent))
  >=.95 的硬條件，單線另有 fallback；current Tap overlap 另有 >=.9。
  新設計須分別驗證追蹤資格、線關聯與真正接觸，不能只刪掉角度門檻。
- 原線仍有當前 pixels 與 Note 承載支持時，新近似線不得僅靠更近、
  更長、較高白色信心或略微變色搶走關聯。原線的歷史 ID 也不是真值。
- 顏色持續微變而不閃爍列為可測弱線索；線自身位置／角度的變化不能
  直接等同假線，須保留真線平移、旋轉、線追 Note、突現與多線。
- Hold 接入、持續 body／同 contact Move、tail 結束分開；角色識別
  不得讓插補成為當前支持，或讓完成／未知 Down 因新 ID 復活。

## 最小 C++20／fake-clock 正反例

| 輸入 | 預期要驗證的契約 |
| --- | --- |
| Note 沿法向朝線接近，長軸與線平行 | 保留既有 Tap／Drag／Flick／Hold 行為 |
| Note 長軸垂直，接觸區域真的抵達線 | 候選不因朝向硬拒；接觸與時序獨立成立 |
| Note 長軸垂直，但仍很遠／正在離開線 | 有界觀測追蹤；沒有即時 Down 資格 |
| Note 移動方向與長軸不同、接近線才轉向 | 用相對位置／運動維持候選，不把長軸當速度 |
| 原線仍清楚，近似裝飾線移出且持續微變色 | 無承載支持的近似線不搶走已支持的 Note 關聯 |
| 同色／不變色装飾、Hold rails、寬填色、背景線 | 顏色不是唯一排除條件；避免產生錯線 Down |
| 真雙線各有獨立 Note、真線旋轉／平移／反轉／突現 | 不以原線優先或色差將有效多線濾掉 |
| 幾何角色歧義、掉幀、支持消失 | 有界拒絕／撤銷／釋放，恢復不重播已完成或未知 Down |

先重用新版合成 pixels／組合庫的輸入與反例，與36版策略邊界解耦。
以當前像素抽取、關聯、接觸和注入分層比較，記首個分歧；先證明
故障和修正的正負例，再凍結新版本。容量帳本與現有12輪上限不擴張。
診斷不回饋遊戲策略，不按歌曲身分調參，不讀譜或訓練模型。

## 當前 pixels 參照與缺口

第十輪 clip6 frames10220–10222 的原 RGB／SHA 已核對，另由 C++ audit
輸出 raw／overlay。代表畫面有水平白線、中央 Hold 的白邊／填色、
灰色垂直元素與黄色 Drag；不是已標註的「移出變色假線」時間片段。
整批 clips 為三幀短窗，缺完整前後角色／持續顏色演變；此例保持
proposed／unknown，不能宣布已辨識該假線或把觀察直接作人工 gold。

## 批後 C36g 冷候選，2026-09-30

使用者停止本批12輪並授權優化後，隔離工作樹加入
`C36g-current-note-line-support`。凍結C36f與12輪原始資料保持可比較。
C36g尚無實戰，不等於主checkout的observer49／planner26或既有38版冷主線。
沿用36／18 lineage欄位，必須以variant＋executable／source SHA區分。

正式改動：

- 彩色薄核心以自己的PCA長／短軸與有界局部尺寸辨識，不要求先平行
  於某條線。局部厚度至少4px，保留原Note尺寸上限；2px暖色Hold邊框
  不得因旋轉方向成為新Drag。Tap／Drag／Flick幾何與Hold rails分開。
- 多線候選先檢查當前線與有限端點覆蓋，區分Note長軸與近期相對位移。
  最近兩次Note觀測相隔10–40ms、revision至少2、相對移動至少3px、速度
  不超4000px/s、法向份量至少85%且朝線接近時，可優先於僅圖形平行的
  候選。線的有界motion fit只輔助關聯，不冒充当幀線或撞線時間。
- 已有至少3個實測關聯點、跨度至少30ms，且同ID的線仍可見、Note繼續
  接近／當前Hold body支持时，優先維持原關聯；不因較長／較亮信心的新
  分支或Note轉向改配。ID本身不是真值。其他同等候選分數差<8px時保持
  無關聯；新的撞線預測仍須原3點／30ms擬合與不確定度門控。
- current Tap／Drag overlap移除圖形平行必要條件，改驗證真正hit在當前
  Note局部核心內，仍要求線新鮮、中心距線≤8px及原樣本／唯一性資格。
  長條的一端碰線、核心未接近，不可作即時Down。
- Hold當前paired rails／filled body搜尋採当前線座標，舊anchor朝向不再
  排除旋轉後的當前證據。多線延續需先有最近同線關聯；不盲掃所有線。
  同線支持可容許新舊Hold朝向差≤60度，但原48px位移／尺寸限制維持。
  接入後的同一contact只由當前body修訂Move；head-on-line anchor也受
  色塊／投影拒絕保護。60ms missing grace、100ms evidence lease保持。
- Hold尾端確認排除association-invalid線；有line_id的正式target只使用
  同ID且observed_ns==當前evidence的線。兩次尾端觀測確認與原20ms釋放
  規則保持；其他線不能代替已配對線確認結束。

上述數值是通用冷開發假設，沒有按歌名設定，不修改lead35ms／uncertainty
30ms／五指mapping／門控／未知Down不重試。灰線的微小持續色變尚未另建
可靠角色分類器；目前修正以當前承載與相對運動抵抗搶配，不能宣稱所述
變色假線已完成實戰辨識。

驗證：新增11組C++20合成pixels／fake-clock案例，初始8組在C36f全失敗、
改後通過；另新增perpendicular Drag、靜止Note被線追及錯線／stale尾端
反例。完整Release 227 passed、2 opt-in skipped、0 failed（229列）；另
recorded RGB row-prescreen opt-in27張通過，result/menu opt-in仍未跑。
途中完整回歸發現細長暖色rail誤入，加入局部4px限制後既有反例恢復；
舊Drag負例改為非法非unit tangent，垂直unit tangent另有正反例。

12輪實際保存330張RGB（前2輪各30、其餘各27），C++ audit逐clip cold reset
並核對index／RGB SHA。C36f→C36g線候選／幾何全同；140 frames decision
不同，去除純數字ID後46 frames target不同。target-frames796→813；
FakeTouch Down10→11，stop後contact總0。新增一次發生於第六輪clip4
frame5120：當前上斜線上的黃色核心已有2個當前支持點，原版配對至較遠的
下面水平線；已保存raw／overlay與首個分歧。此為proposed角色與模擬觸控，
不是獨立Note命中率或人工gold。三幀clips缺warm state，不能取代全曲驗收。

所有新來源、binary／DLL／profile SHA、測試與冷重播分母見同一資料根
`cold-optimization/candidate36g-freeze.json`、`cold-optimization/review.md`。
本批remaining0，沒有啟動emulator、manual-session、真觸控或模型訓練。

後續使用者另授權C36g首輪manual驗收，Dlyrotz IN13為796284分／71Miss；PAS
正常停止。仍未大幅改善，轉向[疑似Miss影格標註](../07-data-learning/MISS_FRAME_ANNOTATION_20260930.md)。
首個有原圖的Hold撤銷發生於水平有效line138，原ID1052／新rails ID1063切分，
不能只當成旋轉或假線問題。現有27RGB多數不覆蓋事件，逐Note結果維持unknown。
