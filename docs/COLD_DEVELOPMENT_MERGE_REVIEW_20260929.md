# 冷開發合併驗收（2026-09-29）

使用者要求驗收原task「非學習式跨曲能力與延遲冷開發」並合併main。本次審查以[原計畫](NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md)及[開發結果](COLD_DEVELOPMENT_RESULT_20260929.md)為準，範圍是observer47／planner23／diagnostics11的冷開發。未啟動emulator、ADB、真觸控、manual-session或模型訓練。

## 審查結論

冷開發整合驗收通過，可合併本機main。審查發現一項冷測量器資料競爭並已修正；遊戲observer／planner策略沒有因本次審查改版。Release／Debug／ASan各310／310通過，修補版1,000幀冷鏈完整消費、Down／Up各83、失敗／未知receipt皆0且收尾零留指。合成矩陣、既有RGB、非學習式工具與受控冷鏈是本次接受範圍；實景長Hold、遊戲觸控採納、Flick有效速度、Dlyrotz恢復、HD AP／zero Miss仍未驗收。

## 程式與證據審查

- 核對當前像素、line／note身份、關聯、局部時間擬合及owner之間的邊界；重點檢查Hold同contact續接與tail、已確認關係不可跳到剩餘錯線、pending Down在新快照缺Note時撤銷，以及已執行／未知Down不能復活。
- 核對逐列預檢與原掃描條件：採樣五連續pixel對應原最多四pixel缺口，必需區塊數是合格長run的保守下界；既有完整RGB的語義相等是另項驗證。16線／128 Note、五指、latest-frame、QPC與單owner契約保留。
- 開發交付的`candidate47-23-final-provenance.json`於審查修改前112筆SHA全數相符，包括86個source/config、7個binary、16份關鍵證據與3份測試log。修補後來源與binary另記`measurements/merge-review-20260929/provenance.json`，不回填歷史hash。
- coverage的32列是G1–T2及R1–R7正反代表案例，不是32種涵蓋全部排列的保證。validator只查契約／證據存在，不能代替實際測試，因此另重建並重跑整套回歸。
- 30clip／90幀的proposed標記再次驗證；753個索引frame、736個unique SHA再次從raw重播，候選數及完整語義決策相對交付版均零差異。contact sheet僅作目視抽查，沒有人工gold或實景precision／recall宣告。

## 審查修正

`src/game_cold_pipeline.cpp`原capture worker在`LatestFrame::publish()`返回後寫`Sample::published`，perception worker可能已被喚醒並讀取同欄，沒有同步關係。現改從持有的不可變Frame lease讀`published_ns`；該值在LatestFrame發布鎖內寫入。publish呼叫成本仍由producer寫獨立欄位，僅在threads join後讀取。

新增`GameColdPipeline.ConsumedPublicationTimesComeFromSynchronizedFrameLeases`以48個抖動frame走實際離線三執行緒鏈，核對所有消費紀錄的capture≤publish≤recognition、分母及收尾釋放。此測試加上鎖／記憶體所有權審查支持修正，不把ASan當ThreadSanitizer，也不聲稱用測試證明不存在所有race。

CMake的compiled-source hash清單同步補上冷鏈實作及Journal source/header，避免修改這些路徑後建置指紋未涵蓋。以上修正不改觸控策略／到期契約，不改既有baseline37／38或凍結效能meter。

## 驗證

原始重跑輸出保存在`measurements/merge-review-20260929/`，均為本機離線資料。

| 檢查 | 結果 | 證據 |
|---|---|---|
| 交付版Release完整回歸 | 309／309通過 | `ctest-release.log` |
| 修補版Release完整回歸 | 310／310通過 | `ctest-release-final.log` |
| 修補版Debug完整回歸 | 310／310通過 | `ctest-debug-final.log` |
| 修補版ASan完整回歸 | 310／310通過 | `ctest-asan-final.log` |
| 代表性coverage | 32／32、pending/failed/unknown皆0 | `coverage.json` |
| 既有RGB重播 | 753幀，語義差異0 | `replay-review-diff.json` |
| proposed validator | 30clip／90幀，gold_eligible=false | `proposals-validation.json` |
| observer37／38基準保留 | 149筆SHA全相符 | `frozen-baselines-verified.json` |
| 原凍結Tap ABBA重算 | passed | `tap-ab.json` |
| 原凍結dense ABBA重算 | 每批加速failed；原非退步規則通過 | `dense-ab.json`、`c5-gate.json` |
| 修補版冷鏈執行 | 1,000／1,000幀，83 Down／83 Up，failed/unknown=0，零留指 | `pipeline-smoke.json` |

最初修補建置與舊test process重疊，曾因Windows exe占用出現LNK1104；等待舊測試退出後順序重建與310項回歸通過。舊309項重跑沒有算進修補版驗收，失敗log保留，未改測試期待掩蓋。

審查派生資料帳本為`capacity-ledger.json`，約7.1MB、硬上限64MiB；沒有複製原始RGB。最終來源／binary與重跑證據hash見`provenance.json`。編譯於提交前的dirty tree，不能單用base commit重建；應用保存的source hash與本次提交内容核對。

## 效能主張的範圍

核對原A/A容忍檔、同stimulus hash及每批ABBA輸入後，重新執行C++分析器，原C5跨場景規則仍passed：Tap的recognition及capture→owner p95/p99超過預先凍結容忍下降；dense未通過額外的逐批加速規則，不能稱全場景加速。各次原始run仍保存n、p50/p95/p99/max及drop／失敗。

AB分析器的`baseline_mean/candidate_mean`是同一場景同批兩次run之分位數的算術平均，**不是合併樣本的p95/p99**，也不是跨歌曲平均。本次不將這些值冒称新binary的精確分布；更不能推論真gRPC或render-to-game-effect。歷史meter的`published_ns`有上述race，因此不採其發布欄作精確時間證據；capture→recognition／owner直接分布的公式不使用該欄，但舊程序有race仍是其方法限制。修補版另跑冷鏈核對正確時間序與釋放，沒有以單次smoke重新宣稱三批效能驗收。

## 合併與保留範圍

審查起點main與`codex/main-legacy-pixel-clips`皆為`baf3d4fcbaac56ab085e19b9fba8a5ef6615d3bc`。本次提交包括尚未提交的非學習式開發、資料工具／回歸、使用者形式文件／計畫，以及先前已授權並有[清理紀錄](CLEANUP_AUDIT_20260928.md)的舊文件／legacy移除；不是本次重新刪除原始量測。合併採保存來源分支commit後fast-forward本機main，不推送remote。

`measurements/`與`out/`維持ignored且留在原地，Git合併不等於備份這些raw。保留兩輪歷史及七輪新實戰、37／38配套基準與凍結效能meter。下一步仍是使用者另行安排的有限HD manual-session；不能因merge自動啟動熱測。
