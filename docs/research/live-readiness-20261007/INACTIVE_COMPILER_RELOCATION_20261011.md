# Inactive compiler relocation

71 inactive compiler roots、2593 files已逐entry ZIP解碼核對bytes／SHA，再核對全部原件，
保存所有proof後才以LiteralPath逐檔清除。原2,495,727,046 bytes，ZIP356,484,970 bytes；
compiler payload減少2,139,242,076 bytes，扣除proof metadata前約1.99GiB。
同期新增main45建置與source snapshot，實際retained帳本差額為1,552,509,702 bytes，
不能把兩種分母混稱同一容量回收量。

[完整索引](INACTIVE_COMPILER_RELOCATION_20261011.json)綁定result、完整original-file-manifest、
每個original root／ZIP／proof。歷史source、binary SHA、負結果及STOP保持原值；
被搬移的舊compiler路徑已無原件，需依ZIP／entry／SHA還原後才能在原位置重新核對。
不冒稱原binary仍在舊路徑，也不把封存當新資格。

main32 Release、当前候選、dependencies、sources／snapshots、raw、reports及corpus根
未移除，empty compiler directories保留。Retained上限48GiB及minimum free20GiB不變；
device_operations=0、qualification_issued=false、live_ready=false，不建立新thread。
