# Job 身分修復已驗證，成本仍 NOT_READY

2026-10-07。使用者要求「執行修復並評估」，已完成 Job 成員 image identity 的 v3
修補與實際評估。主 checkout 保持 e9e305e／tracked clean，本分支 local commits 未 push。
正式遊戲與成本 C++20 candidate 仍為 CD frozen source/build；新維護守衛另外列 source SHA。
機讀路徑、SHA、source 與原 STOP 見 [process-guard-repair-results.json](process-guard-repair-results.json)。

原 v1 在 root exit 才開始辨識子成員。v2 提前驗證並持有 handle，但新 AA03 又觀測
到 conhost 的 image query error 5 先於 handle signaled。此新 PID 已有完整初次 image，
但不能據此指認 AA01 的未知 PID。v3 對同 PID／creation／原 held handle 的已驗身分，
在必要 snapshot 內以原 `AwaitHeld` 等待退出；snapshot 共享50ms，不按成員重置。
signaled 後才可採用舊 image，仍重核 creation／membership；未知、逾時、遲到、換
handle／creation、WAIT_FAILED 均拒絕。每檔64KiB、8個checkpoint、64個held identity、
streams、總期限及 Job-only cleanup 維持硬限；沒有 emulator／ADB／触控／live toggle。

v3 的原39個 identity／wait／shared-budget 控制、9個快取控制均通過。四個實際
native 資格控制通過，包括 exit7及未結束子程序的預期 runner125／owned cleanup。
另24次 console exits＋3次原 Tap3 analyzer 唯讀診斷，共27個 COMPLETE。
其中一次實際 root-exit 的 conhost image query error5，原handle等待約0.0717ms確認
signaled；同creation、before/after membership均核對，identity trusted／native0／
runner0／held signaled／streams completed／active_final0。這驗證了真實故障分支。

新 AA04 完整複製原 campaign 契約，候選binary、corpus、profile、20ms cadence、
256warmup＋2304measurement、順序及門檻未改。首筆 Tap 2560 attempts 全產生，
producer、auditor、analyzer 三個直接root的程序驗證全過；auditor raw joins完整。
失敗為原必要 full phase lateness p99：18.832ms > 15ms。批次已停止，後11筆、
noise、ABBA全NOT_RUN。不能由measurement p99=12.346ms改判full gate。

|AA04 Tap1 實量|n|p50 ms|p95 ms|p99 ms|max ms|jitter p95−p5 ms|
|---|---:|---:|---:|---:|---:|---:|
|full capture→owner|2556|10.181|16.813|24.020|46.946|9.601|
|measurement capture→owner|2301|10.210|16.927|23.971|46.946|9.651|
|full all-phase lateness|74|0.037|6.734|18.832|21.066|6.730|
|measurement all-phase lateness|60|0.037|5.927|12.346|18.006|5.923|

Full own D/M/U=37/0/37，measurement=30/0/30。Coverage full2556/2560、measurement
2301/2304；peak RSS=57,565,184bytes（54.898MiB）。pool/writer drops、writer faults、
injection failures皆0，release verified。來源 age unknown；不是 capture/RPC 或遊戲採納。
環境為Windows NT10.0.26200、Core Ultra5 125H、14cores/18logical、約34GB RAM；
背景摘要／source／build／profile保存在campaign binding。

原AA01及新負結果永久保留：v2 native-01的checkpoint超過舊16KiB、evaluation-01
彙總容量失敗的空檔與舊writer、AA02缺scenes的analyzer exit2、AA03的error5／未signaled、
AA04的numeric NOT_READY。AA02是新增recipe遺漏；後續建立器改為複製全部原欄位，
並在launch前readback核對。沒有覆寫、quiet rerun或把後續結果補入STOP batch。

目前OUT約6.93GB，低於12GiB；磁碟free約187GB，高於20GiB。容量snapshot另入機讀檔。
冷收據CLI／83個cold stage仍為原獨立結果。完整production packet/action/wake/capture/RPC
成本、live validator、physical、當前emulator／五指mapping資格與全曲IN Miss=0仍未完成。
manual-session仍由使用者選曲及按Play；入口保持UNQUALIFIED。

Microsoft [PROCESS_INFORMATION](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-process_information)
及 [PID重用時機](https://devblogs.microsoft.com/oldnewthing/20110107-00/?p=11803)
說明held handle保留process object／PID，直到所有handle關閉。此依據配合實際creation／
membership複核，不拿PID或conhost名称单独作信任。
