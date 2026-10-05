# X11-P-R2 預先量測有效性／active coverage protocol

2026-10-03 Asia/Taipei。僅離線有界工程，自驗後交「交付完成，待總控獨立驗收」。authoritative 為 R1 總控驗收與本 task 工作包；不進 X12，不建立 live 根、chat、goal，不啟動 emulator/ADB/manual-session/真觸控/模型，不 commit/push。正式 src/include/root CMake 與所有舊 frozen sources/binaries/raw 不改、不重建。

## 先驗問題與既有資料（新成本 run=0）

R1 四項 noise fail、aa-owner-1 的114/128全分母 fail 保持原判定，不改 gate 或事後排除任何尾值。原 ASan owner stress receipts0 不具 active coverage。新 C++20 bounded reader 衍生自總控 raw audit，逐筆重算12run 的11組 raw metrics、receipt/release 全內容及 archive EOF；輸出 existing-raw-analysis.json。writer/enqueue 只有原 aggregate，保留它們的原 summary，不聲稱新分析重算該分布。

描述窗口事先固定為 capture offset [0,100)、[100,500)、[500,1000)、[1000,2000)、[2000,5000)、[5000,∞)ms；offset 原點是該 run 第一筆 capture。所有樣本、skip、最後四 attempts 及最高八個尾值保留，窗口只描述，不能重評原 gate。principal metrics 報 nearest-rank 的 rank、上方 order statistics 個數、對應 attempt/time/targets；輸入負載報 consumed targets histogram/sum、每種完整 JSON event bytes，accept間隔與 recognition-end→owner-start gap。後者包含 JSON/等待/先前 owner 工作，無法隔離因果。OS trace、真正 startup 根因及有效獨立樣本數 Unknown。

## 方法改善與成本可行性停止

原 owner n114–119 的 p99 依賴第二大值；RGB n255–256 依賴第三大值。合成固定1ms＋0/1/2/10/21個100ms高值（n114/128/256/1024/2048）量化 order-statistic 敏感度，不能用合成分布保證主機 noise 會通過。較長窗可降低少數樣本的 rank 影響，不能證明樣本獨立、OS 根因或性能改善。

事前待驗長窗設計：normal RGB沿 R1 同16 RGB SHA、16ms cadence；normal owner沿 R1 同32 targets、8ms cadence；兩版 lead35/uncertainty30/enabled15、jitter{-2,+1,+2,-1}ms、完整 Wake/SessionGameOwner/SessionArchive/full JSON 路徑。每run256 warmup＋2304 measurement attempts，共2560，principal recognition/owner/capture→owner 各 measurement n≥2000（p99 上方至少21個 order values；不是21份独立證據）。所有 warmup/raw/fail/late照存，all-phase hard safety仍對整run評；measurement與warmup各自報完整n與quantiles，不平均 p99，不剔除不利尾值。每run固定四個576-attempt measurement block供描述，不挑最佳block。原90%全分母、capture→owner p99≤100/max≤250ms、lateness p99≤15/max≤100ms及 R1 noise formula/floors/.30 adequacy 都保留；不足任一分母就 fail。

**本包不執行新 A/A/ABBA：成本方法容量資格停止。** 既有 run 實體檔長投影（不是硬限）：owner 最大2,238,175B/128attempt，四筆2560attempt為179,054,000B；RGB 最大1,224,131B/256attempt，四筆為48,965,240B。單是8筆A/A投影228,019,240B已越新根128MiB（134,217,728B），尚未包括候選/失敗/工程/驗收資料，不能保證保留完整分母。即使線性投影非因果或硬上限，它也不能支持容量 admission。沒有已驗的有界壓縮／去重 collector；不刪 full diagnostics、warmup、late、不縮樣本後沿用新推論、不隱藏meter成本救 gate。這是具體 not-ready 交付的停止線，不改 R1 結果。未啟動的長窗只是可審查方法設計，非已實作成本驗收。

原配額上限8 A/A、16 ABBA、4 concurrency/stress，失敗計額；本包具體預定0 A/A、0 ABBA、**3 active owner concurrency runs**（B0 Release→B1 Release→B1 Debug-ASan），沒有 RGB stress重跑。第四筆保留且不自動使用。stress在候選正常成本之前但已看候選memory/coverage資料，不能聲稱候選完全未見。最多一次量測工程修復，保存失敗；若新 active coverage 不成立，停止该驗收、不追加 stress找成功。

## 固定 active stimulus 與覆蓋契約

沿既有 meter，只在 test adapter 用固定 song-independent typed current snapshots，不用 recorded touches/oracle。QPC origin 為 run 初始化 begin；[0,800)ms 持續可見 Hold head/rails，每份 current snapshot 用 capture+35ms root、evidence=capture、expires=capture+100ms；[800,1800)ms 可見 body/rails、無root；≥1800ms gate=false/無目標。角度a=.7×elapsed_seconds，位置(400+60sin(a),500−30sin(a))，同note1/line7；不按收據或owner進度延長啟動窗、可見性或截止。僅一個target，避免把接入只放在第一snapshot；不與R1 normal32targets比較速度。

每run256attempt、8ms cadence+jitter、三物理frames/latest1/decision1、consumer16ms、fakeRPC3ms、writer5ms、五指上限；所有 lease/grace/unknown Down/owner守門仍為連結的 frozen R1 core。正常情況有約800ms接入機會；如果OS/ASan慢到錯過或lease失效，coverage fail，不能保證成功而放寬策略。排空100ms後finish，真正OS Wake並行/記憶體用途；ASan不作Release latency。沒有fakeclock latency主張。

coverage 必須且只允許：exactly1 Down、≥2 Move、0 scheduled Up（本fixture以gate撤銷收尾）、≥2份body-current snapshot在poll後仍有同一active contact、全部receipt與release requested id為同contact、exactly1 requested release ID、failed/unknown=0、exit contacts0。完整 receipt_n/down/move/up/release_n/requested/failed/unknown 全分母匹配；零receipt/僅exit0/aggregate peak不通過。每frame保存body_current、active_contacts_after_poll、active_contact_id；archive保存full events與offline coverage。

先 FakeClock 契約：跳過初始400ms仍接入、固定phase邊界、body不能bootstrap、stale100ms拒絕、unsupported body不Move/續證據、unknown Down不重試；gate負例驗零收據、缺release分母、錯contact、缺body witnesses、unknown release都拒絕。原 pending suite、唯一已知pending-grace fail期待、原 R1 suite/public streaming bridge及source inverse保持。新B0/B1 tests及B1 ASan先通過，再freeze才進3筆OS concurrency。重用既有Core/Wake/archive，不造第二runtime。

## 診斷成本、容量與来源

保留原meter admission dump、archive push byte-accounting dump、writer queue-accounting dump/final JSON dump、完整plan/receipt/release。旁路計時與default正式仍有差異，明列；沒有去除診斷工作來稱策略優化。接既有 frozen B0/B1 Release與B1 instrumented Debug-ASan pas_core.lib、原headers，**新runtime pas.exe不存在**；沿用R1 runtime SHA與profile references，新的meter/tests/audit EXE另freeze，不能繼承live效能/語義。formal strategy與唯一pending8行delta都不改。

硬界限沿meter：每runframes257、receipt/release各512、archive aggregate samples16384、每event≤256KiB、admission≤6MiB、mailbox8192/16MiB、native32×16MiB logical segments；frame row≤1022B、receipt/release≤510B。按admission最多2倍CRLF＋raw/meta保留，3stress各≤16MiB physical raw/meta/journal，超限停止並保留失敗；其餘工程/報告/source/log/XML/ledger≤16MiB，新batch總≤128MiB。新out≤3GiB；old campaign+prior仍≤8GiB，原PNG只引用、不Copy展開junction。開始實測8,238,244,597B；free206,277,783,552B。事前reserve新batch128MiB＋獨立總控16MiB，out3GiB與另free5GiB；最終逐檔實體檔長另重算，allocated clusters Unknown。每次run前核容量與freeze，所有失败也計額。

X12最新磁碟規劃保持R1總控更正：512MiB logical journal限，journal每attempt1GiB physical reserve＋standby8MiB＋metadata2MiB＋result4MiB，6attempt共6,530,531,328B；future12GiB根/另5GiB free，不新設1GiB runtime cutoff，不在本包建live根。

## 交付及分類

protocol/result/handoff、既有raw分析、coverage正負例、commands/exit/XML/ASan範圍、所有引用core/runtime/profile及新工具/dependency SHA/inverse、raw完整n/p50/p95/p99/max/jitter/fail/drop/skip/release、機讀final-summary及exact capacity ledger。status追加J27及forward plan写「交付完成，待總控獨立驗收」。Verified/Strong inference/Hypothesis/Unknown分開；14lost取捨、77Miss、真採納與跨曲仍Unknown。not-ready與工程交付可同時成立，不自簽總控、不進X12。
