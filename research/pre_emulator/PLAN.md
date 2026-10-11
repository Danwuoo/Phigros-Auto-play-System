2026-10-11 最新 main53／`11079f8`：CPU32／584、reader2／81、production52／七控制、runtime18來源／36副本通過；原 producer wait43 與新 diagnostic20 分列通過。normal Tap 完整 raw10／3,321,144B integrity 通過，producer／auditor Native 皆0；原數值 pool drops6、producer／callback deadline overruns149而 STOP。完整946份 source、原 raw／STOP與小收據見[main53負結果](../../docs/research/live-readiness-20261007/MAIN53_PRODUCTION_TAP_NEGATIVE_20261011.json)。Hold／Dense／AA／noise／ABBA未跑。

新有界 wait telemetry 定位兩次 blocking wait：warm149 設17.6978ms、Wait跨782.3458ms；measurement1996設17.5702ms、跨825.7037ms，皆 raw WAIT_OBJECT_0、無 spin；不能由 thread CPU counter零差認定精確off-CPU，也不能推定 timer signal、dispatch、sleep 或 guard 根因。只讀 power 記錄未見該輪 sleep／resume，目前 idle sleep AC／DC為Never；guard無runtime suspend，沒有逐poll交集資料。原 main32 PASS不撤銷。

下一 main54 cold synthetic producer 改為 full_period_active_qpc：整段 remaining period 以QPC／YieldProcessor主動等待，initial horizon100ms／total poll cap2^24；不建立或阻塞timer，absolute epoch／2560／20ms／warm256／measurement2304與全部原數值界限保持。增加CPU使用量尚待原 workload量測，A／B同policy，不冒稱2ms active tail；正式 capture／manual runtime、writer、優先序與affinity不改。原43／584及diagnostic20不重定義，另列active wait12與readonly policy12必要控制，目前尚待fresh Native驗證。

原75／production9不重跑；必要身分建置／控制後續 normal3 → AA12／noise18 → 原未跑ABBA36 → 完整readonly → 裝置邊界。最新完成noise仍main44的6／18 adequate、ABBA0／36。cost NOT_READY／live false；本輪不開emulator、裝置或新thread。

2026-10-11 最新 main52／`c7aad78`：CPU32／584 controls、reader2／81、production52／七個控制執行及新27項 producer placement 控制通過，既有 producer wait43 控制仍單列。normal Tap 完整 raw／原成本數值通過；Hold 完整 raw integrity 通過，但 pool drops2、producer deadline overruns115（warmup37、measurement1＋77；gap769.3928／1551.2314ms）。四個 producer／auditor Native 皆退出0；完整946份 source 快照、原 raw／STOP及小收據保留於[main52負結果](../../docs/research/live-readiness-20261007/MAIN52_PRODUCTION_HOLD_NEGATIVE_20261011.json)。

priority／affinity 的實際 readback／explicit restore verified，但 timing 尚未修好；根因 unknown，不能認定 OS 或 guard cause。下一 main53 僅添加 diagnostic-only 有界 timer／spin／thread CPU telemetry，不調 policy 或 gates，目前尚未 compile；新 nested diagnostic_controls 為20項，與原43／584分開，尚待 fresh Native 實際驗證。default AA 與正式 manual 不啟用此診斷，score no-gameplayfiles 契約保持。

原75／production9不重跑；必要身分建置與控制後仍依 normal3 → AA12／noise18 → ABBA36 → 完整 readonly → 裝置邊界順序。最新已完成 noise 仍為 main44 的6／18 adequate，ABBA0／36；cost NOT_READY／live false，本輪不開 emulator、装置或新thread。

2026-10-11 最新 main51／`8257eb0`：CPU32／584、reader2／81、production52／七控制、runtime18來源／36副本通過；必要normal Tap數值通過，Hold完整raw integrity通過但producer deadline overrun1，Dense／AA／noise／ABBA未跑。原四Native皆退出0；負結果與原source／raw／STOP見[main51記錄](../../docs/research/live-readiness-20261007/MAIN51_PRODUCTION_HOLD_NEGATIVE_20261011.json)。下一main52只在cold synthetic producer重用現有AA producer affinity，補readback／explicit restore正反控制；writer與正式manual runtime不改，原HIGHEST2／2ms tail／2560／20ms／全部數值門檻保留。現有source資格契約須fresh必要CPU32／reader2／production52與七控制，再完成normal3／AA12／noise18／未跑ABBA36／完整readonly。原75／production9不重跑；最新完成noise仍main44的6／18 adequate，ABBA0／36；完成至emulator邊界停止，不開裝置或新thread。

2026-10-11 main50 `775d5c1` CPU32/584 controls7、reader2/81、pre-cost actual source/base joins通過；production core/action/emulator/bench四groups通過，entry在24.205001s因`held-identity-cap64` STOP，runner125/native exit unknown，identityfalse/cleanupunknown，streams完整、active0及64 held全部signaled。原33小pins/full945source/native/logs/checkpoints保留於`MAIN50_PRODUCTION_ENTRY_NEGATIVE_20261011.json`；沒有compiler error結論，pipeline/affected controls/normal3/AA/noise/ABBA未跑。下一main51只修冷build runtime部署：configure時applocal OFF，以完整original installed Release/bin十八DLL pins直接file(COPY)到root/production，逐來源及兩副本SHA、actual52graph/seals/before-after核對；原六groups/52TUs/64held及R2不改。此修補不是main50 STOP轉PASS，current fresh收據與noise/ABBA仍待實際取得；不重跑原75/prod9，完成至emulator邊界停止、無新thread。

# main 的 emulator 啟動前工作包 — 2026-10-08

2026-10-11 最新續作：使用者要求完成全部啟動前可冷軟體；CPU等值修補新增明確32TU／兩fresh library分支，validator及正式pas.exe接線同步完成。原75／production9不重跑；新noise通過後補未跑ABBA，完成後不開新thread。見[修補契約](../../docs/research/live-readiness-20261007/CPU_COMPONENT_REPAIR_20261011.md)。

# 2026-10-11 最新縮小重驗

main42／`96c8c2a` consumer HIGHEST(2)候選AA12 validity全PASS、producer／publication各12筆零超期，
noise 12不足／owner6 adequate、ABBA NOT_RUN；actual reader base／delta／AA／18公式及final bytes
全核對，source snapshot與原負結果保留。main43／`ba1aaa6`同923 source pins的兩TU有界診斷
通過，A/B measurement各744、九phase端點全一致；主要成本仍observer／hook，CPU量化未量測。
下一候選只改固定producer active tail20ms→2ms，不改兩thread HIGHEST(2)、NORMAL32、
generic末兩core／原due與20ms cadence／2560嘗試／4早醒／poll硬界或R2。kernel只等至due−2ms，
tail繼續QPC／pause；精確plan／controls／compile source綁定新2ms與封存舊20ms。先完成必要
四TUs與43 deadline controls、兩TU reader／46 controls及實際base/delta核對，再新AA／noise及未跑ABBA。
不以同binary重複挑好值，不重跑原75／production9；完成所有冷工作後停在emulator邊界。

main32／aa9de3c的75冷組、完整圖3、3063 prefix、fault4及production9已通過；AA12
validity全PASS但原noise不足、ABBA未跑。使用者要求修復後不全盤重跑，仍完成啟動前
必要開發與未跑noise／ABBA；成功至需emulator處停止，先不要建立新thread。
見[授權及base/delta身分](../../docs/research/live-readiness-20261007/NOISE_REPAIR_SCOPE_20261011.md)。
main36 fresh AA12 validity及priority還原通過，但原noise仍12欄不足、ABBA未跑。
main37／`156214a`的A/B dense1000診斷全部native通過；observer不同core端點
583/733及558/739。這是實際endpoint事實，不證明整段residency或所有noise根因。
新冷worker affinity候選依有界OS topology的最高rawEfficiencyClass，選兩個不同實體
core及各自最低LP；consumer先、producer後，A/B同policy，writer在scope前建立。
保存並核對原group/mask、set/readback與explicit restore，多group或不完整資訊明確fail。
只重新編譯必要chain TU、36項priority及65項affinity控制，沿用原source-bound Release libs；
新current_chain、原analyzer／auditor各自SHA綁定。正式src/include/pas及production binary
不改；保留原負結果，完整AA12／noise／ABBA原gate不降。後文fresh全盤要求為先前包規則，
本次以明確使用者縮小重驗指示為準。新delta不是新三配置whole或live資格。
同步補既有main32 preservation與新narrow cost的只讀bundle joins，不授qualified token。
main38／`b37516d` fresh3TU／priority36／affinity65、reader fresh2TU／34 controls及實際base
bundle joins通過；首Tap2560原raw audit通過，但原gate因producer1／publication2超期失敗。
controller helper scope STOP亦保留，923 source snapshot及原status不改。下一修補原due與
全部attempt不變，producer固定20ms QPC active tail／2^24 poll硬界，遠距原timer到tail；
CPU成本照報，不能推定OS延遲根因。只fresh四必要TUs與原22＋新21共43 deadline controls，
reader仍兩TUs／34controls；原75／production9不全盤重跑，原R2及正式libs不改。
讀取current device/profile/physical實際收據及live啟用接線依需emulator的階段完成，
這輪先完成其餘所有能冷完成的開發／測試，不建立新thread。

main39／`f28643c`四必要TUs、36／65／43 controls與reader34及實際pre-cost joins通過；
新AA12 validity全PASS，Tap／Hold producer及publication超期0。原noise 12項不足／owner6
通過，ABBA NOT_RUN；dense3 warmup producer1／publication2照報，原R2零超期gate僅
streaming Tap／Hold，未放寬dense原gate。完整負結果及923 source snapshot保留。
下一候選只把最高raw class每core最低LP選位改成排序末兩個，consumer較低／producer較高，
仍由有界OS查詢決定，不硬編LP12／14、不聲稱CPU0根因；A/B同policy且65 controls不減。
只讀post-noise runner在native前因遞迴導入舊freeze／closure leaf中metadata超8192而STOP；
下一runner封存leaf／freeze本身、不遞迴其歷史file list，reader仍SHA核對、完整denominator
及before/after joins。原8192上限不改，原75／production9不重跑。

main40／`f40fecf`四必要TUs及36／65／43、reader34／actual pre-cost joins通過；
前兩Tap validity PASS，第三Tap原gate因producer／publication各1超期STOP，後九筆／
noise／ABBA NOT_RUN。attempt2462開始晚20.7934ms，render .2661ms、publication .1785ms，
前一publication至該due僅19.6515ms，因此當次無kernel timer分支；失去CPU的具體邊界未量測。
下一候選只將producer scoped thread priority固定為HIGHEST(2)，consumer仍1／writer原值／
process NORMAL32、LP generic policy與原20ms tail、所有attempt及R2門檻不變；43 controls
更新requested/readback期待，不加MMCSS或real-time process，不把假說當實測根因。

main41／`7057be7`四必要TUs及36／65／43、reader34／actual pre-cost joins通過；
AA12 validity全部PASS、producer／publication超期12/12皆0。noise8項adequate／10不足，
ABBA NOT_RUN；actual只讀post-noise核對base／prefix／fault／production／delta／AA／final
bytes PASS，18公式全部一致、files6760、正確NOT_READY，未發live資格。
下一候選只將consumer scoped priority1→2，producer2／NORMAL32／其他policy不變。
新plan及controls以consumer_priority_requested精確綁定2，舊1只接受已識別legacy header／
tests SHA pair；AA／ABBA及reader按該唯一值核對requested/readback與原restore安全條件。
必要四TUs及36／65／43不增減，reader新增六policy fixtures為40；不全盤重跑原75／production9。

2026-10-10 main31／`afaa8f3` fresh22 TUs、三配置各25組、writer20／auditor93、完整圖3、
3063 prefix及fault4通過；production normal Tap／Hold完整raw與原數值通過。normal Dense
native2／2.993s，僅145張、round `user_stop_aborted`，沒有report；guard identity／held／
streams／active0皆正常。原件未分辨Escape或console stop，不能認定具體外部輸入；原完整
raw、908 formal＋方向／guard共912份SHA snapshot及STOP見
`out/prelive-pre-emulator-planning/main31-stage-inventory.json`。後六production、AA12／noise／
ABBA36 NOT_RUN，cost NOT_READY／live UNQUALIFIED。先前ASan prelaunch容量STOP亦保留；
12個歷史compiler roots逐entry ZIP讀回SHA、原件再核對後清理，回收3.08GiB，僅續跑同source
原NOT_RUN三目標，不重測已完成資格，見`main31-asan-capacity-recovery32.json`。
冷production／score-only／fault fixture新增有界可控keyboard／console-registration doubles，
正式`src/manual_session.cpp`及其Escape／Ctrl+C停止行為不改，真實外部console終止仍記負結果。
原8 invalid與實際Stop loop Escape／Ctrl+C兩種取消／釋放／無遊戲檔案回歸通過，原2560／20ms
完整Dense及原raw auditor通過、診斷數值true；只屬組件，不授candidate／cost／physical資格。
組件configure26 native1、compile27／28 native2與原來源／輸出保留；修正依賴及生成include後
component29通過，見`out/prelive-pre-emulator-planning/cold-stop-prototype-29-result.json`。
原完整archive／R2期限及coverage／RSS／容量不改。新整合仍fresh三配置與原全包，未啟動裝置；
成功到emulator邊界才建立指定GPT-6.1 Sol／xhigh local main task，沿用Chapter Legacy IN一輪
只記分數／整理後停止授權，新task取得當前裝置收據及完成validator後才觸控。


2026-10-09 續派範圍見 [一輪分數授權](../../docs/research/live-readiness-20261007/SINGLE_PASS_SCOPE_20261009.md)。
2026-10-10 main30／`f6548e5` fresh22 TUs三配置各25、完整圖3／prefix3063／fault4通過。
production normal Tap／Hold／Dense原數值及raw通過，stress Tap／Dense與Debug Tap integrity
通過；stress Dense producer0／123.951s，原mailbox overrun未重現，normal cost false保留。
Debug Hold producer0／auditor2：五個Down各有中途成功backend release，原auditor直到最後
才處理release，錯把下一個不同intent的contact重用當搶占。907 formal及guard／方向共911
SHA snapshot、原raw／stderr／STOP保存，ASan後兩項及AA12／noise／ABBA36 NOT_RUN。
盤點 `out/prelive-pre-emulator-planning/main30-stage-inventory.json`；cost NOT_READY／live
UNQUALIFIED。新診斷join按原journal順序核對receipt及release的sole-owner QPC；最多五個
own active IDs、dispatched、failed／unknown空、完成後才重用。原Down intent不復活，
Move／Up必有未釋放的own Down；release單獨計數、不充explicit Up或game effect。
原67＋新26共93控制及七份原完整raw組件核對通過；normal原數值／stress與Debug負結果照報，
不授candidate資格。builder每配置要求93 controls；新來源仍fresh三配置及其後原全包。
runtime、完整archive workload、容量及R2門檻不改；未啟動裝置。

2026-10-10 main29／`208ad7b` 三配置各25、writer20、auditor67、fresh22 TUs及
三份完整圖／3063 prefix通過。capture／writer故障及audit通過；RPC producer0、
auditor0/control_pass=true但guard runner125：root-exit未驗image的PID9008 error5，
image unknown，不能推定conhost或以active0補足身分。slow／production9／AA12／noise／
ABBA36 NOT_RUN，原status將已執行RPC寫NOT_RUN的錯誤也保存；以
`out/prelive-pre-emulator-planning/main29-stage-inventory.json`更正索引，不改原件。
原906 formal source及guard／方向文件共910份SHA snapshot保存。
guard在resume後立即acquire、首50ms sleep1ms，之後原25ms；原image／held／snapshot／
shared50ms drain與所有時限不改。39／9／新12 controls、native4及固定short RPC24
診斷通過；組件不授candidate資格。新gate main06；reader核對新增controls及shared
source binding。新fault status區分已執行失敗與真正未跑；原R2／runtime／archive不變。
新整合重新fresh三配置及其後全包，不復活main29 STOP或採挑選成功的重測。
2026-10-10 main28／`62a8000` 三配置 fresh22 TUs／各25組（writer20項）全部通過，
3063 prefix／fault4／production normal Tap、Hold、Dense 完整 raw及原數值 gates通過。
stress Tap producer0／85.429s，auditor2：原取消核對僅接受Hold，raw有六次已Down的Tap
於物件消失後取消Up。原905 source SHA snapshot／raw／STOP／後五項與AA/noise/ABBA
NOT_RUN保存，不能授成本或live資格。新取消核對保留原own contact／position／最新plan／
100ms evidence lease／current packet QPC／唯一收據／executed prefix要求；僅增加兩種
明列Tap basis、原Down+Up兩步／18ms間隔／cursor1／40ms missing grace的取消Up。
Down／Move不能用此路徑；Drag／Flick／未知kind仍拒絕，Tap不得冒稱Hold或game effect。
原39＋新28控制共67及原完整stress Tap raw獨立組件核對通過，六次取消皆有own Up；
原68 pool drops、coverage .8855、producer2／callback3 overruns與normal cost false照報。
組件不授整體資格；新整合仍重新三配置及所有後項，原runtime／策略／R2 gates不變。
main28只讀圖：Release通過，Debug攤平8247檔超單stage8192；修正為凍結完整transitive
metadata，由原C++ Reader逐檔SHA並finish再驗，原native/graph caps不改。Debug通過；
ASan reader877.054s watchdog停止且清理active0／held/streams完整，原負結果保存。
以同source sealed Release reader核對完整ASan圖通過（291.359s），明列核對工具配置。
`build_graphs.ps1`正式化此核對路徑；三配置的實際binaries／instrumentation／25 controls
仍各自保留，reader始終只讀、不發admission。新來源整包須另驗，未啟動emulator。

main27／`2082a9d` Release／Debug 各25通過；ASan19通過後 score-only Tap native2
（22.398s）：未從 result pixels 完成，沒有 control-report，不能推定觸控計數。
原件／904 source SHA snapshot／STOP 保存，後五項與 graph/prefix/fault/production/AA/ABBA
未跑。原 core 的結算診斷 n16，total p50/p95/p99/max=94.921/113.172/115.639/116.256ms，
5張超過原100ms freshness，六標籤均辨識正確但確認重置；不是記憶體報錯。
新 matcher 對固定 label ROI（至多22×2 uint64、352 bytes）一次計算白色像素，
以 constexpr template rows／popcount 算原 intersection/union；±3px、.72門檻、
色彩條件、完整標籤、source freshness／result confirmation／所有期限不改。
component23 原19項及新精確原演算法 parity 共20項通過，含49平移、跨64位元边界、
缺標籤、threshold colors、noise、HUD/menu/invalid；ASan Tap/Hold/Dense/invalid 均通過。
新結算 n16 total p50/p95/p99/max=31.705/46.641/47.752/48.030ms，無超100ms，
第三張 result 確認。兩次為獨立診斷，未作 R2 noise/ABBA 比較或成本資格。
新整合每配置仍25組，writer必須20 real tests；fresh22 TUs／其後原 gates 全部另驗。

main26／`f90d345` Release／Debug 各25通過，ASan19通過後 score-only Tap native2：
已正常 result／停止／無檔案／contacts0，但額外的 Down positivity 失敗。原件保存。
archive 對照亦 Down0；較慢 pixels component19 仍 Down0，試驗撤回並保留來源／負結果。
ASan 的 no-persistence 功能測試明列 instrumentation integrity、非即時資格，允許
unchanged deadline 下安全不 Down；所有已發生 Down 仍恰有 Up 或 cancellation、final0。
Release／Debug Tap／Hold positivity 保留，builder 核對 test-only instrumentation flag
與 mode，report real-time qualification false。原 runtime、pixels、profile、時限、
production9 與 R2 AA/noise/ABBA 數值 gates 不改。ASan component20 四組通過不授整體
candidate；新整合重新完整三配置及其後 stages。

main25／`1f1266c` Release 25/25 通過、Debug 21 組通過後在 score-only Dense 的
synthetic result deadline 失敗（native2／79.077s），後續 NOT_RUN，原失敗不改。
Debug Dense rendering 使 workload 落後 epoch，terminal64 slots catch-up 成 burst；
score-only 功能 fixture 將 result epoch 設 max(planned end, actual completion)，
64 slots／原 cadence／2s terminal deadline 與原 requested due times 保留。
只有 score-only fixture 啟用；diagnostic/cost producer 預設 false，正式 runtime、
archive workload、原 auditor absolute QPC 契約及 R2 gates 不變。
四項 fake-clock＋Debug Dense 組件通過（80.507s／final0／無 gameplay files）；
不能授予整體 candidate 資格，新來源仍須重新完整三配置及其後 stages。

main24／`dc20077` Release 25 組通過；Debug 在 score-only Hold 的「必有明確 Up」
斷言失敗（native 2），後續未跑。另立 diagnostic_archive 組件診斷為 11 Down／0 Up／
85 release calls。新的 cold 邊界分開累計 explicit Up、cancelled contacts、final contacts，
要求 Down = Up + cancelled 且 final=0、Tap/Hold 確有 Down。這是有限生命週期／無持久化
測試，不是 Hold tail 語義或成本資格；不將 cancellation 計入原 Up，原 normal cost 的
Down/Up 必要條件不改。Debug 組件測得 12 Down／4 Up／8 cancel／final0；新來源整包另驗。

本輪整合 object-map 診斷建構、score-only 無持久化、current capture geometry 核對與
只讀 build reader。新增 3 個無持久化測試及 1 個含 13 情境的原 byte oracle，writer
共 19 正式測試；新增 reader controls 及 literal score-only Tap／Hold／Dense／invalid
4 組，三配置各 25 冷回歸。原 22 TUs、archive workload、R2 數值 gates 不變。
完整 live enabling validator 仍須根據實際成功的 production/cost/current device 收據完成，
score-session 公開入口當前保持 UNQUALIFIED，不能用 cold PASS 或授權取代資格。

使用者最新授權繼續完成，實機測試 ready 後建立新的 GPT-6.1 Sol／xhigh local
main task，授權該 task 啟動 emulator／Computer Use 進行有限測試。本 task 完成
冷修補與原門檻驗證後交接；下列較早停止文字按當時範圍閱讀。

新候選保留完整診斷 JSON，只移轉已擁有的 arrays／scene，避免 initializer-list
深層複製。16 MiB archive mailbox、每份 snapshot 256 KiB 計費、gzip level 1
及完整事件內容不變。main14 partial gzip 經 CRC／SHA 完整讀取（360 rows，
11,461,381 decoded bytes），主要負載是完整 candidate／scene，非 ledger 重複忙迴圈。

endpoint-free 合成 producer 採用作用域 THREAD_PRIORITY_ABOVE_NORMAL（1），
normal process class，query／set／readback／restore 失敗皆取消資格；A 與 B 政策相同。
原 QPC epoch、所有嘗試、cadence、等待與 deadline overrun 判定不改。
這是待原成本 gate 驗證的環境修補，不宣稱 Windows 排程保證 deadline。

main20 的 normal Tap／Hold／Dense 已通過完整 raw integrity 與原數值 gates。
8 ms stress Tap producer 完成，auditor 因錯誤 publication 分母停止：10013 produced、
10005 published、7 physical pool drops、1 result stop 後回呼。新 auditor 明確核對
published＋pool drops＋至多一個 result stop 後 tail＝produced，兩處 pool counters
必須一致，所有 requested attempts 仍保留。normal 成本仍要求零 pool drops；
stress drops 不予成本資格。補正只影響分析，不修改三 buffer、runtime 或門檻。
原 native exit 2／STOP 保留，新結果另立 source/build/campaign 身分。

builder 將 production_entry_controls／fault_controls／pas 分成獨立 native groups，
guard held limit 64 不改；所有 native build 完成後串行封存 final executable PDB，
逐 entry 讀回 SHA，再刪除 originals，最後封存 build-binding。Compiler PDB 保留。
歷史 analyzer 單元 fixture 在隔離目錄用当前 source copies＋byte-exact Git legacy
protocol，正式 R2 protocol 不變。compiler 6 GiB 帳本只含 compiler／evaluation／
fixture／planning；全部 raw 和保留 archives 仍進入 48 GiB 總帳。新 production
狀態區分 FAILED_EXECUTION／FAILED_STAGE／NOT_RUN，失敗原件不改。

使用者授權繼續實作，**到需要啟動 emulator 就停止**。本包只執行 endpoint-free
Windows 建置、fixed pixels、R2 AA/noise/ABBA 和 literal production cold stages；
不啟動 emulator、ADB、遊戲、真 capture/RPC 或 touch，不新建 goal／worktree。

所有新來源、binary、native Job／input closure、raw 分母與失敗均按現有 gate
保存。R2 cadence、256/2304/2560、noise 公式、coverage／lateness／RSS 門檻不改，
失敗停止該批，後續 NOT_RUN；有具體修補才能建立新的 campaign，不能挑成功重測。

新的 unified CMake project freshly 編譯正式 22 TUs 及完整 production branch，
和 endpoint-free cold tests 共用同一 pas_core；pas.exe 只建置不執行。literal
manual_session 包含原 translation unit，替換的只有 capture/preflight/touch 邊界。
原 main07 三配置 compiler outputs 壓縮保存，每個 entry SHA 讀回核對及 originals
再次核對後才刪原 build roots；原始 raw、source、失敗與 receipts 保留原樣。
48 GiB retained／6 GiB candidate／12 GiB cost raw／20 GiB free 等界限不變。

完整 live reader／qualified token 必須以真實成功 production、cost 及當前
device/profile/physical 收據建立 schema；沿用交接的信任契約，不先創造可啟用
格式。本包在需要 emulator 的 profile/physical/capture/RPC 階段停下，入口保持
UNQUALIFIED，cold/comparison 的 PASS 不授予實機資格。

main10 的新 auditor 字串／JSON 比較編譯失敗保留完整 negative receipts；
main11 改成明確型別，先編譯 auditor／batch helper 再新編全部正式 core。
小型 executable 每组三個 native build，64 個 held identity gate 不變；
每組完成後將非 runtime 的 .ilk／.map 壓縮讀回 SHA 核對，再移除原 cache，
執行檔、PDB、objects、libraries 與原始證據保持可用。

main11 所沿用的 builder 未完整封存 Debug/lib、Debug/bin、protoc／plugin。
依賴清單修補會使正在收尾的 source closure 明確 STOP，native stage 不強制終止。
main11 整包不予資格；main12 在 configure 前封存已安裝 triplet 全部檔案與 toolchain，
同時覆蓋 Release／Debug library、runtime、package metadata 及 protobuf 產生工具。

main11 asan source withdrawal at native stage build-prelive-pre-emulator-asan-main11-targets-01: dependency closure incomplete; wait for natural native completion, never issue qualification.
