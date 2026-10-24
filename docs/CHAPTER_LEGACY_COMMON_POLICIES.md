# Chapter Legacy 共通候選政策

本輪處理原譜匯入與首音模型的共通限制，原始 chart bytes／SHA256、所有音符與原生判定時間不變。候選可進入人工全測，不代表已取得整首時序、視覺或遊戲判定資格。來源資料與逐曲產物留在 repo 外的 local-assets；main 分支及 EXTRACTION.json 列出的 PAS blobs 不變。

## 匯入與來源

預設仍為 `phigros-v3-strict-v1`。實驗設定可明確選擇 `phigros-v3-source-priority-constant-empty-candidate-v1`：

- 保留每條線的原生 move／rotate／opacity／speed 宣告、原 JSON pointer、原值、原始區間與整份原譜雜湊。
- 常數的反向 pose 區間沒有滿足 start≤t<end 的時間，保留為空宣告；不交換端點，不改音符，不刪來源紀錄。
- 需解決的重疊／失序 pose tracks 以原陣列第一個有效來源優先，產生另外的互斥求值片段；各片段保留來源對應與時間。正常排序 tracks 保持既有語義。
- 非常數反向事件、反向 speed、奈秒邊界碰撞與未證明的零長／重疊混合仍拒絕；來源與細分數量均有上限。

來源優先取自本機 phisap `8ac9a67` 的 judge_line.py 掃描方式；半開區間及間隙端點保持是本 adapter 的明確約定，未宣稱為官方遊戲引擎語義。所有相容匯入標記 interpretation_unqualified，政策與異常來源自動納入接點計畫身份，首音模型也另綁政策。不能把相容求值另存成「修正過的原譜」。

## 首音與執行準備

首音模型以整份原生譜面建立候選；同時首音、offset、Hold／below、signed speed 與判定線移動／旋轉不能再因舊的單一水平 Tap／Drag 假設而直接排除。所有可見的同色未來音符仍需參與別名檢查。判定時間與首次 DOWN 分開，Flick 的預按時間不改寫原生 judgment。

`typed-rle-oriented-head-v1` 在原水平 head 提取之外加入有界連通分量／主軸矩形取點，支援斜向 cyan／yellow／pink 音符。Hold 使用 head，不以相連長 body 的重心替代；複雜 body、合併、邊界截斷與超出資源上限均保守拒絕。模型的「未配對音符必然完整可見」區域另留旋轉外框與3 px誤差餘量，不把截斷的別名誤刪。像素偵測器仍無法證明完整／無遮擋，journal明列 solver 的條件假設與 detector_coverage_verified=false。

兩個不同的實際 frame、100 ms receipt 新鮮度、epoch／來源／幾何一致性、候選歧義及至少 200 ms 的送出前置時間仍由模型檢查。完整計畫的雜湊／來源／事件驗證在擷取首音前完成；只有不可修改且可移轉一次的 Prepared 值能綁定新錨點，初始化不會更新觀測時間。

明確的 `shared-in-prefix-score-v1` HUD 候選綁定 IN 選曲；每幀仍需 pause／IN prefix／七位數 score 結構，含原 template 已判為 playing 的畫面。暫停／結算優先，unknown 不更新 UI 權限。共通 HUD 不推論曲名、等級或 AP，各曲實際像素仍待全測；判定線穿越數字區可能保守拒絕。

`--tight-projection-speed-bounds 1` 是另外有版本的接點候選，只收緊投影後的解析速度上界。原 critical steps、完整 Hold trajectory、4 px 誤差預算與原預設策略保留；不能藉此略過慢 RPC。

六首獨立路徑稽核在190,473個樣本的最大held誤差為3.89843px，16,806個critical keys的時間／phase／pixel與原計畫完全一致。事件數減少會改變循環RPC成本落在哪個ordinal，成本回放通過數增加只代表合成可行性，不可推論實機延遲已改善。[完整稽核](../../local-assets/phigros-4.0.1/chapter-legacy-tight-speed-replay-001/report.json)保留所有失敗。

## 全測範圍

全測清單保留全部 20 首 IN、原生順序、每首一輪，先用封存執行檔完成全部離線 check，再由操作員選曲及按 Play。執行時 20 ms lateness、500 ms UI receipt、100 ms anchor、五指實際容量證據與 release／STOP 路徑不變；任一實際失敗便停止，不自動重試或跳過曲目。

舊 RPC durations 的循環壓力測試失敗保留為 qualification_warnings，與阻止候選建立的匯入／模型／計畫錯誤分列。這是候選準備資格的區分，不是放寬實際執行 guard。尤其雙押的第一個同步 RPC 若花超過 20 ms，第二指仍會按原門檻停止；接點精簡不能消除這個瓶頸。

最新逐曲結果與封存識別見 [測試計畫](CHAPTER_LEGACY_TEST_PLAN.md)；歷史 001 前檢查與已驗證 Dlyrotz AP 均維持原紀錄。

## 2026-10-24：分數被音符遮擋的共通 HUD v2

使用者啟動sweep004後，Glaciaxion首輪在歌曲約62.7s以actual_UI_receipt_expired停止。連續18張來源有效影格、pause／IN prefix仍吻合，只有分數結構拒絕；Drag白邊接到第一個分數字，500.275ms後按原規則釋放。288 DOWN／287 UP，清理後kernel零。原始回合與[稽核](../../local-assets/phigros-4.0.1/chapter-legacy-round01-ui-audit-001/README.md)保留，未改為成功。

新增明確 `shared-in-cell-score-v2`，舊v1與title政策不變。新政策從乾淨reference取得baseline／右錨點／字形寬度界限；分數實際是比例寬字型，不套固定pitch。仍須七個實際分離的baseline字形，逐字檢查實際墨跡範圍、暗色負空間、對比、位置與間距；只允許首個碰左邊界的字形有限裁掉外接note，重新計算觀測墨跡，不補造像素、不拆合併多字、不借舊觀測。裁切前後框及支持數寫入journal。

[233筆／204個unique RGB核對](../../local-assets/phigros-4.0.1/chapter-legacy-score-cell-validation-001/README.md)：本次45張playing圖全通過，包含兩張失敗圖；116張既有實際正例不退步，39個嚴格負例全拒絕。Dlyrotz38.218s修復，但36.046／38.911s合併字形仍維持unknown；較大遮擋與其他18首全曲UI仍未資格化。沒有延長500ms UI watchdog或放寬100ms anchor／20ms lateness／pause／result／foreground／release。

新候選為build003／readiness004／sweep005。外層腳本在native manifest可用時，會直接列出真正failure與保存kernel_final_zero，避免只顯示Round exited 2。新版完整實機尚未重跑，依使用者要求只交付指令，不自動啟動。

## 共通HUD v3：字形相位，不是逐曲降門檻

Verruckt IN Lv.16字形各部分偏移−1／−2px，白色筆畫粗細也不同。前綴ROI沒有誤含數字，單一dx/dy平移也不夠；v2所有1291／1277影格因此拒絕。新增shared-in-subpixel-prefix-score-v3保留v2精確接受，再以binary白色前景形狀的1px鄰域支持（白色覆蓋≥.98、到255平均距離≤8）、反向多餘墨跡≤.10、墨跡比例≥.65、原負空間／對比、四方向五個非微小元件及bbox邊≤1px，要求同一連續且跨度≤2px的位移族。原始reference灰階誤差另記錄，沒有修補／正規化輸入像素。

八方向連通會接受某些棋盤偽字形，因此採4CC；數字／文字缺字、條紋、實心、棋盤、週期／多重族及狀態負例都保留拒絕。分數v2與pause優先、source／前景／100ms／500ms／20ms門檻未改。此政策只修HUD取得；目前Verruckt實際首音尚未成功取得，需由使用者操作新回合觀察。
