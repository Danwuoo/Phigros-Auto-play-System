# main 的 emulator 啟動前工作包 — 2026-10-08

2026-10-09 續派範圍見 [一輪分數授權](../../docs/research/live-readiness-20261007/SINGLE_PASS_SCOPE_20261009.md)。
2026-10-10 main30／`f6548e5` fresh22 TUs三配置各25、完整圖3／prefix3063／fault4通過。
production normal Tap／Hold／Dense原數值及raw通過，stress Tap／Dense與Debug Tap integrity
通過；stress Dense producer0／123.951s，原mailbox overrun未重現，normal cost false保留。
Debug Hold producer0／auditor2：五個Down各有中途成功backend release，原auditor直到最後
才處理release，錯把下一個不同intent的contact重用當搶占。907 formal及guard／方向共911
SHA snapshot、原raw／stderr／STOP保存，ASan後兩項及AA12／noise／ABBA36 NOT_RUN。
盤點 `out/prelive-pre-emulator-planning/main30-stage-inventory.json`；cost NOT_READY／live
UNQUALIFIED。新診斷join按原journal順序核對receipt及release的sole-owner QPC；最多五個
own active IDs、dispatched、failed／unknown空、完成後才重用。原Down intent不復活，
Move／Up必有未釋放的own Down；release單獨計數、不充explicit Up或game effect。
原67＋新26共93控制及七份原完整raw組件核對通過；normal原數值／stress與Debug負結果照報，
不授candidate資格。builder每配置要求93 controls；新來源仍fresh三配置及其後原全包。
runtime、完整archive workload、容量及R2門檻不改；未啟動裝置。

2026-10-10 main29／`208ad7b` 三配置各25、writer20、auditor67、fresh22 TUs及
三份完整圖／3063 prefix通過。capture／writer故障及audit通過；RPC producer0、
auditor0/control_pass=true但guard runner125：root-exit未驗image的PID9008 error5，
image unknown，不能推定conhost或以active0補足身分。slow／production9／AA12／noise／
ABBA36 NOT_RUN，原status將已執行RPC寫NOT_RUN的錯誤也保存；以
`out/prelive-pre-emulator-planning/main29-stage-inventory.json`更正索引，不改原件。
原906 formal source及guard／方向文件共910份SHA snapshot保存。
guard在resume後立即acquire、首50ms sleep1ms，之後原25ms；原image／held／snapshot／
shared50ms drain與所有時限不改。39／9／新12 controls、native4及固定short RPC24
診斷通過；組件不授candidate資格。新gate main06；reader核對新增controls及shared
source binding。新fault status區分已執行失敗與真正未跑；原R2／runtime／archive不變。
新整合重新fresh三配置及其後全包，不復活main29 STOP或採挑選成功的重測。
2026-10-10 main28／`62a8000` 三配置 fresh22 TUs／各25組（writer20項）全部通過，
3063 prefix／fault4／production normal Tap、Hold、Dense 完整 raw及原數值 gates通過。
stress Tap producer0／85.429s，auditor2：原取消核對僅接受Hold，raw有六次已Down的Tap
於物件消失後取消Up。原905 source SHA snapshot／raw／STOP／後五項與AA/noise/ABBA
NOT_RUN保存，不能授成本或live資格。新取消核對保留原own contact／position／最新plan／
100ms evidence lease／current packet QPC／唯一收據／executed prefix要求；僅增加兩種
明列Tap basis、原Down+Up兩步／18ms間隔／cursor1／40ms missing grace的取消Up。
Down／Move不能用此路徑；Drag／Flick／未知kind仍拒絕，Tap不得冒稱Hold或game effect。
原39＋新28控制共67及原完整stress Tap raw獨立組件核對通過，六次取消皆有own Up；
原68 pool drops、coverage .8855、producer2／callback3 overruns與normal cost false照報。
組件不授整體資格；新整合仍重新三配置及所有後項，原runtime／策略／R2 gates不變。
main28只讀圖：Release通過，Debug攤平8247檔超單stage8192；修正為凍結完整transitive
metadata，由原C++ Reader逐檔SHA並finish再驗，原native/graph caps不改。Debug通過；
ASan reader877.054s watchdog停止且清理active0／held/streams完整，原負結果保存。
以同source sealed Release reader核對完整ASan圖通過（291.359s），明列核對工具配置。
`build_graphs.ps1`正式化此核對路徑；三配置的實際binaries／instrumentation／25 controls
仍各自保留，reader始終只讀、不發admission。新來源整包須另驗，未啟動emulator。

main27／`2082a9d` Release／Debug 各25通過；ASan19通過後 score-only Tap native2
（22.398s）：未從 result pixels 完成，沒有 control-report，不能推定觸控計數。
原件／904 source SHA snapshot／STOP 保存，後五項與 graph/prefix/fault/production/AA/ABBA
未跑。原 core 的結算診斷 n16，total p50/p95/p99/max=94.921/113.172/115.639/116.256ms，
5張超過原100ms freshness，六標籤均辨識正確但確認重置；不是記憶體報錯。
新 matcher 對固定 label ROI（至多22×2 uint64、352 bytes）一次計算白色像素，
以 constexpr template rows／popcount 算原 intersection/union；±3px、.72門檻、
色彩條件、完整標籤、source freshness／result confirmation／所有期限不改。
component23 原19項及新精確原演算法 parity 共20項通過，含49平移、跨64位元边界、
缺標籤、threshold colors、noise、HUD/menu/invalid；ASan Tap/Hold/Dense/invalid 均通過。
新結算 n16 total p50/p95/p99/max=31.705/46.641/47.752/48.030ms，無超100ms，
第三張 result 確認。兩次為獨立診斷，未作 R2 noise/ABBA 比較或成本資格。
新整合每配置仍25組，writer必須20 real tests；fresh22 TUs／其後原 gates 全部另驗。

main26／`f90d345` Release／Debug 各25通過，ASan19通過後 score-only Tap native2：
已正常 result／停止／無檔案／contacts0，但額外的 Down positivity 失敗。原件保存。
archive 對照亦 Down0；較慢 pixels component19 仍 Down0，試驗撤回並保留來源／負結果。
ASan 的 no-persistence 功能測試明列 instrumentation integrity、非即時資格，允許
unchanged deadline 下安全不 Down；所有已發生 Down 仍恰有 Up 或 cancellation、final0。
Release／Debug Tap／Hold positivity 保留，builder 核對 test-only instrumentation flag
與 mode，report real-time qualification false。原 runtime、pixels、profile、時限、
production9 與 R2 AA/noise/ABBA 數值 gates 不改。ASan component20 四組通過不授整體
candidate；新整合重新完整三配置及其後 stages。

main25／`1f1266c` Release 25/25 通過、Debug 21 組通過後在 score-only Dense 的
synthetic result deadline 失敗（native2／79.077s），後續 NOT_RUN，原失敗不改。
Debug Dense rendering 使 workload 落後 epoch，terminal64 slots catch-up 成 burst；
score-only 功能 fixture 將 result epoch 設 max(planned end, actual completion)，
64 slots／原 cadence／2s terminal deadline 與原 requested due times 保留。
只有 score-only fixture 啟用；diagnostic/cost producer 預設 false，正式 runtime、
archive workload、原 auditor absolute QPC 契約及 R2 gates 不變。
四項 fake-clock＋Debug Dense 組件通過（80.507s／final0／無 gameplay files）；
不能授予整體 candidate 資格，新來源仍須重新完整三配置及其後 stages。

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
