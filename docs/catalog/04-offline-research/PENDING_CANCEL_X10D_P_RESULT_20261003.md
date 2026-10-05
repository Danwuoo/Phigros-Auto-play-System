# X10d-P：pending missing 單機制移植與完整冷驗交接

2026-10-03（Asia/Taipei）。**隔離候選的生命週期契約自驗通過，待總控獨立驗收。** 正式 src/include/root CMake 未修改，沒有採入正式 candidate、emulator/live、X10d-O、X11/X12、訓練、goal、額外 chat、commit/push。C36h tint1 仍 behavioral／experimental baseline（77 Miss 未改善）；main50 仍 mechanism donor/control、live0。少 Down 不是改善證據。

工作區 main／HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、唯一 registered worktree，接手既有 X1–X10c dirty 與 frozen evidence 保留。父 chat 未並行寫入，本 task 沒有派 sub-agent。證據根 [pending-cancel-x10d-p](../../../measurements/game-assist/2026-09-30-m0-manual-continue/pending-cancel-x10d-p)，新 export/build [out/x10d-p](../../../out/x10d-p)。

## 1. 唯一 hook 與來源

[預先 protocol](PENDING_CANCEL_X10D_P_PROTOCOL_20261003.md) 在實作／測試／新 replay 前保存。兩版均複製 frozen `out/x10c/baseline`，其 parent 是 C36h-v3；**X10b suppression OFF**。reference 72 檔逐 byte 與 parent 相同；candidate 71 檔相同，唯一 game.cpp 變更是 main50 missing loop 的既有分支：

```cpp
if(identity.submitted) if(const auto cursor=scheduler_.executed_steps(identity.intent);
   cursor&&*cursor==0) {
    cancel_contact(id,identity,"pending_down_current_object_missing");
    continue;
}
```

連同 donor 原註解共新增8行，inverse patch 還原 reference 全文（忽略換行）。[isolated-hook.patch](../../../measurements/game-assist/2026-09-30-m0-manual-continue/pending-cancel-x10d-p/isolated-hook.patch) 可 review。原 cancel_contact 不改：只有已知絕對 cursor0 設 submitted=false；不存在 cursor（可能已完成／unknown／外部取消）、已 Down 或 prefix_offset 修訂後均保留退休守門。新意圖仍需 fresh pixels／relation／prediction 全部既有 gate，不復活完成意圖。

已讀 caller：runtime/manual-session 只將最新完整 decision 交 owner；SessionGameOwner 依 allow_down 門控。accept 的 sequence/context/fresh/capacity/set_context 檢查在 missing loop 之前。舊 sequence/context 不進 hook；非法 gate/stale/future capture 沿原撤銷路徑，較舊 capture evidence 由 scheduler 的 gate_evidence_invalid 拒絕，不能被當成 absence。未 accept、未消費或沒有新 frame 時只 poll。沒有添加新的 snapshot admission 機制。

cursor 是 `plan.prefix_offset + next_step`，不是 plan 剩餘 steps 數量。alias／shared Drag 只從已開始 contact 取得資格，不能將 pending cursor0 當 active；本輪保持原匹配、group_seen/group_alive、grace/lease、observer/association/threshold 全部原樣。原 X10c baseline 的 readonly diagnostics 保留，candidate hook 不讀診斷。

| Exact binding | SHA256 |
|---|---|
| reference game.cpp | `2ac9c356496d2bea9f18ddb27f85d514b8d5d346f8d22d5f0eb8118286064aa4` |
| candidate game.cpp | `fe115cf8f8d1861cf3caa1693978841eefa1bfd01d75624349d882747a553f28` |
| reference replay EXE | `f1c17e698f20b84dd573738be976cc3115bf207aac281a245bb597cb6cfb1c5b` |
| candidate replay EXE | `27879bdeed3647cfd1cc43232cae271ef40063202452ca2cdeeb6d35d06f3027` |
| input manifest | `26630fc251222628c77383a7042f4027f1cd59daa80a73e295f3df146487b3fa` |

[source-binding-before-replay.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/pending-cancel-x10d-p/source-binding-before-replay.json) 保存全部 compiled sources 的實體 snapshot、兩 export、EXE/DLL、tests/protocol/provenance SHA；之後未 rebuild replay/core。版本字樣仍37/19，不冒稱是原 live binary。後加 causal review 是獨立 analysis build、連既有 baseline_core.lib，另在 final-source-freeze 綁定，不改 frozen replay。

## 2. 測試：先紅後綠及刻意歷史契約差異

Windows x64／MSVC v145／C++20。新 [x10d_p_owner_tests.cpp](../../../tests/x10d_p_owner_tests.cpp) 13 個完整 owner/scheduler/FakeClock/Touch 契約測試；包含四 Note 類型的取消與 fresh-return、無返回／無新 snapshot、拒絕 sequence/context、gate/capacity/stale/future capture、due 前／後／完全同時、capture tie 但 ready 較晚、rootless 旋轉 Hold、短缺失原 grace、修訂後非零 prefix_offset、completed／缺 cursor／unknown Down、epoch/geometry/session reset、active Hold alias／shared Drag 與另一 pending intent 共存。每個 kind 均保存 receipts／contact／新 intent／取消理由及狀態驗證，不只重複 if predicate。

| 最後編譯版本實際執行 | 結果 |
|---|---|
| Release reference 原 suite | **207/207** |
| Release reference 新契約（先紅） | **8通過／5失敗**；即 pending cancellation、缺席後不得 Down、frame-first cancel、alias／shared共存取消的新期待 |
| Release candidate 新契約 | **13/13** |
| Release candidate 原 suite | **206通過／1刻意契約失敗** |
| Debug-ASan candidate 新契約 | **13/13** |
| Debug-ASan candidate 原 suite | **206通過／同1刻意契約失敗** |

全部 errors/disabled/skips=0。唯一 candidate 原 test 失敗是原樣保留的 `SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt`：兩個 returns 分支都仍期待 pending_count=1，candidate 實為0。原 XML/assertion/log 未改、不過濾；新增 fresh-return 測試另外證實可重新建立且只 Down 一次。沒有其他原 regression。原 suite 包括27張 frozen RGB、patch/front／rotation／alias／tail／shared Drag／unknown receipt 守門；共享 cases 不相加作獨立場景。

ASan 插樁 candidate core 與兩 test targets；第三方 DLL 不全面插樁，沒有 ASan full recording。第一次 ASan build 成功，但後續 DLL copy 用了不存在的 debug z.dll，於任何 tests 前 exit1；核對 installed zd.dll 後修復。失敗登錄及 logs 保留，首 script revision 未獨立 snapshot，最終 source binding 才是首 replay 的準確來源。診斷 acceptance_ns 在 freeze 前修為未呼叫 owner 時 null，最後 Release／ASan tests 重核同編譯來源。

## 3. 三次完整 replay、時間與單向診斷

恰 **3次新 full replay／0失敗**：reference ON、candidate ON、candidate OFF。每次7722 PNG逐 SHA／decode、32 preroll、7715 perception、原 owner-consumed cadence、196 selected frames、EOF contacts0、無 truncation。七窗 H2440–2495/K2865–2895/A3493–3504/B4979–4990/C5516–5524/E5281–5293/D6158–6220 全保留。PNG只引用，original recorded touch 只另存外部 comparison，不供策略；local IDs 只作同run join。

frame-first、零 fake recognition／RPC、五指 success receipts、fake QPC offset 與 X10c 完全相同。capture arrival 先更新 envelope，pixels_ready 才 process／accept；due < boundary 先發生，ready==due 時先 accept 再 poll；capture==due 但 ready>due 可以在 ready 前合法 Down。future pixels 不追溯否定已發生動作。實機 render age、早期錄影前狀態、原 perception race 仍 Unknown。

**Verified：** reference 全 events、semantic及7722行 state-digests 與 X10c frozen baseline ON byte/semantic 完全相同。reference OFF 重用已重新核 SHA 的 X10c ON/OFF 等價 control；沒有第四次重跑 reference。candidate ON/OFF 全 events、semantic、state-digests 完全相同。reader 的新增 [full-prefix lifecycle stream](../../../apps/frame_review/x10d_p_contact_replay.cpp) 在 owner/poll 後單向輸出，每幀 targets/identities≤128、≤36000行／32MiB；兩ON實際23,075,275／23,075,550B，OFF=0。它不供策略讀取；scene/bank/history每幀digest在两策略間也全部相同。

summary 保存 host n/p50/p95/p99/max、jitter/failure，scope為 perception＋accept/poll/drain/event writer，排除PNG/hash/每幀額外digest/lifecycle serialization，非受控 runtime成本比較。沒有 X11成本驗收，不把 fake duration0 當實測零延遲。

## 4. 全 action 分母、第一因果點及 lost opportunity

[exact LCS audit](../../../measurements/game-assist/2026-09-30-m0-manual-continue/pending-cancel-x10d-p/full-action-audit-1.json) 沿用 X10c 原 bounded C++ audit：≤10000 actions／25 million cells，保留 ordinal、source_frame、座標、scheduled/start/return、success/failure、release failure/unknown count，排除local intent/contact ID。**2165 reference／2131 candidate，2121 matched、44／10 unmatched、未稽核0，涵蓋至 EOF。** normalized 等价不代表physical/contact配置等價。

| Kind | current target occurrences | reference／candidate distinct intents | reference／candidate Down | reference／candidate Move | reference／candidate Up | candidate pending missing取消 |
|---|---:|---:|---:|---:|---:|---:|
| Tap | 10451 | 247／249 | 237／234 | 0／0 | 236／233 | 5 |
| Hold | 3672 | 82／86 | 75／73 | 257／257 | 66／64 | 7 |
| Drag | 6495 | 118／125 | 112／105 | 46／45 | 108／101 | 14 |
| Flick | 4244 | 110／110 | 102／100 | 381／376 | 101／99 | 2 |

occurrences 是重複逐幀 target，非 physical notes；distinct intents 是 plan map 去 revision 後分母，並非所有 plan revisions。unknown action kind0、failed receipts0。其餘444個 actions/role 是包含空集合的 release reports；非每個 Down 都對應一個 inject Up（可由 release_all 結束）。cancellations reference199／candidate215，pending缺席hook28，取消數或 Down減少都不是成功指標。

**第一 causal divergence（Verified）：** 1975 local355 Flick 的 pending plan source13742、evidence34869061900ns，原 Down due34908583087ns；1976新完整snapshot source13743、capture34871394800、pixels_ready/owner accept34872318600ns缺355。此前 cursor0、prefix0、未有 receipt，candidate立即取消；reference仍處75ms Flick grace。reference於1978幀間Down，再三次Move及1981 Up。candidate沒有這5個actions；1982返回但association_ambiguous/samples0，不重建，2003退休。不是靠1982未來pixels禁止1978，亦不是unknown Down重試。owner/semantic首差1976，contacts/action首差1978；observer scene/bank/note-history差0/7722。沒有宣稱所有未序列化private state的首差。

[lifecycle-audit](../../../measurements/game-assist/2026-09-30-m0-manual-continue/pending-cancel-x10d-p/lifecycle-audit-1.json) 與 [causal-review](../../../measurements/game-assist/2026-09-30-m0-manual-continue/pending-cancel-x10d-p/causal-review.json) 保存**全部28取消、54個LCS unmatched及19個受影響local Note的完整receipts**，包括取消前cursor／plan、新snapshot、返回資格、重建及退休。每一hook取消都是已知絕對cursor0、無 prior receipt、在接受的完整snapshot缺席；28/28後續退休。16次取消後返回、13次建立新intent、其中12次有Down；1555於5311重建後5312再次缺席而取消，沒有Down。另355／748返回為ambiguous，1121返回insufficient_history，均不借返回就自動觸控。

所有 action 差異由以下鏈解釋，Unknown action notes=0：

- **14組 contact 未再開始／lost opportunity**：355(Flick)、571(Drag)、616(Tap)、822/884(Hold)、985/1027(Drag)、1087(Flick)、1121/1129/1135/1142(Drag)、1447/1555(Tap)。reference有Down而candidate無Down。其physical notes與遊戲是否採納未知；不能稱都是ghost或都是Miss被避免。
- **5組 fresh-return更換plan**：455／927／947／980／1145(Drag)，依返回當前root建立新intent；Down時間／座標／source改變，四組相應Up座標也改變。947原3495較早Down再Move，新版直接在當前位置Down，少一個Move。
- **1個完全相同Move的排序差**：947在3496的Move，其時間、source、座標/receipt signature完全相同，但新intent使 scheduler map key 的既有同due/同phase tie次序改變；global LCS各算一個unmatched，per-note receipt仍相同。忽略順序的exact bag為43／9 unmatched。保留44／10原audit，不藉bag覆寫。frame-first與scheduler phase priority政策未更改。

沒有hook取消active contact、重試unknown Down、completed復活或新unsupported Move。14組未開始contact和5組重建中的既有後續正常取消／Up仍需按兩版自身生命週期解讀；不把normalized matching變成物理連續性gold。

## 5. 七窗 controls及採納邊界

全部196窗targets去ID/revision後相同；更強的全prefix scene/bank/history digest也全相同。H31／K12／B4／C6／E2／D23個normalized ordered actions兩版均相同。**H2478額外Down仍存在，K684於2881原root_past取消與2885後續Down亦保持**；本hook不是讓X10b suppression復活，也沒有解body claim ownership。

A窗actions22→21，是local947 Drag於3494 pending缺席取消、3495 fresh-return重建及3496等時Move次序；local持續Hold的Down/Move/Up signature保持。七窗沒有涵蓋所有新pending問題，故使用既存全prefix compact records定位1976–5312，不追加窗口或第4次replay。最後action差在5310，之後至EOF亦對齊。

**Verified：**單一isolated hook、source/clock/cadence/receipt綁定、紅→綠、唯一刻意歷史test差、observer零改變、diagnostics單向、完整54個unmatched稽核及28次known-zero生命週期。

**Strong inference：**在本固定pixels+success receipts模型內，19組action差異由pending撤銷／fresh-return及既有tie排序解釋，沒有須引入第二機制的新反例。

**Hypothesis：**即時較新pixels缺席後取消尚未Down的intent可能減少無當前target的觸控，但也可能失去短暫漏觀測期間的合法機會。14組lost opportunity不能由此資料判定好壞。

**Unknown：**physical identity／body歸屬、recording前state、真recognition/RPC/閉環feedback、14組機會損失的judgment、77 Miss改善、跨曲泛化與實機採納。human gold增加0。現有pixels+FakeClock足以驗這一程式契約，因此本task沒有必要啟動emulator；若總控後續要判定效應，最小新增證據是凍結candidate／baseline的有限閉環comparison，且先完成X11成本/freeze。這屬總控後續決策，本task不進X11/live。

## 6. 保護、重現及 freeze

入口 [prepare](../../../tools/prepare-pending-cancel-x10d-p.ps1)／[run](../../../tools/run-pending-cancel-x10d-p.ps1)／[bind](../../../tools/bind-pending-cancel-x10d-p.ps1)／[finalize](../../../tools/finalize-pending-cancel-x10d-p.ps1)。**現有batch/out已封存，prepare/bind/finalize不可重做，不在out/x10d-p/x10c/x10b rebuild。**重現 replay需另一有界root/protocol，以下只供新小report重算；不是追加replay許可：

```powershell
$x10dBatch='measurements/game-assist/2026-09-30-m0-manual-continue/pending-cancel-x10d-p'
./out/x10d-p/build/Release/x10d_audit.exe "$x10dBatch/baseline-on-1" "$x10dBatch/variant-on-1"
./out/x10d-p/build/Release/x10d_lifecycle_audit.exe "$x10dBatch/baseline-on-1" "$x10dBatch/variant-on-1" "$x10dBatch/full-action-audit-1.json"
./out/x10d-p/review/Release/x10d_causal_review.exe $x10dBatch
```

實際build/test/replay commands、exits/logs/XML、初始 workspace/disk、pre-replay snapshot與final analysis binding保存於batch。初campaign+prior8,021,936,506B、disk free31,942,201,344B；新batch≤128MiB、out≤768MiB、campaign+prior≤8GiB，精確結束bytes/餘額以 [final-summary.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/pending-cancel-x10d-p/final-summary.json) 為準。全部logs、失敗、source/export/build、commands與reports計額，沒有PNG副本或刪raw騰空間。artifact-ledger列逐檔SHA/bytes，ledger/final-summary自身bytes亦計入settled總額。

舊dirty及X10c frozen exports/binaries再核，正式src/include/root CMake對HEAD仍無差異。status／forward-plan舊文於追加前另存snapshot。**交付狀態：待總控獨立驗收；cold candidate契約通過不等於正式採用。** X10b standalone suppression仍否決，X10d-O／X11／X12／跨曲未啟動。
