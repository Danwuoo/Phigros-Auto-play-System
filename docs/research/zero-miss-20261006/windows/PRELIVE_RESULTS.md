# 2026-10-06 current-rails-v1 冷準備交付

本包完成新的 current-pixel typed hook、候選自身 contact 前綴、三配置回歸、有限完整鏈
成本／fault／stress與操作準備。**成本與 physical gate 仍 NOT_READY；Chapter Legacy 目標未完成。**
沒有裝置讀取、emulator／Fixture／遊戲／真 touch、模型、push／PR／merge、goal、automation或另派agent。

唯一開發根 `C:\Users\wurre\Desktop\PAS-zero-miss-r4-20261006`，分支
`codex/prelive-preparation-20261006`。原 checkout HEAD 74e5443、使用者 AUX、ZIP 保持原狀。
初始核7份v3 core、3715份封存項目、256 PNG；完整選擇另核原錄影1–3063全部SHA。
本包不自簽總控獨立驗收；所有新收據與再跑入口在 `prelive-evidence-01`。

## 實作與自身前綴

`current-rails-v1`在同一 frame lease 的正式 GameObserver／lifecycle與
SessionGameOwner／scheduler之間執行；預設、唯一 backend 為無endpoint ReplayBackend。
正式 owner 增加 typed `hold_front_rails` proof，不能以 reason字串或front proposed偷換Down資格。
current RGB／rails／inside-outside、完整當前線競爭、三次30ms近期關聯與自身receipt分開驗證。
body patch不能給Down；active Hold 保留contact依當前body交區更新，tail另驗。
Tap沿未改v3，Drag/Flick沿formal observer／owner並受同一terminal execution gate。

history128×6／90ms，ledger128，max5 contacts，unknown Down不重試，completed／cancelled／
unknown／retired不復活。原v3／front/oracle不改；diagnostics不回流。沒有live `pas manual-session`
接線或可用真touch候選，操作包明確區分donor命令。

|原圖輸入|完整處理|raw proposals|候選accepted次數|自身 Down／Move／Up|裁決|
|---|---:|---:|---:|---:|---|
|原四個64-frame窗|256/256|859|164|3／7／1|ledger與釋放有效；動作來自Drag/Flick|
|原因果前綴1–3063|3063/3063|10314|2771|55／121／51|ledger與釋放有效；227 receipts完整join|

完整前綴含一筆 typed Hold head plan（note685／intent57／source14646），自身contact0成功Down
在(495.5,576)，下一當前frame14647有同intent body plan；後續自身Up有receipt。
這筆不能升格為正常tail完成、合法觸點gold或遊戲採納。原圖沒有證實旋轉Hold的實際Move／tail；
這些機械組合由独立冷控制驗，物理語義仍unknown。accepted不是合法opportunity分母。
body／terminal量測49／361、unknown1773完整保留。3030人工確認只在外部review，不是策略輸入。

首輪完整前綴在2742幀因已取消Drag/Flick identity重生，ledger在新Down後拒絕；
native／runner1，verification1與完整raw保留。修補先在dispatch前拒絕terminal identity，
新同corpus完整重跑通過，不seed舊touch。首輪False release_verified表示ledger不再可信；
原fake backend release receipt另保留，沒有把失敗改綠。

## 回歸、失敗與source閉包

|套件|Release13／attempt14|Debug05|ASan04／attempt05|
|---|---:|---:|---:|
|未改原prefix|113/113|113/113|113/113|
|未改front|56/56|56/56|56/56|
|自身複製bridge prefix|113/113|113/113|113/113|
|獨立current控制|68/68|68/68|68/68|
|受影響根Game／Tracking tests|186/186，skip0|186/186，skip0|186/186，skip0|

所有模式60個原RGB clip SHA在前後核對。改動涉及正式owner ABI的core先在同模式fresh roots
完整重編；後續fresh roots只複用source/header/flags/config相符的formal_core.lib：
Release13←Release11、Debug05←Debug04、ASan04←ASan03。候選、adapter、tests重新編譯，
原v3/front仍從未改source build。MSVC ASan自有closure插樁，第三方不宣稱全面插樁／UBSan／leak。
CL/link commands、Ninja deps、maps、imports所對應loaded-module backing SHA與core donor bindings均留存。

原root349證據引用其已核同源未改套件，未將舊timeout／skip算pass；186受影響tests實際重跑。
原356 layer-cases／3938 assertions的36fail仍原樣：effect2、capacity24、raw-body8、raw-hit-order2；
原54修18、新增原套件失敗0是歷史結果，本包未重跑／改原layer oracle，不宣稱修成全綠。

新失敗全部保留：Release01/02 configure、Debug02 configure、Release10 formal及ASan04 front
的member-image:5／identity STOP；Release03/05 compile失敗；Release04 null-ledger存取錯誤；
Release06三項、07一項控制失敗；Release12／Debug04／ASan03的三項新取消control設定失敗；
Review01 WIC RGB encoder；Release12 job契約json-cap preflight；原圖prefix01及容量prefix02拒絕。
Release08 formal185pass1skip另保留，後續已實跑該clip case。没有重開舊STOP、猜PID或放寬guard。
source08只有事後hash receipt，不能冒稱exact當時source snapshot；最終source／donor與raw閉包完整。
long-dense01的native0結果也因identity guard而STOP，另開long-dense02同source／輸入通過；
兩筆10000分母／raw均保留，不將第一次STOP升格成qualified run。

## 全鏈成本與安全

事前規則為未改 `PRELIVE_CURRENT_COST_PROTOCOL.md`，SHA
58d6136fcbce1ebf6c25a23795f10aab78013d46318c2b6261b7f6206abe91e6。
source、binary、flags、profile、generator與host/background在首筆前凍結，cost source不再修改。
Windows host QPC，latest producer→observer→pre-dispatch hook→sole owner/scheduler→FakeTouch
receipt→正式Journal enqueue，writer獨立線程並完整close。owner與observer同consumer，與歷史R3
Wake／decision mailbox／SessionArchive topology不同；真capture／driver／RPC與source age未量。
不加各階段p99。逐run完整attempts、joined QPC、n/p50/p95/p99/max/jitter、warmup、drop/skip、
all phases／failed／unknown／release、OS peak RSS、完整Journal都在raw，C++20獨立auditor核整體分母。

A/A實跑5筆：Tap四筆各2560（256warmup＋2304measurement），所有單筆門檻及raw integrity通過；
第5筆Hold baseline Down/Up=0，phase lateness n=0，停止normal。後續Hold3＋dense4共7筆未跑。
Tap noise也不adequate：capture→owner p95 d=2.75238ms、p99 d=2.957375ms；
observer p95/p99不adequate，owner兩項adequate。freeze SHA
a87db45accfa41d33b38dba239d68ad2fdfff7e90387215c6b91b227bf7f3eed，comparison_allowed=false。
36筆ABBA未跑，沒有候選結果用來決定noise或偷偷補跑。**沒有C5改善或R1–R3資格宣稱。**

獨立finite負載為Debug／ASan各Tap/Hold1000；Release slow／writer／5ms RPC stub／unknownDown
各1000；Tap與16線×128note raster dense各10000，完整producerattempt與consumer skips分開報。
它們不補進noise。Debug／ASan coverage下降及零動作照報，不是即時資格。
writer容量2＋30ms延迟觸發critical滿載，停止owner；unknownDown只一次失敗、不重試，verified release。
長Tap10000中owned8479、skip1521，Down/Up279/279、release有效；wall80.025s，OSpeak69,644,288B。
長dense02完整10000attempts、owned3393、skip6607、Down0；wall80.046s、OSpeak70,115,328B，
capture→owner n3393、p50=28.891／p95=52.754／p99=64.911／max=77.680ms，jitter39.716ms。
Journal admitted611,502,146B／13578rows，release有效。coverage33.93%與空動作不能作normal qualification。
所有pipeline raw auditor完整性通過；writer case明示prefix evidence不完整，其資料沒有刪掉或算成功前綴。
独立負控制只增加reported own_down，C++ auditor拒絕receipt_phase_counts，原raw／oracle未改。
Debug Tap/Hold owned603/486，ASan229/218，全部producer1000；ASan兩者零Down仍照報。
slow owned256/1000、skip744，rpc owned1000、Down/Up12/12；fault owned7、failedDown1，
writer owned1且critical fault停止。所有Release pool drop0，全部最終release有效；不足門檻仍NOT_READY。

## 容量、交付與下一個gate

新OUT上限12GiB、metadata64MiB、free最低20GiB；每新runner stage前後記帳，失敗占額。
為保留規定1GiB raw餘量，曾先拒絕prefix02；只將較早本包348個編譯中間`.obj`逐檔SHA無損封存，
原2,555,465,788B→355,097,490B ZIP，entry SHA全部驗證。全部attempt roots、exes／libs／DLLs／
maps／source／measurement raw與STOP原位保留，沒有壓縮／刪除raw JSON或借此改cost gate。
ZIP SHA4ffec459f52721c0b64939e825aad6a5a9e6dc9e609ae7cdec6af836c5017752；
object bindings提供舊path與archive entry。帳本使用file bytes，沒有量NTFS allocation clusters。

metadata seal保存全部params／freeze／stdout／stderr／退出及失敗、source snapshots、結果與SHA索引；
大raw timings／Journal、PNG、binary、依賴以external hashes保留，不重複複製入64MiB metadata。
最終容量、sealed counts、original checkout及PNG前後SHA核對在 `closure.json`。
再跑以fresh stage/root/report，詳見 PRELIVE_REPRODUCTION。所有local commits留在本分支，沒有push。

主要local code commits：7ac2a31（typed current Hold／自身橋接）、971000d（獨立旋轉line raster）、
ae34a18（完整pixel plan/receipt與容量）、54c0e86（Drag/Flick terminal pre-dispatch）、
cdc6b8f／be7ee7e（取消control設定及同配置core綁定）。成本controller602d756；證據文件另commit。
Release current_chain SHA `cedfc190875e0fb31e1f9cbb573f0ad070240fecfc82a62e153b488554e4e61b`；
formal game.hpp SHA `f17a9f03fc385d30dc3bc1b04aa6fcdd3fe8e1a2bb642e9aec0e2b4f0128c4b6`，
game.cpp SHA `7cd3b8379c963ae709975c99dde0d605fa36ba7a82fd752f0c6b34c85799edc8`。
完整SHA而非版本號是辨識依據，before hashes、三配置binary／DLL／lib均在manifest。

|readiness項目|完成／結果與收據|仍需外部證據|
|---|---|---|
|舊source／封存／原PNG|初始與closure核對、原36fail/STOP保留|無|
|current pixels與review|通用候選、4張原圖卡、typed proof；physical unknown|1519身份、合法Hold接觸語義|
|正式typed離線整合／自身prefix|68controls＋3063完整自身receipt，無device endpoint|遊戲adoption／真driver|
|三配置變更回歸／依賴|113/56/113/68/186均pass；source/binary/donor閉包|無|
|完整冷鏈成本／安全|AA5停止、AB0；獨立finite負載與10000stress結果完整|未取得資格；真capture/RPC未量|
|操作包／五contact／停止與結果表|已備，預設零裝置命令、未啟用live入口|新的device/touch授權與資格|
|當前Chapter Legacy分母／IN／Miss0|目前unknown；模板不借歷史20曲作分母|目前UI＋同candidate完整IN結算|

本地授權範圍的有限準備以可核負結果交付；不承諾冷測通過便可實機ready。
目前Chapter Legacy同一候選全曲IN解鎖／完整Miss=0未完成，P/G/B／score照報，沒有AP前置。
