# 持續開發目標：Chapter Legacy 全曲 IN 解鎖與 Miss=0

啟動／更新：2026-10-05。起點：`a53a9b7021bcc900f498047f5cc017b42e4711b0`。

**狀態：10/6 新雲端候選已完成必要冷工程、三配置與同來源 3063 幀回歸，可交接 Windows 同機資格驗證；成本／physical／live 仍 NOT_READY，產品目標未完成。** 這是 repo 內開發紀錄，沒有建立系統 Goal、automation 或裝置 task。配套：[機讀狀態](legacy-in-zero-miss-state.json)。

## 唯一完成條件

在當前遊戲版本、已核實完整Chapter Legacy曲目分母下：

1. 每曲IN已解鎖，狀態有對應畫面／證據。
2. 同一凍結候選逐曲完成IN，結算Miss=0，原圖、manifest及執行身分可配對。
3. 最終驗收包含實際遊戲／裝置測試；合成、單曲、平均改善或雲端全綠都不能完成本Goal。

P/G/B與score照報，不加AP要求。先前提出的每曲連續3次是穩定性提案，**不是已核准的額外完成門檻**。

## 10/6 e9e305e 後雲端 readiness 里程碑

以指定 main e9e305e 獨立續作，核原 accepted receipts 與 f07 compact payload，不重套舊 patch。
修成本 false READY、shared Hold renderer、own-ACTIVE body／typed terminal-only 接縫，以及
第一次 uncertain release 保留。三配置各 1339 checks 加 Flick case 通過；真實 3063 幀
完整保留 own Down55／Move121／Up51、最終 release verified 與 active0。

合成固定 family 的 B 每配置 24 次正常尾端完成；真圖前綴 typed-tail 與 normal-tail Up 為 0，
合法機會分母 unknown。未放寬門檻、未改 frozen oracle、未把 old36fail／STOP 改綠。
新的 production C++ API 已接入共用唯一 owner，UNQUALIFIED 入口在任何裝置I/O前拒絕；
expiry-first 固定反例已修。新 source 尚未 MSVC完整production build／實際鏈A/A成本／
啟用validator／live CLI／physical資格驗證；沒有裝置讀取或 touch。
完整 [結果](../research/live-readiness-20261006/RESULTS.md)、
[四層 readiness](../research/live-readiness-20261006/READINESS.json)、
[待手動移交 Windows prompt](../research/live-readiness-20261006/WINDOWS_HANDOFF.md)。
後續歷史段落保留原當時狀態，不能取代這個最新 checkpoint。

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

10/6前一輪派送已撤回，改由本 task 自行開發。source774f3e7完成有界診斷防護及
独立原 PNG audit；三配置113項前綴、32項診斷、256幀／1,665取樣核驗，兩個竄改控制
被拒絕；首次程序查核 STOP 保留。使用者僅確認3030可見 Hold head前緣一筆，
1519物件身分、合法機會與實際接觸採納仍 unknown；沒有更改策略或凍結 oracle。
詳見 [自行開發驗收](../research/zero-miss-20261006/windows/WITHDRAWAL_ACCEPTANCE.md)。

使用者之後另授權新的GPT-6.1 Sol／xhigh task開發，由總控依「驗收」完成獨立review。
source `e26231e` 的隔離終端量測、typed no-action seam已簽收；全新Release重建，
三配置重跑56項契約與113項前綴，256幀原圖核1,910新取樣及1,665舊取樣，三個竄改
控制被拒絕。191筆終端仍proposed、eligible=0。worker3 STOP及總控1 STOP原件保留，
不重開舊STOP；正式接觸、全鏈成本與實機仍NOT_READY。原36fail及timeouts/skips本次
未重跑、未刪除。詳見 [總控獨立驗收](../research/zero-miss-20261006/windows/FRONT_CONTROLLER_ACCEPTANCE.md)。

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
