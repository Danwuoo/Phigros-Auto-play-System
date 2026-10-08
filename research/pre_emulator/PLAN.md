# main 的 emulator 啟動前工作包 — 2026-10-08

2026-10-09 續派範圍見 [一輪分數授權](../../docs/research/live-readiness-20261007/SINGLE_PASS_SCOPE_20261009.md)。
main24／`dc20077` Release 25 組通過；Debug 在 score-only Hold 的「必有明確 Up」
斷言失敗（native 2），後續未跑。另立 diagnostic_archive 組件診斷為 11 Down／0 Up／
85 release calls。新的 cold 邊界分開累計 explicit Up、cancelled contacts、final contacts，
要求 Down = Up + cancelled 且 final=0、Tap/Hold 確有 Down。這是有限生命週期／無持久化
測試，不是 Hold tail 語義或成本資格；不將 cancellation 計入原 Up，原 normal cost 的
Down/Up 必要條件不改。Debug 組件測得 12 Down／4 Up／8 cancel／final0；新來源整包另驗。

本輪整合 object-map 診斷建構、score-only 無持久化、current capture geometry 核對與
只讀 build reader。新增 3 個無持久化測試及 1 個含 13 情境的原 byte oracle，writer
共 19 正式測試；新增 reader controls 及 literal score-only Tap／Hold／Dense／invalid
4 組，三配置各 25 冷回歸。原 22 TUs、archive workload、R2 數值 gates 不變。
完整 live enabling validator 仍須根據實際成功的 production/cost/current device 收據完成，
score-session 公開入口當前保持 UNQUALIFIED，不能用 cold PASS 或授權取代資格。

使用者最新授權繼續完成，實機測試 ready 後建立新的 GPT-6.1 Sol／xhigh local
main task，授權該 task 啟動 emulator／Computer Use 進行有限測試。本 task 完成
冷修補與原門檻驗證後交接；下列較早停止文字按當時範圍閱讀。

新候選保留完整診斷 JSON，只移轉已擁有的 arrays／scene，避免 initializer-list
深層複製。16 MiB archive mailbox、每份 snapshot 256 KiB 計費、gzip level 1
及完整事件內容不變。main14 partial gzip 經 CRC／SHA 完整讀取（360 rows，
11,461,381 decoded bytes），主要負載是完整 candidate／scene，非 ledger 重複忙迴圈。

endpoint-free 合成 producer 採用作用域 THREAD_PRIORITY_ABOVE_NORMAL（1），
normal process class，query／set／readback／restore 失敗皆取消資格；A 與 B 政策相同。
原 QPC epoch、所有嘗試、cadence、等待與 deadline overrun 判定不改。
這是待原成本 gate 驗證的環境修補，不宣稱 Windows 排程保證 deadline。

main20 的 normal Tap／Hold／Dense 已通過完整 raw integrity 與原數值 gates。
8 ms stress Tap producer 完成，auditor 因錯誤 publication 分母停止：10013 produced、
10005 published、7 physical pool drops、1 result stop 後回呼。新 auditor 明確核對
published＋pool drops＋至多一個 result stop 後 tail＝produced，兩處 pool counters
必須一致，所有 requested attempts 仍保留。normal 成本仍要求零 pool drops；
stress drops 不予成本資格。補正只影響分析，不修改三 buffer、runtime 或門檻。
原 native exit 2／STOP 保留，新結果另立 source/build/campaign 身分。

builder 將 production_entry_controls／fault_controls／pas 分成獨立 native groups，
guard held limit 64 不改；所有 native build 完成後串行封存 final executable PDB，
逐 entry 讀回 SHA，再刪除 originals，最後封存 build-binding。Compiler PDB 保留。
歷史 analyzer 單元 fixture 在隔離目錄用当前 source copies＋byte-exact Git legacy
protocol，正式 R2 protocol 不變。compiler 6 GiB 帳本只含 compiler／evaluation／
fixture／planning；全部 raw 和保留 archives 仍進入 48 GiB 總帳。新 production
狀態區分 FAILED_EXECUTION／FAILED_STAGE／NOT_RUN，失敗原件不改。

使用者授權繼續實作，**到需要啟動 emulator 就停止**。本包只執行 endpoint-free
Windows 建置、fixed pixels、R2 AA/noise/ABBA 和 literal production cold stages；
不啟動 emulator、ADB、遊戲、真 capture/RPC 或 touch，不新建 goal／worktree。

所有新來源、binary、native Job／input closure、raw 分母與失敗均按現有 gate
保存。R2 cadence、256/2304/2560、noise 公式、coverage／lateness／RSS 門檻不改，
失敗停止該批，後續 NOT_RUN；有具體修補才能建立新的 campaign，不能挑成功重測。

新的 unified CMake project freshly 編譯正式 22 TUs 及完整 production branch，
和 endpoint-free cold tests 共用同一 pas_core；pas.exe 只建置不執行。literal
manual_session 包含原 translation unit，替換的只有 capture/preflight/touch 邊界。
原 main07 三配置 compiler outputs 壓縮保存，每個 entry SHA 讀回核對及 originals
再次核對後才刪原 build roots；原始 raw、source、失敗與 receipts 保留原樣。
48 GiB retained／6 GiB candidate／12 GiB cost raw／20 GiB free 等界限不變。

完整 live reader／qualified token 必須以真實成功 production、cost 及當前
device/profile/physical 收據建立 schema；沿用交接的信任契約，不先創造可啟用
格式。本包在需要 emulator 的 profile/physical/capture/RPC 階段停下，入口保持
UNQUALIFIED，cold/comparison 的 PASS 不授予實機資格。

main10 的新 auditor 字串／JSON 比較編譯失敗保留完整 negative receipts；
main11 改成明確型別，先編譯 auditor／batch helper 再新編全部正式 core。
小型 executable 每组三個 native build，64 個 held identity gate 不變；
每組完成後將非 runtime 的 .ilk／.map 壓縮讀回 SHA 核對，再移除原 cache，
執行檔、PDB、objects、libraries 與原始證據保持可用。

main11 所沿用的 builder 未完整封存 Debug/lib、Debug/bin、protoc／plugin。
依賴清單修補會使正在收尾的 source closure 明確 STOP，native stage 不強制終止。
main11 整包不予資格；main12 在 configure 前封存已安裝 triplet 全部檔案與 toolchain，
同時覆蓋 Release／Debug library、runtime、package metadata 及 protobuf 產生工具。

main11 asan source withdrawal at native stage build-prelive-pre-emulator-asan-main11-targets-01: dependency closure incomplete; wait for natural native completion, never issue qualification.
