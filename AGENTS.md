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
