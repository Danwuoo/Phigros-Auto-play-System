# 同 consumer 的 owner 到期喚醒

2026-10-07，使用者要求繼續開發。AA04 STOP、CD frozen source/build 及所有舊 raw 保留於
`PAS-windows-readiness-20261007`，本候選在独立 sparse Git worktree。

舊成本 host 在沒有 frame 時跳過 owner.poll。20ms producer 間隔可讓18ms Tap Up
等到下一個 frame，原 full phase p99=18.832ms 超過15ms。新增同線程 poll-before-wait，
wait=min(scheduler next_due,10ms watchdog)；next_due 也包含獨立 plan／gate expiry。
producer cadence／corpus／A與B／完整 Journal／三線程 topology／門檻均保持原契約。
此處只修 endpoint-free host，不宣稱正式 production action thread 或真 capture/RPC ready。

`develop.ps1 -ReceiptRepo <保留舊收據 checkout>` 使用原已資格驗證 v3 Job guard，
凍結新 source commit、來源與輸入閉包。Release／Debug／ASan各使用原且 source byte joined
的 formal_core donor，重編變更 host 與 action seam，執行新到期／未知結果回歸及既有
disabled seam／finish／workload 控制。原83 cold stages只對未改演算法保留，不冒稱新跑。
候選 cost binary 為本輪新 current_chain.exe；舊 analyzer/auditor 原碼未改，可原 SHA 重用。

新 source/binary 與事前 freeze 必須建立獨立 AA 批次。任一必要 gate 失敗仍 STOP，
不得補跑停止批次、刪warmup、放寬15ms或將 raw 舊結果算成新結果。
入口維持 UNQUALIFIED；使用者選曲及按Play，待真資格驗證及啟用入口完成。
