# Glaciaxion HD 階段驗收與新 task 交接

## 最新動作修正（source59c92bf／planner9）

使用者補充四／五指及持續接觸語義後，新增五指profile、實测容量門控、Hold當前可見尾端過線後正常Up，以及連續Drag覆蓋相同contact。Release／Debug／嚴格ASan各142／142；當前Native Fixture四／五指各30次、五指全部取消30次通過，新capability `max_contacts_verified=5`、五指profile preflight指紋相符。完整設定／hash／分布見[動作語義紀錄](GAME_ACTION_SEMANTICS_20260927.md)及[metadata](GAME_ACTION_EVIDENCE_20260927.json)。Phigros已帶回前景，沒有新遊戲輸入；使用者準備PLAY後才啟動下一輪HD。observer29仍T0、不接模型，新Hold／Drag遊戲效果尚待驗證，灰色漏辨／身份重生仍未解決，HD AP未通過。

## 追蹤／資料開發續作（source b32517d）

本輪在桌面 `codex/tracking-segmentation-pilot` 完成共享候選T0/T1/T2比較、單worker shadow與有界原生ROI／mask／QA／export；Release／Debug／嚴格ASan各138／138通過。真實input仍為T0／observer29／planner8，diagnostics3，不接模型。完整pipeline／line tracking與正式替換資格未完成；結果與限制見[追蹤報告](TRACKING_COMPARISON_20260927.md)／[資料報告](VISION_DATASET_20260927.md)。不merge／push，未恢復4c3c，未宣稱舊ignored資料已恢復。

三輪由使用者逐輪手動準備PLAY後完成，均185秒STOPPED／exit0，同source／binary／lead35／兩指，真實input均T0。run後綴17904941035800460／17904949475779425／17904951892485298：結算345／6／0／42、348／3／0／42、336／6／0／51，分數811,985／815,153／787,303，未AP。shadow無backend／skip0／fault空，54clips408ROI（dataset384＋diagnostics24）、3run已保存，5 proposed masks／human review0。source最大空窗795.0122／324.6703／472.1912ms，不能將分數差當新tracker效益。合併C++QA通過，但2064對跨run近似候選需group review，全部development／training_ready=false。已停止此批遊玩，下一步為獨立標註／去重及困難例分析；200–400經核對ROI尚未達標。不再向原討論task來回回報或自動導航。

## 桌面 main 續作（v75，2026-09-27）

使用者已恢復開發，舊暫停與禁止合併 main 限制已由新授權取代。main 起點為合併提交 `09dc0d0`，本次程式修改在 `C:/Users/wurre/Desktop/Phigros-Auto-play-System`。observer29／planner8／diagnostics2 修正與新增三項回歸見開發紀錄 v75。固定35ms／兩指；再次實戰前仍需完成當前三配置回歸並由使用者手動準備 Glaciaxion HD 選曲頁，保留 PLAY。

v75三配置已完成：Release／Debug／嚴格ASan各113／113，6.67／13.29／33.03秒，日誌在桌面`measurements/hold-transition-20260927/`。使用者授權啟動模擬器、手動選曲後回覆「已就緒」；當前capability指紋及1280×720／rotation1預檢通過。同一binary已以`configs/phigros-hd-assist-lead35.json`／185秒／`--no-preview --keep-diagnostic-anomalies`完成一輪，run `cpp-observe-17904849394349323` STOPPED／exit0。結算圖核對 **356 Perfect／1 Good／0 Bad／36 Miss，833,295分、90.75%、MaxCombo65、Early0／Late1**，未AP。PLAY與Note由C++即時pixels，曲中無build/test、第二capture或CU擷取；目前已停止，沒有開始下一輪。

使用者指出線抖動等情況容易中斷，希望加入輔助技術。下一步研究見[視覺輔助與時間追蹤](VISION_ASSIST_RESEARCH_20260927.md)：第一Hold診斷1025主線穩定、Note61缺一幀但未即刻Up；第一combo窗口1193有約1.56px線偏移，中央Hold身份87／92交替、各自最後證據約62／63ms後Up；灰色Hold的PNG1208單張重算沒有target。不能把所有Miss歸因於抖動，也不能直接延長missing期限。先補有界多幀像素／身份診斷與追蹤對照，再評估小型分割模型；目前未實作／訓練新模型或tracker。

歷史資料異常：本次開始時確實讀到原 `C:/Users/wurre/.codex/worktrees/4c3c/Phigros-Auto-play-System` 的 v73 events／manifest／診斷PNG，隨後該專案路徑消失，`git worktree list` 亦不再列出它。此 task 沒有呼叫封存、移動或刪除工具。已向使用者詢問歷史 measurements 的新位置；尚不知是否另有備份，不能假稱全部證據仍可讀。後面以該路徑記載的歷史資料不可直接視為現存。桌面的 `measurements/touch-cpp-full-run3/summary.json` 仍存在，SHA256 `315d738cf2917da84fdbf60afbc2e6afe7e69615c2055c9f01c56d74fb7246da` 與 v73 capability 一致；仍須啟動時核對當前裝置指紋。

使用者後續允許必要時從封存恢復4c3c。已核對其Git快照ref `refs/codex/snapshots/985e4b7595454947183d269d79cba72068d1a611` 指向1b7b05a，已完整包含於main；本輪不需要舊checkout，未恢復。該snapshot没有measurements檔案；沒有宣稱舊ignored證據已恢復。新run的原始events、結算manual／PNG、兩張診斷及離線分析均在桌面。

## 續作停止點（v73 實戰後，使用者重啟電腦）

2026-09-27 使用者要求「跑完這輪之後先休息，我要重啟電腦」。目前已停止，沒有下一輪 assist、capture 或 build/test。模擬器導覽改由使用者自行操作；恢復後請由使用者準備 Glaciaxion HD 選曲頁並保留 PLAY，主程式再自動 PLAY。不要自行恢復 computer-use 導覽。

- 新實戰 v73（observer28／planner8／diagnostics2）run `measurements/game-assist/cpp-observe-17904707957978302/`，185秒 STOPPED／exit0。結果 **328 Perfect／16 Good／0 Bad／49 Miss**，790,229分、86.11%、MaxCombo60、Early0／Late16；HD AP 未通過。PNG 及 result-manual.json 已核對，不能再以 v71 當最新結果。
- 分析：`measurements/g0-preflight/live-assist-lead35-v73-analysis.json`；結算 PNG：`measurements/g0-preflight/assist-lead35-v73-result/diagnostic.png`。hash與完整統計見開發紀錄最後 v73 段落。
- 5個未Down取消中，Note232／859／860／876後續以新intent80／272／273／281由新畫面重排且有一次Down；Note923沒有同ID新接受。這只證明本機排程行為，不證明遊戲命中。
- 最早combo消失frame565之前，frame554 Note30完整Hold與Note33內部重建前緣重疊，555兩者association_ambiguous，intent5提前Up。兩張診斷PNG與事件時序保留。下一步需重現造成重複候選的當前pixels條件，再最小修正；不要直接放寬歧義、接觸期限或按住時間。
- v74／v74b／v74c 的連通body＋中央warm矩形合成案例在舊observer28即通過（121／218／309ms），**尚未重現實機失敗，正式邏輯未改**。最後版本保留三個遮擋位置與單身分／單Down的正例；移除臨時stderr列印。110項中的新增短測已通過；原109項三配置驗收照舊，沒有宣稱全套110項已跑過。
- 再次實戰前不用重開擷取選型，也不要重跑無關矩陣。先解決真正反例；新production修正才做對應回歸及實戰。暫停期間不自行啟動下一局。

日期：2026-09-27。使用者要求核對 Perfect 339／Good 6／Miss 48，
驗收後換新的 task 繼續。這是 Phigros 專案交接，不涉及 IRIS-X 或 Kaggle。

## 驗收結論

**實戰證據有效，HD AP 未通過；繼續 HD，不進 IN。**
本輪驗收沒有操作模擬器，也沒有新增實戰。

已獨立檢視結算 PNG，與 `result-manual.json` 一致：Glaciaxion HD Lv.6，
802,570 分，339 Perfect／6 Good／0 Bad／48 Miss，Max Combo 68，
Accuracy 87.25%，Early 0／Late 6。共 393 個判定，仍有 54 個非 Perfect。
數字屬 v71（observer28／planner7／diagnostics2），不能當作 v73 的成績。

- 原始 run：`measurements/game-assist/cpp-observe-17904671758582395/`。
- 已重新計算 `events.jsonl` SHA-256：
  `fab1de7c3e9ac24beca5114b9fcdbccf030808d0c7af8bd069ca96699da6f782`。
- 結算圖：`measurements/g0-preflight/assist-lead35-v71-result/diagnostic.png`。
  已重新計算 SHA-256：
  `b3b3a3a6418255bfc88f792bdc0e0477187f8ad251780a41c2ab8c8d4d1d8d04`。
- 原始設定：35ms lead、兩個 contact、185 秒，退出 STOPPED／0。
  個別 Miss 成因、第三指需求與絕對 source age 尚未由這組統計證明。

## 待交接程式

工作區為 `C:/Users/wurre/.codex/worktrees/4c3c/Phigros-Auto-play-System`，
分支 `codex/phigros-live-runtime`。驗收開始時 HEAD `9c8cb22`，另外六個
未提交檔案包含 v73 修正、測試與紀錄；均為應保留的開發內容。
既有 `measurements/`、build 及設定需沿用，不另從桌面 main 重建舊版本。

檢查 `GamePlanOwner`、runtime 接線和新增回歸後，v73 的範圍是：
最新 pixels 明確否定預測時，取消 cursor=0 的未執行 Down，保留新的有效
預測重排機會。已注入／已完成的 Down 不重播，active Drag 的原視窗保留。
四種 Note × 三種否定原因、新預測恢復，以及分析事件不冒充觸控均有回歸。
舊 planner7 的反例失敗日誌仍保存於
`measurements/g0-preflight/test-assist-v72-before-fix.log`。
沒有在此次程式審查發現阻擋該修正交接的問題；遊戲收益仍待新實戰驗證。

## 新 task 的工作順序

本次以現有 v73 source 重新建置並驗證：Release、Debug、嚴格 ASan 各
**109／109 通過**，CTest 時間分別 9.79／15.52／46.04 秒。
`ASAN_OPTIONS` 未設定；第三方 DLL 未插樁的既有限制仍保留。
日誌為 `measurements/g0-preflight/acceptance-v73-build-{Release,Debug,ASan}.log`
與 `acceptance-v73-test-{Release,Debug,ASan}.log`；`git diff --check` 通過。
這只驗收程式修正與回歸，不宣稱 v73 已改善 HD 或達成 AP。

1. 閱讀本交接、AGENTS、主程式計畫與開發紀錄最後 v71–v73 段落。
2. 保留此次驗收的 code／版本／測試與原始實戰證據；不要把舊成績改名成新版本。
3. 沿用 observer28／planner8、35ms lead、兩指及現有通過的裝置指紋，
   先完成一輪有界 HD 實戰。每輪只改一個可解釋原因；沒有新畫面就不補按鍵。
4. 核對結算、pending cancellation、contact conflict、gate/source expiry，
   找可重現的剩餘缺口；取消數量下降或 RPC 成功不能代替遊戲判定。
5. 每次時序修正先重現失敗，再補短回歸、Release／Debug／ASan；build／tests
   停止後才做正式實戰，遊玩中不加入第二擷取或 computer-use 畫面擷取。
6. 首次結算確認 HD AP 才轉 IN；不新增歌曲腳本、譜面、音訊節拍或預錄操作。

上一 task 因 Escape 停止該輪操作；本次使用者要求新 task 繼續。
新一輪須從當前可見畫面與有效裝置條件開始，不沿用舊 UI 假設；
任何新的停止訊號立即停止。PLAY 與 Note 注入仍由 C++ 即時 pixels 路徑完成，
少量導覽依既有使用者授權及當前工具能力處理。禁止自動合併桌面 main 或推送。
