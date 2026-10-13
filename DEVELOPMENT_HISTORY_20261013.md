2026-10-13 現行 approach：採用完成 Chapter Legacy 20首可用IN分數收集的 **baseline／35ms／score-only**。預設原碼、launcher與建置選擇皆為baseline；最近的lazy診斷省略版保留為明選實驗，只有三首實跑，未證明穩定提升。原碼與已測版本SHA相符，已測binary保持原樣。[審查入口與完整分數證據](research/manual_score_only/review/README.md)列出選擇依據、兩版來源、檢查方式及限制。

本次為本機未提交的審查整理，沒有追加遊玩或cold readiness修補。07:10收尾核對已完成，程式保持停止；原cost NOT_READY／live UNQUALIFIED與歷史負結果保持。以下按日期保留原開發及量測記錄；[原完成紀錄](research/manual_score_only/result-20261013.json)中的lazy預設為這次改選baseline之前的決定。

2026-10-12 使用者接受既有main80並放棄實機ready全過前置，授權commit／push及新的GPT‑6.1 Sol／xhigh local main task先說明下一步。原測試負結果保持，不再繼續cold readiness修補循環。後續Chapter Legacy可用IN各一次、只記分數與一輪後停止授權保持；見[最新交接](docs/research/live-readiness-20261007/USE_EXISTING_MAIN80_NEXT_TASK_20261012.md)。以下ready前置／不建新thread等限制按歷史解讀。

2026-10-12 已依使用者指示回復main80／`3ee69ed`原碼；tracked程式及建置／驗證工具逐Git bytes／SHA核對一致，兩版Git歷史與測量證據保留。歷史Normal／AA／noise結果main80為3／3、11／12、15／18，main89為2／3、6／12、11／18；兩版ABBA36完整raw皆PASS但成本資格皆NOT_READY。見[回復與比較](docs/research/live-readiness-20261007/MAIN80_ROLLBACK_AND_MAIN89_COMPARISON_20261012.md)。本輪不重建／重測，不開emulator／新thread／ETW；回復不是新binary或實機ready資格，後續以main80原碼為基準。以下「最新」皆為歷史。

2026-10-12 最新 actual main89／`0486ed3`：必要16 TUs／13 build-control Native及motion26／Dense12／Frame8通過；同候選Normal3、AA12／noise18、ABBA36一次收齊，共51 full raw audits PASS。Normal2 PASS／Hold NOT_READY；AA6 validity PASS／6 NOT_READY，noise11／18 adequate；正式比較器及最後readonly保持NOT_READY。cost NOT_READY／live UNQUALIFIED，尚未到啟動emulator邊界。

Dense18描述性比較項目均在本輪原T內，兩個超T項目在Hold；producer排程與publication毫秒級長窗仍存在，根因UNKNOWN。原負結果／STOP／source／raw及131 Native8保存於[main89完整盤點](docs/research/live-readiness-20261007/MAIN89_ALL_COLD_COMPLETION_20261012.md)。本輪沒有追修單次33.9µs、放寬原門檻、全盤重跑原75／production9、開裝置／新thread或追加ETW；後續文件HEAD不是binary來源。以下「最新」皆為歷史。

2026-10-12 最新 AA／ABBA completion：binary 仍為 main80／`3ee69ed`，executor 為文件提交 `558fc88`。依使用者新指示「先把剩下的 aa 和 abba 測試一併跑完，再看看我們要修改甚麼」恢復診斷收集；剩AA5與fresh ABBA36皆完成完整raw audit。全AA12為11 validity PASS、原Hold3 NOT_READY1；noise15／18 adequate。ABBA36 raw PASS仍有時序負結果，原formal comparator因stopped／invalid AA而Native2／runner2／verification0拒絕，cost NOT_READY／live UNQUALIFIED。

描述性ABBA54 cells有2個Dense observer p95超原T；9 batches無paired瓶頸＋capture穩定收益。原AA Hold3同attempt2336為publication超33.9µs；ABBA另有較長排程空窗，根因UNKNOWN。優先判讀冷producer排程／publication與Dense退步，尚未新增runtime修補或重測。完整索引與範圍見[AA／ABBA完成盤點](docs/research/live-readiness-20261007/MAIN80_AA_ABBA_COMPLETION_REVIEW_20261012.md)。

本輪完成量測與盤點，原source／raw／STOP／負結果保持；不調寬原gate、R2、48GiB／minimum-free20GiB，不重跑原75／production9，不開emulator／device／CU、新thread或追加ETW。後續文件HEAD不是binary來源；較早休息及「最新」記錄皆為歷史，本次只明列已授權的AA／ABBA收集與結果判讀。

2026-10-12 最新 actual main80／`3ee69ed`：14 fresh compile occurrences、10 build/control Native、26原motion回歸、pre-cost七個必要section及current Normal3完整raw／原數值全PASS。AA第7筆Hold3單次publication deadline overrun，原validity NOT_READY而STOP：7筆完整raw、6 validity PASS、1 NOT_READY、5 NOT_RUN；noise18／ABBA36／full readonly／collection皆NOT_RUN。原source／raw／負結果／STOP、38個Native8及容量整理索引見[本輪盤點](docs/research/live-readiness-20261007/MAIN80_AA_STOP_AND_REST_20261012.md)。cost NOT_READY／live UNQUALIFIED。

使用者最新指示「完成當前 aa 測試後休息一下」：本輪已結束並整理，停止開發與測試，不自動續跑；後續需使用者恢復。實機ready目標仍保留，本輪不開emulator／device／CU、新thread或追加ETW；不重跑原75／production9、不調寬原gate。較晚文件HEAD不是本輪binary來源。以下較早「最新」／proposed及續作授權為歷史，不能覆蓋本次休息指示。

2026-10-12 最新 actual main79／`4ea686a`：14 fresh compile occurrences、10 build/control Native及26個原motion回歸PASS；pre-cost Native2／runner2／verification0因actual core rows缺`replaced_archive_sources`而REJECT narrow_build。原11個Native8與完整負結果見[main79保存](docs/research/live-readiness-20261007/MAIN79_BUILD_REGRESSION_PASS_PRE_COST_STOP_20261012.json)。Normal3／AA12／noise18／ABBA36皆NOT_RUN，cost NOT_READY／live UNQUALIFIED。

下一必要修補只補兩個core的exact motion replacement宣告與loader／既有reader12反例，見[修補契約](docs/research/live-readiness-20261007/MOTION_ARCHIVE_DECLARATION_REPAIR_20261012.md)。像素策略與原R2／全部數值不變；fresh必要14與controls後續pre-cost→Normal3→AA12／noise18→未跑ABBA36→full readonly→collection邊界，不重跑原75／production9。容量整理已完成索引，source／raw／原負結果／STOP保留；使用者離開仍完成全部可冷工作至emulator邊界，本輪不開装置、新thread或ETW。以下較早最新／proposed皆歷史。

2026-10-12 最新 actual main78／`43359f8`：14 fresh compile occurrences 的 configure／build Native、runner、verification皆0，完整held／streams／active0；首affinity control因48GiB總量50,029,647,183B＋原build reserve1.5GiB超限而NOT_LAUNCHED。不是控制或producer失敗，26 regression／Normal3／AA12／noise／ABBA皆NOT_RUN；原source／14objects／兩Native8及capacity STOP見[main78保存](docs/research/live-readiness-20261007/MAIN78_COMPILE_PASS_CAPACITY_STOP_20261012.json)。main77的Normal3／AA12 PASS、noise13／18及ABBA NOT_RUN仍是歷史結果，cost NOT_READY／live UNQUALIFIED。

後續只修正已封存build之後控制階段使用原tests1GiB reserve，48GiB／minimum-free20GiB及全部數值不變；必要新HEAD依現有來源契約fresh14，不重跑原75／production9。另沿用既有逐entry decoded SHA／原件再次核對的lossless歷史封存方式，保持原source／raw payload／負結果／STOP可恢復；回收前不授資格。使用者離開仍完成全部可冷工作至emulator邊界，本輪不開裝置、新thread或ETW recording。以下較早最新／proposed為歷史。

2026-10-12 最新 actual main77／`fce23e7`：必要10 fresh cold TUs、9 build/control Native、pre-cost必要核對、current Normal3完整raw及原數值、AA12完整raw與validity全PASS；noise13／18 adequate、5不足，ABBA NOT_RUN。原source／copies／raw／STOP與53個Native8保存於[main77結果](docs/research/live-readiness-20261007/MAIN77_NORMAL_AA_PASS_NOISE13_STOP_20261012.json)。CPU v3 partition未解決全部波動，cost NOT_READY／live UNQUALIFIED。

下一必要候選只短路`src/game_motion.cpp`純讀ridge的無用後續gray探查，像素／80與18門檻／布林策略保持；一個fresh formal motion object明列覆蓋原22-member archives，另跑26個既有相關回歸。新分支14 compile occurrences／11targets／9exe，原libs与原PASS來源不冒fresh全包；R2／noise／ABBA及全部原數值不變。見[必要motion delta契約](docs/research/live-readiness-20261007/MOTION_PROBE_DELTA_20261012.md)。使用者離開仍完成全部可冷工作至emulator邊界後停止；本輪不開裝置、新thread或追加ETW recording。以下較早最新／proposed為歷史。

2026-10-12 main68／1941cf4 同ETL reader-only一TU／40控制／3 Native8及離線Native0／wrapper0通過。最大空窗36.553ms中target Ready36.5492ms；CPU14相鄰高priority peer residence36.5474ms，邊界unknown3.8µs，無PROC/lifetime故不認定程序或根因。原controller UNKNOWN／STOP／costfalse保留。必要cold producer eligibility與actual factory收據接入正在實作，base55正式來源＋新delta明列、不冒fresh全包；noise最新6／18、ABBA0／36，cost NOT_READY。完成至emulator邊界停止，本輪不開裝置、新thread或追加recording。見[修補契約](docs/research/live-readiness-20261007/PRODUCER_ELIGIBILITY_DELTA_20261012.md)。

2026-10-12 reader-only main67／c7b2f08 一TU、原20＋schema8＋prefix12及3個Native8通過。同一ETL離線reader Native0／EOFtrue，781buffers／1,264,141callbacks、file reported-loss全0；最大36.553ms區間為Ready36.5492ms＋邊界unknown3.8µs，CPU14 switch-out state1／reason31→switch-in。原controller final statistics UNKNOWN／STOP／cost=false保留，TID lifetime與causal claim仍false。下一步只用同ETL核CPU peer／priority，無新recording／producer／auditor；noise最新6／18、ABBA0／36。目標仍完成全部可冷工作至emulator邊界，本輪不啟動裝置或新thread。详见[排程分類與保存](docs/research/live-readiness-20261007/COLD_SCHEDULER_DIAGNOSTIC_20261011.md)。

2026-10-11 actual main62／`6e3755d`冷排程r03 producer Native0／52.738s，完整raw離線auditor通過；producer／callback各2超期，lateness最大36.1862ms，cost仍false。原ETL／STOP保留。reader的版本2硬限制尚未辨識actual事件版本，最大36.553ms空窗仍UNKNOWN；新維護只建reader一TU及20＋8控制，再讀同檔，不重跑已PASS auditor或新增producer／recording。詳見[診斷與保存](docs/research/live-readiness-20261007/COLD_SCHEDULER_DIAGNOSTIC_20261011.md)。noise最新仍6／18、ABBA0／36；本輪不開emulator或新thread。

2026-10-11 冷排程診斷工具main61／`eb6ed0b`三TU／52控制及4個Native8通過；nested Job預檢通過，但Windows預設hybrid-shutdown旗標觸發STARTUP_STOP，原128KiB ETL／STOP保留，producer尚未啟動。新工具明列該旗標，未再次錄製；current Normal3／AA12／noise18／ABBA36仍待完成，cost NOT_READY。詳見[診斷與保存](docs/research/live-readiness-20261007/COLD_SCHEDULER_DIAGNOSTIC_20261011.md)。本輪不開emulator或新thread。

2026-10-11 最新 actual main57／`5dce75d`：獨立三TU／16控制及Tap／Hold／Dense各一次完整raw auditor通過；原數值皆true，Dense兩次render超期照報。這些只屬diagnostic，不能替代current Normal3或授cost／live資格。原main55正式Tap STOP保留。

compiled55／`12ec055`、execution57的獨立AA r03已排除264字元source canonical錯誤：全939 exact copies使用main內短路徑，原946 snapshot保存。首Tap producer／auditor／gate Native均0、full raw integrity通過，但producer／publication超期40／40、schedule lateness最大768.03ms，原數值gate失敗而STOP，後11筆NOT_RUN；noise／ABBA未執行。詳見[實際盤點與完整保存索引](docs/research/live-readiness-20261007/MAIN57_DIAGNOSTIC_AND_AA_STATUS_20261011.md)。

latest completed noise仍main44的6／18 adequate、ABBA0／36、cost NOT_READY／live UNQUALIFIED；沒有已支持的排程修補，原因unknown。必要validator／collector／score-only軟體及原冷控制已完成；尚須current Normal3、AA12／noise18／未跑ABBA36及完整readonly，真正裝置收據在emulator邊界後取得。本輪不開裝置或新thread，不重跑原75／production9；以下「最新」與proposed為歷史封存記錄。

2026-10-11 最新 main55／`12ec055`：必要CPU32／原584及wait43／diagnostic20／active12／QoS24、reader2／原81及兩policy各12、production52／七控制通過。Normal Tap full raw11／3,358,832B integrity通過、pool0，但一次168.9509ms active wait造成producer／callback超時7而STOP；後續Hold／Dense／AA／noise／ABBA未跑。原負結果、source、raw、STOP保留；CPU零差不是精確off-CPU證據，原因仍unknown。

下一步為獨立bounded相鄰QPC max-gap診斷，base55 libs與新compiled delta分列，不改shared wait policy、原數值或資格，不重跑原75／production9；詳見[診斷範圍](docs/research/live-readiness-20261007/PRODUCER_POLL_GAP_DIAGNOSTIC_20261011.md)。latest completed noise仍main44的6／18 adequate，ABBA0／36、cost NOT_READY。完成全部可冷工作至emulator邊界停止，本輪不開裝置或新thread；以下main55 proposed為原封存計畫。

2026-10-11 最新 main54／`35073ef`：CPU32／584、原wait43／diagnostic20／active12、reader2／原81／active-policy12、production52／七控制及runtime18來源／36副本通過。normal Tap producer／auditor Native皆0、完整raw10／3,332,367B integrity通過，原數值pool drops4、producer／callback deadline overruns65而STOP；Hold／Dense／AA／noise／ABBA未跑。原946份source、raw、STOP、兩Native8與1944 leaf／24parents／31shards見[main54負結果](docs/research/live-readiness-20261007/MAIN54_PRODUCTION_TAP_NEGATIVE_20261011.json)。原main32 PASS保留，latest completed noise仍main44的6／18 adequate，ABBA0／36。

本輪actual wait診斷：measurement2136 full active spin跨1313.0831ms／28327polls，lateness1293.2918ms，kernel calls0；前callback至wait .0003ms、afterwait至render0、render .3464ms／callback .1231ms。CPU原樣本零差的accounting granularity未知，不能據此認定精確off-CPU、OS／guard根因。actual54 Native UTC前後30秒無Kernel-Power／Power-Troubleshooter含506／507或Kernel-General clock-change1事件；不能由無事件完全排除漏記。

下一main55為實驗cold own-process HighQoS候選：以正式Get／SetProcessInformation(ProcessPowerThrottling)讀完整prior，只控制execution-speed位元並保留其他documented位元，explicit query／set／readback／exact restore；未知／失敗拒絕，析構fallback不授verified。scope影響own cold process所有worker含writer，AA A／B一致；正式capture／manual runtime與host registry／power plan不改，process NORMAL32、thread HIGHEST2、full_period_active_qpc與原workload／期限／成本界限不改。QoS分類是官方支援的候選理由，尚未證明main54原因或有效性；必要控制與成本仍待fresh Native，不冒PASS或live資格。

新QoS控制分列24項（23個fake API邊界＋1個actual Windows full-state還原），readonly另列12項policy／型別／身分／完整還原拒絕控制，原43／584／diagnostic20／active12／reader81／active-policy12分母保持。actual Windows snapshot須綁定該Native PID；AA／ABBA各producer須綁各自PID及consumer main thread。這些控制與main55成本尚待fresh Native，不能以合成資料通過代替實際量測。

依最新授權只完成必要cold開發／Normal3／AA12／noise18／未跑ABBA36及完整readonly，原75／production9不全盤重跑；retained48GiB／minimum-free20GiB與原R2保持。完成到emulator邊界停止，本轮不啟動裝置、不建立新thread。以下main53／54 proposed字樣為原歷史記錄。

2026-10-11 最新 main53／`11079f8`：CPU32／584、reader2／81、production52／七控制、runtime18來源／36副本通過；原 producer wait43 與新 diagnostic20 分列通過。normal Tap 完整 raw10／3,321,144B integrity 通過，producer／auditor Native 皆0；原數值 pool drops6、producer／callback deadline overruns149而 STOP。完整946份 source、原 raw／STOP與小收據見[main53負結果](docs/research/live-readiness-20261007/MAIN53_PRODUCTION_TAP_NEGATIVE_20261011.json)。Hold／Dense／AA／noise／ABBA未跑。

新有界 wait telemetry 定位兩次 blocking wait：warm149 設17.6978ms、Wait跨782.3458ms；measurement1996設17.5702ms、跨825.7037ms，皆 raw WAIT_OBJECT_0、無 spin；不能由 thread CPU counter零差認定精確off-CPU，也不能推定 timer signal、dispatch、sleep 或 guard 根因。只讀 power 記錄未見該輪 sleep／resume，目前 idle sleep AC／DC為Never；guard無runtime suspend，沒有逐poll交集資料。原 main32 PASS不撤銷。

下一 main54 cold synthetic producer 改為 full_period_active_qpc：整段 remaining period 以QPC／YieldProcessor主動等待，initial horizon100ms／total poll cap2^24；不建立或阻塞timer，absolute epoch／2560／20ms／warm256／measurement2304與全部原數值界限保持。增加CPU使用量尚待原 workload量測，A／B同policy，不冒稱2ms active tail；正式 capture／manual runtime、writer、優先序與affinity不改。原43／584及diagnostic20不重定義，另列active wait12與readonly policy12必要控制，目前尚待fresh Native驗證。

原75／production9不重跑；必要身分建置／控制後續 normal3 → AA12／noise18 → 原未跑ABBA36 → 完整readonly → 裝置邊界。最新完成noise仍main44的6／18 adequate、ABBA0／36。cost NOT_READY／live false；本輪不開emulator、裝置或新thread。

2026-10-11 最新 main52／`c7aad78`：CPU32／584 controls、reader2／81、production52／七個控制執行及新27項 producer placement 控制通過，既有 producer wait43 控制仍單列。normal Tap 完整 raw／原成本數值通過；Hold 完整 raw integrity 通過，但 pool drops2、producer deadline overruns115（warmup37、measurement1＋77；gap769.3928／1551.2314ms）。四個 producer／auditor Native 皆退出0；完整946份 source 快照、原 raw／STOP及小收據保留於[main52負結果](docs/research/live-readiness-20261007/MAIN52_PRODUCTION_HOLD_NEGATIVE_20261011.json)。

priority／affinity 的實際 readback／explicit restore verified，但 timing 尚未修好；根因 unknown，不能認定 OS 或 guard cause。下一 main53 僅添加 diagnostic-only 有界 timer／spin／thread CPU telemetry，不調 policy 或 gates，目前尚未 compile；新 nested diagnostic_controls 為20項，與原43／584分開，尚待 fresh Native 實際驗證。default AA 與正式 manual 不啟用此診斷，score no-gameplayfiles 契約保持。

原75／production9不重跑；必要身分建置與控制後仍依 normal3 → AA12／noise18 → ABBA36 → 完整 readonly → 裝置邊界順序。最新已完成 noise 仍為 main44 的6／18 adequate，ABBA0／36；cost NOT_READY／live false，本輪不開 emulator、装置或新thread。

2026-10-11 最新 main51／`8257eb0`：CPU32／584、reader2／81、production52／七控制、runtime18來源／36副本通過；必要normal Tap數值通過，Hold完整raw integrity通過但producer deadline overrun1，Dense／AA／noise／ABBA未跑。原四Native皆退出0；負結果與原source／raw／STOP見[main51記錄](docs/research/live-readiness-20261007/MAIN51_PRODUCTION_HOLD_NEGATIVE_20261011.json)。下一main52只在cold synthetic producer重用現有AA producer affinity，補readback／explicit restore正反控制；writer與正式manual runtime不改，原HIGHEST2／2ms tail／2560／20ms／全部數值門檻保留。現有source資格契約須fresh必要CPU32／reader2／production52與七控制，再完成normal3／AA12／noise18／未跑ABBA36／完整readonly。原75／production9不重跑；最新完成noise仍main44的6／18 adequate，ABBA0／36；完成至emulator邊界停止，不開裝置或新thread。

2026-10-11 main50 `775d5c1` CPU32/584 controls7、reader2/81、pre-cost actual source/base joins通過；production core/action/emulator/bench四groups通過，entry在24.205001s因`held-identity-cap64` STOP，runner125/native exit unknown，identityfalse/cleanupunknown，streams完整、active0及64 held全部signaled。原33小pins/full945source/native/logs/checkpoints保留於`MAIN50_PRODUCTION_ENTRY_NEGATIVE_20261011.json`；沒有compiler error結論，pipeline/affected controls/normal3/AA/noise/ABBA未跑。下一main51只修冷build runtime部署：configure時applocal OFF，以完整original installed Release/bin十八DLL pins直接file(COPY)到root/production，逐來源及兩副本SHA、actual52graph/seals/before-after核對；原六groups/52TUs/64held及R2不改。此修補不是main50 STOP轉PASS，current fresh收據與noise/ABBA仍待實際取得；不重跑原75/prod9，完成至emulator邊界停止、無新thread。

# Phigros Auto-play System

2026-10-11 最新冷修補：main49／`bffd61f` 必要CPU32及七control executions完成，保留原binary／Native／source快照；
reader／production／normal3／AA／noise／ABBA均未跑。只讀交接核對找到actual factory的
Native8 closure root.verification_output誤比process verification；正式producer實際指
input_closure.path。原actual COMPLETE metadata由舊parser拒絕，僅記憶體改此join後接受；
這是診斷，不冒稱main49 runtime失敗或新qualification。main50用既有TU共享file-only
helper修正output join，保留其餘source／seal／denominator／before-after檢查；collector
77＋8＝85必要正反控制，admission82／entry13／score-only4不改。原component及compiler
逐entry ZIP／原SHA讀回保留，舊路徑重驗須恢復exact bytes與原source。仍須fresh必要
CPU32／reader2／production52與受影響控制、normal3、AA12／noise18及未跑ABBA36，再做
完整readonly。最新完成noise仍main44的6／18 adequate、12不足；ABBA NOT_RUN。
不重跑原75／production9，不開emulator或新thread，完成到裝置邊界後停止。

2026-10-11 最新冷續作：main48／`3d4c050` CPU32／584 controls、reader2／81 controls及readonly前提核對通過；
production core／action／emulator／bench通過，但entry的collector連結LNK2019／LNK1120：
wrapper僅列五個emulator來源，漏`candidate_receipts.cpp`，原graph實際51 TUs。
原945 source snapshot、完整Native／STOP保留；normal3／AA／noise／ABBA均未跑。
main49 wrapper直接取root emulator6／bench5清單，configure後先核12個指定targets的
52個source／object／archive／link成員，再跑原六批建置及既有physical member核對。
13項graph文字正反例只是維護控制，不授新資格；仍須fresh必要build、controls、normal3及
AA／noise／未跑ABBA。兩批main48歷史資料與失敗compiler已逐entry ZIP／原SHA讀回保留，
原位置重驗須恢復exact bytes及原source；不重跑原75／production9、不開emulator或新thread。

2026-10-11 最新續作：main44 AA12通過但noise仍12欄不足；main45 fresh build觸及原held-identity-cap64，31／32 objects與原STOP已保存。下一候選拆為有界owned build批次，保留原64上限及全部來源／closure。CPU等值修補、validator及收據工具已接線，仍須fresh affected controls、新noise及未跑ABBA；完成後停在emulator邊界，不開新thread。見[本輪實作契約](docs/research/live-readiness-20261007/CPU_COMPONENT_REPAIR_20261011.md)。

main46／`7b898c4` configure通過，第一build因空prior清單產生null path而在啟動前停止，尚無新noise／ABBA。main47修正清單產生方式，保留原輸入核對；[main46負結果](docs/research/live-readiness-20261007/MAIN46_BUILD_PRELAUNCH_NEGATIVE_20261011.json)與完整source保留。

main47／`87af239` CPU32／7控制、reader2／81控制及readonly前提核對通過，但production載入遇owner source-array字串串接錯誤，尚未Native啟動。main48修正此來源清單；[main47原結果與STOP](docs/research/live-readiness-20261007/MAIN47_PRODUCTION_LOAD_NEGATIVE_20261011.json)保留，noise最新仍6／18、ABBA未跑。

2026-10-11 main32／`aa9de3c` 三配置75組、完整圖3、prefix3063、fault4及production9
integrity通過；normal三項原成本數值通過，AA12 validity亦全通過，但observer／
capture-to-owner噪聲不足，ABBA未跑。原raw／913 source snapshot／37 native完整收據保留。
依使用者最新指示，修補後不全盤重跑既有PASS，做必要新噪聲及未跑ABBA至emulator邊界，
完成後先不開新thread；見[縮小重驗範圍](docs/research/live-readiness-20261007/NOISE_REPAIR_SCOPE_20261011.md)。
main36／執行來源`505ae43`、編譯來源`42a450c`的新AA12 validity全通過，noise仍12欄不足；
ABBA未跑。main37／`156214a`有界A/B診斷通過，observer起訖core不同583/733及558/739；
main39／`f28643c`四必要TUs、priority36／affinity65／deadline43及reader34通過。
新AA12 validity全PASS，streaming Tap／Hold producer與publication超期0；noise仍12欄不足，
owner六欄通過，ABBA未跑；見[負結果](docs/research/live-readiness-20261007/MAIN39_NOISE_NEGATIVE_20261011.json)。
main40／`f40fecf`末兩core候選的前兩Tap通過，第三Tap producer／publication各1超期；
負結果與923 source snapshot保留，後九筆及noise／ABBA未跑。
main41／`7057be7` producer HIGHEST候選的AA12 validity全PASS、producer／publication全部零超期；
noise 8／18 adequate、10不足，ABBA未跑，actual reader已重核原前置／新build／AA與18公式。
main42／`96c8c2a` consumer HIGHEST候選的AA12 validity全PASS、producer／publication全部零超期；
noise仍12欄不足／owner6 adequate，actual reader核對base／delta／AA及18公式全吻合。
main43／`ba1aaa6`同923 source pins的有界A/B分階段診斷通過，measurement各744；
observer／hook仍主要成本，全部phase起訖端點一致，CPU量化未量測，不能精確推算off-CPU。
下一候選只把producer固定active tail由20ms縮至2ms，保留consumer／producer HIGHEST(2)、
NORMAL process、generic末兩core、absolute due／20ms cadence與原R2；減少整週期忙等，
實際timer／CPU及噪聲效果仍須新AA／noise與未跑ABBA驗證，成本NOT_READY／live UNQUALIFIED。
同步補只讀base/delta收據join；最新指示須完成全部啟動實機前可完成的測試／開發，
完成後停在emulator邊界、先不開新thread。正式production binary保持原來源。

2026-10-10 main31／`afaa8f3` 三配置75組／完整圖3／3063 prefix／fault4通過，production
normal Tap／Hold原raw與數值通過。normal Dense僅145張即`user_stop_aborted`，native2；
原件無Escape／console分支，原source／raw／STOP保留，後六production及AA/noise/ABBA未跑。
冷fixture改用可控keyboard／console-registration邊界，正式Escape／Ctrl+C不變；原8 invalid、
兩種真實Stop loop清理、完整2560 Dense及raw／診斷數值通過。組件不授資格，新整合仍須原
完整驗證，cost NOT_READY／live UNQUALIFIED，尚未到emulator交接。見[工作包](research/pre_emulator/PLAN.md)。


2026-10-10 冷驗證 main30／`f6548e5`：三配置75組／3063 prefix／fault4及production normal
三項、stress兩項與Debug Tap通過。Stress Dense完整archive正常完成；Debug Hold auditor
錯把中途已release後的contact重用當搶占。修補按原journal／QPC核對verified backend release，
93 controls及七份原full raw組件通過；原STOP保留，ASan後兩項與AA/noise/ABBA未跑。
新整合完整驗證尚待完成，cost NOT_READY／live UNQUALIFIED，未啟動emulator。

2026-10-09 後續測試範圍已限定：[Chapter Legacy IN 一輪、只記分數、完成後停止](docs/research/live-readiness-20261007/SINGLE_PASS_SCOPE_20261009.md)。
冷修補整合直接建置三配置的新正式來源；舊 `382d845` stress dense mailbox overrun
與未執行的 AA／ABBA 保留。新的分數模式停用遊玩持久化，仍須通過當前資格，尚未啟動 emulator。

2026-10-08 使用者已續派：先完成 AA producer／production dense 冷修補與原門檻驗證，
實機測試準備完成後交接新的 GPT-6.1 Sol／xhigh local main task，授權其 emulator／
Computer Use 有限測試。最新 recipe 見[啟動前工作包](research/pre_emulator/PLAN.md)。
修補正在驗證，尚未取得新成本／live 資格；下列停止盤點為前階段快照。

2026-10-08 最新入口為[開發盤點與本階段停止](docs/research/live-readiness-20261007/PRE_EMULATOR_STAGE_20261008.md)
及[逐項收據索引](docs/research/live-readiness-20261007/PRE_EMULATOR_STAGE_20261008.json)。
依使用者「完成當前階段後停止，盤點開發情況」，本輪已封存並停止後續開發／測試。
實際 source `23a2634` 完成 Release／Debug／ASan 各自 22 正式 TUs 新建置、**60/60 組**
affected 冷回歸、3063/3063 像素前綴與四項 literal production 故障控制。
Production normal Tap／Hold 的 raw integrity／數值 gate 通過；normal dense 因 archive
mailbox overrun 執行失敗，後六項未跑。R2 AA Tap1/2 PASS、Tap3 producer／publication
各一次 deadline overrun，後九項／noise／ABBA 未跑。**實機 NOT_READY，入口 UNQUALIFIED**；
仍有可冷修補項，完整 live validator、當前裝置／physical 與完整 IN Miss=0 未完成。

外部專案副本、證據與封包清算見[main 整合與清理帳本](docs/catalog/06-engineering/MAIN_CONSOLIDATION_20261008.md)；
main 是唯一 worktree，retained prelive 上限依[容量授權](docs/research/live-readiness-20261007/CAPACITY_APPROVAL_20261008.json)
為 **48 GiB**，收尾帳本 44.916 GiB。未啟動 emulator／ADB／真觸控，沒有自動續跑。
下列較早來源與「最新」入口保留為歷史快照；後續文件 HEAD 不代替已編譯 source SHA。

2026-10-07 最新續作見 [owner cooperative 階段交接](docs/research/live-readiness-20261007/OWNER_COOPERATIVE_STAGE_HANDOFF_20261007.md)。
候選 `45f8a5f` 三配置39組 affected 冷回歸、3063 prefix、成本工具42＋22控制通過。
新 Windows AA 前兩筆 Tap PASS；第三筆安全停止，owner seen 2181/2560、measurement 1925/2304，
producer/publication overruns 12/13，必要 gate NOT_READY；後九筆／noise／ABBA未跑。
實機入口仍 UNQUALIFIED，全曲完整 IN Miss=0 未完成。使用者最新授權下一個
GPT-6.1 Sol／xhigh task 以 Computer Use 與 emulator 自行選曲／Play／驗收，仍須通過既有 gate。
下列先前階段結果保留為歷史，不代表此次候選。

2026-10-07 Windows 實際結果見 [Windows readiness：NOT_READY](docs/research/live-readiness-20261007/README.md)。
三配置與 83 個冷 stage、3063 WIC prefix、cold CLI 15 控制通過。
[Job 身分修補 v3](docs/research/live-readiness-20261007/JOB_IDENTITY_REPAIR_20261007.md) 已通過
39＋9 個 file-only 控制、4 個 native 資格控制與27次實際追加控制，包含 error 5 的真實分支。
新 AA04 的直接 producer／auditor／analyzer 全部通過程序驗證，但首筆 Tap full phase
lateness p99=18.832ms 超過原15ms門檻，批次停止；後11筆／noise／ABBA未跑。
入口保持 UNQUALIFIED，零 emulator／觸控；完整 live validator、physical 與全曲 IN Miss=0 尚未完成。

最新雲端 readiness 候選由 [10/6 續作入口](docs/research/live-readiness-20261006/README.md) 進入。
起點為 main `e9e305e3aba13f05a786c74540419428610b2f76`，本分支只有 local commits，未 push。
本輪補強成本原始證據／campaign 身分、活動 Hold 的當前 body 與不確定釋放證據；
共享 renderer 與 tail 缺口分層揭露。軟體冷驗證不能授予 Windows 成本或實機資格，
目前仍 **NOT_READY**；產品 Chapter Legacy 全曲完整 IN Miss=0 未完成。

Windows／C++20 的即時 pixels-to-touch 研究專案。正式程式以單程序多執行緒處理最新畫面、音符／判定線追蹤、撞線預測與可撤銷多指觸控；不讀譜或遊戲內部狀態。

2026-10-06 current-rails-v1 已整合至 main 的正式離線 observer／owner／scheduler；
三配置回歸及3063幀自身接觸前綴通過。成本／physical gate仍NOT_READY，未接live CLI，
Chapter Legacy完整IN Miss=0目標未完成。最新結果由
[Windows冷準備入口](docs/research/zero-miss-20261006/windows/README.md)進入，
近期 ignored 資料與精簡 ZIP 見 [封包說明](docs/research/zero-miss-20261006/windows/PRELIVE_IGNORED_PACKAGE.md)。
分類文件見[文件總目錄](docs/README.md)。下列較早驗收及2026-10-04狀態保留為歷史快照。

前一包派送撤回後的[檔案驗收與診斷修補](docs/research/zero-miss-20261006/windows/WITHDRAWAL_ACCEPTANCE.md)保留原件與 STOP。使用者已確認一筆 Hold head前緣語義，當時三配置獨立核對256幀決策及原圖取樣；該標註不等於合法觸點或遊戲採納。

**最新 [10/6 冷準備結果](docs/research/zero-miss-20261006/windows/PRELIVE_RESULTS.md)：三配置113／56／113／68／186項回歸通過，完整3063幀自身receipt有效；原v3的36fail原件保留，本包未重跑／改原oracle。A/A在第5筆Hold無動作時停止，ABBA未跑；有限負載／fault／10000次stress有完整負結果，成本未取得資格。沒有裝置操作或實機採納，產品Goal未完成。**

2026-10-05 [首輪雲端多分支 zero-miss 研究](docs/research/zero-miss-20261005/README.md)完成程式、逐曲總帳與建置盤點，以及隔離 C++ 冷實驗；當時原 frozen suite 尚缺輸入。

[補件後第二輪](docs/research/zero-miss-20261005/round2/README.md)首次完整執行356 layer-cases／3938 assertions，發現54fail並分類；C36h M77圖及Windows失敗收據已覆核。

**10/5 [第四階段冷候選v3與交接](docs/research/zero-miss-20261005/round4/README.md)：原3938斷言43→36fail、無新增失敗；stationary／exact alias／斜交contact及正反例完成三配置。當時雲端委託停在真圖／Windows必要門檻；本機接手另見10/6入口，產品Goal未完成。**

**[第三階段冷測修補](docs/research/zero-miss-20261005/round3/README.md)：新隔離候選修正rails／端部與merge資格，原同oracle54→43fail、無新增原suite失敗；新48-case契約339斷言全過，獨立35-case仍保留duplicate ROI既存2fail。** 程式在[research/bvi_cold_v2](research/bvi_cold_v2/README.md)，尚未整合正式鏈，未進live。Windows、成本、真圖及全曲分母／解鎖仍有門檻。

## 歷史狀態（2026-10-04）

產品目標為 **Chapter Legacy 所有曲目解鎖 IN，完整 IN 結算 Miss=0**；P/G/B與分數照樣報告，HD為解鎖及回歸階段，沒有AP前置。最新逐曲證據、研究狀態與順序工作包見[10/4基準總帳](docs/catalog/01-project/LEGACY_IN_ZERO_MISS_BASELINE_20261004.md)及[配套JSON](docs/catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json)。現行章節分母未核，`chapter_listing_verified=false`；目前各曲解鎖狀態unknown，不能由HD分數代推。

main HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`；整合沿革見[整合交接](docs/catalog/06-engineering/MAIN_INTEGRATION_HANDOFF_20261001.md)。**C36h tint1為behavioural／experimental baseline；main50 observer50／planner27／diagnostics11是comparison／donor、live0**。同號歷史版本依binary/source SHA分讀。[10/1狀態](docs/catalog/01-project/PROJECT_STATUS_NEXT_STEPS_20261001.md) A–I及早期「最新」是歷史，J5–J32銜接後續。

- 整合 Release 回歸 349 項：348 通過、1 opt-in 跳過、0 失敗；離線 CPU 工具 8/8。這是軟體驗證，不是遊戲效果驗收。
- 恢復分支 C36h tint1 的 Dlyrotz IN13 為 795950／P496-G11-B0-M77，前次全錄亦 M77；使用者回報主要問題未改善。C36h 同號 37/19 與歷史 main 37/19 不是同一 binary。
- 保存全錄7722張PNG、12段選取與理由；pixel human gold=0。X1完整fixed-pixels owner／scheduler／FakeTouch接觸重播及R1/R2於10/2已獨立驗收；固定pixels不包含新觸控的遊戲feedback。
- 模型僅供離線輔助標記及主程式優化。C++ LibTorch CPU 小試已有權重與 18 張原生 ROI proposal；不進即時迴圈，不以合成 IoU 宣稱真實準確率。
- X10b standalone suppression已否決，suppression OFF；X10d-P單一pending-missing hook冷契約已獨立通過，14組機會損失利弊unknown。X11-P/R1/R2/R3工程與負結果已簽收、成本仍not-ready，X12仍conditional；X10d-O尚未開始。
- 本包只整理證據與入口；接續順序為A獨立驗收收尾→B量測契約審查→X10d-O→conditional候選live／全曲IN。未啟動emulator／觸控／模型／goal，未追加runtime、cost、stress或full replay。

歷史跨曲兩版各 19 首 HD 的 Miss 為 815→818，非 Perfect 957→860；沒有 AP 或未知曲泛化驗收。9/29 的 D1–D4／C0–C6 是已保存的冷工程成果，其後實戰與整合結果依日期分開閱讀。

[10/1整理帳本](docs/catalog/06-engineering/CLEANUP_AUDIT_20261001.md)保留當時範圍；[9/28清理紀錄](docs/catalog/06-engineering/CLEANUP_AUDIT_20260928.md)列不可重算資料。10/4現查：舊recovery目錄未registered，其LibTorch及CPU runtime路徑不存在，main CPU cache仍指舊路徑；不宣稱當前CPU可build，也未重裝。原frozen引用與raw不回寫。

## 建置與回歸

需要 Windows x64、VS2026 MSVC v145、Windows SDK、CMake 3.28+ 及 vcpkg。依賴由 vcpkg.json／vcpkg-configuration.json 與 third_party overlay 固定；不要替換已修補的 gRPC 1.81.1。

```powershell
$env:VCPKG_ROOT='C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg'
$env:VCPKG_MAX_CONCURRENCY='4'
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

Debug／ASan 對應 windows-debug／windows-asan。presets 含本機 VS instance 與 sanitizer 路徑，換機須核對。第三方 DLL 未全面 ASan 插樁；效能只用 Release。

初次安裝的 gRPC 深路徑可能超過 Windows 限制；本機 C:/pas-bld-9408 與 C:/pas-pkg-9408 為 out/vcpkg_installed/vcpkg 下 blds／pkgs 的短路徑 junction。清理後可重建空 staging 目錄；換機須設定自己的短路徑。已安裝 x64-windows 依賴保留。

## 手動實戰操作參考

2026-10-12 既有 main80 試跑新增 `manual-session --score-only`：使用原手動策略，
由使用者選曲及按 Play；`--rounds 1..20` 設定結算首數，無預覽／clips／錄影／journal／summary／結果圖，
保留當前裝置能力指紋、即時 HUD 門控與停止釋放。此入口不要求 cold admission，
不改寫原 cost NOT_READY／live UNQUALIFIED。執行檔為 main80 保存元件加兩個入口 TU 的新連結，
不是原 main80 executable 的相同 bytes。
本機已還原的 main80 與入口建置分別在 `out/prelive-producer-eligibility-main80-e1`、
`out/manual-score-main80-20rounds-20261012-r2`；這是預設launcher使用的已測入口。
`research/manual_score_only/build.ps1` 預設重建baseline到新的 output root，`-Variant lazy` 可重建保留實驗；
重建產生新artifact，不替換或冒稱原已測binary。腳本依賴本機保存的main80 libraries／objects／toolchain。
模擬器需用 `-avd phigros -grpc 8554 -grpc-use-token` 啟動；程式待命命令為
`& ./research/manual_score_only/standby.ps1`，預設連續20首；每首結算後釋放觸控並回到待命，
由使用者選下一首及按 Play，第20首後停止。單曲 watchdog 仍為10分鐘，曲間待命30分鐘，
整段 session 上限5小時；曲間待命從每首結束重新計時。Escape／Ctrl+C、故障或中止仍立即停止。
2026-10-13本輪先前獲授Computer Use續測、補測與有限調參，已完成並於07:10核對停止；
這次只整理現行approach與審查資料。
診斷省略版另在 `out/manual-score-main80-lazy-20261013`，只在 score-only 模式跳過會被丟棄的
每幀／receipt／plan 診斷 JSON 建立，原辨識／planner／scheduler／觸控及原 archive 實作沿用；
16項軟體檢查通過，包含 none 不呼叫 factory／不產檔、close 後拒絕及啟用診斷時輸出一致。
`standby.ps1` 預設使用baseline、35ms及20首；`-Variant lazy` 明選診斷省略版。
一般CMake的manual session亦為baseline；lazy原碼保留於 `research/manual_score_only/variants/lazy`。
新版是省略無用工作，三首分數不構成穩定提升證明。
`-LeadMs 35`／`40` 只選既有 lead profile，不改其他門檻。

以下保留main50歷史操作參考，它目前是comparison／donor、沒有live-ready資格。preset建置輸出至`out/release-v145`，不能把殘留binary或此命令當作候選升格／本輪啟動授權。

```powershell
out/main-integration-v145/Release/pas.exe manual-session `
  --config configs/phigros-hd-assist-five-lead35.json `
  --capability measurements/game-semantics-20260927/touch-five/summary.json --no-preview
```

啟動後由使用者選曲並按 Play；程式僅由即時 HUD 接手。結算確認後釋放觸控、保存同源結算圖並回待命，不自動選曲／重試。Escape 或 Ctrl+C 停止。未知畫面不取得觸控資格；結算辨識限已核對的 1280×720 英文布局。

--pixel-clips 需使用從當前 source 重建的執行檔，或保留的第二輪 main-legacy-v145 binary；它啟用既有有限三幀診斷：最多 20 輪、每輪 30 張，writer mailbox 4 張；不是新研究的兩秒採樣器。--round-watchdog-s 預設 0，超時記 FAULT／aborted，不當成曲尾。這些命令是操作說明，不是自動啟動遊戲的授權。

熱調參分支另用 `configs/phigros-hd-assist-five-lead40.json`：manual-session 接受 30–45 ms lead，owner 依 profile 設定並在 manifest 記有效值。40 ms 四首已重測，分數皆小幅上升但 Miss 仍多。observer48／planner25 的後續手動試驗由使用者停止、無結算，不作效果證據。observer49／planner26 針對原線短暫消失補有界線位移投影；2026-09-30 的手動實測完成 Glaciaxion HD6、Dlyrotz HD9 兩首後依使用者要求停止，結果見[熱調試紀錄](docs/catalog/02-game-results/FOUR_SONG_HOT_TUNING_20260929.md)。

歷史比較版本請保留原 binary，不在當前 checkout 重建覆寫：
- 舊版：out/hd9-session-release/Release/pas.exe。
- 第二輪：out/main-legacy-v145/Release/pas.exe。
- 目前 out/release-v145 與第二輪 binary hash 不同，不能互當完全相同版本。

## 文件入口

| 文件 | 用途 |
|---|---|
| [10/4 IN zero miss總帳](docs/catalog/01-project/LEGACY_IN_ZERO_MISS_BASELINE_20261004.md)／[JSON](docs/catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json) | 最新入口、逐曲證據、基準角色及順序工作包 |
| [10/1狀態](docs/catalog/01-project/PROJECT_STATUS_NEXT_STEPS_20261001.md) | 歷史因果鏈；J5–J32接續最新狀態 |
| [整合交接](docs/catalog/06-engineering/MAIN_INTEGRATION_HANDOFF_20261001.md) | 合併來源、frozen 配套、依賴與整合測試 |
| [架構](docs/catalog/01-project/ARCHITECTURE.md)／[路線圖](docs/catalog/01-project/ROADMAP.md) | 現行契約與後续優先順序 |
| [逐幀與 C36h 結果](docs/catalog/02-game-results/DLYROTZ_FRAME_ANALYSIS_C36H_20261001.md) | 使用者 12 段理由、原圖／事件及未改善結果 |
| [全錄契約](docs/catalog/06-engineering/FULL_ROUND_RECORDING_20261001.md)／[CPU 小試](docs/catalog/07-data-learning/OFFLINE_CPU_VISION_PILOT_20261001.md) | 已有離線工具與限制 |
| [跨曲研究](docs/catalog/01-project/MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md) | 原始 M0–M6 設計；即時模型部分已退出現行範圍 |
| [冷開發結果](docs/catalog/04-offline-research/COLD_DEVELOPMENT_RESULT_20260929.md)／[合併審查](docs/catalog/04-offline-research/COLD_DEVELOPMENT_MERGE_REVIEW_20260929.md) | 已有組合庫、fake-clock 與性能證據 |
| [退步審查](docs/catalog/02-game-results/HOT_REGRESSION_REVIEW_20260930.md)／[恢復計畫](docs/catalog/02-game-results/BASELINE_RECOVERY_HOT_DEVELOPMENT_PLAN_20260930.md) | 歷史實戰、基線選擇與未證假說 |
| [舊版結果](docs/catalog/02-game-results/HD9_LEGACY_HD_RESULTS_20260928.md)／[新版結果](docs/catalog/02-game-results/MAIN_LEGACY_HD_COMPARISON_20260928.md) | 兩輪跨曲原始證據與限制 |
| [第三方授權](docs/catalog/08-licenses/THIRD_PARTY_NOTICES.md) | 固定依賴及授權原文 |
| [本次清理](docs/catalog/06-engineering/CLEANUP_AUDIT_20261001.md)／[9/28 清理](docs/catalog/06-engineering/CLEANUP_AUDIT_20260928.md) | 容量、保護與已刪範圍 |

src／include為正式核心；apps為CLI；tests／fixtures為回歸及Android C++ Fixture；configs為profile。measurements／out不在Git追蹤內，Git提交不等於備份原始證據。後續有限live依既有授權、gate與總控續派；本整理包不啟動。
