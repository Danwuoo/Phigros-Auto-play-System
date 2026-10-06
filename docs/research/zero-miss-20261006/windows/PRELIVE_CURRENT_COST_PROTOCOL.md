# current-rails-v1 本包成本事前規則

2026-10-06。本文件寫於任何本包 pipeline cost run 前。C5 §8.6 是跨場景完整冷鏈規則；
R3 的 host QPC／全分母／warmup／coverage／late／noise adequacy 作更嚴核對。
本 host 與歷史 X11-P source、capture、consumer/owner thread topology 不同，沒有繼承 R3 資格。
測的是 latest producer → current observer → pre-dispatch typed hook → sole owner/scheduler →
offline FakeTouch receipt → Journal enqueue。Journal writer 是獨立第三線程；owner 與 recognition
在同一 consumer。真 capture_complete、driver／gRPC及遊戲採納另待裝置，來源 age unknown。

baseline A 與候選 B 使用相同新 owner ABI、latest-only、execution ledger、instrumentation、
編譯閉包與生成 corpus；A 呼叫未改 v3，B 呼叫 current-rails-v1。lead35ms、uncertainty30ms，
正式profile bits=15，五contact；Drag/Flick沿原formal observer/owner，沒有新BVI契約或採納宣稱。
20ms producer cadence 是本包新 normal load；slow／10,000 attempt stress 固定8ms。
原8ms探索實作尚未執行cost，不拿此不同source取得任何比較資格。

## 一次有限順序

功能及三配置回歸／原PNG前綴完成後，凍結commit、source、binary、flags、依賴、
生成12張不同RGB／loop數、environment／背景程序摘要與commands。Release同機同root。
Tap、Hold、dense 各 A 四run（12 total），順序 tap1–4 → hold1–4 → dense1–4。
各run 256 warmup＋2304 measurement=2560 attempts；全attempt保留，measurement≥2000
owner-seen及full／measurement coverage≥90%。不能刪warmup／drop／reject／fault。
normal safety：全attempt產生、pool drop0、writer drop/fault0、backend failure0、release verified；
capture→owner p99≤100ms/max≤250ms，所有phase lateness p99≤15ms/max≤100ms。
Tap／Hold 必須有own Down/Up，空動作分布為NOT_READY，dense是容量計算負載而非採納測試。
必要 validity/hard gate 失敗即停止本批normal cost；未跑的AA明列未跑，不補安靜重測。
獨立的功能／fault／stress仍可完成，不能將stress資料補進noise資格。

noise只用A：每load pairs(1,2)/(3,4)，d=max abs pair差；T=max(floor,2d)。
各load observer／owner／capture→owner p95及p99 floor分別 .5／.25／3ms；
2d≤max(min四run metric×.30,floor)才adequate。前三批滿足C5的至少三批要求，第四批
用以形成R3完整pairs。全部AA必要gate與noise通過才以機讀JSON事前freeze容忍。
ABBA每load三批 A/B/B/A（36run、同2560分母），全部run及順序保存。
逐批兩A／兩B的quantile平均比較，p95/p99非退步≤T；至少一個已量瓶頸改善>T且傳遞
到capture→owner才能稱C5改善。沒有任何已證實優化收益的預設，功能新成本可以NOT_READY。
任一comparison必要gate失敗停止該比較；不換threshold、corpus、cadence或source補過關。

獨立 B fault/負載：slow、writer、rpc、fault各1000 attempts；Tap/dense各10000 attempts
長跑（8ms，另報wall/fake time；生成attempt與實際consumed分開）。Debug／ASan各Tap／Hold
1000 attempts，非即時qualification。功能fault控制覆蓋原113、新測試及受影響根tests。
必要無裝置輸入無法證實遊戲合法機會分母，不能升格成實戰gold。

## 限額與判定

producer pool3、logical latest1；history128×6/90ms；ledger128，終態retire水位禁止revive；
Journal8192 rows，writer壓力capacity2；transport event64；最多20000 receipt samples/run；
attempts最多10000+1。新OUT≤12GiB、metadata seal≤64MiB、free≥20GiB逐stage核。
完整observer scene／raw candidate batch／filtered scene／accepted plan／coverage／cancel與
receipt／release均進正式Journal；JSON建構、共同byte-admission dump、enqueue及receipt
join在整鏈中實量，writer dump/write另由第三線程執行。warmup及fault資料不刪。
每record保守加128B覆蓋Journal schema/domain後，row≤256KiB、count≤65536、raw stream≤1GiB；
任何critical full、writer fault／admission超限即supervisor停止owner並保留第一次release，
producer維持有限全attempt分母、後續owned=false；沒有觸控重試。新OUT逐stage核實，
後續run需要保留1GiB raw headroom，資源不足也NOT_READY，不能刪舊attempt換比較資格。
RSS每100 consumed量，初值包含12張stimulus；另讀OS PeakWorkingSet涵蓋raw JSON構造。
Release gate128MiB，Debug512MiB、
ASan1GiB。非同步writer完整close；record rows/count bounded，physical bytes事後另核。
formal Journal本身只有row cap，本host固定型別輸出及n/receipt count給出byte上限；
不把本host bounded output聲稱通用Journal byte admission。

全部stage沿qualified Job/identity/held/streams/deadline verifier，fresh stage/root/report；
native0／JSON存在均不代替verification0。原STOP／編譯失敗／存取錯誤永久保留。
若noise不足或action/coverage不足，AB未跑是門檻結果，整體NOT_READY；不宣稱所有冷gate已過。
