# 一次冷排程診斷

使用者已回答可以提供管理員環境；只讀 UAC preflight 實際回報 admin=true、PID19852、recording_started=false，收據為 `out/planning/cold-scheduler-admin-preflight-r01.json`，SHA256 `e5fe6af17eeba115d950d56d1868f3478a4f2ff168ba38d4a2b12796b8009a5f`。本輪仍不啟動 emulator、裝置或新 thread。

僅新增獨立 C++20 ETW controller、watchdog 及只讀 reader；不修改正式 runtime、producer wait、優先序或原 gate。原 main32 的75組／production9 PASS、Normal55 STOP、AA r01／r02／r03及 main57 三場診斷原件保留。必要建置及受影響控制通過後，只執行一次 diagnostic-only Tap2560／20ms，compiled component57／base55／新工具與實際 execution 身分分列，所有資格仍 false。

診斷限75秒（含 session startup）、normalized ETW buffers≤64MiB、單一 raw ETL≤128MiB。只啟用 CSWITCH＋DISPATCHER＋NO_SYSCONFIG（mask0x10000810）與 QPC。將原待審 v02 的 circular 改為 sequential，上限不提高；順序檔達上限停止寫入，避免循環覆寫造成的覆蓋未知。[Microsoft 記錄模式](https://learn.microsoft.com/en-us/windows/win32/etw/logging-mode-constants)。不得增加 process／image／file／network／stack／DPC／ISR 或遊玩紀錄。

watchdog 先就緒，controller 成功建立新 named SystemLogger 後發布 exact handle／GUID／name／file／logger thread／固定 deadline。兩程序以各自256MiB committed-memory Job readback 約束、獨立持有 handle；既有 producer 仍走原 Native8 guard。只清理本次成功建立且身分核對的 session，collision不接管、不更新或全域停止。診斷工具輸出總量≤8MiB，reader summary≤1MiB；ETW buffers/kernel overhead與 committed memory 不混稱 heap 或 RSS。

reader 使用 RAW_TIMESTAMP、ETL PerfFreq及當輪實際 producer TID。component57 端點是 HostClock 轉換後的 absolute QPC-ns；當 frequency=10MHz 且 ns可被100整除時才精確反算 raw ticks，不猜 UTC 或首事件 offset。只報 scheduled-running／ready／waiting／unknown 的不重疊區間；scheduled-running不能證明沒有 ISR／DPC／VM／firmware 中斷。loss、schema、finalization、cover／join缺失及不完整區間照報 UNKNOWN。

此診斷有擾動，不能替代 current Normal3／AA12／noise18／ABBA36／完整 readonly 或授予 admission。Windows API完成時間沒有硬即時保證，程序與 watchdog 的停止請求時間、實際停止收據及任何超時須如實保存。單次未重現不宣稱修好，也不為取得較好結果重試。

main58／eec2167、main59／867707d及main60／fb7aa92的三TU建置及controller32／reader20共52控制實際通過，每輪configure／build／兩控制4個Native8均0，原收據保留。交叉審查修正runner的ready→request時間／身分join、STOP final-flush statistics及controller／watchdog actual exit核對。512MiB約束指兩個C++診斷程序的 committed memory，PowerShell維護launcher不納入此數字；reader512MiB僅在兩程序實際退出後使用。

main60執行的r01（原目錄標籤main58-r01）只有preflight：actual admin=true、session查詢無collision，但in_outer_job=true被原封裝一律拒絕。ETW start／stop、recording及producer Native均未執行，[原STOP及準備收據](MAIN60_SCHEDULER_PREFLIGHT_STOP_20261011.json)不改。Windows允許符合條件的nested Job；修補改為在ETW前實際Assign／readback自身256MiB process及job限制，不調整或繞過host Job。[Microsoft nested Job規則](https://learn.microsoft.com/en-us/windows/win32/procthread/nested-jobs)。controller及watchdog各持自己的Job，但共同祖先Job仍可終止兩者；不宣稱能存活host終止。新必要工具建置／52控制通過後使用全新r02輸出，不覆盖r01。

main61／eb6ed0b三TU／52控制及4個Native8均通過，r02 actual nested Job／collision preflight通過。StartTrace實際成功，但Windows加入預設`EVENT_TRACE_STOP_ON_HYBRID_SHUTDOWN`，mode由0x02000001成為0x02400001，原精確比較拒絕。controller退出2，StopTrace0及exact handle absence確認；final-statistics仍false。watchdog未取得owner、deadline後退出2／ttl_overrun=true照報。原128KiB ETL、STOP及準備metadata見[保存索引](MAIN61_SCHEDULER_STARTUP_STOP_20261011.json)。producer／auditor／reader均未啟動，完整量測0；實際recording attempt1，不能再稱recording NOT_RUN。

修補明列官方支援的hybrid shutdown stop policy，requested／readback exact mode固定0x02400001，其餘flags與上限不變，不任意忽略未知旗標。新的必要工具／52控制仍須actual驗證；未執行第二次recording，cost NOT_READY、live UNQUALIFIED。

main62／6e3755d修正版三TU／52控制及4個Native8均通過。使用者明確另授追加一次後，r03 producer Native0／52.7384801s，ETL51,183,616B及完整raw保留。root在ClosureReceipt產生前取ProcessEvidence而STOP；修補改為STOP後完成原after／ClosureReceipt，再取完整收據。controller requested_stop／StopTrace0／exact handle absent confirmed，但STOP回傳被當成存活session核對，原final_statistics_verified=false／exit2照報；不能用停止前query的zero loss補稱最終統計已驗證。原件見[量測STOP](MAIN62_SCHEDULER_MEASUREMENT_STOP_20261011.json)。

main63／09a70d9僅離線處理同一份raw：原base55 auditor Native0／1.569277s，完整11檔／3,381,642B integrity通過；producer／callback超期2／2、lateness最大36.1862ms，原數值cost=false。reader62 Native2／0.134905s，最大adjacent gap36.553ms（attempt1549／TID27284）在事件版本硬限制處停止，ProcessTrace1223／未EOF／UNKNOWN；原[離線负結果](MAIN63_OFFLINE_READER_NEGATIVE_20261011.json)保留。新維護只編reader一TU，原20state controls與新8schema controls分列；先取得bounded actual descriptor／TDH schema catalog，未知版本保持UNKNOWN並讀至EOF，不預設支援。原audit不重跑、producer／recording新增0，所有資格仍false。

保存索引加註：main60／61的aggregate original-pin total bytes欄位因維護sum方式錯而記0；逐pin bytes／SHA及讀回均正確，原索引不覆寫。由保存值逐scalar重算為main60 1,964,594B／46pins、main61 2,166,137B／79pins；main62已用逐scalar sum，57,705,796B／118pins。這些索引加總不替代原容量逐檔帳本。

2026-10-12 續作 main66／8246156：新 reader 單TU建置及原20＋schema8控制通過，三個Native8均0。原main64 configure的尾斜線／Windows argv負結果另見[原件索引](MAIN64_READER_CONFIGURE_NEGATIVE_20261011.json)；main65因新保存文件抵達clean-source檢查而在Native前停止，所有原位置保留。

同一ETL的actual catalog已確認CSwitch descriptor version5／28 bytes，TDH回ERROR_NOT_FOUND(1168)；ReadyThread version2／8 bytes的TDH schema可用，但其ProviderGuid是SystemTraceControlGuid、EventGuid才是ThreadGuid。原reader混用兩GUID而在ProcessTrace1223停止，EOF=false、loss仍UNKNOWN；[原建置、catalog與STOP索引](MAIN66_SCHEMA_READER_NEGATIVE_20261012.json)保留。新最小修補將 observed CSwitch5／exact28以Microsoft解析器支援的24-byte prefix解讀，保留extra4 bytes未解讀及原actual版本；ReadyThread仍核TDH named scalar與正確GUID角色，不把任意版本假定為2。[Microsoft CSwitch與ReadyThread解析器](https://github.com/microsoft/perfview/blob/main/src/TraceEvent/Parsers/KernelTraceEventParser.cs)、[TDH TRACE_EVENT_INFO](https://learn.microsoft.com/en-us/windows/win32/api/tdh/ns-tdh-trace_event_info)。原20＋schema8分母不變，另列prefix12必要控制，尚待新Native建置及同檔重讀；不新增recording／producer／auditor、noise或ABBA量測，不改原cost、STOP或TID lifetime未知。

為把75秒用於實際量測，producer的原 input-closure／capacity／params核對在session前完成，原owned guard／Native8照常執行；停止session後再做原after核對。容量以.NET逐檔enumeration加總，48GiB／minimum-free20GiB／原reserve不變；補計先前main32→base55原短路徑副本`out/s55aa03`，不重hash原大型raw／libs。
