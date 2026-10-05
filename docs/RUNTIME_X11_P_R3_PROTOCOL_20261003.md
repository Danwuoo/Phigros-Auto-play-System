# X11-P-R3 有界長窗口成本資格 protocol

2026-10-03 Asia/Taipei。僅離線開發與一次有限資格驗證，交付後待總控獨立驗收。C36h tint1 behavioral baseline；main50 donor/control；B1 唯一策略差是已驗 pending-only 八行 known absolute cursor0 hook，suppression OFF。不得把重建 B0 稱原 live binary。正式 src/include/root CMake、舊 frozen/raw 保留，不 commit/push、emulator/ADB/manual-session/真觸控/模型/X12、新 goal/chat/automation；過期 timer 不恢復。

## 配额與資料完整性

新根 `measurements/runtime-cost-x11-p-r3` ≤2GiB，normal24筆×80MiB=1920MiB，工程／stress／全部 logs/source/XML/ledger ≤96MiB，總控獨立驗收預留32MiB。新 build `out/x11-p-r3` ≤3GiB；舊 campaign+prior ≤8GiB，兩帳≤10GiB，另 disk free 保留5GiB。初始化實測舊帳8,241,831,623B；初始與每個 run 前 capacity 快照另存。失敗占額，不刪舊資料、不複製原 raw，不採樣、壓縮或丟完整 JSON。

每 run 界限：2560 attempts/2561 slots、receipts/release calls 各8192、events/enqueue/archive serialize/write samples 各65536、單 event（含 envelope/LF）≤256KiB、event admission logical≤40MiB。mailbox8192/16MiB、native32×16MiB logical segments保持。新 isolated archive cpp 只把三個16384常數改65536（兩reserve、一fail），header/class/ABI不變；原 frozen core 不改、不重編，link map證明 meter/tests 的 SessionArchive 全部由新 object 提供、原 library archive object未抽取。兩版連各自未改 R1 core，ASan連原插樁 B1 core。新 runtime pas.exe不存在；正式 default 無 meter clock/delay，原 R1 instrumented 上限16384。本包數據不是整個 manual-session 或原 live 的效能。

raw 使用 LF binary stream，每 frame≤1023B、receipt/release≤511B、event timing≤255B。完整 compact JSON每row只有一個LF，內嵌CR/LF必被escape，archive text-mode CRLF physical≤40MiB+65536B；連同2560×1023＋2×8192×511＋65536×255＋4MiB metadata =73,905,664B <80MiB=83,886,080B。直接核logical、CRLF extra與physical，allocation reserve另列，不把32×16MiB logical聲稱physical硬限。初期24MiB估計在新cost前由既有R1 owner完整journal2,043,814B/128的線性投影40,876,280B否證容量adequacy；因此改40MiB、總80MiB/2GiB不增，舊初版source/tests/log保留，三版重新測試。投影不是硬上限，真正40MiB admission與row/count界限仍超限fail。總 raw/event 核 count/row/EOF/字元限；缺資料不能pass。完整性包括所有warmup/fail/late、完整receipt/release/event內容及 archive segment SHA；所有 enqueue/serialize/write逐筆旁路計時另存，以核完整分布。每run actual file length及最終逐檔SHA/檔長帳本另核；allocated NTFS clusters 未量，不等於 file bytes。

## 凍結方法與原門檻

normal 每run256 warmup＋2304 measurement，共2560；RGB沿R1原16RGB SHA、cadence16ms；owner沿R1原32target adapter、cadence8ms。lead35ms/uncertainty30ms/enabled15、jitter{-2,+1,+2,-1}ms。LatestFrame physical3/latest1/decision1、真shared high-resolution Wake、SessionGameOwner start/accept/poll/finish、SessionArchive event/complete/close與完整production JSON保持。meter的admission/envelope dump、enqueue计時、queue-accounting dump、writer serialize/write计時是有成本的共同instrumentation，沒有去除診斷。真capture/RPC、HUD/result/supervisor/裝置閉環未覆蓋。

warmup、measurement及四個固定576-attempt blocks全部報 attempts/published/drop/consumed/owned/consumer skip/decision skip、receipt/phase/late/failure/release全分母與14組n/p50/p95/p99/max/p95−p5 jitter。frame/receipt按source attempt；release/enqueue/writer按drain當時最後accepted attempt錨定，finish歸最後accepted attempt，非服務時刻分窗。blocks僅描述，不挑最佳block。缺phase記n0/null Unknown，normal all-phase lateness必有樣本。principal measurement recognition/owner/capture→owner各n≥2000；此為尾rank解析度，不是独立樣本或信心保證。

保留R1的normal90%完整attempt分母及measurement90%（2304×.90）；完整run的capture→owner p99≤100/max≤250ms、all-phase lateness p99≤15/max≤100ms。全run time order、published+pooldrop=attempts、owner≤consumed≤published、receipt/release failed/unknown0、contacts exit0、worker/archivefault0、無debug drop/discard、完整raw/count/sample/byte界限必須過，warmup硬安全不排除。empty/nonfinite/missing metric不能pass；normal action_expectation固定required。

成本noise用measurement前三metric p95/p99：每load四次B0的pairs(1,2),(3,4)，d=max pair absolute diff，T=max(floor,2d)，adequacy 2d≤max(min四run metric×.30,floor)。floor recognition .5ms/owner .25ms/capture→owner3ms保持。ABBA各load兩批B0/B1/B1/B0，以兩B0與兩B1 run quantile算術平均差≤T；不是pooled p99，不加stage p99。全部必要noise/validity/integrity/hard gates过才進ABBA。

先驗顺序：新collector tests B0→B1→ASan、兩Release streaming bridge與原512input bytes比對、原XML只核不冒稱重跑；再source/input/options/clock/environment/commands/test order/capacity及不可變文件snapshots freeze，然後AA rgb1–4→owner1–4。最多8AA、16ABBA、stress≤4（本包預定0新stress，引用R2同源active concurrent結果）；任一run必要gate不足立即保存負結果並停止後續cost，noise不足停candidate comparison。若8AA全過才freeze noise，noise通過才固定ABBA rgb batch1/2→owner batch1/2。沒有安靜重跑、事後調負載、改門檻或用candidate結果改T。

最多一次量測工程修復；首次B0同行GTest label與後續archive verifier字串/JSON C++20 overload共兩份failed build，修排版及明確get<string>；初版source/log保留，計一個pre-cost修復週期，cost0。初版三套9/9；40MiB admission完善後三套10/10重新執行，所有初驗輸出亦計額。測試完善於freeze之前，不回改策略；freeze後禁止rebuild及修復重跑。新tests覆蓋exact/over receipt-release/event樣本、bounded reader count/row/truncation/malformed/empty、缺樣本/空metric/全run warmup硬fail、完整archive determinism與缺row。ASan只聲稱實跑自有collector/tests與連結core，第三方未全面插樁；R2三笔active OS結果只引用來源不重跑，不重造body/Wake規則。

## 交付與停止

result/handoff/final-summary、全部來源snapshot/source-binding、CL/link tlogs dependencies、map/archive diff/inverse/provenance/profile、commands/exits/XML、完整raw及逐檔 logical/LF/CRLF/physical lengths、負結果保留。status與forward只追加，batch內不可變副本freeze；R2歷史兩mutable文件只能用總控pre-review SHA例外，其餘逐項核。Verified / Strong inference / Hypothesis / Unknown分類，不自簽總控、正式採用或live-ready；若仍not-ready交具體容量/樣本/有效性/候選差異結果，無自動R4/R5/X12。14lost opportunity利弊、77Miss改善及跨曲均Unknown。

## 交付補記（成本結束後，只澄清來源）

本輪AA5/ABBA0，新stress0；owner1原full owned90% fail後立即停止，noise未評估。上文「保留R1…及measurement90%」中的measurement90%是R3成本前新增較嚴條件，不是R1原條件；full90%已獨立足以否決本run。原immutable protocol-before-cost/source/門檻不回改，來源差異和完整結果見R3 RESULT/HANDOFF。此補記不修改量測或判定。
