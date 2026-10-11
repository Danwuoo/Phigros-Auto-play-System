2026-10-12 最新 actual main78／`43359f8`：14 fresh compile occurrences 的 configure／build Native、runner、verification皆0，完整held／streams／active0；首affinity control因48GiB總量50,029,647,183B＋原build reserve1.5GiB超限而NOT_LAUNCHED。不是控制或producer失敗，26 regression／Normal3／AA12／noise／ABBA皆NOT_RUN；原source／14objects／兩Native8及capacity STOP見[main78保存](MAIN78_COMPILE_PASS_CAPACITY_STOP_20261012.json)。main77的Normal3／AA12 PASS、noise13／18及ABBA NOT_RUN仍是歷史結果，cost NOT_READY／live UNQUALIFIED。

後續只修正已封存build之後控制階段使用原tests1GiB reserve，48GiB／minimum-free20GiB及全部數值不變；必要新HEAD依現有來源契約fresh14，不重跑原75／production9。另沿用既有逐entry decoded SHA／原件再次核對的lossless歷史封存方式，保持原source／raw payload／負結果／STOP可恢復；回收前不授資格。使用者離開仍完成全部可冷工作至emulator邊界，本輪不開裝置、新thread或ETW recording。以下較早最新／proposed為歷史。

2026-10-12 最新 actual main77／`fce23e7`：必要10 fresh cold TUs、9 build/control Native、pre-cost必要核對、current Normal3完整raw及原數值、AA12完整raw與validity全PASS；noise13／18 adequate、5不足，ABBA NOT_RUN。原source／copies／raw／STOP與53個Native8保存於[main77結果](MAIN77_NORMAL_AA_PASS_NOISE13_STOP_20261012.json)。CPU v3 partition未解決全部波動，cost NOT_READY／live UNQUALIFIED。

下一必要候選只短路`src/game_motion.cpp`純讀ridge的無用後續gray探查，像素／80與18門檻／布林策略保持；一個fresh formal motion object明列覆蓋原22-member archives，另跑26個既有相關回歸。新分支14 compile occurrences／11targets／9exe，原libs与原PASS來源不冒fresh全包；R2／noise／ABBA及全部原數值不變。見[必要motion delta契約](MOTION_PROBE_DELTA_20261012.md)。使用者離開仍完成全部可冷工作至emulator邊界後停止；本輪不開裝置、新thread或追加ETW recording。以下較早最新／proposed為歷史。

2026-10-11 最新 actual main57／`5dce75d`：獨立三TU／16控制及Tap／Hold／Dense各一次完整raw auditor通過；原數值皆true，Dense兩次render超期照報。這些只屬diagnostic，不能替代current Normal3或授cost／live資格。原main55正式Tap STOP保留。

compiled55／`12ec055`、execution57的獨立AA r03已排除264字元source canonical錯誤：全939 exact copies使用main內短路徑，原946 snapshot保存。首Tap producer／auditor／gate Native均0、full raw integrity通過，但producer／publication超期40／40、schedule lateness最大768.03ms，原數值gate失敗而STOP，後11筆NOT_RUN；noise／ABBA未執行。詳見[實際盤點與完整保存索引](MAIN57_DIAGNOSTIC_AND_AA_STATUS_20261011.md)。

latest completed noise仍main44的6／18 adequate、ABBA0／36、cost NOT_READY／live UNQUALIFIED；沒有已支持的排程修補，原因unknown。必要validator／collector／score-only軟體及原冷控制已完成；尚須current Normal3、AA12／noise18／未跑ABBA36及完整readonly，真正裝置收據在emulator邊界後取得。本輪不開裝置或新thread，不重跑原75／production9；以下「最新」與proposed為歷史封存記錄。

2026-10-11 最新 main55／`12ec055`：必要CPU32／原584及wait43／diagnostic20／active12／QoS24、reader2／原81及兩policy各12、production52／七控制通過。Normal Tap full raw11／3,358,832B integrity通過、pool0，但一次168.9509ms active wait造成producer／callback超時7而STOP；後續Hold／Dense／AA／noise／ABBA未跑。原負結果、source、raw、STOP保留；CPU零差不是精確off-CPU證據，原因仍unknown。

下一步為獨立bounded相鄰QPC max-gap診斷，base55 libs與新compiled delta分列，不改shared wait policy、原數值或資格，不重跑原75／production9；詳見[診斷範圍](PRODUCER_POLL_GAP_DIAGNOSTIC_20261011.md)。latest completed noise仍main44的6／18 adequate，ABBA0／36、cost NOT_READY。完成全部可冷工作至emulator邊界停止，本輪不開裝置或新thread；以下main55 proposed為原封存計畫。

2026-10-11 最新 main54／`35073ef`：CPU32／584、原wait43／diagnostic20／active12、reader2／原81／active-policy12、production52／七控制及runtime18來源／36副本通過。normal Tap producer／auditor Native皆0、完整raw10／3,332,367B integrity通過，原數值pool drops4、producer／callback deadline overruns65而STOP；Hold／Dense／AA／noise／ABBA未跑。原946份source、raw、STOP、兩Native8與1944 leaf／24parents／31shards見[main54負結果](MAIN54_PRODUCTION_TAP_NEGATIVE_20261011.json)。原main32 PASS保留，latest completed noise仍main44的6／18 adequate，ABBA0／36。

本輪actual wait診斷：measurement2136 full active spin跨1313.0831ms／28327polls，lateness1293.2918ms，kernel calls0；前callback至wait .0003ms、afterwait至render0、render .3464ms／callback .1231ms。CPU原樣本零差的accounting granularity未知，不能據此認定精確off-CPU、OS／guard根因。actual54 Native UTC前後30秒無Kernel-Power／Power-Troubleshooter含506／507或Kernel-General clock-change1事件；不能由無事件完全排除漏記。

下一main55為實驗cold own-process HighQoS候選：以正式Get／SetProcessInformation(ProcessPowerThrottling)讀完整prior，只控制execution-speed位元並保留其他documented位元，explicit query／set／readback／exact restore；未知／失敗拒絕，析構fallback不授verified。scope影響own cold process所有worker含writer，AA A／B一致；正式capture／manual runtime與host registry／power plan不改，process NORMAL32、thread HIGHEST2、full_period_active_qpc與原workload／期限／成本界限不改。QoS分類是官方支援的候選理由，尚未證明main54原因或有效性；必要控制與成本仍待fresh Native，不冒PASS或live資格。

新QoS控制分列24項（23個fake API邊界＋1個actual Windows full-state還原），readonly另列12項policy／型別／身分／完整還原拒絕控制，原43／584／diagnostic20／active12／reader81／active-policy12分母保持。actual Windows snapshot須綁定該Native PID；AA／ABBA各producer須綁各自PID及consumer main thread。這些控制與main55成本尚待fresh Native，不能以合成資料通過代替實際量測。

依最新授權只完成必要cold開發／Normal3／AA12／noise18／未跑ABBA36及完整readonly，原75／production9不全盤重跑；retained48GiB／minimum-free20GiB與原R2保持。完成到emulator邊界停止，本轮不啟動裝置、不建立新thread。以下main53／54 proposed字樣為原歷史記錄。

2026-10-11 最新 main53／`11079f8`：CPU32／584、reader2／81、production52／七控制、runtime18來源／36副本通過；原 producer wait43 與新 diagnostic20 分列通過。normal Tap 完整 raw10／3,321,144B integrity 通過，producer／auditor Native 皆0；原數值 pool drops6、producer／callback deadline overruns149而 STOP。完整946份 source、原 raw／STOP與小收據見[main53負結果](MAIN53_PRODUCTION_TAP_NEGATIVE_20261011.json)。Hold／Dense／AA／noise／ABBA未跑。

新有界 wait telemetry 定位兩次 blocking wait：warm149 設17.6978ms、Wait跨782.3458ms；measurement1996設17.5702ms、跨825.7037ms，皆 raw WAIT_OBJECT_0、無 spin；不能由 thread CPU counter零差認定精確off-CPU，也不能推定 timer signal、dispatch、sleep 或 guard 根因。只讀 power 記錄未見該輪 sleep／resume，目前 idle sleep AC／DC為Never；guard無runtime suspend，沒有逐poll交集資料。原 main32 PASS不撤銷。

下一 main54 cold synthetic producer 改為 full_period_active_qpc：整段 remaining period 以QPC／YieldProcessor主動等待，initial horizon100ms／total poll cap2^24；不建立或阻塞timer，absolute epoch／2560／20ms／warm256／measurement2304與全部原數值界限保持。增加CPU使用量尚待原 workload量測，A／B同policy，不冒稱2ms active tail；正式 capture／manual runtime、writer、優先序與affinity不改。原43／584及diagnostic20不重定義，另列active wait12與readonly policy12必要控制，目前尚待fresh Native驗證。

原75／production9不重跑；必要身分建置／控制後續 normal3 → AA12／noise18 → 原未跑ABBA36 → 完整readonly → 裝置邊界。最新完成noise仍main44的6／18 adequate、ABBA0／36。cost NOT_READY／live false；本輪不開emulator、裝置或新thread。

2026-10-11 最新 main52／`c7aad78`：CPU32／584 controls、reader2／81、production52／七個控制執行及新27項 producer placement 控制通過，既有 producer wait43 控制仍單列。normal Tap 完整 raw／原成本數值通過；Hold 完整 raw integrity 通過，但 pool drops2、producer deadline overruns115（warmup37、measurement1＋77；gap769.3928／1551.2314ms）。四個 producer／auditor Native 皆退出0；完整946份 source 快照、原 raw／STOP及小收據保留於[main52負結果](MAIN52_PRODUCTION_HOLD_NEGATIVE_20261011.json)。

priority／affinity 的實際 readback／explicit restore verified，但 timing 尚未修好；根因 unknown，不能認定 OS 或 guard cause。下一 main53 僅添加 diagnostic-only 有界 timer／spin／thread CPU telemetry，不調 policy 或 gates，目前尚未 compile；新 nested diagnostic_controls 為20項，與原43／584分開，尚待 fresh Native 實際驗證。default AA 與正式 manual 不啟用此診斷，score no-gameplayfiles 契約保持。

原75／production9不重跑；必要身分建置與控制後仍依 normal3 → AA12／noise18 → ABBA36 → 完整 readonly → 裝置邊界順序。最新已完成 noise 仍為 main44 的6／18 adequate，ABBA0／36；cost NOT_READY／live false，本輪不開 emulator、装置或新thread。

2026-10-11 最新 main51／`8257eb0`：CPU32／584、reader2／81、production52／七控制、runtime18來源／36副本通過；必要normal Tap數值通過，Hold完整raw integrity通過但producer deadline overrun1，Dense／AA／noise／ABBA未跑。原四Native皆退出0；負結果與原source／raw／STOP見[main51記錄](MAIN51_PRODUCTION_HOLD_NEGATIVE_20261011.json)。下一main52只在cold synthetic producer重用現有AA producer affinity，補readback／explicit restore正反控制；writer與正式manual runtime不改，原HIGHEST2／2ms tail／2560／20ms／全部數值門檻保留。現有source資格契約須fresh必要CPU32／reader2／production52與七控制，再完成normal3／AA12／noise18／未跑ABBA36／完整readonly。原75／production9不重跑；最新完成noise仍main44的6／18 adequate，ABBA0／36；完成至emulator邊界停止，不開裝置或新thread。

# 2026-10-11 噪聲修補與縮小重驗範圍

2026-10-11 最新補充：所有可冷完成的啟動前開發／測試仍需完成；明確fresh formal／action CPU delta、validator及正式binary接線見[本輪契約](CPU_COMPONENT_REPAIR_20261011.md)。原75／production9只歷史前提，不全盤重跑。main44失敗已保存；完成後停在emulator邊界，不開新thread。

使用者原文：「修復完不用全盤重跑，跑噪聲檢查就好，趕快完成」；其後補充
「但是你還是要完成到實機 ready 前的所有測試與開發」。已通過的三配置75組、
完整source/build圖3、3063 prefix、fault4、production9不全盤重跑。
修補僅限必要component建置／控制、新AA12／原R2噪聲檢查，再完成原未跑ABBA36；
成功至需要emulator的current profile／capture／RPC／physical階段停下。

使用者另指示：「然後我要離開一下，所以妳完成後先不要開新的 thread」。
本task不得建立新thread或啟動emulator。既有Chapter Legacy IN一輪只記分數授權保留，
不是本輪冷測操作；沒有新goal、automation、訓練、push或worktree授權。

原source `aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de`（main32）三配置各25組、
完整圖3、prefix3063、fault4及production9 integrity通過；normal Tap／Hold／Dense
原numerical gate通過，其餘stress／instrumented normal cost false保留。AA12 validity
全通過，但18 noise欄中observer及capture-to-owner的p95／p99共12欄不adequate，
owner六欄adequate。noise producer/native/runner/verification均0，原數值NOT_READY，
ABBA36 NOT_RUN。這不是producer deadline failure或程序身分失敗。

原913份source snapshot、原raw與STOP見
`out/prelive-pre-emulator-planning/main32-stage-inventory.json`；37個native stages完整
296小ProcessEvidence見`main32-stage-native-completeness.json`。原件不回寫。
Debug／ASan舊compiler outputs逐entry ZIP讀回SHA後保存，Release保持原bytes。
還原舊output path才能重新讀原完整build圖；不把較晚文件HEAD冒稱原binary來源。

18個inactive歷史／main32失敗噪聲journal逐entry解壓SHA與原件再次核對後，
4539082365 bytes保存於125609008-byte ZIP：
`out/retained/consolidation-20261008/inactive-noise-raw-archive33/original-journals.zip`，
同目錄`proof.json`保存原path／bytes／SHA／entry索引。只刪除核對後的未壓縮副本；
原payload可恢復，原報告、負數值及STOP不變。原main32完整raw讀取須先恢復原path。
inactive main05與main32 Debug／ASan compiler ZIP及各自verified proof亦保留；
Release static libraries和binary仍在原path，新修補不依賴上述封存ZIP。

有界B dense1000診斷（cost qualification false）測得741個measurement owner frames，
observer wall總4874.582ms／thread CPU總4265.625ms，hook wall總3695.217ms／CPU總
3078.125ms。CPU採樣量化未知、counter與QPC界線不同；不能把差值當精確off-CPU，
不能認定特定背景程序／core migration／thermal根因。原診斷與首次summary prelaunch
capacity STOP均保留；回收inactive main05 outputs後只讀原診斷，未重跑producer。

新的cold consumer調度候選維持NORMAL process，consumer scoped ABOVE_NORMAL，
query／set／readback／同thread explicit restore逐項驗證；A／B同policy。
producer原ABOVE_NORMAL、writer、frame cadence、QPC epoch、workload與原R2全部數值
界限不改。這是待量測候選，不宣稱已證實noise根因或實機成本改善。
正式`src/`／`include/`與production `pas.exe`不改。新chain只重新編譯必要TU，
沿用逐檔SHA核對的main32 Release static libraries；收據明列base/delta identity、
原25組只是原來源prerequisite，不能把重用說成新三配置fresh建置或whole qualification。

必要噪聲重驗須有新source／binary／campaign／environment及原完整AA12，不能挑成功
重測、重排配對或改原noise公式／coverage／deadline／RSS門檻。失敗停止該批並照報。
通過後原未跑ABBA36仍須完成；noise或ABBA通過都不發live admission。
保留retained48GiB／free20GiB及原stage、candidate、raw、metadata caps。

main33／`c3a51ca` configure native／runner／verification皆0。後續graph檢查原以
POSIX分隔符及`/MD`辨識，Windows Ninja實際使用反斜線及等價MSVC `-MD`，因而冷STOP；
沒有compile／controls／AA量測。保留原configure與STOP，修正路徑正規化及相同runtime
flag辨識後用新attempt，不放寬兩TU、Release flags或原測量契約。

main34／`626d32d`兩個target、controls36及全部native／closure通過。AA loader的
script binding欄位與caller同名PowerShell參數碰撞，將path變成hashtable，AA campaign
封存前冷STOP；沒有producer／噪聲量測。改為獨立`noiseRepairBindingPin`，原必要建置
收據及prelaunch負結果保留；新attempt不重跑75組或production。

main35／`42a450c`必要兩TU及controls36通過。首筆Tap完整2560張、producer／publication
overrun0、consumer readback／restore及完整raw auditor通過；cost analyzer native2，
stderr `hash file bound/type`。原native guard identity／held／streams與active0正常，
原status保留PRODUCED_BUT_UNQUALIFIED／後11筆NOT_RUN，未有noise或ABBA數值。
新loader誤把全部已核歷史依賴加入每次測量的actual inputs，六個不供該冷runtime使用的
gRPC Release／Debug library為655596174至2515648002 bytes，超過原256MiB單檔界限。
修正為明列當次runtime／source／static／receipt metadata，完整依賴仍於loader核對，
不提高原hash、freeze或closure上限。純維護修正沿用已通過相同C++ component bytes、
compile source與controls收據，另列新execution source／maintenance amendment；
不重新編相同TU，也不冒稱新source fresh compile或重用舊producer作新AA測量。

main36 執行來源 `505ae43`、compiled source `42a450c`，沿用 main35 chain SHA
`fc7c040c4673899b4a237a5917eb734e5c13c088923843a65d49de0ddf9d5a89`。
全12筆新 AA 的 producer／auditor／gate validity通過，producer／publication overrun0，
consumer priority 12/12 readback與explicit restore通過；noise native／runner／verification0，
80.103942秒自然退出。原公式18欄獨立重算一致：observer與capture-to-owner p95／p99
仍12欄inadequate，owner六欄adequate。noise READY=false、comparison=false，ABBA NOT_RUN；
不把單一priority變更當已解決。全915 codefiles先逐bytes複製／SHA核對保存於
`out/prelive-pre-emulator-planning/source-505ae43-noise-snapshot/snapshot.json`。

main36失敗noise的12份journal共2013879133 bytes，逐entry讀回SHA及原件再次核對後，
保存於56923317-byte ZIP：
`out/retained/consolidation-20261008/inactive-noise-raw-archive36/original-journals.zip`；
同目錄proof保存原path／bytes／SHA。原report、attempts、audit、noise數值與STOP保持；
完整舊raw reader需先還原原path。保留48GiB上限，未刪除payload。

下一個有界診斷只以A／B各一次dense1000補原九段wall／thread CPU／cycles及
GetCurrentProcessorNumberEx begin/end；每process一次64KiB／256 LP上限的
RelationProcessorCore topology保存原group／number／EfficiencyClass。
只fresh編譯chain與summary兩TU，五個原library不持有Profile並逐SHA核對重用。
端點相同不證明中途未遷移，wall−CPU不是精確off-CPU，cycles不換算QPC；
不猜P/E編號，不改affinity／CPU sets。此診斷cost qualification=false，不能代替AA／ABBA。
最新使用者再確認須完成所有啟動實機前可完成的測試／開發；仍不開新thread或emulator。

main37／compiled source `156214a`的新A/B dense1000 producer及兩個summary全部native／
runner／verification0，4個stage自然退出、active0、held與streams完整。
A measured733、B739；observer不同processor起訖583及558，hook277及402。
當前拓撲18 LP／14 cores，raw最高class1為core0/9/10/11；不同端點已確認切換，
class別統計仍有中途遷移混合，不能解讀成特定core的純residency效能。
全部918 codefiles逐bytes複製／SHA核對保存於source-156214a-phase-snapshot；
這兩個probe不授noise或live資格，不作favorable rerun。

下一個明確冷修補以獨立OS查詢產生worker placement，不讀診斷JSON作policy。
最高rawclass取各不同physical core的最低LP，排序首LP給consumer、次LP給producer；
當前18-LP環境將為0／10。原process與writer affinity不變，producer原mask（可能繼承
consumer）另行保存，兩者explicit set/readback/restore皆必須通過。只支援已實際確認
single active group；不猜跨group還原、不選同core sibling、不默默fallback。
三個必要TUs為chain、原priority controls及新的65項affinity controls；新AA／ABBA每筆
讀回placement與兩scope收據，R2所有workload、公式、門檻與原PAS正式source不變。

為保留新AA／ABBA headroom，另將兩個inactive歷史cold production目錄的43份大journal，
以及已完成main37 A/B與main32 phase診斷3份journal，共46份／1265799319 bytes，
逐entry decoded SHA及原件再核對後保存於35560489-byte ZIP：
`out/retained/consolidation-20261008/inactive-cold-diagnostic-raw-archive38/original-journals.zip`。
同目錄proof保存原path／bytes／SHA，原報告與summary保持；只刪核對後未壓縮副本。
新AA／ABBA raw與當前main32正式前置證據未移動；原診斷完整重讀先restore原path。

main38／`b37516d15a13a9b254e2e3caec32c387fa45cdd6` fresh3 TUs、priority36／affinity65
與reader fresh2 TUs／34 controls全native通過。實際只讀bundle核對原三配置／prefix3063／
fault4／production9／新narrow build均PASS；AA／noise／ABBA缺少，正確NOT_READY，未發資格。
新chain SHA `c46b6a70f3a39a7aef0488f40b864d7080f41a7895b7cb6458aed00491f5d469`。
首筆Tap2560 producer native0，priority及兩affinity scope還原verified，但controller新helper
未跨LoadBuilds作用域，STOP於auditor之前。另以原source、原完整raw補audit與gate封存：
audit integrity PASS／native0；原gate native0、validity=false，failure
`tap_streamed_workload_and_cadence`。producer超期1／publication2，不能由修腳本升格通過。
原status／raw／STOP、後11筆NOT_RUN、noise／ABBA NOT_RUN不改；923 source files逐SHA
複製保存於`out/prelive-pre-emulator-planning/source-b37516d-main38-snapshot/snapshot.json`。

原attempt811開始晚27.4477ms，render及publication超下一deadline8.2108／9.1408ms；
attempt1573開始晚18.4794ms，render尚餘0.6206ms、publication超0.6023ms。
沒有timer-arm／signal／resume邊界，不能認定OS timer或特定排程根因。下一候選仍保留
原epoch、2560全attempt及20ms cadence，producer在專用core以固定20ms QPC active tail
避免每周期睡至deadline；較遠deadline用原high-resolution timer睡到tail，ceil100ns，
四次early wake上限。tail只QPC／YieldProcessor，poll硬上限2^24；不早render、不跳attempt、
不重設due、不fallback。CPU負載增加照實保留，這是候選修補，不是noise／cost資格。
原22＋新21 producer controls共43，fresh必要四TUs；reader仍fresh2TUs／34controls。
所有原R2、正式source／five libraries、容量及停止門檻不改；不重跑75／production9。

main39／`f28643c646c9ba5637e6b91f8cd4f5753dc225f8` fresh4 TUs，priority36／affinity65／
deadline43全native通過；reader fresh2 TUs／34 controls及actual pre-cost base/delta joins通過。
chain SHA `747e27ff67756a36432da51f144fb6f961b1eeb848539a84aabe585f437b93bb`。
AA12 validity全PASS，Tap／Hold producer／publication超期0；dense3 warmup producer1／
publication2照報，原R2零超期限制本來只適用streaming Tap／Hold，dense不加新資格。
noise native／runner／verification0，78.3014071秒自然退出、active0；18項公式獨立核對一致，
observer／capture-to-owner十二項inadequate、owner六項adequate，ABBA NOT_RUN。
詳見[18項原值與來源索引](MAIN39_NOISE_NEGATIVE_20261011.json)，923 source files已逐SHA
保存於`out/prelive-pre-emulator-planning/source-f28643c-main39-snapshot/snapshot.json`。

此候選實際consumer LP0／producer LP10，12/12 placement及兩scope readback／restore正常；
Tap／Dense尾端成本較main36高，不能證明CPU0根因或末兩core必較好。下一候選只改highest
raw class各core最低LP排序末兩個，consumer較低／producer較高；generic bounded OS policy，
不讀diagnostic JSON、不硬編12／14，不修改priority／active-tail或原R2，不在同binary重測挑好值。
必要fresh4 TUs與36／65／43 controls不變；新AA／noise通過後才執行原未跑ABBA36。

post-noise readonly runner在native前因遞迴導入歷史closure leaf／freeze的file lists超8192
冷STOP，沒有reader數值。修補只封存這些metadata本身並停止展開它們的歷史file list；
reader仍核對各metadata SHA／完整葉denominator及before/after closure，不提高8192 cap。

main39十二份inactive raw journal共2002144786 bytes，逐entry decoded SHA與原件再核對後，
lossless保存為56583392-byte ZIP及原path／SHA proof：
`out/retained/consolidation-20261008/inactive-noise-raw-archive39/original-journals.zip`。
報告／attempts／noise／STOP均不改；只刪核對後未壓縮副本，完整重讀先restore原path。

main40／`f40fecf`fresh4 TUs、36／65／43 controls及reader fresh2 TUs／34 controls通過；
actual只讀pre-cost joins核對原75／prefix3063／fault4／production9及new delta全PASS。
AA前兩Tap validity PASS，第三Tap原gate `tap_streamed_workload_and_cadence` STOP：
producer／publication各1超期，2560 published／2559 consumed，後九筆及noise／ABBA未跑。
923 source files已sealed於`source-f40fecf-main40-snapshot/snapshot.json`；
見[原負結果與attempt索引](MAIN40_AA_NEGATIVE_20261011.json)。

唯一超期attempt2462開始晚20.7934ms，render .2661ms、publication .1785ms，分別超nextdue
1.0595／1.2380ms。前一publication到2462due僅19.6515ms，故20ms active tail不會走kernel
timer分支；不知道延遲在wait入口前、spin中或return到render之間，不能斷言preemption根因。
2463晚1.2396ms、2464恢復.0012ms；完整2560 due均原epoch+20ms、沒有early render／reset。
下一調度候選只將producer scoped thread priority1→2（THREAD_PRIORITY_HIGHEST），consumer
仍1／writer原priority／NORMAL process32／generic末兩core／20ms tail不變。fake與actual
Windows readback期待同步，43 controls總數不變；不是real-time保證，實際CPU／writer成本仍
由新source/binary的原AA／noise／ABBA評估，不放寬gate或在同binary重測挑好值。

main40三份inactive Tap journal共36098542 bytes，逐entry decoded SHA及原件再核對後，
lossless保存為1707589-byte ZIP及proof於`inactive-noise-raw-archive40`；報告／attempts／
gate／STOP保持，原path完整重讀須restore，沒有刪除原負結果內容。

main41／`7057be7`fresh4 TUs及36／65／43 controls、reader fresh2 TUs／34 controls通過。
chain SHA `0e2cc31dbeba1b4eee5d4bd7bdc21dd54dcf0cf7b4257253dad52de065bd2a9a`。
新AA12 validity全PASS，producer／publication超期12/12皆0，兩affinity scope及consumer
priority readback／restore全核對。noise native／runner／verification0、80.6176276秒自然退出，
owner六項及dense observer／capture-to-owner p99兩項adequate，其餘十項inadequate；
ABBA NOT_RUN。完整18原值及source／binary／receipt索引見[main41負結果](MAIN41_NOISE_NEGATIVE_20261011.json)。
923 source snapshot已保存；actual post-noise reader核對base／prefix3063／fault4／production9／
delta／AA12／final bytes全PASS，18公式全部重算吻合，files6760、正確NOT_READY，未發資格。

此actual runner外層Native是明列metadata projection：closure roots／freeze／leaf metadata
為BFS terminal，不宣稱外層凍結全部transitive leaf payload；actual source／runtime／selected
dependencies仍作外層inputs。readonly Reader仍逐直接leaf SHA、完整identity分母、completed
native before/after seals及finish再次SHA核對；原Native8192files／128leaves及reader32768files
各自上限不改，不重讀歷史raw或大ZIP、不冒稱移除的compiler graphs已重新驗證。

下一候選只提高consumer scoped thread1→2（HIGHEST），producer2／NORMAL32／writer原值／
generic末兩core／20ms tail全部不變。新controls scalar與corePlan.consumer_priority_requested
必須同為2，source／fresh compile／controls native joins照核；缺scalar／缺plan的舊1只准
已封存legacy header與tests SHA pair，不給任意缺欄位default或priority範圍fallback。
AA／ABBA與readonly reader每run按唯一policy核requested/readback及原restore／failure條件。
36／65／43不增減；reader新增六個有界舊1／新2／缺欄位／混合／不一致／錯runtime控制為40。
這仍是調度候選，不是已確認噪聲根因或real-time保證；原R2及old75／production9範圍不改。

main41十二份inactive raw journal共2003731386 bytes，逐entry decoded SHA及原件再核對後，
lossless保存為56583098-byte ZIP與原path／SHA proof於`inactive-noise-raw-archive41`；
reports／attempts／noise／STOP不改，只刪已核未壓縮副本，完整重讀須restore原path。

main42／`96c8c2a4b465c041d9117faa8140b9361c0beeae` fresh4 TUs／36／65／43 controls及
reader fresh2 TUs／40 controls通過；AA12 validity全PASS，producer／publication皆12筆零超期。
noise native／runner／verification0、73.3545193秒自然退出，但observer／capture-to-owner
12欄不足，owner六欄adequate，ABBA NOT_RUN。actual reader base／prefix3063／fault4／
production9／delta／AA／final bytes全PASS、18公式重算吻合，files6835／338912560 bytes，
正確NOT_READY。source923 snapshot與完整18值見[負結果](MAIN42_NOISE_NEGATIVE_20261011.json)。
十二份inactive journal共2006924437 bytes，逐entry decoded SHA與原件再核對後，lossless
封存為56664238-byte ZIP與proof於`inactive-noise-raw-archive42`，完整重讀須restore原path。

main43／`ba1aaa6f3b66db9620e57560e9110387c1d4fc8c`只新增負結果文件，923 code pins與main42
source snapshot完全一致。既有兩TU的A/B dense1000診斷／兩summary全部native0，measurement
各744，full owned998／1000，九phase起訖端點皆一致。observer wall sum3672.216／4934.045ms、
CPU sum3046.875／4187.5ms；hook wall3021.018／4697.656ms、CPU2375／3859.375ms。
CPU取樣呈15.625ms階梯但量化未獨立量測，不能逐frame相減當off-CPU，cycles不轉QPC；
診斷不作AA／ABBA資格，見[來源及完整summary索引](MAIN43_PHASE_DIAGNOSTIC_20261011.json)。

下一候選只有producer固定active tail20ms→2ms。20ms cadence下，前次publication至下一due
不超20ms，所以舊20ms tail常態整周期忙等；這是程式结构，不是已測得熱／OS根因。新2ms
尾以同high-resolution timer等至due−2ms，保留ceil100ns、4早醒、QPC與pause／2^24 poll界。
consumer／producer仍HIGHEST(2)、process NORMAL32、generic末兩core與writer不變；原due、
全部2560 attempts／20ms cadence、R2及門檻不改。新plan／controls／實際compile joins精確綁定
2ms，舊20ms只許已保存來源配對，不新增任意range fallback。原22＋active21共43 controls
保留並修訂必要時間期待；真正timer超期仍由新AA／noise原gate照報，不在同binary挑好值。
Reader新增六項2ms／兩exact legacy／缺欄位／混配／類型／錯run控制，共46項，兩TU不變。
兩份main43 inactive diagnostic journal共378832511 bytes，逐decoded SHA核對後封存為
11306658-byte ZIP與proof於`inactive-noise-raw-archive43-repair`。首次helper因ZIP count仍為12
而在刪除前STOP，原件全保留；該11MB未簽收ZIP另留於`inactive-noise-raw-archive43`，不當正式proof。

## 2026-10-12 main76 實際結果及下一候選

main76 `2fddccd10607726128ea6a0f296fcf843e40acd0` 必要10個fresh冷TUs及9個build/control Native stages通過；actual pre-cost reader保留原75／prefix3063／fault4／production9來源核對，新Normal Tap／Hold／Dense三項full raw與原數值門檻均PASS。AA12完整raw與單筆validity全PASS，noise14／18 adequate、4／18不足，ABBA NOT_RUN；cost NOT_READY、live UNQUALIFIED。原source／copy／report／raw／STOP及53個Native8完整索引保留於[main76收據](MAIN76_NORMAL_AA_PASS_NOISE14_STOP_20261012.json)。Noise分析Native0／162.536161s，outer workflow1是既有「noise不足不開ABBA」停止，沒有新的producer失敗。

不足項為Tap capture-to-owner p99、Hold observer p99、Dense observer與capture-to-owner p99。原attempts分位數重算吻合，未發現分母缺陷。Tap尾部主要包含開始observer前的空檔；Hold／Dense含observer wall尾部，沒有consumer ETW或逐段CPU量測，不能把wall直接認定為preemption、CPU熱或某個外部process。未新增ETW錄製。

下一單一候選為`pas.cold-worker-placement.v3`／`highest_raw_efficiency_class_disjoint_balanced_core_lowest_lps`／version3。保留原末兩highest-raw-class核心的consumer／producer anchors，其他可用highest-class cores按lowest LP排序後輪流consumer-first／producer-next，兩worker各一組互斥physical cores，每core只取lowest LP。兩mask為各自eligible union，完整physical-core unions互斥；原anchor不可用仍拒絕，無SMT／lower-class fallback。兩core主機仍各singleton，不加四core門檻。本機預期consumer LP0／12、producer LP10／14；這只是通用調度候選，不是根因證明、CPU獨占或已修好聲稱。Migration/cache及producer少一個eligible core的成本，由新actual必要成本判斷。

原priority HIGHEST(2)、process NORMAL(32)、QoS/readback/restore、writer-before-affinity、2560／20ms、raw/archive容量、R2／noise／ABBA原門檻不改。沿既有10-TU overlay與原正式libs；不重跑原三配置75／production9。只完成變更所需控制及新的必要成本、尚未完成noise／ABBA與最後collection-preflight，至需要emulator邊界停止；這輪不開裝置或新thread。

控制流程僅排除原selected installed dependency中超256MiB的compile-only `.lib`，以canonical path／bytes／SHA精確核對；actual images/runtime/sources/objects/imported project libs與控制分母皆保留。Configure／build前後與最後整份依賴核對仍包含全507 selected dependencies；每個control的前置assert採相同compile-only排除，避免反覆讀同一份2.298GB archive。這不是量測或資格門檻放寬。

main76只讀attempts timing分解：`out/planning/main76-p99-timing-decomposition.json`，51855B／SHA256 `bed06c98e2457e145ac70c6d02719e1a1424d7d87f1637e4f66d9b48a820b75f`。只讀12份小report／attempts與少量既有Tap/Hold journal行；未新增Native、ETW或diagnostic錄製。全部measurement分位數與原report差值0；既有skip及不同owned分母照報，不省略長尾樣本。診斷不回饋runtime策略，也不授予cost或live資格。
