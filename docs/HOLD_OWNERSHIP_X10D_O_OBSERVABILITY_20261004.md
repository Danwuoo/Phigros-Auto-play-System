# X10d-O 工作包4R：可觀測分界、時序關聯與 contact 契約

2026-10-04，Asia/Taipei。**建議只將「當前端部／分界＋有界分界延續」做成下一包可否證的觀測介面；不再輸出 BCC-v1 的唯一 physical owner。** 真圖可見的差異足以提出更精確的 current-support 與 incoming 機會契約，尚不足簽收 physical 歸屬或任何觸控候選。此建議是一個設計，不是實作授權。本包完成後停止交總控獨立驗收。

最新範圍是 [O 總控驗收的工作包4R](HOLD_OWNERSHIP_X10D_O_CONTROLLER_ACCEPTANCE_20261004.md)，優先於總帳內歷史的「O 尚未開始」。原 [O protocol](HOLD_OWNERSHIP_X10D_O_PROTOCOL_20261004.md)、[負結果](HOLD_OWNERSHIP_X10D_O_RESULT_20261004.md)、C++ sources/tests、封存報告均保留。C36h tint1 仍為 behavioural/experimental baseline；main50 comparison/donor、live0，suppression OFF，X10d-P 不混入，X12 成本 not-ready。Chapter Legacy 全曲解鎖 IN／完整 IN Miss=0 未達；HD 只作解鎖／回歸、P/G/B 照報、無 AP 前置。

機讀 [設計與 vectors](HOLD_OWNERSHIP_X10D_O_OBSERVABILITY_DESIGN_20261004.json) 是本文件的具體輸入／預期限制。新 evidence 根為 `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-observability`；檔案只新增，沒有產品／分析演算法、binary、build 或策略執行。

## 1. 核過的來源及本輪界線

接手逐 SHA／已宣告 bytes 核對原總控 integrity 的 **1322 distinct 檔，0 不符**，另核 controller ledger 的檔案及外部驗收頁。保護清單擴為 **1762 distinct 檔**：含原 450 Git 路徑、原核對範圍、整個 O batch/controller-review 與 `out/x10d-o`。HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、index SHA `260c49037641b2c8c44f40e828491778779223193d91cb7cd824c8534ef7e9e1` 保留。這不是全部 ignored raw、SDK/STL 或其他 out 的逐檔再驗；沒有修改或刪除它們。

核對 [controller receipt](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o/controller-review/controller-receipt.json) 及原 artifact ledger，不重跑 O：歷史總控 Release207/207、reference3pass8fail、BCC-v1 10pass1fail、owner8/8、196PNG reader 逐 byte 一致。本輪 **tests/build/ASan/full replay/runtime/cost/stress/live 全部未執行**；沒有 emulator、ADB、真觸控、模型、goal、automation、commit/push 或自行續派。

開工 free=203128758272B，大於16MiB＋5GiB。前次已 settled aggregate=8274413909B；新維護 script 首寫6359B後開工實核8274420268B。帳本保留 campaign8229024439B、prior45307809B、O外部72115B、O總控頁9546B，加本包根及三份外部 docs/JSON；不把新根歸零或只算 campaign。結束精確值以本根 `final-receipt.json`／`artifact-ledger.json` 為準；12MiB 開發含外部檔、4MiB 總控預留、16MiB 新根總額與原8GiB aggregate 同時約束。此處 campaign 長度沿既有保守 entry sum，沒有新量 NTFS allocated bytes。

## 2. 原11項的 input／oracle 審查

固定 source `research/x10d_o/contract_tests.cpp` SHA256 `c9850bea7e81139d6e4e07c8d992164fa2956036ad726f9a31beadb575dbd0e8`。共用 input：640×640 RGB888 黑底、frame sequence2、epoch/generation/geometry1、capture50ms；Hold中心(320,575)、寬140、深300、Note切向(1,0)、rails/head_on_line=true；line7當前量測、中心(320,575)、切向(1,0)；anchor1同context、observed40ms。renderer填 RGB(40,190,255)，不是遊戲 renderer。

| 原 case | 可觀測 input／變體 | 原 oracle 與需另存的限制 |
|---|---|---|
| SolidSupportPositive | 一個完整填色矩形 | support=true、depth≥290、owner1；support正例可留，**owner1無法與下述同輸入期待共存** |
| GapCannotBeBorrowed | 下段front575/depth80、上段front400/depth125，中間背景gap | depth≤85；這是 extent 不能借用，原測試沒有要求 owner=false，不可改述成原 physical oracle |
| AbsentBodyIsUnknown | 黑底，幾何／flags不變 | support=false、無owner；flags不是 pixels |
| OneSideAbsentIsUnknown | y270..579、x250..279清黑，破壞一側支持 | support=false、無owner；另一側、歷史支持不能補成當前雙側 |
| SecondAnchorIsUnknown | 同圖，兩份除id1/2外完全相同anchor | 無owner；軟體別名重複不等於兩個physical物件，需先分開該概念 |
| TouchingTailIncomingIsUnknown | 175px及125px兩段填色；union與Solid逐byte相同，其餘inputs亦相同 | negative無owner，又要求兩world owner/probes相同；與Solid owner1矛盾。**原測試與失敗不改** |
| RotatingAndTranslatingSupportUsesCurrentNotePose | k0..7、角度.05k、中心(300+3k,500−2k)、深200；各次anchor也是當次pose | support／owner、depth≥190；沒有輸入前幀pixels或跨角度舊anchor，並非active旋轉追蹤驗收 |
| ThinTapNeighborAndOtherDirectionCannotOwn | tap高度8、anchor沿切向偏200、或anchor切向(0,1) | 無owner；不代表thin Tap應被取消，Tap新Down另列正例 |
| LateAlignmentDoesNotConflateNoteAndLineAxes | 單幀Note角.25rad、line水平、深200、anchor同Note pose | support／owner；沒有遠→近 rollout，note→line/root交caller，不能叫late-alignment已驗 |
| StaleContextAndCapacityRemainUnknown | anchor過90ms／未來、epoch或geometry不符、line invalid、129anchors | 無owner；原允許90ms的邊界不是新增 physical真值 |
| InvalidFrameAndNonFiniteGeometryCannotClaim | RGB短1byte或width=Inf | 無owner；invalid輸入不能有任何動作資格 |

確定性相同输入必須有相同輸出；隨機方法亦須有相同輸出分布，不能用私藏world label、case名稱、runtime ID或anchor ID分叉。新 V00 原樣引用此 current-input pair；V01另描述全有界近期影像亦相同的控制，兩者均不要求唯一physical owner。新的可判別正例需實際增加可見端部或不同近期pixels，不是將anchor命名成gold，也不是重寫這11項讓原結果變綠。

## 3. 直接目視：52張、四組，新增PNG0

先保存 [review-selection.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-observability/review-selection.json) 的ordinal/source_frame/原capture_complete/pixels_ready/source timestamp/SHA及理由，再直接用原PNG目視：H2468–2488共21張、K2865–2889共25張、A3496–3500共5張、D6214一張。H/K為完整连续ordinal短序列，沒有從代表單張推前後。所有圖都在O原七窗196張內，额外相鄰額度0/64、total distinct196/260；不複製、裁切或產生PNG。

以下位置是AI肉眼約值，未計算pixel masks、人工標註或precision/recall。直接目視≠human gold；新 physical human gold=0。表內可見事實與proposed關聯、unknown分列。

| 序列 | 可見事實 | proposed／unknown |
|---|---|---|
| H2468–2470 | x約280高body上端截出畫面、雙側white/cyan rails。下端由約y552到y584再到y600；主線從水平變小斜角。另一條斜線穿過附近 | 可見body跨圖延續為proposed；軟體2469已Down不證遊戲內同physical Hold已採納 |
| H2471–2476 | 黃色圓／方／菱形effect在下端展開。外側rails與上方fill仍可見；內部／下端被透明effect疊住。2474–75布局近似，不能以不同序號當兩次獨立視覺確認 | 未見可靠第二個bilateral end/cap；「539為fragment」合理但proposed。無seam不證只有一個physical物件 |
| H2477–2488 | effect重疊增加，body外側持續，上端仍出畫面；下端約y590緩移至y585。effect局部色塊不能當完整獨立前緣 | 同contact當前支持Move可提出正例需求；各幀是否有足夠未遮擋接觸區需新介面驗，不能以本AI描述先簽Move位置 |
| K2865–2873 | x約783兩段body有兩组端部：2865下段約352..446、上段119..290，中間约62px背景gap。x495亦有分離body。前緣总体下移，2870局部呈回退 | 此為實際可觀測分界，支持兩個observed regions及獨立incoming假說；不能由ordinal假設單調運動，也不能把regions直接命名682/684 physical ID |
| K2874–2879 | 下段front到y576線，body可見高度縮短，黃色effect遮下段；上段front由約427到503，上段與下段曾保持可見分界 | 需同時保存「原接觸區piece」與「上方有自己的端部」，不能用最大藍extent合併。下段末端究竟是真tail／完結仍unknown |
| K2880–2883 | x783上段front約518→533→548→563，線約576；2880–81原下段body不再清楚可辨，effect圈仍在。2881上段與線約42px的背景間隙；2883仍約13px | effect不證持續body。682在2881 front576/depth214、2883 hit559是軟體claim；不能借上段fill刷新原held身份。舊physical Hold是否ended／遮住unknown |
| K2884–2889 | x783上段front與線重疊，body向線收縮，effect增加；x495後續body仍在接近；上方另有傾斜body入鏡 | 支持完整可見新front的機會需保留；不由原column anchor吞掉。physical採納、exact tail完成、684是否應Down仍unknown |
| A3496–3500／D6214 | A可見斜body與斜line、鄰近vertical lines；短窗有位置／角度改變、effect疊下端。D為horizontal/vertical lines、不同朝向thin notes | A不是完整head→active旋轉→tail；D不是Hold旋轉序列。無完整late-alignment正例 |

### 時鐘及軟體狀態不能互換

原O packet綁原PNG時鐘；X10c/X9 traces是同源全prefix重播的 **rebased fake-clock**。例如H2478原capture23126393917400ns、replaycapture43357925700ns；K2881原23133341705500ns、replay50305713800ns。兩者都屬主機尺度但原點不同；只能各自作dt，不能相減當延遲。source timestamp保持原domain，source render age unknown。K2882原圖確可目視，但原trace consumed=false／scene null；不補造其observer或owner證據。

直接只讀X10c baseline-on trace及X9 c36h-on trace：H2469 local521/intent156/contact1已Down；2470仍同contact、cursor3/prefix1且Move到(278.8094,599.1934)。H2478另539/intent160/contact0 Down，2479該身份取消，而521/contact1持續。X9對應三幀state與此一致。K2878 local682/intent194有contact0；2880該target缺席contact仍在；2881 claim front576/depth214；2883同contact Move到(781.5,559)，2884回576，2885另684/intent197/contact2 Down，2887該身份消失。**這些是軟體狀態，未升為physical關聯、Miss或新設計oracle。** 完整精確來源引用及SHA見設計JSON/source索引；本包沒有新full-prefix跑法。

### 覆蓋與缺口

H2469→2470確有可見line小角度變化，且原軟體contact1活動；這比原十張代表圖更強，但仍缺physical採納、完整大角度旋轉／反轉中的body支持與tail分開驗收。A五張亦只補斜向短窗。K頂部斜body並未在所看片段抵達線，不能填成「遠處未對齊→近線對齊」金標。旋轉中Hold、晚對齊、雙側、不同線交叉、真fragment與獨立小Hold外觀相同、全遮擋後恢復及真tail完成均須另列未驗。沒有為補這些缺口多取64張、跑模型或要求新實戰。

## 4. 最多兩種新增可觀測資訊的審查

**A：當前端部／分界與接觸區支持。** 在Note自身切向/法向描述可見雙側rails、fill、兩侧終止／bevel cap、背景gap與遮擋範圍；在所關聯當前line附近另列contact-region支持。未知端部不命名head/tail；leading僅表示近期相對接近中的端部。K的兩组cap與gap是真圖支持；H的內部色塊缺可靠自己的雙側cap。O掃描從candidate.center後方2px起，深度≥16即有支持，未驗真正的line/contact區，所以「遠處上段支持」與「原接觸區支持」必须分列。透明effect、背景線、tap、兩body剛好無縫、灰區低對比或cap被裁切皆會失效。不能把gap一律命名兩physical Hold：遮擋也可形成gap；需保存其可見背景／effect的不確定性。

**B：有界已觀測端部／分界的延續。** 每個region保存最多6個實際量測／90ms的frame key、部位位置、局部支持及可見性，不保存整首或未來影像；原錄圖可作離線說明，runtime不能讀檔／歌名／進度／舊按鍵。保存兩個分離piece的時序link、最後可見分界時間與至多2個可行幾何對應；當下暫時接近／effect遮住時，不能將最近兩piece合成一個exclusive owner。原anchor只是摘要geometry／flags，沒有這份端部來源與排他反證。**有界影像／摘要也可能完全相同**（V01／V19），因此B增加資訊機會，不給physical唯一保證。K全25張涵蓋約405ms，runtime每次只能用尾部≤90ms；不能讓2865的gap永久續租到2889。

兩資訊不新增第三種模型、texture classifier、flow或外觀家族。具體幾何門檻為未校準冷設計proposal，在新JSON先列，不依H/K調歌別參數。短時序是觀測關聯，prediction位置另欄；插補不能更新pixels timestamp或支援Move。使用Note自身姿態提取部位，line局部(s,d)處理相對接近；Note朝向、實際位移向量、line法向各存，遠處不要求同法向。線旋轉時以當前線／body交會支持更新同contact位置，不剛體旋轉舊觸點。

## 5. 唯一建議設計：有界可見分界介面（設計名 BVI-1）

BVI-1只輸出四層，禁止 `owner=anchor.id`：

1. **current support**：body局部支持、contact-zone支持、independent end支持、gap/occlusion/invalid，各含同幀pixel來源與有限區域；不知道就明示。
2. **temporal relation**：continued observed piece／recently separated piece／contained feature／ambiguous，附至多2個link與實際sample年齡。contained feature只說像素feature位於共同outer rails內，不能斷言physical fragment。
3. **physical identity**：可為proposed假說集合或unknown，沒有human／遊戲gold。唯一可行視覺link只是proposed；在世界相同觀測下私藏不同identity時不要求resolver猜中。
4. **contact constraints**：same-contact Move候選、independent new-demand機會、no-current-support、no-Down及release責任；接入、續接、結束分開。這些是下一包冷oracle限制，不是runtime已採用。

同contact正例需有實際像素支持的既有接入／近期延續、當前body與所關聯當前line的接觸區、無可見分離／replacement矛盾、單一可行觀測link、有效context/期限，以及owner已執行prefix。**原anchor幾何單獨不合格**。既有contact只供生命週期約束，不當physical gold或模仿舊按鍵策略。新incoming正例需自身當前雙側end／body與局部新鮮關聯，不能因同column有held anchor直接取消；至多五指仍獨立檢查。局部contact支持≠完整獨立新front；H沒有第二cap時不給新Down，但其fragment身份仍unknown。

K2881的當前line區無可靠body支持應交明確 `no_current_contact_support`／replacement alternatives，不能將矛盾藏成一般missing並沿舊rootDown；不混X10d-P hook或復活suppression。new-demand即使資格不足也必須保留region／理由與机会分母，不刪raw/direct front／thin Tap取得少Down。若新front與舊tail在完全同像素下合併，續接/新Down皆按同輸入一致限制保守；這個信息缺口不可能靠ID解決。

無當前支持時不Move、不刷新evidence；按既有Hold≤60ms missing上限安全釋放，明確矛盾可以更早撤銷。無root本身不取消有當前支持的活動Hold。未知Down不重試，留release責任；completed／已Up／非零prefix不能新Down或復活。兩份新鮮可見tail支持才提出正常Up；幾何過線、effect消失、gap或負時間root都不證完成。capacity/context invalid撤銷資格，不靜默丟最難incoming。

### 硬上限與失效邊界（提案，未量成本）

最多16當前lines、128 regions、每region6×256B sample descriptors、最多2 links及512B當前descriptor；links metadata每region128B、5 contacts各512B。line pose16×6×128B，所有metadata含frame refs固定總上限1MiB；不得隨Hold長度累積。當前frame僅借LatestFrame lease、不新增past RGB ring；最多一張解碼是本包viewer能力，不是提出新runtime reader。若摘要不足保存可否證端部，下一包應停並交缺口，不能暗加影像buffer。

每region當前probe／候選提取仍最多4096×12次pixel讀、128regions總6291456，這是工程硬界而非可接受成本；來源valid/ROI完整性及budget先驗。history每份≤90ms、相鄰有效量測gap≤40ms、至少3份不同pixel證據跨≥30ms才建立可用link；duplicate RGB／相同descriptor不算新端部確認。有效期不因prediction、grace或分界假設而延長；line/source/epoch/generation/geometry改變、clock非遞增/未來、跳變、對應tie、兩line候選、端部遮擋、overflow皆降為unknown/invalid。新JSON的4px誤差界、≥12px可見gap等只是合成控制的proposal，不是遊戲閾值已定。

### 獨立oracle及可否證交付

JSON V00–V19描述RGB declarative recipes／typed observations與context/clock/contact state；不產生pixels或執行它們。下一包的renderer依座標填形狀，oracle由預先列的support點集合／分界區間／幾何運動及scheduler prefix狀態決定，不能讀候選helper結果填期待。private physical label只在合成世界帳內，從resolver輸入隱去；真圖的physical期待仍null。輸出正例至少包括當前支持、可見分離incoming、同contact current-supported Move、thin Tap新Down；「全部unknown」不通過。

V00/01保留不可辨pair；V02/03/04分開續接、分離incoming、contained feature；V05/06遮擋／缺側；V07合併時保留分界假說但不造當前cap；V08多線；V09 thin Tap；V10旋轉中Hold同contact Move；V11远處不對齊近線才對齊；V12/13 unknown/completed；V14返回；V15 tail；V16/17 context/capacity/gap；V18 ID重編不改幾何答案；V19可見端部也可能由不同physical世界產生的countermodel，禁止把BVI升成唯一physical分類器。

下一包最小工作僅為C++20隔離current part/region descriptor與有限link契約、獨立renderer/oracle及synthetic/fake-clock；先不接owner，不build原O/out或回寫原11項。若其通用觀測正負例可成立，再由總控另決定完整接線、原controls、frozen真圖回歸與成本；本設計不自動授予任何步驟。觀測全綠也不能叫Phigros語義通過。欲主張H fragment被解決，還缺可靠實景獨立端部／像素link與真正fragment/小Hold混淆的人工可判別例；欲主張K ownership修復，還缺旧tail與incoming当前部位和逐contact回饋。

## 6. 結論、未做及交接

可觀測差異是**當前接觸區是否有支持、是否有自己的雙側端部與背景分界、這份可見分界在有限近期是否持續並與接觸piece分開**。它們支持觀測層關聯與接觸約束，不由連續藍區推出physical唯一。相同有界觀測仍只能輸出相同限制，這不否決所有pixels方法，也不把合成世界當真遊戲gold。

本包只交一個值得下一包隔離冷驗的BVI-1設計；可靠physical ownership／H真fragment／K真正採納、旋轉／late alignment實景語義、有效runtime成本、端到端候選、全曲IN仍未完成。未執行任何產品tests或策略；機讀自驗只檢JSON可解析、來源hash／保護／容量及引用。詳見 [交接](HOLD_OWNERSHIP_X10D_O_OBSERVABILITY_HANDOFF_20261004.md)。完成停止等待總控，不自行續派。
