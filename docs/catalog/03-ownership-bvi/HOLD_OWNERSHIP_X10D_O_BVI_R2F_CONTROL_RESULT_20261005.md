# R2F control結果：三項控制通過，Release configure STOP

2026-10-05 Asia/Taipei。唯一 attempt `bvi-r2f-20261005-03`；同 chat 新授權與容量 carry 見新 batch 的 `scope-amendment.md`。本輪完成共享控制修復與冷預驗，三項正式控制各一次通過；第一個產品命令 configure-release 回傳 1/1/1，立即 STOP。未重試、未修改凍結來源、未續跑其餘九項命令。詳 [交接](HOLD_OWNERSHIP_X10D_O_BVI_R2F_CONTROL_HANDOFF_20261005.md)。

## 修復與預驗

- `identity.ps1` 是正式 parent/child 共用的發布與讀取 helper。只有完整 schema、明確 published/pending=false、同 attempt/PID/creation/image 與有效發布 QPC 才 ready；空字串、null、半個 JSON、pending 與缺欄位均不能升格。父程序使用實際 child Process 身分，單一 3 秒 QPC deadline，保留最多 16 個狀態轉換及各狀態次數。
- `owned.cs` 在 Job active=0 之後仍等待每個先前持有且驗證過的 handle；全部共用單一清理期限，最多 5 秒並受 stage 剩餘時間限制。WAIT_TIMEOUT、WAIT_FAILED/last-error 分開保存，等待與 streams 完成後才簽 cleanup_verified。30 秒控制的 root deadline 改為 7 秒，明列 15 秒 quiescence、5 秒 cleanup、2 秒 drain、1 秒餘額；不增加外部限制。
- 既有 helper 25/25 一次、共享交易 32/32 一次、新控制 39/39 第一輪通過，沒有第二輪或預驗修復。五項 cross-shell 拒絕檢查成立。oracle 先落盤，真檔案 IO/held writer SHA/JSON 使用正式 helper；假時鐘/假 wait/假身分的 CreateProcess=0，不能作 Windows 平台驗收。
- 凍結 23 個來源檔與 13 個命令的實際 argv；R1 `bvi.cpp/bvi.hpp` 精確 SHA 不變，successful pretest helper SHA 與 freeze 相符。原 89×4=356、22 supplemental、4 schema controls、1128 coverage rows/2392 refs、錯誤 contact adapter 與 thresholds/oracle 保留。

## 正式執行

| Stage | native / runner / verifier | 秒數 | 結果 |
|---|---|---:|---|
| natural | 0 / 0 / 0 | 1.4818045 | READY |
| nonzero | 7 / 7 / 0 | 1.1513093 | READY，預期非零 |
| owned-child | 0 / 125 / 0 | 18.1231383 | READY，預期 descendant 拒絕與清理 |
| configure-release | 1 / 1 / 1 | 0.1338654 | STOP，預期為 0/0/0 |
| 其餘九項產品命令 | 未執行 | — | STOP 後不可續跑 |

owned-child parent 25988、child 25420、conhost 7016 均有實際 PID/creation/image/Job membership。完整 15.0122752 秒 quiescence 結束時 child 與 conhost 仍在 Job；TerminateJobObject 返回 true，active_final=0、streams 完成、errors=0。三個 held handle 在同一 elapsed deadline 23.0522575 秒前 signal；child 等待約 51 ms，沒有把 active=0 當成 handle 已結束。parent 38 次 poll 實際看到 pending 36、empty 1、ready 1，成功發布 parent receipt。這證明本輪控制可處理實際空白窗口，但不反推舊 attempt02 的確切失敗原因。

configure-release 的實際 root 為 `C:\Windows\System32\cmd.exe`，stderr 41 bytes：`The syntax of the command is incorrect.`；stdout 0。root/Job/held handle/streams 自然清理通過，唯一驗收失敗為 exit-predicate。沒有新 out、CMake configure log 或候選 binary。精確出錯命令行未被 trace，不能確稱是 vcvars、路徑、quoting 或 CMake 本身；目前分類為工程命令封裝失敗，不能作候選演算法反例。來源 `.cmd`、argv、result、verification 與原始 stderr 全保留。

state STOP revision9、consumed4、known launches4、已驗收收據3。新 shell 對 configure-release 重入與 build-release 均 state-blocked。state SHA `9089d70d45978a13c83fe62d34b82fa39db08a861403aea2808160998be1f363`；contract SHA `e12a3d8710c62d6797958779c458dd8171ea372060b322c0a73dcd53d96e93d5`；freeze SHA `ed96c197db4b1e3316bbffcc6e34ffbc27a5aec3c726e7c9c5b9be197226dfa7`。

## 完整性、容量與缺口

唯一新 source `research/x10d_o_bvi_r2f_control/`、batch `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2f-control/`，out `out/x10d-o-bvi-r2f-control/` 未產生。獨立入口為 batch 的 `final-receipt.json`、分片 artifact-ledger、stage/source/input/dependency/binary manifests、freeze/contract/argv、三項預驗 oracle/results、四項正式 command/result/verification/checkpoints/logs、STOP-readback 與 protection 前後。

原 3100 個 unique protected files／3394 refs、原 569 Git paths、controller 532 fingerprints、HEAD/index/dirty 保持。old R2F、resume、R2E 等原始失敗與收據未寫；所有 frozen/helper/input/dependency 與 ledger SHA 再驗，結果見 final receipt。本輪只新增两份 CONTROL docs，未更動 src/include/root CMake/apps/tests。

容量 carry aggregate8285248004、development8335047、controller1299900、oldout374705，controller7088708 保留。原 R2F 40MiB 已用1040357，故本輪 newdev 子限40902683；newout134217728、sharedout256MiB、aggregate8GiB、free 至少5704253440。所有本輪 source/docs/batch/scratch/receipts/ledger/self bytes 計入，精確 settled bytes/SHA 以 final receipt 為準；estimate correction=0，未刪舊資料。

BVI CXX build、binary、assertions、supplemental/schema 執行均0；sizeof/metadata/probes 尚未量測。Debug/ASan 僅既有 dependency 文件存在與 SHA，未 configure/build/test。正式控制使用的 pwsh/cmd 根與子程序 image 有身分證據；完整 managed/Roslyn/system loaded DLL closure 未收集。contract 的 compiler/SDK/STL/CRT 文件 pin 是預備依賴，不冒稱實際編譯或載入 closure。

PNG0/2 保持，原 geometry review SHA `e80ab11234bd8ed8d4d64f0e6354e4dca1dec6f3e4aa0e7d7e801d53207effb0` 按 SHA 引用；未開圖、未執行 provenance-gap driver。合法 current ROI/all-lines adapter、physical owner、cost/runtime/live 與產品採用仍未通過。本包停在工程 partial：三控制 gate 本輪成立，BVI cold 尚待新授權解決 configure 封裝並另行驗證。本 chat 不續派、不另立 attempt/goal/automation/commit/push/live。Chapter Legacy IN Miss=0、C36h tint1、main50 donor/live0、suppression OFF、P excluded、X12 not-ready 均保持。
