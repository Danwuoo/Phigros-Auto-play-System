# 持續開發目標：Chapter Legacy 全曲 IN 解鎖與 Miss=0

啟動／更新：2026-10-05。起點：`a53a9b7021bcc900f498047f5cc017b42e4711b0`。

**狀態：10/6 Windows 離線接手及橋接里程碑完成，停在真圖語義／合法機會與完整成本資格；產品目標未完成。** 這是repo內的開發目標與進度紀錄；本輪沒有建立或啟動新的系統Goal、automation或裝置工作。配套機讀狀態：[legacy-in-zero-miss-state.json](legacy-in-zero-miss-state.json)。

## 唯一完成條件

在當前遊戲版本、已核實完整Chapter Legacy曲目分母下：

1. 每曲IN已解鎖，狀態有對應畫面／證據。
2. 同一凍結候選逐曲完成IN，結算Miss=0，原圖、manifest及執行身分可配對。
3. 最終驗收包含實際遊戲／裝置測試；合成、單曲、平均改善或雲端全綠都不能完成本Goal。

P/G/B與score照報，不加AP要求。先前提出的每曲連續3次是穩定性提案，**不是已核准的額外完成門檻**。

## 10/6 本機接手里程碑與授權

依使用者本機交接 prompt 核驗 ZIP、DELIVERY、SHA256SUMS，在獨立 branch／worktree
只套 cumulative patch，交付 tree 與 v3 七份 core SHA 精確相同。原 main、使用者
dirty 修改、證據、frozen inputs、oracle 與 STOP 保留。Windows 建置及回歸、隔離
current ROI／all-lines 橋接及正式 owner／scheduler 的自身 receipt 前綴驗證已實作；
詳見 [Windows 收據與限制](../research/zero-miss-20261006/windows/README.md)。

原 3938 assertions 三配置仍 36 fail、沒有新增原套件失敗；前綴 113/113；256 張
原真圖前後 SHA 相符，卻没有 eligible action。Hold 真色與 frozen blue predicate、
current front 表示存在可核落差；沒有人工合法物件分母，不能以冷測或安全拒絕驗收。
正式 BVI runtime hook、latest-frame／journal／RPC 壓力及有效 A/A→A/B 成本尚未完成。

本 prompt 授權本地實作、離線測試及本地 commit；沒有授權裝置讀取、Fixture 或遊戲。
下一步先對既有少量真圖進行可追溯語義 review，保留 proposed／unknown，再另 freeze
必要候選／typed adapter。需要裝置時先列指定裝置、目的、命令、風險、停止條件，
取得相應授權；安全及成本 gate 未成立不進遊戲。

## 10/5 雲端持續範圍與工作路由（歷史）

- 能在雲端安全完成的研究、必要程式修補、契約、正反例、整合／FakeTouch回歸及有界成本查核持續做；小型可恢復失敗修復後重跑，不為每個小階段再停下等批准。
- 每一組改動小範圍凍結、獨立覆核並階段性本機commit。舊frozen inputs、oracle、失敗、STOP與歷史source保留；新契約要有可觀測依據，不改gold追綠。
- 最新停止條件：開發到實機證據成為下一個必要門檻時，完成必要cold regression與完整交接包即停止。由使用者自行交接至Codex task；本雲端工作及主對話均不代為建立本地task、不安排或執行實機，也不等待裝置上線。
- 不以「所有想得到的雲端問題都修完」延長本次委託；依可驗證的下一個必要門檻決定交接，而非省略必要冷回歸。
- 不自行push／PR／merge、付費運算、修改系統安全／遊戲存檔／裝置設定，不建立定時automation。

## 10/5 雲端現況與活動順序（歷史）

最終候選cold v3在c2dda1db9fe95b46ff103300d43583857130cd8f，只整合於隔離研究目錄。原3938斷言仍36fail（原54修18、新失敗0）；原契約339、原獨立79、新relation74／contact167／line-ambiguity16／獨立101及另5controls三配置通過。沒有新實戰，Chapter Legacy目前分母與IN解鎖仍unknown。詳見[第四階段](../research/zero-miss-20261005/round4/README.md)。

本輪已完成1–4並在真圖／Windows必要門檻停止；後續由使用者自行移交，沒有活動中的雲端開發或自動裝置工作。原工作順序與後續目標保留：
1. 有界stationary新接入與exact duplicate ROI語义，保留重播／未知Down／distinct物件反例。
2. 斜交contact與effect可見性契約；以真像素支持和安全／合法機會雙分母決定修補。
3. 選定候選的合法current ROI／lines橋接、owner／FakeTouch回歸及成本；不把研究fake attachment當正式ownership。
4. 準備並驗證最小Windows／真圖／裝置測試包、自足接手prompt及可套用commits/patch；完成後先留存，只回報完成，不自動發送交接或附件。由使用者之後取用並自行移交Codex task；本次雲端委託到準備完成為止。
5. 產品Goal在後續Codex task仍需凍結遊戲版本與全曲分母、處理必要HD解鎖、同版逐曲IN結算實機驗收；未完成前不標產品達成。

遇到只有裝置才能回答、規格無法由已授權證據裁定、或需要額外權限時，記明最小阻塞與可並行工作。不得把等待、未跑或未知改成完成。
