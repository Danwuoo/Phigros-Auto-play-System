# 本輪驗證：2026-10-23 起，更新至 2026-10-25

## 2026-10-25 完整操作面板／AT與接點預算

新005 native29/29 CTest、Flick／來源／opening207 checks、learned HUD161 checks、HUD glyph158 checks；Python244項（243通過／1個symlink權限skip），portable configure/build/CTest4/4，面板SelfTest52 checks。全389份已逐譜記錄準備或跳過；最終binary263/263離線前檢（IN241／AT22）及實際v2 context1166個pins核對通過。授權APK原譜384份bytes相同、5份版本差異保留；Credits AT真實只讀校正與離線正／負例通過。2001接點／4002事件的compiler、opening及runtime回歸通過，沒有截斷音符。126份按格式／身份／資源原因跳過；不把離線準備稱為遊戲判定或AP。詳見[完整面板、身份與限制](FULL_LIBRARY_PANEL_2026-10-25.md)。

## 2026-10-25 固定判定錨點直線Flick候選

新004封存native29/29 CTest、Flick policy199 checks、portable configure/build/CTest4/4、Python232項（231通過／1個Windows symlink權限skip），面板SelfTest52 checks。21首native offline preflight與324個GUI pins通過。Glaciaxion IN原譜51個Flick離線折返28→0，端點位移71.253–72.250px；判定時刻座標、DOWN／滑動末端／UP時間、opening及678個非Flick接點均保持一致。原dynamic政策fingerprint重現，新policy與profile／plan獨立綁定。新004遊戲實測NOT RUN，不從離線命令座標宣稱Miss或AP改善。初次Python WinError32清理錯誤及後續成功重跑均保留，詳見[完整路徑／封存紀錄](FLICK_STRAIGHT_PATH_2026-10-25.md)。

## 2026-10-25 Glaciaxion IN三輪實測

003 binary完成72→60→72 px三輪，使用者文字回報Perfect／Good／Bad／Miss為725/3/1/0、726/0/0/3、725/0/0/4。三輪均729 DOWN／729 UP、全部主機RPC成功、native確認結算、failure null與最後kernel zero；實際圖片0張。三輪均使用terminal-only結算v2 fallback，實際候選分別5／3／4個receipt、跨越71.436／69.730／64.454 ms。原manifest的unreviewed／AP false不變；沒有穩定改善、winner或新AP，時機／共用預設未調整。Cipher／FULi／Winter新版本實機重測仍NOT RUN。完整條件、時序與私有原件SHA見[三輪報告](FLICK_BAB_2026-10-25.md)。以下暫停狀態屬於較早準備階段。

## 2026-10-25 本機 native、結算與保存政策

新003封存使用既有patched gRPC，Windows native **29/29 CTest**、portable configure/build/CTest **4/4**；Python **229項：228通過、1項Windows symlink權限skip**，面板離線SelfTest52 checks。21份baseline plan/opening身份不變，三組距離設定共23次離線前檢通過，另核對兩種明確影像opt-in模式。

結算local-contrast-v2在8張既有結算／92張playing影格上100/100通過，修復三張舊漏記圖的離線分類。一般回合預設不寫PNG與HUD灰階，保留receipts、actual receipt metadata及結算frame SHA；舊原件不刪改。此前002 probe另完成一次新boot五指fixture及最後kernel zero；這不是003新遊戲資格。歌曲實測依使用者要求暫停，**新003歌曲／裝置整合NOT RUN，沒有新改善或AP宣稱**。詳見[本機接續](LOCAL_NATIVE_FLICK_2026-10-25.md)及[結算與保存](RESULT_RECOGNITION_AND_STORAGE.md)。

## 本次封存核對

使用者要求停止修正並commit／push。最新使用者實跑與結果圖已整理為[逐曲／逐回合表](SESSION_RECORDS_2026-10-24.md)及[機器索引](SESSION_RECORDS_2026-10-24.json)：build006共18輪，14輪完整輸入／8輪原生確認結算／6輪完成但未確認；正式章節8首AP與額外Verrückt AP由保存PNG複核。FULi／Winter的AP與Cipher結果補記在衍生表，原生failure不改寫；Engine／Non-Melodic缺完整結果圖，不由選曲頁最佳分補造本輪成績。

封存前重新執行：portable configure/build/CTest **4/4**、Windows非音訊整合CTest **28/28**、Python unittest **145/145**（使用task-local依賴，包含LZ4，無skip）。14個保留PAS source Git blobs核對原EXTRACTION身份。gRPC使用既有patched install，第三方fresh source重建 **NOT RUN**；本次整理新增裝置／ADB／遊戲測試 **NOT RUN**。使用者先前的實際重啟、五指及歌曲紀錄則以既有原件獨立列明。

原有280個本機目錄的metadata索引與1448份原件SHA封存在repo外；原native manifests、圖像、音源、譜面與校正均未覆寫。結算漏記與build006的20ms遲到仍未解決；沒有宣稱20首全測或跨輪時序合格。

## 2026-10-24 連續 UI 逾期與開場自適應候選

三次build005的actual_UI_receipt_expired均有持續有效影格、正常pause/prefix、連續27／29／22張score-only拒絕；Hold接到分數，約500ms後按原guard釋放。另一輪Glaciaxion5接點為獨立20ms遲到，owner間隔約27.6ms；現有日誌不能再歸因CPU／排程／配置。原件與kernelzero保留，見[時序稽核](../../local-assets/phigros-4.0.1/chapter-legacy-repeated-stop-timing-audit-001/README.md)。

新learned-title-redundancy-v1的157個離線檢查、Windows整合28/28 CTest及預設portable configure/build/CTest4/4通過。原v3正例路徑保留；新路徑要求兩份開場clean實際觀測、當下三固定glyph、實際score邊界交疊及部分數字；無時鐘／期限放寬。原生chart/profile/contact/opening bytes不變。48RGB與64MiB journal配置移到READY前，最多2個diagnostic lease與pool5有界；新增stage時戳，不把它當輸入延遲資格。

保存的Eradication兩張pre-arm實際影格相隔90.1282ms，可用於歷史學習回放；Glaciaxion保留的pre-arm影格不足60ms，其他真實像素補測需明示反事實unarmed時序，不能聲稱新實機學習已成功。新版本將保留兩份支持PNG及hash。新binary／重啟工具的實際emulator、五指及完整歌曲測試NOT RUN，等待使用者自行執行重啟流程。

## 2026-10-24 Verrückt AP 與結算標籤辨識

`chapter-IN-panel-20261024-190641-385-bb3ebb/round-01` 的原始結果圖確認1,000,000分、1285Perfect／0Good／0Bad／0Miss、MaxCombo1285、100.00%、Early/Late0。[獨立結果及回執稽核](../../local-assets/phigros-4.0.1/verruckt-result-review-001/result-observed.json)核對全部3358events（1285DOWN／788MOVE／1285UP）、peak3、最後零接觸。此AP僅屬於該build004/chart/profile/boot；原manual_result_confirmed=false及exit2保留，原因為結算模板未通過，180秒round watchdog結束。

修正新增共通caption標籤特徵，僅全曲輸入完成後可供manual-mode辨識結算，不判讀分數／AP。原模板、HUD許可、時鐘與安全條件不變。MSVC新測試與全整合CTest27/27、portable configure/build/CTest4/4通過；新binary的實際遊戲／裝置生命周期測試NOT RUN。這次工作沒有啟動遊戲或送入觸控。

## 最新：Verruckt HUD字形相位與等待狀態

兩個實際回合0 DOWN／UP、無anchor；所有1291／1277 UI observations卡HUD，未執行首音detector/model；user_stop後kernel零。原件與逐影格稽核在verruckt-noinput-ui-audit-001，無改寫。

HUDv3production852ae828...：255case/222unique RGB與獨立Python決策／位移族一致，190v2正例不退步、55嚴格負例全拒絕；首字形4CC與1px支持通過真實IN16，scorev2不變。實際fallback成本中位約0.7ms/p951.75ms，為離線host測量，不是dispatch或capture-clock資格。

整合CTest26/26與portable configure/build/CTest4/4通過。新GUIfebe887...取得階段70/70、回歸72/72、SelfTest32/32；offscreen視窗預覽不顯示、未連裝置。Native新增1s未arm診斷與最多24張100ms前置影格，全部不更新UI觀測／時鐘，新build實際送觸控與整首仍NOT RUN。


## 最新：Verruckt與非終端面板

Verruckt IN原件／音源已入檔，1285來源的起訖時間以獨立Fraction核對，完整3358events／peak3與raster範圍通過；精確時間檢查為1285start＋1285end。nativeprepare/check為離線，無裝置探測／輸入；EZ／HD raw另存。

本機library21項保留Legacy20原生順序，新增暖身不混入章節proof。library離線測例7/7；全Python114/114（含LZ4）。GUI純函數／hashworker／offscreen預覽及隱藏程序橋接測試範圍另見[面板說明](SONG_TEST_PANEL.md)，不是21首實機。

## 最新：004首輪中止與HUD v2離線修正

使用者實測Glaciaxion第一輪以actual_UI_receipt_expired停止：288 DOWN／287 UP、287combo，最終kernel零。18張連續有效影格只因Drag連到分數字而拒絕，500.275ms release符合原guard，原始紀錄未改。

HUD shared-in-cell-score-v2保持七個實際字形、baseline／右錨點／暗負空間／對比／間距，有限首字外接修復；不使用固定pitch或重建被遮像素。233筆RGB／204unique，45張本輪playing全通過、116既有正例不退步、39嚴格負例拒絕；pause/result優先與component cap保留。兩張嚴重中間合併仍unknown。

新build003：26/26非音訊整合CTest、4/4portable configure/build/CTest及20首新binary offlinecheck通過。原所有chart／contact／opening身份與14保留PAS blobs不變。新完整實機、重新裝置探測、第三方gRPC重建、Linux及其他18首實際UI均NOT RUN；既有patched gRPC編譯/離線contract涵蓋於整合測試。


## 最新：共通匯入／首音模型與20首候選

20/20 IN完成相容匯入、首音模型、完整編譯、profile及封存binary的離線check。共15,817 notes／74,302 events；62份原譜與421份舊產物雜湊不變，31,634個原生起訖以獨立Fraction核對最大差0 ns，所有來源恰好一次。四首異常tracks以明確候選政策求值，原始宣告與來源證據保留；嚴格預設不變。原Dlyrotz584／5669／peak4及fingerprint亦維持完全相同。

- Portable configure／build／CTest **4/4**；Windows/MSVC非音訊整合build／CTest **26/26**；Python **107/107**，含LZ4與11項全測清單測例。gRPC使用既有patched install，第三方source未重建。
- 完整計畫驗證移到首音擷取前，以不可修改且一次移轉的Prepared值綁新錨點；舊100 ms新鮮度不續期。共通IN HUD每幀仍經glyph／score及否決狀態檢查，含未arm／已判playing的畫面；STOP／release／epoch／容量條件不變。
- 80組歷史成本回放 **57完成／23按原guard停止**；9首尚無完整成本資格。清單004的20首是可控人工候選，沒有將失敗轉成通過、沒有放寬20 ms lateness，也沒有宣稱共通HUD已逐曲驗證。
- 新binary及本輪20首實機、重新確認當前boot、音訊同步、Linux新版本、世界計畫裝置：**NOT RUN**。全測準備未啟動遊戲或送輸入；14個保留PAS blobs及main不變。

逐曲來源、雜湊、封存、命令與限制見 [最新測試計畫](CHAPTER_LEGACY_TEST_PLAN.md)及[共通政策](CHAPTER_LEGACY_COMMON_POLICIES.md)。

- v3旋轉head detector及同色別名可見性保守邊界：15組／98 checks通過；20首簡化原生角度像素fixture全數形成唯一候選，85次觀察真實時間均保留。既有Dlyrotz／Glaciaxion實際anchor中心完全不變。這不等於18首實際渲染／Hold body資格。
- 連續父流程的NOOP檔案logger替身：全部20項native前檢先完成，再依序20輪；第3輪故障時只執行3輪、ledger標stopped，沒有裝置／gRPC／輸入介面。這是script流程驗證，不是遊戲全測。

## 歷史001：Chapter Legacy 入檔與原前檢查

完整來源與逐曲阻擋見 [Chapter Legacy 測試準備](CHAPTER_LEGACY_TEST_PLAN.md)。本機已封存20首、62份原生譜面、20組音源；421份產物、20份 WAV PCM、來源 APK 前後 SHA 及 Dlyrotz／Glaciaxion 的10份基準 bytes 均核對通過。兩份非 IN format1 原件保留，沒有偽稱 v3 匯入通過。

20首 IN 的15,817個原生 notes均列入索引；16首完整編譯，64組完整 FakeTouch 成本回放有49完成、15按 guard停止。四份 IN 嚴格匯入拒絕、首音模型與UI參考缺口皆保留；目前只有Glaciaxion符合本批全部離線條件，**整輪實機全測尚未就緒／NOT RUN**。清單保留20項，一首一輪；任一項 blocked 時，整輪 `-Run` 在連裝置前拒絕，不自動省略。

- 本輪最終 Python unittest：**106/106**，包含11項章節封存及10項測試清單檔案／順序／SHA／路徑／保留阻擋檢查，使用 task-local lz4 環境，沒有略過壓縮測例。
- Portable CMake configure／Release build／CTest：**4/4**；Windows/MSVC非音訊整合 build／CTest：**26/26**。gRPC為既有 patched install 的編譯／連結與離線 contract，本次沒有重新建置第三方 gRPC source。
- 實際五指 fixture：Android完整紀錄 peak5、ID0–4正常路徑／釋放、最後kernel零；五指live入口要求證據／原件hash與目前boot／route一致。失效boot、改動原件與缺少五指證據的拒絕案例通過。
- Glaciaxion live005完整729 DOWN／4,892 MOVE／729 UP，manual result confirmed、failure null、final kernel zero；獨立結算為**722 Perfect／1 Good／3 Bad／3 Miss，932,353分**，不是AP。live003的725 Perfect／0 Good／1 Bad／3 Miss及其結果辨識失敗、live002系統列停止、live001／004無輸入停止均保留。
- 原PAS extraction blobs再次比對不變，main未修改；新章節入檔／前檢查沒有新增裝置輸入或歌曲啟動。Linux新版本與世界計畫裝置測試：**NOT RUN**。

## 既有：live011 已確認 Dlyrotz IN13 AP（一次完整實測）

依使用者回來後的最新要求，首音視覺觸發後直接依完整 Dlyrotz IN13 譜面輸入；開始／結束仍沿用互動式 manual-mode，音訊保持暫緩。[全曲實驗報告](M7_WHOLE_SONG_EXPERIMENT.md) 分開列出離線驗證、歷史中止與 live011 的實際 AP；一次成功不繼承為其他時鐘或生產資格。

- [Live011 AP 結算 sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-011/result-observed-001.json)獨立目視 **1,000,000分、584 Perfect／0 Good／0 Bad／0 Miss、Max Combo584、100.00%、Early/Late0**。全曲 **584 DOWN／4,501 MOVE／584 UP＝5,669 events** 完成、failure null、manual-mode 結算、release known、final kernel zero。[逐筆 audit](../../local-assets/phigros-4.0.1/m7-whole-song-live-011/analysis/live011-timing-audit.md)確認來源／phase／pixel／deadline／完整步數與host接點生命週期peak4／final0；DOWN lateness最大3.896696 ms、全部events最大5.955067 ms。原manifest的unreviewed／AP_verified=false未改，sidecar的AP_verified=true只適用這份結果。
- [Build009](../../local-assets/phigros-4.0.1/m7-whole-song-build-009/manifest.json) **25/25 CTest、default4/4**；Flick為30 ms lead／60 ms travel／60 reference pixels／24 ms末端停留，Tap／Drag32 ms、phase−80 ms。相較010，480個非Flick contacts全同，104個Flick增加758steps；cache fingerprint `74e16603971e407627ea679a56f20300043a43c9fdf89008d9d6645d703e3ce5` 已固定。一次AP不證明各參數因果或跨輪重現率。
- 全曲 `ExperimentalContactRuntime` 接收完整 **584 contacts／peak 4**；Build001–008為4,911 steps，Build009為5,669 steps，保留 6 個 D250 投影候選與 `requires_calibration=true`。Build001–005 使用 32-step rolling prefix；目前 Build006 逐 contact 保留完整未執行 tail，總預算仍為 20,000 steps，100 ms lookahead／2 s first-step admission horizon／20 ms lateness 不變，沒有整首一次提交、漏 MOVE 或縮短 Hold。
- Build001 歷史 runtime 合成測試 **14 組／11,973 checks** 通過，包括完整規模、1,002-step 長 Hold、coverage 逾期與未知輸入／釋放。當時私有原生譜面 FakeTouch 全曲 **19,650 checks** 通過，584 contacts 完成、4,911 events 的時間／phase／來源／像素逐項吻合，active 峰值 4、pending 峰值 8、最後 UP 為 song **121.216 s**、最後接點為空。相同時間／phase 的多接點按來源 canonical 排序核對完整多重集合，沒有把 scheduler 的 birth-intent tie order 誤當缺失。
- Build001 歷史非音訊完整 **22/22 CTest** 通過；[build001 archive](../../local-assets/phigros-4.0.1/m7-whole-song-build-001/manifest.json) 保存來源、binary、依賴、[CTest 日誌](../../local-assets/phigros-4.0.1/m7-whole-song-build-001/whole-song-ctest-001.log) 與 [私譜 FakeTouch 日誌](../../local-assets/phigros-4.0.1/m7-whole-song-build-001/experimental-whole-private-replay-003.log)。這是離線／建置結果，不是遊戲輸入或 AP。
- 第一次全曲 launch 在 endpoint discovery 階段回報 `emulator discovery lacks valid port or token`；未建立 live output、未送音符輸入。auth 重啟後 live001／002／003 分別由起始 STOP、UI過期、visual observation gap 停止，均0 DOWN／UP並有final kernel zero。[Live004](../../local-assets/phigros-4.0.1/m7-whole-song-live-004/launch-failure.json) 在首音 constructor／READY 前因 native positive-approach 歧義拒絕，零輸入；沒有當次一般manifest或final kernel觀測。
- [Build003](../../local-assets/phigros-4.0.1/m7-whole-song-build-003/manifest.json) 首音觸發整合非音訊 **23/23 CTest**、default **4/4** 通過。UI watchdog明示500ms，unknown不刷新最後實際playing receipt，pause/result立即停止；anchor仍限100ms、step lateness仍20ms。[首音離線回放](../../local-assets/phigros-4.0.1/m7-first-note-trigger-replay-001/manifest.json)保留PTS-as-receipt成功與舊actual PNG因不足200ms lead被拒的兩種結果，不是live clock資格。
- [Live005](../../local-assets/phigros-4.0.1/m7-whole-song-live-005/manifest.json) 有 **53 DOWN／64 MOVE／51 UP、168成功RPC receipts**，於 `immutable_prefix_revision_rejected` 停止，runtime另釋放IDs0／1、RPC known且final pinned kernel zero。[結果PNG及sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-005/result-observed-001.json)獨立確認 **53 Perfect／0 Good／0 Bad／531 Miss、score90,753、maxcombo53、accuracy09.08%、Early/Late0**；原manifest的unreviewed／manual_result_confirmed=false未改。53筆DOWN send-after-due最大12.384647ms，ACK不是遊戲判定時間，下一輪維持−80ms phase。
- [Build004](../../local-assets/phigros-4.0.1/m7-whole-song-build-004/manifest.json) 已修正跨過真 UP 時的 immutable-prefix revision 邊界：舊版針對性回歸失敗，新版 **20 組／12,087 checks**、完整 **23/23 CTest**及 default **4/4** 通過。Live006 越過 live005 的停止位置；`overdue_tail_revisions_deferred=0`，不能說這輪實測觸發了修正分支。
- [Live006](../../local-assets/phigros-4.0.1/m7-whole-song-live-006/manifest.json) 完成 **150 DOWN／874 MOVE／150 UP、1,174 筆成功 RPC**，在 `actual_UI_receipt_expired` 停止，沒有未釋放 ID，release known、最後 pinned kernel zero。[獨立結果 sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-006/result-observed-001.json)與[其 SHA](../../local-assets/phigros-4.0.1/m7-whole-song-live-006/result-observed-001.json.sha256)保存 **150 Perfect／0 Good／0 Bad／434 Miss、score 256,849、Max Combo 150、Accuracy 25.68%、Early/Late 0**；原 manifest 不改。28 筆連號 4755–4782 的有效來源畫面被分為 unknown，含邊界最大 receipt gap 31.3076 ms；4783 再認到 HUD 時舊 playing receipt 已過 **506.6265 ms**，正確超過 500 ms watchdog。沒有逐 note 判定配對，也未完成整曲。
- [Build005 HUD 實驗整合](../../local-assets/phigros-4.0.1/m7-whole-song-build-005/manifest.json) 完整 **24/24 CTest**、default **4/4** 通過，HUD 純契約 **8 組／81 checks**。可選 pinned clean reference，只在已 armed、有效來源、unknown 且 `no_calibrated_state_match` 時補充實際 HUD 特徵；同幀 paused／result scores 優先，500 ms actual-receipt watchdog 不變。[V3 樣本核對](../../local-assets/phigros-4.0.1/m7-hud-glyph-reference-001/FEATURE_API_V3.md)為 11 個選定保存／合成樣本；live006 的 28 張 unknown 原圖未保存，不能據此證明能消除那段空窗。Build005 已用於下述 live007。
- [Live007](../../local-assets/phigros-4.0.1/m7-whole-song-live-007/manifest.json) 有 **196 DOWN／1,514 MOVE／195 UP、1,905 筆成功 RPC**；226 幀實際 HUD fallback 讓本輪越過先前約 39 s 的停止區域，約 song **47.25 s** 因 `compiled_trajectory_coverage_expired` 中止。Release known、最後 pinned kernel zero。[獨立結果 sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-007/result-observed-001.json)保存 **196 Perfect／0 Good／0 Bad／388 Miss、score 335,616、Max Combo 196、Accuracy 33.56%、Early/Late 0**，原 manifest 不改。
- [Build006](../../local-assets/phigros-4.0.1/m7-whole-song-build-006/manifest.json) 完整 tail／owner wait 修正完成 **24/24 CTest、default 4/4**；runtime **23 組／12,582 checks**。精準 live007 非零 RPC 時序在舊版重現 coverage failure，新版完整保留 42 個最小 fixture events；owner 跨舊視窗成功，以及真 backlog 超過 20 ms 必停／釋放的負例也通過。Live008 已完整送完並取得結算，見下列獨立結果。
- [Live008](../../local-assets/phigros-4.0.1/m7-whole-song-live-008/manifest.json) 完成 **584 DOWN／3,743 MOVE／584 UP、4,911 筆成功 RPC**，全部 contacts 完成，manual-mode 確認結算、failure null、release known、最後 pinned kernel zero。[獨立結果 sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-008/result-observed-001.json)記錄 **581 Perfect／0 Good／0 Bad／3 Miss、score 963,014、Max Combo 395、Accuracy 99.49%、Early/Late 0**，原 manifest 未改。[逐筆稽核](../../local-assets/phigros-4.0.1/m7-whole-song-live-008/analysis/live008-timing-audit.md)確認全 584／4911 的來源／phase／pixel／deadline／完整步數；DOWN lateness median／p95／max **0.473719／1.590705／3.603505 ms**，全 phase 最大 **9.436977 ms**，最密 60 events 全送。這是主機 deadline 稽核，不能直接辨認 3 Miss、量得遊戲時鐘或證明 AP。
- [Build007](../../local-assets/phigros-4.0.1/m7-whole-song-build-007/manifest.json) 完整 **25/25 CTest、default 4/4**；實驗入口 Tap／Drag dwell 32 ms、50 ms receipt 間距的預配 HUD timeline，compiler 預設 16 ms 不變。採樣不刷新 UI permission，釋放後才寫檔。
- [Live009](../../local-assets/phigros-4.0.1/m7-whole-song-live-009/manifest.json) 全 **584／4911** 完成、failure null、manual-mode 確認結算、release known、final kernel zero。[獨立 sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-009/result-observed-001.json)為 **579 Perfect／0 Good／0 Bad／5 Miss、score 936,815、Max Combo 260、Accuracy 99.14%、Early/Late 0**。[時序及 compiled 對照](../../local-assets/phigros-4.0.1/m7-whole-song-live-009/analysis/live009-timing-audit.md)確認 104 Flick／78 Hold 全 steps 與 live008 相同，只有 402 Tap／Drag 的 UP 延後 16 ms，全部 DOWN 不變。DOWN lateness median／p95／max **0.4898635／1.655571／2.848685 ms**，全 phase max **7.771330 ms**。HUD timeline 保存 **1,904 samples**、無 capacity drops；使用者回報 Miss 全是 Flick，逐 note 配對仍待獨立核對，不能由 3→5 Miss 推論 dwell 的因果。
- [Live010／Build008](../../local-assets/phigros-4.0.1/m7-whole-song-live-010/manifest.json) 完整 584／4911、failure null、manual-mode 結算與 final kernel zero；[獨立 sidecar](../../local-assets/phigros-4.0.1/m7-whole-song-live-010/result-observed-001.json)為 **580 Perfect／0 Good／0 Bad／4 Miss、score 929,110、Max Combo 206、99.32%、Early/Late 0**。Build008 **25/25 CTest、default 4/4**；[逐筆稽核](../../local-assets/phigros-4.0.1/m7-whole-song-live-010/analysis/live010-timing-audit.md)確認只把 104 Flick UP 延後 20 ms（末端停留 4→24 ms），其餘 4,807 events 不變；DOWN lateness median／p95／max **0.479325／1.526718／2.804889 ms**，全 phase max **3.544504 ms**。保存 1,918 個 HUD samples；使用者仍回報 Miss 全是 Flick，逐 note 對照另行分析，5→4 Miss 不證明因果。
- [主機等待量測](../../local-assets/phigros-4.0.1/m7-host-wait-001/REPORT.md)：無通知的 CV 1 ms wait 中位數 **15.5933 ms**，高精度 timer 為 **1.5305 ms**；新 owner wait header 的 **100 次 smoke** 無 QPC 回退或 handle 增加。這是 host-only 證據，未量測 gRPC／遊戲端到端改善。
- 非零 RPC 全譜[離線封存](../../local-assets/phigros-4.0.1/m7-whole-song-latency-replay-001/README.md)：live007 的 1,905 個 duration 循環，在兩種 owner 模型均完成 **584／4911**，最大 send lateness 約 **18.408／18.520 ms**；固定 0.5 ms 完成，固定 1／1.2 ms 仍按 20 ms guard 停止。已送事件的來源／deadline／pixel／未跳步前綴均吻合。新舊 baseline、每筆 traces 與 SHA 已封存；另三個錯位到事件空隙的 owner-stall 案例不算密集區驗證。這是 synthetic clock／UI 模型，不是 live 完整演奏。
- 首 Tap live007（−80 ms）的[實際結算原件](../../local-assets/phigros-4.0.1/m7-first-tap-result-007/result-1.png)是 **1 Perfect／0 Good／0 Bad／583 Miss**，score 1712；同設定 live005 的 Perfect 與 live006 的 Good Late 均保留。[三 Tap live001](../../local-assets/phigros-4.0.1/m7-three-tap-live-001/manifest.json) 有 **3 DOWN／3 UP** 成功 RPC 與最後 kernel zero，但缺少最終結算原件，**不宣稱 3 Perfect**。

生產 SessionRunner 的 live gate 未改；固定視覺 receipt-origin、rate 1、−80 ms phase 都是實驗假設，沒有取得 source pixel age、獨立 input lead 或全曲時間資格。**指定 Dlyrotz IN13 的 live011 AP 里程碑已達成一次；音訊仍暫緩，其餘未取得資格不自動完成。** 以下保留各次建置與試驗當時的歷史範圍；最新結果以上述全曲報告與開場試驗表為準。

## M0 原生資產接續（最新）

已從既有 phigros AVD／Phigros 4.0.1／157 取得四分包並核對裝置／主機 SHA。透過 catalog 精確 address→dependency→container key 抽出 Dlyrotz.Likey.0／Chart_IN 原生 TextAsset 及同曲 AudioClip FSB5。24 lines／584 notes（251 Tap、151 Drag、78 Hold、104 Flick）；44.1 kHz stereo PCM 解碼為 5,454,984 sample frames。全新目錄重跑的 10 份產物與 manifest／report 逐檔 SHA 相同；獨立 JSON／WAV reader 核對統計一致。詳見 [M0 來源與重現](M0_ASSETS.md)。

此項只驗資產身份及抽取；M1 原生時間語義、M2 幾何、M4 同步與 M7 實玩仍各自驗收。以下段落保留各階段當時的驗證範圍。

## M1 原生時間與語義接續

嚴格 Phigros v3 importer／Semantic IR／chart_inspect 已實作，16 項合成檢查、optional build 4/4 CTest 通過。Dlyrotz 原件可解析為 24 lines／584 notes／31,369 line events；C++ 逐 note 匯出的 1,168 個 start/end 時間與 Python exact Fraction 逐項比對最大差 0 ns。Offset 正值延後，僅正規化一次；來源 ID 使用 actual SHA＋JSON pointer。詳見 [M1 匯入與時間驗證](M1_IMPORT.md)。本項不驗證判定線幾何、實際對時或觸控。

## 已執行

### 本機 M2–M7 接續狀態

Windows/MSVC 19.50 先前包含既有音訊元件的 optional build **16/16 CTest 通過**；manual-mode 首輪完整非音訊 build 為 **14/14**，fixture adapter 為 **15/15**，非同步 foreground watcher 的 build002 與逾期邊界修正 build003 各 **16/16 CTest 通過**。兩個 audio options 均 OFF，`VCPKG_MANIFEST_INSTALL=OFF`，依賴取自已建置的 task-local patched install。[fixture build](../../local-assets/phigros-4.0.1/m6-probe-build-002/manifest.json)、[manual build002](../../local-assets/phigros-4.0.1/m7-manual-build-002/manifest.json) 與 [build003](../../local-assets/phigros-4.0.1/m7-manual-build-003/manifest.json) 分開保存來源、binary、日誌與 SHA。002 期間 Default 離線 fresh configure／build／**4/4 CTest** 亦通過；LastTest.log 已封存，configure/build tool output 沒有另存 transcript；003 沒有再跑 default。14 個保留檔案的 Git blob 與 extraction manifest 一致。音訊線仍依使用者要求暫緩。M5 非整數毫秒 UP 邊界修正的回歸仍通過。

- [M2](M2_GEOMETRY.md)：66 golden checks；真實 1,752 pose queries 的獨立 normalized 誤差至多 1.11e-16；6 張錄影疊圖保留原始音訊定位與 +130 ms 視覺相位候選。這不是輸入提前量校正。
- [M3](M3_CONTACTS.md)：全首 strict compile 仍因 6 個畫面外目標或中途路徑拒絕；新的 bounded normal intersection 候選保留全部 584 notes、4,911 steps，需 4 contacts。D=250 reference pixels 的候選仍 requires_calibration，未省略 note 或擅自宣稱遊戲接受。
- [M4](M4_AUDIO_SYNC.md)：合成 clock／失鎖測試通過，但真實 1D RMS 有結算後錯鎖；多頻帶研究改善負例仍未在首音前取得可靠 lock。全首 live readiness 未通過。
- [M5](M5_PAS_BRIDGE.md)：14 群 bridge 契約通過；完整 584 notes／4,911 steps 候選的 FakeTouch 回執逐筆吻合、pending 峰值 8、contacts 峰值 4、全部釋放。2 點容量負例正確拒絕。這是 synthetic exact-clock 模擬，實際遊戲輸入未測。
- [M6 裝置驗證](M6_DEVICE_VALIDATION.md)：patched gRPC fresh build／runtime 通過；fixture SINGLE002、capacity002（四指）、batch001、cancel001（release-all→UP）與 client transport-loss001 通過各自範圍的 host／native 核對。STOP002 真實 DOWN 後觸發 STOP，已按下的兩指釋放至 native zero，16 項獨立 cleanup 檢查通過；正常 expected-path analyzer 保持 failed。SINGLE001 缺 native、capacity001 verifier 誤拒及 STOP001 輸入前黑畫面失敗均保留原狀。單指 callback order-only clock join 只提供有條件區間，不是遊戲 input lead／jitter；Ctrl+C、最大容量及實際 Phigros 的連續輸入時序仍未取得資格。
- [M7](M7_RUNNER.md)：8 群／298 checks 通過；CLI --session-replay 使用完整真實譜面候選，經 supervisor→bridge→FakeTouch 完成同一組 4,911 events，末尾 known-release ACK 成功。fake-only API 明確拒絕 live。
- [畫面狀態](M4_SCREEN_STATE.md)：15 synthetic checks、13 實際樣本（3 模板自檢＋10 heldout）通過；小數／溢位幾何及 RGB hash 錯誤另有 CLI 負例。沒有把 frame arrival 當 capture-clock 資格。
- [視覺時間候選](M4_VISUAL_CANDIDATES.md)：solver 52 checks、開場 detector 22 checks，47 個 frozen PNG detector 結果逐項吻合；primary／heldout 密集回放可在明示 PTS-as-receipt 模擬時鐘下 arm。這是保留歧義、條件式 coverage／rate 假設的離線可行性，沒有 live source pixel-age 或 SyncLease 資格。[私有來源／回放封存](../../local-assets/phigros-4.0.1/m4-visual-candidates-001/manifest.json) 保留 SHA。
- [開場 Tap 校正](M7_FIRST_TAP_CALIBRATION.md)：首 Tap build001／003 各完整 **19/19 CTest**；build004 為有界記憶體 journal／過期 frame 處理的增量 CLI 重建，未改 freshness gate。live002／003 均在 DOWN 前失效停止的原件保留。live004（phase 0 ms）實際結算 **0 Perfect／1 Good Late／583 Miss**；live005（−80 ms）為 **1 Perfect／0 Good／583 Miss**，相同設定 live006 為 **0 Perfect／1 Good Late／583 Miss**，live007 再得到 **1 Perfect／0 Good／0 Bad／583 Miss**。各次各一組成功 DOWN／UP、最後 pinned kernel route zero；其餘 583 notes 沒有播放，這些結果不校正 host↔game latency。新三 Tap 核心 **15 groups／211 checks**、非音訊全 **20/20 CTest** 通過，[新 build archive](../../local-assets/phigros-4.0.1/m7-three-tap-build-001/manifest.json) 與實測單 Tap binary 分開保存；三 Tap live001 已有三組 DOWN／UP 成功 RPC 與 kernel zero，但缺最終結算原件，不能宣稱 3 Perfect。[004 獨立 audit](../../local-assets/phigros-4.0.1/m7-first-tap-audit-004/analysis.json)、[005 結算](../../local-assets/phigros-4.0.1/m7-first-tap-result-005/result-1.png)、[006 結算](../../local-assets/phigros-4.0.1/m7-first-tap-result-006/result-1.png) 保存實際判定；這些歷史開場試驗沒有取得 source pixel age、穩定相位、input lead／jitter、整首或 AP 資格；最新整首 AP 另由 live011 的結算證據確認。
- [Manual-mode](M7_MANUAL_MODE.md)：25 群／1,311 個檢查通過；舊錄影 4,789 幀的一輪起訖與 3 秒 paused 裝置短測保留。兩次實際只讀 run 各完成一開始／一確認結算、code 0／failure null；002 開場舊 result latch 後人工 retry 才開回合。001／002 分別 7,410／8,110 rows、consumer skips 718／0、source gaps 14／5；直接 PLAYING→STARTING 回退 97／1，current receipt-to-process age 最大 46.3443／9.9833 ms，但 cadence 最大間隔為 479.8721／594.0041 ms。這不是受控比較；002 中途另跑 default build/test。002 foreground 137 次成功、stop joined；其後無 owner poll expiry-gap 修正的 build003 有 31 foreground checks／16 CTest 離線通過，沒有第三首裝置 run，也不由 002 回溯取得資格。兩輪均 arrival-only；來源像素 age、同程序多輪、連續 playing／歌曲時鐘與遊戲輸入資格未取得。原件與獨立摘要見 repo 外 `m7-manual-live-round-{001,002}`／`m7-manual-live-round-audit-{001,002}`；來源 SHA 與原 binary 在報告分列。
- [M6 證據分析器](M6_EVIDENCE.md)：25 項 Python unittest 通過；缺少 Android MotionEvent 記錄會回報 incomplete，沒有跨未映射的 QPC／Android 時鐘計算輸入延遲。

以下保留初次 extraction 及較早接續的歷史驗證範圍。

環境：Linux；GNU C++ 14.2.0；CMake 3.31.6。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror"
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/rhythm_memory_tests
```

結果：default targets 成功編譯；3/3 CTest entries 通過。`rhythm_memory_contracts` 中的 28 項具名檢查通過，另外兩項是 Phigros / Project Sekai ID 的合成 dry-run。

涵蓋：身份必填／hash 字串格式、接點生命週期、重複 ID、時間順序、NaN／越界座標、pixel 邊界、容量超限、同時 release/reuse、仿射映射、rate／overflow／input lead、取整坍縮、租約到期邊界、epoch／geometry 不符、不確定度與預設 disarmed。

注意：兩種 game ID 使用同一組合成接點，不是兩個原生 adapter 的測試；格式合法的 hash 不代表真的核對過資產 bytes。測試程式不使用會在 Release 被移除的 `assert`。

## 初次抽取時未執行／尚未具備

- Windows/MSVC、保留 PAS source 的 fresh build、optional gRPC overlay build、舊 core GTest：NOT RUN。
- 原生譜面 parsing、真實譜面 golden test、audio capture/實際同步、native game geometry、PAS bridge：NOT IMPLEMENTED。
- 手機／emulator 操作、live touch、長按／滑條／flick 判定、真實延遲或分數測試：NOT RUN。
- GitHub Actions 遠端 CI：本輪沒有設定或宣稱已執行。

本報告只對新的 portable contracts 有效。不得據此宣稱原系統已重建、兩款遊戲已可玩、AP、零 Miss、固定毫秒精度或 live qualification。

## 2026-10-23 本機接續驗證

環境：Windows 11 10.0.26200、Visual Studio 18 2026、MSVC 19.50.35729、Windows SDK 10.0.26100.0。

- 全新 default configure／Release build／CTest：3/3 通過；未設定 vcpkg toolchain 或第三方 package prefix。
- `RHYTHM_BUILD_PAS_CORE=ON`：初次編譯 `pas_adb` 因未宣告 `nlohmann/json.hpp` 依賴失敗；補上 CMake package discovery、PUBLIC link 與 optional vcpkg feature 後，`pas_core.lib`、`pas_adb.lib` 全新編譯成功，該 build 的 3 項離線 CTest 通過。這 3 項不測 ADB library 的裝置行為。
- JSON dependency 使用本機已有的 nlohmann-json 3.12.0 cache，獨立安裝至 scratch prefix；未改全域環境、未下載或建置 gRPC。cache tar SHA256：`4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187`。
- `tools/inspect-local-assets.ps1` 在 PowerShell 7.6.5 與 Windows PowerShell 5.1 的暫存 fixture 通過：已註冊／缺註冊／缺 image、缺 SDK tool、存在與不存在的 recording root、PNG 計數與 JSON 輸出。真實本機唯讀盤點可辨識 3 份 AVD，其中 `phigros` 註冊路徑相符。
- 補回的 4 份歷史文件 blob 與基準比對；原 `EXTRACTION.json` 列出的 PAS／其他保留來源不變。此處保留初次 extraction manifest 的歷史內容，不把它改成新一輪通過證明。

## 本機觀測錄製與工具驗證

使用者隨後確認舊原件刪除，要求 main 若無則重做。已啟動既有 `phigros` AVD 的 read-only instance（不回存 snapshot），確認 Phigros 4.0.1／versionCode 157，並以選單操作重試 Dlyrotz IN13；沒有注入任何音符觸控。

- `tools/record_emulator.py` 與 `record_scrcpy.py` 的 **18 項離線 unittest 通過**，涵蓋覆寫拒絕、容量／時間限制、來源時鐘、PNG 解壓損壞、失敗狀態、取消錯誤與 stalled worker 有界停止。
- gRPC PNG／PCM 已實際短錄；音量為 0/15 時 PCM 稀疏，改為 5/15 後 3 秒取得約 3.010 秒 PCM。server PNG 約 9–12 fps，不作整首主要畫面來源。RGB888 方向另與 PNG／ADB 截圖比較，確認本機為 top-down。
- 使用完整配對的官方 scrcpy 5.0.1 release（ZIP SHA 見 RECORDING.md），以 no-control／no-playback／require-audio 錄製 H264＋FLAC。首輪因 pre-roll 過長而漏尾，保存為 partial negative result；第二輪從 retry 開始，涵蓋開場至結算，設定上限 180 秒。
- 第二輪 MKV：**60,029,995 bytes**，SHA256 `9ef128e5072eae782d1c4aec373403926bc07af4c6497e9519856e5e2a90bcc2`。PyAV 19.0.2 全部 demuxed A/V packets 解碼成功，4,789 張 1280×720 畫面，無 corrupt frame／PTS 倒退。最大影格 PTS 間距 **143 ms**；H264 為有損來源，不能宣稱原始 framebuffer 每幀完整。
- 音訊為 FLAC／48 kHz stereo S16，8,613,888 sample frames＝**179.456 秒**，非零樣本約 93.97%。音訊 PTS 單調；另匯出的 WAV payload 與解碼樣本一致，沒有補零、剪接或重採樣。WAV SHA256 `da282d7e0c3fa361cf0cd4c677e0376e756c650e5d8069d06c62c9bf42bd18c7`。音訊是整份 capture，不是已校正的 song-only 參考。
- 目視覆核末幀為 Dlyrotz IN13 結算；0 分／584 Miss 是本輪刻意沒有點擊音符的觀測結果，不是新 autoplay 實戰成績。
- analyzer 另以真實短片、silent FLAC、精確 sample／plane padding、planar 交錯、格式拒絕、末幀 ordinal 與 WAV payload 做離線核對；不依賴 NumPy 或 Pillow。

仍未測／未實作：patched C++ gRPC fresh build、retained PAS GTest、實際 touch capacity／輸入延遲、原 7,722 PNG 的原件驗證、原生譜面／錄影記憶 importer、歌曲時間錨點、自動音訊同步與 chart-driven autoplay。新錄製與舊資料分開保存，沒有繼承舊 qualification。
