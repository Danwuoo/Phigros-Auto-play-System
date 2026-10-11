# 一次冷排程診斷

使用者已回答可以提供管理員環境；只讀 UAC preflight 實際回報 admin=true、PID19852、recording_started=false，收據為 `out/planning/cold-scheduler-admin-preflight-r01.json`，SHA256 `e5fe6af17eeba115d950d56d1868f3478a4f2ff168ba38d4a2b12796b8009a5f`。本輪仍不啟動 emulator、裝置或新 thread。

僅新增獨立 C++20 ETW controller、watchdog 及只讀 reader；不修改正式 runtime、producer wait、優先序或原 gate。原 main32 的75組／production9 PASS、Normal55 STOP、AA r01／r02／r03及 main57 三場診斷原件保留。必要建置及受影響控制通過後，只執行一次 diagnostic-only Tap2560／20ms，compiled component57／base55／新工具與實際 execution 身分分列，所有資格仍 false。

診斷限75秒（含 session startup）、normalized ETW buffers≤64MiB、單一 raw ETL≤128MiB。只啟用 CSWITCH＋DISPATCHER＋NO_SYSCONFIG（mask0x10000810）與 QPC。將原待審 v02 的 circular 改為 sequential，上限不提高；順序檔達上限停止寫入，避免循環覆寫造成的覆蓋未知。[Microsoft 記錄模式](https://learn.microsoft.com/en-us/windows/win32/etw/logging-mode-constants)。不得增加 process／image／file／network／stack／DPC／ISR 或遊玩紀錄。

watchdog 先就緒，controller 成功建立新 named SystemLogger 後發布 exact handle／GUID／name／file／logger thread／固定 deadline。兩程序以各自256MiB committed-memory Job readback 約束、獨立持有 handle；既有 producer 仍走原 Native8 guard。只清理本次成功建立且身分核對的 session，collision不接管、不更新或全域停止。診斷工具輸出總量≤8MiB，reader summary≤1MiB；ETW buffers/kernel overhead與 committed memory 不混稱 heap 或 RSS。

reader 使用 RAW_TIMESTAMP、ETL PerfFreq及當輪實際 producer TID。component57 端點是 HostClock 轉換後的 absolute QPC-ns；當 frequency=10MHz 且 ns可被100整除時才精確反算 raw ticks，不猜 UTC 或首事件 offset。只報 scheduled-running／ready／waiting／unknown 的不重疊區間；scheduled-running不能證明沒有 ISR／DPC／VM／firmware 中斷。loss、schema、finalization、cover／join缺失及不完整區間照報 UNKNOWN。

此診斷有擾動，不能替代 current Normal3／AA12／noise18／ABBA36／完整 readonly 或授予 admission。Windows API完成時間沒有硬即時保證，程序與 watchdog 的停止請求時間、實際停止收據及任何超時須如實保存。單次未重現不宣稱修好，也不為取得較好結果重試。

main58／eec2167 的三TU建置及controller32／reader20共52控制實際通過，configure／build／兩控制4個Native8均0，原收據保留。交叉審查修正runner的ready→request時間／身分join、STOP final-flush statistics及controller／watchdog actual exit核對；新維護來源另作fresh必要工具建置，原main58結果不改。512MiB約束指兩個C++診斷程序的 committed memory，PowerShell維護launcher不納入此數字；reader512MiB僅在兩程序實際退出後使用。

目前 recording NOT_RUN、cost NOT_READY、live UNQUALIFIED。
