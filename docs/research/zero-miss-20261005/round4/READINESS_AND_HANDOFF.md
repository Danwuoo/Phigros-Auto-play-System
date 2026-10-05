# Round 4 readiness：下一個必要門檻與精確交接

2026-10-05 UTC。唯讀介面盤點基準 `a53a9b7021bcc900f498047f5cc017b42e4711b0`。本文件沒有修改 main、candidate、owner、scheduler、工具鏈或舊 STOP。**v3最終core已凍結，完整結果見VALIDATION.md與evidence/final-core.sha256；本介面盤點不代簽正式整合。**

## 1. 結論與本次停止線

雲端能回答 isolated BVI 在明示 current-RGB／typed／fake-guard 契約中的通用正反例，不能回答它在真實遊戲畫面中的 ROI 完整性、實際 contact ownership 或改動後的遊戲採納。下一個必要步驟是 **Windows 離線重建＋合法真圖橋接／完整 contact prefix**，不是直接開遊戲，也不是再增加合成數量直到全綠。

本次委託在選定 v3 的必要 cold regression、獨立覆核和交接完成後停止，由使用者自行移交 Codex task。這份文件不建 task、不等裝置上線、不執行 Windows／ADB／Fixture／遊戲／真觸控／付費或 push／PR。產品 Goal 仍未完成：目前章節分母 N、逐曲 IN 解鎖及同一候選完整 IN Miss=0 尚未核實；不加 AP 或未核准的每曲連續 3 次门檻。

## 2. 本分支實際做了甚麼

- 讀 main 的 observer→candidate batch→target→owner→scheduler 介面、BVI v2/v3 header、Windows presets、profile/preflight、X1 replay 與 X11 成本接口；逐項結果如下。
- 以原封 source 和已驗 JSON header 實跑 GCC C++20 `-fsyntax-only`：`src/game.cpp`、`game_tracking.cpp`、`game_motion.cpp`、`game_session.cpp` 各 exit0。
- 同方法檢查 `src/core.cpp`，在 line11 的 `windows.h` 缺失處 exit1；未改 core、未換掉 QPC、未造 Windows shim、未做連結或執行 owner。
- 當前工具確認：GCC 14.2.0 在場；CMake、PowerShell/pwsh、MSVC cl、MinGW cross-compiler 不在 PATH。沒有安裝工具。
- 原始命令、source/header SHA 和退出碼：[interface-compile.log](evidence/readiness/interface-compile.log)。**4 translation units 語法通過不是 4 個行為 case，更不是正式全回歸。**

## 3. Source 介面證據與最小 adapter 要求

### 3.1 Current pixels / ROI / all-lines

| 接點 | source 可核的現況 | 接手需要的最小橋接與拒絕規則 |
|---|---|---|
| Frame→BVI View/Key | `include/pas/core.hpp::Frame` 有 RGB/stride、sequence、epoch/generation/geometry、capture_complete/pixels_ready/source_valid；BVI `View/Key` 接相同類別但不自動連 main | 借用同一 frame lease，source geometry/layout/bytes 先驗；完整 context、capture、ready、sequence 明示映射，絕不拿 recognition_end 或現在時間補成 capture。exact same-frame RGB 與 batch 綁 SHA/source-frame/context；不得把另一幀線混入 |
| GameObserver→ROI | `include/pas/game.hpp:GameObserver::candidate_batch()` 公開最後 CandidateBatch；`src/game.cpp:1308–1310` 在 legacy tracking 前建立；`src/game_tracking.cpp:527` 標 `history_source=baseline_guided`，含 origin、quality、head/body、recent hint | 可以作 **current pixels 上搜尋 ROI 的有界提案**，不是獨立 detector gold；所有 BVI 支持要重讀 current RGB。側表保存 candidate_id/origin/head-visible/geometry semantics，不能丟 provenance。手框 ROI 只做診斷、不能成為正式動作輸入 |
| Hold front/depth | BVI `Query.front` 是前端，沿 `-normal(angle)` 掃 depth。main `NoteCandidate` 的 center/height 因 origin 而有不同語義；`src/game.cpp:939` 有 tail 時 height=`center.y-tail.y`，是 screen-Y 差 | 不能普遍 `note.height→Query.depth`。有當前 tail 時需以 note 局部 normal 投影 front−tail，再核正方向與可見 rails；沒當前 tail 時只能用明示 bounded search extent，不能冒稱真尾。旋轉、tail clipping、normal sign 翻轉各要控制 |
| Interior patch | `held_body_patch` 在 `game.hpp` 與 `src/game.cpp:1380` 明示為 interior touch region，不是 measured front；`make_candidate_batch` 對它 head_visible=false | 不得把 patch center當front/新Down cap；如果目前 BVI API不能無歧義表示該語義，先記 unsupported/abstain，保留已Down責任；不能用手填 front bypass。後續最小 typed adapter若擴充要另 freeze |
| All-lines | `CandidateBatch.lines` 是當幀觀測，`GameLineTracker::update` 另附 track_id、observed_ns、association_valid；`BVI Line` 只有center/angle/length/id | 量測幾何與線身分可信度分開。所有當前量測線仍參與遮罩／競爭，不能挑一條最長／已配線再稱 all-lines。association_invalid或不可信ID不能因轉型丟旗標後取得 eligibility。可用 sidecar拒絕這種 action；不要先靜默刪線製造唯一性 |
| Angle/unit/order | main tangent 是Vec2，BVI angle是radians；BVI使用u=(cos,sin),v=(-sin,cos)；Query有tap布林但沒有Drag/Flick語義 | `atan2(tangent.y,tangent.x)` 只轉型不補方向真值；Hold normal/front方向要獨立驗證。BVI目前只可在Tap/Hold明示範圍消費，不把Drag/Flick當Tap。ROI與line permutation須保留相同語義；distinct相鄰物件不因duplicate去重而合併 |
| Bounds | main最多128 candidates/16 lines，BVI也限128/16、metadata≤1MiB、總probe budget6,291,456 | 真正容器超限要invalid，不能截斷後pass。invalid/source/context不一致不得產生eligible hit。raw diagnostic hit與eligible action projection分欄，不以raw值直送owner |

因此最小真圖冷接點為：`Frame → 原封 GameObserver.process → 同幀 candidate_batch → 有界 typed adapter → BVI.extract/relate → shadow report`。先證明 ROI/all-lines/角度/前端語義與身份封閉，再考慮 owner消費。它不是已存在的 CLI，也沒有在本雲端跑過。

### 3.2 Owner / scheduler / FakeTouch

- `GamePlanOwner::accept(const DecisionSnapshot&)` 接受 `GameTarget`，不是 BVI Observation/Constraints。正式 `Identity`、intent、revision、ContactAlias 是 owner 的私有 bounded state；沒有 BVI hook。
- `ContactScheduler::executed_steps(intent)` 回 **absolute cursor**；`ContactPlan.prefix_offset` 在活動修訂後會trim。`cursor==0`、`cursor>prefix_offset`、沒有cursor的completed/unknown不能互換。來源：`include/pas/core.hpp:ContactPlan/ContactScheduler`、`src/core.cpp:403`、`src/game.cpp:1348,1696,1796`。
- BVI `Guard.execution/prefix/contact_id/attachment_query/free_contacts/last_contact` 是手建 fake harness輸入，不是從 scheduler receipt自動取得。`constrain()` 的 known_down只有相符字串/數字，不證明某physical body真的屬於某finger。
- `TrackingCandidate.candidate_id=i+1` 是每幀ROI序號，不能直接當長期note/intent/contact ID。最小消費adapter須保留 `frame+query index ↔ canonical query ↔ note_id ↔ intent_id/revision ↔ contact_id` 的明確來源；v3若做exact-duplicate canonicalization，attachment必須跟同一mapping走，不能按排序後的新index猜。真owner提供的已執行狀態仍是權威。
- 正式absolute cursor/prefix_offset是uint64，而研究Guard.prefix是int；不可截斷cast，也不可把「沒有cursor」直接轉成0。接手應用有界、明示的execution-state投影或owner內的typed消費seam，保留completed/unknown/retired三種責任；這個adapter尚未實作。
- main已有 alias與同指 body Move，不應把「新增owner/alias」當空白任務重寫。最小 integration 必須讓同一owner保有 intent/contact/release責任，BVI只能供給有界 current支持／否定，不能成為第二touch owner或直接inject。
- 建議冷控制使用原封 `SessionPerception + SessionGameOwner + ContactScheduler + ReplayTouch`；以實際接受plan／fake receipt生成候選attachment state，按當前ROI關聯再消費。不能由舊events或人工`known_down`注入候選策略。
- 必驗：合法新Down機會；同contact旋轉Move；pending失效；5指全占用；錯contact；unknown Down不重試；completed不復活；截短prefix；geometry/epoch/gate/expiry；tail正常結束與失效釋放。安全拒絕與合法機會雙分母都要報。
- `apps/frame_review/replay_support.hpp::ReplayTouch` 有 bounded receipt/bytes、錯contact/duplicate Down拒絕、unknown/failure/釋放控制和幀間due規則。`tests/contact_replay_tests.cpp`與`tests/x10d_p_owner_tests.cpp`提供控制參考，但後者依隔離X1/P source的replay_state插樁；**它們沒有全部註冊於root pas_tests，不能用普通CTest成功冒領這些case。**

### 3.3 成本與 Windows 不是同一問題

| 層 | 可用入口／現況 | 尚缺的證據 |
|---|---|---|
| BVI計數 | Candidate::metadata_bytes、Observation.probes；v2最高已測558720 | 不是CPU worst-case/p99。extract目前還會每幀hash全部RGB；1280×720即921600 pixel visits，且line-mask幾何運算未等同pixel probe。要量完整adapter＋extract＋relate＋owner／archive成本 |
| 主鏈離線成本 | `pas analyze game-cold-pipeline`用LatestFrame、GameObserver、GamePlanOwner、FakeTouch、Journal；CLI在`apps/pas/main.cpp:574,629` | main目前不呼叫BVI，直接跑這個CLI量不到候選增量；先 integration + compiled closure，不把 baseline cold-pipeline結果借給v3 |
| 舊X11/R3 meter | `apps/runtime_x11_p_r3/CMakeLists.txt`連`out/x11-p-r1/.../pas_core.lib`及其include | 它量的是凍結C36h/pending-only B0/B1，不會因repo出現v3就量到v3。改成本入口要新版本、source/linked object maps與trace on/off等價，不重開舊STOP |
| 有效成本gate | R3曾AA5/ABBA0/stress0，baseline owner_seen2282/2560=89.140625%低於原90%；noise未取得資格 | 原負結果保留。不得放寬90%、只看success subset、減diagnostics或原樣重跑追綠。新候選method先凍結，A/A noise/完整分母合格後才A/B；記n/p50/p95/p99/max/jitter、skip/drop/late/failed/unknown及release |
| 正式Windows build | root CMake用C++20、gRPC1.81.1 EXACT、CLI11/json/protobuf/gtest/FFmpeg/zlib，link bcrypt/windowscodecs/ole32/D3D/DXGI/user32；`core.cpp`用Windows QPC | GCC translation-unit通過無法替Windows SDK/link/DLL/ABI/QPC/OS排程簽收。不能為在Linux測試而抽換正式clock/owner核心 |

成本證據必須對應 **將進入game的exact source+binary+profile+loaded dependencies**。現有X12仍not-ready；需要新有效資格，而非宣稱v3冷測好就自然繼承。

## 4. 最小下一個 Windows 工作包（本輪未執行）

### W0：身份、依賴與離線 build

1. 使用者指定的Windows checkout，先讀AGENTS、Goal、本文件及最終round4結果；另讀該checkout `.agents/skills`中相關指引。記`git rev-parse HEAD`、`git status --short`、候選/source/header/dirty diff SHA。不覆寫使用者未提交檔。
2. 確認VS2026/v145、Windows SDK、CMake≥3.28、實際VCPKG_ROOT及安裝的JSON依賴。root preset綁VS18 BuildTools 18.9.12105.275；ASan支援根又指Community14.50.35717。這是machine-specific路徑，必須核本機，不能默默換compiler版本。
3. 保留vcpkg baseline `93c50752b23e350ca6b9063a167f0a4cf8a3b3eb`與`third_party/vcpkg-overlay/grpc`，尤其256KiB read-chunk patch。CPU Torch預設OFF；不恢復已刪舊recovery runtime、不下載模型。
4. 在新output root做Release/Debug/ASan與`ctest --test-dir <new-build> -C <config> --output-on-failure`。最小新Release命令是 `cmake --preset windows-release -B out/<fresh-id>-release -DPAS_ENABLE_CPU_VISION=OFF`、`cmake --build out/<fresh-id>-release --config Release --parallel 4`。先核fresh-id不存在與preset所有cache路徑；ASan/Debug對應preset和配置。退出碼與實際test數/skip必記，空CTest不是pass。
5. BVI Windows harness要對最終v3另建target與fresh attempt，逐原suite、新契約、獨立suite分列；使用現有six inputs、typed-singleton IO差異及oracle，不從候選輸出生成gold。最終round4驗證文件會列精確source/runner命令；沒有exe與receipt不得標run。

停止：configure/build/link/依賴身份不明就留原log，做最小定位，未解前不進profile/device，更不叫live-ready。

### W1：若碰舊 wrapper，依收據分層恢復

[round2 Windows 收據](../round2/WINDOWS_RECEIPTS_UPDATE.md)已核A9 root native/runner/verifier=1/1/1、41-byte syntax error和成功自然收尾。D19/D20則在交易root字串guard前測就被拒絕，actual CreateProcess=0。二者不是同一已證根因。

不要直接執行舊`initialize/run`：它們綁舊絕對根、HEAD、594 paths和不可續跑STOP。新必要入口沿未改owned/identity/transaction核心，依序：

1. canonical `Join-Path` case root＋完整INIT nullable schema；驗D19 reject、D20 positive整條reserve→RUNNING→result→receipt→readback。
2. 檢查`FinishDiagnostic`先寫`tx.dir/state.json`卻讀全域`Evidence/state.json`的source-level後續阻塞；**尚未實修／實跑**，不可聲稱只改slash就20/20。
3. 新凍結允許清單內做最小argv fixture/version probe，收lpApplicationName、真正CreateProcess命令字串、`%cmdcmdline%`、entry、vcvars start/end和即時exit、target entry、0/7返回及Job/held/streams/cleanup。
4. 首份trace指出失敗層後只修該層。安全核心/control未通過不啟動候選產品命令；保留所有失敗、source版本與STOP，另開fresh-root，不放寬guard。

不用為接v3重寫整個恢復平台；但若採其啟動鏈，不能略過它的實際門檻。

### W2：profile／五contact核實，再真圖cold

- 先離線檢查 `configs/phigros-hd-assist-five-lead35.json`：gRPC payload/RGB888/top-down/1280×720/rotation1/256KiB；touch720×1280/90°/5 contacts；evidence100ms、plans128、steps16、horizon350ms、lead35ms、uncertainty30ms。profile不按歌調參。
- 已有capability有實物檔案才核SHA與內容。`max_contacts=5`宣告不是`max_contacts_verified=5`證據；`match_touch_capability`還查serial、APK、mapping/geometry、Android SDK/ABI/model/wm-size/density。
- `game-preflight --config <verified-profile> --capability <verified-report>`會讀目前裝置/擷取，但不建立input；它只簽historical capability fingerprint，不簽gameplay。這已涉及裝置，須在使用者移交的指定環境/授權內才做。
- 若實測five-contact缺失／指紋不合，只在另有裝置觸控授權時做bounded Native Touch Fixture four/five、逐指Move/錯開Up、cancel、final active0；不偷偷開始遊戲。實際Fixture操作的明確命令參考`GAME_ACTION_SEMANTICS_20260927.md`，先確認使用者可配合前景app，不照搬舊output根。
- 再用下節最小真圖原件完成same-frame ROI/all-lines shadow與owner冷整合，通過後才做exact候選有效成本。新真實pixels可在不啟動任何live策略下分析。

### W3：安全／成本資格都成立後，才是有限實機

這是後續Codex task的條件步驟，**本cloud並未獲准執行**。須有使用者在指定裝置的有效移交及操作授權。每次僅一個手動Play輪次；使用者選曲／難度，禁止auto-start、自動選曲／連打／自動重試。

使用exact已驗exe、profile和capability；可用 `pas.exe manual-session --config <profile> --capability <report> --one-round --no-preview --round-watchdog-s <預先約定上限>`。若本輪需要完整接觸診斷，另用`--full-recording`（自帶one-round），先預留5GiB錄影與必要archive空間；不把三幀pixel-clips當完整contact因果錄影。watchdog只停止，不證明結算。

先選一個能回答已明示缺口的有限測試，原始結果和不利结果都保存；Miss非零或fault先診斷再決定下一輪，不反覆Play找最好分數。未知注入/釋放、geometry/profile變更、gate/source失效、容量/recording/archive fault、按Escape/Ctrl+C立刻停止並核contact釋放，unknown如實保存。配置或策略改動後重新freeze，不能與前候選的好結果混成最終同版驗收。

## 5. 最小缺件，按問題索取，不重索整包

已提供的小包就在雲端`/workspace/shared/phigros-evidence-round2/unpacked`，含six-suite所需五份包內inputs、第六supplemental在repo，以及A9/D6和C36h Dlyrotz M77結算。其manifest/derived-SHA已核。**不要再索取這些同樣bytes，另一executor的路徑不同也不等於要求使用者重製包。** 先在使用者本機找到既有原始來源；需要搬移時用已提供附件的正常下載/材料化途徑，核當地實際檔案。

| 目的 | 最小新增／本機讀取 | 足夠與不足的界線 |
|---|---|---|
| rails/端部/斜交/effect真色校準 | 選定少量原生RGB/PNG連續短窗，附ordinal/source_frame/capture_complete/pixels_ready/context/layout/SHA與選片理由；至少有合法正例、遮擋/無rails/neighbor反例 | 看實際pixels並記人工review/proposed/unknown。只看renderer alpha、overlay、結算圖不能判真實合成色或可見cap；不能按case改predicate |
| 合法ROI/all-lines橋接 | 同短窗可重跑的原封GameObserver＋candidate batch sidecar，包含所有當前lines與origin/head/body/association旗標 | 手填geometry只可隔離extract問題，不算真detector完整性；漏提取物件本身要計入分母 |
| contact完整prefix／ownership反例 | 現有session manifest、profile、round manifest/summary列的必要events segment原bytes；full-recording index及從round reset或可驗checkpoint到目標窗的received PNG序列 | 真實原events只供比較/查receipt，不回餵候選。90ms視覺warmup不恢復數秒前已Down的Hold；只有目標三幀不够。若要模擬候選owner，必須由其自身prefix建立contact，不能seed原版按鍵 |
| 當前章節分母與IN解鎖 | 當前遊戲version畫面、完整Chapter Legacy逐屏清單、每曲IN可選/鎖定畫面及拍攝時間；可遮帳號區 | 歷史20曲不是保證當前N；舊Dlyrotz/光IN結算不等於現在全部已解鎖。先核已開IN，只對仍鎖者做必要HD流程 |
| 候選實機結果 | 同一freeze每曲完整IN result.png、session/round manifests、summary、event segment hashes、執行exe/profile/capability/game version對應 | 明報P/G/B/M/score、完成或aborted、release/fault。一次歷史M77圖不新增成新輪次；cancel/combo不是逐note Miss真值 |

不是当前最小所需：整個out/cache/SDK安裝樹、APK或模型整包、帳密/token、全量舊7722張PNG先搬雲端。若完整prefix確實需要较多本機原件，可就地重播而非先上傳全部。

## 6. 何時synthetic不能繼續代答

- 真實note/line/effect的顏色、反鋸齒、遮擋與幾何觀測；renderer fixture只能測被指定的分布，不能裁定遊戲的actual可見性。
- current提案是否漏掉合法物件、是否有兩個physical Hold／同物件多ROI。相同可觀測pixels允許unknown，不用預期owner當gold。
- 某Down/Move/Up在遊戲內是否採納、Hold接續/tail結束是否正確。改touch後產生的新回饋不能由固定錄影重播或FakeTouch製造。
- 真Windows QPC/driver/RPC/五指mapping與系統負載尾部成本；Linux函式計數無法測出。
- 當前Chapter Legacy完整N、IN解鎖、完整結算Miss=0；必須當前遊戲畫面。

這些是證據門檻，不是授權再合成更多例子便能關閉的TODO。下一個task按W0→W2真圖cold→有效成本／安全gate→W3有限實機推進；任一必要證據缺失時明列最小問題後停在該門檻。
