# main 開發盤點與本階段停止 — 2026-10-08

依使用者最新指示「完成當前階段後停止，盤點開發情況」，本輪已完成目前批次的
執行、必要 gate 停止與結果封存，停止後續開發及測試。**實機仍 NOT_READY，入口
UNQUALIFIED；尚有可冷完成的修補，並未到只差啟動 emulator 的階段。**

實際編譯／測試來源是 `23a26342663b5d5b968f098620db435855a25087`，branch `main`。
本次收尾只更新盤點文件，不將較晚文件 HEAD 冒稱 binary 的編譯來源。完整索引見
[階段 JSON](PRE_EMULATOR_STAGE_20261008.json)，原 C++ verdict 與 native 收據彙整在
`out/prelive-pre-emulator-results-main14/result.json`。沒有 emulator、ADB、遊戲、真
capture/RPC／touch、模型訓練、新 goal、遠端 push 或新工作副本。

## 已完成及驗證範圍

| 項目 | 本輪實際結果 | 範圍 |
|---|---|---|
| 外部副本整合／清理 | 18 個外部 worktree 移除，47 個保存／搬移 action；63263 個來源檔案、35199550411 bytes 已核對 | main 是唯一 worktree；原始證據、Git refs／bundle 與失敗封包保留，SDK／AVD 作依賴保留 |
| 完整 production 建置 | Release／Debug／ASan 各自新編正式 core 22 TUs，共 66 次；`pas.exe` 編譯、未執行 | 全部同 source；不借 donor objects。統一 CMake 共用 fresh core，包含 production／cold tools |
| 三配置 affected 冷回歸 | 各 20/20 組，共 **60/60 PASS** | Windows MSVC ASan；不宣稱 UBSan 或第三方 DLL 全面插樁 |
| 原完整像素前綴 | **3063/3063 PASS**，獨立 raw audit 通過 | release、輸入 SHA、所有輸入處理、plan／座標 joins、釋放證據；不是遊戲採納 |
| 四項 literal production 故障控制 | **capture／writer／unknown RPC／slow，4/4 PASS** | 各 1000 planned attempts；故障情境保存完整 planned 分母及 partial／not-produced，未宣稱全數產生 |
| Production normal Tap／Hold | **2/2 raw integrity 及必要 Release 數值 gate PASS** | 原 `manual_session.cpp` translation unit、packet／action／wake／archive；capture／preflight／touch 是 endpoint-free doubles |
| 成本 analyzer／獨立 review | 每配置 **42／160 控制 PASS** | test-double fixtures；不計為本機成本量測 PASS |
| Native 收據圖 | 176 個已選 stage：175 COMPLETE、1 STOP | 唯一 STOP 是 production dense；原始 native exit 與 source closure 分開保存，不將整張圖報 PASS |

目前 main 已有 latest-frame／bounded tracking、線局部座標及關聯、owner 到期拒絕與撤銷、
旋轉 Hold 的 body 接觸／Move 與 tail 分離、未知 Down 不重試、單一 touch owner、
QPC 時點、bounded typed diagnostic writer、嚴格 gzip reader 及獨立旁路 phase 診斷。
診斷只向外輸出；普通 `current_chain` 不啟用 phase profiling。這些程式及 cold 契約
已存在，仍需成本、physical／裝置及遊戲結果驗證。

20 組回歸涵蓋 writer／checkpoint／expiry／producer timer／Tap reset／owner wait、
current observer-owner、活動 body／tail、disabled live seam／packet／runtime／finish、
成本比較、production auditor／入口拒絕，以及 analyzer unit／獨立拒絕 review。
Producer timer 的 11 個功能控制含 FakeClock、絕對 epoch／cadence、late attempt 保留、
early wake 有界重查及 Windows 實際 timer；功能通過不代表 OS 每次準時醒來。

## 必要門檻與實際負結果

**R2 AA main14：Tap A1／A2 PASS，A3 NOT_READY，後九筆 NOT_RUN；noise 未凍結、
ABBA36 未啟動。** A3 完整 planned／produced 2560，owner 2559、measurement 2303/2304。
唯一 analyzer failure 是 `tap_streamed_workload_and_cadence`：producer／publication
deadline overrun 各 1，原 20 ms deadline 及其他門檻未放寬。

原 `main14-tap-closed-reset-v1-3.json.attempts.json` 的 attempt 1928：
`producer_due_ns=29373925167700` 至 `render_start_ns` 的實際差為 24.6327 ms，
render 0.6078 ms、publication 2.2439 ms，due 到 publication return 合計 27.4844 ms。
以上是既有 raw 時點的定位；尚未取得能區分 OS timer、執行緒搶占等因素的追蹤。
沒有修改 thread priority、MMCSS、cadence、time origin 或跳過 attempt，也未重跑同源
成本挑選成功結果。此項是待修補，不能寫成已修好。

**Production main14：normal Tap／Hold PASS；normal dense 已執行失敗；後六項 NOT_RUN。**
Tap measurement owner 2304/2304、capture→owner return p99=14.449565 ms；
Hold measurement owner 2283/2304、同量 p99=50.377352 ms。
兩者 native producer／auditor 都 COMPLETE，各自 raw／數值判定通過；整個 production
批次尚未通過。

Dense native 約 1.5102 秒後 exit=2、runner=2，stderr 是
`session archive mailbox overrun`，wrapper state=STOP／verification=1。
held identities、streams、active_final=0 與 natural quiescence 均成立，但期望 exit=0
未成立，該 stage 沒有成功的完整 before／after closure qualification。
session summary=`FAULT`、published=41；round=`aborted`／`partial=true`，保留 gzip
11461381 decoded bytes／402377 physical bytes。沒有最終 `report.json` 或獨立全量 audit，
不宣稱完成 2560 attempts、數值成本通過或完整 raw denominator。

原 production `status.json` 的 `remaining` 包含 normal-dense 並標 NOT_RUN，是以
「尚未加入成功 audit rows」產生的列表。**實際 native 收據證明 normal-dense 已啟動並失敗**；
本盤點另列 `FAILED_EXECUTION`，保留原收據原樣，不覆寫成未執行。
真正未啟動的是 stress Tap／dense、Debug Tap／Hold、ASan Tap／Hold 共六項。

## 建置與證據沿革

| 配置 | 實際 compiler root | 現行 20 組評估入口 |
|---|---|---|
| Release | `out/prelive-pre-emulator-release-main12/` | `out/prelive-pre-emulator-evaluation-main14-release/evaluation.json` |
| Debug | `out/prelive-pre-emulator-debug-main13/` | `out/prelive-pre-emulator-evaluation-main14-debug/evaluation.json` |
| ASan | `out/prelive-pre-emulator-asan-main12/` | `out/prelive-pre-emulator-evaluation-main14-asan/evaluation.json` |

main14 是評估彙整 namespace，不是三份重新編譯的 compiler roots。Release／ASan main12
和 Debug main13 都來自 `23a2634`。依賴在 configure 前已封存完整 installed triplet：
2154 files／11383669733 bytes，包含 Release／Debug libs、runtime、package metadata、
protoc／plugin；每個 native stage 沿用原固定 64 held identities／8 snapshots gate。

早期 main08 configure scope、main09 include、main10 auditor 型別編譯失敗、main11
dependency closure 撤回、main12 Debug identity-cap STOP，以及 Release child identity
負例都保留，未充當現行資格。Debug main13 最終 executable PDB 的 archive／seal race
也保留 negative；所有 compiler 已退出後再次核對 ZIP entries／原檔 SHA，再串行清理、
重封有效 build binding。34 PDB／1618362368 decoded bytes 保存於
`out/retained/consolidation-20261008/main13-debug-symbol-archive/symbols.zip`，ZIP 468585545 bytes。
舊競態 build binding 無效；現行 Debug 使用 `build-binding-resealed.json`。

舊 analyzer fixture 所預聲明的 protocol SHA 為 `58d6136f…`，當前歷史入口文件已變成
`0dcaa25b…`，初輪 fixture 因此失敗。從 Git commit `7ac2a31a…` 取回逐 byte 原 blob，
另建隔離 test-double repository，所有當前 source copy／原 protocol SHA 再核對，三配置
42＋160 控制才通過。正式文件／R2 protocol 沒有被改成舊 blob，R2 SHA 保持
`0000ad66d05649ebc4f60570b58e9a9a4cb58f7e6629279e4d38a945c8d4c060`。

成功接續使用的維護 recipe、原失敗 recipe／receipt、符號與失敗 compiler ZIP 都保存在
OUT，逐檔 binding 見階段 JSON。尚未把大型 production target 隔離、legacy fixture snapshot、
串行 PDB 清理與 reseal 的接續流程完整回寫成新的統一 `build.ps1`；不能直接把舊預設
main12 recipe 宣稱為下一來源可一次重現的 pipeline。後續改 recipe 仍須新 source 身分與驗證。

## 容量及後續缺口

收尾 retained 帳本 **48227847084 bytes（44.916/48 GiB）**，C 槽 free
207322050560 bytes（193.084 GiB），minimum free 20 GiB 通過。
compiler／evaluation／fixture／planning 合計 5722844756 bytes（5.330/6 GiB）；
prefix／fault／AA／production raw 另列於全域帳本，不混入 compiler 預算。
SDK／AVD／vcpkg 等依賴按既有容量契約另列，不能把 retained 44.916 GiB 當作整個專案
或整個 C 槽的總佔用。沒有新容量授權或門檻增加。

後續尚需：修查 production dense 的 archive admission／writer／action wake 負載、
AA producer 晚醒，保存全分母與失敗；將已驗證的維護接續流程固定化。
具體修改後才能新建 campaign，原 AA/noise/ABBA、coverage、RSS、deadline／capacity
門檻與失敗停止規則保持。剩餘 production stress／Debug／ASan 六項，以及 AA 後九項、
noise／ABBA36 都是待辦，不能用此前各 PASS 補成已跑。

完整 live reader／qualified token 尚未實作；public admission 仍拒絕。當前 device／app／
profile／五指 mapping、physical qualification、真 capture/RPC 及有限 manual-session
都未取得本候選的資格。本輪不啟動 emulator，也沒有新實機結果。Chapter Legacy
章節分母、目前逐曲 IN 解鎖 unknown；本候選完整 IN 結算 Miss=0 證據 **0**。
HD 只用於解鎖／回歸，P/G/B 照報，不新增 AP 前置。

本階段結果封存與盤點完成；其餘工作按使用者要求停止，未建立自動續跑或新長期 goal。
