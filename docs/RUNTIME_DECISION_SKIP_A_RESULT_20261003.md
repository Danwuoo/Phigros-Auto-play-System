# 選項 A：既有 R3 193 次 decision skips 定位結果

2026-10-03 Asia/Taipei。**選項A交付完成，待總控獨立驗收；成本資格仍not-ready。** 本包完成有限既有資料診斷，沒有新 runtime/cost/stress/OS 量測或 gameplay。主要結果是到達群聚與 accept 外邊界可確認；193 份因果歸因全部仍 Unknown，沒有硬湊單一根因。

**值得由總控選擇有界的選項 B「量測契約審查」**：有具體的 selection/publication 缺口與 RGB anchor 命名差異需要處理。這只支持審查價值，不支持直接修方法、降低90%、重跑 R4 或進 X12。若不能補最小 publication/selection 觀測，現有 raw 對 JSON/lock/wake/OS 的識別性已到停點，應停止此線，由總控考慮獨立 X10d-O；本包不執行它。

## 可確認的邊界與保留的 Unknown

原始 `aa-owner-1` 2560 全發布；2475 consumed、2282 owned，**85 consumer skips＋193 decision skips**，保持 2282/2560=89.140625% 與原90%差22次。193次全都有前後 consumed/owned 可追溯 row，沒有首尾 censor 導致漏分類。

| owner decision skip 的實測完成位置 | 次數 | 可以說的話 |
|---|---:|---|
| recognition_complete 在前 owned accept 開始前 | 142 | 完成不是 publication；其中136份 publication 上界也在 accept 開始前，另6份仍可能跨這個邊界 |
| recognition_complete 在前 owned accept 結束後 | 51 | publication 下界晚於 accept 結束；純前一次 game.accept 無法包住這些 skips 的 availability |
| recognition_complete 在該 accept 閉區間內 | 0 | 沒有這種完成時刻 overlap；不等於 action 沒有其它工作或當時 mailbox 尚未更新 |

來源給 `P_i∈[recognition_complete_i,recognition_start_next_consumed]`，`S_p∈[max(recognition_complete_p,owner_end_previous),owner_start_p]`；所有 row 均列這些 bounds。最後一份 publication 上界不能補猜；本193份都不是最後一份。publication residency 的 duration 下界是下一 consumed 的 recognition duration，上界是再下一 consumed start−本次 complete；它们只是界限，不是精確 actor trace。

額外 **exploratory** 收緊：6份 skipped decision 連下一份 replacement publication 上界都在前 owned accept 開始前。因此 source 的 latest1／同 mutex selection 順序將它們的整個 residency 機會界定在前 snapshot 已 selection 到 accept 開始之間。這個區間包含 decision_json 建構、兩次 enqueue 及未測的排程／等待，不能把6份直接稱為 JSON caused。沒有任何一份整個可能 residency 區間被 game.accept 完整包住。136／51是 publication 相對 accept 的可證界限，其餘6份對該 publication 邊界仍 Unknown；不是193份互斥根因。

## 群聚很常見，與 skip 有關但不充分

先驗 cutoff 固定 owner≤4ms、RGB≤8ms，不按結果調整。owner capture 第一至最後跨度 **20,484.0891ms**，不是 summary elapsed24,707.0129ms。capture gaps2559份有1238個≤4ms；consumed recognition-complete gaps2474份有1157個≤4ms。scheduled sleep_until 的 absolute wall origin／實際 due 沒有保存，不能由群聚證明 OS timer 或 catch-up 根因。

| 同 run owner 的下一 consumed recognition gap | n | ≤4ms | p50 / p95 / p99 / max ms |
|---|---:|---:|---|
| skipped decision→下一 consumed | 193 | 191（98.96%） | .3774 / 1.5449 / 4.0557 / 4.9120 |
| owned→下一 consumed | 2281 | 966（42.35%） | 13.7310 / 16.3093 / 17.5026 / 38.4799 |

全部193含2個非cluster反例；全部owned亦含966個cluster反例，所以群聚不是 skip 充分或必要條件。skipped-consumed連續群：178群長1、6群長2、1群長3，共185群／193份。按 raw attempts 的連續群另存，不能和 consumed streak 混用。

matched control 規則先驗為同 run、前 owned 同 warmup/block、同 phase、同 targets，最近 previous attempt，tie取較小者，可重用。185個含 skip owned edges 全有匹配。額外 exploratory 將每個 skip arrival 對該 matched edge 的下一 owned arrival（193對、重用），下一 recognition gap 的cluster為191/193對133/193；下一phase、capture gap、未觀測 scheduling未控制，不是獨立193對因果 A/B。

四RGB consumer/decision skips=15/1、15/0、17/0、7/0，確為低decision-skip對照；不同 cadence／RGB recognition／typed owner 負載不能隔離因果。RGB唯一skip1218的complete亦在前accept結束後、下一recognition gap2.7857ms≤8ms。其餘RGB owned next-gap clusters=203/2543、192/2544、203/2542、201/2552，群聚同樣非skip專有。

## action accept 外區間的可量下界

相鄰 owned p→q，`[owner_end_p,owner_start_q]` 內沒有其它 game.accept。source 順序將 p anchor 的最前 decision/lifecycle 兩項之外所有 enqueue，及 q 最前兩項 enqueue，放在該區間內。可以求它們的 elapsed duration 和；不能給每個 event 絕對時刻。該和含 archive mutex 等待與 admission／push dump，**排除在 lambda 進入前的 decision_json 等建構**，不是純 JSON CPU。完整落在區間內的 fake receipt/release calls 另加，與 enqueue 不重疊；writer 平行 duration 不加入。

| 全 owner consecutive owned edges | n | p50 | p95 | p99 | max | p95−p5 ms |
|---|---:|---:|---:|---:|---:|---:|
| accept 外 gap | 2281 | 10.9362 | 17.0449 | 20.4721 | 34.0114 | 16.6996 |
| contained enqueue elapsed 下界 | 2281 | .7805 | 3.0805 | 5.1196 | 13.1465 | 2.9769 |
| 未被上述enqueue/calls包住的 remainder | 2281 | 9.3668 | 15.9922 | 17.6738 | 32.1829 | 15.8292 |
| game.accept（另一組，不相加） | 2282 | .0815 | .2648 | .5715 | 1.9225 | .2297 |

| matched185 edges | skip p50/p95/p99/max | 非skip control p50/p95/p99/max ms |
|---|---|---|
| gap | 2.5085 / 15.8749 / 19.1390 / 19.3596 | 12.2675 / 16.4715 / 24.7376 / 26.7481 |
| contained enqueue | 1.0035 / 3.6181 / 5.0442 / 5.0901 | 1.0447 / 3.5261 / 6.2579 / 8.0512 |
| remainder | 1.2312 / 15.0960 / 16.6776 / 18.5048 | 10.4785 / 15.3931 / 20.4020 / 24.9850 |

skip gap 並非普遍更長、enqueue 中位亦未更高；這反對把 skips 一律說成長 action stall，但不排除短 residency 被正常服務時間遮過，也不證明插樁沒有影響。全部 matched quantiles／jitter、所有2281 owner edges與其它run edges保留；沒有用成功subset或加 stage p99。remainder可能包含正常等待下一frame、drain／poll、JSON建構、mutex、Wake及OS scheduling；不是「OS耗時」量測。

## 固定分母

| owner window | attempts/published | consumed | owned | consumer skips | decision skips | complete前accept／後accept |
|---|---:|---:|---:|---:|---:|---|
| all | 2560 | 2475 | 2282 | 85 | 193 | 142 / 51 |
| warmup | 256 | 244 | 230 | 12 | 14 | 11 / 3 |
| measurement | 2304 | 2231 | 2052 | 73 | 179 | 131 / 48 |
| block1 257–832 | 576 | 559 | 516 | 17 | 43 | 37 / 6 |
| block2 833–1408 | 576 | 553 | 508 | 23 | 45 | 27 / 18 |
| block3 1409–1984 | 576 | 559 | 512 | 17 | 47 | 35 / 12 |
| block4 1985–2560 | 576 | 560 | 516 | 16 | 44 | 32 / 12 |

五run總194decision／139consumer skips（owner193／85，RGB1／54），12800attempts全包含。四RGB consumed/owned=2545/2544、2545/2545、2543/2543、2553/2553。receipt/release 全row與archive完整內容核對，1515 owner receipts（525Down/466Move/524Up）、5release calls／requested1、26108event timings／archive rows保留，無failed/unknown、exit contacts0。每run原所有 phase/release/event denominators另列原summary值，不換算成Miss。

phase 0–15每層160attempts全報，targets、warmup/measurement/fourblocks皆在機讀row。phase／targets只為離線typed workload，不能寫成正式曲目／進度規則。

## 原 anchor 語義差異

實際 R3 action `seen/current_attempt=DecisionSnapshot.sequence`。owner adapter指定sequence=source attempt；RGB GameObserver則`++sequence_`，只對consumed frame遞增。consumer skip後，event-timings.attempt／release.offline_attempt的 raw anchor不等於source frame。第一次新reader因此於RGB1 event135 raw35拒絕「owned frame35」join；保存失敗後沿source修正。

新 `source-anchor-map.jsonl` 用完整 archive `(sequence,frame_sequence)` 解析每份accepted mapping，event timing仍保留原raw，不補絕對時刻。新報告 event partition 用resolved source attempt；原R3 summary按raw anchor的event/release窗口只引用，**不回改舊ledger、raw、noise、gate或歷史資格**。全窗count/quantiles與owner193問題不受這項RGB命名差異影響。這是值得選項B審查的契約發現，不是R3封存損壞或候選性能退步。

## 可核對時間線

先驗選最早skip、最長skip gap、最高enqueue/gap比例（去重），各附同規則非skip control；共8張SVG。綠為實測accept、橘為publication bounds、灰為accept外gap；小interval顯示至少1pixel，不表示量得較長duration。原時戳與上下界仍以row為準。

| owner example | gap / enqueue / remainder ms | 作用 |
|---|---|---|
| 3→5，skip4 | .5084 / .2821 / .2263 | skip4 publication界限已在accept3開始前，不能用owner_start當selection |
| 881→883，skip882 | 19.3596 / .8531 / 18.5048 | 最大skip gap的enqueue不大；其餘區間不能直接歸OS |
| 979→981，skip980 | 1.4749 / 1.2759 / .1990 | 最高enqueue占比86.51%；只證此gap的服務下界，不證其發生時刻恰遮住skip |

圖及完整control參見[機讀報告](../measurements/runtime-decision-skip-a/deterministic-final-1/report.json)的examples與SVG。RGB1217→1219另列，不能拿一次RGBskip替代owner全分母。

## 證據分級與下一假說

- **Verified**：兩套R3 ledger、51 source/binary/method、575 compiled dependencies、2input bindings與R1 source305核對；完整raw join／SHA／count／順序／denominators；51份publication在前accept之後、136份在前accept之前的bounds；191/193群聚、非skip反例與全matched結果；RGB sequence/source-frame差異。原90%負結果不變。
- **Strong inference**：由同mutex最新selection與單action/perception順序，6份exploratory residency全部落在前一snapshot selection→accept之間；contained enqueue是相鄰owned gap內的elapsed下界。都不能拆成JSON純成本。
- **Hypothesis**：短decision residency與selection後diagnostic服務共同促成overwrite。producer sleep_until在落後時可連續發布的source機制與實際群聚一致，但scheduled due／OS原因未量。
- **Unknown**：193份JSON/lock/wake/OS等causal allocation；各P/S確切時刻、iteration/drain/poll duration、next_due/wake reason、writer絶對時刻；有效獨立n、noise尚未評估、B1差異；真capture/RPC/game adoption、14lost opportunities、77Miss與跨曲。

只提一個下一hypothesis：**前 snapshot selection→accept 的共同診斷服務，比較短的decision residency，是否是主要overwrite區間？** 支持是142份complete前accept、136份publication前accept及6份整個residency前accept；反例／限制是51份明確在accept之後，匹配enqueue中位無增加，群聚亦常被owned。最小缺失觀測為同clock下decision publication linearization與action selection時刻；現有owner_start/end保留。它們能辨明P_i→P_next在哪個邊界，仍不能拆OS/JSON；若完整trace大部分residency落在前accept結束後，則「主要前accept」假說被否證。無這兩個時刻，鄰近completion／enqueue duration不能代填。本包只提案，未插樁、未執行成本。

## 自驗與封存

獨立最小C++20 `apps/decision_skip_a`，不連pas_core、不重build舊out。Release／ASan合成各17/17；涵蓋錯source/SHA、duplicate/reverse/missing、非法時間/Unknown零欄、truncation/row/count、RGB raw anchor、contained service邊界、非識別性反例、匹配與nearest rank。另ASan只讀五run做完整reader/joins/metrics驗證，非新runtime。第三方JSON系統header非全面插樁。

最終兩份分析全部16檔（含每列skip、全edges/arrivals/matches/source-anchor-map/8SVG）逐byte相同；新input/source/binary及396實際CL/link/compiler依賴有SHA，link不含runtime core。首次MSVC字串/JSON C++20 overload build失敗、ASan缺thunk lib build失敗、兩次raw anchor拒絕都保留。初14/14及analysis-1探索版也保留，未刪失敗換配額；preliminary dependency freeze漏讀MSBuild命名前綴的CL tlog，獨立v2補全396項，舊29項留作修正證據，不當完整freeze。

容量／精確commands/exits／input/source/binary/output hashes在[final-summary](../measurements/runtime-decision-skip-a/final-summary.json)及ledger。自驗<48MiB、總控16MiB保留、新build<1GiB/free5GiB；原campaign+prior8,241,831,623B保持，R3初始化189,820,719B、兩舊帳及新資料實際合計<10GiB。file length不冒稱NTFS allocation。status J31及forward只追加且原byte prefix核對，batch文件快照綁freeze。R3原tests/bridge/latency資格及R2 active歷史只引用，沒有重跑。AA5全B0、ABBA0、noise未評估與not-ready保持；C36h tint1 baseline、main50 donor/control、pending-only/suppression OFF不變。交付後停止，不自行開task或傳訊。
