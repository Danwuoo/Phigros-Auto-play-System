# Windows Job 修補 v3：已驗身分與退出時間窗

使用者 2026-10-07 明確授權修復並評估。v1 只在 root exit 開始辨識子程序；v2
提前驗證並持有 handle，但 aa03 的實際 producer 又重現 error 5／未 signaled：
PID 23960 的初次完整 image 是 `C:/Windows/System32/conhost.exe`，退出時 image
查詢先失效，handle 當下尚未退出。這是新證據，不能據此認定舊 PID 21012 的 image。

v3 保留 v2 的全部 identity、creation、membership、64 個 held identity、8 個
snapshot／checkpoint、64 KiB 每檔、stream 與原 shared-cleanup 判定。只有已完整
驗證、同 PID／creation／原 held handle 的成員，在必要 snapshot 的 image 查詢
失敗且未 signaled 時，才呼叫原 `AwaitHeld` 等待退出。每個 snapshot **共用 50 ms**，
不是每個成員另給 50 ms。執行期間的 acquisition 取樣不等待，不授予可信 snapshot。

確認 signaled 後才可用原 image；仍重查 creation 與 Job membership。逾時、遲到、
WAIT_FAILED、未知 handle、換 PID／creation、空 image、incomplete mandatory snapshot
均拒絕。不以 conhost 名稱豁免，也不跳過 cleanup／held／streams／總 deadline。
原守衛與 v2、各 campaign 的所有 STOP／raw／source 均保留；從新的 campaign 起算。

這是必要維護 shell／Win32 interop；正式遊戲及資料／成本分析 C++20 binary 不變。
new maintenance source/commit 與 CD candidate 的 source/build SHA 分開記錄。
實機 admission 仍 UNQUALIFIED；本模組不操作裝置。

`pretest.ps1` 保留原 39 個 identity/wait/shared-budget oracle，`cache-controls.ps1`
包含 9 個允許／拒絕控制；`qualify.ps1` 為四個實際 native 控制建立獨立 v3 gate。
`evaluate.ps1` 為 24 個 console exits 與 3 次原 Tap3 analyzer 的唯讀診斷建立最多
1 MiB 的彙總，不復活原 AA batch。`run-aa-complete-freeze.ps1` 複製原 campaign
全部欄位並在 launch 前核對／readback；AA02 漏 scenes 的失敗保持原樣。

Microsoft [PROCESS_INFORMATION](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-process_information)
與 [PID 重用時機](https://devblogs.microsoft.com/oldnewthing/20110107-00/?p=11803)
說明 held handle 保留 process object，直到所有 handle 關閉才可重用 PID。
這是使用原 handle 的依據；仍核 creation，沒有把 PID 名稱當身分證據。
