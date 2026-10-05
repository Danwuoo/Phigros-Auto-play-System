# X11-P-R2 既有 R1 raw 分析

2026-10-03。成本run=0：本頁描述原12run，沒有改寫R1 gate、窗口或summary。C++20 reader延用總控 audit 的有界JSONL讀取／quantile核對，重核11組raw metrics、逐receipt/release、archive全內容/EOF與時間順序。完整機讀為新R2根 existing-raw-analysis-final.json；首次報告亦保留。各run所有14組原summary metric（包含未獨立重算的writer/enqueue）、全部skip、最高八尾值、最後四attempts與固定capture窗口均在報告內。

## 樣本與 order statistics（Verified）

nearest-rank p99為ceil(.99n)，表中upper含該rank本身，不是獨立樣本數。

| 原run | attempts | consumed | owned | owner p99 rank/upper | owner p99對應attempt | capture→owner p99對應attempt |
|---|---:|---:|---:|---|---:|---:|
| aa-owner-1 |128|124|114|113 / 2|78|99|
| aa-owner-2 |128|126|119|118 / 2|81|115|
| aa-owner-3 |128|126|119|118 / 2|77|77|
| aa-owner-4 |128|128|118|117 / 2|91|78|
| aa-rgb-1 |256|255|255|253 / 3|64|209|
| aa-rgb-2 |256|255|255|253 / 3|75|221|
| aa-rgb-3 |256|256|256|254 / 3|206|255|
| aa-rgb-4 |256|256|256|254 / 3|51|201|
| stress-owner-B0 |64|64|45|45 / 1|59|64|
| stress-owner-B1 (ASan) |64|58|5|5 / 1|1|64|
| stress-rgb-B0 |64|18|15|15 / 1|26|46|
| stress-rgb-B1 |64|18|17|17 / 1|6|30|

owner四run capture跨度1021.37–1026.92ms，RGB4085.17–4097.78ms。owner p99對應capture offset615.082–724.443ms，capture→owner p99在615.082–916.1137ms；RGB capture→owner p99在3201.2406–4077.8278ms。故這些noise-sensitive尾值並非全部在開頭100ms。aa-rgb-4的owner最大值確實在51.7769ms，但p99在815.1133ms；aa-owner-1的最大owner值在91.4013ms，但p99在633.5008ms。不能以某一個startup max推導全部p95/p99失敗原因。

固定[0,100)、[100,500)、[500,1000)、[1000,2000)、[2000,5000)、[5000,∞)ms窗口全部列n/p50/p95/p99/max/jitter，沒有刪失敗或尾值、沒有重算新的pass gate。原最後frame有owned亦有skipped，全部保存，不挑有利尾端。consumer與decision skips分開；aa-owner-1仍owner114/128=89.0625%，低於90%。其他四项noise原判定保持。

## 負載與診斷邊界（Verified / Unknown）

aa-owner-1的124 consumed包含77份32-target、47份1-target；其1,347完整事件logical2,043,814B，其中decision1,519,178B、plan423,474B。完整每type bytes與input targets分母在機讀表。Normal owner32、R1 stress128與原X11 owner128各是不同負載，不跨批比較提速。

source呼叫順序是decision full JSON admission→archive push→UI JSON→owner_start→accept→owner_end→plans/receipts/release等drain，再poll。recognition_end→owner_start的可觀測gap包含decision交付等待、兩個enqueue與之前action工作；QPC不能把這段拆成純serialization或OS排程。Archive還有queue扣byte的dump、writer final dump/write及close/hash，meter另有admission dump/計時；正式default沒有這些meter options，但完整production diagnostics成本保持。writer/enqueue三個aggregate分布沒有逐sample raw，不能依本audit声称重算，也不能把stage p99相加。

## R1 ASan 零 active 路徑的較精確定位

**Verified：** 原stress-owner-B1只有attempts1/25/26/49/64被owner處理。1/26/49/64是128-target snapshots，其capture→owner_start分別195.8654/190.9578/189.4011/265.9851ms，全部超過未改core的100ms freshness。attempt25是1-target body-only snapshot，capture→owner_start8.4841ms。對應recognition_end→owner_start的dense gap為192.9317/187.4945/186.6946/261.0996ms。receipt/pending/contact peak仍全部0。

**Strong inference：** 對上述四份dense輸入，core freshness guard本身就足以拒絕新Down；唯一fresh處理份為body-only，不能bootstrap未開始的Hold。source另把唯一Hold head放第一snapshot，之後全body-only；接入資格不耐慢consumer。這把零覆蓋定位到可重現的fixture/current-evidence路徑，不能稱已證實OS或JSON單一根因。

**Hypothesis：** full JSON競爭與OS排程可能解釋dense gap；沒有OS trace或對稱消融，不分配因果比例。較長窗口可能提升重現性，仍待容量可行的實驗。

## 新方法停止線

合成1ms＋固定高值100ms：n128兩個高值即可令p99=100；n2048兩個或十個高值仍p99=1，21個才p99=100。這只驗rank影響，不證明主機noise會降、樣本獨立或性能改善。按protocol每principal metric至少2000樣本／256warmup＋2304measurement attempts，保留全warmup與tail。完整JSON A/A8筆的線性磁碟投影228,019,240B，超過新根134,217,728B；投影不是硬上限，更不足以支持capacity admission。沒有已驗壓縮collector，所以新AA/ABBA皆0，交not-ready；不把更多相同replay當有效性解法。

真capture/OS因果/整個manual-session性能、候選normal差、14lost取捨、77Miss、physical身份及跨曲效果仍Unknown。新三筆active fixture僅驗固定一target的並行接入／延續／release，另見[R2結果](RUNTIME_X11_P_R2_RESULT_20261003.md)，不能倒填這個原R1負結果。
