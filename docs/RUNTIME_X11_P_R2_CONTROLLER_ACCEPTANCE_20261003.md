# X11-P-R2 總控獨立驗收

2026-10-03。**簽收既有raw分析、active並行覆蓋與容量admission負結果；維持not-ready。** 本次不是新的性能否決：R2沒有執行normal A/A或ABBA，pending-only候選成本仍Unknown。X12未啟動。

## 独立驗證結果

main HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、唯一worktree、既有dirty保留。正式src/include/root CMake未改，main metadata仍50/27/11。總控讀R2四份文件、active stimulus/coverage gate、新七項tests、raw reader/analyzer、meter的並行/觀測/收尾路徑、build/test/freeze/stress/finalize scripts，核對原core freshness guard與R1 immutable來源。R2連既有core，沒有新runtime pas.exe；C36h baseline、main50 donor、pending八行、suppression OFF及X10b否決保持。

| 本次實際動作 | 結果 |
|---|---|
| 重hash及檔長 | R1 305 source/binary、1580 compiled dependencies、376 negative-tool inputs、2059 artifacts；R2 38 source/binary/method、562 compiled dependencies、707 artifacts，全部符合驗收開始時綁定 |
| 新R2 tests重跑 | B0/B1 Release及B1 ASan各7/7，skip/error/disabled0 |
| 兩版新meter streaming bridge重跑 | 各512 synthetic/FakeClock inputs；summary bytes符合原freeze，B0/B1 receipts177/127、contacts0 |
| 兩份raw分析重新執行 | 原R1十二run報告及R2三run active報告分別與freeze byte-identical；各11組raw quantile、分母、archive receipt/release及coverage核對符合 |
| normal拒絕驗證 | exit1且run目錄未建立，沒有產生任何cost樣本 |
| 開發11套XML | hash與逐testcase結果核對；本次未重跑未改的原238/pending13/R1十項suite，不把XML核對冒稱重跑 |

沒有新build、cost/stress run、full PNG replay、emulator/ADB/live/模型、commit/push。新ASan tests是deterministic契約；並行ASan那一run僅重算已封存raw，没有重跑。新驗收根 `runtime-x11-p-r2-controller` ≤16MiB，精確commands/XML/reports/bytes見其final-summary；重現脚本 `tools/accept-runtime-x11-p-r2.ps1`。

## 能確認與不能推論的事

**Verified：** 三筆已封存OS並行各1Down/24Move/0 scheduled Up、body active witnesses32/33/33、same contact0、各requested release ID1且exit contacts0。三筆合计768publish、206consumed/owned、562consumer skips、75receipts、40release calls、708archive rows；failed/unknown/fault0。第三筆ASan確有active body延續及gate-release路徑，不再是R1的零receipt。

**範圍限制：** 一target typed fixture、800ms固定head可見窗、其後body窗與gate關閉，不讀收據來延長資格。測的是並行生命週期及memory coverage，不是RGB observer accuracy、128-target與active聯合壓力、tail scheduled Up或正常成本資格。scheduled Up n0屬未覆蓋，不能以gate release替代tail驗收。receipt/Move數不是遊戲命中指標。

**Verified：** 原owner n114/119/119/118的p99取第二大樣本；RGB取第三大。原noise敏感尾值有多筆在中尾段，不能都歸startup。原ASan四份dense snapshots在owner_start已超100ms freshness，唯一fresh是body-only。

**Strong inference：** freshness拒絕與未接入body不能bootstrap已足以解釋原零active覆蓋。新固定持續head fixture在已測慢consumer條件補到覆蓋。這不是JSON、OS排程或timer單一根因證明。

**Hypothesis：** 較長窗或不同環境控制可能提升noise可辨識性。≥2000樣本是預先選定的尾端rank解析度工程目標，不是統計信心保證；21個upper order values不等於21個獨立樣本。不能僅因樣本增多就宣稱noise一定通過。

## 本次停止線合理，但不是長期阻塞理由

原R1最大run實體bytes按2560attempt投影，八筆AA合计228,019,240B，大於本包128MiB上限；完整AA+ABBA同尺度約684,057,720B，還未含失敗/metadata。線性投影不是硬上限或容量不可行的數學證明，但在缺乏可保留完整資料的已驗collector之下，足以拒絕在原128MiB預算啟動。**簽收「本包admission拒絕」，不推論機器不能跑長窗，也不將未執行當candidate performance fail。**

不需要為繼續成本驗證先造壓縮平台。現有full JSON診斷可直接保留，下一包改用明確独立且有界的資料預算。舊8GiB campaign/prior與各freeze維持，不刪raw、不把舊128MiB gate回填為pass。

## 下一工作包：X11-P-R3 長窗成本資格（規劃，未派送）

1. **容量方案先落實。** 建議另立 `measurements/runtime-cost-x11-p-r3`，總≤2GiB（舊campaign+prior仍≤8GiB、兩帳合计≤10GiB）；新build另≤3GiB、disk free保留5GiB。原raw只引用。這是下一包明列預算調整，不是悄悄擴充R2。建議normal24筆各≤80MiB共1920MiB，剩128MiB分給工程/stress≤96MiB與總控驗收32MiB；開始前實測reserve。若需要不同切分，必須在新cost前凍結且總額不增。
2. **補真正的長窗硬界限。** 不能只把attempts改2560：現有receipt/release512、archive timing samples16384、reader rows16384及raw限都須逐項核對，設定有推導的有限上限與超限fail。若需要更大archive旁路統計上限，只在新隔離兩版共用且不改strategy的來源處理，runtime default界限另列，不改frozen core。保留所有warmup、failure、late、完整JSON；LF/CRLF與實體bytes分帳，先用合成邊界測試驗資料完整與容量。不要採樣/丟診斷來假裝降成本。
3. **沿R2長窗規格作一次有界qualification。** 256warmup＋2304measurement attempts、principal measurement n≥2000；R1 RGB16ms/owner32targets8ms、lead35、固定jitter與完整正式共享路徑保持。warmup及四個固定measurement blocks各列分母與分布，hard safety對全run、原noise formula/floors/normal90%及timing gates不放寬。instrumentation與default正式差異明列。source/input/clock/options/環境及配額先freeze。
4. **不再反覆救批次。** normal最多8 A/A＋16 ABBA，另stress≤4，失敗計額、最多一次量測工程修復。新A/A或完整性/硬gate不足即停，不啟動ABBA；沒有額外安靜重跑、不得排尾值。舊R2 active結果可作來源相同部分的引用，變更collector的部分另用有意義tests/ASan覆蓋，不必重造Wake/archive或新body規則。
5. **交付並停止。** SHA/inverse/provenance/完整raw/actual bytes/全部負結果及精確gate供獨立驗收；not-ready有效，但要定位是容量、樣本、measurement有效性或候選差異。即使cost自驗通過也不能自動X12。這是成本資格的最後一個明確方法候選；若仍失敗，先回總控決定是否停止該候選，不能自動R4、R5無限微調。

原live未列compiled SHA、整個manual-session/真capture/RPC、14lost機會利弊、77Miss及跨曲效果仍Unknown。未來合格後的≤6次live仍按原授權與預檢/停止條件；本次沒有新task或live執行。

## 可變文件與freeze紀律

R2 artifact ledger將status及forward plan的**工作目錄路徑**也列為frozen inputs。總控先驗707項全部符合，再將兩文件當時完整bytes保存為本驗收根的 `pre-review-*.md`，SHA與原ledger吻合，之後只追加J28及進度段。原R2 ledger不修改；因此後續對mutable工作目錄兩項的hash會因已記錄的追加而不同，應用保存preimage核歷史，其他705項必須仍相同。這不是可忽略任意hash差異的例外。下一freeze應綁定batch內不可變文件快照，另記當時workspace path，不再把日後正常更新的status當永久不可改artifact。
