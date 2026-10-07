# Windows Job 成員身分修補 v2

2026-10-07。使用者明確要求修復並評估原 Tap3 `member-identity-untrusted:root-exit`。
這是必要維護守衛，不是遊戲策略、正式成本分析器或實機 admission。

原 `research/x10d_o_bvi_build/owned.cs`、v1 adapter 與所有舊 STOP 保持原位。
v2 在根程序存活期間，每次原有 25 ms 等待循環嘗試辨識 Job 成員。只有完成
membership-before、creation、image、creation-recheck、membership-after 的成員
才持有 handle 與身分；最多 64 個，達限即拒絕。過渡中的不完整取樣計數，不宣稱
完整程序歷史；四個必要 snapshot 的完整性與 fail-closed 判定保持。

退出時優先重新查詢。映像查詢失敗時，只容許同一已持有 handle、同 PID／creation、
已驗證的非空 image，且該 handle 已 signaled；仍重查 creation 與 Job membership。
未知、仍活著且不可辨識、換 handle、PID／creation 不符，均拒絕。不按 conhost
名稱放行；本輪控制實際觀測到 conhost，不能据此認定舊失敗 PID 21012 的映像。

v2 的 checkpoint 上限由 16 KiB 明列為 64 KiB，最多 8 個；result／verification
同為原 64 KiB。這容納實際建置工具控制的 42 個持有身分與等待記錄；超限仍拒絕。
`VerifyProcess`、identity publication、共用清理期限、stream cap、Job-only termination
及 transaction state predicates 直接沿用原檔。每次 launch 綁定 v2 source SHA
與 v2 qualification receipt，不用 v1 的控制收據批准新守衛。

9 個快取正／負控制與原 39 個 identity/wait/budget oracle 均通過；四個實際 native
控制通過，包括 exit 7 與未結束子程序的預期 runner 125／owned cleanup。另有
24 個 console 退出控制、3 次原 Tap3 analyzer 的唯讀診斷，全部 COMPLETE。
這些診斷不把舊 AA batch 改判合格，也沒有重跑 producer。

保留的新負結果：native-01 的最後 checkpoint 超過舊 16 KiB；evaluation-01
的 27 個單次收據全部完成後，彙總超過預設 64 KiB，留下原空檔。新版 review 只
重新讀取並核對那些收據，以 1 MiB 硬上限寫新彙總，不重跑控制。

`run-aa.ps1` 建立全新 AA12 campaign，重用逐 byte 核對的 CD candidate/build/
corpus/profile，另列 maintenance commit 與 guard v2。原 threshold、順序、分母與
noise 規則不變。任一必要 gate 失敗即停，不復活旧 STOP、不接續 Tap4 来补旧批次。
AA/noise 未通過，不做 ABBA；任何冷成本結果都不代表 physical 或全曲 IN Miss=0。

Microsoft 文件說明 [image query 的 handle 權限與錯誤回傳](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-queryfullprocessimagenamew)。
[Job completion-port 文件](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_associate_completion_port)
明示一般通知不保證送達且有 race，因此不拿通知替代必要 membership／identity 核對。
