# M0 與 manual-mode 有限續作（2026-09-30）

在 `codex/baseline36-recovery` 接續；桌面 checkout 的未提交改動不改寫。
入口是 `pas manual-session`；選曲與 Play 由使用者在可見 emulator
逐次操作。沒有 `manual-mode` CLI，也不使用選曲／Play 腳本。
PLAYING 由唯一 PAS owner 持有觸控；UI 操作與換版限結算釋放後的 STANDBY。

新資料根：桌面 `measurements/game-assist/2026-09-30-m0-manual-continue`。
上一批 16/16 與所有 raw／binary 保留。新硬上限 12 次開局（中止亦計）、
raw／clips／logs 共 8 GiB；到上限停止。容量帳本在新資料根。

## 離線工作

1. 保存交接時既有 C36e dirty patch／source；它尚未驗收，不自動視為正確。
2. 以同一 RGB、同一 QPC、每 clip 冷重設，對照原 A36 library＋原 header
   與候選；逐檔檢 SHA，輸出完整 decision 與有界 FakeTouch 收據。
   原 header 必須與原 library 同用，因候選 GameObserver ABI 已不同。
3. 相交／突現線、線 ID、逐 Note→line 與旋轉 Hold 各保留少量例圖；
   AI 審核是 proposed，人工 gold=0。三幀缺先前狀態／長 Hold 部位者列 unknown。
4. 每項策略差異有合成／fake-clock 正反例。先跑相關回歸，再完整 Release；
   source／binary／config hash 凍結後才進實戰。資料工具不能回饋正式策略。
5. 比較 host 擷取／消費間隔、recognition、owner／RPC、失敗與 drop。
   source 絕對 age 未校準，保持 unknown；35 ms lead 不改稱擷取延遲。

## 開局前凍結的配對與停止條件

同一 lead35／uncertainty30／五指 profile；候選不按曲名改參數。
A36 SHA `de22022803a00ec0fd81ba19c8575b3728cb6e3bfd007ac73a360b08e08dc601`。
候選 binary 在相關冷回歸後另記 SHA。每輪 preflight 指紋須符合
1280×720／rotation1／RGB888 top-down／gRPC fast 256 KiB。

| 開局額度 | 譜面 | 固定版序 |
| --- | --- | --- |
| 1–2 | Glaciaxion HD6 | A36 → 候選 |
| 3–4 | Eradication Catastrophe HD7 | 候選 → A36 |
| 5–6 | Dlyrotz HD9 | A36 → 候選 |
| 7–8 | Dlyrotz IN13 | 候選 → A36 |
| 9–10 | Dlyrotz IN13，解釋第七／八輪重大退步 | A36 → C36f |
| 11–12 | Dlyrotz IN13，反向有限重複 | C36f → A36 |

原定最後四次為光 IN12／Glaciaxion HD6。第八輪後依下列停止條件先
診斷明顯退步，再將剩餘額度用於同一既有配對；不增加總輪次或容量。

危險重複／未知 Down 重試、錯接 Hold、失效不釋放一旦有實證，立刻停止
候選並保存證據；不能為完成矩陣繼續。普通成績退步於結算後先診斷，
未解釋的明顯退步不推廣候選；必要修正須重新離線驗證並有新版本界線，
剩餘額度只用於解釋既有配對，不擴張輪次。容量或 emulator／source 故障亦停。

## 報告契約

HD／IN 分開，保存每次 P/G/B/M、總判定分母、score、result SHA、完整或中止。
每項時序報環境、n、p50/p95/p99/max、jitter、drop/skip/failed。
target-frame 與 accepted Down 子集不能當獨立 Note 成功率；單輪高分不叫
穩定恢復或 AP。不同 source／binary 的修改、保留與回退逐項列。
不 merge main／push／訓練模型／恢復無限 AP goal。

## 使用者指定重設（2026-09-30）

使用者明確要求刪除重啟前兩輪，重新記錄 12 輪。刪除
`manual-session-12501552442800` 的原始資料、兩輪分析與該次 PAS 日誌；
不保留這兩輪成績，也不將它們納入新批比較。此處只保留刪除範圍與
使用者重設指示，容量帳本另記刪除檔案數與位元組數。

新批從 `manual-session-956845989800` 的 A36／Glaciaxion HD6 起算，
計數重設為 0/12，恢復上表完整 12 次順序。此後中止仍計一次開局，
8 GiB 與安全停止條件不變；一般重啟不再自行重設額度。
手動選曲與 Play 由使用者操作；Computer Use 已依使用者停止。

## 本批凍結與首輪

C36e binary SHA 為
`5dc869433bf20eb4c6caf46e07e06627e9612fbd9d81a925cc94d6215c68b51a`，
基底 `e23b3f9edfc2df5c6e1ce1aa4c8168af8aeea726` 加 dirty patch；
新資料根的 `candidate36e-freeze.json`、`candidate36e-source.patch` 與
`candidate36e-source/` 固定來源、原始建置 manifest 和第三方 DLL 雜湊。
18 個嵌入的正式來源 SHA 與工作樹相符。Release 214 passed／2 opt-in skipped；
其中 recorded-pixel opt-in 已另以 current138、historical558 各跑一次並通過，
結果／選單 pixels opt-in 尚未跑。這些均不是遊戲驗收。

首輪 A36 的 Glaciaxion HD6：857468 分、368/1/0/24（393 判定）；
結果 PNG 與 event segments 的 SHA 已核對。結算 STANDBY 後正常 Ctrl+C
退出，exit=0；未知觸控收據與 release failed／unknown 皆零。
維護 helper 只對 image path 與獨占 console 成員均已核對的 PAS 發出
Ctrl+C，不操作 emulator UI，也沒有強制終止 fallback。第二輪以相同
profile／指紋的凍結 C36e 測同曲，仍待使用者按 Play。

第二輪 C36e 同曲完成：867061 分、367/3/0/23（393 判定），max combo 87；
正常退出，未知觸控收據與 release failed／unknown 皆零。本批 2/12，
剩 10 次。使用者指出 Drag／Hold Miss 居多；逐 Note 指派仍 unknown。
完整首組成績、時序、Hold 取消語義與有界 pixels 查核在新資料根
`glaciaxion-pair-review.md`。兩版冷 replay 都有 Hold 邊緣列為 line 的疑點，
clip 未含 warm 取消瞬間，標為 proposed、不作人工 gold 或已確認 Miss 根因。
第二輪沒有 current Tap overlap Down；單組結果不構成 C36e 驗收。

第三輪按既定矩陣使用相同凍結 C36e，譜面 Eradication Catastrophe HD7；
不針對曲名／使用者觀察改參數，完成後再以 A36 配對。

使用者再次因斷線要求重啟 emulator。`manual-session-3371898928300` 的
stdout 停在初始 STANDBY，沒有 STARTING／PLAYING、round 目錄或 raw frame；
standby journal 與 pixel index 皆 0 bytes，沒有完整 session summary。
此為未正常收尾且未記錄開局的 session，保留原檔與
`attempt03-disconnection-audit.json`；不冒稱已正常釋放或實戰完成。
未記錄的外部狀態仍 unknown；依現有開局證據維持 2/12，接續原定第三輪，
不自行刪除已完成兩輪或重設 12 次上限。重啟使用 cold boot、相同凍結
C36e／DLL／profile，重新 preflight 後才進 STANDBY；Play 仍由使用者操作。

重新連線的 `manual-session-602167715800` 完成第三輪 C36e：
Eradication Catastrophe HD7，814200 分、174/4/0/22（200 判定），
max combo 39、accuracy 88.30%。result 與 event segments SHA 核對通過；
正常 Ctrl+C 退出，exit=0、未知觸控收據與 release failed／unknown 皆零。
27 raw frames、writer drops=0、pool drops=0；scheduler 有 gate 過期 1 次與
target/window 過期 2 次，完整時序與原始紀錄在 attempt03-c36e-analysis.json。
沒有 current Tap overlap Down。此時 3/12，剩 9 次；第四輪切回凍結 A36，
同 profile 重新 preflight，繼續同曲比較。

第四輪 A36 同曲完成：831000 分、181/0/0/19（200 判定），max combo 33、
accuracy 90.50%。SHA 核對與正常退出通過，未知觸控收據與 release failed／
unknown 皆零；本批 4/12、剩 8 次。此組 C36e 多 3 Miss，尚未改善；
PLAYING 間隔及 RPC 分布也不同，不能單獨歸因於策略。

使用者再次明確指出中段 Hold 與判定線一起旋轉，必須動態修訂觸點。
既有 owner 有 Move 修訂，但當前 body／幾何支持失效可先撤銷；不能只
增加 Move 就宣稱旋轉續按完成。新資料根 `eradication-pair-review.md`
保存成績、完整時序參照與下一輪冷驗證項目：同一 contact 的 head 接入、
body 延續與 tail 分開，依當前 pixels 支持修訂 Move、區分朝向／運動／
法向，覆蓋旋轉平移、反轉、掉幀與未知 Down 不重播。使用者觀察不作
逐 Note 人工 gold。第五輪仍按原矩陣使用凍結 A36，譜面 Dlyrotz HD9。

第五輪 A36 Dlyrotz HD9：934935 分、451/0/0/7（458 判定）、max combo 223、
accuracy 98.47%。SHA 核對、正常 Ctrl+C 退出通過；未知觸控收據與 release
failed／unknown 皆零，scheduler rejection=0。保存 27 raw frames，writer／
pool drops=0；完整 C++ 分布在 attempt05-a36-analysis.json。此時 5/12，
剩 7 次，第六輪以凍結 C36e 比較同曲。

使用者指出 Drag 少量 lost，以及 Hold 與判定線一起移動就斷觸。冷修正
需求明確包含平移與旋轉中的同 contact body 支持／Move 修訂；不能只
修轉向或把觸點鎖在初始 Down。基線有 active Hold 取消 34 次（missing 20、
geometry unsupported 6、held-region unsupported 3、identity ambiguous 7
為含開始前後的各原因數），但不能把取消事件直接指派給 7 個 Miss。
head 接入、body 延續與 tail 結束仍分開驗證；整輪使用者觀察不升格
逐 Note gold，預測或舊 body 不補作當前支持。本批正式策略不變。

第六輪 C36e Dlyrotz HD9：942576 分、452/0/0/6（458 判定）、max combo 249、
accuracy 98.69%，較 A36 少 1 Miss／多 7641 分。SHA 核對與正常退出通過，
未知觸控收據與 release failed／unknown 皆零；27 raw frames、writer／
pool drops=0。完整配對時序及本地取消語義見新資料根
`dlyrotz-hd-pair-review.md`。使用者仍指出 Hold 斷觸為主要問題；未指派
個別 Miss 或升格人工 gold。此時 6/12、剩 6 次；第七輪 C36e Dlyrotz IN13，
HD／IN 分開報告。前三组配對仍有進退，候選未驗收。

第七輪 C36e Dlyrotz IN13：534289 分、330/5/6/243（584 判定）、max combo 121、
accuracy 57.06%。SHA 核對與正常退出通過，未知觸控收據及 release failed／
unknown 皆零；27 raw frames，writer／pool drops=0。此時 7/12、剩 5 次，
第八輪切回凍結 A36 測同一 IN13，不能把 HD9 的成功率當此譜面基線。

使用者指出非真實判定線的白線干擾、抗震動不足、真實判定線未觸控。
完整 C++ 診斷有 PLAYING 無 line 935 frames、全輪 multi-line 1541 frames，
以及重複 target-frame reason line_unobservable 7066、未驗多線關聯 522；
這些不等於獨立 Miss 數，也不能由白色或多線直接指派真偽。
新資料根 attempt07-c36e-cold-audit 核對 27 RGB SHA，clip6 的代表例圖
17863–17865 顯示水平線與 Hold／Drag；此短片未證明使用者所述假白線／
震動故障，不升格 gold。warm 狀態與逐 Note 遊戲效果仍缺。

冷修正增加可重現的白線反例與有界噪聲回歸：Hold rails／裝飾白線不能
僅憑長度、白色或信心成為觸控資格；維持線身分與 note→line 支持，
將純觀測抖動和真實平移、旋轉、反轉、突現分開。穩定化不能把真實
運動濾掉、把插補當當前 pixels，或重播已執行／未知 Down。正式策略
仍凍結，先用第八輪完成同譜面比較。

## 第八輪重大退步與 C36f

第八輪 A36 Dlyrotz IN13：747192 分、461/16/5/102（584 判定）、
max combo 121、accuracy 80.72%。SHA 核對與正常退出通過，未知觸控
收據與 release failed／unknown 皆零。使用者指出問題相近但好很多。
本批 8/12、剩 4 次；相對 C36e 多 212903 分、少 141 Miss。C36e 不推廣。
配對完整證據與分母在新資料根 `dlyrotz-in-pair-review.md`。

同一 54 張 RGB，A36 與 C36e 每 clip 冷 replay 的 scene／line／target
完全一致；不能解釋 warm 退步或證明整輪辨識等價。新增 C++ 只讀
line-history 診斷核對 event segment SHA，用當前線幾何隔離 tracker。
全輪 association_valid replay 差異 0；缺 pre-round／跳過的辨識幀，
不是 RGB 或 Note／owner replay，不連 emulator／真觸控，不作 gold。

C36e 有線但全線關聯無效 1316 frames，A36 268；最長分別
4898.7956／1167.1858ms。原有 greedy tracker 在 contested 時拒絕回配，
卻將無效觀測存成 fresh historical track，後續單線不斷與這些近似
軌跡競爭／生新 ID。這個循環在 A36 也存在，兩輪觸發頻度不同。
候選 frame18319–18613 的長線 ID 逐幀增加、motion_samples=1；缺完整
RGB，尚不能判定最初近似線的遊戲身分或將全部額外 Miss 歸因此故障。

C36f-no-contested-line-birth 只修改該出生分支：無效且未指派的當前線
不註冊進歷史、ID=0、無 motion fit。當前歧義仍拒絕動作，不猜舊 ID。
原有有效軌跡仍依 90ms／100ms 契約過期；owner、Note 關聯、motion fit、
lead、deadline 不改，保留 C36e 的獨立 Tap overlap／ridge splitter。
不能宣稱假白線、抖動或旋轉／平移 Hold 已整體修復。

兩個新增 fake-clock 回歸在修正前均失敗、修正後通過：一次近似重複
觀測後回到原 ID；兩條既有線合併後，在歷史到期前拒絕猜 ID，再以
當前觀測建立新 ID 且不反覆污染。相關 18 回歸通過。相同完整線幾何
重播中，invalid line observations 從 C36e 1538→78、A36 356→52；
剩餘歧義仍拒絕。此為 derived 重播的機制改善，未作遊戲／身分 gold。
C36e→C36f 的同 54 張 RGB 冷 scene 比較仍全同。

完整 Release 216 passed／2 opt-in skipped、0 failed，共 218 列。另核對
recorded pixels 的 prescreen 等價；結果／選單 pixels opt-in 仍未跑。
另凍結 source patch／snapshot、build manifest、binary／DLL／profile SHA，
再交付剩餘四輪的 A36→C36f→C36f→A36。全部同 Dlyrotz IN13、共同
通用策略，曲名僅診斷 metadata；選曲／Play 仍由使用者操作。
C36f 尚待實戰，不合併 main／push，也不增加額度。

C36f 冷檢查與凍結已完成：binary SHA
`f7f7bbffab71ae92b071bf8eb36cfc94e39adf216cd01e7c345c29a41a290ed0`；
23 個 source snapshot 檔、18 個 build header 正式來源 SHA、12 個 DLL SHA
見新資料根 candidate36f-freeze.json；source hash match 已核對。
embedded session manifest 待首次 C36f STANDBY 再核對。
第九輪 A36 preflight 通過，PID2092／manual-session-8695119652800，初始
STANDBY 與 executable SHA 均核對。帳本仍 8/12，等待使用者選 Dlyrotz IN13
按 Play；不把待命啟動另計一輪。資料根保守容量約 1.02 GB／8 GiB。

## 第九輪 A36 A/A 重大波動

第九輪仍是原凍結 A36，同 Dlyrotz IN13：549623 分、338/8/7/231（584），
max combo121、accuracy58.77%、Early/Late4/4。相對第八輪同 binary 少
197569分、多129Miss。版本、source build clean、profile／指紋相同，
emulator未重啟。結果／segment SHA verified，正常 Ctrl+C exit0／STOPPED；
未知觸控收據0、release failed／unknown皆空。9/12，剩3次。

C++ line-history 以全輪7684決策的線幾何重現 association_valid，差異0；
有線而全線無效1135 frames（第八輪268），最長2814.7887ms。
invalid observations1331；相同資料 C36f replay58。這是derived tracker
診斷，不是完整RGB／Note／owner replay或遊戲身分gold。A36本身會觸發
同一出生循環，先前C36e→A36的大幅差距不能全歸候選改動。候選仍未
驗收，基線也未證明穩定；不挑最高輪取代第九輪。完整結果、分母、
時序與限制追加於資料根 dlyrotz-in-pair-review.md。

第十輪按固定順序換 C36f-no-contested-line-birth，同Dlyrotz IN13，
binary／12個DLL／profile SHA及 preflight均已核對，等待使用者Play。
策略／lead不改，剩3次順序C36f→C36f→A36，硬上限仍12次／8GiB。

第十輪 C36f 已核對初始 STANDBY，PID2492／新 session manifest 的 binary、
variant與18個embedded source SHA均符合candidate36f-freeze.json，待使用者
選同一Dlyrotz IN13／Play。freeze補記首個實際manifest與SHA，前述待核對
項已完成；尚無C36f實戰結算。帳本9/12，待命本身不額外計次。

## 第十輪 C36f 首輪實戰改善與使用者新需求

第十輪Dlyrotz IN13：783793分、488/11/4/81（584），maxcombo121，
accuracy84.79%、Early/Late7/4。比第九A36少150Miss、比第八A36少21Miss。
結果／segments SHA verified，正常Ctrl+C exit0／STOPPED，未知觸控收據0、
release failed／unknown皆空。10/12、剩2次；單輪改善未作穩定／跨曲驗收。

C++ line-history以本輪7798決策重播C36f，association差異0；PLAYING有線
而全線無效2frames，invalid observations79。相同線幾何A36 tracker則
產生1517invalid observations／association差異1425frames。這是derived
診斷，缺RGB／Note／owner全鏈重播與逐Notegold。完整時序／分母／SHA與
本輪例圖索引追加於資料根dlyrotz-in-pair-review.md。

使用者明確補充：Note移動方向垂直於線、Note長條朝向垂直於線，兩種
都要考慮；原線仍清楚時向外移出的近似假線顏色微變而不閃爍，需避免
非法線干擾。新增NOTE_LINE_ROLE_FOLLOWUP_20260930.md記功能契約與C++
fake-clock正負例，尚未實作到凍結C36f，不升格逐frame人工gold。
剩兩次仍凍結C36f→A36同譜面比較，第十一preflight通過；批後冷修新需求。

第十一輪相同凍結C36f已核對初始STANDBY，PID20368；binary／variant／
18個embedded source SHA符合freeze。帳本10/12，待使用者同Dlyrotz IN13
按Play，重複比較本輪改善；新垂直／線角色規則尚未混入。

## 第十一輪 C36f 有限重複

777560分、489/7/7/81（584判定）、maxcombo99、accuracy84.51%、Early/Late6/1。
C36f兩次皆81Miss，但第二次Bad多3／combo較低；不只報較好子集。
segments/result SHA verified，正常Ctrl+C exit0／STOPPED，unknown contact0、
release failed／unknown皆空。11/12，剩1次，仍不作跨曲／通用穩定驗收。

本輪有線而全線association invalid4frames、最長5.3116ms；invalid
observations73，7789決策的C36f derived geometry replay association差異0。
逐Note遊戲效果／角色gold仍缺。完整時序／分母追加dlyrotz-in-pair-review.md。
第十二輪原凍結A36，同Dlyrotz IN13，preflight通過；策略／profile／lead不改，
完成後本批停止PAS，不加第十三輪。使用者新需求仍按
NOTE_LINE_ROLE_FOLLOWUP_20260930.md在批後冷修，不混入本批。

第十二輪原A36已核對初始STANDBY，PID5476；executable SHA與
source_build_commit cd0ec437／clean符合凍結基線。帳本11/12，
使用者同Dlyrotz IN13按Play，作本批最後一次比較。

## 第十二輪完成與本批停止

第十二輪原凍結A36／Dlyrotz IN13：665206分、413/8/5/158（584判定），
max combo121、accuracy71.61%、Early/Late5/3。結果／segments SHA verified，
round result_confirmed；未知contact收據0、round release failed／unknown皆空。
結算後gRPC擷取中斷（status14），session以FAULT收尾；PAS／emulator／qemu
程序皆已退出。沒有發送Ctrl+C，不宣稱正常exit0或獨立驗證fault後釋放。

本批12/12、剩0次；使用者明確要求「停止本批測試，開始優化調參」。
capacity-ledger.json active_launch=null、live_testing_stopped。不啟動emulator、
manual-session或真觸控；後續授權範圍為離線C++規則／參數與fake-clock驗證。
最後A36較第十一C36f多77Miss、少112354分；C36f兩次81Miss，A36同譜面三次
102／231／158Miss，存在重大A/A波動。只支持有限比較中的機制改善，不宣稱
AP、跨曲穩定或每個Miss的原因已知。全部12輪與FAULT保留，不挑最好子集。

第十二輪line-history：7374決策、717 invalid observations，有線而全線無效
605 frames，24段、最長5028.6916ms；A36 derived geometry replay association
差異0。27RGB clips frames均關閉保存。Recognition n7374，p50/p95/p99/max
5.3419/9.09008/12.604351/20.5697ms；來源絕對age仍unknown。完整分母與
間隔／host residency／jitter见資料根dlyrotz-in-pair-review.md。

批後先保留C36f與既有A36／C36e executable SHA及證據，按
NOTE_LINE_ROLE_FOLLOWUP_20260930.md修正Note朝向與運動的獨立觀測、
多線承載支持及Hold旋轉延續。新增合成像素／fake-clock正反例先記修前
失敗，再改正式規則。原12輪共330張RGB另作逐clip冷重播基線；短窗沒有
完整warm history，不升格人工gold，也不能驗收假線的實戰角色或遊戲命中。

## 另授權的 C36g 首輪驗收與 Miss 標註

此為原12/12封存後的獨立階段`acceptance36g-01`，不加第十三輪到舊帳本。
使用者要求啟動emulator及manual-mode，選曲／Play仍由使用者操作。
C36g同Dlyrotz IN13：796284分、491/18/4/71（584），max combo126、
accuracy86.08%、Early/Late15/3。PAS正常Ctrl+C exit0／STOPPED，結果與
segments SHA verified、unknown contact0、release failed／unknown皆空。
只有這一輪，不能宣稱大幅改善；使用者亦回報問題未大幅改善、要求找Miss影格標註。

新增離線C++20 `pas_miss_review`及27張原圖／當時候選對照頁，9組首尾跨度
相加231.1412ms。228疑似事件有221個±500ms缺圖，只有1個source_frame
保存。第一組frame27449 Hold身分歧義撤銷可見當前body，已提出3張body／rail／
tail／水平線圈選；Miss結果未知，human gold0。詳見
[標註流程與補採缺口](MISS_FRAME_ANNOTATION_20260930.md)。沒有追加實戰或調寬觸控門檻。
