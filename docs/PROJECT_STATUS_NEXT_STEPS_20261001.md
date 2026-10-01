# 整合後現況與下一步研究（2026-10-01）

**建議先補最小的真實全錄接觸重播，據此修正已定位的關聯／Hold 機制，再做少量針對性覆核。** 不需要先建立大型標註平台、增加訓練或重開擷取選型。本輪已讀取程式及原始證據、實際重播 main50、清理可再生檔案並整理文件；沒有修改正式策略、啟動 emulator／真觸控／AP goal 或追加模型訓練。

## 1. 版本、成果與未驗項

研究基點為 main `9fea67e248411f7c2309f220daf989f652b27d15`，**observer50／planner27／diagnostics11，live rounds=0**。它合併 main 的線身分／關聯／有界投影與 C36h 當前 ridge、Flick 核心及 Hold patch/front 續接。歷史 C36h observer37/planner19 與歷史 main 同號版本不同，必須按 variant／binary SHA 辨識。完整合併和來源清單見[整合交接](MAIN_INTEGRATION_HANDOFF_20261001.md)。

| 範圍 | 已有證據 | 不能據此宣稱 |
|---|---|---|
| 正式 C++20 即時鏈 | latest-frame、獨立觀測／身分／關聯／預測／owner／注入；QPC、多指門控、到期與失效釋放 | 所有判定線／Note 形式已驗收 |
| 9/29 冷開發 | D1–D4、C0–C6 可冷範圍、合成 RGB＋獨立 oracle／fake-clock、混合動作、三執行緒冷鏈及容量／故障回歸 | 合成接觸已被遊戲採納；所有性能子規則都通過 |
| main 整合 | Release 349：348 pass／1 opt-in skip／0 fail；CPU 8/8；post-commit 小回歸另存 | main50 有新實戰；本次跑過整合版 Debug／ASan |
| 兩輪歷史 19HD | Miss 815→818、非 Perfect 957→860，獨立 binary／原始 session 保留 | 已改善主要 Miss、AP 或未知曲泛化 |
| 恢復線實戰 | C36g-rec1：778185／P485-G16-B6-M77；C36h tint1：795950／P496-G11-B0-M77；使用者回報主要問題未改善 | Score 上升即代表 Miss 問題解決；C36h 是舊 main37 |
| 全錄及選片 | 7722 原生 PNG；131.8293931 秒 received pixels；12 段理由已填，3722 unique／3843 references；clip09 已更正為正常對照 | 來源完全無漏幀（capture 最大 gap443.5487ms）；理由是 pixel gold |
| C36h 新一輪 | 27 張 sparse RGB／9 triples、完整 journal 與結算；逐 Note 判定 unknown | 用這27張精確定位全部77 Miss，或由cancel數推算Miss |
| 離線模型 | 4377 參數合成小試、合成 macro IoU .89267、18 native ROI proposal；frozen 工具／權重存在 | 真實準確率、跨曲驗收、training-ready，或模型已進即時迴圈 |
| 本轮驗證／整理 | main50 observer 暖機重播、原檔 hash 保護；清理後117/117相關回歸、CPU packet只讀audit有效 | 完整真實owner反事實重播已完成，或遊戲效果改善 |

目前 pixel human gold=0。18 ROI 全屬 Dlyrotz development；人工文字、AI 目視判讀、模型 proposal、runtime ID 都不等於人工 instance／relation 真值。資料分家族的規則仍保留，不能把同一曲的不同片段當未知曲。

## 2. 本輪新增的可重現差異

以現有 `pas_frame_review.exe` 對原全錄執行 main50 replay，沒有更動工具或策略。輸出在 [main50-observer](../measurements/research-next-20261001/main50-observer/summary.json)，與已保存 `frame-analysis36h-v3` 比較見[比較輸出](../measurements/research-next-20261001/c36h-to-main50.json)。

- 核對全部 7722 PNG、21184 journal rows；原 journal 有7684筆 decisions，按原辨識 frame集合及最多32 preroll 執行7715次 observer。輸出3722張不同選取影格；選段有6個decision join缺口（按clip引用計數），保留 unknown。
- `frames.jsonl` 是原 decision／event join，SHA仍為 `e54f2744e58767073efb01349fc83c24cf5a676eda869a843786c6fd79a53195`；不是新owner輸出。新版觀測在 `replay.jsonl`，SHA `103742ee30e897c81a530bb99e3c4ff7329c1f33703b1d286c65ed01baeabaff`。
- 3722選段幀中960幀的 root／垂直線數不同；增加240、失去1136次root出現，增加28次垂直線出現。工具只比數量，**不是同物件配對、正確率、Miss或命中差值**；選段亦有使用者選擇偏差。
- 新工具 SHA `5b027ebba3f38d95fe420d73475441019d45212b5ce8a257815745cafb7abee5`；正式 main binary SHA `1440cba9b176f23f9c95513e56b3e8826df1f638b8b17c055955ee170a88addb`。完整 provenance 與容量見本輪 maintenance 帳本。

### 2.1 Hold：當前 body 存在，不等於一定已有接觸

目視[3493–3504 原圖接觸表](../measurements/research-next-20261001/hold-3493-3504.png)可見左側 Hold body／側軌、斜線與穿越的垂直亮線。ordinal3498（source_frame15265）兩版都輸出當前 held body／rails、相同幾何約(303.60,604.78)：

| 重播 | Note／line ID（各版本內） | samples／reason | root |
|---|---|---|---|
| C36h-v3 | 945／122 | 4／root_past | 無 |
| main50 | 949／105 | 1／motion_discontinuity | 無 |

main50在ordinal3495的patch=true，到3498變false；3501仍有同ID949／line105、當前body／rails，samples=3、reason=relative_velocity_small。另一上方候選main963在3498／3501關聯到近垂直line131，與C36h不同；這需要分辨同一body的片段候選與真正獨立物件。

這些是「候選、關聯與擬合」差異，**不能說主Hold已被cancel**。`GamePlanOwner` 的活動Hold可在合格current held_support下不靠新root續接；未開始的Down與活動contact走不同分支。必須查出该版本自己的head是否成功Down、cursor、body續接、alias和Up，不能從原版journal搬一個contact來當新版本狀態。

### 2.2 缺body：不能把不可見片段全部當辨識漏失

[4979–4990 原圖接觸表](../measurements/research-next-20261001/no-body-4979-4990.png)包含線／Hold暫時消失與返回。ordinal4986兩版都没有Hold，只剩特效附近Drag候選、line=0／samples=0、line_unobservable。當幀缺少body支持不證明遊戲內Hold已完成，也不能要求模型憑空補成可行動body。這個窗適合作為「不可見、不Move、不重播Down、只在既有期限內保留後釋放」的對照，tail／逐Note遊戲結果仍unknown。

### 2.3 側向Drag：線已被看到，關聯仍可失敗

直接檢視[ordinal6214原圖](../measurements/game-assist/2026-09-30-m0-manual-continue/full-recording36g-01/sessions/manual-session-22885039263800/full-recording/frames/frame-006214.png)：垂直線x988.5、水平線y576及右側黃色Note可見。main50的同一Drag1644在6208→6211→6214→6217從x849→887→933→964，y約287–288，顯示當前序列主要向右移動。

| ordinal6214／source_frame17981 | C36h-v3 | main50 |
|---|---|---|
| 當前兩條線 | 垂直360、水平343 | 垂直318、水平305；pose與C36h相同 |
| 右側Drag約(933,288) | ID1622→垂直360；samples4；root23189781496968 | ID1644→水平305；samples5；relative_velocity_small，無root |
| 左側Flick約(302,363) | ID1627→水平343；samples5，有root | ID1648→line0；samples0；confirmed_line_relation_conflict |
| Tap約(473,519) | 有root23189791599522 | 相同root，ID不同 |

**這是main50相對C36h已恢復路徑的具體關聯反例候選**：當前線提取並沒有失去那條垂直線。main的已確認關聯保留、初始配對及身分歷史應優先檢查；尚未完成消融，不能直接把原因定為某一行或宣稱造成一個Miss。Flick在6217又接到水平線、重新累積samples2，也需核對最初為何確認到另一條線。不要只再放寬ridge顏色或只加模型辨線。

ordinal5520的C36h Flick1571／main1592都保留一個合併核心及同root；可作Flick局部合併對照。clip09原指定90秒／ordinal5287應保留正常對照，不能事後把其cancel事件改寫成使用者認定的問題。

### 2.4 成本與重播限制

環境為同主機Windows x64、MSVC v145 Release、Intel Core Ultra5 125H／32GiB RAM、1280×720。main50 observer n7715，p50=6.4493、p95=12.70719、p99=17.543938、max=28.8193ms，jitter p95−p5=8.09109ms；程序exit0、無報告的重播錯誤。計時包住`observer.process`及`decision_json`，不含PNG解碼／磁碟／其他工具開銷。執行期間有檔案hash／維護編譯負載，既有C36h-v3亦有ctest負載，**不是受控效能A/B**，不宣稱改善或退步。

現有程式用FakeClock呼叫GameObserver，但沒有FakeTouch／owner；epoch／generation／geometry固定1，也未還原SessionPerception的new-round reset。早於32 preroll的待命狀態缺失。`summary.reports` 的cancellations等來自原journal，不能拿它證明main50的動作。這些限制不妨礙把差異拿來建立回歸候選，但禁止把重播當原實戰的完全重現。

## 3. 已有實作與真正缺口

| 層 | 已存在的正式機制／來源 | 下一步要區分的失敗 |
|---|---|---|
| 像素支持 | [game.cpp](../src/game.cpp)既有Hold優先側軌續接、前緣／outline／body搜索，一組rail只給一候選 | 原圖可見而未提取，或原圖本就不可見；不能用歷史boolean續租 |
| Note身分 | 有界近期匹配與held rail更新；新片段不應擦掉既有body | 同body被切成多ID、兩個Hold誤合併；runtime ID不是人工物理ID |
| Note→line | [game_tracking.cpp](../src/game_tracking.cpp)全域線ID、候選分數、confirmed關聯／連續線重接 | 初始誤配被保留、線交叉搶配、ID換代錯配；不能只看線數 |
| root | 分段相對距離、反轉／跳變清fit、時間與殘差門檻 | 首次接入時序不足，與活動Hold不需要新root兩者分開 |
| 活動contact | current held_support、compatible_body、已有contact alias、patch/front特例、同指Move／tailUp | 舊contact是否曾開始；哪個guard取消；不因暫時無root就直接重試Down |
| session／scheduler | [game_session.cpp](../src/game_session.cpp)的round reset／gate／finish，單owner、幀間到期／due | 只按幀pump漏掉幀間deadline；錯誤暖機、UI門控、未知receipt與release |

兩個值得先寫反例的靜態契約風險：

1. `preserve_confirmed`的註解說持續接近，實際條件為`current_distance <= prior_distance + max(12, width*0.10)`；逐幀小幅遠離或平行運動仍可通過。不是本輪引入，也還沒證明是77Miss根因；需側向Drag／兩線交叉／短暫量測噪聲成對約束，避免一刀移除後又被交叉線搶配。
2. owner在body grace之前對`samples==0`直接`current_geometry_unsupported`取消；samples=0可能與關聯失效同時出現，與「當前body不可見」不是同一分類。先記錄來源和分支，不直接放寬此安全guard。alias、patch切換、正常tail與completed identity不可混在同一修補。

## 4. 方案比較與最小下一步

| 方案 | 能回答什麼 | 成本／風險 | 排序 |
|---|---|---|---|
| 延伸現有C++全錄reader，暖機正式session／owner＋FakeTouch | 當前支持存在卻掉contact的具體分支；關聯修正是否改變接觸 | 必須處理reset、due、gap、初始unknown；只做本批小範圍輸出 | 第一 |
| 沿用12段理由及18 ROI做針對性覆核 | body／tail／干擾線與物件連續性；拆開proposed與human | 人工時間；僅Dlyrotz仍無跨曲驗收 | 與已定位反例配合 |
| 直接反覆調lead或放寬body grace | 容易改變分數，無法辨認關聯根因 | 危險誤觸、舊contact續命、掩蓋問題；main50未實战 | 不作起點 |
| 再訓練大模型／擴大合成選模 | 可能改善離線proposal，尚不能回答owner失效 | gold0、domain gap；不能進runtime；同eval反覆選模失真 | 目前不做 |
| 大型通用Fixture／全新標註平台 | 長期工程能力 | 既有合成RGB、oracle、fake-clock與全錄reader可重用，會推遲具體根因 | 不作前置 |

### 4.1 P0：完整接觸重播的最小規格（尚未實作）

1. **輸入／暖機**：使用原完整session索引，核對SHA、source domain與capture／pixels_ready。先保持已記錄的消費frame集合，從最早可用preroll連續推進到選段；全received-frame模式是另一實驗。原decision只提供對照／cadence，不把原按鍵、原note ID或原root餵回策略。從pixels重建SessionPerception、new-round reset、gate和owner生命週期；缺更早狀態標unknown，不能注入一個假定的既有Down。
2. **時間模型**：以原host QPC相對間隔驅動FakeClock。明列選用的frame交付時間（優先pixels_ready）與辨識耗時模型；首版可採宣告的零fake辨識耗時，只驗語義，不能冒稱重現原排程。按`next_due_ns`在幀間處理到期與已到期步驟；同時刻frame／due的排序先凍結，以合成tie案例測差異。CPU成本另用真QPC，不與fake時鐘混報。
3. **小範圍輸出**：暖機整段但只保留3493–3504、4979–4990及必要前後文，另加5287正常、5520 Flick、6214側向Drag的有限窗口。輸出source_frame→scene→plan revision→intent／contact→Down／Move／Up／cancel／receipt，明列觸發guard、當前body／line證據、samples、cursor與期限。
4. **硬上限**：重用現有16線／128note與owner容量；每個事件步立即排空診斷，避免128／256有界隊列截掉關鍵因果。輸入仍沿用全錄界限；首批新反事實產物建議≤128MiB，越限明列失敗。只引用原PNG，不再複製全錄；一切derived輸出仍計入原campaign，不用新目錄繞容量。
5. **驗證**：同binary／inputs／time policy兩跑語義一致；合成已知head→body→tail、body缺失、跨幀due、reset／UI gate、真假alias、未知Down、completed返回等正反例；先確認錄影開始後能獨立建立相關contact，再談長Hold反事實。完整鏈不能可信暖機的案例保留unknown，不阻塞其他冷驗項。

### 4.2 P1：每次只改一個已定位機制

| 假說 | 支持或推翻方法 | 必保留對照 |
|---|---|---|
| H1 body提取不足 | 原圖可見body時替換為獨立離線oracle，其他層不改；若contact仍失效，單純提取不足不夠解釋 | 當前無body、特效、tail已結束；oracle不是runtime輸入 |
| H2 身分碎裂／alias錯誤 | 對同body的候選／alias edge逐幀列來源，查head已Down及完成去重 | 雙Hold交叉、同位置Tap、已完成返回、不重播Down |
| H3 關聯錯誤被確認保留 | 6214及其早期窗定位首次確認；合成向右Drag／水平與垂直線／漸遠及量測噪聲，最小消融confirmed規則 | 真正同線接近、交叉線搶配、晚對齊、線追Note、旋轉Hold |
| H4 owner取消分類不當 | 有current body卻取消時，分出samples0、歧義、expired、body不相容、缺物件各條路徑 | 缺像素不Move、未知receipt不重试、期限不延長；不能僅增grace |
| H5 原始交付不足 | 列capture gap、消費skip、缺journal／preroll和time模型；以fake-clock插入同樣gap | 不補造幀、不把drop全算Miss、不由遊戲分數反推物件 |

接受一個修正前，至少須有可重現失敗→修正後通過、相鄰負例未破壞、主線安全測試與本批已知正常對照。main50與C36h比較按幾何／frame定位，不按ID數值或root總數打分。若只改變root數而無可解釋contact差異，停在「觀測／預測差異」，不升格成實戰改善。

### 4.3 P2／P3：標記及後續驗收

標記先覆核已有18 ROI與上述窗口，補當前可見body／head／tail、干擾線、同物件連續性與unknown，不要求全7722張先人工gold。模型proposal與非學習式提取都可作離線助手；每個人工改動保留來源、覆核者與狀態，不自動升格。只有已確認的觀測缺口才比較離線模型是否節省人工成本；缺跨曲gold時不再用合成IoU替代。

冷驗後再整理有限實戰議程：凍結binary／profile／fingerprint與原始資料預算，固定跨曲A/A、A/B，分HD／IN報完整分布、故障與退步。這是後續建議，須由使用者另行安排；本輪沒有開始它。M3／M4可略，模型runtime／GPU部署已退出現行範圍，原[跨曲研究](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)中相關文字僅作歷史提案閱讀。

## 5. 整理交付與可追溯性

README／AGENTS／架構／路線圖已統一當前版本和優先順序；原始研究／實驗報告保留日期與結果，只修正與現況混淆的狀態。使用者的判定線／Note形式及`REASONS.md`原文不改。實際刪除211個可再生中間／單元測試檔、保護驗證、帳目修正及依賴引用見[本次清理帳本](CLEANUP_AUDIT_20261001.md)。

本次新增研究資料45,307,809 bytes，仍向原8GiB campaign加帳；冷研究與清理沒有消耗新的實戰輪次。原始／frozen／失敗證據均保留。`measurements`／`out`為ignored，文件與Git提交不能取代這些實體檔案的備份；較早已刪raw的缺口仍以[9/28紀錄](CLEANUP_AUDIT_20260928.md)為準。
