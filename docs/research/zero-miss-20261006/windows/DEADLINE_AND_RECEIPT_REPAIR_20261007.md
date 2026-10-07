# Windows deadline 與成本收據修補（2026-10-07）

接續使用者本輪 Windows 量測／收據驗證／emulator 驗收要求。使用者已指定 emulator，
選曲及 Play 由使用者操作。歷史整理包的「不啟動」描述保留為當時範圍；本輪仍須通過
現有成本、正式鏈、指紋及 physical gate，沒有把歷史文件當成新的使用者指令。

## 已完成的修補與量測

主修補分支 `codex/windows-readiness-20261007` 另有兩個保留 source 的候選分支：

| 候選 | source commit | 已知結果 |
|---|---|---|
| 原 Windows 候選 | `cd21144440b1d02c11eafb3f332e63d20c0079cb` | 三配置冷回歸及 3063 幀前綴通過；AA04 第一次 Tap 因 full phase lateness p99=18.832054ms 停批 |
| owner deadline wake | `d53b7fed10a8600171fbdb57adfd93efe20d97cd` | Release／Debug／ASan 每配置 247 控制通過；新 binary 全 3063 幀前綴核對通過；wakeaa01 四次 Tap 通過，第一次 Hold 發布 cadence gate 失敗停批 |
| producer deadline wait | `58b99e779720db1d1fce43d899be1883a32c1406` | 保留 owner 修補；新增高精度 QPC 絕對週期等待；三配置及新 AA 結果由下述 fresh receipts 接續 |

owner 在每次 frame wait 前先 poll scheduler；等待上限取 next due 與 10ms watchdog 的較小值。
frame timeout 回到 poll，不再把到期 Up 綁到下一張 frame。保持單一 owner、既有 gate／expiry、
未知注入不重試、已完成 Down 不復活。23 項 fake-clock／owner 回歸核對此行為。

wakeaa01 的四次 Tap full phase lateness p99/max（ms）依序為
10.352437/10.7509、9.917215/10.3795、8.086339/8.2705、10.525498/11.8211。
各筆 process／raw audit／完整及 measurement validity 都通過。
Hold1 唯一 numeric failure 是 `streamed_workload_and_cadence`：render overrun=1、
publication overrun=3；producer lateness p50/p95/p99/max=
8.64945/15.61103/16.485637/17.3954ms。其他必要 validity 通過。
這是等待抖動假說的依據，尚未將跨候選差異聲稱為隔離因果證明。

producer 修補使用 Windows high-resolution waitable timer，按 QPC 絕對 epoch 等待；
原 20ms／8ms cadence、每次 attempt 與 due 不變。100ns 轉換向上取整，提前喚醒重查 QPC，
至多四次，kernel wait 上限 250ms。timer 建立／arm／wait 失敗即失效；
沒有 fallback、跳 attempt、reset epoch、改 timer resolution／優先權或新增 thread。
11 項回歸涵蓋晚到不移動後续 deadline、提前喚醒、有界失敗及 Windows QPC 不早到。

## 唯讀成本收據 CLI

程式見 `research/windows_cost_receipt/README.md`。重用原 frozen analyzer 的
campaign／source／profile／程序身分／raw audit 與數值 predicates，不另換門檻。
外部 SHA256、完整 source commit、原 analyzer 路徑均為必要參數。

實際控制共九項：真實 AA04 NOT_READY、錯誤 SHA、錯誤 commit、重複 key、
runner 非零、identity unknown、audit metrics false、既有輸出不得覆寫，以及真實
wakeaa01 Tap1 PASS。全部沿 v3 owned Job 執行並取得 COMPLETE 收據。
exit0 只表示完整性 verified；真實 NOT_READY 保留原理由。
`offline_cost_valid=true` 仍固定 `live_ready=false`、`device_operations=0`。

## 原件及接續入口

本機根：`C:/Users/wurre/Desktop/PAS-windows-readiness-20261007`。

- `out/prelive-aa-aa04/`：原 Tap numeric STOP。
- `out/prelive-owner-wake-wake06/candidate-builds.json`：owner 三配置 build／test／donor／source canonical byte joins。
- `out/prelive-owner-wake-wake06/new-candidate-prefix-receipt.json`：新 owner binary 3063 幀證據。
- `out/prelive-aa-wakeaa01/status.json`：四 Tap PASS、Hold1 NOT_READY、剩七筆 NOT_RUN。
- `out/prelive-cost-receipt-reader02-evaluation-v3/evaluation.json`：八項收據控制。
- `out/prelive-cost-receipt-reader02-positive-wake/evaluation.json`：真實 PASS 控制。
- `out/prelive-owner-wake-prod01/candidate-builds.json`：producer 修補三配置收據，完成後才允許新 AA。
- `out/prelive-owner-wake-prod01/new-candidate-prefix-receipt.json`：producer 修補全 3063 幀核對通過。
- `out/prelive-owner-wake-prod01/run-new-aa-with-prefix.ps1`：保持原 protocol／scene／denominator／threshold 的新批次 recipe。

舊 STOP 不恢復，raw、warmup、失敗 build 與 prelaunch 嘗試均保留。
donor libraries 只繼承 canonical UTF-8 LF 相同的未變算法；新 chain 必須重建，
CRLF/LF 差異有逐檔 byte join，不以版本號代替 source／binary SHA。
每 native stage 保持 v3 Job 身分、64 held 上限、完整 streams 及 input closure 核對。

完整 AA12／noise、ABBA36、獨立 fault／stress／Debug／ASan 成本、正式
production packet／action／wake／capture／RPC、當前 emulator／app／profile／五指指紋，
以及 physical qualification／授權／解鎖驗收仍分開必需。此 endpoint-free host 的 recognition
與 owner 同 consumer，不能代替正式執行鏈或遊戲採納。

目前没有 emulator／ADB／觸控／model 操作，沒有 push 或新增長期 goal。
Chapter Legacy 分母及逐曲 IN 解鎖仍須實際核對；全曲完整 IN Miss=0 尚未完成。
