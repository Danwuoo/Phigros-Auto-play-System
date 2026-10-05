# X10c：X10b 獨立工程驗收與 suppression／pending-grace 反例

**X10b 的封存工程證據獨立核對通過；standalone suppression family 否決採用。** 新完整重播定位了2883的原因，沒有改善77 Miss、沒有正式候選、沒有live。C36h tint1仍experimental baseline；main50仍donor/control、live0。這是科學停止線，不把工具通過當策略通過。

工作區：main／HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，唯一registered worktree。父chat已停止；本chat自行核對、實作、執行，沒有派額外chat或sub-agent。原dirty與frozen source/binaries/raw全部保留，正式src/include／根CMake對HEAD無差異，沒有reset/clean/commit/push、emulator、真觸控、模型訓練、goal或新timer。

## 1. 獨立簽收的範圍

新證據根：[hold-cascade-x10c](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-cascade-x10c/)。[預先protocol](HOLD_CASCADE_X10C_PROTOCOL_20261003.md)在新實作/replay前保存；原X10b三次replay與source-freeze不改、不追加。

**Verified：**讀原predicate、caller、tests及comparison/audit程式；重新核source-freeze全部source snapshot、actual export及EXE/DLL SHA；四份XML的case數、fail/error/disabled/skip、三份run來源／manifest／政策／分母、全events hash與baseline bridge均相符。原XML為歷史執行證據，沒有冒稱重跑X10b的四套tests。檔案稽核見[x10b-independent-file-audit.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-cascade-x10c/x10b-independent-file-audit.json)。

原hook確只在history-derived fallback末端作用，claimed_outlines來自本幀outer重觀測；raw/direct候選不被新predicate直接刪除。predicate是純讀取，最多128 claims及60個RGB probes，不合併ID、不改owner/lease、不重試Down。**這些程式條件不能保證current claim的physical歸屬正確，也不能保证抑制後既有pending intent消失。**型別／幾何測試的通過範圍按此收窄。

## 2. 新診斷及完整重播

新export `out/x10c/baseline`／`variant`分別從frozen C36h-v3／X10b複製。每份72檔中69檔不變，僅game.cpp的單向fallback witness與兩個header的const history getter有差異；inverse patch還原各自parent全文（忽略換行），見[diagnostic-only-export-audit.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-cascade-x10c/diagnostic-only-export-audit.json)。baseline只在trace開啟時純讀predicate；variant仍同predicate、同continue。沒有跨lineage ABI、policy或clock混連。

新reader [x10c_contact_replay.cpp](../apps/frame_review/x10c_contact_replay.cpp)從X10b封存reader複製，保留舊五窗入口；新X10c manifest最多八窗、有integer範圍／唯一ID／每窗120幀界限。本輪七窗：H2440–2495、K2865–2895、A3493–3504、B4979–4990、C5516–5524、E5281–5293、D6158–6220，共196幀。K的31張原RGB只引用，[packet](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-cascade-x10c/rgb-packet-K2883.json)另存原index／PNG／source_frame／QPC綁定。

每幀保存semantic、scene、candidate bank、note history、owner、contacts各自digest；selected-window記全部物件及identity/relation/root/owner trace。fallback witness含抑制前candidate、近期history、當前claims及raw candidates。每幀buffer≤256 records／2MiB，history≤128×6，streams仍用batch硬上限；OFF不保留mechanism records。trace資訊不供策略讀取。

- **新Release原回歸兩版各207/207**（含原27張RGB），新diagnostic/window tests Release／Debug-ASan各9/9，fail/skip/disabled0。共享case不相加當獨立場景。ASan只跑新診斷契約，未跑ASan完整recording。
- **新full replay恰4次**：兩版ON／OFF，各7722 PNG逐SHA、7715 perception、32 preroll、196窗幀、結尾contacts0、無truncation或replay failure。相同owner-consumed集合、frame-first、零fake recognition/RPC、五指success receipts；每版全events／semantic與對應frozen X10b run相同。
- 每版ON/OFF的全events、semantic、7722行state-digests均相同。新Release replay SHA：baseline `59f1bcddaac683808c20fc7549cc9cfa81b049730522bbbd2f2712acc96e65b0`；variant `a965feaabb83c3299835fe14c0502c63f93a32bbcef68fd4cc3d576528aaa223`。這不是原C36h live binary。
- 第一輪build的Windows path macro warnings已在測試前修正；Release build2／ASan build1因GTest exception macros同一source line失敗，移到各自行後通過，沒有改期待，所有logs保留。工程失敗不算成功分母；完整replay失敗為0/4。

來源／binary在第一個replay前綁定，之後未重build replay/core。後續inspection／fake-clock probe是獨立分析工具，source、binary及linked baseline_core.lib另凍結。host cost及jitter保存於四run summary；其scope不含PNG或每幀digest/trace序列化，不是受控runtime latency。**X11未驗收。**

## 3. 全首action與第一state差異

新C++ [x10c_audit.cpp](../apps/frame_review/x10c_audit.cpp)對全actions做exact LCS，硬限10000 actions／25 million cells；signature保留ordinal、source_frame、座標、scheduled/start/return、receipt status與release failure/unknown數，排除local intent/contact ID。先讀原X10b，再讀新ON輸出，兩次alignment完全相同；不沿用舊刪除audit於2883中斷的結論。

| 完整分母 | 數量 |
|---|---:|
| baseline／variant actions | 2165／2163 |
| 相同normalized actions | 2161 |
| unmatched baseline／variant | 4／2 |
| 未稽核actions | 0 |

差異只有：baseline2478 Down／2479 Up被移除；baseline2885 Down／2887 Up對應variant2883 Down／2884 Up。**2887之後至EOF也已對齊**；沒有把剩餘1453 actions當已驗而略過。這是normalized action signature相同，不是physical identity或contact ID配置等價。完整reports：[原封存独立audit](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-cascade-x10c/independent-old-action-audit.json)、[新audit](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-cascade-x10c/full-action-audit-1.json)。

**Verified：首個可觀測semantic／scene／bank／note-history差異在2477；owner／contacts首差2478。**這補齊原fullprefix first-state Unknown，scope為本工具明列的逐幀資料，不冒稱所有private line-tracker內部狀態。原錄影前state仍unknown。

raw digest不同的幀數：semantic4434、scene3910、bank814、note-history4075、owner4348、contacts8。後續ID/retirement差異不能自動解釋成physical差異。196窗幀中去note ID/revision後targets只在15幀不同：H2477–2486十幀、K2881／2883／2884／2885／2887五幀；A/B/C/D/E全部targets及normalized actions保持。**沒有保存全7722幀的normalized target明細，不能宣称全首其餘觀測語義等價。**停止於負結果，不為這項未知追加第五次replay。

## 4. K2883的第一causal boundary

[causal-K2883.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-cascade-x10c/causal-K2883.json)逐幀核31份相同pixels／clock，682與684均為各run內join，不作physical gold。

**Verified：**684在2865–2880的target/history兩版相同；2881抑制前witness的全部欄位也相同，只有`applied`不同。故這一局部鏈不是用2477以前／H傳下來的不同684 history解釋。2881為K的第一target／history分歧，682在K窗的target保持相同。

| boundary | baseline | X10b |
|---|---|---|
| observation／2881 | fallback684重建到(781.616,472.763)，保留target | 同fallback通過interior witness而skip，當前快照無684 |
| relation/root | 684仍有4 samples，得到`root_past` | 沒有當前684，因此沒有新root/contradiction傳給owner |
| owner／2881 | 原pending intent197收到明確`root_past`而取消，cursor0可由後續新pixels合法重建 | generic missing branch仍在Hold60ms grace內，不區分pending/active，保留intent197 |
| scheduler／2883 | 此時無684 Down | 舊plan於50310736400ns Down，source_frame14647（2880）；早於2883 capture50311868700／pixels_ready50313358500，是幀間due，不是由2883新root造成 |
| release／2884 | 682仍Move | grace到期後684 Up／`current_object_missing_or_region_lost`取消，682仍Move |
| baseline後續 | 2885新current root Down、2887 `identity_ambiguous` Up | 沒有這一組Down/Up |

source_frame14647的plan evidence=50271878400ns；variant Down evidence age38.858ms，仍在原100ms期限，gate保持。**不是unknown RPC重試／completed復活，也不是scheduler漏看已到期計畫。**C36h `game.cpp` generic missing分支未檢cursor，明確root矛盾取消則在current target處理中。原`SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt`測試也證明舊版本有刻意的pending missing容忍；不能把main50文件的更嚴敘述套到C36h。

[pending-probe.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-cascade-x10c/pending-probe.json)用**未修改C36h owner**獨立FakeClock隔離這個機制：lead0、Hold due50ms、30ms的新快照。明列root_past時pending=0且無Down；target缺席時pending=1、50ms Down、61ms grace取消，兩邊stop contacts0。這是純生命週期反例對照，與真實lead35 replay分列，不當新candidate或gameplay命中。

## 5. Pixels、判定與停止線

已目視原2865、2874、2880、2881、2883、2884、2885。**Strong inference：**2865在x783同欄有兩個可見分離body（下方约352–446，上方约119–290），到2881上方incoming front約533，距y576線仍有gap；但682的current claim被重建成front576、depth214，將此incoming body納入claim。X10b在y472局部兩側藍帶連續，不能證明該body是原已held的physical note。這是值得另立兩Hold／tail過線／incoming近線counterexample的ownership缺口。AI目視及跨幀link均proposed，human gold增加0。

**Verified negative：**standalone suppression可把一個會取消pending Down的current root矛盾，變成沿舊grace繼續到期執行的target absence；與預先要求的完整行為驗證及不新增無當前target Down不相容。故**X10b family不採用、不進X11/X12**。不調color/width/extent閾值救2883，不把總actions少2叫改善。physical684是否同一Note、兩版哪個遊戲判定較好、682是否誤接下一body、77 Miss與此例關係、cross-song效果皆Unknown。

本輪四次replay額度已用完，且已有可重現composition反例，達科學停止線。下一個**獨立X10d提案，尚未實作**：先為current held claim的body歸屬與suppression後pending invalidation定契約；從兩個同欄但分離Hold、舊tail過線時新front接近、旋轉/neighbor/absence，分開pending／active／unknown Down／completed。優先保留當前完整front，並研究如何明示suppression原因，避免把矛盾變成一般missing；不能放寬alias或以像素顏色替代physical identity。以K與H作real-pixel regression，保留全部controls，再最多一個事先聲明的candidate。若仍無法因果辨識，保留unknown，不盲目live。

X11成本／source-binary-profile freeze仍conditional；只有冷接受後才做。X12必要Computer/emulator授權仍有效，最多6輪且先preflight；本輪0輪。X13跨曲／holdout仍未做，原raw只讀、fixture能力不當Phigros語義。沒有可用candidate或必要新資料缺口，故不啟動live。

## 6. 重現、容量及保護

入口：[initialize](../tools/initialize-hold-cascade-x10c.ps1)、[prepare](../tools/prepare-hold-cascade-x10c.ps1)、[run](../tools/run-hold-cascade-x10c.ps1)、[bind](../tools/bind-hold-cascade-x10c.ps1)、[finalize](../tools/finalize-hold-cascade-x10c.ps1)。**現有batch與out已封存，initialize/prepare不可重做；不要在out/x10c或out/x10b rebuild。**重現full replay需下一獨立experiment新root、預算／protocol及新的source/binary binding，不把以下commands當追加本batch額度的許可。

本輪實際commands／exits逐次保存於batch；既有檔可讀或用新小report名重算analysis：

```powershell
$x10cBatch = 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-cascade-x10c'
./out/x10c/build/Release/x10c_audit.exe "$x10cBatch/baseline-on-1" "$x10cBatch/variant-on-1"
./out/x10c/inspect/Release/x10c_causal_report.exe "$x10cBatch/baseline-on-1" "$x10cBatch/variant-on-1"
./out/x10c/inspect/Release/x10c_pending_probe.exe
```

容量以[final-summary.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-cascade-x10c/final-summary.json)的實際bytes為準：batch≤128MiB、out≤768MiB、campaign+prior≤8GiB；初disk free32,397,443,072B、campaign+prior7,964,287,797B。全部新source/log/XML/失敗/report/四run與帳本計額，原PNG只引用。新export/build／analysis產品另列；沒有用刪舊raw騰空間。

final freeze核所有pre-replay sources、exports、binary bindings及四份bridge；原X10b source-freeze仍原SHA `4907fdcde7d8f4ccbfc1ccbf9d83ac60529fd705c194b92a6aeaeafc8a88fcf4`。較遠狀態見[前進計畫](C36H_FORWARD_EXECUTION_PLAN_20261003.md)、status J19。可冷範圍已交付negative result；較遠研究沒有標完成。
