# 曲庫與視窗測試面板

## 本輪已實跑並封存

最新build006已由使用者實跑：18個回合、14輪完整輸入、8輪原生確認結算；正式Legacy有8首保存結算AP，另有Verrückt暖身AP。6個完成輸入未確認結算的回合及剩餘未測歌曲已列入[完整封存紀錄](SESSION_RECORDS_2026-10-24.md)。使用者要求先停止修正並commit／push；本次整理不新增實測。

以下保留各階段建立時的條件、NOT RUN與限制，不能當作目前仍未實跑；當前狀態以結果索引與MODEL_HANDOFF為準。

## 連續 UI 逾期修正：panel004 候選

build005 的 Glaciaxion 兩輪（360接點）及 Eradication（226接點）連續收到有效畫面，但 Hold 邊緣接到分數，分數檢查分別連續27／29／22張拒絕；pause／IN prefix持續吻合。最大尾段處理約2–5ms，並非500ms傳輸中斷。[逐輪時序稽核](../../local-assets/phigros-4.0.1/chapter-legacy-repeated-stop-timing-audit-001/README.md)與[Eradication像素稽核](../../local-assets/phigros-4.0.1/eradication-ui-stop-audit-001/README.md)保留原件，原guard約500ms後釋放，kernel最後為零。Verrückt同一binary完整完成並確認結算。

新state005明確啟用learned-title-redundancy-v1；原HUDv3及結算標籤政策保持不變。每輪開場由兩張來源有效、實際v3完整HUD通過、不同序號且相隔至少60ms的影格，建立並交叉核對目前pause／完整IN等級／有界標題字形參考。不能從未知畫面或輸入後學習，不要求首音等待它；未取得參考時原v3路徑照常。參考及支持像素／SHA／實際receipt會在釋放後保存，不把模板時間當成新的UI觀測。

只有輸入已armed、當下v3唯一prefix與pause吻合、只缺score、實際score邊界相交且仍有至少四個觀測到的合理字形，才可用當下影格的三個輔助glyph及負空間維持HUD。暫停／結算／歧義denial仍優先，來源／epoch／幾何改變使參考失效；不延長500ms／100ms／20ms，也不填造分數字形。這是有版本的額外像素假設，不識別曲名／成績、不證明來源渲染時間。

另一次Glaciaxion首5接點失敗是獨立的20ms動作遲到：HUD仍有效，owner處理間隔約27.6ms。既有資料不能細分OS／配置／像素工作原因。新候選將48個開場RGB緩衝區與64MiB日誌預先配置／觸碰，將反覆last-frame拷貝改成最多兩個immutable lease、pool有界增為5格，並加入原有capture／pixels-ready／publish及owner階段時戳。期限不放寬，改善與跨輪穩定性仍待實測。

使用者已關閉所有程式；準備期間不得啟動emulator、測試或觸控。重啟入口將在使用者執行後核對新boot／路徑／helper／零接點，在明確測試階段取得新的五指fixture證據，再產生新的面板config。不能改舊boot字串冒充重新驗證；遊戲選曲與Play仍由使用者操作。

## Verrückt AP 與結算記錄修正：panel003

panel002 的 20261024-190641 回合已由保存的原始結算圖獨立確認 Verrückt IN16 AP：1,000,000分、1285 Perfect、0 Good／Bad／Miss、Max Combo1285、100.00%、Early／Late0。1285 DOWN／788 MOVE／1285 UP 共3358筆回執逐項對應原編譯計畫，最後kernel零。[獨立結果](../../local-assets/phigros-4.0.1/verruckt-result-review-001/result-observed.json)與原圖／來源／binary雜湊均封存；只證明這個build004指定回合的AP。

原生manual_result_confirmed=false與exit2仍保留：結算模板未認出1285連擊的結果頁，180秒round watchdog最後以manual_round_ended_without_result_confirmation停止。逐像素核對顯示主要是半透明背景亮度差；三份實際結果的Perfect文字bounds相同，沒有數字寬度造成位置移動的證據。AP沒有因記錄故障而消失，原始失敗也不被改寫成原生成功。

新state004保留六份原模板與全部RGB bytes，再加兩份由既有Dlyrotz／Glaciaxion結果參考取得的五個固定標籤：Max Combo、Accuracy、Good、Bad、Miss。新shared-result-labels-v1使用128灰階caption形狀、局部對比、反向多餘墨跡與有界共通位移；不讀數值、不推導AP。容錯特徵只在全曲接點已送完後使用，再經原manual-mode三張不同receipt／至少60ms確認；原HUD、100ms錨點、500ms UI、20ms遲到、停止與釋放條件不變。

panel003將輸入、結算辨識、觸控釋放與分數複核分列；輸入已完成但結算未知時顯示明確原因及保存影格，仍停止佇列。結算確認後保存result.png，分數保持待獨立複核；不以RPC成功或exit0宣稱AP。新build005／panel003實機尚未重跑。已取得的暖身AP可保留，下一輪可取消Verrückt勾選，從Legacy20首開始；選曲與Play仍由使用者操作。

## 最新修正：panel002／HUD v3，尚未重跑實測

使用者兩次Verruckt測試均為0 DOWN／UP、沒有anchor。1291／1277張有效UI影格全卡在STANDBY，尚未執行首音detector；最後由使用者停止，kernel皆為零。根因是IN Lv.16文字的筆畫位置與抗鋸齒相位，並非首音模型已判錯。舊panel001與兩回合原件保留。

新桌面捷徑改用song-panel-002、song-library-002、native build004。HUD shared-in-subpixel-prefix-score-v3先保留v2精確路徑，再以1px白色支持、四方向五字形、負空間／反向墨跡／位移族檢查處理字形相位；分數v2及100ms錨點／500msUI／20mslateness未改。

面板現在將READY標為「程式備妥／尚未觸控」，以實際原生訊息區分等待遊戲、HUD尚未通過、等待首音與正在演奏；不從主機經過時間推論開始觸控。取得首音前保留最多24張實際candidate影格，100ms取樣，停止／釋放後才寫PNG，避免下一次無輸入時缺少原像素。

Production C++255筆回歸與Python全相符，保留190個v2正例、55個嚴格負例拒絕。兩張Verruckt實際prefix通過唯一族[-2,-1]；較早分數被遮擋仍維持unknown。實際首音／新build完整遊戲尚未驗證，不能以此宣稱已送出觸控。請關閉舊面板，再從桌面開啟新版本。


使用者指定由自己操作遊戲選曲／Play，程式在同一介面接續有限曲目；準備完成後不自行啟動實測。

本機曲庫現在有21個IN候選：額外暖身曲Verruckt，以及正式Chapter Legacy20首。暖身曲不寫入原生章節名單，也不改20首順序。原始譜面／音源／校正與實測紀錄仍在repo外。

## 使用方式

桌面「Phigros Test Panel」捷徑使用已確認的bundled PowerShell完整路徑，隱藏終端並開啟視窗；開啟本身不啟動任何實測。

1. 預設勾選Verruckt＋20首，按「開始佇列」即依序測21首。若只想單獨試Verruckt，選取該列後按「只測本首」；之後要只測Legacy20，可取消Verruckt勾選再開始佇列。
2. 面板先核對所有相關原件及選取曲目的原生離線前檢。
3. 在遊戲選好目前這首IN，按面板「已選曲，啟動本首」。
4. 面板顯示READY之後，再到遊戲按Play。偵測到首音後顯示演奏中；輸入結束後等候遊戲結算。
5. 結算／釋放證據核對完成才可按「下一首」；在遊戲選下一首，再重複上述流程。分數仍待原結果獨立複核。

「停止」會送出當輪STOP檔並等待原生程序釋放／結束；關閉視窗也採相同流程，不強制殺掉可能仍持有觸控的程序。故障會停止整批，不自動重試、選曲或按Play。「開啟紀錄」會開啟本次目錄，保留ledger及每輪stdout／stderr／原生manifest。

原生100ms錨點、500ms實際UI receipt、20ms lateness、前景／epoch／幾何／實際容量及release-all條件不變。主機計時顯示只用於介面，不當作新的來源觀測或同步資格。

## 實作與資料邊界

`tools/sweep-panel.ps1`是Windows WinForms前端。背景C#程序橋接以ArgumentList傳遞Unicode／空格路徑，隱藏原生console，使用有界佇列與日誌，不由背景PowerShell delegate直接操作表單。啟動後READY／首音取得／輸入完成由實際stdout事件顯示；原生manifest的曲目、來源、binary、結算與kernel-zero需相符，不能只憑exit0視為成功。

檔案驗證只依已知且實際消費的schema處理相對路徑，不遍歷任意歷史provenance。等待選曲期間若原件改變，啟動前再次核對會拒絕。STOP不會搶先建立原生輸出目錄，避免破壞NEW目錄契約。

`tools/build_song_library.py`以正式20首plan與額外歌曲descriptor產生新的本機library。七個離線測例包含21項分組／原生順序、來源與profile不符、變動原件、重複曲目及既有輸出保留；不會連裝置。正式Chapter Legacy資料來源仍以原生章節proof為準。

## Verruckt

Native ID為`Verruckt.Raimukun.0`。IN原件包含1285 notes（837Tap／398Drag／50Hold／0Flick），完整候選3358 events、peak3，首DOWN9.075630252s，末UP117.763092437s，音源121.233719s。使用同一套capacity5、D7000、inset64、Drag32ms、原Flick設定及HUD v2政策，沒有刪音符或逐曲放寬門檻。

原生EZ273notes與HD621notes另存，共用一份音源；只有IN建立測試候選。[入檔／來源說明](../../local-assets/phigros-4.0.1/verruckt-ingest-audit-001/README.md)及[獨立來源時間稽核](../../local-assets/phigros-4.0.1/verruckt-independent-audit-001/report.json)保留原始雜湊。實際開場／HUD／遊戲判定及完整成本回放仍為NOT RUN。

面板離線SelfTest、offscreen DrawToBitmap預覽及hash／狀態機替身驗證，不等於操作遊戲或證明21首實機通過。新build003的HUD修正與先前004中止紀錄仍見[全測計畫](CHAPTER_LEGACY_TEST_PLAN.md)。

## 本次封存與驗證

舊版001封存：本機入口在song-panel-001，程式副本／config／曲庫／native build均固定雜湊；桌面捷徑已建立但未執行。GUI純離線SelfTest11/11、獨立QA72/72，同一source SHA3eb991a99e5e4908fa2468fe0406fbd569694d2ee4d61a6053c48ca47480ff1d。root另經同一C#隱藏程序橋接逐項執行21個check-adaptive，全部stdout身分／exit／input_enabled=false核對通過；沒有建立實測輸出目錄，也沒有裝置探測或觸控。

全Python114/114；本輪portable configure/build/CTest4/4。前次原生build00326/26整合結果保持原範圍，GUI沒有重建或更改binary；gRPC source重建、實際表單點擊／關閉中的裝置生命周期及21首新實機均NOT RUN。等待選曲／結果時直接關閉目前會保留最後waiting狀態與原生紀錄，不假稱整批完成。
