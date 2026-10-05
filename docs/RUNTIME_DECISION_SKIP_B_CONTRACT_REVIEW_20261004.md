# 工作包3：B最小量測契約審查

2026-10-04 Asia/Taipei。**建議：值得另案最小實作，限回答區間覆蓋；本包沒有實作或量測驗收。** 新觀測有機會區分「短decision生命期被前snapshot的accept前服務涵蓋」與「主要位於其後」，從而決定是否值得另研究accept前的共同診斷路徑。不能從覆蓋直接指派JSON／OS原因、刪診斷、改觸控或宣稱Miss改善。若總控沒有這個工程選擇要做，或新契約仍不可識別，停止量測線。

[包2總控決策](RUNTIME_DECISION_SKIP_A_PACKAGE2_CONTROLLER_REVIEW_20261004.md)允許本純設計審查不以A checker成功為前置。A獨立技術驗收仍未完成；原reader0419的17/17與verify-input是歷史自驗。2037檔一致是總控完整性核對，不是raw解析證明。兩次configure均因owned vctip超原2秒gate停止，cleanup verified／active0；本包configure、compiler／guard／checker修補及build均0。原A的2560/2475/2282、193/85、136/51/6、6 exploratory、群聚／matching只作歷史來源结論，沒有新checker背書。

## 1. 來源順序與識別性

本包靜態核對的是R3實際frozen來源，不是當前main同名檔。定位鏈為A [input-manifest-v2](../measurements/runtime-decision-skip-a/input-manifest-v2.json)／[source-binary-binding-v2](../measurements/runtime-decision-skip-a/source-binary-binding-v2.json)→R3原與controller兩套ledger→R3 source-binding及R1 B0 source-binding／core。選定輸入的檔長、exact SHA在[本包input-binding](../measurements/runtime-decision-skip-a/controller-review-b-contract-20261004/input-binding.json)；只核這些reference與少量例列，未重算原1710來源、完整raw或報表。

| frozen來源與exact SHA-256 | 本包讀取行號／用途 |
|---|---|
| R3 `source-snapshot/apps/main.cpp`：`1d4d668bfbf38dea73393cfd83ce3a0e705c60f63cf901f1e2a2c57f0c32ed9f` | 73–74 adapter；105–139 capture／recognition／publication；155–165 selection／accept；180–181、196 raw schema |
| R1 B0 `src/game.cpp`：`ffe8141727f16db57292e7490017b3b4c70b4ab0f749884047d0ba8b24e10c62` | 576 reset不清sequence；634–640 observer sequence／frame；1774–1809 decision JSON |
| R1 B0 `include/pas/game.hpp`：`5b70446fffa22e646758920069ee5f2d4c5f4752d0a55954df708674024781e7` | 64–75 snapshot；176私有sequence |
| R1 B0 `src/core.cpp`：`7b6ae89deca1501da2cd5a777c28c13cf3f086add1eb816070fbd325442952b3` | 15–30 QPC整數轉ns；45–129 capture mailbox／published_ns |
| R1 B0 `src/game_session.cpp`：`bbe00dff49e14bb15e14ea27698300c8ccdb6ce55cbe3c91ef912bf40070390f` | 129–134 accept／poll邊界 |
| R3 `source-snapshot/apps/session_archive.cpp`：`4770c8fe95d5443c0d2bf1a3fcd413d0b9675030536032735d86ac7165c5d76c` | 26–38 admission／push／notify；73–75 writer mutex；117–128 serialize／write |
| R3 `source-snapshot/apps/contract.hpp`：`76b2ddfa3fd378f745cb4aa29a71b8f328eff08b67b7e3cd3ca1c4f8e48592e7` | 11–18容量；54–65全分母與舊anchor窗口 |

`main.cpp`的序列為：capture LatestFrame publication→lease讀取→process→外層recognition_complete→lease.reset→decision mutex內pointer／latest_decision更新→unlock／notify。action在同decision mutex取得最新const snapshot，unlock後建構decision_json、兩次enqueue，再讀owner_start→game.accept→owner_end→drain／poll／drain／wait。這只保證程序順序，不保證兩步緊鄰的elapsed很小。

| 對象 | 現存量測／由source可推界限 | 缺失、不可推導／證據層級 |
|---|---|---|
| capture publication | `LatestFrame::publish`的pool／latest mutex與decision mutex不同。core113–115在capture pointer swap後、unlock前讀published_ns；R3只在consumed lease保存它。publish_cost包整次publish | published_ns不是decision P，也不是render time；未consumed的0為未量。順序：**本包靜態核對**；全raw數量：歷史驗收範圍 |
| recognition完成 R | R3 main137在process／allocation／可選slow之後讀clock，138才lease.reset。不是observer內部欄位原讀值（外層會覆寫） | R不是mutex publication；同一perception worker給`P_i∈[R_i,下一consumed recognition_start]`，末份上界缺失。界限：**本包靜態核對**；原逐列計算：**歷史自驗、獨立未驗** |
| decision publication P | main139更新const snapshot pointer及latest_decision，容量1；同mutex使publisher／selector的操作可線性排序 | 實際P、unlock可見性時刻、mutex wait均未量；P不等於notify。P→replacement是抽象mailbox生命期，不宣稱整段無lock阻擋 |
| selection S與accept入口 A | main156 copy shared_ptr是selection；main158–159先做JSON及兩enqueue。main160的owner_start/end是呼叫前／返回後的clock sample | `S_p∈[max(R_p,前owner_end),owner_start_p]`僅正常單action連續流程可用。owner_start不是S，也不是精確callee入口；真入口`A_p∈[owner_start_p,owner_end_p]`。**本包靜態核對**；精確時間**未驗** |
| decision sequence vs frame | RGB observer638自增sequence，639保留f.sequence；reset576不清sequence。adapter74刻意令sequence=source attempt。game_decision JSON1800／1803分開輸出 | main157 current_attempt是decision sequence；lifecycle159的source_frame卻填seen，不能信欄名。RGB consumer skip後二者不同。映射全表：**歷史自驗、獨立未驗**；來源及例列：**本包靜態核對** |
| enqueue／writer | main114計時在JSON參數建好後才開始，涵蓋admission.dump、archive push.dump／mutex等；writer有並行serialize/write duration | 無個別绝對時刻，不能拆純JSON CPU／OS／lock／wake；writer不能加到action critical path。**本包靜態核對**；成本與noise **未驗** |

少量原列定位在[raw-examples](../measurements/runtime-decision-skip-a/controller-review-b-contract-20261004/raw-examples.json)：owner frames行3–5的skip4；RGB frames行35未consumed而published_ns=0，行36已consumed；source-anchor-map行35為decision35→source36。decision-skips行1為RGB1218，行2為owner4。兩個衍生檔SHA分別為`0e0db39a88e30959696b6157014e2470cd69b2fe8d58b2ad0b2b193d6d2fd369`及`ec9cbe7b7343941ab6c9aa65754d34d951514ab61965140b4532d3e7fa4204c5`。它們示範欄位語義，不能當193列重算或新的raw驗收。

## 2. 命題、判定域與全分母

把含混的「主要」固定成**份數嚴格多於50%**，不是時間總和、不是90%成本gate。對單一凍結run、同clock／epoch／generation／geometry區段，publication i的抽象residency為`L_i=[P_i,P_(i+1))`。skip是**有publication、沒有successful selection**；必須與「selected但accept未返回／fault」分開。對每份skip，p為其P之前最後一次successful selection，服務區間為`V_p=[S_p,A_p)`，A是真game.accept呼叫入口。

H：在短residency skips中，超過一半的**整個**L落在V。短固定`duration≤4,000,000ns`；這是本包預聲明的新描述分層，源於owner8ms cadence的一半，不把原A recognition-gap cluster當residency，也不修改任何舊判準。各RGB run用同4ms門檻另報、不與owner池化。另報全部skips的覆蓋比例與duration分布，避免短分層掩蓋長skip。H只關於instrumented、typed workload；不外推到live或77Miss。

必報完整attempt/capture-published/pool-drop/consumed、decision-publication、selected、accept-returned／failed／unknown、consumer-skip、decision-skip、terminal-unselected及trace缺口。`capture published&&!consumed`是consumer skip，沒有decision L，不能算decision skip或從H分母悄悄消失。consumed但未publication要另列aborted-before-publication；現存consumed欄不能在失敗run自動等同P。只有selection與accept同時記錄才可辨明selected-but-fault。

全窗、warmup1–256、measurement257–2560及四576 blocks固定保留；以source frame分窗，decision ordinal只排序。跨窗相鄰P／S可引用但只由當份source frame計一次；不能在每個block邊界重設狀態製造censor。phase／targets作描述層，不作因果控制。原R3的owned≥90%與noise門檻維持不變；B統計不是取得成本資格的方法。

## 3. 最小schema與同步契約草案（未實作）

選用**每份publication兩次、每次successful selection兩次新增clock reads**。加上原每次accept已有owner_start/end兩次，新增數為`2*N_publication+2*N_selected`，不是「加兩個timestamp就精確」。沒有successful selection的poll不新增clock read。另有固定header/footer與epoch控制資料；若要給reset精確時間須另列boundary reads，這個最小版本只留censor。

| 新側錄 | 必要欄位與定義 |
|---|---|
| run header | schema與semantics版本；run id／source、binary、core、config SHA；clock_domain=host_qpc_ns、同HostClock identity／frequency／轉換版本；預期attempt上限／windows；generator的sequence語義；trace與原diagnostic ON/OFF各自狀態 |
| publication | run／epoch／generation／geometry、decision_sequence、source_frame_sequence；同mutex遞增且無缺的publication_ordinal；`p_before_ns,p_after_ns`；完成／fault狀態。source binding在所有publication保存，包含從未selected者 |
| selection | selection_ordinal、**實際copy的**publication_ordinal與完整key；`s_before_ns,s_after_ns`；原owner_start/end sample及accept outcome（returned／failed／unknown／not-entered）。選取成功不等於遊戲採納 |
| footer | 每thread attempted／completed records、overflow／drop／fault、first／last keys、termination reason、epoch transitions、worker joined／archive完結狀態、檔bytes／rows／SHA。缺欄值null＋reason，不能補0 |

P的線性化點是同decision mutex內把新snapshot及其identity binding替換mailbox的邏輯操作；unlock才允許其它thread看到它，兩者不混稱。在既有mutex取得後，相鄰讀p_before→pointer/binding swap→p_after，**兩讀都在同mutex內**；不在其間建構JSON、allocate、enqueue或wait。固定小metadata的copy可能延長hold，不能假定零成本。p_after存入同mutex保護的envelope／producer側錄，unlock前完成；const DecisionSnapshot本體在publication後不修改。selector在同mutex核條件成功後，相鄰讀s_before→copy完整const snapshot/envelope key→s_after，再unlock。錯配metadata／snapshot、mutate共享snapshot、寫入後未同步就被reader看到，都是invalid。

同mutex提供操作次序；可用publication／selection的operation ordinal補相同timestamp的先後。若另設ordinal，必須在既有mutex以固定整數更新，禁止加新鎖。envelope生命期與snapshot一同pin住，不能事後讀latest的metadata來綁舊s。trace各thread自寫固定slot，主thread只在join後读；不共享可變row、不把診斷資料回饋策略。

讀值包住的是操作：`P∈[p_before,p_after]`、`S∈[s_before,s_after]`。單次前讀只有下界，後讀只有上界；鎖外前後讀还包lock wait／競爭，容易失去區分力。單讀放在assignment後也不能等同assignment瞬間；因此兩對是最小**有用的有界版本**。bounds寬度包含clock呼叫、固定copy、被搶佔等，不能標為lock成本。編譯器／clock adapter的順序、const資料同步及非遞減clock須未來獨立驗證；新增等待或鎖不因「量測」而當然正確。

沿用owner_start/end作`A∈[owner_start,owner_end]`，避免加callee-entry插樁。這是故意保留的不確定性：完整落在owner_start之前者可確定pre-accept；owner_start到owner_end附近可能Unknown。只有這個帶寬阻止事前決策才另案考慮callee入口讀值；本提案不加它，也不把end-start叫entry延遲。S後的JSON／兩enqueue仍照原路徑，不能移除以求通過。

時間計算使用有界整數ns，overflow拒絕；frequency不是已測clock precision。core的整數轉換會量化，同timestamp不能推出同時或零成本；QPC與其它clock domain不混算。header記轉換及已知解析度／校準限制，若無法給跨thread一致時間界限則比例invalid。量化邊界／與門檻相等又無足夠解析度時保留Unknown，不用中點作精確時刻。

## 4. 界限分類、缺口及可否證規則

先核身份與完整性，再分類，不能把壞trace化為好比例。令P_i bounds=[l,u]、replacement=[l',u']，則duration bounds=[max(0,l'−u),u'−l]；負上界／逆序是invalid，不以clamp修掉。S=[s_l,s_u]、A=[a_l,a_u]。完整覆蓋的充分條件是`l≥s_u 且 u'≤a_l`；source已證selection在該publication前，仍保存這項次序約束。確定非完整覆蓋可由`u<s_l 或 l'>a_u`等嚴格界限得出；其餘保留Unknown。partial只在每個可行時間排列都有正長度相交、也都有一段在V外時標記；可能partial／可能full不混算。相等邊界须按half-open、ordinal及解析度核對。

| 情況 | 全分母處理／停止條件 |
|---|---|
| publication被overwrite且從未owned | 依所有P與所有S直接join，進skip分母；不得靠game_decision archive只記owned者重建全部P |
| first skip無前S；末P無replacement；停止／epoch切換 | 左／右censored，保留為Unknown與數量。跨epoch不能當相鄰replacement，不用final capture／recognition時刻補界限 |
| duration跨4ms；partial；同timestamp | 分別保留membership Unknown／確定非完整／解析度Unknown；不報零延遲、不挑最窄bounds |
| sequence跳號 | pub ordinal必連續。RGB decision sequence依observer完整處理路徑核對；owner adapter可因consumer skip跳source/decision sequence，不能誤判trace drop。缺expected publication、source binding或終止證據時invalid／Unknown，不用sequence差直接算skip |
| duplicate／reverse／epoch reset／frame錯配 | 同key不允許多次publication或selection；逆序／重複為invalid。重設需明確transition及clock連續證據；不能跨epoch join。合法重設由header semantics辨認，不從相同數字猜 |
| 診斷drop／overflow／truncated LF／SHA或count失配 | 全分母無法建立：run invalid，留失敗與counts；零drop須由獨立footer與slot上限核對，缺footer不是零。不得只刪壞列再算pass |
| selected但accept exception／未進入／release unknown | 與未selected分開；沒有A上界則相關服務區間Unknown。run故障率另有全部run分母，不拿正常success subset代替全部 |

对每個run全部unselected publications，分成確定long、確定short且full(K)、確定short且非full(F；含partial／disjoint)、其餘U（含可能short、censor及cover Unknown）。明列U的membership／time／boundary原因與交集，不能重複計數。短群比例保守界為`[K/(K+F+U),(K+U)/(K+F+U)]`：將可能短者全納入上下極端，可能偏寬但不會靠排除未知製造結論。確定long仍在全部skip帳與另一比例分母。

**上界≤50%就否證H；下界>50%只支持該run的區間命題；中間或空分母為未識別。** 用整數比較2*(K+U)≤K+F+U及2*K>K+F+U，避免浮點／降低門檻。N_short_possible=0時ratio=null，叫無判定域，不叫H通過。各run／windows／blocks完整報，不平均／池化成因果結論；未獨立的相邻skips不能當独立統計樣本。邊界例是規格oracle，沒有執行過tests。

## 5. 手核整數例與独立期望（設計待驗）

[fixture-draft.json](../measurements/runtime-decision-skip-a/controller-review-b-contract-20261004/fixture-draft.json)用理想fake-clock整數ns與toy short門檻40ns（正式提案4,000,000ns，分開記錄）。共同S=[10,12]、A=[50,60]。期望由下列具體可行排列／算術手核，沒有用待寫分類函式產生expected；不能自封tests passed。

| 例 | P／replacement bounds | 手核期望 |
|---|---|---|
| full真陽性 | [20,21]／[30,31] | 起點至少20>12；終點至多31<50；duration9–11，K=1 |
| late反例 | [70,71]／[80,81] | A最晚60，起點最早70；必disjoint；duration9–11，F=1 |
| 必partial反例 | [45,46]／[65,66] | 起點必小於A、終點必大於A；duration19–21，F=1 |
| entry不可識別 | [45,46]／[54,55] | A=50時partial；A=60時full；同記錄容許兩者，U=1。兩對新clock仍不能拆callee-entry帶寬 |
| 同timestamp | [20,20]／[20,20]且ordinal先後 | 抽象次序可知，物理duration不能叫0；解析度Unknown，U=1 |
| 首／末censor | 前S缺失／replacement缺失 | 各U=1，不能把缺值補0或删列 |
| 確定long | [20,21]／[90,91] | duration69–71>40，long=1；仍保留全部skip |

小全分母例：4份short skips有1 full＋2 late＋1 partial，K/F/U=1/3/0，上界25%≤50%，否證H；另一組1 full＋1 late＋2 Unknown為25–75%，未識別。consumer fixture有capture frames1、2、3全發布，2未consumed，RGB decision1→frame1、decision2→frame3全部selected：consumer skip1、decision skip0，H=null。owner adapter同情況decision序號1→3是合法source gap。另留duplicate／reverse／missing pub ordinal／wrong binding／trace overflow負例期望invalid，不與Unknown正常列混用。

## 6. Observer effect、容量與未來驗證界線

clock本身及clock前後位置、mutex hold變長、shared_ptr refcount／key copy、固定側錄slot寫入與cache sharing都可能改變P/S分布。p_after後unlock還有固定小工作，不能將抽象P當無阻塞availability。原action已含decision_json建構、admission dump、archive mutex內dump／push、兩次notify，以及writer持同archive mutex的pop/dump；新P/S事件若再走enqueue就增加兩次以上序列化／wakeup，可能製造要測的skip。因此最小草案**不在熱路徑enqueue新P/S JSON**：perception／action各自寫預配置固定陣列，join後序列化；所有原診斷保留。沒有新鎖、runtime writer、thread或wait。

提案上限沿2560attempt：P與S各2560 slots、每slot≤128B（未來static_assert／型別核對），共655360B；header/footer／counter與控制區另≤256KiB，新增記憶體≤1MiB。超slot／clock fault即記incomplete並結束該有限離線試驗，不擴vector或等待writer；診斷單向，不把overflow變觸控選策。P/S各row含LF≤1024B，最多5120rows=5242880B；metadata≤1MiB，全部新trace檔≤8MiB/run；原R3 80MiB/run另列，所以未來run總額最多88MiB。這些是設計預算，沒有實測sizeof／RSS／CPU或新檔案。

未來若總控另授權，必須先凍結下列階段；本頁不開放任何執行：

1. **一次有界冷實作及獨立契約核對**：正式自有邏輯仍C++20。fake-clock涵蓋上列手核oracle、同timestamp／censor／epoch／capacity與identity race／snapshot一致性；新舊public-output bridge至少沿已有512 RGB＋512 owner inputs，保留所有輸出／first divergence、accept、receipt、cancel、release與exit contacts。trace ON/OFF同fake-clock應同策略輸出；診斷ON/OFF的public比較只投影事前定義的diagnostic欄，不能忽略觸控／故障差異。bridge不是OS負載等價或live證明。
2. **最小observer-effect檢查**：原完整diagnostic狀態D與新trace T分別ON/OFF，四格都需要；D-OFF僅隔離離線副本，不等於採用刪診斷。每格至少2次相同策略A/A，合計最多8runs×2560attempts；跨格順序事前平衡，不能照結果補owner2–4或湊90%。T-OFF沒有P/S不能算H，但可量完整分母／既有metric差異。這個最小screening不能替代既有完整AA noise及B0/B1成本protocol。
3. **預算／停點**：8runs raw最多704MiB；新橋接／fixtures／reports／logs合計≤64MiB，campaign追加提案≤768MiB，build另有預留與free≥5GiB。必須重核既有campaign+prior8GiB、R3 2GiB與aggregate10GiB剩餘額度並取得新包決策；不向本A review16MiB／batch64MiB塞run，也不以新root重置舊帳。本包仍受1MiB新增限制。一次cold方案或獨立checker不可完成就交失敗停止，不重開compiler／guard試誤、放寬2秒gate或把A建置修復作B/O無限前置。

實測必保留環境／Release source-binary-core-profile／clock／ON-OFF／背景負載，逐run全n、p50/p95/p99/max、p95−p5 jitter、fault／failed／unknown／drop及bound widths；完整attempt還含未consumed／selected者。新增P/S duration有bounds時分別報上下界分布及missing n，不能拿中點當精確latency；accept、capture→owner等原metric各按適用分母報，end-to-end直接量，不相加stage p99。兩次重複不是獨立skip樣本，也不足簽AA noise。

任何identity／order／clock／drop缺口使run invalid即停；public策略／release守門差異未解即停。8runs預算內若無可識別H、trace ON造成既有gate或尾端負結果，或H被否證，就停止此量測線，不擴插樁／輪次／門檻搜尋。即使H得到支持，只有區間定位；JSON純CPU、lock wait、OS搶佔、wake原因仍Unknown。需另有具體工程收益才考慮第二個因果實驗，不能自動ABBA或候選成本。

## 7. 決策依賴與交接

值得最小實作的理由是source本身確認有未量P/S及selection後兩enqueue；不依賴把原6 exploratory或193份比例補簽通過。新完整trace若大部分短residency在前accept入口之後、跨界或其比例保守上界≤50%，會否證H並取消優先研究accept前共同診斷路徑的理由。支持H只允許考慮這條路徑是否值得另因果研究；本包不推薦任何動作或診斷行為改碼。

A缺獨立checker降低歷史群聚／matching／136/51/6數字的信心，必須保持其證據等級；未來B可用新raw與獨立hand-oracle驗證建立自己的結論，不能拿A數字代驗新trace。原raw的P/S缺口無法由更多reader重放修復，現在已到識別性停點；本包停止讀例列後不追加解析。若缺實際工程決策、授權、容量或有限驗證能力，保留草案並停線。

HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`／唯一worktree。193因果Unknown、X12not-ready、noise未評估、B1成本Unknown、suppression OFF、14lost利弊Unknown、C36h77Miss、main50 donor/live0不變。X10d-O未開始，且不依賴193全歸因。交[handoff](RUNTIME_DECISION_SKIP_B_HANDOFF_20261004.md)與機讀final receipt供總控驗收後停止；本包設計通過也不授權implementation／cost／live。
