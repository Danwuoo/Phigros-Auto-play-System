# Glaciaxion HD 階段驗收與新 task 交接

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
