# X1：C36h／main50 完整 fixed-pixels contact replay 交接

完成日期：2026-10-02（Asia/Taipei）；依指定保留本文件的 20261001 檔名。範圍依 [現況研究 G1／H](PROJECT_STATUS_NEXT_STEPS_20261001.md)：**X1 冷驗工具與五案例因果交接完成**。C36h 為研究 baseline，main50 為比較／donor；未做 X2／X3 策略介入，未升格 main50，未增加 live round。

## A. 交付與閱讀順序

1. [固定輸入 manifest](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/input-manifest.json)、[來源設定 audit](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/input-source-audit.json)。
2. [C36h source provenance](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/source-v2/c36h-source-provenance.json)、[main50 source provenance](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/source-v2/main50-source-provenance.json)、[工具／binary／test／export freeze](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/tool-freeze-verified.json)。
3. [五案例窗口比較](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/five-cases-verified.json)、[完整可用前綴的第一動作差異](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/prefix-first-divergence-verified.json)。逐例獨立檔見 D。
4. [determinism／CLI 負例核對](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/check-verified.json)、[六個最新 run／四個政策 control 帳](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/run-audit-verified.json)、[歷次失敗／skip 帳](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/attempt-ledger-verified.json)、[容量／artifact SHA 帳](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/capacity-ledger-verified.json)。

工作目錄保留了各次 ignored measurements 與 `out/x1`，沒有提交、push、merge、刪除舊 evidence 或傳訊其他 chat。`docs/PROJECT_STATUS_NEXT_STEPS_20261001.md` 原有使用者／父 chat 改動未修改，SHA 仍為 `cdfc49dac07420498e653a0fb71ca7c854a7888c7d8cc50bb41283d2af7c9b0c`。正式 `src/`、`include/` 策略檔未動；診斷插入只在隔離 export。

## B. 凍結來源與設定

| 項目 | 實際使用與驗證 |
|---|---|
| 工作 HEAD／main50 source | `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`；50／27／11，live=0 |
| C36h freeze | 37／19；freeze commit `e23b3f9edfc2df5c6e1ce1aa4c8168af8aeea726`、dirty=true；保存 commit `98169a575bd7c3b7503849ceb4b91a934d50ce0f` |
| C36h 原 runtime binary | 實際重算 SHA `61ecfacd2cc48ed270506719a1089be646d915c64e9026706ac1654e0d4bbce9`；沒有重建或覆寫它 |
| C36h source 邊界 | freeze 所列檔案逐一 SHA 相符；正式 src/include/apps/tests 與保存 commit 忽略換行後內容相同。CMake／分析文件有後續差異，記 false 並保留 snapshot 原文；generated provenance 檔不冒稱經 Git 比較 |
| final C36h replay tool | `out/x1/verified-tools36/pas_frame_review.exe`；SHA `438508987ec885873549582637b0521bc55c0e5ea0cf3c3cfa7d1675879ede95` |
| final main50 replay tool | `out/x1/verified-tools50/pas_frame_review.exe`；SHA `f402388a3316a251cc8d2b4fce2c05e30a9ccb64c229aefc1137d329a4838dfd` |
| prefix report tool | `out/x1/verified-tools36/x1_subject_prefix_report.exe`；SHA `b62c588ea2ae4171bd5a542ee6b5d95e6f6ebf0dbac2f8cdb806b6cf123c3e09` |
| R full recording | `full-recording36g-01/sessions/manual-session-22885039263800`；7,722 PNG；index SHA `0bd8ec74b6bc19d1f8080c064453a052095e328e31d2759f12d6d04d78500f8d` |
| original journal | 21,184 rows，2 segments；SHA `ac1c86818a428ecb44a04fdeb3299f2f25459329f8c0c142aec7cd3e2920d6e0`、`75188c702d814fede7a4d1c1896e8362025f07ee7c00b54fe79b2fb6c69d3a72` |
| replay input manifest | SHA `99e8a364ea9f8575d67fbe39b1b76cccd6820816bc4d999d1b1bdfcfec2dd955`；五窗共 109 幀 |
| profile 差異 | H 的 `acceptance36h-01/profile.json` SHA **4201679…ef18b**；本 run 的 `configs/phigros-hd-assist-five-lead35.json` SHA **d2b5cfd…bd0e0**。逐欄比較 H／original session／本 profile，**只差 log_dir**；不是把兩個 file SHA 說成相同 |
| 有效 policy | all four types、lead 35ms、uncertainty 30ms、五 contact；owner 原生 scheduler 128 plans／16 steps／350ms horizon／100ms evidence。1280×720 RGB888 top-down、source rotation1；原 preflight fingerprint matches=true、無 mismatch，僅核對歷史記錄，沒有查詢裝置 |

每個 lineage 獨立連結自己的 SessionPerception、GameObserver、GamePlanOwner、SessionGameOwner、ContactScheduler，沒有跨版 ABI。C++20 離線 CMake 不建 GameRuntime、emulator capture/touch transport 或模型；原 LibTorch 路徑不可用不影響本工具。FakeTouch 命令以 **frame pixels** 記座標，沒有 Android RPC／native rotation 注入；原 mapping 指紋留在 audit 中，不能因此宣稱新實戰 mapping 驗收。

## C. 已實作 replay／trace 契約

入口為 [frame_review/main.cpp](../apps/frame_review/main.cpp)、[contact_replay.cpp](../apps/frame_review/contact_replay.cpp)，共用 [review_io.hpp](../apps/frame_review/review_io.hpp)。[prepare script](../tools/prepare-contact-replay.ps1) 由固定 commit 建新 export、核對 frozen source，再加只讀診斷；before／instrumented SHA、插入位置、排除測試名稱均保存。新 export／同名 provenance 必須不存在，不覆寫先前嘗試。

- **輸入**：核對 index/profile SHA、唯一 decision frame、ordinal/source_frame/capture/ready 單調、geometry/domain、全部 journal SHA；全部 7,722 PNG 每次都 SHA／decode／geometry 驗證。缺檔、錯 SHA、join 不完整不默默略過。五窗外仍從最早可用 preroll 暖機到 EOF，不按 clip reset。
- **cadence**：default=recorded-owner-consumed-set 7,684＋preroll 32，union 後 7,715 calls，7 幀不進 perception；capture envelope 和 PNG 驗證仍涵蓋全部 7,722。all 模式另做 7,722 calls。原 decision 只供 frame 集合及外部 comparison，不供 target/root/line/plan/receipt 給策略。
- **時間**：`fake=original−23084035991700+1s`。capture arrival 更新最新 capture，pixels 在 ready 交付，保留兩者間隔；recognition／RPC 的 fake duration=0，實際 CPU cost 不推進 FakeClock。source timestamp 另存，render age unknown。
- **due／gate**：[advance_before_frame](../apps/frame_review/replay_support.hpp) 逐次處理最早 `next_due_ns`，包含 gate expiry／plan期限／steps；預設同時刻 frame-before-due，另測 due-first。時間倒退、同時刻無進度、容量越界立即報錯並停止／release。dispatch guard 使用自身 reconstructed allow_down／active round／最新 capture <100ms。
- **session**：UI、round、allow_down 由自己的 pixels lifecycle 重建；new-round reset observer、owner 按自己的 round 開始，gate false 走原 owner 撤銷與 retirement，像素 RESULT 確認後 finish。source epoch/generation/geometry=1 為明示假設，round epoch另列；未還原原 perception thread gate／race。
- **接觸**：[ReplayTouch](../apps/frame_review/replay_support.hpp) 同步向外送每個 receipt，沒有 receipts 累積 vector。預設成功、零 RPC 耗時、五指；成功 Down 是這個 counterfactual backend 的回覆，不等於遊戲接納。合成腳本另驗 delay、unknown Down、unknown/failed release。unknown Down 不 retry，首次 release report 保留；EOF／fault清空模擬接觸並保存 unknown。
- **診斷**：每次 accept/poll 排空 accepted plans、coverage、cancellation、scheduler notices。trace 含 candidate bank、候選→track assignment／ambiguity、每候選最多16 identity alternatives、line gate／score components／preserve／winner、history clear 的 instrumented source site、owner cursor／alias／evidence／期限、scheduler 和 contact。額外 edges 數明列，不能把診斷前16條當完整 assignment graph。window 外保留 bounded identity 的首次可用摘要／本 run 成功 Down／最後 receipt。
- **界線**：index最多36,000 frames／64MiB、JSON row≤2MiB、PNG≤4MiB、preroll≤32、五窗各≤120；owner summary≤128、line≤16。trace 使用一次 lazy fixed storage，含 bookkeeping ≤16MiB；drain 後 reuse，越限 throw，不 silent drop。receipt/release≤100,000／32MiB，inject 前拒容量並保留 terminal release slot；全事件≤100,000，磁碟 stream同時受 batch／campaign 額度，留1MiB給summary。
- **writer／fault**：最新binary用host named mutex讓完整contact run只有一個writer；第二個同時啟動立即`batch_writer_busy`，不以各自舊容量快照競爭。CLI負例已實測拒絕。sink在release後throw、scheduler尚未接到report時，外層先保留backend非空report，再finish，避免被第二次空release覆蓋；receipt容量測試追加此unknown release負例。metadata／compare維護仍由caller順序執行與總帳核對。

每個 run 的 `events.jsonl` 為全錄 counterfactual事件；`trace.jsonl` 為109選窗＋前綴摘要。`original-recorded.jsonl`／`original-prefix-events.jsonl` 完全獨立，保留原 C36g raw events；join 分 `exact_source_frame` 與 `journal_order_latest_decision_not_exact_frame`，不把後者說成精確事件發生幀。無 frame join 的缺口為0。

事件的`ordinal_at_delivery`實際是event-loop當次將處理的索引；幀間due可能在該索引capture／ready之前觸發。D的6215標記因此不是「先使用6215 pixels再Down」：精確因果看scheduled／injection_start／return與命令source_frame17981（6214證據）。下表action ordinal沿用此索引標記，不冒稱實際畫面交付時刻。

Canonical semantic digest 包含每幀 scene／lifecycle／owner identity與cursor／contacts及所有 plan、cancel、receipt、release，排除 CPU cost、絕對輸出路徑與只讀 trace。early_state_unknown=true；本 run 像素 RESULT 已確認，input_truncated=false、exit contacts=0；這仍不是 counterfactual 遊戲世界的歌曲完成證明。

## D. 五案例與第一差異

逐幀 correspondence 為 **proposed geometry matching，human_gold=0**：同 kind／centroid≤16px、幾何差≤2px、line tangent差≤.02、root差≤10µs。每幀 greedy一對一；跨幀 cohort 可含 split/continuation 多個 ID，不按兩版 ID 數值配對。`first_evidence_divergence` 指所選 note 幾何／body flags，在此容差下五窗皆無；line pose／relation／samples／fit reason差異列為 state，**不表示全 candidate/line bank 完全相同**。缺對應 occurrence=0不是物理正確率。

| case／獨立報告 | 窗口內 state 第一差異 | 可用前綴 action 第一差異 | 窗口內 action 第一差異 | 本 run 成功 Down C36h／main50 |
|---|---|---|---|---|
| [A3498](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/case-A3498-verified.json)，3493–3504 | 3493，所選 Hold cohort 的旁側候選 fit/relation | **3488 Down** 時間／位置差 | 無，9／9 commands一致 | 各1；均3488 |
| [B4986](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/case-B4986-verified.json)，4979–4990 | 無 | **4964 C36h Move／4965 main50 Move** | 無；各1 Up | 各1；均4963 |
| [C5520](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/case-C5520-verified.json)，5516–5524 | 無 | 無，6／6 commands一致 | 無 | 各1；均5516 |
| [D6214](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/case-D6214-verified.json)，6158–6220 | **6160 初始 relation** | **6215 C36h Down／main50無動作** | 同左；6／0 commands | 1／0 |
| [E5287正常control](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/case-E5287-verified.json)，5281–5293 | 無 | **5293 C36h Move／main50尚無Move** | 同左；2／1 commands | 各1；同一5293 Down |

前綴 action 比較由 [subject_prefix_report.cpp](../apps/frame_review/subject_prefix_report.cpp) 讀完整輸出事件，驗 parent comparison 的兩個 summary SHA，對該 proposed subject cohort 從最早輸入比到 window.last；仍不宣稱所有物理 Note 的全世界第一差異。B 的第一 Move 比較在選窗之前，當時完整 per-frame scene未輸出，前因為 unknown；保留了完整 action而沒有借原journal補狀態。

### A：3498 有 body／no-root，接觸確實維持

**Verified**：C36h Hold945 的 Down 在3488，fake time 60,580,580,069ns、(303.79998,576)；main50 Hold949 同ordinal、60,576,079,082ns、(300.83389,576)，相差4.500987ms。3498兩者 current body／rails=true、patch=false、中心(303.59807,604.78398)；C36h samples4/root_past，main50在 history site438的 reversal/jump reset 後 samples1/motion_discontinuity，root都null。兩者 `held_support=true`、對已持有identity `compatible=true`，cursor為10／9，evidence刷新，contact持續Move；主Hold沒有 alias改接或Up。窗口的9條 subject命令相同，前綴總14／13條與Down不同。

原 **C36g** Hold975 的3501 `current_held_region_unsupported` cancel保存於original stream，不能搬成C36h/main50的cancel。**本窗否定H5「無root即premature Up」**；也未證明3498之前head取得／遊戲採納相同。3493 cohort首差來自旁側958／961，不能冒稱主Hold的第一failure就是3493。

### B：4986 absence，取消不是 tail 真值

**Verified**：C36h1526／main501547均4963成功Down；前綴第一Move為4964／4965，窗口內沒有對應current Hold。兩版在4986 fake time85,988,537,200ns Up，source evidence16750；cancel source_frame16753、reason=`current_object_missing_or_region_lost`、last evidence85,926,539,900ns，相隔61.9973ms。沒有借body續租／新Down或retry。原C36g1562在同anchor亦有absence cancel，但原RPC結果只算effect_unverified。

**Strong inference**：此窗符合缺支持後60ms grace撤銷。**Unknown**：tail自然完成、遮蔽、真miss或遊戲是否已採納；absence／cancel不能升格completion。前綴Move差異未由本窗的absence倒推原因。

### C：5520 Flick核心合併正對照

**Verified**：本次C36h1571／main501592各一Flick核心、root／幾何在容差內一致，各5516成功Down，六條動作從可用前綴到5524完全一致。原C36g在5520仍是1606／1607兩個分片target；三欄不混用。它驗的是兩候選目前merge與動作一致，不能證明遊戲判定，亦不能代表所有Flick皆正確。

### D：先是初始配線，再是split／preservation，最後才Down分歧

本例selector固定為 Drag `250<y<325`，避免把6158已存在的y≈359另一Drag當本例。舊 `five-cases-v1.json` 的寬selector報6158，已保留但不作本例結論。

| ordinal | C36h trace | main50 trace |
|---|---|---|
| **6160出生** | 新1622、(20.99514,287)；水平線因role_alignment被拒；垂直356 score917.60486、role_class1，preserve=false | 新1643同幾何；垂直315 score917.60486；近水平316 score26.59611，distance13.999998＋orientation11.996114＋confidence.6；另一305 score289.12119。選316，confirmed=0、preserve=false |
| 6170交疊 | 1622 identity_ambiguous=true；fragment另有候選；preserve356但final relation無效 | 1643亦ambiguous；選／preserve316。不能把selected winner直接當有效target relation |
| **6174–6175** | 幾何延續1622、保留356 | 6174承接的是fragment1644，assigned prior index5、ambiguous=true；6175配305、samples1，history site405是line/history continuity reset |
| **6197** | 此幀垂直線缺失，fallback359、site195 clear，samples1 | 有垂直315，但preserve confirmed305；其score較近水平317高，winner仍305。C36h整段並非oracle |
| **6214** | 恢復垂直360(x988.5)，samples4、root106,745,505,268ns；owner已提交intent504，cursor0，Down due106,695,505,268ns | 垂直318明明存在且score**53.69926**低於水平305的**171.40163**，但 `confirmed_line_id=305`、preserve=true、winner305；samples5、relative_velocity_small、rootnull。對應identity尚未submitted |
| **6215** | 幀間due觸發成功Down，source17981、(988.5,287.99710)；窗口共6 commands | 選定1643／1644 cohort前綴及窗口 **0 commands／0成功Down** |

**Verified**：第一relation差異6160當時沒有confirmed preserve；6214則是目前分數會選垂直、confirmed preserve覆蓋winner。main50未產生所選subject的plan／Down；對照source，沒有root且沒有current spatial overlap時，new-contact guard拒絕規劃。global `owner.last_rejection`可能是更早遺留值，不拿它當此target的即時拒絕原因。

**Strong inference**：距離主導的初選、identity split continuation與後續confirmed保留共同構成這條鏈，比「垂直線一直没被看到」更符合輸出。**Hypothesis**：只改一種機制能否修復整鏈尚未測。**Unknown**：6170物理identity gold、C36h6197缺線的最佳處置、何種介入足以改Down／遊戲Miss。不能稱main50多一個Miss。

### E：正常control，owner Move條件本來就不同

**Verified**：兩版5293成功Down完全相同（source17059、fake time91,088,035,879ns、(773.56683,576)）；當幀root／note幾何一致。5293的revision27計畫中，C36h新增Move到(783.97057,576)、due91,095,456,600ns；main50未新增Move，只更新Up。只讀guard顯示main50 `held_support=false`、`compatible=true`；target為head_on_line=false／held_body=false／rails=true。C36h active Hold Move分支使用samples>0；main50另要求held_support、body compatibility與tail guard，因此在head已Down、當前尚無held support時分歧。

此案例仍為使用者更正的 **normal control**。前綴Move差異或不同guard本身不是failure、unsupported Move或遊戲收益的gold，不據此做X3修改。

### 政策敏感性

[all-frame比較](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/five-cases-all.json)／[其prefix](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/prefix-first-divergence-all.json)，以及 [due-first比較](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/five-cases-due-first.json)／[其prefix](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/prefix-first-divergence-due-first.json) 的五例first差異／Down有無／窗口command數一致。all-frame會改全錄digest與部分ID（例如C36h+1）／event數，不能宣稱cadence無影響；due-first與default在本錄的全語義digest也相同。這兩個政策都未涵蓋真recognition／RPC延遲、未知source render age或未錄thread race。

## E. 驗證、成本與失敗分母

all／due-first四個政策control及observer-compat是前一個工具binary的冷驗，SHA留在各summary。最後只補writer門控／fault-report保留，正式strategy、clock、trace hooks及成功路徑未改；最新binary重新跑兩次＋trace-off的全錄digest與前版default完全相同。未把前版control說成最新binary的新run，也未為重跑刪掉它們。

| 驗證 | C36h | main50 |
|---|---|---|
| final Release離線套件 | **197/197，fail0、skip0** | **224/224，fail0、skip0** |
| Debug＋ASan同套件 | **197/197，fail0、skip0** | **224/224，fail0、skip0** |
| 同binary同input on-1／on-2／off | 三run digest相同；`54ce21932b25eabf5d552f49b3020fa31856ba9c0ffe1c3d981a0b7bb79ec4ae` | 三run digest相同；`c58428a4d4ecfa72253fb38f15de90771907a28bf3101cf0cf384225b5b28119` |
| observer-only原reference | [3,722 checked，different0；7,715 calls](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/c36h-observer-compat-final.json) | [3,722 checked，different0；7,715 calls](../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/main50-observer-compat-final.json) |
| default全錄事件／receipt+release | 6,115／2,165 | 5,964／2,121 |
| all-frame事件／receipt+release | 6,127／2,168 | 5,976／2,123 |
| 峰值contacts／identity摘要／trace bytes | 4／42／11,981 | 5／52／18,370 |
| 最新binary full runs | verified-on-1、on-2、off均success；每run全部7,722 PNG驗證、join缺口0、exit contact0 | 同左 |

XML／原log以 `tests36-verified`、`tests50-verified`、`tests36-asan-verified`、`tests50-asan-verified` 保存。套件是各lineage的game／tracking／session／core與10項新增 ContactReplay測試；**未宣稱repository所有測試皆跑**。兩個GameRuntime transport案例 `ActualCaptureOptionsUseEffectiveProfileGuard`、`HistoricalFingerprintRejectsMappingDeviceAndUnverifiedReports` 明確排除，另列於provenance；不列為通過或skip。原歷史pixels／menu/result fixture可讀，opt-in27 PNG由PAS_RGB_CLIP_ROOT啟用。

新增測試覆蓋幀間due、tie撤銷、gate expiry進度、unknown Down不retry／unknown release、receipt count/bytes拒容量與terminal release、trace容量與drain、早期body無borrowed Down、unknown UI無round、腳本delay、failed release、clock regression、pixels→observer→owner→receipt trace-on/off一致。原有lineage測試涵蓋head/body/tail、active無root／missing evidence、pending／completed／returned、旋轉期間Hold持續Move及late alignment。CLI四個負例（錯indexSHA、existing-output、outside-batch、mixed-policy compare）都以預期原因拒絕，沒有改寫既有輸出。另有1個競爭writer負例，最新binary以batch_writer_busy拒絕。

**歷次嘗試沒有從分母隱藏**：23個完整replay summaries均保留（7 preliminary＋10前版final＋6最新verified）；最新binary驗收用已凍結的6 run，四個前版政策control另列。初始8項新增測試有1 fail（unknown注入序號設在arm初次release之前，後修正test腳本）；早期套件各有2 skip（fixture root／環境未設定，已補成final零skip）；兩次舊compat工具呼叫因moved-json取ordinal的evaluation order錯誤失敗，修正工具後零差異。compile／link／prepare的中途錯誤亦留原logs／exports。ASan36第一次DLL copy誤指Debug z.dll（實際zd.dll），測試仍通過；明確copy zd.dll後又驗一次。這些是工具／test工程過程，不混入final成功率，不說所有歷次執行都成功。

環境：Windows NT10.0.26200、x64、Core Ultra5 125H、RAM34,037,383,168B；MSVC19.51.36256.0／v145 Release。以下為default on-1全部7,715 calls，單位ms：

| lineage | n | p50 | p95 | p99 | max | jitter p95−p5 | replay failure／skip perception |
|---|---:|---:|---:|---:|---:|---:|---|
| C36h | 7,715 | 2.3272 | 6.33457 | 11.22948 | 22.0377 | 4.82683 | 0／7（另32 preroll） |
| main50 | 7,715 | 2.3884 | 5.42973 | 7.98145 | 13.5092 | 3.78288 | 0／7（另32 preroll） |

成本scope為perception＋accept/poll/drain＋事件digest/event writer；**不含PNG hash/decode、外層每幀semantic digest及選窗trace序列化，不推進fake clock**。編譯／ASan／部分前版replay曾並行，不能以這些值推論受控速度A/B、live端到端latency或deadline通過率；未把各stage p99相加。全部run分布／失敗數另存run-audit。

## F. 重跑、容量、未知與下一個單項實驗

以下在repository PowerShell執行；所有 output須用新名稱。`$x1Batch`、manifest、provenance都使用本机既有檔；先核對freeze／capacity，再重跑，剩餘額度不足不要再複製整批。固定inputs沒有複製全錄。

目前剩7,214,512B不足再新增一組完整A/B（約11MB）；以下完整重跑指令是已驗政策的重現規格，需後續先決定資料保留／容量配置，不能另root繞額度。本次沒有為騰空間刪掉舊run。可先用既有`c36h-verified-on-1`／`main50-verified-on-1`執行contact-compare與prefix report產生小型新報告，或只核對freeze／inventory，不新增full replay。

```powershell
$x1Batch = 'measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1'
$x1Manifest = "$x1Batch/input-manifest.json"
./out/x1/verified-tools36/pas_frame_review.exe contact $x1Manifest "$x1Batch/source-v2/c36h-source-provenance.json" "$x1Batch/c36h-recheck-new" on owner frame-first
./out/x1/verified-tools50/pas_frame_review.exe contact $x1Manifest "$x1Batch/source-v2/main50-source-provenance.json" "$x1Batch/main50-recheck-new" on owner frame-first
./out/x1/verified-tools36/pas_frame_review.exe contact-compare $x1Manifest "$x1Batch/c36h-recheck-new" "$x1Batch/main50-recheck-new" "$x1Batch/five-cases-recheck-new.json"
./out/x1/verified-tools36/x1_subject_prefix_report.exe "$x1Batch/five-cases-recheck-new.json" "$x1Batch/c36h-recheck-new" "$x1Batch/main50-recheck-new" "$x1Batch/prefix-recheck-new.json"
```

`off owner frame-first`驗trace關閉；`on all frame-first`、`on owner due-first`分別改單一政策。observer相容命令使用原時間、不經session lifecycle：

```powershell
./out/x1/verified-tools36/pas_frame_review.exe observer-compat $x1Manifest measurements/game-assist/2026-09-30-m0-manual-continue/full-recording36g-01/frame-analysis36h-v3/replay.jsonl "$x1Batch/c36h-compat-recheck-new.json"
./out/x1/verified-tools50/pas_frame_review.exe observer-compat $x1Manifest measurements/research-next-20261001/main50-observer/replay.jsonl "$x1Batch/main50-compat-recheck-new.json"
```

由source重建例（main50改Lineage、source／build目錄；ABI分開）：

```powershell
./tools/prepare-contact-replay.ps1 -Lineage c36h -ExportRoot out/x1/c36h-rebuild-new -EvidenceRoot "$x1Batch/source-rebuild-new"
cmake -S apps/frame_review/offline -B out/x1/build36-rebuild-new -G 'Visual Studio 18 2026' -A x64 -T v145 '-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275' "-DX1_SOURCE=$pwd/out/x1/c36h-rebuild-new" "-DX1_REPO=$pwd" "-DCMAKE_PREFIX_PATH=$pwd/out/vcpkg_installed/x64-windows"
cmake --build out/x1/build36-rebuild-new --config Release --parallel 4
Copy-Item -LiteralPath out/vcpkg_installed/x64-windows/bin/z.dll,out/vcpkg_installed/x64-windows/bin/gtest.dll,out/vcpkg_installed/x64-windows/bin/gtest_main.dll -Destination out/x1/build36-rebuild-new/Release
$env:PAS_RGB_CLIP_ROOT = "$pwd/measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
./out/x1/build36-rebuild-new/Release/x1_tests.exe --gtest_brief=1
```

ASan使用獨立build dir，configure另加 `-DX1_SANITIZER_SUPPORT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include`，build `--config Debug --target x1_tests`，copy Debug `zd.dll/gtest.dll/gtest_main.dll`。CMake依專案既有方式複製available sanitizer headers/runtime，未安裝新工具；目前Release replay binary未被Debug覆寫。重新編譯必記新binary SHA，不能預期相同compiler source一定bit-identical。

| 容量帳（bytes，含所有失敗／舊tool產物） | 數值 |
|---|---:|
| 前置快照campaign＋prior research | 7,677,068,112 |
| 完成campaign（已包含X1） | 7,758,732,212 |
| 保留prior research | 45,307,809 |
| campaign＋prior research完成合計 | **7,804,040,021／8,589,934,592** |
| 本X1 batch全部檔案 | **127,003,216／134,217,728**（約121.12MiB） |
| batch／campaign剩餘 | 7,214,512／785,894,571 |
| `out/x1`及兩個初始export的build／source／binary產品 | **1,025,672,651**，逐目錄另列；不含新錄影／模型資料 |

前置快照已含31,307B初期X1檔；本批總127,003,216B全部列入campaign，淨增126,971,909B。campaign扣除此batch的7,631,728,996B與此前原帳一致。capacity ledger自身50,852B計入after totals，其自hash不列以避免循環；其它batch artifact逐檔SHA已保存。没有以另root繞過資料額度；build/export產品分開明報，未刪除失敗嘗試來美化帳。

仍為 **Unknown**：最早錄影前的state／contact、完整原perception集合／未錄capture、原thread race、real recognition/RPC delay、source render age、物理跨幀identity與judgment line role gold、遊戲採納，以及77 Miss的逐Note原因。全部pixels已受原C36g觸控／特效影響，FakeTouch不會改後續畫面；本研究是給定已錄pixels下的策略比較，**不推導gameplay hit rate、AP、Miss改善或跨曲收益**。沒有模型標註升格人工gold，也沒有做模型訓練。

**建議下一個單項X2（尚未執行）**：在main50隔離離線比較版只禁用`preserve_confirmed`覆蓋winner，其餘initial score、identity、history／projection、owner／clock保持不變。6214已有「垂直score較低但winner被保留水平」的直接診斷，適合分離preservation的充分性；6160初始錯選應仍存在，可否修復後段root／plan／Down待測。對照C36h及A/B/C/E正常／absence／merge controls，另保留旋轉／late alignment／過線返回／completed與unknown Down反例。若只是後段root變多或E動作改變，不算改善；若Down仍不建立，據first新的guard再選下一層。這是後續研究建議，**本交接沒有開此介入，也沒有將其移植production或啟動live**。
