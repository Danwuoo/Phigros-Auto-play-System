# main80 AA／ABBA 完成與修補判讀

2026-10-12 使用者恢復工作：「我覺得你先把剩下的 aa 和 abba 測試一併跑完，再看看我們要修改甚麼」。
剩餘 AA5 與 fresh ABBA36 已全部執行並完成完整 raw audit。AA12 共 11 validity PASS、原 Hold3 一筆 NOT_READY；noise15／18 adequate。
ABBA 的完整 raw 可核對，但時序有負結果，正式比較仍因原 AA stopped／invalid 前提而 REJECT；cost NOT_READY、live UNQUALIFIED。

這輪完成量測與只讀判讀，runtime code changes=0、fresh compile=0。沒有重跑原75／production9、Normal3 或已完成的前七筆 AA。
未啟動 emulator／device／Computer Use、新 thread、ETW recording、模型訓練或遠端 push。

## 來源與保存邊界

實際 binary compiled source 為 `3ee69edc85acb6b9b968424374a376f351d0e2dd`（main80）。
本輪 executor 為 `558fc889e95d49d7539d114b0e76da8dfc05731d`，相對 binary source 僅有 exact seven 已記錄文件變更；後續文件 HEAD 不能冒稱 binary 來源。
開始兩階段前都保留原 loader 的 source／binary／build／motion declaration／controls／source snapshot checks。
maintenance adapter 只把兩處執行 HEAD predicate 明列為 compiled80／executor558；其餘原 SHA 與數值不變。
timed process 的 runtime closure 為 1,145 files，包含全 campaign source 與 bindings、runtime seals／19 imported analyzer DLLs／guard／維護 recipe／identity sidecar。
這是 diagnostic completion，沒有宣稱新 whole-build qualification。

原[AA STOP 盤點](MAIN80_AA_STOP_AND_REST_20261012.md)及其[38 Native8／160 outputs 索引](MAIN80_NORMAL_PASS_AA_HOLD3_STOP_20261012.json)保持原件。
原七筆 AA 只引用；剩餘五筆沿原 frozen plans 補齊，新增 continuation sidecar 明列使用者恢復與 diagnostic scope。
原 stopped status、Hold3 負結果及預先建立的 ABBA user-stop 均未覆寫。
本輪 ABBA 使用不同 fresh campaign／36 個 run IDs；没有把舊 STOP 改為 PASS。

## 實際完成情況

| 項目 | 本輪結果 |
| --- | --- |
| 原 build／controls／Normal3 | main80 原 PASS 身分保留；本輪未重跑 |
| AA12 | 全12完整 raw audit PASS；11 validity PASS、原 Hold3 NOT_READY |
| 新 AA measurements | 5／5；各 producer／auditor／gate Native0、runner0、verification0 |
| noise18 | 原 C++ analyzer 完整計算；15 adequate、3 inadequate；ready=false、comparison_allowed=false |
| ABBA36 | Tap／Hold／Dense 各12，固定三批 A／B／B／A；36 full raw audits PASS；11筆 report-only 必要數值檢視有負結果 |
| 原 formal comparator | Native2／runner2／verification0；`complete unstopped same-candidate AA required`，未產生 comparison.json |
| full readonly／collection preflight | 未新增執行；成本資格尚未通過 |

本輪新增16個 AA／noise Native 與73個 ABBA／comparator Native，共89個；原38與本輪合計127個。
程序退出、raw integrity、numerical validity 與正式資格分開記錄。

## 原 AA Hold3 的精確超期位置

`main80-e1-hold-closed-reset-v2-3` 唯一 publication overrun 在 measurement attempt2336。
同一 attempt 的 start lateness19.6558ms、render0.2296ms，render end lateness19.8854ms，仍未越過20ms producer deadline。
render end 至 publication return0.1485ms，使 publication return lateness20.0339ms，超期 **33.9µs**。
這是同一 attempt 的原始時間邊界，沒有將不同 frame 的 maxima 相加。
render max1.5906ms 與該 attempt render0.2296ms 是不同統計，不能互換。
publication 路徑越過期限已確認；wait／dispatch／OS／guard 或具體函式根因仍 UNKNOWN。

## ABBA 時序及原必要條件檢視

使用原 report＋audit 已核對的數值，逐項重現 frozen `validity` predicates 作只讀風險盤點；這不是另外執行原 C++ `load_run`／run gate 的 PASS。
全36筆中11筆至少觸犯一項原必要數值條件，沒有刪掉失敗、縮小分母或補跑。

| 場景 | A 必要條件有負結果 | B 必要條件有負結果 |
| --- | ---: | ---: |
| Tap | 5／6 | 4／6 |
| Hold | 0／6 | 2／6 |
| Dense | 0／6 | 0／6 |

15／36筆至少記一次 producer 或 publication overrun；合計 producer945、publication949。
Dense 原本未要求 zero producer／publication overruns，其 A3／4、B25／25 的 producer／publication 次數照報，不新增 Dense zero gate。
所有36筆 pool drops0、writer drops／fault正常、injection failed0、safe／release正常。
Tap 有長時間 start lateness（最高1770.799ms）；四筆 capture max260.1672／459.6339／860.7079／1534.3938ms，超過原 hard max250ms。
三筆 Tap coverage<0.9；OS peak RSS 全36筆最高60,207,104B，未碰原128MiB界限。
start lateness、render duration、publication callback 的 maxima 位於不同 attempts 時分開記錄，不能拼成一個根因故事。
945／949次 overrun 可包含一次長空窗之後 absolute epoch 的多筆 catch-up，不能說成同樣多個獨立卡頓。
完整 attempt joins／warmup與measurement分母／contiguous bursts 保存在只讀風險附件；不能由原時序資料認定具體 OS／guard／wait 因果。

| 場景／run／attempt | 同一 attempt 的原邊界 | 可支持的定位 |
| --- | --- | --- |
| Tap A batch2 offset0／2047 | start late1770.799ms；前次 pub return→start1790.5242ms；render0.4377ms；post-render→pub0.1375ms | 長窗已在 render start 前；後列沿原 epoch catch-up |
| Tap A batch2 offset3／2086 | start late26.1677ms；render0.1971ms；post-render→pub440.7352ms | publication span 本身另有長窗 |
| Dense A batch2 offset0／472 | start late0.001ms；render0.0005ms；post-render→pub27.3206ms | 不是只有 render start 遲到；此列 publication span 超20ms，按原 Dense gate 照報 |

`LatestFrame::publish` span 涵蓋 slot lock、2.7648MB copy、metadata、publish lock／notify。
目前 endpoints 不能分出各內部步驟與被排程中斷的成本，因此沒有已證實可直接套用的唯一 runtime 修補。

## AA noise 不足

| 場景／metric | 原 d（ms） | 原 T（ms） | 判讀 |
| --- | ---: | ---: | --- |
| Hold capture_to_owner_ms.p95 | 4.233010 | 8.466020 | inadequate |
| Hold observer_ms.p95 | 3.593930 | 7.187860 | inadequate |
| Dense observer_ms.p99 | 1.522694 | 3.045388 | inadequate |

值來自原 C++ noise-freeze；沒有重新配對、排除原 Hold3、替換 A、提高 floor 或放寬原數值。

## ABBA 成本差異的描述性檢視

只讀 recipe 使用原 C++ reports 的 p95／p99 與原 frozen AA T，按原 `metrics.hpp` 的 A=(A1+A4)/2、B=(B2+B3)/2 逐批計算。
沒有合併場景、拼接 distributions、相加 phase p99 或用 B 重新估 noise。
這是 report-only arithmetic：沒有重執行原 `load_run`／完整 process closure 或另授 comparison／cost 資格，原正式 comparator 的拒絕仍有效。

54個 metric cells 中52個在原 T 內、2個超出；15 adequate-noise scene metrics 對應45個 batch cells，其中2個超出原 T。

| 場景／batch／metric | B−A（ms） | 原 T（ms） | 原 AA noise |
| --- | ---: | ---: | --- |
| Dense batch1 observer_ms.p95 | +0.9436075 | 0.5485 | adequate |
| Dense batch2 observer_ms.p95 | +0.6910050 | 0.5485 | adequate |

只有 Tap batch2 capture_to_owner_ms.p99 單項改善超過原 T，沒有同批 observer／owner 改善配對。
九批的 paired bottleneck＋capture improvement 全為 false；不能宣稱 C5 三批穩定收益。
帶有时序／hard-cost 負結果的 run 仍列在固定分母，沒有抽走或重試。
`Runtime::dispatch` 的 observer wall timer 在 A／B 都包住同一 `GameObserver::process`；A／B 的 mode 分支在其後 `OwnerCandidateStep::accept`。
因此不能把 observer p95 差值直接歸為 B hook 在 observer timer 內增加工作。
Dense 三批 observer p95 的 B−A 均為正（0.9436075／0.691005／0.182775ms）；其 p50 差只有0.0207／0.048275／0.064225ms，差異主要出現在尾端而非均勻抬高。
run 間波動、不同 skipped frames／後續負載都仍是可能影響；目前不能證明具體函式或 OS／guard 根因。

## 下一步修補範圍

優先定位冷 producer 的 start lateness、render end→publication return 邊界與長空窗，依 existing attempts 決定局部修補。
單次33.9µs的 AA 負結果不能涵蓋本轮 ABBA 更大的 timing 負結果；避免只針對該微小差值調整。
另核對 Dense 的 B observer p95 退步及 Hold／Dense noise 波動；目前没有支持直接改像素策略或調寬 gate 的證據。
修補後只跑變更必要 controls 與新的必要 cold cost／noise／comparison，保留既有75／production9 PASS 來源；這份判讀不是修補候選已有效的證明。
成本合格後仍須完整 readonly／collection 邊界；真正裝置收據尚未取得。

## 容量與歷史 source 整理

24個不再被 current wide references 使用的歷史 source snapshot，22,279 files／211,289,633B，已逐 original SHA→ZIP decoded SHA→original SHA 再核對、durable proof 與即時 LiteralPath containment checks 後移除原副本。
一個 ZIP 為72,324,499B，六個逐檔 manifest shards分列原SHA；包含保存 metadata 的 actual retained 淨回收125,181,517B，原 source bytes 可按 restore-index 回到 exact paths。
main32／base55／current80 與現行 refs 保留；原 raw、STOP、負結果與已保存 archive 未刪除，整理不授資格。

本輪量測完成後原 capacity enumeration：**50,246,927,134B** retained、free210,630,004,736B。
48GiB cap51,539,607,552B／minimum-free20GiB 保持；after-phase reserve0、capacity PASS。
後續 build／tests 的原 before reserve 仍須另行核對，不能把 after PASS 冒稱可開始 fresh build。

## 完整證據入口

[本輪 Native8／raw／診斷附件索引](MAIN80_AA_ABBA_COMPLETION_REVIEW_20261012.json)保存89個新 Native8／712 pins、41個新 runs，以及82個 attempts／journal raw files的逐檔 SHA（8,007,187,561B）。
原38／160索引另引原件；report-only 成本與風險 recipe、source-boundary 判讀及capacity分開標示，沒有冒充原正式 gates。
[歷史 source 整理補充索引](MAIN80_AA_ABBA_COMPLETION_REVIEW_20261012_ATTACHMENTS.json)另列 archive proof／restore-index／ZIP container的原SHA與保存 recipe；Native保存階段本身沒有刪除原件。
ignored r1 status／campaign／continuation 與原 source、raw 路徑可由索引定位；只讀診斷不增 Native、measurement 或裝置操作。
