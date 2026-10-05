# C36h 主線：離線視覺證據、因果定位與下一階段交接（2026-10-01）

> **最新總控驗收（2026-10-03，J22）：X10d-P單機制工程／冷契約驗收通過，可進X11成本與候選凍結準備。** 尚未正式採用或驗證Miss改善；14組baseline有Down而candidate無Down的機會損失保留為未知取捨。X10b suppression仍否決，X10d-O另案。詳[總控驗收](../04-offline-research/PENDING_CANCEL_X10D_P_CONTROLLER_ACCEPTANCE_20261003.md)。

> **總控複核補記（2026-10-03，J20）：X10c negative result簽收，X10b standalone suppression否決維持。** 下一步X10d先拆main50既有pending-missing cancellation作C36h單機制研究，body ownership另立反例；尚未實作／未有新live候選。較舊各節的「驗證中／Unknown」依其日期閱讀。

> **2026-10-03最新：X10c已完成X10b獨立工程驗收與四次有界fullprefix因果稽核，見J19。** standalone suppression family因2881 target absence／舊pending Hold grace交互作用否決採用；首observable state差2477，全actions未稽核0，2887之後也已對齊。X6／X10亦為negative，X9原contact持續。詳[X10c結果](../04-offline-research/HOLD_CASCADE_X10C_RESULT_20261003.md)及[較遠執行計畫](C36H_FORWARD_EXECUTION_PLAN_20261003.md)。A–I／舊J節按各自日期閱讀；C36h仍baseline／77 Miss，main50仍donor/control、50／27／11、live0，X11–X13未啟動，沒有正式candidate或新实機結果。

**C36h tint1 是主要 behavioural / experimental baseline；main50 是 comparison candidate、mechanism donor，也可能帶有 regression。** main HEAD 的整合狀態不構成產品 baseline 升級。下一步先建立逐物件因果 trace 與 **C36h 完整 contact replay**，再以相同 input／clock policy 比較 main50，選擇性移植經消融支持的機制。小量視覺覆核與 replay 同步，不以完成18 ROI標註或再訓練模型為前置。

本研究要回答：**在錯誤 decision 的時刻，current RGB 支持什麼？第一個偏離發生在 observation、identity、relation、prediction、owner，還是 scheduler／session？** 模型是離線研究儀器：pixels → 可覆核視覺證據 → first divergence → 可證偽假說 → bounded C++ rule → regression → frozen comparison。離線結果不等於遊戲採納或 Miss 改善。

證據標記：**Verified**＝原始檔／程式／hash／測試直接支持；**Strong inference**＝多項證據一致但缺介入實驗；**Hypothesis**＝待消融；**Unknown**＝資料不足。本文工程規格、門檻與實驗均為**待實作提案**，不是已完成能力。

## A. Current State：版本、原始資料與驗證邊界

### A1. 本次接手時的 Git 狀態（Verified）

| 項目 | 實際核對 |
|---|---|
| 工作目錄 | `C:\Users\wurre\Desktop\Phigros-Auto-play-System` |
| `git status --short --branch` | `## main...origin/main`；開始時沒有 tracked／untracked 未提交修改。未 fetch，不據此聲稱遠端即時狀態 |
| HEAD | `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c` |
| 整合 anchor | `9fea67e248411f7c2309f220daf989f652b27d15`；其後到本 HEAD 只改9份文件，沒有 production source 變更 |
| registered worktree | 僅目前 main；未新建、reset、clean 或切換 worktree |
| 保存分支 | `codex/baseline36-recovery` → `98169a575bd7c3b7503849ceb4b91a934d50ce0f` |
| main strategy | [strategy_version.hpp](../../../include/pas/strategy_version.hpp)：observer50／planner27／diagnostics11；新 live rounds=0 |
| 本 task 修改 | 僅本文；未提交、push、修改策略、啟動 emulator／真觸控／manual-session／goal／模型訓練 |

**環境差異（Verified）**：歷史 recovery 路徑 `C:/Users/wurre/.codex/worktrees/baseline36-recovery/Phigros-Auto-play-System` 仍有空目錄，但不在 `git worktree list`；其中 `out/dependencies/libtorch-cpu-2.7.0/libtorch` 不存在。舊 README／整合交接／清理帳本的 dependency reference 不可當成現時可用依賴。這不代表 frozen runtime 遺失，也不證明其他位置沒有 LibTorch；本輪未重裝、未驗證 CPU 模型重新建置。

### A2. Lineage／binary／source map

| 名稱與角色 | 來源／版本 | binary／實驗身份與證據 |
|---|---|---|
| **C36h tint1，主要 baseline** | variant `C36h-orthogonal-ridge-patch-recovery`；observer37／planner19 | frozen `pas.exe` SHA256 `61ecfacd2cc48ed270506719a1089be646d915c64e9026706ac1654e0d4bbce9`，本輪重算相符 |
| C36h build provenance | freeze 記 `e23b3f9edfc2df5c6e1ce1aa4c8168af8aeea726`、`dirty:true`；之後保存到 `98169a5…` | **不能把 live binary 說成從 clean 98169a5 重建**。freeze＋逐檔 source SHA 才是當時輸入；本輪比對下述7個核心快照 hash 且 Git blob 與98169a5相同 |
| main50，比較／機制供體 | 整合9fea67e…，當前f83c7ea… production source相同 | `out/main-integration-v145/Release/pas.exe` SHA256 `1440cba9b176f23f9c95513e56b3e8826df1f638b8b17c055955ee170a88addb`，本輪重算；無新實戰 |
| main50 observer 工具 | 整合候選的 `pas_frame_review.exe` | SHA256 `5b027ebba3f38d95fe420d73475441019d45212b5ce8a257815745cafb7abee5`；僅 observer replay |
| C36g-rec1，全錄來源 | 前一次實戰版本，**不是 C36h live** | Dlyrotz IN13的7722 PNG／journal；C36h-v3與main50都在這份C36g pixels上重播 |
| 歷史 main37/planner19、observer33／36／38、A36衍生 binaries | 分屬其他 source／binary／環境 | 保留獨立baseline與frozen證據；不能以相同版本號合併。來源見[熱測審查](../02-game-results/HOT_REGRESSION_REVIEW_20260930.md)、[恢復計畫](../02-game-results/BASELINE_RECOVERY_HOT_DEVELOPMENT_PLAN_20260930.md)、[整合交接](../06-engineering/MAIN_INTEGRATION_HANDOFF_20261001.md) |

已核對C36h快照：`src/game.cpp`、`src/game_tracking.cpp`、`src/game_motion.cpp`、`src/game_session.cpp`、`src/core.cpp`、`include/pas/game.hpp`、`include/pas/core.hpp`。完整freeze另含DLL、tests、工具、設定與其他source；不把7檔核對擴稱全建置可重現驗收。98169a5尚無現在的 `include/pas/strategy_version.hpp`，不能只換header或混連兩版ABI來「重建C36h」。

### A3. Evidence map 與可重現入口

以下縮寫皆相對repository root，只為本文定位，不改原始檔：

| 縮寫 | 路徑／意義 |
|---|---|
| `C` | `measurements/game-assist/2026-09-30-m0-manual-continue` |
| `H` | `C/acceptance36h-01`：`candidate36h-tint1-freeze.json`、`candidate36h-tint1-runtime/`、`candidate36h-tint1-source/`、`result-review.json` |
| `H-live` | `H/sessions/manual-session-16174738262800/round-1`：C36h真實journal／result／sparse clips |
| `F` | `C/full-recording36g-01` |
| `R` | `F/sessions/manual-session-22885039263800/full-recording`：原PNG、`index.jsonl`；原round journal在該session的 `round-1` |
| `S` | `F/review/selected-clips-v1`：12段使用者理由／原影格引用；clip09已更正正常 |
| `O36` | `F/frame-analysis36h-v3`：C36h observer replay，不是C36h gameplay journal |
| `O50` | `measurements/research-next-20261001/main50-observer`：main50 observer replay |
| `V` | `C/learning-cpu-01`：CPU pilot／`packet-v2`的18 native ROI／proposal；v1失敗及superseded證據保留 |

可直接開啟：[C36h freeze](../../../measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/candidate36h-tint1-freeze.json)、[C36h result-review](../../../measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/result-review.json)、[全錄 index](../../../measurements/game-assist/2026-09-30-m0-manual-continue/full-recording36g-01/sessions/manual-session-22885039263800/full-recording/index.jsonl)、[O36 replay](../../../measurements/game-assist/2026-09-30-m0-manual-continue/full-recording36g-01/frame-analysis36h-v3/replay.jsonl)、[O50 replay](../../../measurements/research-next-20261001/main50-observer/replay.jsonl)。`measurements`／`out`是ignored，Git文件不能代替實體備份。

重要核對值與限制：

- **Verified**：`R/index.jsonl` SHA256 `0bd8ec74b6bc19d1f8080c064453a052095e328e31d2759f12d6d04d78500f8d`；`O50/replay.jsonl` SHA256 `103742ee30e897c81a530bb99e3c4ff7329c1f33703b1d286c65ed01baeabaff`，本輪重算相符。index有ordinal、source_frame、source_sequence、capture_complete、pixels_ready、PNG SHA、rotation；來源timestamp另有未驗證domain，`source_absolute_age=null`。
- 已保存summary：7722原圖、21184 journal rows、7684 recorded decisions、7715 observer calls、3722 unique selected frames／3843 clip references；6個decision join缺口按引用計數。這些是既有工具結果，本task**沒有重新執行全輪observer replay**。
- `O36/O50`的 `frames.jsonl` 是**原C36g decision／event join**，既有共同SHA `e54f2744e58767073efb01349fc83c24cf5a676eda869a843786c6fd79a53195`；`replay.jsonl`才是各版新觀測。舊報告cancellation不是main50/C36h反事實cancellation。
- 全錄received-pixel時長131.8293931s、capture最大gap443.5487ms、最多32 preroll。缺失來源pixels、較早待命狀態不能補造。C36h live僅27張sparse RGB／9 triples；原分析223/232 diagnostic events無nearby RGB、exact event frame=0，不能逐一解釋77 Miss。
- 原比較的960幀count差異、+240／−1136次root出現、+28垂直線出現，均為**數量差**，沒有physical-object配對、命中率或gameplay因果意義。
- CPU pilot：4377 parameters、synthetic macro IoU約 .89267、18 ROI全Dlyrotz development；**real human pixel gold=0，cross-song accuracy unknown**。本輪AI目視也不把gold改成非零。

### A4. 本輪與歷史驗證分開記錄

**本輪Verified**：目視原生3498、4986、5287、5520、6214，以及新增追溯的6170／6174；讀兩版既有逐幀replay、使用者理由、freeze／result／來源hash，未改寫evidence。使用現存main50 test binary執行下列純離線回歸，**137/137 pass，0 fail／0 skip**：

```powershell
& out/main-integration-v145/Release/pas_tests.exe '--gtest_filter=GameTracking.*:GameOwner.*:GameScheduler.*:GameMotion.*:ManualLifecycle.*:ManualOwner.*:ManualIntegration.*:ColdRgbH1.*:ColdRgbG3.*:ColdOracleG2.*:ColdOracleR7.*' '--gtest_brief=1'
```

test binary SHA256 `b8a0da76ffbb1231d9795caad10cd02346bdcb9d3d383c74531b0213a14ae7da`。未rebuild、未跑CPU optimizer self-test／Debug／ASan；這137項不是新寫的真實像素contact regression。

**歷史結果**：整合Release 348 pass／1 opt-in skip／0 fail（349），CPU 8/8，清理後相關117/117；不稱本task重跑。舊observer成本在Windows x64／MSVC v145 Release／Core Ultra 5 125H／32GiB、1280×720：main50 n=7715，p50/p95/p99/max=6.4493/12.70719/17.543938/28.8193ms，jitter(p95−p5)=8.09109ms；C36h-v3=3.3429/9.39837/14.181976/25.1434ms、jitter=7.60583ms。計時含process＋decision_json，兩次有不同並行負載，**不是受控latency A/B**，亦非capture→touch全鏈。

## B. C36h 實際修了什麼、沒有修什麼

| 機制 | 已支持的局部結果（Verified，範圍有限） | 未驗收部分 |
|---|---|---|
| current orthogonal／tinted ridge | 對灰／低飽和有色的水平、垂直長ridge增加當前像素提取；有限gap、span、contrast、厚度、鄰近body防護 | 線外觀存在不等於judgment role，也不保證note→line正確；effect／outline負例不可刪 |
| Flick core／arrow segmentation merge | 5520紅核心與中央白箭頭恢復為一個局部核心；旋轉局部座標、中央箭頭支持、距離／尺寸限制 | 6214不是全部由Flick merge解釋；無箭頭／獨立雙Flick仍須分開 |
| Hold patch/front continuation | 同ID、同線、當前paired rails／body支持的interior patch→front切換有限定相容路徑；3498附近反例有冷驗 | 尚無C36h對本全錄的完整owner replay，不能宣稱該接觸已維持至tail |
| motion-aware relation與嚴格current support | O36在6214恢復垂直線association／root；current body搜索失敗不能沿用歷史held flag | C36h也有中間失配／samples重建，不是逐幀oracle；rotation／late alignment不可由遠處同法向硬門檻排除 |

**Verified live**：C36g-rec1＝778185／Perfect485／Good16／Bad6／**Miss77**／maxcombo86；C36h tint1＝**795950／Perfect496／Good11／Bad0／Miss77**／maxcombo120。C36h result-review引用result SHA256 `d85240867fa6386538076fb556c2f995f82e1e887d7675d64ab789a69c4271af`。這支持局部分數、Bad、combo行為變化，**主要Miss數沒有改善**，也未建立多輪受控因果效果。不能把77 Miss對應到77個cancel、root缺失或runtime ID。

## C. Failure taxonomy 與程式實際邊界

本次全錄實際鏈需包含 [manual_session.cpp](../../../src/manual_session.cpp)：capture latest frame → `SessionPerception::process` → latest packet → `SessionGameOwner` → `GamePlanOwner::accept/poll` → `Scheduler` → backend。[runtime.cpp](../../../src/runtime.cpp)另有直接observer／owner的入口，不能代替manual-session的lifecycle。

| 層 | 可驗問題 | 已讀程式／現況 | 不能下的結論 |
|---|---|---|---|
| pixels／observation | 當前core、body、paired rails、tail、線外觀是否存在？ | [game.cpp](../../../src/game.cpp)的ridge、Flick merge、moving front／outline／patch搜索；一組rails只支持一候選 | 亮線就是真判定線；4986歷史Hold應補成current body |
| identity | 同一可見物件是否split、不同物件是否false merge？ | [game_tracking.cpp](../../../src/game_tracking.cpp)的候選匹配、近期幾何、main兩邊交換可行性檢查；owner已有active alias | ID改變＝新物理Note；每個fragment都可Down |
| note→line | 正確線是否在集合；哪個gate／score／preserve選錯？ | `track_legacy_batch`；[game_motion.cpp](../../../src/game_motion.cpp)的line identity／pose；6214線存在而relation不同 | 增ridge detection就能修6214 |
| prediction | 歷史不足、line-ID換代、反轉reset、relative speed、residual或expiry？ | 有界history（6 samples／90ms、10ms buckets）；main分段fit與conflict等處理 | root null＝active Hold應Up；root更多＝hit更多 |
| owner／contact | Down成功了嗎？cursor／alias／lease／body compatibility在哪失效？ | pending／active、current support、Move、tail、Up／cancel／retired；[core.cpp](../../../src/core.cpp)receipt與到期 | recorded Down自動成為另一版Down；cancel就是Miss |
| scheduler／session | between-frame due、gate、reset、source revoke、epoch／generation是否先改變動作？ | [game_session.cpp](../../../src/game_session.cpp)、manual-session latch、dispatch guard／expiry／release | 逐幀poll等價runtime；observer replay就是gameplay replay |

### C1. 影響下一步的具體 code findings

1. **Verified：有body不等於通過owner current-support gate。** 兩版held_support均需current rails、head-on-line或held-body、fresh evidence、`samples>0`、非歧義等；main另排除projection-only。`samples==0`在body grace前走 `current_geometry_unsupported`。**C36h已存在此guard**；零samples可能源於identity／relation，而非pixel body消失。先保留上游原因，不改成「有顏色就續租」。
2. **Verified：active Hold已有不靠新root的續接路徑。** `SameHoldCanRecoverItsFrontFromAnInteriorPatchWithoutChangingContact`等測試包含root_past、body支持、同contact；pending與active分支不同。既有60ms body不相容grace不產生新的Move／evidence refresh；合格支持的lease與tail確認另處理。H5應查哪些上游gate阻斷此路徑，不重寫已存在的「無root可續Hold」。
3. **Verified：main confirmed註解比實際條件強。** `preserve_confirmed`需近期至少3 samples／30ms、當前線有效、extent合格，距離條件卻是 `abs(current_d) <= abs(previous_d) + max(12, note.width*0.10)`；逐幀小幅遠離或平行仍可過，不必持續接近。另有same-recent score −120、relative-trend僅±6、confirmed conflict／重接門檻。**Hypothesis**：初始錯配被保留；須分測initial scoring與preservation。
4. **Verified：C36h關聯規則不同，也有弱點。** observed displacement的normal approach（有限dt／速度／方向、可扣線運動）與appearance alignment分role class；近期保留需held body或歷史距離確有縮小，另有唯一current line fallback。main不是只調同一規則的門檻。C36h的alignment gate／single-line fallback也可能在late alignment、暫時缺線時失效，不能整組當真值搬回。
5. **Verified：line ID、note ID、物理物件不同。** main有有界全域line assignment；C36h已有有界line tracking／爭用處理。兩版也都有部分alias／完成意圖防復活，不能把owner／bounded tracking全稱main新發明。main同線body compatibility等擴充逐項比較。
6. **Verified：observation bank不是完全獨立於history。** current Hold搜索選線／rail anchor已受既有track／relation引導。同一 `CandidateBatch` 餵兩tracker可隔離downstream，不能宣稱比较兩版完整pixel observer；oracle介入層必須明列。
7. **Verified：現有「原辨識cadence」須收窄。** frame_review按journal `game_decision` frame集合加最早最多32 preroll呼叫GameObserver，FakeClock用capture time、context固定1，沒有session／owner／FakeTouch。manual-session在 **action worker消費latest packet時**才寫game_decision；perception可能處理過後來被跳過的packet。因此完整原perception cadence **Unknown**。這個集合可作受控比較，不是原執行緒時序重建。

### C2. Mechanism donor組合與interaction風險

| 組合 | 值得保留／消融的理由 | 接受前要排除的interaction |
|---|---|---|
| C36h current ridge＋main global line assignment／bounded pose tracking | **Hypothesis**：當前外觀與更穩定line identity互補；6197 main保有垂直線而C36h沒有，是研究入口 | 固定C36h note→line規則單換line機制，再檢查線合併、ID交換、短fragment競爭；不能直接把新ID表視為更正確 |
| C36h motion role＋main confirmed relation／幾何重接 | **Hypothesis**：前者可改善初始側向配對，後者可防crossing搶線 | 6160初始錯配與6175後黏住要分開；保留correct relation的收益不能掩蓋wrong relation lock。不得恢復遠處硬alignment門檻而破壞late alignment |
| main short-missing-line projection | **Verified範圍**：Tap/Flick、短缺線至多40ms的有界預測；可另研究缺線時pending timing | 不作3498 active Hold支持、不延長evidence lease；已完成／unknown Down仍不可復活。先保留預測欄位與當前觀測欄位分離 |
| C36h patch/front特例＋main body compatibility／alias擴充 | **Hypothesis**：可補轉動時同contact的current-body續接；C36h本已具alias與完成去重 | 一次只改一個相容條件；同線的較大容許範圍可能誤接鄰近body。双Hold／colocated Tap／缺body反例不可失守 |
| main reversal／motion-discontinuity分段＋C36h active Hold語義 | **Hypothesis**：pending root撤銷與active body續接可互補 | history reset不應被誤等同current pixels消失；先trace samples來源，不能為active Hold盲目保留舊root，也不能因此重Down |

## D. 高資訊真實案例與第一個 divergence

以下ordinal皆為**R零基索引**；source_frame、PNG SHA與時間從index join。O36/O50 ID只作該run定位鍵。AI目視描述是可覆核evidence，尚非人工instance gold。

| 案例 | 當前證據／比較 | 能驗證什麼 | 不能推論什麼 |
|---|---|---|---|
| **A：3493–3504，3498** | [原圖3498](../../../measurements/game-assist/2026-09-30-m0-manual-continue/full-recording36g-01/sessions/manual-session-22885039263800/full-recording/frames/frame-003498.png)有左側body／paired rails與斜線；O36 Hold945→line122、samples4、root_past；O50 Hold949→105、samples1、motion_discontinuity；兩者body／rails=true、幾何約(303.60,604.78)、root皆null | patch/front、同body多candidate、relation／fit reset、owner支持分支；須向前追該run的Down | 尚不能斷言C36h/main50掉contact；原C36g Hold975的3501 cancel不是它們的counterfactual cancel |
| **B：4979–4990，4986** | [原圖4986](../../../measurements/game-assist/2026-09-30-m0-manual-continue/full-recording36g-01/sessions/manual-session-22885039263800/full-recording/frames/frame-004986.png)無對應可見Hold body／線，兩版也無Hold；原journal Hold1562曾Down，後約64ms無支持取消 | absence／effect negative；不產生current evidence／Move／Down retry；區分缺支持與tail confirmed | 看不見不代表已完成；也不能說原Hold從未Down；返回不自動恢復已完成／未知接觸 |
| **C：5520 Flick** | [原圖5520](../../../measurements/game-assist/2026-09-30-m0-manual-continue/full-recording36g-01/sessions/manual-session-22885039263800/full-recording/frames/frame-005520.png)紅核心／中央箭頭；O36 Flick1571／O50 1592各一核心、相同root | current merge正對照；相鄰雙Flick／無箭頭亮片負對照；CPU proposal domain gap | 不把所有error歸Flick；正確merge不證明遊戲採納 |
| **D：6214；擴窗6158–6220** | [原圖6214](../../../measurements/game-assist/2026-09-30-m0-manual-continue/full-recording36g-01/sessions/manual-session-22885039263800/full-recording/frames/frame-006214.png)有x≈989垂直線、y≈576水平線、向右Drag；O36恢復垂直relation/root，O50看得到垂直線卻配水平 | initial association、ID split/rejoin、confirmed lock、line continuity、motion/appearance競爭 | 6214不是第一差異；不能稱main50多一個Miss |
| **E：clip09／5287 normal** | [原圖5287](../../../measurements/game-assist/2026-09-30-m0-manual-continue/full-recording36g-01/sessions/manual-session-22885039263800/full-recording/frames/frame-005287.png)；使用者在S/REASONS.md更正「這裡沒有問題，誤填」 | 新規則不能任意增Down、錯接或取消；保留正常control | 不因analyzer／模型proposal／cancel存在重標failure；正常也不等於已有逐Note gold |

### D1. 6214往前追：initial relation、fragment、preserve的交互

本輪直接解析O36/O50的新增追溯（**Verified＝輸出差異**；物理連續性仍須人工覆核）：

| ordinal | O36 | O50 | 意義 |
|---|---|---|---|
| **6160** | 新Drag1622，(20.995,287)，垂直356、samples1 | 相同note幾何，Drag1643→水平316(y273)、samples1；垂直315(x938)已存在 | 該目標第一次出現在輸出時relation已不同；不能只怪後來confirmed。線位置有小差異，仍須記gate／score |
| **6170** | 舊1622暫無relation；fragment1623/1624→垂直356 | 舊1643暫無relation；fragment1644/1645→水平316 | 原圖黃色Drag與紅Flick交疊；兩版都有分片、形狀相近而relation不同 |
| **6173–6175** | 1622於6173恢復356；6174同一幾何仍接1622 | 1643仍conflict；6174由1644承接；**6175的1644開始配下方水平305**，垂直315仍可見 | 拆開identity continuation、舊relation影響與新選擇，不以ID數值配對 |
| **6197** | 只輸出水平359(y361)，1622暫配359、samples1 | 仍有垂直315與其他水平，1644配305 | **C36h也有缺線／fallback差異**，不可整段當oracle；main line機制可能互補 |
| **6214** | 1622→垂直360(x988.5)、samples4、root23189781496968 | 1644→水平305(y576)、samples5、relative_velocity_small、root null；垂直318同樣存在 | 當幀association不同而所需線可見；root值不是命中 |

**Strong inference**：初始多線選擇、重疊時note identity與錯relation保留的交互，比「垂直線沒看到」符合證據。**Unknown**：何種單機制介入足以恢復C36h contact、哪些preserve branch實際被走到、其他Drag／Flick更早何時受影響。6158是首批輸出起點，不是暖機起點；必要時向前找出生，仍從完整可用前綴暖機。

### D2. 3498：先確認該版本自己的Down

O50在3495 patch=true，3498變false，3501仍同949／105、body=true、samples3、relative_velocity_small；O36主候選945與其他候選的relation/history不同。原C36g約52px的patch/front anchor切換曾撞上舊48px相容限制，C36h已作限定修正（[原分析](../02-game-results/DLYROTZ_FRAME_ANALYSIS_C36H_20261001.md)）。

查證順序：**成功Down → active cursor/contact → current body → identity/alias → relation → samples來源 → compatibility → lease/Move/Up**。若從未Down，第一failure往head acquisition找；若active且no-root仍持續，便推翻「沒有root造成此處premature Up」。本輪尚無此完整trace。

## E. 離線視覺分析策略：與runtime diagnosis同一張證據表

### E1. 分工、標註狀態與join

1. **原圖／模型助手**：先看無overlay原生frame與有限连续上下文，提出可見core/body/rail/tail、線外觀、split/merge、同物件假說；允許absent／occluded／unknown。再開兩版overlay查差異，避免被runtime ID／reason帶偏。VLM不根據「77 Miss」猜哪個Note漏打。
2. **人工**：只覆核對假說有判別力的輪廓、可見性、跨幀對應、候選線集合，記reviewer／時間／來源hash／unknown；line appearance與judgment role分欄。文字理由、AI目視、model confidence不得自動成為 `reviewed_human`。
3. **runtime diagnostics**：記每層輸入、候選競爭與guard，包含rejected alternatives；不只輸出成功target。唯讀trace不改分支順序、score或時間，單向流出。
4. **join key**：recording/index SHA＋ordinal＋source_frame＋PNG SHA＋run manifest；其後另列candidate_id／track_id／line_id／intent_id／contact_id／revision。人工 `visual_object_key` 獨立於runtime IDs，對照可多對多且unknown，不把ID當物理真值。
5. **per-object causal sidecar**：記visible_now、body/rail/tail polygon或稀疏線段、same-object hypothesis、competing line evidence、available_frame_range、future-context-used、review_status，再join observation／identity／relation／root／owner／receipt。未來幀可助人工理解，不能把事後理解當作當時可用runtime feature。

流程：full recording → causal window → raw visual proposal／human review → observer候選 → identity edge → relation候選/winner → fit/reset → owner guard/cursor → scheduler/FakeTouch → **first divergence與未知原因**。模型產物只進離線oracle／分析executable；正式capture→observer→owner→touch沒有模型依賴。

### E2. 方法按uncertainty選，不按模型大小

| 方法 | 本repository的具體用途 | 門檻／優先度 |
|---|---|---|
| 原生contact sheet＋幾何／軌跡overlay | 3498 patch/front、6160–6214多線競爭、4986 absence；核對ROI是否截斷證據 | **立即做**，延伸pas_frame_review既有輸出，不另建平台 |
| frame differencing／component overlap／局部optical flow | 6170 split→rejoin是否沿同一可見核心移動；body是否只改anchor；線與Note運動是否不同 | **優先非學習基線**：temporal IoU、前後向誤差、rail pair。propagated region不是current evidence。OpenCV官方已有 [PyrLK／Farneback API](https://docs.opencv.org/4.13.0/dc/d6b/group__video__track.html)；是否引依賴按收益決定，先用現有C++影格/component |
| VLM對有限連續幀作結構化比較 | 3498/4986可見性、6170重疊、人工漏看的反例；輸出位置與uncertainty | **小試可並行**，尚未選型／驗收；回答哪裡可見與哪幀相連，不回答何時按；幾何精度需原圖覆核 |
| pas_vision_cpu frozen inference／既有proposal | proposal成本／錯誤基線，特別是5520 Flick誤作Hold body的domain gap | **不先再訓練**。4377參數／synthetic IoU不是採用理由；先audit依賴與原source路徑。256² native crop不能刪掉另一競爭線 |
| 提示式segmentation foundation model | 若body/rail邊界覆核最耗時，用點／框提示產生可修改mask，比人工修正成本 | **條件式第二步**。[SAM 2官方實作](https://github.com/facebookresearch/sam2)支援image/video promptable segmentation；僅證明能力類型，未證明本遊戲準確率／CPU成本。memory/propagation不算current body；不先建GPU部署或訓練流程 |
| embedding搜尋／failure clustering | 在trace已識別的同類失敗中找更多反例 | **延後**。先用kind、guard、distance trend、split signature等可解釋欄位搜尋；五組案例已足夠起步，不先掃全資料訓練embedding |

本輪外部研究只確認上述官方能力與適用邊界，沒有下載／執行新模型或上傳資料。自有正式分析／資料／訓練程式仍用C++20；第三方原版工具可保留原語言，不把任意新Python pipeline變成專案正式邏輯。

### E3. 不先把18 ROI全部填滿

18 ROI是engineering pilot，包含選到背景後另存v2的經驗，不是information-optimal dataset。首批**最多24個anchor frame**，優先選3493–3504的切換前／當下／後，4968可見body與4986 absence，6160／6170／6174／6175／6197／6214，5520及無箭頭負例、5287正常窗。每anchor可參考少量鄰幀與原生全圖，覆核只解第一divergence所需證據；unknown保留，不強迫完成mask。

比較無模型人工baseline與至多兩種助手，計**含駁回／修正的總人工時間、false-visible proposal、false merge/split、unknown率、每個已解uncertainty成本**。必要時才沿用 `src/vision_cpu.cpp` 的native-crop／hash／reviewer／polygon-mask audit與 `src/game_dataset*.cpp` 的資料/split保護；causal sidecar不強塞training schema。跨曲資料前只屬Dlyrotz development，不把同曲鄰幀分成訓練與未知曲驗證。

## F. Visual finding → measurable signal → deterministic C++

**可量測信號＋反例才是runtime候選；「模型覺得同一物件」仍是研究evidence。** 先離線量測，規則固定後才移入正式路徑；不按歌名／進度分支。

| 發現／假說 | 可量測信號 | 候選正式規則（尚未採用） | 必保留的負例 |
|---|---|---|---|
| 同Hold body換patch/front | rail pair間距/方向、current body coverage、centerline投影區間、anchor位移、temporal overlap | 已Down identity的有界semantic anchor切換；依當前body更新同contact，不固定初始Down位置 | 4986無body、短不相容body、隔壁Hold、不同line、completed／unknown Down；先驗既有C36h特例，不直接加大48/80/128px門檻 |
| effect前後同核心分片 | component一對多／多對一、位移、形狀／rail pair、competition margin | 有界split/rejoin與唯一claim；區分candidate identity與execution birth | 真雙Note相交、colocated Tap、兩Hold；多fragment不可各自續租／重Down |
| 線存在但配到平行／遠離線 | 每線signed distance、絕對距離導數、relative normal velocity、pose confidence、extent、score | initial motion-vs-appearance評分與confirmed撤銷/hysteresis分開，證據不足unknown | 噪聲、線追Note、過線往返、late alignment、旋轉；遠處Note朝向不是必要配對條件 |
| 假線／rail當judgment line | local contrast、雙邊support、厚度、component連續性、body拓撲 | current geometry負向guard／candidate品質；role未知不靠亮度猜線 | 有色／短／遮蔽線、交叉真線；不能硬刪垂直線修一首曲 |
| body存在卻samples0掉contact | current pixels、identity/relation有效性、history-reset origin、active cursor/receipt | 先分 `pixel_support_missing`／`identity_unsupported`／`relation_unsupported`／`fit_not_ready` 診斷；證據支持後才分prediction readiness與active support契約 | line不可觀測／歧義／expired／body不相容仍不Move或續lease；unknown不升格支持 |
| Flick箭頭切核心 | 核心分片位置、中央箭頭／局部法向拓撲、尺寸/gap | 保留C36h現有局部merge，只有新反例才改 | 無箭頭highlight、相鄰雙Flick；5520不可回歸 |

沿用16 lines／128 note candidates、短history及owner容量；新增scores／edge也有硬上限。測幾何／動作契約，不測「root變多」。main的global assignment、confirmed、projection有不同用途：**short-missing projection是預測，不是當前line/body pixels，不得延長active Hold lease**。

## G. 優先實驗與完整counterfactual replay契約

順序重構為：**X1逐物件trace＋C36h完整replay → X2/X3機制消融；X4視覺小試同步 → X5選擇性移植／凍結。** 不先升main50、不先標完18 ROI、不新造大型Fixture或annotation平台。

### G1. X1：先C36h full contact replay，再main50

- **Question／Hypothesis**：A/B/D案例的第一個action分歧能否由完整鏈定位？observer差異可能在owner前消失，也可能早在head Down／gate發生。
- **Input／code**：R全索引／PNG、原session journal、H freeze；擴充 [apps/frame_review/main.cpp](../../../apps/frame_review/main.cpp) reader/hash/選段輸出，共用manifest協定，分別連結各lineage的SessionPerception、SessionGameOwner、GameObserver、GamePlanOwner、Scheduler及有界FakeTouch。參考game_session_tests與cold_scenario_tests；HostClock多執行緒game_cold_pipeline不是deterministic real replay。
- **Intervention／controls**：先零策略介入，各版自pixels生成下游狀態。保留舊observer-only compatibility模式作reference；先C36h、後main50。合成已知head→body→tail、between-frame due、gate/reset、unknown receipt有正負例。
- **Output**：manifest、語義digest、五組per-object trace，original／counterfactual事件分離，first observed divergence、first action divergence及unknown原因。
- **Success**：同binary/input兩次語義digest一致；可自初始狀態追出Down，或明列未建立／起點未知；正負例排程與release符合宣告契約。
- **Falsification／decision**：更早狀態缺失或cadence/clock政策足以改變結論，該case contact因果為unknown；仍可分層合成介入，不用原touch補答案。若body支持已維持contact，轉查head acquisition／其他層，不強修Up。

#### X1必須凍結的replay contract（proposed）

| 項目 | 首版規格與驗收 |
|---|---|
| exact input | 核對A3 index SHA、全部引用PNG、完整journal segments SHA、唯一ordinal/source_frame、1280×720 RGB/rotation1與profile。H記lead35ms／uncertainty30ms／五指，profile SHA `4201679acc8ded39a394852d5f8d7f11fd62e736c32f10aa77657fc8fdbef18b`；所有實用設定入manifest。缺檔、重複、非單調不能默默略過 |
| cadence | 首版 **recorded-owner-consumed-set＋原最多32 preroll**；all-received-frame模式另列敏感性實驗。不是已知完整perception集合。原decision只供frame集合／外部對照，**不讀原target/root/line/plan/touch作策略輸入** |
| warmup／reset | 從R最早preroll一路到選段，不只從3493/6214開始、不每clip reset。SessionPerception自pixels推UI/round、在自己的new-round reset observer；owner依active round開始，suspend保留retired、finish釋放。前史不足標 `early_state_unknown`，不預填Down |
| context／gate | 可信metadata給source generation/geometry/validity/revocation；index缺的先標 `assumed_constant`（固定1），不聲稱還原。round epoch由重建session產生，和source epoch分欄。allow_down latch、owner suspend、round/source/freshness dispatch guard重建，不依原round動作強制開gate；未記錄的原perception gate變化保持unknown |
| FakeClock | `t_replay=t_original−t0+1s`保留host QPC間隔與capture/pixels_ready差。capture_complete仍是Frame evidence time，交付在pixels_ready；source timestamp僅存domain，不換算未知render age。缺pixels_ready則fail或另立fallback run，不混在同policy |
| recognition policy | 首版recognition start=end=交付fake time，序列執行，稱**零計算延遲語義實驗**。真QPC的observer/owner成本另記，不推進FakeClock。若用原recorded recognition delay，兩版共用、缺值政策明列，另作敏感性run；不以各版CPU耗時決定cadence |
| capture envelope | 可用recording每次capture arrival維護latest-capture freshness，只有選入cadence的frame進perception。這也是宣告政策，不是完整原thread reconstruction；不補造未錄arrival，不從原touch/cancel倒推gate |
| between-frame due | 事件迴圈處理下一capture/frame交付或 `next_due_ns` 最早者，包含gate expiry、每plan evidence/valid_until、pending steps，不能僅frame後poll。嚴格早於t的due先處理；frame與due同t時首版先交付/process/accept再poll，對應manual accept→poll順序但非原thread race真值，另測due-first敏感性。同一時刻未取得進度須報錯，不能忙迴圈 |
| FakeTouch receipts | 預設成功／零RPC耗時／五contact；另以合成腳本注入delay、failure、unknown Down與release unknown。記scheduled、source evidence、injection_start/return、結果、contact集合與release report；unknown Down不retry。原receipt不控制FakeTouch回傳 |
| EOF／early state | EOF安全stop/release，標input_truncated；除非本run像素lifecycle確認result，不稱自然tail或歌曲完成。無前置Down不能搬original contact；獨立合成unit test可構造active狀態，但不叫真實完整replay |
| input／output bounds | 沿用reader 36000 frame、PNG≤4MiB、index≤64MiB／row≤2MiB；最多五組輸出窗口、每組≤120 frames，暖機完整前綴但stream摘要。窗口外仍保存active identity的birth／成功Down與最後receipt來源摘要（最多128筆），不因裁剪丟掉head因果，也不留無界history。首批所有lineage/proposal/trace磁碟**合計≤128MiB**，加入原8GiB campaign，不複製全錄。trace ring≤16MiB、每frame最多128×16 relation edge；receipt/event總數≤100000且bytes仍受限，越限停止並release |
| backend bound | 現FakeTouchBackend的receipts_累加vector沒有drain，不能因runtime有界就聲稱長replay有界；需可排空離線backend或inject前硬拒容量adapter。每次accept/poll排空owner diagnostics；截斷/drop必入summary，不丟第一guard |
| determinism | 同binary/source/input/clock/tie/receipt政策兩跑，比canonical decisions、reset/state、relation、plan revision/cursor、cancel、命令、receipt、release、unknown及digest；排除真QPC耗時/絕對輸出路徑，固定序列與tie-break。trace-on/off語義一致；與舊reference比時正規化時間offset，保持舊模式不受新session路徑改動。失敗分母含缺檔、join、容量與起點unknown |

**固定pixels反事實限制**：錄影已受原C36g觸控影響。改FakeTouch不會改後續特效／combo／畫面；這是**給定已錄pixels下的策略比較**，不是完整遊戲世界模擬。不能從中推導候選gameplay hit rate。

### G2. X2：6214 initial relation／identity／preservation消融

- **Question／Hypothesis H3＋H2**：6160初始錯配、6170分片、6175水平relation、confirmed保留各自是否必要／充分？
- **Input／tool**：6158–6220及完整warmup；game_tracking.cpp、game_motion.cpp、GameTrackingComparison／candidate_batch。trace含每線gate、role class/score分項、distance trend、confirmed狀態、identity alternatives。
- **Intervention**：原兩版先找第一差異，再一次只換一項：(a)當幀line choice離線oracle；(b)identity edge；(c)initial score；(d)confirmed preserve；(e)line tracking。各自從可恢復前綴重跑；fixed-bank與完整pixel試驗分開，不把history-guided bank當中立。
- **Controls**：真正同線接近、crossing不搶配、平行/小幅漸遠、late alignment、線追Note、旋轉Hold、真雙Drag；6197 C36h缺線也是反例；5520、5287不可退步。
- **Output／success**：逐物件first relation/action divergence、移植前後原因；至少一個介入產生預測效果且另一條真線不被錯搶。
- **Falsification／decision**：刪preserve仍6160/6175錯配→否定preserve alone；給correct line仍無Down→H3不足解釋action。保留C36h有效motion證據，僅吸收通過反例的main單機制，不整包搬tracker。

### G3. X3：3498／4986配對Hold介入

- **Question／Hypotheses H1/H2/H4/H5**：body、identity、relation、fit readiness、compatibility哪層先失支持？active無root是否正常？
- **Input／tool**：3493–3504／4979–4990並向前追head；game.cpp的current body／owner、game_tracking.cpp、core.cpp；X1 trace與原生RGB。
- **Intervention**：離線專用runner一次只替換**一個**已覆核observation、identity edge或relation；另用合成已Down狀態獨立測root=null、samples0不同來源、patch/front。改root不可順便補body/samples/lease。未人工覆核oracle仍標proposed，不作正式正確性驗收。
- **Controls**：3498可見body／4986 absent；雙Hold、fragment、colocated Tap、不同line、正常tail、按住時線仍旋轉、接近才對齊、expired、completed返回、unknown Down。
- **Output／success**：current support→guard→contact因果鏈，列Down是否成功、Move/Up/release時間與缺支持來源；有支持維持同contact，不創新Down／無支持Move。
- **Falsification／decision**：correct observation仍取消→H1 alone不足；correct identity仍同guard→H2 alone不足；active no-root已持續→本窗H5否定；body存在但relation unknown→先修relation，不加grace。支持確實消失則接受保守釋放，遊戲tail語義仍unknown。

### G4. X4：視覺助手是否降低分析成本

- **Question／Hypothesis**：model proposal比人工＋現有幾何overlay更快找出可驗body／split／relation證據嗎？
- **Input／code**：X2/X3最多24 anchors與控制組；pas_miss_review的provenance/sparse view、pas_frame_review、pas_vision_cpu既有proposal；必要時比較一種VLM或提示式mask助手。無訓練步驟。
- **Intervention／controls**：原圖先於overlay，人工/助手流程交錯順序並記熟悉效應；模型不先知道哪些是failure。4986可答absent/unknown，5520不能高confidence改成body，5287保留正常control。
- **Output／success**：review log、改正/拒絕數、人工分鐘、可量測feature與未解uncertainty。建議採用門檻：含修正時間≤人工baseline80%，且無新增經覆核false-visible／false-merge。小樣本只決定工作流，不報泛化accuracy。
- **Falsification／decision**：不省時間、要由runtime答案引導，或只得到「看起來像」便停止該方法，保留原圖＋C++trace；不自動以追加訓練／擴資料延長pilot。

### G5. X5：選擇性donor、regression、freeze

- **Question／Hypothesis**：某main50機制在C36h上能解已定位缺口，且不產生interaction regression嗎？
- **Input／tool**：X2/X3通過的單機制＋C36h；game_tests、tracking/session/cold scenario、固定real-pixel windows。line assignment、confirmed、projection、owner相容門檻分donor manifest。
- **Intervention／controls**：C36h base、base+one、必要時base+one+one、main50 reference；不以版本號描述差異。synthetic＋real windows都跑，正負例及未排程／無Down／abort分母全列。
- **Output／success**：失敗→修正→負例不退步trace、閒置主機受控latency/capacity、binary/source/profile SHA與frozen candidate。只改counts無預期因果效果，不接受為改善。
- **Falsification／decision**：錯接、重Down、unknown retry、無支持續lease、正常control退步即拒該組合，保存反例。冷驗只稱候選；有限frozen live A/A／A/B需另獲授權，不自動啟動。

### G6. Hypothesis ledger

| 假說 | 現有evidence／等級 | test／positive control | negative control／falsification | 預期runtime改動 |
|---|---|---|---|---|
| H1 observation failure | A有body，但3498兩版已輸出held body；**不支持將該幀簡化為完全漏檢**；其他fragment範圍Unknown | X3獨立current geometry oracle／合成可見rails | B無body；oracle後仍失敗→非單獨H1 | 只補已證漏失的current feature/geometry |
| H2 identity fragmentation | 6170分片／6174接回ID不同Verified；同物理對應Strong inference | X2/X3獨立identity edge、連續rails/centroid | 真雙物件／completed；修ID仍同failure→不足 | bounded split/rejoin／unique claim，不增Down資格 |
| H3 wrong relation | 6160/6214線存在、relation不同Verified；語義錯配Strong inference | X2 initial／preserve分開消融 | crossing/late alignment；改preserve未修initial→否定單因 | motion-aware bounded score／hysteresis |
| H4 owner cancellation分類混淆 | samples0先於body grace取消Verified；C36h該contact實際是否走到Unknown | X1 cursor/receipt＋X3 samples來源分類 | absent/expired/unknown receipt；從未active或gate先撤銷→非該分支 | 先診斷，有證據才改支持契約 |
| H5 body visible but no root | active無root路徑已存在Verified；上游gate阻斷Unknown | 已Down/fresh body/root null fake-clock | 不可new Down；若接觸本來持續則本窗H5否定 | 分timing fit readiness與current body，仍需有效identity/relation |
| H6 cadence／session／due | reader與manual記錄位置不同Verified；選段contact影響Unknown | X1完整session、幀間due、tie敏感性 | stale/reset/unknown；政策改變使結論不穩則降為unknown | 僅合成重現實際缺陷後改scheduler/session，不補造frame |

## H. 下一個coding agent的可停止工程步驟

**下一個engineering task的完成界線：X1可重現C36h/main50固定pixels完整contact trace＋五組first-divergence報告。** 不以「修好77 Miss」作未經證據的驗收，也不把live當自動收尾。每步可獨立完成、測試、停止：

1. **凍結manifest與C36h建置邊界。** 重查Git／worktree／使用者變更；由98169a5與H source snapshot建立隔離離線建置，不覆寫H runtime/source。核對實際編譯檔hash，保留C36h observer-only reference；main50另編不混ABI。工具source/binary hash與策略source hash分列。LibTorch舊路徑不可用不阻塞不需模型的replay。
2. **加唯讀per-object trace。** 擴frame_review既有reader/output；先交6160–6214候選/line scoring、3498 body/fit guards。欄位含current evidence、candidate→track edge、confirmed/winner原因、history reset、owner cursor、alias、期限、scheduler/release。驗trace on/off一致；未加contact前標observer-only。
3. **實作X1 C36h session→owner→scheduler→bounded FakeTouch。** 固定G1 clock/warmup/tie/gate/容量，自pixels建立Down；合成驗head/body/tail、無body、幀間due、同時expiry、round reset、pending vs active、unknown Down/release、EOF、early-state-unknown、receipt容量。此步不改production action semantics。
4. **同manifest跑main50、join五案例。** 兩次determinism digest；original recorded／C36h counterfactual／main50 counterfactual分三欄。按visual geometry/proposal對物件，不按ID相等。交first evidence/state divergence與first action divergence、是否成功Down、政策敏感性；缺gold/前史可交unknown與所缺資料。
5. **據trace選一個X2或X3介入，X4只覆核必要anchor。** 先立反例與數值feature，再寫單機制C++；因果未證前不搬main50整套owner/tracker。每項独立候選／測試，可拒donor而完成研究。
6. **合格才X5 freeze，停於冷驗交接。** production候選跑適用完整回歸；記憶體/生命週期變更補Debug/ASan，再量效能。保留反例、容量帳本、不可驗項；有限live另提議另授權。

### H1. 驗收metrics與分母

| 類型 | 報告內容 |
|---|---|
| observation | 有可靠覆核才報visible-object recall/precision；逐case列body/rail/tail、false positive、fragment/duplicate、line appearance與role unknown。無gold只報差異／覆核量，不造百分比 |
| identity／relation | correspondence review狀態、continuity、false merge/split、relation correct/incorrect/unknown、competition、first-divergence frame；unknown不從分母消失 |
| action | duplicate Down、premature Up、unsupported Move、lost active contact、invalid resurrection、unknown Down retry；另列no-plan、no-Down、no-root-but-supported、lost-evidence、release unknown。cancel下降不是目標 |
| runtime | 環境/binary/設定、全部處理與skip/failure n、p50/p95/p99/max、jitter定義、capacity峰值、determinism；stage與可實測end-to-end並列，不加stage p99，不只成功Down子集 |
| gameplay | 本階段不量候選hit rate；未來另獲准frozen A/A、A/B才報每曲P/G/B/M、score、combo、abort、完整failure distribution，HD/IN與lineage分開；C36h主要問題仍77Miss |

### H2. 閱讀範圍與文件差異follow-up

本次按要求讀 [AGENTS](../../../AGENTS.md)、[README](../../../README.md)、[架構](ARCHITECTURE.md)、[路線圖](ROADMAP.md)、本文前版、[整合交接](../06-engineering/MAIN_INTEGRATION_HANDOFF_20261001.md)、[C36h逐幀分析](../02-game-results/DLYROTZ_FRAME_ANALYSIS_C36H_20261001.md)、[CPU pilot](../07-data-learning/OFFLINE_CPU_VISION_PILOT_20261001.md)、[全錄](../06-engineering/FULL_ROUND_RECORDING_20261001.md)、[Miss標記](../07-data-learning/MISS_FRAME_ANNOTATION_20260930.md)、[Note/line role](../04-offline-research/NOTE_LINE_ROLE_FOLLOWUP_20260930.md)、[跨曲研究](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)、[判定線形式](判定線形式.md)、[Note形式](note形式.md)，再追baseline recovery／hot regression、冷開發C0–C6／結果／merge review、line identity M0與[9/28清理](../06-engineering/CLEANUP_AUDIT_20260928.md)、[10/1帳本](../06-engineering/CLEANUP_AUDIT_20261001.md)。历史已刪raw不假稱可重算。

source已查 `src/game.cpp`、`game_tracking.cpp`、`game_motion.cpp`、`game_session.cpp`、`manual_session.cpp`、`runtime.cpp`、`core.cpp`、`game_dataset*.cpp`、`vision_cpu.cpp`、`game_cold_pipeline.cpp`；`include/pas/game.hpp`、`game_tracking.hpp`、`game_session.hpp`、`strategy_version.hpp`；`tests/game_tests.cpp`、`game_tracking_tests.cpp`、`game_session_tests.cpp`、`cold_scenario_tests.cpp`、dataset/vision tests；`apps/frame_review`、`apps/miss_review`、`apps/vision_cpu`與CMake模型隔離。C36h核心source按快照／98169a5比較，非僅讀main文件。

需後續同步、但本task不大量改動的差異：

- 前版status的main整合基點易被當產品baseline；本文依最新意圖改C36h主線/main donor。README／ROADMAP／AGENTS的整合版本仍是Git事實，不能代替baseline選擇。
- 全錄／frame_review文件的「原辨識cadence」應收窄為owner-consumed集合及缺失perception狀態；confirmed註解「持續接近」需對齊實際非單調條件。
- 歷史worktree／LibTorch引用已過時；保留frozen manifest的當時路徑，另補現況manifest。
- 跨曲研究早期模型runtime／GPU或M3/M4安排僅屬歷史提案；本階段offline microscope，模型階段可不做。
- 歷史campaign帳本7,631,728,996 bytes＋此前research45,307,809＝7,677,036,805 bytes，仍受8GiB；**不是本task現時容量重盤**。本task無新全錄／模型產物；下一步新增前重算全campaign/derived，不能另root繞額度。保留兩輪跨曲原始證據、能力報告、獨立baseline及必要輸入。

## I. Do-not-do與尚未解答

- 不把模型/VLM接runtime，不用它決定Down/Move/Up、identity、note→line或延長lease；不可見物件不因memory/mask propagation變current evidence。
- 不用歌曲身分／譜面／進度／遊戲內部狀態／音訊／歷史按鍵調策略；不改gRPC payload fast／RGB888 top-down／256KiB基礎，不重開capture選型。
- 不把runtime ID當physical gold；不把proposal、AI判讀、使用者文字、confidence當human pixel gold；不用cancel/combo消失倒推Miss。
- 不用舊touch/root/recorded relation作counterfactual oracle；獨立人工oracle介入須明列層與review狀態，禁止進正式策略。
- 不直接放寬grace/lease掩蓋relation/identity error；無root不是active Hold結束證據、幾何過線不是完成；unknown Down不retry、completed不復活。
- 不因line/root/Down增加、cancel下降、synthetic測試/IoU上升或score提高，就宣稱主要failure解決。
- 不因HEAD新把main50整體升baseline，也不忽略它可能互補的line identity/assignment/projection/owner機制。
- 不自動恢復emulator／真觸控／manual-session／AP goal／無限實戰或無界模型訓練，不為模型先建大型dataset/platform。
- 不reset/clean／刪除覆寫frozen evidence／丟使用者改動／自行push；ignored raw缺口如實保留。

**Unknown清單**：77個遊戲Miss各屬哪層；3498在C36h完整狀態是否成功Down／真的取消；6170物理身份與各score/preserve的充分因果；缺線及session/cadence對contact影響；source render age與未錄perception序列；model real/cross-song accuracy；任一donor的跨曲gameplay淨效果。下一task應縮小這些unknown，不能用更多AI idea或更高root總數代替答案。

## J. X1總控驗收（2026-10-02，Asia/Taipei）

**首次驗收判定（其後修補驗收見J5）：核心實作與正確綁定的五案例證據通過核對；當時工具入口有兩項可重現的來源驗證缺口，X1暫不正式結案。** 開發交接為 [C36H_CONTACT_REPLAY_HANDOFF_20261001.md](../04-offline-research/C36H_CONTACT_REPLAY_HANDOFF_20261001.md)，完成日期10/2，保留原派送檔名。這不是main50升baseline、production修正或live驗收。

### J1. 獨立驗證及更新的案例判斷

- HEAD仍 `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，main／唯一registered worktree；X1改動未提交。正式 `src/`、`include/`、根CMake沒有diff。原status在驗收更新前SHA仍為 `cdfc49dac07420498e653a0fb71ca7c854a7888c7d8cc50bb41283d2af7c9b0c`，與派送時／freeze一致；本節是之後的總控更新，不改寫frozen檔。
- **本輪重跑**兩個verified Release測試套件，設定既有 `PAS_RGB_CLIP_ROOT`：C36h **197/197**、main50 **224/224**，0 fail／0 skip。只代表離線套件；本輪沒有重跑ASan或整份repository測試，也不把兩套共用cases當421個獨立場景。
- **本輪重算hash**：11個binary/DLL＋12個工具source、149個export檔、278個原batch artifacts，均相符。六個verified runs的summary語義digest在各lineage內一致；events.jsonl的實際檔案hash也在on-1／on-2／off三run一致，on-1／on-2 trace hash一致。
- 重新執行既有兩run的 `contact-compare`，輸出與 `five-cases-verified.json` **byte hash完全相同**；prefix工具亦重跑成功。本輪沒有新增full replay：原batch只剩7,214,512B，不足再保存一組完整A/B；沒有刪舊run或另root繞額度。
- **A3498**：trace核對兩版current body/rails、held_support與對已持有identity的compatibility均成立；各自在3488成功FakeTouch Down，3498無root仍續contact。本窗「無root即提前Up」被否定；更早Down時間相差約4.50ms，遊戲採納仍unknown。原C36g cancel不能挪作這兩run的cancel。
- **D6214**：直接讀trace，6160 main初選水平316且preserve=false；6214垂直318 score≈53.70低於水平305≈171.40，卻被confirmed preserve覆蓋為305。C36h後續有Down、main50所選cohort無Down。這支持X2消融優先，但不是Miss真值，也未證明只改preserve便修復整鏈。
- **B4986**維持absence negative；**C5520**merge與六條命令一致；**E5287**維持使用者正常control，5293的Move差異不是failure gold。H2/H3的物理語義及gameplay效果仍待證。

### J2. 必須補修的兩個可重現缺口

均在 [contact_replay.cpp](../../../apps/frame_review/contact_replay.cpp) 的 `compare` 入口（首次驗收時263–297行）。當時只比較sa/sb的policy彼此相同，之後便用新传入manifest與固定c36h/main50欄名產報告。以下保留原缺陷及反例；修補後狀態見J5。

| 編號 | 實際重現（Verified） | 影響／必要修正 |
|---|---|---|
| R1／P2：未綁定本次manifest與原run | 將input manifest的index_sha256改成64個0、保持五窗不變；`contact-compare` **exit0**並產完整報告。report的manifest SHA是錯誤新檔，兩summary的input_manifest SHA仍是原檔 | 在讀trace／建立output前驗 `sha256(argv[2])` 與**兩個**run的input_manifest_sha256一致；驗schema／五窗等必要契約。若未來允許不同查詢窗口，必須另立query-manifest語義，不能混稱run input。負例須非零退出且不留有效comparison output |
| R2／P2：lineage只按參數位置命名 | 用正確manifest，將main50 run放第一參數、C36h放第二；仍 **exit0**。D6214輸出c36h_down=0、main50_down=1，與真實已驗結果完全相反 | 比較前驗summary中的lineage／宣告角色與source/binary binding，或輸出明確動態run角色而不硬編碼c36h/main50。X1現有CLI應拒絕交換／同lineage冒充配對；未來ablation需明確variant與reference角色 |

最小重现證據在原campaign內 [acceptance-20261002/reproduction.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/acceptance-20261002/reproduction.json) 與 [swapped-reproduction.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/acceptance-20261002/swapped-reproduction.json)。同目錄保存正確control、wrong manifest、兩份被錯誤接受的報告及prefix control；**帶accepted字樣的負例產物不得當研究結論**。新增容量另列 [acceptance-summary.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/acceptance-20261002/acceptance-summary.json)，舊capacity ledger作當時快照保留。

這兩項不推翻現有正確run的hash／案例輸出；它們會讓之後的錯誤輸入或run順序被包裝成成功的因果比較，故必須在X2前封住。

### J3. 補修與下一步的驗收界線

先做 **X1-R1/R2**：僅修改比較入口及相關validation tests／維護腳本，保留原replay與策略語義。正確fixture的comparison語義不變；錯manifest（含只改window／index）、交換run、同lineage冒充比較均須可證拒絕。更新工具freeze與交接，保留本次反例，不覆寫旧binary/evidence。此修補不需要新live、訓練或重新保存整組full replay。

通過後再派 **X2單機制離線消融**：main50只禁用confirmed preserve對winner的覆蓋，initial score／identity／history／projection／owner／clock都固定；C36h仍是reference baseline。6160 initial錯配預期仍在，檢查6214後是否得到正確relation、root、plan、Down及新的第一guard；保留A/B/C/E與crossing、rotation、late alignment、completed／unknown Down負例。結果只用於決定機制是否值得蒸餾回C36h，不直接移植production。

新實驗前另核算保留資料與額度。X1 batch接近128MiB上限；不得刪失敗證據或把同批產物另放root逃避帳目。本輪總控只驗收／保存負例／更新status，沒有修改開發程式、啟動X2或監控開發chat。

### J4. X1-R1／R2開發修補交付（2026-10-02，待總控驗收）

開發自驗已完成，詳見 [C36H_CONTACT_REPLAY_REPAIR_20261002.md](../04-offline-research/C36H_CONTACT_REPLAY_REPAIR_20261002.md)。本段只追加交付狀態，J1–J3的獨立驗收、缺陷發現及原錯誤accepted報告保留；**自驗通過不等於總控已驗收，X1未由開發chat宣告結案。**

- compare在讀trace前綁定supplied manifest實際SHA與兩份run；驗schema／五窗必要契約。固定C36h-first／main50-second，拒絕交換、same-lineage、missing／unknown lineage；以既有root／source-v2 provenance的實際SHA及lineage支持run角色，保留歷史replay binary SHA，允許新的compare binary重算舊trace。runtime策略、association、owner、replay clock／cadence不變。
- 新Release binaries自驗：C36h **207/207**、main50 **234/234**，0 fail／0 skip；包括10項直接呼叫正式CLI的新測試。另1個真實正例及16個CLI負例均符合預期，原R1／R2現在exit1、明確reason、未新建report；existing output保持SHA。沒有用舊test binary冒充新patch驗證，也未新增full replay。
- 有效五案例report與frozen檔byte-identical，SHA `37d469caa0017be00fb78736fd69fb5cc87585c40b9fd671bb7e8397c420e7f7`；prefix案例與來源SHA不變，只改report路徑／新prefix工具binary SHA。D6214仍Down=1／0，E5287仍為正常control；offline統計不換算gameplay命中率。
- 新工具放 `out/x1/repair-tools36`／`repair-tools50`，evidence放原batch的 `repair-r1-r2-20261002`，source／binary freeze、commands／logs／XML／artifact SHA／capacity均已保存。原278個batch artifacts、7個acceptance artifacts、11個frozen binaries、149個export檔案核對無差異。
- 修補新增 **983,240B**；batch結束 **129,609,342／134,217,728B**，剩 **4,608,386B**；新build／tools另計328,940,174B。沒有刪舊／失敗evidence、另root規避額度、修改production src/include／根CMake、提交／push或啟動X2／live／訓練／goal／監看。下一步仍是總控獨立驗收本修補。

### J5. R1／R2總控獨立驗收：通過，X1結案（2026-10-02）

**Verified：R1／R2均已封住，沒有新的阻擋項；X1作為fixed-pixels離線接觸比較工具與五案例交付正式驗收。** 本次直接讀修補source、共用validation、正式compare入口、10項新增CLI測試及維護腳本，再執行新frozen binaries；不是只接受開發chat的自驗聲明。HEAD仍為 `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、main／唯一registered worktree、50／27／11，X1及修補仍未提交。

獨立證據保存在原batch的 [acceptance-r1-r2-20261002/acceptance-summary.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/acceptance-r1-r2-20261002/acceptance-summary.json)，SHA256 `53338cb1b4f5910399d15366db2d661567e4fc834578f78145dc6eeb6bf7cb83`。

- **新工具回歸實跑**：C36h **207/207**、main50 **234/234**，0 fail／0 skip；使用既有27張opt-in RGB fixture，保存本次XML／logs。兩套test binary SHA分別為 `a89bb561f7e247a0500d245f9c8b1e0c54f3831500abf7b0f6ef599c04677dec`、`c6521c507f420a22ae27ee36536e2fdc7eff28dd715701c822cb3a35cd59e4c8`。未重建、未重跑ASan／全repository套件，兩套共用cases不合算為441個獨立場景。
- **原反例與入口契約**：用修補compare重跑1正例＋16個預期拒絕，全部符合；[實際commands／exit／reason](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/acceptance-r1-r2-20261002/cli/result.json)保存逐項結果。原錯manifest現在exit1、`comparison_manifest_SHA_mismatch`；原交換順序現在exit1、`comparison_lineage_expected_c36h`，均未產生report。只改bytes／windows、同lineage、兩側missing／unknown／failed／spoofed-source、mixed policy皆拒絕，既有output不變。
- **正例語義不變**：五案例重算report為537,176B，SHA仍 `37d469caa0017be00fb78736fd69fb5cc87585c40b9fd671bb7e8397c420e7f7`，與原frozen report byte-identical。新prefix工具亦實跑，其cases、events SHA及comparison SHA與原版一致，僅report路徑與執行工具SHA不同。D6214仍是C36h／main50成功Down=1／0，E5287保持正常control；見[語義核對](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/acceptance-r1-r2-20261002/semantic-check.json)。
- **來源／保護重算**：14個workspace工具source及14個frozen副本、12個新binary／DLL、149個export、60個修補artifact、11個舊binary／DLL、12個舊source副本、278個原batch artifact、7個首次驗收artifact均相符。另核對10個未改工作區檔及J4追加前status原文，無差異；`src/`、`include/`、根CMake無diff。核對數量是檔案檢查次數，不把不同清單的重複路徑當獨立證據。
- **驗收執行記錄**：最外層PowerShell曾將維護腳本最後一個預期負例留下的 `$LASTEXITCODE=1` 誤判為整體失敗；兩套測試及17項CLI結果當時已全部完成。逐項重核exit／reason／檔案存在性後確認無工具失敗，記在 [independent-checks.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/acceptance-r1-r2-20261002/independent-checks.json)，未重跑或覆寫證據掩蓋該次wrapper錯判。

**驗收範圍**：CLI驗實際manifest SHA、宣告角色、既有provenance SHA／lineage及binary SHA格式／相異性；本次維護核對另驗歷史replay binary的實體SHA。這不是對任意被改造trace的全面鑑證，也沒有以新compare binary冒充原replay來源。本次new full replay=0、new live round=0，production語義未改；原perception cadence／早期state、source age、真實recognition／RPC delay、物理identity gold、遊戲採納與77 Miss逐Note原因仍Unknown。

**容量**：本次獨立驗收新增 **746,950B**，batch實測 **130,356,292／134,217,728B**，剩 **3,861,436B**；campaign＋prior research **7,807,393,097／8,589,934,592B**，剩 **782,541,495B**。全部新增資料留原batch、包含本次帳本自身；未新增build/export、full replay、刪舊資料或覆寫frozen證據。

**下一個工程項目仍為J3的X2**：先核容量與有界輸出，再在main50隔離離線版只消融confirmed-preserve的winner覆蓋，C36h保留為reference；比較6160初選、6214後relation→root→plan→Down及新第一guard，保留A/B/C/E與crossing／rotation／late alignment／completed／unknown Down反例。只回答該機制的因果作用，不直接改production或宣稱Miss改善。本次沒有派送／啟動X2、監看chat或建立自動化。

### J6. X2 winner-only 開發交付（2026-10-02，待總控獨立驗收）

**最新交接指標：[CONFIRMED_PRESERVE_ABLATION_X2_HANDOFF_20261002.md](../04-offline-research/CONFIRMED_PRESERVE_ABLATION_X2_HANDOFF_20261002.md)。** 本節只追加X2實作／實驗／自驗，J1–J5原文及其當時狀態保留；開發chat不宣告正式結案。C36h仍為behavioural／experimental baseline，main50 control／donor仍50／27／11、live0。

- 只在新offline export使`selected=&*established`受開關控制；原`preserve_confirmed=true`、eligibility與ambiguity bypass保持，initial score／identity／history／projection／owner／clock無第二策略介入。三角色明列reference／control／winner-only variant，綁定同一新manifest實際SHA、role／variant、source provenance／tracking source及實體binary SHA；main50兩角色共用同一ablation-capable binary，分別用明確variant／provenance綁定。
- 五次完整fixed-C36g-pixels replay：每次7722 PNG核對、7715 perception calls／7 skip，owner cadence／frame-first／zero fake recognition與RPC／原session／owner固定；variant兩次trace-on與trace-off的canonical digest、events、逐幀state digest及mechanism byte-identical，兩次on trace相同。C36h／main50 control與X1 digest、events及109幀核心trace等價；不是C36h新live或gameplay反饋。
- 全prefix首個different-winner及semantic-state分歧在1535：control把best3覆蓋為confirmed2；variant保留3、原flag仍true／ambiguity=false，卻被原`confirmed_line_relation_conflict`拒絕。1533–1537自動context保留；全receipt第一差異為variant1547 vs control1552的Down，時間／位置不同，不能只按runtimeID比較。
- **D：winner-only有因果作用，但不足恢復C36h垂直鏈。** 6160兩main50角色仍初選水平316；窗內首state分歧6194，variant保留best317後被conflict拒絕。6199原90ms history到期後重新累積317；6200無fitted root時由原current-drag-overlap路徑plan／成功水平Down，6205完成Up，共6命令，Down前綴0→1。6214垂直318雖是best，卻被自然演化後confirmed317的原conflict再次拒絕，line0／samples0／rootnull；completed owner亦未復活。C36h垂直Down仍是6215。新增Down不等於命中改善，未做第二消融或production移植。
- A／C／E所選cohort的window state及prefix動作相同，E仍為正常control；B absence release保留，但4963前置Down約晚0.539547ms／dx−0.003638663px，4986 Up同時間、dx−0.023557888px，不能稱全部controls完全不變。三角色local-note重複successfulDown=0、unknownDown=0、unknownretry=0、EOFcontact0；physical身份、unsupportedMove／prematureUp等全局正確性及gameplay仍unknown，unknownreceipt分母0，原fake-clock負例保留。
- 新Release與Debug／ASan均實跑：C36h213/213、main50control244/244，fail0／skip0；variant兩套均**243/244、fail1／skip0**，唯一原gold失敗`GameTracking.CrossingLineDoesNotStealAnApproachingConfirmedNote`保留，不排除／改期待。15個實際CLI预期負例符合，X1 R1／R2仍拒絕；X1正例report SHA仍`37d469caa0017be00fb78736fd69fb5cc87585c40b9fd671bb7e8397c420e7f7`。原build及兩次maintenance-wrapper失敗保留於attempt帳，未用舊binary冒充驗證。
- 新資料留campaign內`confirmed-preserve-x2`，獨立硬上限64MiB，包含失敗／logs／source／provenance／ledger；campaign＋prior仍受8GiB。最終精確bytes、remaining及另列out/x2 build／export磁碟帳見[capacity-ledger.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/capacity-ledger.json)。X1仍130,356,292B，既有evidence／export／frozen工具及formal source SHA保護，status歷史前綴保留；沒有刪舊／另root規避。

下一步唯一建議：總控先獨立驗收本交付，再以既有D6194–6214與crossing負例冷研究confirmed-line conflict／水平overlap的線局部幾何role及有效切線證據，才決定是否值得另一個單機制實驗。HEAD／main／唯一worktree不變，正式src/include／根CMake無diff；本chat交付後停止，未啟動live／emulator／真觸控／訓練／goal／自動化、未訊息其他chat、未commit／push。

### J7. X2總控獨立驗收：通過，離線因果實驗結案（2026-10-02）

**Verified：實作符合winner-only介入，結果可由原pixels重現；X2作為因果實驗交付通過。** 這不要求variant變好：原crossing保護的退化是有效負結果，保留原gold與非零exit；此次消融不作production候選。HEAD仍 `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、main／唯一registered worktree、50／27／11，所有開發成果仍未提交。

獨立驗收包：[acceptance-20261002/acceptance-summary.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/acceptance-20261002/acceptance-summary.json)，SHA256 `5436b5f6d981df40554dcce9bbbcb41e5aa15b7a2650ffb5a075883de8349dc8`；[已校正交接](../04-offline-research/CONFIRMED_PRESERVE_ABLATION_X2_HANDOFF_20261002.md)。

- **實查source／patch**：main50新export與X1共同檔案僅 `src/game_tracking.cpp` 不同。唯一策略改動是以offline switch控制 `selected=&*established`；原eligibility、`preserve_confirmed=true`、ambiguity bypass、relation conflict及其他策略保持。新增diagnostics只向外輸出。C36h／control與X1的canonical digest、events及109幀核心trace等價，由工具重算確認。
- **本輪Release實跑**：C36h **213/213**、main50 control **244/244**，fail0／skip0；variant **243/244**、fail1／skip0，唯一失敗仍是 `GameTracking.CrossingLineDoesNotStealAnApproachingConfirmedNote`。新增測試直接走原tracking，確認flag未關閉及margin<8仍保留ambiguity bypass。保存三套XML／logs與實際binary SHA；未重建或重跑ASan，開發提供的ASan binary／XML僅本次核對hash，不冒稱本次實跑。
- **本輪新增一次完整variant replay**：使用同一X2 manifest及frozen binary、完整preroll／owner cadence／frame-first／原FakeClock政策，全部 **7722 PNG**核對、**7715 calls／7 skip**、6083 events、EOF contacts=0。digest仍 `a4f79cde887df2689506cf1105cda3e5403f9984cada7c61cf7b646d6c16f042`。events、逐幀state digest、mechanism、109幀trace、首次介入context及兩份original stream共**7檔byte-identical**；新summary的host cost／容量可不同。開發5次與總控1次分開計，沒有新live。
- **入口／比較實跑**：15個預期CLI拒絕全部重現，含原R1／R2；錯輸入不產有效report，既有output不變。三角色comparison、causal-chain report及X1正例皆重算byte-identical。詳[比較檢查](../../../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/acceptance-20261002/comparison-checks.json)，沒有以版本號或run順序替代角色／source／binary／manifest綁定。
- **檔案保護**：重算23個workspace source及23個snapshot、25個binary／DLL、159個export、177個原X2 artifact、883個先前受保護檔與85個formal source，0 mismatch；數目為檢查次數，不作獨立場景數。原X1仍130,356,292B；formal `src/`、`include/`、根CMake無diff。只在本次驗收目錄新增資料，未覆寫frozen檔。

#### 獨立確認的因果鏈與三項文件校正

全prefix首個不同winner／state仍為 **1535**；D所選目標窗內首差 **6194**。保留winner後，先被原relation conflict拒絕；6199近期history自然到期，6200沿水平317的current-overlap路徑Down，6205已Up。6214仍未產生C36h的垂直relation／root／plan／Down。**「winner-only足以恢復該鏈」被此次固定輸入實驗否定**，不是把較多Down稱成改善。A/C/E所選cohort action相同，B的細微時序／位置差及absence release原樣保留；E仍為正常control。

直接核對原source及獨立重播trace後，校正開發交接三處描述；校正前原文及逐幀摘錄保存在[causal-evidence-and-corrections.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/acceptance-20261002/causal-evidence-and-corrections.json)旁的 `handoff-before-corrections.md`，未改任何原始報告：

1. **6160次佳線**：289.121193是水平**305**，不是垂直315；垂直315為917.604857，勝出水平316為26.596113。這是distance主導初配的直接證據，不歸因preserve。
2. **6214 conflict不是舊317仍可見**：317最後有效量測在6211，6212起已從當前lines消失；6214只有318、305。最近317距今61.8127ms，仍在90ms界內，但舊水平切向與新垂直318的絕對dot約0.0000582，未達same-local-line的0.97，故幾何續接被拒。**6194是舊305仍可見，6212–6214是缺線後的新線不符同線續接；相同reason不能當同一子因。** 6205已完成contact的owner也未被復活。
3. **窗內state指標範圍**：`compare_data`只驗所選target幾何／body、line pose、reason／samples／root容差及lifecycle，沒有逐項比較owner／contact／scheduler相等。「首差無」不能稱整條鏈完全不變。全prefix原始digest與X1 control核心trace等價檢查是另外的範圍；action及owner證據另讀。

本次另目視原1535、6200 RGB；6200確有水平線與側向Drag當前幾何交疊，但**judgment role與遊戲是否採納仍Unknown**，AI目視不增加human gold。原perception／早期state、source age、真recognition／RPC delay、物理identity、77 Miss逐Note原因及跨曲效果均未被此次實驗解答。

**容量**：總控新增 **11,020,866B**，X2實測 **52,804,100／67,108,864B**，剩 **14,304,764B**；campaign＋prior **7,860,197,197／8,589,934,592B**，剩 **729,737,395B**。新build/export=0，原資料與失败分母保留；不把驗收新增量寫回舊frozen ledger。

**下一個工程問題**：用既有D6160、6194、6200、6212–6214與crossing反例建立線角色／有效切線的逐物件證據，分開初配、舊線可見的competition、舊線缺失的幾何continuation及current-overlap資格。先比較線局部距離趨勢、相對運動、切向／法向與C36h role gate，量化哪個訊號能區分這些情況；不直接刪preserve／conflict、縮短expiry或放寬lease。據反例才決定下一個唯一bounded C++介入，保持completed／unknown Down不復活。本次未派送或啟動下一task，未恢復emulator／真觸控／live／訓練／goal／自動化。

### J8. X3線角色／有效切向開發交付（2026-10-02，待總控獨立驗收）

**最新交接：[LINE_ROLE_VALID_REASSOCIATION_X3_HANDOFF_20261002.md](../04-offline-research/LINE_ROLE_VALID_REASSOCIATION_X3_HANDOFF_20261002.md)。本輪開發自驗完成，未宣告正式驗收／結案。** C36h仍為主要behavioural／experimental baseline；main50 control為donor／可能regression，50／27／11、live0；X2 winner-only variant不是baseline。

- 新增有界、唯讀C++20 report，使用已驗收X2三角色trace／first-context／events／index／原PNG及provenance。固定父manifest／acceptance實際SHA、run role／variant、binary與來源hash；feature只取current／≤6幀、90ms近期幾何，line16／note128、單report6MiB，new batch32MiB及campaign＋prior8GiB硬上限。沒有改formal src/include／根CMake或association／owner策略，shadow rule=0、新full replay=0。
- 兩次actual report各3,034,295B、byte-identical，SHA `8fce0da93a9455c9762182ce16bda855ac0600ae61385bbc92251746a359c777`；337 role frames、114 unique PNG hash核對。C36h／control／variant分別109／114／114幀、132／137／137 selected object occurrences、315／323／323 valid measured line pairs、74／76／76 unknown pairs，未silent truncate。完整來源event分母與selected receipts另列，不換算gameplay rate。
- **初配與後續guard分開**：6160 main50仍選水平316（26.596113），次佳水平305（289.121193）、vertical315（917.604857），confirmed0／preserve false；C36h role gate選vertical356。6161才有measured motion，可見Note大多沿水平切向、往vertical法向。runtime1649→1650與跨角色physical continuity仍proposed；confirmed建立只有predicate／first-seen bracket，未輸出event維持unknown。
- **closing不足以證role**：6194水平317自己移動，distance也close且比vertical更快，Note主要沿其切向；motion_valid=false只使model velocity為null，不抹除兩次current pose的measured secant。6200水平current overlap／rootnull導致fake Down520，6205完成Up，不能稱命中改善。6194舊305仍在current bank的first guard是old-visible；6214舊317自6212缺失、age61.8127ms、abs tangent dot0.0000582343，first guard是tangent-not-continuation；completed520不得復活。
- 原6211–6214 RGB仍有逐漸變暗的上方水平線；研究ROI九點mean RGB約195.74→190.22→178.22→171.67，6212起九點均不達原white predicate。亮度降低與trace缺線相符為strong inference，physical317續接仍proposed；不能把missing observation bank等同原pixels不存在，也未證唯一提取子因或放寬閾值。
- G1535 best3／confirmed2分數48.229426／178.459826，variant仍被old-visible conflict清0；confirmed horizontal的measured approach與原crossing gold保留。父X2首eligible804、首different winner/state1535、首global action variant1547／control1552不改。A既有body／rails接觸、B absence release及微小前置差、C Flick、E正常control均分開grounding，沒有重標E或增加human gold。
- 自驗Release **X3 13/13、C36h 213/213、main50 control244/244，fail0／skip0**；既有27張opt-in RGB fixture實跑。9個actual CLI負例皆预期拒絕／無新report，existing output SHA不變。X2 variant原243/244 crossing失敗未重跑、未改gold；build失敗、首次report缺optional欄位失敗及初次CLI mutex假覆蓋修正均保存，不報ASan或live效能改善。
- X1 inherited883次檔案檢查、X2 25 binaries／DLL、159 exports、23 snapshots相符；原X1 130,356,292B、X2 52,804,100B不變。J1–J7原文保留，只改top與append J8。本輪X3 **6,950,299／33,554,432B**，剩26,604,133B；campaign＋prior **7,867,147,496／8,589,934,592B**，剩722,787,096B。包含全部失敗attempt與帳本／summary自身；out/x3 build另計120,638,741B，新export=0。首次Finalize將已授權status改動誤列差異的失敗帳本保留，修正後verified核對0 mismatch，歷史原文兩次均通過。

**唯一下一個提案**：總控先獨立驗收本交付；之後可做D的pre-confirmation proposed-role oracle，把未confirmed provisional choice指定為當幀有支持的vertical候選，只變此一機制，其他preserve／conflict／owner／clock保持，判別初配與fragment再初配是否足以恢復鏈。這不是role gold或可部署rule，至多2次有界variant重算、先核≤24MiB新額度及所有負例；未執行或派送。現有closing／overlap證據不足以凍結新候選，也不直接刪guard、縮expiry／放lease、重試Down或恢復live。本chat交付後停止，未啟動emulator／manual-session／真觸控／訓練／goal／自動化／下一task，未訊息其他chat、未commit／push。

### J9. X3總控獨立驗收：通過，離線研究與工具結案（2026-10-02）

**Verified：沒有阻擋驗收項。** 本次直接審查X3 feature／report／CLI／tests／offline CMake及維護腳本，核對原tracking、current-drag-overlap、receipt prefix與白色分類程式，再實跑凍結工具。X3驗收範圍是固定來源下的唯讀診斷與反例研究，沒有新的shadow rule或production修正；不宣稱Miss改善。HEAD仍 `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、main／唯一registered worktree、50／27／11。既有X1–X3未提交工作保留，正式`src/`、`include/`、根CMake無diff。

獨立驗收包：[acceptance-summary.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3/acceptance-20261002/acceptance-summary.json)，SHA256 `75ae973b9003432493c0096b5c2d281669a27953b2a4e708f5e5b054d4c88fd6`。交接中的自驗數字仍作歷史紀錄；本次新增驗證分開計：

- **Release實跑**：X3 **13/13**、C36h **213/213**、main50 control **244/244**，全部fail0／skip0；保存XML、log、commands及binary SHA。C36h／main50使用原27張opt-in RGB fixture，兩套共用測試不合算独立場景。沒有重建、重跑ASan／全repository套件；X2 variant原crossing失敗仍保留，未改期待、未在本輪重跑。
- **兩次獨立report重算**：各 **3,034,295B**，均與交付report byte-identical，SHA仍 `8fce0da93a9455c9762182ce16bda855ac0600ae61385bbc92251746a359c777`。共337 role frames、114 unique PNG核SHA；三角色valid pair **315／323／323**、unknown **74／76／76**，沒有silent truncate。這是讀既有trace的新報表，不是兩次新full replay。
- **9個CLI負例實跑**：角色、路徑、trace SHA、缺file binding、parent SHA、budget、float schema、existing output與missing manifest，全部以原預期reason非零拒絕；不是mutex busy搶先失敗，沒有產新report，既有report SHA不變。來源綁定限已凍結query，不冒稱任意trace鑑證或所有磁碟容量邊界都已實測。
- **保護核對**：7個workspace source＋7個snapshot、5個X3 binary／DLL、208個重用export、86個formal檔、336個先前受保護項、85個原X3 artifact均相符；再核對繼承X1的883項、X2的25 binary／159 export／23 snapshot，0 mismatch。數字為檔案檢查次數，存在重複路徑。執行後再驗X3 source／binary／原artifact不變；X1仍130,356,292B、X2仍52,804,100B。

#### 因果證據與推論邊界

1. **初配問題仍在preserve之前**：6160 confirmed=0，main50選近水平316，垂直315雖存在但score遠高；6161才有第二份current pose，可量到Note主要沿水平切向、往垂直法向。6174重疊時identity ambiguous／1649→1650，不把它當已證實的physical split，亦不以alias oracle跳過它。
2. **closing不能單獨證明role**：6194水平317自己移動，距離縮短比垂直315更快。`motion_valid=false`表示擬合model無效，並不否定兩份有效current pose可計secant；report將model速度留null是正確分層。這仍不是瞬時速度或root。
3. **相同conflict reason的兩個子因保持分開**：6194舊305仍current；6214舊317缺失、age61.8127ms，與垂直318的abs tangent dot約0.0000582，先不符同線幾何續接。從最後exported accepted sample重建的guard仍標Strong inference，未輸出的replacement count與精確confirmation event維持Unknown／proposed，沒有升格為內部state實測。
4. **RGB缺失與observer缺失不同**：本次另目視原6160、6174、6211、6214。6211與6214上方水平灰線仍有像素支持；C++九點probe的white pass為6211的3/9、6212–14的0/9，與原`r/g/b>195`且channel差<35分類相符。亮度下降與line bank缺失相容，但九點不能證明全行／所有detector branch的唯一失敗原因，更不授權放寬white threshold；physical317續接仍proposed，human gold不增加。
5. **幾何／action／遊戲角色不混用**：6200既有overlap條件產生intent520 Down，6205已Up，6214仍completed。這不是C36h的6215垂直觸控恢復，也不是命中證據。A無root仍有body contact、B absence、C Flick與E使用者正常control的定位保留；未從局部state指標推論整個owner／scheduler完全相同。

逐項摘錄與本次目視來源見[causal-checks.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/line-role-x3/acceptance-20261002/causal-checks.json)。沒有需要重寫交接的事實錯誤；本次只補驗收狀態與限制。physical identity／judgment role gold、遊戲採納、原perception race／早期state、source age、真recognition／RPC時序、77 Miss逐Note原因與跨曲效果仍Unknown。

**容量**：本次新增 **6,500,739B**，X3總量 **13,451,038／33,554,432B**，剩 **20,103,394B**；campaign＋prior **7,873,648,235／8,589,934,592B**，剩 **716,286,357B**。驗收script、兩份新report、XML／logs、文件原文snapshot及summary自身皆計入。新build/export=0，既有out/x3為120,638,741B另列；舊frozen ledger未覆写、未刪失敗evidence。

**下一步建議保留X3 §7，但仍是提案**：在新隔離main50離線variant，只介入未confirmed時的provisional選線，指定當幀已存在、有RGB／幾何支持的D垂直候選；保留原identity ambiguity、preserve、relation conflict、history、owner與clock，不能同時提供正確identity／history或解除guard。以C36h為reference，找初配修正後第一個新阻擋點，保留crossing／late alignment／rotation／neighbor／completed／unknown Down反例。成功只支持早期選線對固定pixels鏈的因果作用，不是可部署rule。至多兩次有界variant重算，先核新batch與campaign容量；本次沒有派送或執行它。

目前仍不需emulator來回答此離線因果問題；沒有凍結的新C++策略候選可驗。之後形成通過冷回歸的candidate，才另行安排有限live驗證遊戲採納、真時序及觸控改變後的畫面反馈。本輪new full replay=0、new live=0，未啟動emulator／真觸控／manual-session／訓練／goal／自動化、未commit／push。

### J10. X4未confirmed初配proposed-role oracle開發交付（2026-10-02，待總控獨立驗收）

**最新交接：[PRECONFIRMATION_ROLE_ORACLE_X4_HANDOFF_20261002.md](../04-offline-research/PRECONFIRMATION_ROLE_ORACLE_X4_HANDOFF_20261002.md)。開發自驗與兩次有界實驗完成，未宣告正式驗收／結案。** 本節只追加，J1–J9及baseline原文保留。HEAD仍`f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、main／唯一registered worktree，正式src/include／根CMake無diff；C36h tint1仍baseline，main50 control／donor／可能regression仍50／27／11、live0；未組合X2消融。

- 新隔離offline main50 export只在原score loop之後、confirmed-preserve之前介入`selected`指標：D6158–6220、Drag、confirmed0、當幀唯一current core／有支持vertical候選，綁source_frame／PNG SHA及geometry，無runtime ID／history／action oracle。原分數、confirmed override與flag、identity/relation ambiguity、conflict／continuation、history／root／owner／lease／scheduler／clock保持。packet為proposed，human／expert gold=0；缺／過時／無效／非唯一候選不介入，沒有造線。
- **2次完整variant replay（ON→OFF，未第三次／retry）**：每次7722 PNG、7715 calls／7 skip、5983 events、success、EOFcontact0；同一frozen replay binary與packet。canonical digest=`18b3fe4d19a71097dbc83826eec0acdc3e2aa2abd1e1f05ebaedb591874a3832`，state-digests／events／oracle-attempts byte-identical。舊C36h reference與main50 control明列同input/policy的原X2 manifest橋接與逐檔SHA，不冒稱重產。
- **Verified：early provisional選線在固定pixels上足以恢復所選D鏈。** 首個state／幾何差6160：同局部note1643，control水平316／hit≈(20.995,273)→X4垂直315／hit≈(938,287)。只有6160–6164共5次介入，6164自然confirmed315。6170–6172原identity ambiguity仍清line/root，6174在自然演化中不再ambiguous，未提供alias/history。6199先有vertical root但尚不符owner近時域；6210舊315缺bank且新best水平，原conflict拒；6211新vertical318幾何續接相容但仍等第二份支持，6212才原規則續接並重新累積history。6214 samples3／history33.2308ms、fresh root `106725530098ns`／uncertainty2.128659ms，原short-linear-fit plan519→成功Down `106689291400ns`、(988.5,287.997100621)，同contact0五Move，6222當前物件缺失撤銷Up。D1Down＋5Move＋1Up，沒有completed復活。
- C36h reference D的Down仍在`106695505268ns`（delivery6215），X4早6.213868ms、位置相同；main50 control的此cohort無Down。各run局部ID不作physical gold。首global action差忽略note／intent ID仍是X4 D6214 Down相對control另一物件6216 Down，時間／位置不同；797/7722 raw-frame digests不同不能當797個物理錯誤。
- A12/12、B12/12、C9/9、E13/13的scene/lifecycle/owner/contacts、scheduler與prefix summary相同，G1533–1537 scene/lifecycle/owner/contacts5/5相同；G5/5不含scheduler。E正常control、B absence release保留。全Down505、local-note重複Down0、unknownDown0／receipt未知分母0、EOFcontact0；全局physical duplicate／Move／Up遊戲語義仍unknown，實際unknown no-retry另由direct-owner負例覆蓋。
- **Release及Debug／ASan新variant皆249/249、fail0／skip0**，原crossing、旋轉Hold head/body/tail、late alignment、neighbor、completed及真正unknown Down守門保留；既有frozen C36h reference213/213、main50 control244/244 Release另實跑，27張RGB fixture使用，未重建舊baseline ASan。X2 variant原crossing失敗未改gold／未重跑。CLI最後20/20符合預期，含原R1 wrong-input／R2 swapped-lineage；合法X1正例與原SHA `37d469caa0017be00fb78736fd69fb5cc87585c40b9fd671bb7e8397c420e7f7` byte-identical。初編譯、248/249新測試、ASan wrapper／路徑與初CLI18/20 harness失敗完整保留，不改契約迎合期待。
- 新batch `preconfirmation-role-x4`硬限24MiB、campaign＋prior8GiB；source／packet／trace／events／logs／失敗／ledger自身皆計額，PNG／舊trace僅引用。精確最終用量與另列out/x4 export/build/tools見[capacity-ledger.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4/capacity-ledger.json)；[preservation-check.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4/preservation-check.json)核X1 130,356,292B／X2 52,804,100B／X3 13,451,038B不變、原SHA與J1–J9原文。report/query actual SHA及工具bind見X4交接；第二輪後只加強唯讀query role/trace驗證，原replay binary/source/packet未改。

**最終容量／保護自驗**：X4新增16,571,837／25,165,824B、剩8,593,987B；campaign＋prior7,890,220,072／8,589,934,592B、剩699,714,520B；out/x4編譯產品另列572,866,067B。1888次檔案核對0 mismatch，HEAD／main／唯一worktree、formal source及J1–J9歷史原文保持；舊X1／X2／X3 bytes與SHA不變。兩份唯讀self-report byte-identical，並非總控獨立驗收。

**唯一下一步建議：總控先獨立驗收X4；之後才研究可替代proposed packet的early-role資格與abstain反例邊界，再決定下一個單機制冷實驗。** early choice因果足夠性是此次Verified；6211 replacement子因的counter未直接export為Strong inference；泛化runtime資格為Hypothesis。physical identity／judgment role gold、遊戲採納、source age／原perception race／早期state、真recognition/RPC、77 Miss逐Note原因及跨曲效果仍Unknown。没有production修正／新candidate凍結；未啟動下一chat、emulator／真觸控／manual-session／live／模型訓練／goal／自動化，未commit／push。

### J11. X4總控獨立驗收：通過，離線因果實驗與工具結案（2026-10-02）

**Verified：沒有阻擋驗收項。** 驗收範圍是固定來源的 proposed-role 單機制實驗與離線工具，沒有 production 修正、正式候選或 Miss 改善宣告。HEAD 仍 `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、main／唯一 registered worktree，50／27／11；既有 X1–X4 dirty 工作保留，正式 `src/`、`include/`、根 CMake 無 diff。C36h tint1 仍是主要 baseline；本次 main50 oracle variant 不升格 baseline，也沒有組合 X2 關閉 preserve 的消融。

獨立驗收包：[acceptance-summary.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4/acceptance-20261002/acceptance-summary.json)，SHA256 `7f685429c59de8cf4bd0ab6b65f5f14da1c0bb8ea245344fc8d8fef67cd01fb8`。J10／X4 §1–6 的開發自驗作歷史保留；下列為總控另行實跑與核對：

- **Release 與 Debug／ASan 各 249/249，fail0／skip0／disabled0**，使用已凍結 X4 binaries 與原 27 張 opt-in RGB fixture，保存 XML／log／command／binary SHA。獨立 source review 核對 15 個新 hook tests 及原 tracking／owner 分支；没有重建工具或重新跑舊 C36h／main50 套件，亦未改 X2 原 crossing 失敗 gold。
- **Packet 重算一次、report 重算兩次**：packet SHA 仍 `f8e303c06b03648c874ec565e14c6fd0e4ec26d531d5400a41aa9b0e034d8f8f`；兩 report 各 235,965B，與交付 byte-identical，SHA 仍 `cab91f05748691f81849eda8b5861986ae3e2afb8323321e1511509302299e47`。這是讀已保存 trace 的重算，new full replay=0；兩次 full variant 額度已由開發實驗用完。
- **20/20 CLI 負例**皆以預期 reason、非零 exit 拒絕，沒有新有效輸出；existing report SHA 不變，沒有以 mutex busy 假覆蓋。包含 parent/input/source/binary/tracking/oracle／逐檔 SHA、role/trace/swap、缺 binding、float schema，以及原 X1 R1/R2。所有呼叫與結果另存，沒有覆寫自驗的成功或失敗。
- **來源保護**：1646 個先前受保護項、31 個 workspace source、31 個 final snapshot、28 個 initial snapshot、14 個 final binary／DLL、208 個 export、204 個 frozen artifact 全相符，0 mismatch；結束時再核對。計數是檔案檢查次數，存在重複路徑。兩 export 的共同檔案只有 `src/game_tracking.cpp` 不同；formal diff 空。X1 130,356,292B／X2 52,804,100B／X3 13,451,038B 不變。

#### 因果結論與限制

1. **介入是初配 winner，不是 confirmed-preserve 修補。** 逐行比對確認唯一策略 mutation 為原 score loop 後的 `selected`；best/second 分數、原 confirmed override、ambiguity、conflict／continuation、history、root、owner／lease／scheduler 保持。只有 6160–6164 五幀 applied，當時 confirmed=0；6164 自然 confirmed315，之後不再介入。**ON/OFF 是 diagnostic trace 開關，兩輪 oracle 都啟用**，用來驗診斷不改語義；策略對照是已驗收、相同 input/clock 的原 main50 control，C36h 是 reference。
2. **首個 state／實際幾何分歧是 6160 的 relation。** 相同 current core 下，水平316→垂直315；尚無 root／Down。6170–72 identity ambiguity 仍清空 relation/root，6174 的自然 identity 演化不同不是額外 identity oracle。逐一核對原 trace／oracle stream 的 D63/63 行及完整 event stream 所選 cohort，與 report 一致；runtime ID 僅用於該 run 內索引，不作 physical gold。
3. **6210–6214 沒有跳過續接 guard。** 6210 缺 current vertical bank，原 conflict 拒絕；6211 新 vertical318 的 tangent／gap 相容但仍拒絕，6212 原規則才續接並重建 history。6211 缺第二份、≥12ms replacement 支持為 **Strong inference**，內部 counter 未直接 export。6214 samples3／span33.2308ms 的 fresh root 才接原 short-linear-fit plan519；沒有借舊 root 或放寬 owner。D 實際為 1 Down＋5 Move＋1 Up，Up 原因是 6222 current object missing，不能稱遊戲完成。
4. **動作恢復是離線因果足夠性，不是 hit。** X4 Down `106689291400ns` 比 C36h reference 的 `106695505268ns` 早 6.213868ms，位置相同；後續五 Move 與 Up 的 phase/time/position 逐項相同。原 main50 所選 D cohort 無 Down。首 global action 差忽略 note/intent ID 仍在 ordered index1636，並非純 ID hash 差。797/7722 raw-frame digest 差不是物理 failure 數。原 pixels 已受當時 C36g 觸控影響，本 replay 沒有新觸控帶來的遊戲畫面 feedback。
5. **原圖覆核補充**：本次目視並重算 6160／6174／6210／6214 PNG SHA。6210 原 RGB 的 x≈1007 垂直白線仍明顯存在，中段有 effect；6214 x≈989 垂直線、y≈576 水平線、yellow core 與上方灰水平線可見。6210 bank 缺線不代表 pixels 無線，確切提取／篩選子因仍 **Unknown**，不据此新增 detector 修補。AI 目視、判定角色及跨幀 physical continuity 仍 proposed，human gold 不增加。詳見 [causal-checks.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4/acceptance-20261002/causal-checks.json)。

A12／B12／C9／E13 共46幀完整 exported state（含 scheduler／prefix）相同；G5/5 的 scene/lifecycle/owner/contacts 相同，G 不含 scheduler。這些 controls 多在介入前，支持 scope／前綴等價，不能充作已驗所有 downstream safety。全成功 Down505、local-note duplicate0、unknown receipt分母0、EOFcontact0；真正 unknown Down/no-retry 由 direct-owner test 另驗。physical duplicate、全局 unsupported Move／premature Up、judgment-role gold、遊戲採納、原 race／早期 state／source age、真 recognition/RPC 時序、77 Miss 逐 Note 原因及跨曲效果均仍 Unknown。C36h 的已保存實戰仍為 77 Miss，沒有因本實驗改寫。

**容量**：本次驗收新增 **990,558B**；X4 總量 **17,562,395／25,165,824B**，剩 **7,603,429B**；campaign＋prior **7,891,210,630／8,589,934,592B**，剩 **698,723,962B**。新 scripts、packet/report、XML/logs、raw 摘錄、文件原文 snapshot、驗收清單及 summary 自身全計入。new build/export=0，既有 out/x4 572,866,067B 另列；舊 frozen ledger、失敗 attempts、X1–X3 evidence 不改寫或搬移。

#### 下一個工程 task 提案：X5 early-role 資格與 abstain 反例研究

先沿既有 X3 feature/report 與 C36h role gate 比較，形成最多一個可說明、可拒絕的 bounded C++ 離線 shadow rule；先不接正式 runtime。輸入只用 current candidates 與既有有界近期幾何，分開 Note 朝向、Note/line 位移與線法向／切向，缺 motion 或競爭無法區分時輸出 unknown/abstain。輸出需列 eligibility、measured features、競爭 margin、選線或 abstain 原因及首差；不能把 X4 的 ordinal、y-band、垂直方向、packet、runtime ID、歌名或未來 root/動作寫成 production 資格。

先在 D6160–6164／6170–6174／6210–6214 及既有可用 real traces、原 crossing、late alignment、旋轉 Hold、線追 Note、同向 neighbor／短 fragment／missing line 反例作唯讀評估。驗证 closing／當前同法向／overlap 各自的不足，保留原 confirmed／conflict／owner 及 unknown/completed 不復活。與 C36h 現有規則對照，判定哪些機制值得回移 C36h，而非以 main50 修好一窗就升格 baseline。若沒有可區分訊號，交付反例與 abstain 邊界即停止，不硬補 threshold。只有形成通過反例的單一規則，才另決定隔離 variant／有界 full replay 及 candidate freeze。

本次尚未派送 X5。現在不需 emulator：尚無正式候選可做 live A/A 或 A/B；遊戲採納、真時序與 feedback 仍須日後另行授權的有限 live 驗證。未啟動 emulator／真觸控／manual-session／訓練／goal／自動化，未 commit／push。
### J12. X5有界early-role／abstain研究開發交付（2026-10-02，待總控獨立驗收）

**交接：[EARLY_ROLE_ABSTAIN_X5_HANDOFF_20261002.md](../04-offline-research/EARLY_ROLE_ABSTAIN_X5_HANDOFF_20261002.md)。開發自驗完成，未宣告獨立驗收或結案。** 本節只追加，A–I／J1–J11原文保留。HEAD仍`f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、main／唯一registered worktree，正式src/include／根CMake無diff；原X1–X4 dirty source／evidence／frozen工具保持。C36h tint1仍主要baseline、77 Miss未改善；main50 control／donor／可能regression仍50／27／11、live0。

- **只讀三個已驗收factual history，new full contact replay=0。** 原X2 C36h reference／main50 control及X4 trace-ON history明列bridge與source／binary／逐檔SHA；X4 ON/OFF是diagnostic開關，策略對照是原main50。五窗各109幀、main50／X4另有G5幀，合計337 role frames／114 unique PNG核SHA；三history是同一C36g recording，不是三首歌。X2關閉preserve variant排除，原crossing失敗不改gold。
- **先spec／探索，再凍結最多一個`X5-NDA-v1`。** pure C++20 input只含current note／line geometry、explicit identity／confirmation與最多6-frame／90ms量測；local ID只索引continuity，無ordinal／歌名／固定方向／packet／未來／owner或觸控API。至少3 paired pose／12ms、note-driven normal approach、有限line contribution／角變化、support及competition margin；missing／ambiguous／moving-closing／rotation／overlap等有明確abstain。appearance不作遠處必要配對条件。model invalid不抹除兩pose secant，無current line不補造。Hold head/body/tail全部排除，原owner守門另驗。
- **完整分母保留：** C36h109幀／394 target／1172 target×line、0推薦／394 abstain；main50 control114／413／1208、12／401；X4 history114／413／1208、12／401。幾何eligible candidate210／230／234，early scope0／68／67，valid latest secant926／952／964、unknown246／256／244。C36h缺explicit confirmed export，全abstain是資料缺口，不能當改善；24推薦是高度相關的target出現，不是命中或独立樣本。
- **D6162–6164幾何一致性，不是因果恢復。** control shadow首次在6162推薦315、factual winner316；6160一pose／6161兩pose仍abstain，比X4五次介入晚兩幀，覆蓋3/5。6170–72 identity ambiguity不跳過，control6174仍ambiguous；6194／6200／6210–14已confirmed，排除early scope，不改conflict／replacement／completed。沒有新state、root、Down或Miss結果。
- **Verified synthetic否決：** 相同typed Input的note向下prefix，world A之後對齊水平，world B之後轉右對齊垂直且水平為decoration；獨立scenario labels／未來不進rule，v1選水平而錯B。沒有合成相同PNG或驗證遊戲合法性，不能把此反例升格Phigros role gold；它否定此版本的unique-role足夠性，不是所有bounded pixel rule／學習的不可能性。failure保留，不调threshold救結果。
- **Release／Debug-ASan新tests各27/27、fail0／skip0／disabled0。** 原frozen C36h213/213、main50 control244/244（27張opt-in RGB）、X3 13/13、X4真正unknown Down／completed owner兩guard 2/2另實跑；原crossing、late alignment、線追Note、旋轉Hold及contact守門保持。共有case不相加為新場景；未重建舊baseline ASan、未重跑X2失敗variant。
- **兩份Release完整report各3,953,654B、byte-identical，SHA`1b6154aa6ce16bbb901ed63a7692f0fc3a31a9795f4fcff473da75f2db6f3fcf`。** 全物件／線以共享34／9欄array保存、不truncate；原score／selection及X4 proposed packet只作後置comparison。ASan完整reader在原512MiB commit上限、明列較小quarantine下pass，除binary SHA外report一致。預設quarantine的`Failed to mmap`、mutex wrapper、初configure／build／test／schema／report超額失敗均保留，不虚報pass。
- **收尾只修驗收writer入口：** default仍16MiB開發cap，明確`--acceptance`可用24MiB batch／8GiB campaign額度；旗標不進features／decision。原報表reader及source另凍結保留，最後binary重build且27/27再過；Release兩次／ASan一次從原輸入重算337幀，完整JSON與原兩report僅binary SHA不同。新reader不能冒稱與舊report byte-identical，見[final-reader-verification.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/final-reader-verification.json)。
- **CLI原20/20再由最後binary實跑通過，另6/6模式負例通過。** 包含parent／acceptance／query／rule／index／role／root／file／source／binary／tracking、float schema、budget／path／existing及原X1 R1/R2；新6例含未知mode、acceptance existing／wrong-parent／outside、default實際16MiB超額、verify missing。無新有效output，existing SHA保持，無mutex假覆蓋。合法舊X1入口輸出與原SHA`37d469caa0017be00fb78736fd69fb5cc87585c40b9fd671bb7e8397c420e7f7`一致；沒有用開發額度實寫acceptance正例去消耗預留。

**容量／保護：** 新batch只在`early-role-x5`，硬限24MiB、開發限16MiB、保留至少8MiB給總控；campaign＋prior8GiB，起點7,891,210,630B。PNG／舊trace只引用，source snapshot／reports／failure／logs／commands／ledger及summary自身計額；無新export，out/x5只編譯產品另計。精確最終用量／remaining及artifact SHA見[final-summary.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/final-summary.json)、[capacity-ledger.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/capacity-ledger.json)；[preservation-check.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/preservation-check.json)核原X1 130,356,292B／X2 52,804,100B／X3 13,451,038B／X4 17,562,395B、原SHA／frozen binary／export／snapshot與formal source及A–I／J1–J11歷史原文。所有新source及實體reader binding另見tool-freeze與report-tool-before-acceptance。

**X5最終容量／保護自驗：** 13,742,992／25,165,824B，開發16MiB內尚餘3,034,224B，總控可用11,422,832B（≥8MiB預留）；campaign＋prior7,904,953,622／8,589,934,592B、剩684,980,970B。out/x5 compiled products460,087,722B另計。接手保護1872次、繼承freeze及原report-tool683次核對0 mismatch，新source／snapshot／binary亦相符，formal diff空；四舊batch及campaign除X5外bytes、A–I／J1–J11原文保持。數字為檔案檢查次數，不是獨立場景。

**單一工程決策：否決NDA-v1進入下一個隔離replay，停止此版本及門檻搜尋，交付總控獨立驗收negative result。** 目前缺的是能從current pixels分辨「接近decoration後轉向」的role cue及独立標註，不以closing／同法向／overlap硬猜、不預設訓練。physical identity／role gold、遊戲採納、原perception race／早期state、source age、真recognition／RPC、77 Miss逐Note原因及跨曲效果仍Unknown；human／expert role gold增加0。没有production修正或正式候選。本chat停止：無emulator／真觸控／manual-session／live／訓練／AP goal／無限實戰，無新chat／chat訊息／goal／自動化，未commit／push。

### J13. X5總控獨立驗收：接受固定來源的負面研究結果（2026-10-02）

**驗收通過的是可重現的研究交付；NDA-v1 不通過策略採用。** 保留否決此版本、停止門檻搜尋及不進下一次策略 replay 的決定。本輪沒有 production 修改、新 contact replay 或 live。HEAD 仍 `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、main／唯一 registered worktree、50／27／11；C36h 仍主要 baseline，77 Miss 的已保存結果不變。

驗收包：[acceptance-summary.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/acceptance-20261002/acceptance-summary.json)，SHA256 `a0d2281a19dc07f39797fceae62fc531dc56f24e1754c918a5bef083cb1e93d0`。開發自驗與本次独立驗證分開記錄：

- **Release／Debug-ASan 各27/27，fail0／skip0／disabled0**，實跑最後凍結 binaries，保存 XML、command、environment、log／SHA；沒有重新 build。原 C36h／main50／X3／owner 舊套件本次未重跑，其來源與 frozen 檔案另核，不把開發自驗充作本次實跑。
- **兩份新 Release report 各3,953,654B、byte-identical**，SHA `8bf5b5a5973bd028a254a8cc6b7179720aaed6468c0d00858e0ea0f8c6d20770`。最後 reader SHA `3680e1ac2d55c689c17847bccdab9d4e1419409176e4df19e49d6a8af02aaece` 與原 compact1 工具不同；僅替換 report 的 `analysis_binary_sha256`，全文才與原交付相同，沒有冒稱原始 bytes 一致。此次實際驗了 `--acceptance` 正例 writer；new full replay=0。
- **ASan 完整 reader 另重算337幀並比對新 report 通過**；維持512MiB process commit，明列 `quarantine_size_mb=16:thread_local_quarantine_size_kb=64`。與開發時一樣有較短 freed-block 留存窗口，沒有聲稱 default-quarantine 全 reader 成功。
- **26/26 CLI 負例**（原20＋mode6）皆符合預期 reason／非零 exit、無新有效 output；existing report SHA 保持，沒有 mutex 假覆蓋。這不代表每種磁碟／記憶體上限都實測。
- **來源保護核對**：1871 個先前項（另核 status 歷史正文）、9份新source及9份snapshot、12個binary／DLL、6份shared reader、208份重用export、20個原report工具／source項、186份frozen artifact均相符。檔案檢查次數有重複，不作獨立場景數。正式src/include／根CMake無diff，原X1–X4 source／batches／工具保持。

#### 分母與結論的獨立核對

逐一與原trace對照337個role frame、全部1220次target與3588次target×line，含34欄compact shape、source_frame／PNG、target中心、line observed time、history bounds及推薦scope。C36h／main50／X4的選擇次數仍為0／12／12，abstain394／401／401；geometric eligible210／230／234、early scope0／68／67、valid secant926／952／964，均與原始來源及報表一致。這是同一recording的三段factual history，不是跨曲accuracy。

main50 D所選cohort只有6162–6164推薦315、factual winner316；6160一pose、6161兩pose仍abstain。原five-intervention中的3/5一致不能推導「晚兩幀也足以恢復Down」；首次推薦差6162不等於新策略的first causal divergence。E5286–5288的推薦與原winner274一致，使用者指定的5287正常control沒有被重標為failure。6180另外兩個local target推薦315而原winner305，仍未知role，不能用主D cohort替其背書。本次目視5287及6180原PNG、重算SHA，只有AI視覺覆核，human gold增加0。詳見 [evidence-checks.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/acceptance-20261002/evidence-checks.json)。

**反例的Verified範圍**：pure Input只含當前／過去typed geometry，兩個合成world共用相同Input但指定不同後續role；v1選水平，故不符合world B的scenario label。測試刻意保存這個語義失敗，27/27不是classifier正確率。未渲染相同RGB、未驗證兩world在遊戲中的合法性，也沒證明真實D role錯誤。它否定本規則對此scenario集合的unique-role保證；不構成所有幾何／bounded pixel方法不可行，亦不否定normal/tangent／line contribution作研究feature的價值。現階段不足以採用此規則的判定保留。

#### 驗收補正與擴充前限制

1. **C36h的confirmation不是單純少一個log欄位（Verified）。** frozen `out/x1/c36h-v3/include/pas/game.hpp` 的 `GameTrackHistory` 沒有 `confirmed_line_id`／replacement state；C36h tracking以same-recent、history span、closing／current-body當幀predicate preserve。main50才有confirmed／replacement機制。X5保守地把C36h標`confirmation_not_exported`並abstain沒有捏造state，但J12／開發交接的「缺export」不得解讀成只要多印欄位即可。做C36h有效比較前需定義lineage-specific early-scope契約；不能直接填0、借main50後續state或把當幀preserve flag等同pre-selection confirmation。
2. **stale-prior secant為工具重用限制（source review）。** `x5_adapter.hpp::feature_json`把previous line的observed設成`a.ns`，沒有保留原observed或檢查`old.current`；若未來接入帶stale prior的bank，latest secant可能被錯標valid。pure rule的paired-history仍檢查current，因此不能直接稱會錯選。此次核三history共863次raw bank line全部observed==capture，觸發分母0，固定來源報表未受影響；擴充輸入前須修正時間保真並補stale-prior adapter反例。本輪未改frozen source／binary以掩蓋限制。

上述語義補正已記入X5交接§9；**接受的是固定manifest下的negative result與有界重算，不授予此adapter任意新trace的泛用驗收。** 舊報表、spec中`identical-pixels`／`legal future`及`confirmation_not_exported`原字樣保留作歷史，必須按上述限定閱讀。

**容量**：本次新增 **8,162,616B**，X5合計 **21,905,608／25,165,824B**、剩 **3,260,216B**；campaign＋prior **7,913,116,238／8,589,934,592B**、剩 **676,818,354B**。新report、scripts、XML/logs、原文snapshot、核對與summary自身皆計額。原X1=130,356,292B、X2=52,804,100B、X3=13,451,038B、X4=17,562,395B保持；new build/export=0，原out/x5 compiled products460,087,722B另計，沒有刪除或覆寫失敗證據。

**下一步先補證據／契約，不調NDA-v2或直接live。** 先把C36h實際early association與preserve predicate映射成可審查的離線scope，並處理上列stale-prior診斷保真；再以既有pixels挑少量真實late-turn／crossing／neighbor正負例、對齊逐物件trace，區分當前可見cue、僅由後續可知的role與unknown。沒有證據時不假設必定存在可即時辨識role的視覺cue，也不先訓練模型。取得可證偽的新訊號或有意義的延後決策／abstain方案後，才決定是否值得單機制variant。物理identity／judgment-role gold、真時序、觸控改變後feedback、77 Miss逐Note原因及跨曲效果仍Unknown。沒有可freeze的正式候選，目前無需emulator；有限live要在候選與冷回歸具備後另行授權。本輪未建立下一task、goal、自動化或訊息其他chat，未commit／push。

### J14. 總控直接補正 X5 的 scope／時間保真與真實反例（2026-10-02）

依使用者「直接自己補」完成，詳見 [X5_SCOPE_TIME_REPAIR_20261002.md](../04-offline-research/X5_SCOPE_TIME_REPAIR_20261002.md)。**這是總控實作與驗證的新離線補正，不把它冒稱另經獨立agent驗收；NDA-v1仍否決，正式策略不變。** HEAD仍`f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、main／唯一worktree、50／27／11；既有dirty交付保留。舊source snapshot、binary、report均保持，新build僅`out/x5-repair`，沒有emulator、真觸控、manual-session、模型訓練、新contact replay、goal、chat派送、commit或push。

1. **Verified：stale-prior診斷已修。** `x5_adapter.hpp::feature_json`先驗previous pose的`current`，只有原observed==該sample time時才建measured pair；否則明確unknown，不補造timestamp／velocity。raw三幀測試包含prior stale1ns／5ms、current stale、prior invalid與fresh但motion-model-invalid正例。pure NDA-v1不改。Release／Debug-ASan各34/34（原27＋新增7）、fail／skip／disabled0；兩種新reader重算原337幀並與驗收report全文相同，僅排除analysis binary SHA。原863次bank line都是fresh，固定來源結果不受影響。
2. **Verified：C36h research early-scope有實作契約。** 新`x5_scope.hpp`只在strong current、unique runtime assignment、非Hold、pre-selection prior relation samples<3且preserve=false時列入保守early comparison，因C36h的preserve必要條件尚不成立。≥3但nonpreserved仍mature/unknown，不當main50 unconfirmed；缺欄位／矛盾不補0。main50沿explicit confirmed state分層。這些是relation samples（有bucket／reset），不是raw frames、physical年齡或Down資格；不把scope餵回NDA-v1。完整分母C36h394→85 early／214 current-preserve／34 mature-nonpreserved／57Hold／4identity；main50與X4仍413→68／67 early。原X5選擇0／12／12不變。
3. **Verified量測＋proposed視覺：有界真實反例包。** 新reader只讀固定已驗report，輸出337個role frame／1220 target的scope、27張PNG對應81份三history packet，逐檔核SHA；兩份Release附錄byte-identical，ASan除binary SHA亦相同。另從同一有界input搜尋三pose位移轉角，35次proposal全保留，不作35個physical turn。實際目視6173–75、6180、6188–91共8張原圖，human gold增加0。E5287仍正常control。
4. **Strong inference：6173–75值得先查觀測中心可靠性。** yellow Drag與紅Flick交疊時，C36h local1622主軸長152.007→124.015→138.101px、center y287→275.447→282.984，產生約60°位移轉角提案；原圖較支持遮擋／形狀裁切造成量測偏移，不能標成physical late-turn。6188–91另有Tap／Flick與線的真實姿態變化，可作不同機制control。6180相鄰cores的role仍proposed，不能借主D cohort的X4結果作gold。

新scope CLI的wrong-argc／wrong-parent在Release與ASan各驗2例，合計4/4；未宣稱重跑原X5 CLI26例或舊runtime套件。ASan完整reader沿用512MiB commit與明列小quarantine；預設quarantine的全reader成功仍不在聲明範圍。具體commands／XML、source／binary SHA、原檔保護及精確容量見 [repair-summary.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5-repair/repair-summary.json)。新batch硬限8MiB、campaign＋prior≤8GiB，所有新報告／log／失敗／snapshot／帳本計額；舊X5 21,905,608B與原X1–X4保持，build products另計。

**下一個小工程實驗：離線中心量測可靠性消融。** 用6173–75 overlap對6188–91姿態變化、6180分離cores與5287正常窗，量current core span／coverage／端點、component split與位移在主軸／法向的分量；先檢驗「coverage收縮伴隨主軸center漂移」能否區分觀測偏移與真幾何變化。只能輸出measurement reliable／unknown，不能補支持、改owner或放寬lease。若control也被頻繁抑制，或feature無區分能力，即拒絕，不調NDA-v2救結果。取得可證偽訊號後才考慮bounded C++ gate及單機制replay；本輪尚未實作此新實驗。

**Unknown**：real late-turn的人工judgment-role gold、current pixels是否足夠唯一識別role、physical continuity、old touch feedback、真時序、77 Miss逐Note原因與跨曲效果。沒有可freeze的正式候選，仍不需要emulator；不把補正完成寫成Miss改善。

### J15. 更後續執行計畫、X6 完成與 X9 分支（2026-10-03）

**本輪最新授權優先：** 使用者要求規劃更遠後續並直接開發，允許 Computer／emulator 在需要實機測試時使用，盡量01:00前完成；未完交新gpt-6.1-sol／xhigh chat。已建立一次01:00 Asia/Taipei heartbeat（ID `phigros`），不輪詢時間。這解除先前研究輪「不啟動live」限制，但不是恢復無限實戰／訓練／AP goal。完整工作包、依賴／分支、停止線與資料上限見 [C36H_FORWARD_EXECUTION_PLAN_20261003.md](C36H_FORWARD_EXECUTION_PLAN_20261003.md)：X6中心可靠性→X7承諾可觀測性／X8單機制variant；X9獨立Hold定位；X10 donor interaction；X11成本及freeze；X12最多6輪有限實機；X13跨曲保留驗收。未實作項明列planned。

**X6由總控直接實作、自驗完成，未稱另經獨立agent驗收。** [研究交付](../04-offline-research/PIXEL_CENTER_RELIABILITY_X6_20261003.md)及[封存summary](../../../measurements/game-assist/2026-09-30-m0-manual-continue/pixel-reliability-x6/final-summary.json)。新增offline C++色核心span／coverage／缺口／center offset量測，337 role frames、1220 target occurrences、114 unique PNG逐SHA；Hold排除171次，實量1049次。Release／ASan各13/13，最後Release兩report byte-identical，ASan僅binary SHA不同；最後CLI wrong-argc／parent各兩例通過。新batch7,499,185B；新out/x6 372,938,945B另列。正式src/include／根CMake未改；舊X5修補source、引用export及父report仍相符，未重build frozen binary。

**Verified：** 6173→6174 C36h local1622 observer長度−27.992px、current色span僅−4px，extent midpoint offset由0.5→10.5px；**Strong inference：** observation-shape bias會混淆X5的轉向proposal。**否決：** 預先異色缺口v1在6174只有1個foreign gap bin而未觸發，且相同像素cue可來自兩個獨立同色物件；不能給center可靠性／identity／role保證。保留量測作研究工具，不調threshold、不補座標、不接runtime。初build macro分行錯誤與早期pair canonical軸符號差已修，失敗及舊報表保留，最後pair須讀Report-release-3/4。

**下一分支X9已啟動：** 用原2479可見Hold／identity競爭，新增2460–2495有界trace窗並保留A3498／B4986／C5520／E5287，從原32preroll跑完整recording。重用frozen X1修補binaries，先比全程semantic digest／events與原freeze是否相同；最多兩個新replay，batch≤128MiB、campaign+prior≤8GiB。若C36h已保持contact，便否決此例作當前failure；不因原C36g cancel而修C36h。X7／X8沒有足夠新role cue暫不啟動。此節記錄開發啟動狀態，X9結果另續，不冒稱已驗收。

### J16. X9 Hold causal window完成：額外Down，不是原contact失去支持（2026-10-03）

詳[HOLD_CAUSAL_X9_20261003.md](../04-offline-research/HOLD_CAUSAL_X9_20261003.md)。總控新增C36h／main50各一個完整prefix replay（2 total），各7722 PNG／7715 perception／82選窗frames、contacts_at_exit0。只換診斷窗口、零new build，每role全semantic digest及events bytes與原X1 frozen run相同；既有controls保持。原2479 cancellation確在C36h重現，但不能解讀為原Hold掉線。

**Verified：** C36h local521於2469 Down且持續；539於2474從內部color component出生，2477–78借近期rails重建front後得到root，2478在原contact旁約2.5px另開contact0 Down，2479因identity_ambiguous取消；521/contact1同幀仍Move。main50對應541在2478／79保持root_past、沒有額外Down，原523contact續接。**Strong inference：** AI看2474／78／79原圖較支持同一可見body的fragment，human gold0。無新逐Note Miss判定。

**Hypothesis與下一步：** main50的held-first recovery排序與fallback current-rail claim可能解釋此差異；先拆fallback claim單一機制帶入C36h隔離消融，不整包升格main50。必驗同column但normal完全分離的兩個Hold、獨立新前緣／Tap、不同線與角度／缺rails／unknown及completed守門。原donor只比width及切向距離，可能錯壓真新Hold，不能因少一個Down就採用。X7暫不調NDA-v2；X10有具體可測donor後才開始。正式production仍未改，未開emulator、未訓練、未commit/push。

### J17. X10 donor消融否決採用；X10b current-pixel witness驗證中（2026-10-03）

[X10完整記錄](../04-offline-research/HOLD_RAIL_CLAIM_X10_20261003.md)：只把main50 fallback claim移到C36h，不改排序／association／owner。1次新full replay（7722 PNG／7715 perception／136 traceframes）使2478額外Down消失，原521contact保持，A/B/E/D all-target（去note ID/revision）及actions（去intent/contact ID）相同。全prefix兩版各2165 receipts/releases，首action差在2447，晚2.267459ms及座標微差；不能以H局部少2個action冒稱全體改善。全state首差仍unknown。

原207項distinct tests已全部執行、0fail（先206pass/1skip，再補被skip的27RGB case）；新typed donor contract 3pass/2fail，分離normal extent與claimed方向不同皆會被原predicate壓掉。後者最後反例已使incoming與selected line方向相容，避免將前置必拒的case當成可達路徑。仍未證明完整RGB extractor在兩個negative會進相同call，故精確稱typed contract反例，不能冒稱已發生兩個gameplay regression。原donor拒採、失敗原樣保存，source/binary封存；batch6,897,075B，out/export160,904,698B。沒有對已否決donor跑ASan或trace-off，也不授予正式candidate資格。

**X10b另立可證偽的pixel hypothesis，正在驗證。** 僅抑制history-derived approaching front的重建：它須在同向current held body的normal範圍內（±8px probe全在body中），且兩邊當前藍色側帶穿過擬前緣。原width/along閾值不調；加入的是body extent、方向與current pixels，而不是把20改成另一數字。direct front、active held observation、缺body／缺一側／stale line／分離body均不抑制，最多128 claims、每candidate最多60 RGB probes，不合併identity或改lease/owner。Release原207/207與13個新測試皆通過；兩個full replay／ASan尚待完成核對。輸入H擴2440–2495，避免漏掉X10更早action差。此項未完成，不稱正式fix／Miss改善，待驗項見[交接頁](C36H_0100_HANDOFF_20261003.md)。

### J18. X10b冷驗證完成、2883新差異阻止正式採用（2026-10-03）

詳[CURRENT_INTERIOR_CLAIM_X10B_20261003.md](../04-offline-research/CURRENT_INTERIOR_CLAIM_X10B_20261003.md)。Release與Debug-ASan各原207/207、新13/13，均0fail/skip/disabled；新baseline／variant-ON／variant-OFF共3個full replay，各7722PNG／7715perception／156選窗frames、contacts0。ON/OFF完整semantic及events bytes相同。原C36h baseline仍frozen，live0新增、正式策略不變。

H2478額外Down／2479相應Up確實移除，原521 contact保持。2440–2459的20幀target皆相同，first target差2477（所選union內）、全action首差2478，原donor在2447的差已消失；A/B/E/D normalized target/action保持。**但全actions 2165→2163不是僅刪那兩個actions**：C++全序列audit於2883出現第一個非deletion差，variant local684提早Down，2884又以current_object_missing_or_region_lost取消；baseline則2881 pending root_past、2885才Down。其physical role／像素因果尚未核，不報Miss好壞。

**明確停止線：X10b不採用為正式candidate，不進live；下一task先獨立驗收並定位2881–2885。** 不能以13項新test與H正例蓋過full-prefix差異。原batch3個replay額度已用完，source／binary／raw輸出封存；新問題另立預先spec及capacity。需要補per-object trace／pixel支持、全state首差仍unknown。使用者既有有限live授權保持，但只有完成這些冷gate與X11 freeze才啟用。

### J19. 接手者獨立驗收與X10c完成：否決standalone suppression family（2026-10-03）

詳[X10c結果與交接](../04-offline-research/HOLD_CASCADE_X10C_RESULT_20261003.md)。父chat已停止操作，本接手者自行讀source、四份原XML、三run與provenance，重hash全部X10b frozen source/export/binaries；工程證據相符。這次確有獨立核對，不把父自驗照抄為通過；**工程驗收不等於策略採用，family否決採用。**

新protocol先保存，七窗H/K/A/B/C/D/E共196幀；兩份isolated exports inverse patch還原各自parent，69檔不變、三檔只有單向diagnostic/const getter。Release兩版原207/207，新diagnostic/window tests Release與ASan各9/9；Release build2/ASan build1的GTest同列macro工程失敗保留，移行修復，不改期待。新full replay恰4次且0failure，兩版ON/OFF各7722 PNG／7715 perception／32preroll／contacts0；每版全events/semantic與frozen parent相同，ON/OFF每幀digest也相同。正式src/include/root CMake不變、live0、無emulator／訓練／commit/push。

**Verified全分母：** exact bounded LCS在2165 baseline／2163 variant actions中匹配2161；unmatched baseline4／variant2，未稽核0。差異只有2478 Down/2479 Up刪除，以及baseline2885 Down/2887 Up對variant2883 Down/2884 Up，2887後至EOF也已核對。Normalized actions排除local intent/contact ID，不宣稱physical等價。A/B/C/D/E全部selected targets去ID/revision後及normalized actions保持。原remaining1453已覆蓋；不再把總少2稱改善。

**Verified首boundary：** 明列的逐幀semantic/scene/bank/note-history首差2477，owner/contacts首差2478；不是所有private state的首差保證。K684於2865–2880的target/history相同，2881抑制前witness除applied外全相同。2881 X10b skip使684從snapshot缺席；baseline則有root_past，cancel pending。C36h舊generic Hold missing60ms grace不分pending/active，variant intent197保留，於50310736400ns在2883 capture前幀間Down（source_frame14647），2884 grace到期Up/cancel。原682仍Move。未改owner的獨立FakeClock probe也重現missing可Down、explicit root_past立即取消的差別。這是suppression把矛盾變成absence的composition反例，不是unknown Down重試或scheduler期限失效。

**Strong inference／Unknown：** 原2865同欄兩個分離body、2881 incoming front仍在y533而claim682延至576，顯示current held claim的physical歸屬可能借用了下一body；局部藍側帶連續不證明同一已held Note。AI目視及links只是proposed，human gold0；兩版遊戲好壞／Miss與cross-song效果未知。全prefix raw state digest有大量ID/retirement差，僅196窗有normalized target細節，不冒稱所有其他觀測語義相同。

**停止與较遠plan：** standalone X10b family否決，不調threshold、不用舊grace/alias放寬救它。本輪四次replay已用完且已有可重現negative，達科學停止線。下一獨立X10d只列current body ownership／明示suppression與pending-vs-active反例契約；X11成本/freeze、X12至多6輪已授權live、X13跨曲仍conditional未啟動。沒有可用candidate或必須新采集的缺口，所以不啟動emulator。新batch≤128MiB、out≤768MiB、campaign+prior≤8GiB；精確bytes/source/binary/commands/XML及失敗保留於[final-summary](../../../measurements/game-assist/2026-09-30-m0-manual-continue/hold-cascade-x10c/final-summary.json)。

### J20. 總控複核X10c及下一獨立問題（2026-10-03）

**Verified：** 總控重新讀X10c protocol/result、audit/causal/probe程式、C36h generic missing與root矛盾分支、main50對應owner及tests；核對219個pre-replay source/export/binary與final snapshot/analysis binding、四份XML及四run共12個summary/events/state-digest檔案SHA。實際重新執行frozen `x10c_audit`、`x10c_causal_report`與`x10c_pending_probe`，結果與已封存reports相同。這次沒有再執行完整replay、207項回歸或ASan，不將XML核對稱重跑；未修改frozen batch/export。main HEAD仍`f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、唯一worktree、既有dirty保留，正式src/include/root CMake無差異。

**判定：簽收X10c工程及negative result；維持X10b standalone suppression否決。** 2165/2163 actions中2161個normalized signature相同、4/2不同，audit覆蓋至EOF；physical identity/contact配置等價仍不由此推出。2881抑制前局部witness一致，skip將root_past矛盾變成absence，C36h pending grace保留舊plan；due在2883 capture之前發生，不能靠2883辨識後再取消作解答。FakeClock probe直接支持此owner組合行為；claim誤納下一個body仍是proposed視覺推論，非human gold。

**下一步調整，尚未實作：** main50 `src/game.cpp` 已有cursor==0且新完整snapshot缺目標時立即`pending_down_current_object_missing`；`tests/game_tests.cpp`已有`SingleMissingDragFrameCancelsFutureDownAndFreshReturnCanReplan`及fresh-return guard。因此X10d-P應先對未改C36h、suppression OFF的baseline做這一個owner機制的選擇性移植／消融，分pending/active/unknown/completed及due前後／同時，完整報action差異及lost opportunity。C36h舊pending容忍是既有契約，刻意變更須另列candidate期待，不覆寫baseline test掩蓋差異。X10d-O另建兩Hold／tail與incoming／rotation／neighbor／absence的ownership反例；pending修正通過不構成X10b suppression可採用的證明。先分開驗證，再決定有無必要做交互實驗，詳[前進計畫](C36H_FORWARD_EXECUTION_PLAN_20261003.md)。

X10c封存帳本：batch57,648,709B、out345,857,819B、campaign+prior8,021,936,506B（8GiB上限餘568,998,086B）。本次只更新交接文件，沒有新的測量檔／full replay／live／模型訓練。X11–X13仍conditional；77 Miss改善、實機採納、跨曲效果皆Unknown。

### J21. X10d-P pending missing單機制完成自驗，待總控獨立驗收（2026-10-03）

詳[X10d-P結果／交接](../04-offline-research/PENDING_CANCEL_X10D_P_RESULT_20261003.md)及[預先protocol](../04-offline-research/PENDING_CANCEL_X10D_P_PROTOCOL_20261003.md)。新兩版export均源自frozen X10c baseline／C36h-v3，**suppression OFF**；reference72檔全相同，candidate只在game.cpp新增main50既有`submitted && executed_steps(intent)==0`時立即`pending_down_current_object_missing`的8行hook，inverse patch還原parent。沒有移植owner整包／body ownership、調grace/lease/alias/observer/association。正式src/include/root CMake未改，HEAD/main仍f83c7ea、唯一worktree，既有dirty／frozen evidence保留。

**Verified自驗：** 新完整owner/scheduler/FakeClock/Touch十三項先連未改baseline，8pass/5預期red；candidate Release及Debug-ASan均13/13。reference原207/207，candidate兩build均206pass/1刻意契約fail，原`SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt`兩個pending_count=1期待原樣保存，未過濾／改期待；沒有其他原regression，全部errors/skip/disabled0。四Note種類、fresh-return、unknown/completed/缺cursor、prefix_offset、due前後／同時／capture-ready交錯、gate/reset/context/epoch、active Hold旋轉/body/patch guard及alias/shared Drag共存均覆蓋。ASan插樁candidate core/tests，非ASan真录replay。第一ASan DLL copy用了不存在debug z.dll，於tests前失敗；核對zd.dll後修復，記錄保留。

**Verified完整分母：** 三次新full replay／0failure（reference ON、candidate ON/OFF），各7722 PNG逐SHA、7715 perception、32preroll、196窗幀、contacts0／無truncation。reference全events/semantic/7722行state-digests bridge回X10c frozen baseline，reference OFF重用已核相同的X10c frozen control；candidate ON/OFF三者全相同。source/binary在首run前凍結，之後沒有rebuild core/replay；新增單向fullprefix lifecycle≤32MiB/run，source/analysis/binary/commands/容量另freeze。無emulator/live/goal/模型訓練/額外chat/commit/push。

全observer scene/bank/note-history digest差0/7722，首owner/semantic差1976、首action/contacts差1978：local355 Flick pending缺席取消，reference仍於幀間due執行；不是靠未來pixels追溯禁止。exact bounded LCS全EOF：2165→2131 actions、2121matched、44/10unmatched、未稽核0。28次取消＝Tap5/Hold7/Drag14/Flick2，均known absolute cursor0／無prior receipt；16次返回、13次新intent、12次Down、28次退休。**14組baseline有Down而candidate無Down的lost opportunity**全部保存，不能稱ghost清除或Miss改善。全部差異由14組未開始contact、5組fresh-return重建及一個同signature Move因新intent改變既有等時map排序解釋；忽略順序bag43/9，不覆寫原44/10 audit。

H/K/B/C/D/E normalized ordered actions保持；A22→21，local947 Drag於3494缺席取消、3495重建及3496等時Move次序改變，原持續Hold保持。H2478額外Down仍在；K2881原root_past取消及2885Down維持，不將pending修正當X10b suppression復活。所有新差異已由全prefix現有有界資料定位，不使用第4次replay。

**判定：開發及冷候選契約自驗完成，待總控獨立驗收；未正式採用。** Verified範圍是單一程式生命週期及完整固定pixels反事實資料；19組action差異的機制鏈為Strong inference，physical identity/body歸屬、真閉環採納、14組lost opportunity的判定、77 Miss改善及跨曲泛化仍Unknown，human gold增加0。C36h tint1仍behavioral/experimental baseline、main50仍donor/control。X10b standalone suppression仍否決，X10d-O/X11/X12/live未啟動。精確新batch≤128MiB、out≤768MiB、campaign+prior≤8GiB及全部SHA/失敗/commands見[final-summary](../../../measurements/game-assist/2026-09-30-m0-manual-continue/pending-cancel-x10d-p/final-summary.json)。

### J22. X10d-P總控獨立驗收通過：冷契約候選，未驗gameplay（2026-10-03）

詳[總控驗收](../04-offline-research/PENDING_CANCEL_X10D_P_CONTROLLER_ACCEPTANCE_20261003.md)。總控重新讀唯一hook、owner caller／cancel／cursor／scheduler排序、新tests及三個audit；重hash226個source/export/binary/snapshot綁定及207個artifact-ledger項目。inverse patch還原parent，reference全檔不變，candidate唯game.cpp新增8行、與main50既有分支相同；X10b suppression OFF。新驗收資料獨立存`pending-cancel-x10d-p-controller`，原batch/out/frozen evidence不改。

**實際重跑六套binary：** reference原207/207，新契約8pass/5預期red；candidate Release及ASan新契約各13/13、原suite各206pass/1刻意契約fail。唯一fail仍是舊SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt的兩個pending_count期待；未改期待／過濾，其他fail/error/skip/disabled0。重新執行full action audit／lifecycle audit／causal review，完整解析JSON與封存相同；這次無新build／full replay／live，不將hash核對稱重跑7722PNG。

**Verified：** 新完整snapshot已accept、目標缺席、known absolute cursor0才撤銷；fresh-return依新current evidence建intent。active／unknown／completed／缺cursor／非零prefix的原守門保持。全observer scene/bank/history相同，首owner差1976／首action差1978；2165→2131 actions，2121matches、44/10unmatched、未稽核0。28次pending撤銷、14組reference有Down而candidate無Down、5組fresh-return改plan；A內一個等時Move排序差與既有map key一致。Strong inference是19組action差異的機制解釋，非physical gold。**14組機會損失不是14個ghost或Miss，遊戲利弊仍Unknown。**

**簽收範圍：工程與冷候選生命週期契約通過，可進X11-P成本／完整runtime版本凍結準備；尚未正式採用。** X10d-O body ownership獨立保留，非pending-only候選的X11前置；不可藉此恢復X10b suppression。X11須另核實際latency／jitter／failure/capacity及編譯來源，舊replay成本scope含instrumentation且不包含所有digest/lifecycle，不足效能驗收。X11通過後，X12有限emulator比較才可判定遊戲採納、漏觀測期撤銷的取捨及Miss效果，沿用已授權最多6輪，不以更多相同offline replay替代真閉環結果。本輪未啟動X11／新chat／emulator。

main HEAD仍f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c、唯一worktree、原dirty保留、正式src/include/root CMake未改。總控驗收commands/XML/完整reports及容量見新驗收根final-summary；原開發文件的待驗收為封存時狀態，最新以本節為準。

### J23. X11-P 完整runtime／成本交付：not-ready，待總控獨立驗收（2026-10-03）

詳[結果](../05-runtime/RUNTIME_X11_P_RESULT_20261003.md)、[預先protocol](../05-runtime/RUNTIME_X11_P_PROTOCOL_20261003.md)、[交接／X12預案](../05-runtime/RUNTIME_X11_P_HANDOFF_20261003.md)。已從saved C36h commit＋tint1 snapshots建立隔離完整B0/B1 pas.exe及遞迴DLL closure，runtime metadata離線可核parent/variant/actual SHA，無LibTorch。B1唯一decision semantic差異仍main50已驗8行pending missing hook；inverse恢復B0，suppression OFF。原live binary、新B0、新B1分別SHA61ec…／81cb…／0b93…，不以37/19混同。原live freeze未列compiled inputs的SHA缺口明列，新build完整來源、1490 CL/link/compiler/proto dependencies及profile另freeze。正式src/include/root CMake、舊frozen exports/binaries/raw保留，沒有commit/push。

**Verified工程範圍：** 新B0原238 distinct全pass（首次historical path skip補只讀引用後單項pass）；B1原237pass/1預先聲明pending-grace契約fail、0skip。新pending contracts B0 8pass/5預期red、B1 13/13；兩版512 synthetic/FakeClock public-output bridge各自與frozen研究核心events bytes相同。新uninstrumented candidate core＋新concurrent harness ASan stress未報memory error，所有28run fakefailed/unknown/contacts-at-exit0。未新跑full-prefix PNG，原X10d-P7722幀/14lost opportunity不能改記為新runtime實測。

**成本 gate failed，freeze not-ready：** 按protocol先8 A/A再16 ABBA（每run512），加每版2stress（各256；B1 owner第二筆為ASan、不比Release）。A/A RGB recognition p99及dense owner p99噪聲超預先門檻；密集owner兩批B1 p99又比B0增加.46655/.45685ms，越已凍結T=.298ms。baseline一筆lateness p99=15.0906ms也越15ms。全部13312publish、12709consumed、12420owner分母及skip/drop/reject/cancel/late/raw保留，沒有放寬、重跑挑樣本或用少Down宣稱改善。128pending/5contacts有界；正式SessionArchive序列化與高解析Wake尚未等價量測，真RPC/render age/device/遊戲效果未知。

**完成狀態：工程與negative成本交付待總控獨立驗收；不進X12。** X12至多6輪的exact paths/profile/停止條件只準備，另指出C36h journal原硬512MiB/round與forward plan256MiB/round尚缺可驗guard。X10d-O未啟動、X10b仍否決、C36h tint1仍baseline；14lost機會利弊／77Miss／跨曲未知。新batch≤128MiB／out≤3GiB／campaign+prior≤8GiB，精確bytes與SHA見runtime-x11-p/final-summary；此task停止，不自行續live或開新goal/chat。

### J24. X11-P總控簽收負結果：維持not-ready（2026-10-03）

詳[獨立驗收與下一工程包](../05-runtime/RUNTIME_X11_P_CONTROLLER_ACCEPTANCE_20261003.md)。重hash274 source/binary、1490 compiled dependencies、1923 artifact條目，55 src/include inverse符合。實際重跑四suite：B0原238/238，B1原237pass/1既定pending-grace fail，新契約B0 8pass/5預期red、B1 13/13，全部skip/error/disabled0。兩版各512 synthetic/FakeClock bridge與freeze events bytes相同，兩runtime provenance逐SHA核對。由28份既有summary重算noise/evaluate，各exit2、JSON bytes與freeze相同；無新cost/stress/ASan/full PNG replay/live。

**簽收工程、冷契約與負成本證據，不授予live-ready。** A/A兩項noise failed、owner p99兩批越T、B0 lateness一筆越hard gate維持。AA不穩且compact writer/1ms polling/lead0不同於正式archive/high-resolution Wake/live lead35，不能宣稱已確認正式性能退步。原live未列compiled SHA仍Unknown，三個37/19 binary不得混同。

**下一優先X11-P-R1，尚未派送：** 補正式Wake/SessionArchive共用路徑的離線meter、逐release分母、testcase skip解析與空metric拒絕；先解256/512MiB journal容量契約，再另立有界成本protocol，A/A不足即停、不跑ABBA救結果。pending單hook、suppression OFF、X10d-O獨立、不動正式策略。14lost opportunity／77Miss及遊戲採納仍Unknown，X12≤6次閉環仍conditional。

總控root runtime-x11-p-controller保存全部輸出；初估16MiB因bridge約29.7MB而失敗，明記capacity-failure、不冒稱通過，另以32MiB retention封存。final-summary前batch30,096,176B／campaign+prior8,217,562,555B，summary預留16KiB，8GiB總上限維持。HEAD/main f83c7ea、唯一worktree、既有dirty保留、正式src/include/root CMake未改；原交付文件「待驗收」為歷史狀態，以本節為準。

### J25. X11-P-R1 交付完成，待總控獨立驗收（2026-10-03）

詳[R1結果](../05-runtime/RUNTIME_X11_P_R1_RESULT_20261003.md)/[protocol](../05-runtime/RUNTIME_X11_P_R1_PROTOCOL_20261003.md)/[handoff](../05-runtime/RUNTIME_X11_P_R1_HANDOFF_20261003.md)。新隔離runtime共用原high-resolution Wake、完整SessionArchive及JSON events，options匹配lead35；57 src/include inverse仍僅pending8行。305 source/binary/profile與1580 compiled dependencies freeze，舊frozen與main正式source/dirty保留，無emulator/ADB/manual-session/真觸控/模型/commit/push/新chat/goal。原live未知compiled SHA不回補。

原suite B0 238/238、B1 237pass/1已聲明fail；pending B0 8pass/5預期red、B1 13/13；新Release兩版10/10、ASan新10/10＋pending13/13，skip/error/disabled0。testcase skip用舊歷史XML回歸，空metric/missing row/archive故障/容量/收尾/failed及unknown release不靜默pass。兩版512-input bridge每byte/EOF相同、無30MB複製。每次release含空集合均保存分母，另fake負例保存12calls/2receipts，真裝置驗證仍Unknown。

**not-ready，ABBA=0：** 4個functional stress（含B1 owner ASan）後8A/A；RGB recognition p95/capture→owner p99、owner accept p99/capture→owner p95共四noise fail，aa-owner-1 owner114/128=89.0625%未達90%。全1792publish/1684consumed/1574owner、1000全phase receipts/99release calls/10294完整events、skip/drop/fault/raw保留；ASan stress-owner receipts0是timing Unknown，不冒稱通過。noise凍結後沒有新cost/stress、無門檻修訂或樣本救援。

X12容量準備採原native32×16MiB=512MiB/round可測硬限，pending-only≤6輪計畫明確修為512MiB/round，journal/result/metadata/standby共3,277,848,576B，future live根12GiB/reserve5GiB。這只解容量矛盾，成本資格仍阻擋X12。完整exact SHA/容量見R1 final-summary；交付候選待總控獨立验收，不能自簽。C36h tint1仍baseline，main50 donor，X10d-O獨立/X10b否決；14lost利弊/77Miss/跨曲效果仍Unknown。

### J26. X11-P-R1總控驗收：簽收工程與負結果，仍not-ready（2026-10-03）

詳[總控獨立驗收](../05-runtime/RUNTIME_X11_P_R1_CONTROLLER_ACCEPTANCE_20261003.md)。逐SHA/bytes核305 source/binary、1580 compiled dependencies、376 negative工具inputs、2059 artifacts，57 src/include inverse成立。實際重跑八套suite：原B0 238/238、B1 237/1既定fail；pending B0 8/5預期red、B1 Release/ASan各13/13；R1 B0/B1 Release及B1 ASan各10/10，skip/error/disabled0。兩版512-input streaming bridge及release negatives bytes符合，provenance逐hash核對，舊skip XML collector正確。八份summary重算noise完全相同；獨立C++核12run原raw之11組metric分布、分母及archive receipt/release內容符合，writer/enqueue無raw樣本的分布不冒稱重算。

**not-ready維持：** 四A/A noise fail及owner114/128 coverage fail，ABBA0；B1 ASan stress無receipt仍是active dispatch覆蓋Unknown。未追加cost/stress/full PNG/live，正式策略不變。下一優先X11-P-R2量測有效性設計，先分析既有raw的樣本/啟動/負載及補並行active覆蓋契約，有方法改進才另立有界A/A，不追安靜樣本、不調pending規則；尚未派送。

**修正容量聲稱：** Windows text-mode CRLF令32×16MiB是logical limit，非精確physical hard cap。總控用frozen B1 core作32×256B logical的deterministic反例，實體8224B>8192B且無fault；不是cost/stress run。forward plan已改journal每attempt保守1GiB physical reserve（512MiB logical最多2倍）、standby8MiB/metadata2MiB/result4MiB，6attempt共6,530,531,328B，仍在12GiB future根/另5GiB reserve內。原R1 freeze及歷史3,277,848,576B估算不改，最新以本節/驗收頁為準。容量規劃修正不授予live-ready。

HEAD/main仍f83c7ea、唯一worktree、dirty保留、正式src/include/root CMake未改。總控batch runtime-x11-p-r1-controller≤16MiB、獨立audit build≤64MiB、campaign+prior≤8GiB，精確SHA/bytes見其final-summary。C36h tint1 baseline、main50 donor、X10b否決、X10d-O另案；14lost機會/77Miss/真採納與跨曲仍Unknown。

### J27. X11-P-R2 交付完成，待總控獨立驗收；not-ready（2026-10-03）

詳[R2結果](../05-runtime/RUNTIME_X11_P_R2_RESULT_20261003.md)、[預先protocol](../05-runtime/RUNTIME_X11_P_R2_PROTOCOL_20261003.md)、[既有raw分析](../05-runtime/RUNTIME_X11_P_R2_RAW_ANALYSIS_20261003.md)及[交接](../05-runtime/RUNTIME_X11_P_R2_HANDOFF_20261003.md)。直接連未改R1 B0/B1 Release與B1 ASan core，不新build runtime pas.exe、不改正式src/include/root CMake，pending8行仍唯一decision delta、suppression OFF。57inverse、原305/1580/376/2059綁定、新38source/binary/method與562實際dependency inputs另核；HEAD/main f83c7ea、唯一worktree、原dirty保留。

新C++重算原12run之11組raw分布、order ranks／固定capture窗／全targets與JSON bytes／skip／tail／receipt-release及archive EOF；writer/enqueue僅保留原aggregate，不冒稱重算。normal owner p99取第二大、RGB取第三大，noise-sensitive尾值並非全在startup。原ASan四份128-target snapshot在owner_start已超100ms freshness、唯一fresh份為body-only；source/lease支持接入缺口的解釋，OS/JSON因果仍Unknown。R1四noise及89.0625%owner coverage fail不回改。

方法要求每principal measurement n≥2000、256warmup＋2304measurement，保留全warmup/fail/late/full diagnostics；AA8筆物理檔長線性投影228,019,240B越新根128MiB，尚无已驗bounded壓縮collector，成本admission拒絕。**新AA0、ABBA0；not-ready維持**，不縮樣本、放寬gate或追加同replay。

fixed-QPC head0–800ms／body800–1800ms／gate-off1800ms一target fixture，新B0/B1 Release及B1 ASan FakeClock/負coverage各7/7；原suite238/238與237/1既定fail、pending8/5與13/13、R1三套10/10保持，11套XML skip/error/disabled0。新meter兩版512-input streaming bridge bytes/EOF一致。三筆預定OS Wake concurrency（B0→B1→ASan）各256attempt，各1Down/24Move/0Up、32/33/33 body active witnesses、same contact、各requested1release、exit contacts0，ASan無報告。全部768publish／206owned／562skip、75receipts／40release calls／708full events均有raw核對；不以one-target或ASan數值當normal性能／128-target聯合覆蓋通過。

精確SHA、commands/XML、source snapshot與最終capacity見runtime-x11-p-r2/final-summary.json及ledger；batch≤128MiB/out≤3GiB/campaign+prior≤8GiB，另保留總控16MiB與disk free5GiB。X12保守reserve6,530,531,328B最新更正不變。下一決策由總控獨立驗收工程／負admission，再決定能保留完整≥2000樣本的collector容量；本task停止、不自簽總控、不進X12，不建chat/goal／傳訊／emulator/ADB/live/模型/commit/push。C36h tint1仍baseline、main50 donor、X10b否決、X10d-O獨立；14lost利弊／77Miss／真採納與跨曲效果仍Unknown。

### J28. X11-P-R2總控簽收：active覆蓋成立，成本仍not-ready（2026-10-03）

詳[R2總控獨立驗收](../05-runtime/RUNTIME_X11_P_R2_CONTROLLER_ACCEPTANCE_20261003.md)。R1 305/1580/376/2059與R2 38/562/707 source/dependency/artifact項目重hash及檔長符合驗收開始時綁定；新B0/B1/ASan tests各7/7、兩版512 streaming bridge實際重跑符合。原12run與新3run raw報告重算byte-identical，各11組metric/全分母/archive receipt-release及active witness符合；normal入口exit1且未建run目錄。開發11套XML核對，不冒稱本次重跑未改舊suite。無新build/cost/stress/full PNG/emulator/live。

**簽收工程與容量admission負結果，not-ready維持。** 原三筆OS並行各1Down/24Move、same contact、32/33/33 body witnesses及requested release1，補到ASan active路徑；不代表128target聯合覆蓋、tail Up或性能資格。原四noise fail不重開。長窗八AA投影228,019,240B超本包128MiB，normal AA/ABBA0合理；這不是機器容量不足或candidate性能退步的證明。≥2000樣本只是工程解析度目標，不保證有效獨立樣本或noise通過。

**下一優先X11-P-R3長窗成本資格，未派送：** 另立≤2GiB獨立成本資料根，舊campaign+prior≤8GiB保持、兩帳≤10GiB，新build≤3GiB/另free5GiB；完整JSON直接保留，不先造壓縮平台。先驗長窗receipt/release/row/archive sample及physical byte硬界限，再凍結256warmup＋2304measurement、原負載/lead35/原gate，一次有界8AA→通過才16ABBA。失敗計額、不追樣本、不改pending規則，仍失敗則回總控決策而非自動無限R4/R5。具體容量分配與停止線見驗收頁；X12仍未授予資格。

HEAD/main仍f83c7ea、唯一worktree、原dirty保留、正式src/include/root CMake未改。R2 ledger綁定了兩個可變工作文件，總控在追加本節/forward進度前保存完整pre-review快照並核原SHA；歷史freeze不改，後续只能對這兩個已記錄追加路徑用preimage核歷史，其餘705artifact須保持。新驗收根runtime-x11-p-r2-controller≤16MiB，精確SHA/容量見其final-summary。C36h baseline/main50 donor、X10b否決/X10d-O獨立、14lost/77Miss/真採納Unknown保持。

## J29. X11-P-R3長窗成本資格交付（2026-10-03，待總控獨立驗收）

**交付完成，待總控獨立驗收；not-ready。** [R3結果](../05-runtime/RUNTIME_X11_P_R3_RESULT_20261003.md)、[protocol](../05-runtime/RUNTIME_X11_P_R3_PROTOCOL_20261003.md)、[交接與獨立驗收](../05-runtime/RUNTIME_X11_P_R3_HANDOFF_20261003.md)。新獨立C++20 bounded collector保留256warmup＋2304measurement、原RGB16／owner32 stimulus/shared Wake/SessionGameOwner/full SessionArchive JSON，主measurement n≥2000。R1兩core不改不重編、B1仍唯一pending-only8行，suppression OFF；SessionArchive對稱object僅三處sample16384→65536。正式86檔、原dirty/oldfrozen/raw保持，status/forward只追加。

有限資格AA=5（RGB4＋owner1）、ABBA=0、新stress=0。owner1 full owned2282/2560=89.140625%<原90%（少22），依凍結停止線立即停，不補owner2–4、不重跑。measurement recognition/owner n2231/2052達標，full latency hard gates、五run raw完整性和每run80MiB容量均pass；不是candidate成本退步或容量不足。R3成本前另加measurement90%較嚴門檻亦fail，原full門檻已獨立足以停止；不得把新增條誤称R1原門檻，freeze不回改。必要AA未完成，noise未評估、candidate差異Unknown，X12仍blocked。

新B0/B1/ASan collector各10/10，兩版512input全byte/row/EOF bridge一致，五筆C++raw audits重算14metric families及all/warmup/measurement/四blocks完整分母。原11套XML僅核SHA/歷史結果，R2三筆active OS/ASan僅引用，沒有本輪stress或全原suite重跑。51source/binary/method＋575compiled dependencies＋另2input freeze、六maps/inverse/old SHA核對保存；成本後rebuild/repair/retry0。

normal raw98,156,255B、out278,012,952B；舊campaign+prior前後8,241,831,623B不變。完整new final-summary/ledger含failed build/初版source/log/XML/文件、全raw、LF/CRLF/file lengths，遵守new2GiB、工程96MiB、controller32MiB、build3GiB、舊8GiB、兩帳10GiB/free5GiB；NTFS allocation Unknown。C36h tint1 behavioral baseline仍非重建原live SHA、main50 donor/control live0，本輪無emulator/ADB/真觸控/模型/新goal/chat/automation/commit/push。下一步只待總控独立簽收negative；若另定有界工程先研究已有owner全分母缺口，不自動R4/R5/X12。14lost opportunity/77Miss/閉環/跨曲仍Unknown。

### J30. X11-P-R3總控簽收：長窗已可量測，owner全分母不足（2026-10-03）

詳[R3總控獨立驗收](../05-runtime/RUNTIME_X11_P_R3_CONTROLLER_ACCEPTANCE_20261003.md)。**簽收collector工程及覆蓋率負結果，not-ready維持；停止目前資格追加測試，不自動R4/R5/X12。** 809artifact、51source/binary/method、575compiled dependencies、2inputs、六link maps及舊X11/R1/R2全綁定重新核對，57source inverse及正式86檔不變。B0/B1 Release各8/8（排除兩大型archive fixture）、ASan完整10/10實跑通過；原11套XML僅解析/核SHA。五筆C++raw audits與開發報告逐byte相同，另獨立重讀分母/主要分位數/archive segment SHA/CLI gates；兩版512input streaming bridge實跑一致。無新build/cost/stress/full-PNG/live。

AA5全為B0：四筆RGB各自normal gate過、owner1 owned2282/2560=89.140625%<原90%（少22），85consumer skips＋193decision skips全部保留。measurement recognition2231/owner2052≥2000、全run延遲硬gate/完整性/容量均過。這不是容量不足、候選退步或本轮noise failure；必要AA未完成，noise未評估、ABBA0。R3另加measurement90%為成本前較嚴新條件，不能稱R1原規格；原full90%單獨即足以停止，freeze不回改。latest-only允許skip，未達比較資格不等於正式安全契約錯誤。

總控暫停pending-only候選升格，保留X10d-P冷驗證與全部負結果；不放寬90%、不原樣重跑。若續投入，先以既有owner raw建立193decision skips的可證偽原因，區分producer/recognition到達群聚、action服務缺口及尚缺actor時戳，未能識別則保留Unknown；具體插樁或新成本工作包須另經總控決策。本次未派新task，未啟動emulator/ADB/真觸控/模型。14lost取捨、77Miss、閉環與跨曲仍Unknown。精確驗收bytes/SHA見runtime-cost-x11-p-r3/controller-review/final-summary.json；原R3封存不改。

### J31. 選項A：既有R3的193次decision skips離線診斷交付（2026-10-03）

**選項A交付完成，待總控獨立驗收；成本資格仍not-ready。** 見[protocol](../05-runtime/RUNTIME_DECISION_SKIP_A_PROTOCOL_20261003.md)、[result](../05-runtime/RUNTIME_DECISION_SKIP_A_RESULT_20261003.md)及[handoff](../05-runtime/RUNTIME_DECISION_SKIP_A_HANDOFF_20261003.md)。只讀既有五B0 run；owner2560published/2475consumed/2282owned，85consumer skips與193decision skips各逐列保存，四RGB decision skips1/0/0/0，全五run194/139分開。原90%負結果、AA5/ABBA0/noise未評估保持，沒有新runtime/cost/stress/OS/gameplay或候選B1退步主張。

193份中142recognition complete在前owned accept前、51在其後、0在accept內；publication bounds證136份在前accept開始前、51在accept結束後，餘6對該邊界未知。Exploratory再界定6份整個residency在前selection→accept之間；不能把completion當publication或owner_start當selection。191/193份下一recognition gap≤4ms，但owned966/2281也群聚，matched193對control133/193亦有；不是群聚充分／必要或OS根因證明。185skip edges全按事先同run/window/前phase/targets最近匹配，enqueue p50=1.0035/control1.0447ms，gap p50=2.5085/control12.2675ms，未支持一律較大服務成本。source-contained enqueue為accept外gap內elapsed下界，writer並行不加到action；remainder不叫OS耗時。

發現RGB raw event/release attempt實為accepted decision sequence，consumer skip後不同source frame；新reader用archive sequence→frame_sequence映射並保留source-anchor-map，不回寫原R3窗口、門檻或ledger。owner stimulus兩者相同，193题不受影響。全部causal allocation仍Unknown；建議總控有界選項B量測契約審查，最小publication/selection時刻有價值，但本包不插樁／改方法／開R4。若不補最小觀測，停止此線由總控考慮獨立X10d-O；本包不執行。

新獨立C++20 analyzer不連pas_core；Release/ASan合成各17/17，另ASan只讀完整五run核reader/joins/metrics，exit0。最終两次16檔deterministic、193逐列及全denominators/edges/arrivals/matches/8SVG、94input references、new source/binary及396實際CL/link inputs綁定；原809與controller92兩ledger及51/575/2/R1source305核對。初14tests、探索版、2failed builds、2raw join拒絕及初不完整dependency freeze保留，v2補全不覆寫。精確bytes/hashes/commands在runtime-decision-skip-a/final-summary；自驗<48MiB、總控16MiB保留、build<1GiB/另free5GiB，舊campaign+prior8,241,831,623B及R3不變，合計<10GiB。HEAD/main f83c7ea/唯一worktree、正式86檔與prior dirty保留；本節及forward只追加且原byte prefix核對。C36h tint1 baseline、main50 donor/control、pending-only與suppression OFF不變；14lost/77Miss/真capture/RPC/adoption/跨曲Unknown。完成後停止，不自行開task、傳訊、goal/automation/emulator/ADB/live/model/commit/push。


### J32. Chapter Legacy IN zero miss基準與文件入口整理（2026-10-04，開發自驗交付）

**本包只整理基準／逐曲證據／最新入口，完成自驗後交總控獨立驗收，未宣稱總控已簽收本包。** 最新入口：[10/4總帳](LEGACY_IN_ZERO_MISS_BASELINE_20261004.md)／[逐曲JSON](LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json)。產品目標是Chapter Legacy所有曲目解鎖IN並完整IN結算Miss=0；HD解鎖／回歸、P/G/B照報、不加AP前置。已知20曲名、選入89保存完整結算（70HD/19IN、33sessions）；目前完整章節分母未核、chapter_listing_verified=false，各曲目前解鎖unknown。Dlyrotz與光有IN保存結果不代表僅兩曲已解鎖；HD高分不代推。原source/binary/profile/score/PGBM/result/summary及歷史重跑各自保留，所選IN尚無Miss=0。

X1於J5已獨立驗收，不再列待實作；X2–X6因果／負研究非部署策略；X9原Hold contact保持且額外fragment Down，X10b family經X10c否決。X10d-P J22冷契約保持（新13/13、原suite一項已聲明語義fail），14組機會利弊Unknown，suppression OFF，X10d-O未开始。X11-P/R1/R2/R3工程及負結果已有總控驗收，成本not-ready：R3五normal全B0、RGB4＋owner1，2560published/2475consumed/2282owned、85consumer/193decision skips、89.140625%＜原90%，AA5/ABBA0、noise未評估；不是B1退步或長窗容量不足。

10/4 [0419 receipt](../../../measurements/runtime-decision-skip-a/controller-review-dot-20261004T0419/execution-summary.json)原reader self-test17/17／verify-input原tree相同，只支持reader reproducibility；新independent checker仍未建置驗證。[002](../../../measurements/runtime-decision-skip-a/controller-review-dot-independent-002-20261004T0519/execution-receipt.json)與[003](../../../measurements/runtime-decision-skip-a/controller-review-dot-identity-003-20261004T0618/configure-identity-receipt.json)configure根exit0但兩秒quiescence gate拒絕，003 owned survivor為vctip.exe、Job清理verified／active0；工具守門阻塞不稱產品build/性能失敗，005未執行未採用。193因果Unknown與RGB decision/source anchor差異保持。

後續只準備順序包：第二包A獨立驗收收尾→第三包B最小publication/selection量測契約審查（不授權R4/cost）→第四包X10d-O（真圖反例／合成控制，至多一通用候選、不混P/不復活suppression）→第五包conditional live及全曲IN驗收。O研究不依賴193全部歸因；X12成本gate未過不得自行開。

main/HEAD f83c7ea、唯一registered worktree、C36h baseline/main50 donor live0。接手2tracked dirty＋220untracked保存；status及forward只append byte prefix，其他原dirty不改，正式src/include/root CMake無diff。本包核89輪引用及311項期望SHA；數字沿歷史pixel review，未逐圖重新人工讀數、未全rehash7722PNG。舊recovery目錄不在registered清單且LibTorch／CPU runtime路徑已不存在，cache仍指歷史路徑；不默認可build、未重裝。新增維護證據[controller-baseline-20261004](../../../measurements/controller-baseline-20261004/handoff.md)≤16MiB；全部成功／失敗、JSON／reference／保護與容量自驗留存。A–I/J1–J31及原forward前綴不回寫；未啟動emulator/ADB/觸控/manual-session/模型、runtime/cost/stress/full replay、goal/chat/automation/傳訊，未commit/push/merge/reset/clean。交付後停止，由總控獨立驗收續派。
