# Journal serialization 成本修補：新候選，尚未資格通過

2026-10-07，從 frozen R2 source `8e953c723efe5ed97af51184739673f15ed8af03` 派生。
tapaa01 的 Tap4/Hold4 PASS，dense1 在 coverage/n 停批；所有原件保留，新候選不續接原批。

只改診斷輸出：Journal 的 opaque prepared record 保留原 schema／clock fields，
成本 host 序列化一次後交 writer，原 JSON overload 仍可用。host仍按完整編碼加128B
餘量核對262144B／65536rows／1GiB硬界，沒有刪除記錄或挑選成功子集。
SessionArchive event 在 mailbox 鎖外序列化一次，鎖內只核對原8192／16MiB容量；
writer用cached bytes解扣，完整raw row不再重編碼。completion summary仍由writer加hash／result。
觀測／關聯／ledger／排程／pixels／到期／未知Down／正式live admission均不修改。

先三配置完整重建 formal core/current；新 encoding／drop／fault／drain／archive byte-bound
控制與原 session archive／lifecycle、owner／producer／Tap／shared seam／finish 回歸。
不重用舊 core objects。完整3063 prefix須由新binary重算並獨立audit。全部成功後另預聲明
新成本campaign，沿用R2 scenes／cadence／n／warmup／cost／noise／RSS門檻；舊STOP不續跑。
正式production topology工具亦須新core同源重建與核對；不能混用8e production libraries。

新維護留存累積≤16GiB、本候選新增≤4GiB、metadata≤64MiB、free≥20GiB，build/raw
仍保留1.5／1GiB。原12GiB負結果與所有frozen receipts不回寫。所有buffer仍固定硬上限。
目前不是成本或live資格；emulator、RPC／五指／physical／完整IN Miss=0仍未驗收。
