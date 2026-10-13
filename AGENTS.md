2026-10-13 最新使用者決定：以完成完整本輪Chapter Legacy 20首可用IN分數收集的版本，作為專案目前approach，整理成本機可審查狀態並保留最近版。現行預設為baseline／35ms／score-only／20首；src/manual_session.cpp已精確回復到該入口建置source SHA，launcher／build及一般CMake皆採baseline。最近lazy原碼保存於research/manual_score_only/variants/lazy/manual_session.cpp，helper／16項檢查、三首結果及原binary均保留，須明選，不授完整20首測試或穩定升分資格。兩個原binary與所有原結果保持；四组只含允許欄位的分數及原建置身分有exact SHA審查副本。審查入口見[現行基準與審查說明](research/manual_score_only/review/README.md)，原result-20261013.json維持當時lazy預設的歷史決定。這次只做原碼／預設／文件／證據整理與唯讀核對，不追加遊玩、cold readiness或新的資格；07:10停止核對已完成，原cost NOT_READY／live UNQUALIFIED／負結果／STOP保持。以下10-13完成段落的lazy預設與其餘較早「最新」皆按歷史解讀。

2026-10-13 本輪已完成並停止：使用者先自行選曲／Play，後明確重新授權Computer Use續測、未記錄曲目重打及有限調參，另要求開發最晚至07:10台北時間並設提醒。Chapter Legacy 20首可用IN本輪分數已記齊；前五首補測，光由當前結算畫面讀取，沒有用歷史最高分代替。只保存曲名／IN等級／分數／簡短狀態，沒有保存遊玩截圖、影片、逐幀、touch、trace或journal。原入口lead35 A/A、原入口lead40三首、新診斷省略版lead35三首分列，不挑最好結果覆蓋主表，未證明穩定分數提升。新增manual-session --score-only的1..20首入口及省略不用的診斷JSON，當時預設launcher為lazy／35ms／20首，baseline仍可選；新binary僅fresh入口兩TU加原main80元件，不是原main80 exe exact bytes，也不授新的cost／live資格。92項budget、5項入口拒絕、既有lead deadline釋放控制與16項診斷省略軟體檢查通過；新版三首完成後Native0停止。詳細來源、負結果與分數索引見[本輪整理](research/manual_score_only/result-20261013.json)。07:10一次heartbeat提醒id07-10保留，只確認已完成的收尾，不重新開發或遊玩；未獲新續作指示不自動再跑。原cold負結果／STOP／R2及cost NOT_READY／live UNQUALIFIED保持，不恢復cold readiness循環。

2026-10-12 最新使用者決定：「放棄實機 ready 門檻，使用既有版本，並 commit & push，派送新的 6.1 sol xhigh 告訴我下一步要做甚麼」。使用main80／`3ee69ed`已回復原碼，停止以cold readiness全過為後續使用的前置工作；原cost／live／STOP／負結果仍照報，沒有更改原R2數值或將歷史REJECT冒PASS。這次已授權commit／push及新的GPT‑6.1 Sol／xhigh local main task；新task首輪先盤點既有binary／入口並向使用者說明最短下一步，不自行恢復cold修補循環。既有emulator／CU、Chapter Legacy所有可用IN各一次、只記分數、一輪後停止的後續授權保持。完整最新範圍見[既有版本與新task交接](docs/research/live-readiness-20261007/USE_EXISTING_MAIN80_NEXT_TASK_20261012.md)。本段覆蓋以下舊ready前置與不建立新thread的歷史限制。

2026-10-12 最新使用者指示「那還是回到舊版吧，上一版結果跟這版比較」已完成：main的tracked程式／建置驗證工具回復至main80／`3ee69ed` exact bytes；12個既有files恢復、15個新增實驗code files移出現行原碼，Git歷史／source snapshots／兩版raw與STOP全部保留。沒有reset歷史、重建binary或重跑量測；rollback文件HEAD不是原binary來源。後續以main80原碼為基準，不把main89 binding直接套入現行工具。

歷史同分母比較：main80 Normal3／3、AA11／12、noise15／18；main89為2／3、6／12、11／18。兩版ABBA36 full raw皆PASS，描述性必要負結果11／36對16／36；兩版formal comparator皆拒絕、cost NOT_READY／live UNQUALIFIED。不同量測批次與各自AA T，不能宣稱回復即ready或改動致退步。見[回復與比較](docs/research/live-readiness-20261007/MAIN80_ROLLBACK_AND_MAIN89_COMPARISON_20261012.md)。原R2／gate／48GiB／minimum-free20GiB不變，不重跑原75／production9、不開emulator／device／CU、新thread或追加ETW。以下main89與其他「最新」皆為歷史。

2026-10-12 最新 actual main89／`0486ed3`：metadata cold delta與Dense白色元件提前skip已整合；必要16 TUs／13 build-control Native、motion26／Dense12／Frame8及pre-cost必要sections通過。依使用者「一次測完全部」完成Normal3、AA12／noise18與ABBA36：共51 full raw audits PASS；Normal Tap／Dense數值PASS、Hold NOT_READY；AA6 validity PASS／6 NOT_READY；noise11／18 adequate。原comparator Native2／runner2／verification0拒絕stopped／invalid AA，cost NOT_READY／live UNQUALIFIED。

本輪描述性54 cells有2個Hold超原T、Dense18 cells全在本輪T內；這不是跨版本因果改善或正式比較資格。measurement的AA lateness265.5677ms、ABBA lateness379.6099ms及Dense同attempt publication span95.9341ms仍存在，根因UNKNOWN；33.9µs未單獨追修或加tolerance。full-r2 readonly原必要七sections通過、整體NOT_READY；正式root的NOT_RUN欄與獨立diagnostic completion分列，不冒稱48筆沒跑。完整current131 Native8與source／raw／STOP／容量restore索引見[main89完整盤點](docs/research/live-readiness-20261007/MAIN89_ALL_COLD_COMPLETION_20261012.md)。

本輪一次完整測量與盤點已完成，實機ready目標未達；producer ideal-processor只為未實作／未測的草案。原負結果與STOP保持，不放寬R2／gate／48GiB／minimum-free20GiB，不全盤重跑原75／production9；不開emulator／device／CU、新thread或追加ETW。後續文件HEAD不是binary來源。以下較早最新與休息文字都是歷史，不能覆蓋使用者續作與本輪全部收集授權。

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

2026-10-11 最新 main51／`8257eb0`：CPU32／584、reader2／81、production52／七控制及 runtime18來源／36副本通過；必要 normal Tap 數值通過，Hold 完整 raw integrity 通過但一次 producer deadline overrun，Dense／AA／noise／ABBA NOT_RUN。原四 Native 都退出0，不是程序失敗；完整原件與STOP索引見[main51負結果](docs/research/live-readiness-20261007/MAIN51_PRODUCTION_HOLD_NEGATIVE_20261011.json)。Hold attempt783 start late20.6133ms，render0.6609ms／callback0.5246ms；未有wait或core邊界，不能判定根因。下一main52只在cold synthetic producer重用AA的有界OS producer affinity，保留writer／正式manual runtime／HIGHEST2／2ms tail／2560／20ms／原門檻，補正常、取消、例外與restore失敗控制。沿現有source身份fresh必要32／reader2／production52，原75／prod9不重跑。最新完成noise仍main44的6／18adequate，ABBA0／36。完成到emulator邊界停止；不開新thread或裝置。

2026-10-11 main50 `775d5c1` CPU32/584 controls7、reader2/81、pre-cost actual source/base joins通過；production core/action/emulator/bench四groups通過，entry在24.205001s因`held-identity-cap64` STOP，runner125/native exit unknown，identityfalse/cleanupunknown，streams完整、active0及64 held全部signaled。原33小pins/full945source/native/logs/checkpoints保留於`MAIN50_PRODUCTION_ENTRY_NEGATIVE_20261011.json`；沒有compiler error結論，pipeline/affected controls/normal3/AA/noise/ABBA未跑。下一main51只修冷build runtime部署：configure時applocal OFF，以完整original installed Release/bin十八DLL pins直接file(COPY)到root/production，逐來源及兩副本SHA、actual52graph/seals/before-after核對；原六groups/52TUs/64held及R2不改。此修補不是main50 STOP轉PASS，current fresh收據與noise/ABBA仍待實際取得；不重跑原75/prod9，完成至emulator邊界停止、無新thread。

# 專案工作準則

開始工作先讀 [README.md](README.md)、[架構](docs/catalog/01-project/ARCHITECTURE.md)、[路線圖](docs/catalog/01-project/ROADMAP.md) 與 [主程式架構與跨曲學習研究](docs/catalog/01-project/MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)。

## 現行方向

2026-10-11 最新：main49／`bffd61f` 必要CPU32及七control executions完成，保留原binary／Native／source快照；
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


2026-10-11 最新：main48／`3d4c050` CPU32／584 controls、reader2／81 controls及readonly前提核對通過；
production core／action／emulator／bench通過，但entry的collector連結LNK2019／LNK1120：
wrapper僅列五個emulator來源，漏`candidate_receipts.cpp`，原graph實際51 TUs。
原945 source snapshot、完整Native／STOP保留；normal3／AA／noise／ABBA均未跑。
main49 wrapper直接取root emulator6／bench5清單，configure後先核12個指定targets的
52個source／object／archive／link成員，再跑原六批建置及既有physical member核對。
13項graph文字正反例只是維護控制，不授新資格；仍須fresh必要build、controls、normal3及
AA／noise／未跑ABBA。兩批main48歷史資料與失敗compiler已逐entry ZIP／原SHA讀回保留，
原位置重驗須恢復exact bytes及原source；不重跑原75／production9、不開emulator或新thread。


2026-10-11 使用者最新指示：修復後不全盤重跑既有通過測試，但仍完成實機啟動前所有
必要開發及尚未跑的noise／ABBA。完成至需要emulator的邊界後停止，先不要建立新thread。
詳見[縮小重驗授權](docs/research/live-readiness-20261007/NOISE_REPAIR_SCOPE_20261011.md)。
main32／`aa9de3c` 三配置75組／圖3／prefix3063／fault4／production9通過，AA12 validity
通過但noise不足、ABBA NOT_RUN；原source／raw／STOP及37個native完整索引保留。
main44／`79f8f37` AA12 validity通過、noise6／18 adequate、ABBA NOT_RUN，負結果保留。
main45／`a15986f` 單一fresh build觸及原held-identity-cap64而STOP，31／32 target objects
保存、held／streams完成、active0；AA／noise／ABBA皆NOT_RUN。下一候選以三個CPU／
六個production owned build批次保留原64上限，每批核對actual Native8及來源／closure。
main46／`7b898c4` configure及其before／after closure通過，但第一build在InputClosure
處因空prior receipts的`.path`產生null而PRELAUNCH STOP，沒有native build或target objects。
完整945 source copies及原負結果見[main46記錄](docs/research/live-readiness-20261007/MAIN46_BUILD_PRELAUNCH_NEGATIVE_20261011.json)。
main47／`87af239` CPU32三批及7組控制、reader2／81控制通過；readonly核對原前提與
current CPU收據通過，但AA／noise／ABBA仍NOT_RUN。production載入在Native前因
兩組owner control來源陣列被PS字串串接成單一路徑而STOP，見[main47記錄](docs/research/live-readiness-20261007/MAIN47_PRODUCTION_LOAD_NEGATIVE_20261011.json)。
main48修正source array明確括號及逐來源manifest lookup；隔離只讀loader完成只屬診斷，
current CPU branch不可冒稱maintenance-only舊binary，仍明列fresh／compiled身分與原門檻。
下一候選依[CPU等值修補](docs/research/live-readiness-20261007/CPU_COMPONENT_REPAIR_20261011.md)
fresh core22／bridge2與必要控制，三個未改動Release libs維持原pins；正式入口／收據工具
亦fresh建置並跑必要normal3，不全盤重跑原75／production9。明列base／delta及既有PASS來源，
不冒稱新三配置fresh、whole qualification或live admission。裝置授權沿用，這輪不操作裝置。

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


2026-10-10 main30／`f6548e5` 三配置各25、完整圖三份、3063 prefix、fault4通過；
production normal三項完整raw及原數值、stress Tap／Dense與Debug Tap integrity通過。
stress Dense producer0／123.951s，未重現mailbox overrun；stress normal cost false照報。
Debug Hold producer0，auditor2「Down is unique and cannot steal active contact」：原raw
五次Down各有中途成功backend release，原auditor只在最後處理release而誤判contact重用。
原907 formal＋方向／guard共911份SHA snapshot、原raw／STOP保留於
`out/prelive-pre-emulator-planning/main30-stage-inventory.json`；ASan後兩項與AA/noise/ABBA
NOT_RUN，cost NOT_READY／live UNQUALIFIED。新只讀join按原journal順序及QPC核對release，
只接受own active IDs／dispatched／無failed或unknown，完成後才可由不同intent重用contact；
原intent Down不復活，release不計explicit Up、game effect仍unknown。原67＋新26共93控制
及七份原full raw組件核對通過，不授整體資格。新整合fresh全包，runtime／archive／R2不改。

2026-10-10 main29／`208ad7b` 三配置各25、完整圖三份及3063 prefix通過；RPC audit
程序收據因未驗image的PID9008 error5而STOP（native0、runner125、identity false）。
原source／raw／STOP保存，slow／production9／AA12／noise／ABBA36 NOT_RUN。
新guard首50ms sleep1ms後原25ms，立即resume取樣；原image／64 held／8 snapshots／
50ms shared drain／deadline gates不改。39＋9＋12 controls、native4及short RPC24
組件診斷通過，新gate main06；新整合須fresh全包，未到實測ready。其餘一輪IN分數
授權沿用，不因修補另加門檻、不啟動emulator、不以組件PASS當整體資格。以下main28為歷史。

2026-10-10 冷續作：先讀[一輪分數授權與最新修補](docs/research/live-readiness-20261007/SINGLE_PASS_SCOPE_20261009.md)。
main28 source `62a8000` 三配置各25、3063 prefix、fault4、production normal三項通過；
stress Tap producer0而auditor2（原Hold-only取消契約遇六次Tap），後五项與AA/noise/ABBA未跑。
原905 source／raw／STOP及graph collector／ASan reader watchdog負結果保留；新Tap取消
契約的67控制及原full raw組件核對通過，但不授整體資格。新整合須fresh三配置及其後原gates。
graph以同source sealed Release reader核對各配置完整收據圖、明列executor配置；不發admission。
本task仍只冷開發，完成後交新的6.1 Sol/xhigh local main task。該task獲授emulator/CU/
選曲Play，以當前遊戲畫面盤點Chapter Legacy所有可用IN，各一次、只保存分數表，未解鎖記狀態；
不錄屏或持久化其他遊玩證據。一輪整理後停止休息，不補HD/解鎖/AP/Miss0/高分或額外重試。

2026-10-09 使用者明確指定後續新 GPT-6.1 Sol／xhigh local main task：授權開啟
emulator／Computer Use，自行盤點 Chapter Legacy 現行曲目，所有可用 IN 各測一次。
完成一輪後整理分數並停止休息；IN 未解鎖／不可用就註記，不加 HD 解鎖或重試。
只保存曲名、IN 等級、分數與簡短狀態；不錄屏、不保存遊玩截圖、PNG、pixel clips、
逐 frame／touch／trace 或其他遊玩紀錄。CU 導航／讀分數可用瞬時畫面，不另存證據。
歷史及冷驗證 raw 保留，cold archive workload／原 gate 不變。詳見
[本輪授權與修補](docs/research/live-readiness-20261007/SINGLE_PASS_SCOPE_20261009.md)。

2026-10-08 使用者已續派：完成冷修補與實機啟動前準備後，建立新的 GPT-6.1 Sol／xhigh
local main task，並交接其開啟 emulator／Computer Use、自行選曲／Play 進行有限測試的授權。
先處理 AA producer deadline 與 production dense archive overrun；不把授權當資格或提高門檻。
本 task 先冷開發；實際 emulator／device／physical 驗證交新 task，只有取得相應收據才能觸控。
以下停止指示保留為前階段歷史，最新續派已解除停止。

2026-10-08 最新使用者指示：「完成當前階段後停止，盤點開發情況」。本輪已封存並
停止開發／測試，未啟動 emulator；先讀[階段盤點](docs/research/live-readiness-20261007/PRE_EMULATOR_STAGE_20261008.md)
及[收據 JSON](docs/research/live-readiness-20261007/PRE_EMULATOR_STAGE_20261008.json)。
實際編譯 source `23a2634`，三配置各 22 正式 TUs／60 組冷回歸、3063 prefix、四項故障控制通過。
Production normal Tap／Hold 通過，dense archive mailbox overrun 執行失敗；AA Tap3
producer／publication 各一次 deadline overrun，後九筆／noise／ABBA 及 production 後六項未跑。
成本 NOT_READY、live UNQUALIFIED、完整 validator 未實作；不把較晚文件 HEAD 當作 binary
來源。尚有可冷修補工作，未到只差 emulator 的階段；本次不建立自動續跑。後續續作依新指示。

2026-10-08 使用者最新授權：將 Desktop、Downloads 與可讀 C 槽內的外部專案副本、
證據及封包集中回 main，核對保存後刪除外部位置；後續直接在 main 開發，不再新建
worktree。retained prelive 上限提高為 48 GiB，minimum free 20 GiB 及既有 stage、
candidate、raw、metadata 界限保留。見[整合帳本](docs/catalog/06-engineering/MAIN_CONSOLIDATION_20261008.md)。
保留歷史 source／binary SHA、原始負結果、STOP 與搬移索引；搬移不授予新 main 資格。
既有 emulator／Computer Use 自行選曲及 Play 授權沿用，但須先通過現行 build／cost／
admission／profile／physical gates。模型訓練、遠端 push 與新長期 goal 未授權。
模擬器、SDK 與已安裝 AVD／遊戲資料是後續開發依賴，保留原位置與 profile 指紋。
以下較早 evidence-only／cloud-only 範圍為當時階段記錄，不能覆蓋最新使用者授權。

2026-10-06 雲端續作入口：[readiness 工作](docs/research/live-readiness-20261006/README.md)。
指定起點 e9e305e，ignored compact 與直接父提交 f07dc40 的來源映射已核；不要回套舊 v3 patch。
本輪只雲端實作／必要離線驗證／local commits，未 push、未操作裝置；Windows 成本、physical
與全曲 IN Goal 分開保持 NOT_READY。先讀最新 RESULTS／READINESS／WINDOWS_HANDOFF，再讀
下列歷史方向；保留原 oracle、所有負結果與 STOP。新 candidate 必須以完整 source/build/profile/
evidence 身分銜接，不能用原已簽收 binary 或 donor CLI 冒稱新候選 live-ready。

2026-10-04 最新入口為[Chapter Legacy IN zero miss總帳](docs/catalog/01-project/LEGACY_IN_ZERO_MISS_BASELINE_20261004.md)與[逐曲JSON](docs/catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json)。產品目標是所有曲目解鎖IN並以完整IN結算Miss=0驗收，HD為解鎖／回歸，P/G/B照報、不增AP前置。現行章節分母及各曲目前解鎖unknown；不能由HD分數代推。main HEAD f83c7ea、50/27/11/live0是comparison／donor，C36h tint1仍behavioural／experimental baseline。X1已獨立验收；X10d-P冷契約通過但X11/R1/R2/R3成本not-ready，suppression OFF、X10d-O未開始。順序工作包見總帳，當前包只整理證據與入口，不啟動emulator／觸控／manual-session／模型、runtime/cost/stress/full replay或長期goal。較早授權依當時範圍閱讀，後續live仍需既有gate及總控續派。

2026-10-01歷史整合commit `9fea67e248411f7c2309f220daf989f652b27d15`。C36h同號observer37/planner19與歷史main不同binary，主要Miss未改善。閱讀[狀態J5–J32](docs/catalog/01-project/PROJECT_STATUS_NEXT_STEPS_20261001.md)、[歷史整理帳本](docs/catalog/06-engineering/CLEANUP_AUDIT_20261001.md)及[整合交接](docs/catalog/06-engineering/MAIN_INTEGRATION_HANDOFF_20261001.md)；A–I及早期「最新」字樣為歷史。模型只離線。10/4舊recovery LibTorch／CPU runtime路徑已不存在，cache引用是歷史，不默認可build或重裝。

2026-09-28 使用者指定跨曲學習研究為後續主線。先做 M0 證據／標註小試與線 ID 異常研究，再依 M1–M6 逐階段推進。舊 Glaciaxion HD AP → IN、獨立大型 Fixture 前置階段與五擷取重選均已退出現行順序。研究提案不等於已實作或已驗收；選定方向不代表自動恢復舊 AP goal、無限實戰或模型訓練。

同日使用者補充[判定線形式](docs/catalog/01-project/判定線形式.md)：先以線局部座標、相對運動與明確策略研究多線、斜向、旋轉、線追 Note、突現及過線往返；不得因形式複雜就預設需要學習。學習只對已確認的觀測／關聯缺口比較，M3／M4可不做；詳見研究 §2.7。幾何過線不等於 note 已完成，返回不代表可重播已執行／未知 Down。

2026-09-29持續冷開發以[原計畫§8–9](docs/catalog/01-project/NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md)的C0–C6為準，沿用原GPT-6 Sol／xhigh task設goal。使用既有pixels、非學習式輔助標記、合成與fake-clock補組合／Hold／排程與效能；本輪不啟動emulator、真觸控或模型訓練。部分未達是里程碑，不是goal完成；proposed不能作人工gold，缺實戰語義不阻塞其他可冷完成項。

旋轉情境另須覆蓋「Hold按住期間線仍旋轉」與「Note接近時才與線對齊」。區分Note朝向、運動方向、線法向；不以遠處當前同法向作必要配對條件。Hold的接入、持續body接觸與tail結束分開驗證，保留同一contact並依當前支持修訂Move；不固定初次Down位置，也不盲目跟著線旋轉。

## 不變原則

1. 自有正式邏輯全 C++20、單程序多執行緒，包括正式資料、分析及訓練邏輯；第三方原版工具可保留原語言。CMake、JSON、XML 與必要維護 shell 可使用。
2. 遊戲決策只能來自即時 pixels 與有界近期追蹤。不得讀譜、遊戲內部狀態、記憶體、音訊節拍、歌曲身分／進度或歷史按鍵序列來決定觸控。
3. 擷取只公開最新 frame；慢消費者跳過舊圖。物理 buffer、mailbox、軌跡、推論與診斷皆有硬上限。診斷資料單向流出，不回饋動作策略。
4. 當前觀測、線／音符身分、note→line 關聯、撞線預測、動作規劃與注入有清楚邊界。預測或插補不是當前像素證據。
5. 主機時間統一 QPC monotonic；明列 capture_complete、pixels_ready、recognition_complete、預定觸控、injection_start／return。來源時間另列 domain；未知來源年齡不冒稱已量測。
6. 單一 touch owner；門控與每個計畫獨立到期。未知注入結果不重試 Down，失效撤銷並釋放，完成意圖不復活。
7. 效能報環境、n、p50／p95／p99／max、失敗與 jitter；不能只報平均、只報已排程成功子集或把各階段 p99 相加。

## 固定基礎與驗證

- 主用擷取固定 gRPC payload fast／RGB888 top-down／256 KiB；不重開選型。WGC／DXGI／scrcpy 僅 bench，MMAP diagnostic-only，無已驗正式備用。
- 模擬器、解析度、方向、縮放與五指 mapping 由 profile／preflight 指紋核對。Fixture 能力不等於 Phigros 動作語義驗收。
- main50比較／donor版本observer50／planner27／diagnostics11、live0，C36h tint1為behavioural／experimental baseline；observer38／planner21／diagnostics7是9/29冷起點，歷史main37/19七輪的manifest數字欄另為36/18，依binary/source SHA辨識。保留observer33／36／37／38及A36派生frozen配套，不以版本號代替SHA。manual-session由使用者選曲及按Play；本整理包不啟動、不新建長期goal。
- 時間預測／排程變更須有可重現的合成軌跡／fake-clock 回歸；實戰結果附版本、環境與原始證據。
- 模型資料按歌曲／譜面家族切分；標註的 unknown 與 proposed 不升格為人工真值。不按歌名調參，不將舊動作當專家策略。
- 清理後保留範圍與資料缺口見 [清理紀錄](docs/catalog/06-engineering/CLEANUP_AUDIT_20260928.md)。旧文件／legacy 程式可從 Git 歷史查閱；刪除的 ignored raw 不可假稱仍可重算。不要恢復舊流程只為滿足歷史文件。
- 修改契約時同步更新文件。保留目前兩輪跨曲原始證據、能力報告、獨立 baseline 與必要測試輸入；新增資料另有容量帳本。
