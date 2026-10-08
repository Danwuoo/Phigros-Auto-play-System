# Windows production pipeline probe：預聲明與留存範圍

2026-10-07。使用者已續派開發與 Windows 驗證；歷史交接整理範圍不恢復舊 goal。
R2 `tapaa01` 在 dense1 的 owner coverage／measurement n 停止，後三筆、noise、ABBA
均 NOT_RUN。Tap4＋Hold4 PASS 不構成完整成本資格；本工具不續接該批。

新工具直接 include source `8e953c723efe5ed97af51184739673f15ed8af03` 的
`src/manual_session.cpp`，連結 prodclosed02 同源 frozen core/current/proto；正式 emulator
implementation 不連結。Windows Packet／Wake／pool／perception／action／supervisor／
SessionArchive 為原程式，endpoint／preflight／capture／touch 為明列測試替身。
先執行正式 public entry 的 UNQUALIFIED 拒絕控制；只有 test translation unit 內部 wrapper
可呼叫 implementation。正式 exe 及 admission 不修改。

先做 Tap1000／20ms 有限 smoke，通過程序、生命週期與獨立原始核對後，依序 Tap／Hold／dense
各2560／20ms。256 warmup 全保留；另最多64張 synthetic result pixels 結束 one_round。
每 attempt QPC、全部正式 journal、source/build／程序收據、p50/p95/p99/max/jitter、RSS、
drop／失敗及 skipped 分母完整保留。任何必要失敗停止該 probe，不用 timeout 代替 result。
這是診斷工具，尚無 production AA／noise／ABBA 或 real capture/RPC 資格。

`prodpipe01` 配置前的容量檢查拒絕：累積已保護資料11,783,660,109B，加原1.5GiB
build預留超過12GiB；沒有原生 producer。該容量負結果保留。本 probe 的新維護留存帳本
明確使用 **16GiB累積上限**、本 probe新增≤4GiB、metadata≤64MiB、free≥20GiB；
build／raw預留仍為原1.5／1GiB。這是為保留停止批次與同源 builds 的磁碟預算調整，
不是變更 R2 cost、noise、coverage、latency、RSS 或 process identity 門檻。
舊12GiB帳本與所有 stopped receipts 不回寫，也不以此 probe 授予 R2 PASS。

當前 live validator／qualified token、當前 emulator/app/profile/五指、physical Hold 語義及
完整 IN Miss=0 仍須各自取得證據。本 probe 不操作裝置、啟動 emulator 或授予 admission。
