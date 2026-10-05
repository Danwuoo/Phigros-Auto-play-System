# Zero Miss：輸入 owner、scheduler、多指與動作語義研究

日期：2026-10-05 UTC。研究基準：`74e54437d4a3ad2b2bd1a3b09312211a92f2359e`。本報告只對此 checkout 的 source 下靜態結論；**不把 main50 的 owner 政策倒套到 C36h tint1**。產品目標為 Chapter Legacy 全曲 IN 解鎖及完整 IN 結算 Miss=0，P/G/B 照報，沒有 AP 前置。

## 1. 結論與證據等級

1. **不是缺少基本多指／Hold／Drag／Flick 實作。** 單 owner、五指 fingerprint、可撤銷 plan、同 contact Move、Hold tail 二次支持、Drag 續接、Flick 完整路徑與 unknown Down 不重試都有 source／測試；先驗證「輸入是否有足夠當前像素與正確歸屬」以及「已成立機會在何處被撤銷／逾期」，不要重做已有能力。
2. **五指可見不等於五個 Down 同時生效。** 正式 scheduler 每次同步呼叫一個 `inject`，沒有接 `GrpcTouch::inject_batch`。本次原 scheduler 的 fake-clock 反例：五個同 due、每 RPC 模擬 10ms，初始 Down+30ms window 下只送 3 個、2 個 window-expired；放寬 synthetic window 才看見第 5 個 `down_too_late`。這證明可能機制，未證明真 RPC 是 77 Miss 的原因。
3. **到期／capacity 的「已知沒 Down」與「不知道是否 Down」目前在 owner 的 absent cursor 上收斂。** scheduler 的 conflict/late/expiry 刪 plan；owner 見 submitted=true、cursor 不在就不復活。這是既有保守設計且已有不重試測試，保安全但可能犧牲仍可救的後續機會。先加離線 disposition 稽核，不直接放寬 retry。
4. **Hold 提前 Up 不是單一 60ms grace 旋鈕。** 未開始 Down 缺席可立即撤銷；已開始 Hold 的 samples=0／歧義／expired 可早於 grace 取消；新鮮完整 snapshot 中缺 target 才走 60ms missing；完全沒有新 snapshot 時 scheduler 仍按 100ms gate/target expiry。延長一個 grace 不能修所有分支，可能增加錯誤續接。
5. **Flick 的設定速度不等於真實路徑。** 已有 80px、4 個 20px Move、每 12ms、52ms Up；但遲到 Move 沒有逐步 lateness guard。本次原 scheduler 模擬 owner 空窗後，原 due22/34/46/58ms 的四 Move 都在 fake60ms 依序送出。這只證明 backlog 壓縮，不證明遊戲漏判；需查實際 Move 時間／可見採納。
6. **current body support 不能直接升格 exclusive physical ownership。** X10d-O 的 BCC-v1 已被同像素、不同物理世界反例否決。已有 main50 alias 也不能當 physical gold，不能用「少 Down／少取消」替代 zero miss 成果。

證據標記：
- **S：本次靜態核對**，可證 code／tests 存在及分支條件。
- **C：歷史冷驗紀錄**，本次未重算其 ignored raw；不是新跑結果。
- **R：本次重测**，只限 §5 的 Linux 原 core.cpp＋隔離 shim／FakeClock。
- **L：歷史裝置／遊戲紀錄**，Fixture 與真正 Phigros 結果再分層；本次 live=0。
- **H：待驗假說**，含機會損失、游戏採納與可減少多少 Miss。任何 C/R 都不自動升成 L。

已讀 AGENTS、README、ARCHITECTURE、ROADMAP、跨曲研究及相關歷史研究；checkout 無 `.agents` 目錄。只新增本報告與 `evidence/touch-scheduler/`；正式 src/include、既有 tests、CMake、process-control scripts 未修改，沒有 branch/commit/push/PR、emulator、真觸控、訓練、付費或 full recording replay。

## 2. 實作地圖：責任、界限與不可混淆處

### 2.1 Owner、時鐘與取消

| 層／證據 | 現有實作 | 對 zero miss 的含義 |
|---|---|---|
| `include/pas/game.hpp:214–251`；`src/game.cpp:1329–1365`（S） | 完整 snapshot；note ID、intent ID、contact ID 分離。拒絕倒序 snapshot/context；新 context 撤銷、清 identity/alias；capture age <100ms。只有已知 absolute cursor=0 可令 submitted=false 讓後續新鮮像素建立新 intent。 | 「有新 frame」和「完整且被接受的新 snapshot」不同；被拒快照不能解讀為 absence。缺 cursor 可能代表完成／unknown，不能無條件重試。 |
| `src/manual_session.cpp:210–274`（S） | 單 action owner 先取最新 packet、處理 revoke、accept，再 poll；每次 injection 再檢查 stop/live_gate/round/revoke/capture age。高解析 Win32 relative timer＋event，wait clamp 0–10ms。 | 沒有固定 60Hz polling 假設，也不是等下一張 frame 才 dispatch。replay 的 frame-first tie 是明確政策，不等於歷史 thread race 全部重現。 |
| `src/game.cpp:1318–1319`（S） | GamePlanOwner 明確建 128 plans／16 steps／350ms horizon／100ms evidence scheduler。 | Config 和 generic scheduler defaults 不能代替有效遊戲參數；generic defaults 是 64／16／2s／150ms (`include/pas/core.hpp:267–271`)。 |
| `src/core.cpp:309–355,385–418`（S/R） | revision 保留已執行 prefix；compact 不能丟最後已執行 step；birth intent high-watermark 防舊 intent 重生。active cancel 送 Up；unknown inject 全域 fault＋release；release failed/unknown 保留 fault。 | local terminal outcome 必須和接觸是否真的被遊戲釋放分開。意圖高水位是軟體去重，不是永久物理 note identity 神諭。 |
| `src/game_session.cpp:125–148`（S） | 真正新 round 才建立新 owner；suspend 只 cancel、不抹完成 identity；finish 保留第一份 release report。 | 不應以 UI 瞬斷或每個研究窗 reset 來恢復觸控資格。 |

核心安全狀態機的讀法：
- pending、有 cursor=0、新完整 snapshot 缺當前 object → immediate cancel，可在之後重新满足全部新證據 gates 時建新 intent (`src/game.cpp:1668–1676`)。
- pending、較新幾何明確否定舊 root／uncertainty 超限 → cancel＋submitted=false (`1722–1743`)。
- started、當前有效 body/rootless support → 同 contact Move／續租，不需重造 Down (`1796–1831`)。
- started、缺席／unsupported → grace 不 Move、不刷新 evidence；明確 invalid 優先取消 (`1694–1720`)。
- completed 或 unknown／externally removed cursor → 保留 retirement；不可因返回就 Down (`1360–1364,1694–1697`)。
- gate/source/context fault → 全部 release；fresh gate 不復活舊 target (`src/core.cpp:281–306,436–453`)。

### 2.2 排程詳細語義

`src/core.cpp:426–501`（S/R）：每圈先檢查 stop、gate expiry，再逐 plan 檢查 evidence/window expiry，再選最早 due。**只有相同 due 才是 Up → Move → Down**；同 due/phase 依 map 的 intent 次序。contact 在 Down 即將送出時找最小空 ID，future plan 不預占指。第六 contact 只丟新 intent、不搶 active Hold。unknown RPC 立即 fault，沒重試 Down。

值得量測的邊界：
- `submit` 接受 valid_until==now（拒 `<now`），dispatch 在 now>=valid_until 拒絕 (`309–320,442–453`)。所以 accepted 不保證有一次 Down；本次 R 已重現。
- `max_late` 對 Down 是 `now-due >30ms` 才拒，等於30ms允許；但新 owner plan 的 valid_until 通常 down+30ms (`game.cpp:1882–1887`) **會先於 lateness 分支關門**。pending revision 可改為 down+45ms (`1787`)，兩者須分開報。
- active plan 不再用 valid_until 取消，而用每 plan evidence expiry；Up/Move 沒有 max_late guard。正常 Up 和安全 cancellation 不能混合。
- 新 Down 舊 due29ms、active Up due30ms，owner 到35ms才醒：仍先嘗試 Down，因 capacity 丟掉，再 Up。不要把 equal-due reuse 的成功概括成所有 overdue reuse 已優化。
- plan_capacity 是全域 fail closed；contact_conflict 是單 target drop；identity_capacity 是 owner 全域 cancel。notice≤256、identity/alias≤128、accepted-plan diagnostic≤256 等上限不同 (`core.cpp:351,375`；`game.cpp:1432,1686–1691,1830`)。任何改動都應報 overflow 的失效方式而非只報峰值。

### 2.3 五指 mapping／transport

有效 profile `configs/phigros-hd-assist-five-lead35.json`：capture1280×720／source rotation1，touch720×1280／rotation90，五 contact，touch timeout100ms；四種類、lead35ms、uncertainty30ms。

- `src/core.cpp:252–267`：frame-inclusive edge normalization，90°為 u=1−v/v=old-u，round到 touch grid。本次 R 核對 (0,0)→(719,0)、(1279,719)→(0,1279)、越界拒絕。這不是當前裝置 mapping fingerprint 重驗。
- `src/runtime.cpp:43–70`：報告 schema、Fixture/APK hashes、serial、capture geometry、touch mapping、Android版本／SDK／ABI／model／wm_size／density 需匹配；`max_contacts_verified` 缺失最多沿用2。只改profile的5不能授權五指。
- `src/emulator.cpp:360–429`：RPC success 明列 `rpc_returned_effect_unverified`；failure→unknown IDs＋faulted。未知 Down 保留 release 責任，即使 positions_ 尚未收錄它。
- `src/emulator.cpp:376–408` 有 batch 能力；`TouchBackend` 接口只有 inject/release_all (`include/pas/core.hpp:235–240`)，正式 scheduler 呼叫 inject (`core.cpp:488–497`)，所以不可宣稱遊戲同時多指已 batch 化。
- `src/emulator.cpp:432–462` release_all／emergency_release_all 逐指同步 RPC；`core.cpp:491–495` dispatch mutex 持有跨 RPC，supervisor 不能撤回已開始的一次 injection。100ms deadline 是 API 設定，**不是已實測 stop/release 最坏時間保證**。須分報 in-flight stop delay、逐指 release attempted/failed/unknown、最後可見active狀態；不能僅以 return true 宣稱實機放開。

歷史 L-Fixture：9/27 八類240/240、五指取消30/30；RPC n1290 p50/.95/.99/max=0.84685/1.3585/1.652889/6.0505ms。這是當時不同 workload、不是本次測得，也不能把5×p99當同時五指尾延遲。來源 `docs/catalog/06-engineering/GAME_ACTION_SEMANTICS_20260927.md:24–37`，本 checkout 沒有其 raw 可重新核算。

## 3. 各動作的現有能力與真正未驗項

### 3.1 Hold：head、body、tail 三段分驗

**Head 接入（S）**：新 contact 要有效類型/root或其他該類合法通道、時空安全、未過期；`held_body_evidence` 不能新 Down，tail 已過線不能補 Down (`game.cpp:1835–1880`)。不能拿窗中一個 body 候選當 head 已成功的證據。

**Body 持續（S）**：held_support 要當前 rails、head_on_line/held_body、samples>0、capture 同源、非投影及無已列歧義 (`1368–1374`)。兼容性不是永久物理ownership：同 line 角度 alignment≥.5、不同/未知 line≥.98；寬度差≤max(4px,12%)；位置有界；同一 note patch/front切換可到128px但要同非零line、當前雙方body與方向/深度條件 (`1375–1397`)。active Move 要 >2px、當前支持、相容且tail未通過，使用當前hit，不固定初次Down、不盲目旋轉 (`1796–1806`)。

**Alias 已存在（S）**：唯一定义的当前outer-body、active cursor、原證據<60ms、未tail-terminal、未被另當前owner claim，才 alias candidate→active owner (`1398–1448`)。alias不改observer history；主ID仍見且valid時，無效duplicate不取消它。alias資料100ms後可清理 (`1686`)。不要把「新增alias」寫成未實作需求，也不能用這套幾何相容性宣稱同column兩Hold的physical ownership已解。

**Tail（S）**：同幀可見tail、rails、可用當前line且line_id一致，reference在線區域、方向／深度推得已過線；兩份 fresh tail支持相隔≥10ms，才固定release=evidence+20ms (`1449–1477,1807–1822`)。tail預測不提前正常Up；missing的安全Up不等於正常完成。切向modπ、倒向、frame skip、old tail接incoming front都需負例。

**Grace不可混用（S）**：pending absence立即取消；active一般缺席Tap/Drag40ms、Hold60ms、Flick75ms (`1668–1680`)。明確invalid samples0/ambiguous/expired先取消；body不兼容不刷新、不Move，60ms後才cancel (`1698–1720`)。沒有新snapshot則只有scheduler100ms時間界，不會自動在60ms「看到absence」。因此要報 `new_snapshot_absent`、`seen_but_invalid`、`no_snapshot` 三種。

**歷史 C 與 L 限制**：已有RGB旋轉/平移36幀、rootless Hold+Tap、tail角跳、双Hold+Flick、2指容量負例；不是缺測試架構。見 `tests/cold_scenario_tests.cpp:1142–1255,2047–2246,2811–3013`、`tests/game_tests.cpp:1290–1421,1532–1588,1756–1826`。C36h Dlyrotz IN77 Miss仍是歷史實戰結果，不由這些冷測推為已修。

最新直接反例：X10d-O BCC-v1 在完全相同visible union＋anchor/context兩世界中都宣稱exclusive owner，第二世界要求unknown，因此候選10pass/1fail、不可採用；owner8/8只驗未接resolver的C36h契約。來源 `docs/catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_RESULT_20261004.md:29–41`（C）。**需補的是當前part/region及可辨link與機會分母，不是放寬「沿body都歸原指」**。

### 3.2 Drag

- 既有 current spatial overlap bypass temporal-root能力：至少2樣本／10ms、單當前line、寬/高/confidence/core幾何限制，非outline/rails/heldBody；這不是對所有首幀突現無條件下壓 (`game.cpp:1478–1519`)。
- current near-line／合法root可以有界续租；新root分支額外提早15ms Down，通常到due+75ms Up；current overlap用evidence+100ms (`1578–1588,1844–1852,1893–1896`)。不要把Drag timing等同Tap lead。
- 共用的leader要active、有新鮮current Drag、window重疊且唯一可覆蓋leader；當前新successor先於missing判定匹配 (`1589–1664,1857–1875`)。
- 現行main50已超出9/27原始stationary覆蓋：同非零line允許較寬along（max48px/半寬）及across≤48px，必要時修訂同contact Move (`1599–1605,1620–1633`)。這是source有的策略，不能只援引舊文件的across≤2px說現行一定不Move。
- 多leader ambiguous不選第一個；normal Drag、shared_drag follower/leader不同；需要避免把一次leader接觸算成所有後繼physical notes已成功。

未驗 H：真正ゲームhit region、Drag合流分流兩個獨立需求是否被一指誤吞、移動窗與所需位移是否仍被採納。最小對照使用同幾何的一串與兩個可見分離note、同line/異line/未知line、normal短缺／長缺、同時Hold佔4或5指；安全與合法覆蓋機會都計分。

### 3.3 Flick

- 使用者玩法分類指出可任意滑動方向；此為需求背景，不當遊戲引擎精確判定規格。`docs/catalog/01-project/note形式.md:1–14`；`src/game.cpp:259–270`先下、再上、右、左，須80px clearance且不進top12% UI。
- 六step：Down、四個20px Move間隔12ms、Up在52ms；pending修訂重建整條路徑 (`1771–1784,1897–1906`)。名義80px/48ms≈1667 frame-px/s；不是量得touch/display/game速度。
- source只作screen-bound選向，沒有針對其他活動指／其他note做Flick路徑衝突規劃。缺這個實作不等於它是當前瓶頸，應先有可辨相撞／誤採納反例。
- 本次R證明owner醒晚可把多Move壓在同一次poll，後端Android resampling／coalescing及遊戲採納未知。`tests/game_tests.cpp:1933–1970`與 `tests/cold_scenario_tests.cpp:1423–1646` 已驗路徑、邊界與missing，不等於實機有效速度已驗。

## 4. 根因樹與最小區分實驗

不要由結算77 Miss直接分配到下列葉節點；只有找到同一 physical event 的可核映射才可算貢獻，否則報 unknown。

| 分支 | 要觀察的首個失效點 | 最小可否證實驗／必要反例 | 誰能回答 |
|---|---|---|---|
| A 漏觀測／source缺口 | received pixels內可見head/body/line? candidate不存在? 或當時根本沒收到pixels? | 一個失敗窗＋正常control，保留首可見前後原cadence。人工只標visible/unknown，不用候選ID充gold；加入已知像素oracle分支只供隔離比較。 | 真圖＋有限gold；source未錄區只能unknown |
| B 關聯／ownership | candidate存在但note→line/root/held_support失格、alias/claim吸收incoming | 同column gap、貼合不可辨雙世界、舊tail+新front、thin Tap共存；已存在O反例不能改gold。rootless active Hold正例、真新incoming Down正例必同時保留。 | typed/RGB冷反例＋真圖可辨範圍 |
| C pending撤銷 | accepted但cursor0，下一完整snapshot缺席/contradict導致cancel；有無fresh-return | 49/50/51ms ready vs due、capture==due但ready後到；所有四類；缺席再回、從未回；completed/unknown Down不重試負例。 | 既有X10d-P tests＋full-prefix events；遊戲損益另驗 |
| D late/expiry | source fresh但accept已晚？intentional clamp？owner因RPC/排程醒晚？ | 初始與revision valid_until分層；5個同due，RPC scripted0/1/5/10/45ms，先只離線；Down attempted與window-expired全部列。 | 本次R確認可能機制；真耗時須同域runtime receipts |
| E capacity/reuse | active=5？重複claim耗指？先Down後Up順序？drop後仍有可救窗口？ | 5Hold+1Tap；4Hold+Flick；同due Up/Down與due差±1ns；known-never-down terminal與unknown/completed分開。 | scheduler/owner typed controls；必要時full-prefix replay |
| F transport/mapping | inject失敗/unknown、rotated坐標錯、指漂移、Move resampling、release未知 | 只在另有裝置授權後，凍結profile+能力fingerprint，Fixture per-pointer軌跡及取消；不要從RPC success代判。 | 歷史Fixture只能支持原版本；本次無新裝置測試 |
| G 遊戲未採納 | 有correct-current-region＋RPC returned，仍無可唯一歸因的遊戲feedback | gate通過後另批有限manual comparison；同候選版本/設定/對照，full result與可見回饋，四類分報，無唯一match就unknown。 | 只有有授權的真Phigros閉環；offline不可回答 |

### 安全與機會損失必须成對

歷史X10d-P正是範例：全actions reference2165/candidate2131，matched2121、unmatched44/10；28 pending-missing cancel、14組reference曾Down而candidate沒再開始、5組fresh-return改plan；physical判定及14組損益unknown。來源 `docs/catalog/04-offline-research/PENDING_CANCEL_X10D_P_RESULT_20261003.md:62–99`（C，本次未重算raw）。所以「少34 actions」「少14 Down」不能稱改善或漏打減少。

未來每個候選至少同報：
- 安全：duplicate Down、unknown Down retry、completed resurrection、無當前支持Move、錯contact接續、取消後動作、expiry後動作、failed/unknown release、contacts_at_exit。
- 機會：可辨合法機會中new Down被拒／pending取消後沒再開始／fresh return救回、可支持body仍過早Up、capacity掉失與恢復、合法新incoming被alias/suppression吞掉、unresolved/unknown。
- 不為達「安全0」而把所有候選拒掉；也不為多Down而降低unknown處置。

## 5. 本次非破壞性重測（R）

### 範圍、環境、來源

直接編譯原 `src/core.cpp`、原 `include/pas/core.hpp`；Linux無Windows SDK，僅由隔離 `shim/windows.h` 提供HostClock符號，**所有probe實際用FakeClock**。不是移植正式runtime，不是CMake全案build，不含GamePlanOwner/observer/gRPC/Android。未安裝JSON/GTest等依賴。

- Linux x86_64 kernel6.18.44；g++ Debian14.2.0，C++20。
- source SHA256 core.cpp：`ca0cae9619fd3481b56ca39d89aa05db621a558bbd5aa58b666dc6303bdce7f4`。
- header SHA256 core.hpp：`c27ed9223cd3eed4a496d4d5b63b910bc72a6a76414380c174e9dd2720edf578`。
- 全部命令／source/shim／logs／hash在 [evidence/touch-scheduler](evidence/touch-scheduler/)。binary放隔離 `/tmp`，精確build path和SHA已記；binary並非本報告必需交付。

### 結果

| 新case | 結果／判讀 |
|---|---|
| valid_until==now | submit=true，但dispatch Down0、expired1；accepted≠attempted |
| 初始window到期優先 | down+30ms時window-expired，不能期待down_too_late先出現 |
| late等於30ms／+1ns（synthetic較長window） | 等於允許一次Down；+1ns拒絕，舊intent無法重新提交 |
| 第六指與retirement | 6提交、5Down、1conflict；after Up空閒，同intent拒，新intent可提交 |
| 同due Up/Down | Up先、同contact可重用 |
| overdue先後due不同 | Down29/Up30、到35ms才poll：Down先conflict，之後Up |
| 五個同due、scripted RPC10ms、synthetic window80ms | 4Down（start10/20/30/40ms），第5個too-late |
| 五個同due、scripted RPC10ms、原生式window30ms | 3Down，2window-expired；不是真RPC實測 |
| unknown Down | 只一次attempt；fault鎖住；release含unknown contact；新intent亦不可重試 |
| fresh gate/old target | gate更新90ms，old target仍在100ms到期Up |
| stop-before-due | Down0、disarmed |
| profile90°edges | 兩corner映射正確，越界拒絕 |
| 另檔Flick backlog | due22/34/46/58ms四Move全在fake60ms送出，62ms正常Up，無留指 |

主probe共13 cases／66 assertions，O2初跑及重跑均0 fail；另Flick probe1 case 0 fail，共**14個不同synthetic cases**，不能把重跑當更多獨立場景。

Sanitizer分母如實保留：
1. 第一個ASan+UBSan launch被LeakSanitizer的ptrace環境限制中止；`sanitizers.log`保留fatal原文，**不算測試通過**。
2. 同binary以`ASAN_OPTIONS=detect_leaks=0`重跑，主13 cases／66 assertions通過、AddressSanitizer+UBSan無diagnostic；**不含LeakSanitizer coverage**。Flick另檔只做O2，未做sanitizer。
3. O2 build有probe自身兩處misleading-indentation warning，原log保留；不影響控制流程，未修改production為取得pass。沒有重跑歷史Windows Release/Debug/ASan整套，也不把本次結果冒充QPC成本或gameplay驗收。

這些是機制probe，**沒有host performance分位數**，FakeClock數字不是host測速。已有歷史R7同deadline兩Tap與RPC5/45ms失效測試，見 `tests/cold_scenario_tests.cpp:2589–2677`／`COLD_DEVELOPMENT_RESULT_20260929.md:11,27`。新五指case是擴展既有機制，並非聲稱第一次发现序列RPC。

## 6. 必要metrics：完整分母與clock域

### 6.1 每層都有自己的分母

| 分母 | 必備資料 | 禁止替換 |
|---|---|---|
| 全部frame attempts／received／consumed | capture序號、pool/drop/skip、capture_complete/pixels_ready/recognition/publication/selection/accept，零候選幀也在內 | 只數有targets或有plan的幀 |
| 可辨physical events（人工）＋unknown events | clip-local physical/part/line relation與unknown；同事件跨幀去重；head/body/tail分段 | observer note IDs或occurrences當譜面notes |
| 所有unique intents＋所有revisions | note/intent/contact關係、建立資格、所有owner拒絕理由、pending/active/terminal、cursor/prefix、first/final due及valid_until | 只數成功提交或成功注入；修訂當新note |
| 所有action需求與outcomes | Down/Move/Up planned、superseded、cancelled、expired、capacity、attempted success/unknown、release_all requested IDs及結果；order/cause/previous prefix | inject Up少於Down直接判漏release；空release報告當真Up |
| 全run結果 | 完整／中止／failed／skip、P/G/B/M、max combo與frame/transport/runtime統計；HD/IN分層 | cherry-pick最佳輪；取消/Down数當Miss |

現行已有的診斷不要重做：`game_plan_accepted`、`game_touch_receipt`、`scheduler_rejection`、`game_contact_cancelled`、tail/alias/Drag coverage，見 `manual_session.cpp:187–204,255–271`；分析器有future/past-at-accept、predicted deadline lateness、clamp、join缺失、conflicts、contact cancellation、RPC distribution等 (`game.cpp:2307–2351`)。但 observer `reason`不是每个owner分支的即時拒絕理由；`last_rejection_`也可能沿用舊值，不能用它對單target下歸因。優先補**最小有界 disposition+terminal cause**，而非另一套完整trace平台。

### 6.2 時序需至少分解

每項皆 n/p50/p95/p99/max、jitter（定義清楚，例如p95−p5）、失败与unknown；不平均各曲p99、不相加不同stage p99。

- `accept−capture_complete`、`accept−recognition_end`，及publication→selection（若尚無原raw不能重建），包含rejected/cancelled/unscheduled個案。
- due餘裕 `due−accept`；window餘裕 `valid_until−accept`、target/gate TTL；初始plan與revision分列。
- `injection_start−scheduled_due` 是owner lateness；`injection_return−injection_start` 是RPC；`start−predicted_down`再列lead/clamp。因window-expired未dispatch的例子保留“可觀察到的expiry lateness”，不可當0ms。
- 同due chord內 earliest/latest start、return skew，按1–5 contacts和原先occupied數分層；報全部chords含只送部分／全拒。
- Flick各段 actual dt/位移、压縮Move數、最大gap；Hold支持最後時間→實際Move/Up、支持區位置誤差、early/late tail與unknown；Drag覆蓋／fresh successor／ambiguous leader分母。
- stop/revoke→最後新Down、release開始/返回、failed/unknown IDs；一個RPC in-flight時的延遲另列。

原live QPC與replay rebased QPC **原點不同**。X1以`original−first_capture+1s`重基準 (`apps/frame_review/contact_replay.cpp:213–226`)；不能直接拿兩個絕對值相減叫latency。source Unix/Android timestamp仍另domain，source render age未知。X10d-O觀測報告明確列H2478/K2881兩種時鐘及K2882未消費 (`HOLD_OWNERSHIP_X10D_O_OBSERVABILITY_20261004.md:56–58`)。fixed-pixels replay的zero recognition/RPC policies也不是實機零延遲。

## 7. 優先順序、效益、風險、依賴與驗收

以下為後續提案，不是本輪已授權執行清單；不動既有process-control／quiescence gates，不恢復suppression，不自動開live。

| 優先 | 最小工作 | 預估效益／成本量級 | 風險、依賴 | 驗收／停止條件 |
|---|---|---|---|---|
| P0 | 既有X1/X10d-P小證據包來源+全action ledger重讀；串出known-before-Down、active、unknown及clock域 | 高資訊增益，0.5–1人日；不承諾減Miss | 需小raw包，不需模型／裝置 | 所有actions可追溯或明列unknown，首因分支可區別，no-down/opportunity均有分母；缺prefix就停止該結論 |
| P1 | 隔離 typed terminal disposition 稽核：capacity/expiry/late已知未Down vs completed/unknown；先只量哪些在後續像素仍可合法救 | 找可挽回機会上限，1–2人日；實際比例unknown | 不能以cursor absent放行；须保留所有歷史不retry/active保护测试 | 每一potential retry有證明沒有prior Down及新pixel资格；unknown/completed救回0，加入新根據前不得接正式owner |
| P1 | current part/body支持與incoming可辨性研究沿用O/BVI負例，保留head/body/tail、正常Tap/新Hold機会 | 更贴近现有H/K语义问题，2–4人日依可用样本 | BCC被否決；current support≠physical唯一ownership；body blanket suppression高風險 | 不可辨pair保unknown，可辨独立incoming不丢；rootless active有支持可续，错误alias/unsupported Move为0；真圖收益仍另驗 |
| P2 | 同deadline 1–5 contacts、mixed Hold/Flick、expiry先後及Flick backlog的fake-clock矩陣；必要时Windows受控fixture记录真实串行skew | 识别排程机会损失与实际运输上限，1–2人日冷；装置另计 | 无真实slow-RPC证据时不直接batch化；batch结果部分unknown、release原子性需契約 | 全5类dispatch分母完整，安全不退，机会不因测量消失；冷结果不能开live资格 |
| P2 | 有界batch设计/调度优化只在P2量測指向它时比较，不改变evidence/window来取得表面成功 | 常量RPC主導时可能降低chord偏斜；无瓶颈可能零收益，2–4人日 | `TouchBackend`新契约、unknown batch、优先Up与混合phase、process-stop線性化；不可用多owner并发绕过 | 原單owner安全全绿，exact all-action audit，无未知重试、无错误prefix；Windows真实测量+现有成本gate过后才考虑候选 |
| Conditional | 凍結候選有限Phigros闭環动作语义／完整IN验收 | 唯一能回答Miss與遊戲採納，成本按实际曲目分母另估 | 需当前章节/解锁核实、exact freeze/preflight、有效成本与总控续派；本次未授权live | 完整IN每曲Miss0；所有P/G/B和失败run照报。不加AP，不以Fixture通过或一次分数改善代替 |

先后建议：P0/P1可并行，P2冷矩阵不必等physical gold；**批量RPC或retry是条件性候选，不应因为本次synthetic反例就立即重写正式排程。** 能减少多少Miss目前不能量化，最诚实上限是“已核合法机会中位于该分支的比例”；该比例尚未取得。

## 8. 用户若补 gitignored 文件：最小清单

本checkout没有 `measurements/`、`out/`；本轮程式研究与上述R probe **不需要**它们。历史文档中的hash、pass数字只算记录，未假称本次重算。不要整包上传out或secrets。

### A. 先给小包即可重读历史contact因果

以 `B=measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1`：
- `B/input-manifest.json`、`input-source-audit.json`、`source-v2/c36h-source-provenance.json`、`source-v2/main50-source-provenance.json`、`tool-freeze-verified.json`。
- `B/five-cases-verified.json`、`prefix-first-divergence-verified.json`、`check-verified.json`、`run-audit-verified.json`、artifact/capacity ledger。
- `B/{c36h-verified-on-1,main50-verified-on-1}/{summary.json,trace.jsonl,events.jsonl,original-recorded.jsonl,original-prefix-events.jsonl}`，以manifest/summary的实际SHA及路径核实。这两run包含五窗口109幀trace；events必须保留完整可用prefix，不只剪窗口内receipt。
- 若直接查pending收益，另取 `.../pending-cancel-x10d-p/{full-action-audit-1.json,lifecycle-audit-1.json,causal-review.json,final-summary.json}`，然后只按其引用补baseline-on-1／variant-on-1的summary和完整events/lifecycle records；不要先要全部实验批次。

用途：校验来源、重新审计软件first divergence／cancellation／fresh return／known Down，不执行任何原EXE。没有PNG时不能重新肉眼确认部位、ownership或真漏观测。

### B. 看H/K部位的最小图片包

按现有O packet先补H2440–2495、K2865–2895，以及A3493–3504/B4979–4990/C5516–5524/E5281–5293/D6158–6220，共196 distinct PNG＋原index对应rows＋packet/grounding/source clock join；不能只给十张anchor后宣称有完整运动。若只为第一轮问诊，可先H21张2468–2488和K25张2865–2889＋A3496–3500及D6214，但这52张**仅用于判断需补哪段，不足完整contact replay**。这些路径由packet记录精确取，勿猜文件名。

### C. 要重跑真实pixels→owner，不可只补一个事件窗

现成X1 reader需 `.../full-recording36g-01/sessions/manual-session-22885039263800/` 的：
- `full-recording/index.jsonl`及其引用全部7722 PNG；`round-1/summary.json`及其`event_segments`列出的两journal segments；实际profile/manifest及关联SHA。
- exact source lineage：C36h freeze（同37/19不等于历史main37/19）与main50 source provenance；若Git历史不含dirty bytes，只给freeze逐文件列出的必要源码/export。不是整包build缓存。

现工具从最早available preroll连续暖机至EOF，capture和pixels_ready分开推进、owner-consumed集合及tie policy固定，不能偷偷改成109幀冷起动。若未来设计可恢复checkpoint，它至少须包含：observer/line/note最近历史与ID计数、生命周期/round/gate/context、所有owner identities与alias/Drag leader、每plan evidence/valid_until/revision、executed immutable prefix与prefix_offset/cursor/contact ID/position、tail terminal与首次tail支持、高水位/完成retirement、backend active/unknown/release责任、当前fake clock及rebasing。**只给Down事件不是完整可恢复状态；只给90ms暖机也不覆盖长Hold启动。** 现仓库没有据此已验通用checkpoint恢复入口；即使全录补齐，录影前状态仍可能unknown。

### D. 真实clock／transport或game effect的额外最小来源

同一run的manifest/profile/build hashes、capability/preflight摘要（脱敏）、完整相关journal segments（保留所有失败和unknown）、full-recording index与对应可见反馈PNG／最终result；至少包括Down前建立该plan的prefix及最后release。Replay synthetic events不能替代原runtime receipt。source render age若当时没被量测，传更多文件仍不能凭空恢复；游戏逐note採纳无唯一可见match就保持unknown。

不需要：整个out、vcpkg缓存／LibTorch／模型weights、AVD镜像／APK全包、浏览器/系统资料、认证token或任何秘密。先传manifest／ledger核对实际最小文件与容量，再决定是否值得传全录。

## 9. 停点

本分支已完成代码/测试/历史证据对照、14个隔离scheduler机制cases与最小后续实验设计。正式实现和既有测试保持不变；没有将任何proposal升级为candidate、live-ready或77 Miss根因。当前最有价值的下一步是补小型已凍結contact证据包，确认每个机会在哪条首因分支消失，然后只选一项通用机制做隔离A/B；任何在本输入下不可辨的physical ownership仍应明确unknown。
