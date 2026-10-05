# R2F control交接：三控制成功，configure-release工程STOP

唯一新 attempt `bvi-r2f-20261005-03`，授權及容量限額見新 batch scope-amendment；詳 [結果](HOLD_OWNERSHIP_X10D_O_BVI_R2F_CONTROL_RESULT_20261005.md)。此 STOP 不能續跑或改 source 重試。

- 共享 identity 發布／消費及 held-handle 終了等待完成修復。helper25、transaction32、control39 各一次全通過；預驗 native0，cross-shell 5 項拒絕成立。沒有第二輪或改 oracle。successful shared helper SHA = freeze。
- 三項真控制各一次通過：natural0/0/0、nonzero7/7/0、owned-child0/125/0。第三項 parent poll 有真 pending→empty→ready 證據，child25420/conhost7016 在完整15.0122752s末仍在 Job，單一 shared deadline 下三個 held handles 均 signal，active_final0、streams complete、errors0、cleanup_verified。
- configure-release 第一次回傳1/1/1，cmd stderr「The syntax of the command is incorrect.」。無新 out/CMake log/binary；精確 offending line 未 trace，故分類工程封裝失敗，非 BVI counterexample。不得省略失敗、改成 skip 或引用舊控制成功來續跑。其餘9 product commands未執行。
- STOP revision9、consumed4、known launch4、receipt3；STOP-readback 新 shell 拒絕同 configure 與下一 build。所有23 frozen來源及6fixture/coverage inputs、舊 geometry ref、dependencies SHA 重核。R1 candidate/threshold/contact adapter 與89×4/22/4/1128rows2392refs未變。
- 新source `research/x10d_o_bvi_r2f_control/`；新batch `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f-control/`。先讀 final-receipt、artifact-ledger index/shards、stage-summary、failure-classification、source/input/dependency/binary manifests，再讀四項正式收據、預驗 oracle/results、contract/freeze/argv及STOP-readback。final self hash排除但self bytes計入，ledger index由final SHA錨定。
- 保護舊3100檔/3394refs與原569 Git paths、HEAD/index/dirty；舊R2F/resume/R2E根未寫。容量carry8285248004/8335047/1299900/oldout374705，新dev子限40902683、newout134217728、controller7088708保留；所有新source/docs/scratch/失敗/manifest/ledger計入。精確容量見 final receipt；estimate correction0，newout0、未刪舊raw。
- BVI CXX build/assertion0，wrong-contact未跑，Debug/ASan未設定，compiled及loaded dependency closure未知。PNG0/2，geometry review按SHA `e80ab11234bd8ed8d4d64f0e6354e4dca1dec6f3e4aa0e7d7e801d53207effb0` 引用；合法ROI/all-lines、physical owner、成本/runtime/live尚缺。無產品採用。

可獨立驗收的新增進度是三控制與共享預驗，尚不能簽 BVI cold 或實戰。若總控另立授權，下一個具體工作是先對 Windows 命令封裝建立獨立可重現的語法／argv檢查，再決定新的凍結與建置測試包；本 chat 只交付此次 STOP，不新建第四 attempt，不消息續派。README/Legacy入口等現行方向不更動，沒有emulator、ADB、觸控、manual-session、models、runtime/cost/stress/full replay、goal、automation、commit或push。
