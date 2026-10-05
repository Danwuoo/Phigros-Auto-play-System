# X10d-P 預先 protocol：pending missing 單一 owner hook

2026-10-03（Asia/Taipei），實作／測試／新 replay 前保存。C36h tint1 是 behavioral／experimental baseline（77 Miss 未改善）；main50 是 mechanism donor/control，live0。

問題：已知尚未 Down 的 plan，在 owner 接受較新完整 snapshot 且目標缺席後，是否應立即撤銷？只移植 main50 `GamePlanOwner::accept` missing loop 的 `submitted && executed_steps(intent)==0` 分支與原 reason `pending_down_current_object_missing`。保留 C36h cancel_contact、alias、drag group、退休、grace、lease、observer 與 association。X10b suppression **OFF**，不做 X10d-O。正式 src/include/root CMake 不改。

來源：reference/candidate 均從 frozen `out/x10c/baseline` 複製（其 parent 為 `out/x1/c36h-v3`，無 suppression）；先核 X10c pre-replay binding、baseline provenance、source/export/binaries。候選相對 reference 只新增這一分支；inverse patch 全文核對。原 baseline tests 原樣保留。新工具重用 X10c reader/diagnostics/audit，另有單向全 prefix compact lifecycle stream；首 replay 前保存 exact source、manifest、provenance、binary、DLL SHA，之後不重 build。

時間：沿用原 input 的 capture_complete / pixels_ready QPC，fake=original-first_capture+1s。capture arrival 更新 envelope；capture→ready 期間仍可執行已到期動作；ready 時 perception/owner acceptance 為零 fake duration。due < boundary 先 poll，due == boundary 維持 **frame-first**：先該 boundary，ready 時先 accept 再 poll。capture==due 但 ready>due 時，due 可在 ready 前合法執行。沒有新 frame、尚未 accept、未消費 frame、舊 sequence/context、非法 gate/stale snapshot 均不等於 current absence；不能由未來 pixels 追溯禁止已發生 Down。來源 render age、原 recognition race 與早期 state unknown。FakeTouch 五指 success/零 RPC receipts；另用合成 unknown receipts 驗守門。

先紅後綠：同一套完整 owner/scheduler/FakeTouch 新契約先連未改 reference，保存預期失敗，再連 candidate。涵蓋 Hold/Tap/Drag/Flick pending 缺席與新 intent；已 Down、completed、unknown、缺 cursor、prefix_offset 修訂不得重試；無新 frame/拒絕 snapshot；due 前/後/同時、between-frame、capture/ready 邊界、gate/reset/context/epoch；active body/rails 無 root、短缺失、旋轉、patch/front guard、alias、shared Drag continuity。Release 原207項完整執行；candidate 的舊 `SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt` 預先允許失敗，逐 assertion 保存，不改期待、不略過。任何其他非預期 regression 阻止採用。Debug-ASan 跑新 owner 契約及候選原 suite（已宣告契約差異亦保留），不宣稱 ASan 真錄 replay。

真錄：7722 PNG 逐 SHA、32 preroll、7715 perception、原 owner-consumed cadence；七窗 H2440–2495/K2865–2895/A3493–3504/B4979–4990/C5516–5524/E5281–5293/D6158–6220（196 幀）。recorded touch 只外部 comparison，runtime IDs 只 local join。最多4次新 full replay，失敗亦計次：預定 reference ON、candidate ON/OFF 共3次；reference ON bridge 回 X10c baseline ON 的全 events/semantic/state digests，再由 frozen X10c ON/OFF 已核相等提供 reference OFF control，減少重跑。新 reader全 prefix lifecycle stream只在 ON 輸出，≤32MiB/run；每幀 targets/identities≤128，總行≤36000。candidate ON/OFF 全 events/semantic/state digests須一致。新窗口如必要，只在剩餘一次 replay 前修訂 protocol，不無界追加。

完整 action audit至 EOF：exact bounded LCS≤10000 actions／25 million cells，保留時間/座標/source frame/receipt/failed release，報四類全部分母、所有 unmatched、第一 causal divergence、取消後返回/重建/沒有返回/lost opportunity、active continuity、退休。用全 prefix compact records與events逐一解釋差異，既有窗只作深入 trace。觀測 scene/bank/history 每幀應與 reference 相同；diagnostics不得回饋策略。K 窗 suppression OFF 時可能完全不變，不硬追消失。

成功：候選新生命週期契約及非預期回歸通過，完整差異都有由本 hook 到 receipts/退休的可核因果鏈、active contact 不被 hook 取消，source/binary/ON-OFF/bridge/容量全部通過。這只稱冷候選契約通過並待總控獨立驗收，不稱 Miss 改善、physical identity 正確、gameplay 採用或跨曲泛化。否證：非預期重複 Down/未知重試/完成復活/active 過早 Up/unsupported Move、不可解釋全 prefix 差異或 bridge 不同即阻止採用；不調 grace/threshold 或加第二策略救結果。lost opportunity 單列且遊戲效應 unknown。

容量：新 batch `measurements/game-assist/2026-09-30-m0-manual-continue/pending-cancel-x10d-p`≤128MiB，新 `out/x10d-p` export/build≤768MiB；campaign+research-next-20261001≤8GiB。開始重量 disk/free/bytes；失敗/log/XML/source/commands/reports全部計額。PNG只引用、不刪原raw、不覆寫任何 frozen X1–X10c。保留 dirty，不 reset/clean/commit/push。無 emulator/live/訓練/goal/額外 chat。完成更新 status/forward plan為待總控獨立驗收，Verified/Strong inference/Hypothesis/Unknown 分列。
