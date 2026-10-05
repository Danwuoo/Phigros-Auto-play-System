# X11-P-R2 開發與自驗結果

2026-10-03 Asia/Taipei。**交付完成，待總控獨立驗收；not-ready，不進X12。** 已完成既有raw量化、成本方法／容量admission負結果及active stress覆蓋契約。新正常成本A/A0、ABBA0；三筆事前固定OS concurrency（B0 Release→B1 Release→B1 Debug-ASan）後停止，沒有追加第四筆或修改noise門檻。

## 分析與成本停止

[protocol](RUNTIME_X11_P_R2_PROTOCOL_20261003.md)在新stress前保存／SHA freeze；[既有raw分析](RUNTIME_X11_P_R2_RAW_ANALYSIS_20261003.md)與existing-raw-analysis-final.json保存全12run／11組重算分布、order ranks／固定窗口／輸入負載／skip／tail／全release與archive內容。原四noise fail及aa-owner-1 coverage fail維持，未排除startup/late或重评gate。

原normal owner owned實際114/119/119/118，p99都取第二大；RGB取第三大。重要尾值分散於中尾段，不能說都是startup。原ASan四份dense128-target snapshot在owner_start已超100ms freshness；唯一fresh為body-only，不能新Down。這是來源與raw支持的生命週期解釋，不是JSON／OS因果分解。無OS trace、writer/enqueue逐sample raw及有效獨立樣本數。

提出principal measurement n≥2000、256warmup＋2304measurement／run、原normal負載／全JSON／lead35與原noise/hard gates保持的有界方法。合成高值反例證明rank敏感度改善，不保證實機noise。八筆AA沿原最大run檔長投影228,019,240B>134,217,728B，未包括候選、失敗或工程；沒有已驗壓縮collector可作完整分母capacity保證，**成本admission拒絕，normal runner也明確拒絕執行**。不縮樣本、不刪warmup、完整diagnostics或尾值以湊門檻。長窗方法仍是未執行設計，不能說性能問題已解。

## 工程來源與驗證

main HEAD仍f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c／main／唯一worktree。正式src/include/root CMake86檔SHA前後核對；原dirty170檔，僅允許status與forward plan追加本輪狀態，其他168保持。未commit/push、建chat/goal、傳訊其他task、啟動emulator/ADB/manual-session/真觸控/live/模型或新PNG replay。

R2是離線工具bundle：連未改的frozen R1 B0/B1 Release及B1 instrumented Debug-ASan pas_core.lib/headers；**未新建或重build runtime pas.exe、沒有core export/source差異**。原57 src/include inverse仍只有pending8行hook，suppression OFF；observer/association/ownership/lease/grace均未改。38個首stress前source/binary/method bindings與562個實際CL/link/header/compiler inputs另freeze；source-snapshot逐檔複製且核hash，未展開junction。舊305/1580/376/2059綁定重核，最後再核；R1完整recursive runtime/DLL/provenance/profile沿用引用，未冒稱新live來源。

| binary | SHA256 |
|---|---|
| 未改R1 runtime B0 |42789d73abad89cfb3ab3c9732770804d4aa18e39a15d63e6f0fb7e4b85c9d09|
| 未改R1 runtime B1 |ea2a3e45889e099321b00a41b1b3f83ee6c35bf13a16036c756519f5cfaf841a|
| R2 B0 Release active meter |0999b8c9310bf033df34d4ba63faae9d346912e619f61f551cbe61477aab925f|
| R2 B1 Release active meter |247e2eac3261052ca17eca86aa9d095cfcc9e03f6dcd972e6ff7c3f25f7c01a9|
| R2 B1 Debug-ASan active meter |920a2c8c90197a559e8a9f8365276ff4e43b98b7def1bfa47cbbb944eca9621b|

profile引用R1 x12-profile-preview.json，SHA d9644832d3aa39c608f95aa22562803cf5516de0f5f99c1286c067870c186228，仍lead35/uncertainty30/fast/RGB888 top-down/256KiB；靜態capability不是device preflight。原live61ec…未列compiled SHA仍Unknown，不繼承遊戲結果。環境Windows11 10.0.26200/Core Ultra5 125H/14C18T/32GB級/MSVCv145/QPC10MHz，power/priority/affinity defaults，background load uncontrolled；完整env/commands/exit保存。

| 實際自驗 | 結果 |
|---|---|
| 新R2 FakeClock/order/coverage契約 |B0/B1 Release及B1 ASan各7/7；另保存首B0初验7/7|
| 未改原suite重跑 |B0 238/238；B1 237pass/1預先聲明pending-grace fail|
| 未改pending契約重跑 |B0 8pass/5預期red；B1 Release/ASan各13/13|
| 未改R1 suite重跑 |B0/B1 Release及B1 ASan各10/10|
| 兩版新meter公共bridge |各512-input streaming byte/row/EOF與舊bridge一致，contacts0，無30MB副本|
| 新C++ raw audit |原12run及新3run各11組分布、全receipt/release/archive內容與frame時間順序吻合|

11套正式收集XML全部skip/error/disabled0；已知fail名字及assertions原樣保存。新tests驗跳過開始400ms仍可接入、fixed phase邊界、body不能bootstrap、100ms stale拒絕、unsupported body不Move/續證據、unknown Down不重試，以及zero receipt/缺requested release/錯contact/缺body witness/unknown release皆coverage fail。ASan插樁新自有meter/tests＋既有自有core；第三方未全面插樁，未跑ASan真PNG／整個manual-session。

## 三筆OS並行的完整active覆蓋

固定typed stimulus：head [0,800)ms／body[800,1800)ms／≥1800ms gate-off。256attempt／cadence8ms+jitter／consumer16ms／fakeRPC3ms／writer5ms，一target／五指上限。可見窗口及deadline完全按QPC-relative test clock，不讀收據，不延長lease或放寬unknown Down。它是測試當前支持adapter，不是observer準確率或遊戲current evidence驗收。三筆都經原shared Wake／SessionGameOwner／full JSON／SessionArchive。

| 新run | attempts/published | consumed/owned | consumer/decision skips | Down/Move/Up | body active witnesses | release calls/requested IDs | events | run physical file bytes |
|---|---|---|---|---|---:|---|---:|---:|
| B0 Release |256/256|69/69|187/0|1/24/0|32|14/1|237|260,466|
| B1 Release |256/256|69/69|187/0|1/24/0|33|13/1|237|261,413|
| B1 Debug-ASan |256/256|68/68|188/0|1/24/0|33|13/1|234|259,289|

共768attempt/published、206consumed/owned、562consumer skips、0decision skip/pooldrop；75 receipts＝Down3/Move72/Up0，40release calls／3requested IDs。failed/unknown receipts/release IDs、exit contacts、worker/archivefault、debugdrop/discard全0。每run contact/pending peak1（不宣稱128targets與active聯合覆蓋），同contact0；gate撤銷release收尾而非tail scheduled Up，故Up lateness n0是未覆蓋。ASan無報告，實際33份body active witnesses，不再以零contact的exit0當coverage。

708 full archive rows逐內容／EOF核對，logical443,083B／physical443,791B，差708個CR；保持native logical limit。queue item peak100/100/98，byte peak60,005/61,244/59,503，均有界。全receipt/release含空集合保存。每frame有body_current/active_contacts_after_poll/contact id，新audit獨立重算coverage全分母與same-contact證據。

| run（全部ms） | recognition n/p50/p95/p99/max/jitter | owner n/p50/p95/p99/max/jitter | capture→owner n/p50/p95/p99/max/jitter | lateness n/p50/p95/p99/max/jitter |
|---|---|---|---|---|
| B0 Release |69/30.9171/31.7409/32.0034/32.0034/1.7232|69/.0118/.0373/.09/.09/.0339|69/46.4119/47.9785/60.6175/60.6175/14.8673|25/.0594/.2007/.2049/.2049/.1599|
| B1 Release |69/30.9938/32.0922/32.485/32.485/2.3932|69/.0121/.0338/.1379/.1379/.03|69/46.5146/47.68/61.3901/61.3901/16.4586|25/.0553/.2689/.3666/.3666/.2407|
| B1 Debug-ASan |68/30.9645/31.8832/32.0202/32.0202/1.9683|68/.2644/.7947/2.2524/2.2524/.7022|68/51.4168/60.9127/69.8199/69.8199/13.2896|25/2.4054/5.2923/12.7103/12.7103/3.8647|

其餘publish／Down/Move/Up／fake injection／release／capture→injection／enqueue／writer n/p50/p95/p99/max/jitter、failure/drop、time order／capacities逐run全存summary及active-raw-analysis。fakeRPC指定3ms但實測p99=15.6594/15.9544/18.4836ms，consumer指定16ms實際recognition p50約31ms；這是實際QPC觀測，不把配置sleep當量得延遲，也無OS因果trace。所有stress只判concurrency/memory覆蓋，這些latency不得當Release成本通過、改善或ASan性能比較。

## 保留、容量及終點

本輪量測工程修復0、failed build0、額外cost/stress/full replay0。兩次pre-freeze raw audit建置：先完成原raw分析，後加入time-order assertion及新active模式，初report/log保留；所有freeze後EXE/core皆未重build。git diff source對照exit1是預期差異；B0 pending及B1原suite exit1是聲明契約差異，未改期待。

新增measurement/out根都是runtime-x11-p-r2／out/x11-p-r2；campaign起點8,238,244,597B，預留總控16MiB並保留free5GiB。新raw根≤128MiB/out≤3GiB、campaign+prior≤8GiB；source/log/fixtures/build全部計額。**最終exact bytes/逐SHA以final-summary.json與artifact-ledger.json為準**，包含ledger自身及final-summary；不是copy舊近似數字。原資料不刪，不建立live根。X12保持最新6,530,531,328B保守reserve，不把512MiB logical冒稱physical hard cap或新增1GiB runtime cutoff。

**Verified：**raw/order分母、source/inverse/bridge、各限定tests、三筆實際active同contact/釋放／ASan範圍與完整events、容量admission拒絕、normal0。**Strong inference：**R1 ASan stale dense＋fresh body-only足以解釋無可執行接入；新的持續head fixture在已測slow consumer條件取得所需覆蓋。**Hypothesis：**JSON/OS競爭造成noise／gap，長窗能改善主機重現性。**Unknown：**聯合128-target active stress、OS trace、主機有效獨立樣本數、正常候選成本、整個manual-session／真capture/RPC/裝置採納、原live未列compiled SHA、14lost利弊／77Miss／跨曲效果。

下一決策是總控獨立驗收此有界工程與not-ready，若仍要成本資格，另立可保留≥2000樣本且含完整正式診斷的collector容量方案或明確新預算；本包不自行擴額、重跑或進X12。C36h tint1仍behavioral/experimental baseline、main50 donor/control，X10b否決、X10d-O獨立。
