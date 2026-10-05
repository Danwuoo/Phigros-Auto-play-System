# X5 總控直接補正：lineage scope、量測時間與真實反例（2026-10-02）

使用者授權總控直接補正。這次完成離線工具修補與研究契約，**不改正式策略，也不撤銷 NDA-v1 的否決**。本文件與 status J14 接續 X5 獨立驗收；原交付／验收的 source snapshots、binaries、reports 保持，工作目錄中的 adapter／tests／離線 CMake 是新版本。

## 1. 接手與修改邊界

**Verified**：HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，main，唯一 registered worktree。既有 `apps/frame_review/main.cpp`、status 與 X1–X5 untracked 交付尚未提交；沒有 reset、clean、切分支、commit 或 push。正式 `src/`、`include/`、根 CMake 未改，observer50／planner27／diagnostics11、live0。C36h tint1 仍 behavioural／experimental baseline，已保存實戰仍為77 Miss。

修改／新增：

- `apps/frame_review/x5_adapter.hpp`：拒絕 stale prior 的 latest measured secant。
- `apps/frame_review/x5_scope.hpp`：typed、唯讀、lineage-specific research scope。
- `apps/frame_review/x5_scope_report.cpp`：固定已驗收 X5 report 的獨立描述性附錄及有界轉向候選搜尋。
- `tests/x5_rule_tests.cpp`、`apps/frame_review/x5_offline/CMakeLists.txt`：新增7項測試及離線 reader target。
- `tools/repair-early-role-x5.ps1`、`tools/audit-early-role-x5-repair.ps1`：新目錄建置、限額輸出、驗證與保存。
- 本文件、status J14、X5 交接 §10。其餘歷史文件不改。

**沒有**新 observer／owner／scheduler replay、NDA-v2、threshold search、模型、emulator 或真觸控。新 build 只在 `out/x5-repair`；舊 `out/x5` 不重建。

## 2. Stale-prior 修補與保真範圍

原 `feature_json()` 把 previous line 的 observed time 直接設為 previous Note 的 sample time，未檢查原線是否 current。新實作先檢查 `old.current`；`pose(raw, sample.ns)` 只有在原 `observed_ns == sample.ns` 時才把它設為 true。未通過時輸出 `valid=false / line_not_current_valid_at_both_note_times`，不產生 velocity／distance-rate；通過後才可使用已核等價的 sample time。沒有憑空補造 timestamp。

新增 adapter 回歸使用三幀 raw bank／identity correspondence：只將中間幀的 line observed time 改成 stale 1ns 或5ms，current line 仍 fresh、motion model 仍 invalid。兩個 stale case 必須拒絕 secant；fresh control 仍可量到 −1000px/s。另驗 current stale、prior association invalid。pure NDA-v1 原本就拒絕 stale paired history；這次修的是診斷保真，沒有宣稱修到 runtime 錯選。

**Verified**：Release／Debug-ASan 各34/34（原27＋新增7）、fail／skip／disabled 皆0。兩種新 reader 均從原 manifest／trace／PNG 重算337幀，與獨立驗收 report 全文一致，**僅正規化 `analysis_binary_sha256`**；不是新旧 binary byte-identical。固定來源863次 raw line bank 都 fresh，因此修補沒有改變原結果。

## 3. C36h 與 main50 的資格契約

`X5-lineage-scope-v1` 是 **研究分層，不是選線資格或 Down 資格**。它不進 `Input/evaluate()`，不補填 confirmation，不重算新的 recommendation。

| Lineage／情況 | 契約 |
|---|---|
| C36h strong-current、unique runtime assignment、非 Hold，pre-selection `prior_samples < 3`，selection trace 完整且 preserve=false | 可列入 conservative early comparison：C36h preserve 的必要條件 `points.size() >= 3` 尚不成立 |
| C36h preserve=true、prior_samples≥3 | 當幀 preserve 已走；不列 early |
| C36h preserve=false、prior_samples≥3 | mature-history 非 preserve；same-recent、span、distance 等哪個前提不成立尚未知，不能當 unconfirmed |
| C36h 缺 prior_samples／selection 或出現矛盾 confirmation／preserve | unknown／contradiction，不列 early |
| main50 有明確 `confirmed_line_id == 0`、preserve=false，且 current／identity／非 Hold 守門成立 | 沿 main50 明確狀態列 early；此欄在 expiry 清理後、建立新 confirmation 之前輸出 |
| main50 confirmed、缺欄位，或 weak current／identity ambiguous／Hold | 排除或 unknown |

**程式依據**：frozen `out/x1/c36h-v3/src/game_tracking.cpp` 在 candidate_track 輸出 prior_samples 後才進選線，preserve 前沒有補進新的 relation sample；必要條件是≥3個 relation samples、span≥30ms、same-recent≤40ms，再驗 closing 或 current-body。`out/x1/c36h-v3/include/pas/game.hpp` 沒有 main50 confirmed／replacement 欄位。main50 的 `relation_selection.confirmed_line_id` 與 C36h 當幀 preserve 並非同一 state。

這是 sufficient early stratum，**不是完整重建 preserve predicate**。relation samples 有10ms bucket，可經 reset 清空；不等於 raw frame 數、physical Note 年齡、未曾 Down，或「初次看到」。採用保守子集能先做有意義比較，不必立即重建 replay 或擴增 production diagnostics。

**Verified 分母**：

| factual history | target occurrences | early comparison | 其他分類 |
|---|---:|---:|---|
| C36h | 394 | 85 | current-preserve214、mature/nonpreserved34、Hold57、identity4 |
| main50 control | 413 | 68 | confirmed282、Hold57、identity6 |
| X4 history | 413 | 67 | confirmed286、Hold57、identity3 |

C36h D6160–6164 local1622 的 prior_samples 為0／1／1／2／2，均屬此early stratum；當時 factual winner356。6210有4個prior samples且 preserve=false，仍屬 mature/unknown，不因線缺失自動重新授權。原 X5 scope0／68／67、推薦0／12／12仍完整保留；**85不是新增推薦，也不能與 main5068直接比較優劣**。

## 4. 真實反例小包：避免把 proposal 再升格成 gold

`Scope-release-1.json` 是337 role frames／1220 target 的全量 scope 附錄，另收27張不同PNG的81個 factual-frame packet：E5286–5288、D6160–6164、6170–6180、6193–6195、6210–6214。line／secant欄位沿用原report，原圖只引用及核SHA，不複製。`visual-audit.json` 再記本次實際目視8張原圖與其中的物件數值：6173–6175、6180、6188–6191。

有界轉向搜尋只用每個history內最多6幀／90ms，三次 explicit unique runtime correspondence；每pair≥10ms、位移≥3px、speed≤4000px/s、兩段位移夾角≥30°。全337幀搜尋得到35次proposal（C36h12、main50 11、X4 12），保留全部，不是35個physical turn、更不是35個failure；三history高度相關。這些取樣條件只作離線檢索，不進策略。

| 案例 | 目前可覆核的訊號 | 證據等級／不能推論 |
|---|---|---|
| 6173–6175，Drag與Flick交疊 | C36h local1622 center `(285,287) → (302.005,275.447) → (317.506,282.984)`，主軸長152.007→124.015→138.101px，位移角1.04931rad；圖中yellow直條持續向右、紅色Flick遮住不同部位 | 数值 **Verified**；遮擋導致中心偏移、造成假轉向提案為 **Strong inference**。同runtime ID不能證明physical identity，更不能標成真late-turn gold |
| 6188–6191，Tap／Flick／線的幾何變化 | C36h Tap local1626 center `(473,81)→(473,95)→(424,120)→(431,134)`；6190原圖的cyan core、red Flick與一條白線開始傾斜，另有水平及垂直線 | raw pose變化 **Verified**、AI目視proposal；joint rotation／局部座標變化為 **Strong inference**。是真實姿態變化對照，還不是「先接近decoration再轉向另一判定線」的角色gold |
| 6180鄰近物件／多線 | yellow cores約 `(403,288)`、`(558,359)`、`(714,431)`；三者均向右的history、上下兩條水平appearance與右方垂直線。後兩個main50 shadow推薦315，factual305；C36h對應局部物件當幀preserve356 | 幾何與各run輸出 **Verified**，跨run物理對應及judgment role仍proposed。不能拿主D cohort的X4成功替鄰近物件背書；這也不是已驗「平行line neighbor錯搶」gold |
| 6194線自身移動 | 沿用X3/X5量測：水平候選的closing可由線移動造成，Note normal component可為0 | **Verified** measurement；closing-only不夠判role，不表示該線必是假線 |
| 6210／6214 | 原pixel可見垂直線；6210 bank缺線與6214看見卻配錯要分開 | 先前驗收證據；本包引用，不假裝本輪新因果實驗 |
| E5287 | 使用者確認的正常control；X5 E5286–88推薦與原winner相同 | 保留正常，不因新轉向檢索／scope數量重新標failure |

**Unknown 仍明示保留**：此次沒有取得人工確認的 real late-turn judgment-role gold，也沒有證明當下pixels必然含可唯一識別未來role的cue。找不到gold不以model confidence或後來的Down補齊。已得到的價值是可重現的「轉向候選可能來自觀測形狀偏移」與「Note／線共同姿態變化」兩個可區分對照。

## 5. 下一個可證偽工程步驟

不再加NDA-v1門檻。最有資訊量的下一步是**離線中心量測可靠性消融**，先限於6173–6175交疊與6188–6191姿態變化，以6180分離cores及E5287正常窗作control：

1. 重用原RGB、candidate bank與本包的join，輸出同一候選的current color-core span／coverage、主軸端點、component split、center displacement沿主軸／法向的分量；所有ROI先標proposed，未知部分不補像素。
2. **Hypothesis**：交疊時沿主軸coverage收縮與center漂移共同出現，但法向移動仍連續；真正整體姿態變化應同時改变tangent／line-local geometry。只量這組feature，不先判新role。
3. 先做單一「位移量測可信／unknown」離線mask或annotation；缺支持只能abstain，不能透過歷史補回body、租期、root或觸控。positive＝交疊偏移可被揭露；negative＝未交疊移動、可見旋轉不得全被當noise。
4. **Falsification**：若控制窗也同樣頻繁被抑制，或body/span變化無法區別兩組現象，就拒絕此feature，不繼續盲調。只有 current-pixel signal 通過合成與real regression，才考慮 bounded C++ reliability gate；之後再另評估單機制contact replay。

這是下一個可獨立停止的小實驗，尚未實作。現有 completed／unknown Down與owner guards未被改動。沒有可freeze的正式策略，因此本補正仍不需要emulator；遊戲採納、真時序、feedback與Miss改善需未來候選就緒後另行授權的有限live。

## 6. 重現、保護與容量

新證據目錄 `measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5-repair`，硬限8MiB，campaign＋prior≤8GiB；console／annex≤2MiB，reader process commit≤512MiB。writer採create-new及共享budget mutex；原evidence不覆寫。編譯產品另列，不把報告／失敗log搬進out規避計額。

```powershell
./tools/repair-early-role-x5.ps1 -Mode Configure -Config release -Tag new
./tools/repair-early-role-x5.ps1 -Mode Build -Config release -Tag new
./tools/repair-early-role-x5.ps1 -Mode Tests -Config release -Tag new
./tools/repair-early-role-x5.ps1 -Mode Verify -Config release -Tag new
./tools/repair-early-role-x5.ps1 -Mode Scope -Config release -Tag new
```

這是重現指令格式，**目前凍結後不要直接覆寫同一build目錄**；若需再次build，先改maintenance script的build root並記新provenance。重跑reader只用新Tag，剩餘額度不足時會拒絕，不刪證據挪空間。Debug-ASan用`-Config asan`；完整reader及scope reader明列`quarantine_size_mb=16:thread_local_quarantine_size_kb=64`，沒有聲稱default-quarantine的512MiB全reader驗證。

實跑：Release／ASan各34/34；兩份Release scope附錄byte-identical；ASan附錄僅binary SHA不同。兩種reader原337幀全文重算一致。新scope CLI兩種build各驗錯argc與wrong-parent拒絕，合計4/4；沒有把這說成重新跑過X5舊26例或完整runtime套件。此次不改原X5 CLI／pure rule、也未重跑舊C36h／main50 gameplay suites。

所有具體command／environment／XML／binary與source SHA、1723份接手protected檔案及繼承X5保護清單的核對、實際bytes與剩餘額度，見本batch的 `repair-summary.json`、`preservation-after.json`、`source-freeze.json`、`visual-audit.json`。這是總控直接實作與驗證，不冒稱另一個agent已獨立review新補正。
