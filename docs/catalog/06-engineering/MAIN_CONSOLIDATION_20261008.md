# Main 整合、容量與外部副本清算 — 2026-10-08

後續 main emulator 前工作包已依使用者「完成當前階段後停止，盤點開發情況」收尾，
最新入口為[開發盤點](../../research/live-readiness-20261007/PRE_EMULATOR_STAGE_20261008.md)。
source `23a2634` 三配置 60/60 組、3063 prefix、四項故障控制通過；AA Tap3 deadline
overrun、production dense archive mailbox overrun，仍 NOT_READY／UNQUALIFIED。
本頁以下 main07 的 45/45 與其容量是整合階段快照，保留原 source／receipt；
不是後續 main14 的結果或當前容量。後續收尾 retained 帳本為 44.916/48 GiB。

使用者授權先將 Desktop、Downloads 與可讀 C 槽中的專案工作副本、原始證據、
建置輸出及封包集中至 main，保存驗證後刪除外部副本，並接受 retained prelive
上限由 32 GiB 提高至 48 GiB。後續直接在 main 開發，不再新建 worktree。
容量數字及其餘既有界限見
[授權 JSON](../../research/live-readiness-20261007/CAPACITY_APPROVAL_20261008.json)。

## 原始碼整合

| 來源 | 處置 |
|---|---|
| `a2ea47b245ece8630054aa7661f80b806daee694` | fast-forward：到期拒絕、typed writer、gzip archive／嚴格 reader 及其回歸 |
| `d45046cc57fbf69ccf769aafca9a66a92fa785bb` | `fc5640a`：Windows 維護、取消 Up join、成本與 production 工具、歷史證據入口 |
| `59a0b67c8933038da5cd476a4732307080a72eb9` | `0da7b71`：修正後的離線 dense noise diagnosis |
| `34eddc176c5234e5e4d0d779ad64299a34e8685c` | `4c0f6b0`：固定九個 scalar phase；衝突保留最新 typed writer、gzip 與 reader |
| `fd9c6a24088122af3c9ab0f34f0125ad50e75b29` | 失敗 checkout 的刪除索引只保留 Git 歷史，沒有合入 main |
| 其他較早分支／donor | 已被後續來源包含，或保留在 Git refs／完整 bundle；不重新啟用歷史行為 |

一般 `current_chain` 不啟用額外 phase profiling；獨立
`current_chain_phase_profile` 只接受 B／dense／1000 的有限診斷，不能授予成本資格。
診斷支持 typed snapshot 的三個建構階段及原 JSON 三個建構階段，但每個 frame
只接受完整一致的九階段集合。診斷不能回饋動作策略。

## 保存與刪除契約

清算已完成：18 個外部 worktree 刪除，47 個搬移 action 全部核對；原位置共 63263
個檔案、35199550411 bytes，包含一份驗 SHA 後移除的相同來源封包。main 是唯一
有效 worktree，已盤點的外部 artifact 原位置剩餘 0。逐項 SHA 封存入口為
[完成索引](../../research/live-readiness-20261007/MAIN_CONSOLIDATION_INDEX_20261008.json)。

main 的 `out/consolidation-20261008-01/` 保存完整 Git bundle、C 槽盤點、18 份
外部 worktree 實際原始碼 ZIP 與逐檔 SHA、搬移前後 SHA receipt、刪除 receipt。
原始碼 ZIP 共 15697 個檔案、decoded 130795601 bytes、ZIP 45026155 bytes，
每個 ZIP entry 已獨立讀回核對。完整 Git bundle 已用 `git bundle verify` 驗證。
所有 branch refs 保留；失敗與未採用候選仍可按 SHA 查閱。

唯一大型外部證據庫及其餘 ignored／untracked artifacts 搬至
`out/retained/consolidation-20261008/`。每個 action 先凍結原位置、relative path、bytes
與 SHA-256，再以 native PowerShell 在已核對的絕對範圍內搬移，最後核對目的地的
完整集合、bytes 與 SHA。只在兩邊 bytes／SHA 完全相同時移除同名重複封包。
raw journal、負結果、STOP、舊已執行 recipe 與 receipt 內容不改寫；舊絕對路徑由
`relocation-index.json` 對應到新位置，不能將旧 receipt 當成新位置或新 main 的資格。

外部 worktree 必須 HEAD 未變、tracked／untracked clean、ignored 內容已全部搬走、
實際原始碼 ZIP 再次驗 SHA，才可 `git worktree remove`。main checkout 與 Git refs
保留。孤立遺留樹整體搬回；過期 Python editable 登錄、IDE 專案快取、測試暫存、
crash dump、專案 Recent shortcut 與 Documents 證據也按相同保存契約處理。

IRIS-X／ARC 及其他不相關檔案不屬此次範圍。模擬器、Android SDK、AVD 與既有遊戲
資料保留作後續開發依賴，不作證據副本刪除。主用 phigros AVD 約 24.2 GiB，
較早 4K AVD 約 5.0 GiB；它們不在 retained prelive 48 GiB ledger 中。沒有啟動
emulator、ADB、遊戲、觸控或模型，也沒有重新安裝或更改 profile。

C 槽名稱／目錄 metadata 盤點遍歷 296823 directories、1961888 entries，找到 58 個
匹配位置；590 個 ACL／IO 不可讀位置記錄在 `c-volume-survey.json`，140 個 reparse
points 未跟隨。匹配專案根目錄另作內容盤點。此盤點不能宣稱已檢視不可讀區域或
任意無專案識別名稱的檔案；不以重複掃描／權限提升補成假完整。

## 在 main 繼續驗證

48 GiB ledger 包含搬回的歷史證據與清算 snapshots、fresh prelive builds、guard 與
tool snapshots，這些 roots 不重疊；仍保留至少 20 GiB free、1.5 GiB build reserve、
1 GiB raw reserve、6 GiB candidate build、12 GiB cost raw、64 MiB stage metadata、
256 MiB new metadata total 等原界限。歷史 32 GiB STOP 不改成 PASS。

新 main 程序守衛已重新通過 20 diagnostic、39 identity/wait、9 cache 的 file-only
控制與 4 次 native qualification。每次 native control 都核對 verification exit=0、
held all signaled、streams complete、active_final=0；預期 nonzero 與 owned-child cleanup
仍按原契約照報。沒有用舊路徑的 guard receipt 升格新 main。

`research/main_consolidation/build.ps1` 要求 clean main、48 GiB 授權、新 main guard
與獨立重核的 MSVC tool snapshot。三配置各自重新編譯正式 22 TUs，完整 input closure
及 before／after SHA、自然退出與 bounded native Job 驗證保持原樣，不借 donor objects。
原 writer 的 15 cases 與其餘 14 組 affected 冷回歸照跑；Debug／ASan 原容量 STOP
不能用 Release PASS 補成已跑。各配置 fresh build 輸出分開封存。

第一個新 main Release 嘗試 main05（source `0a322fc`）的 configure 及 22 個正式
TUs 分五組通過 native 驗證；合併的 libraries stage 在 20.225 秒觸及固定
`held-identity-cap64`，runner exit=125、verification exit=1，state=STOP。
CMake log 的 `Build END exit=0` 不能代替 wrapper 自然退出／完整身分驗證；沒有
冷回歸 PASS 或成本資格。完整 result、verification、checkpoint 及 logs 保留於
`out/windows-handoff/build-prelive-main-integration-release-main05-libraries/`，
原 recipe 另存於清算 audit 的 `build-main05-frozen.ps1`。

main06 recipe 將 formal_core、exact_bvi_v3、current_front、own_bridge、current_rails
依依賴順序拆成五個小 stage，並重新 configure、編譯全部 22 個正式 TUs；不使用
中止輸出作 donor，不提高 64 held identities、8 snapshots 或其他守衛界限。
新 attempt namespace 必須全新；不能覆寫 main05 或原地重跑原 combined stage。

main06（source `90a6747`）Configure 也因 vcvars 初始化加 CMake probe 累計身分
觸及相同上限而 STOP；未開始正式編譯。main07 將 vcvars 初始化獨立為一次 bounded
native stage，只封存 14 個具名編譯環境變數，單值 16 KiB／整體 128 KiB，拒絕
quote、percent、換行等 cmd 展開字元。後續 CMake stage 在 COMPLETE capture receipt
及 before／after SHA 的契約內套用同樣值，不重複初始化 vcvars；守衛原封不動。
main06 完整負結果與 frozen recipe 保留，兩次 STOP 都不算冷回歸 PASS。

main07 實際封存來源為 `5513458d7172351262f972cf2426947bfda7015d`。
Release／Debug／ASan 各自 freshly 編譯正式 22 TUs，全部 30 個 build stage、環境封存、
configure 及 15 組 affected 冷回歸都通過。總計 **45／45 組、writer 45 次案例執行、
66 次正式 TU 編譯、141 個 native stage**；每個 receipt 重新核對自然退出、身分、
held all signaled、streams complete、active_final=0、before／after 封存與實際 exit。
三配置並行僅用於 cold regression，不用這次 elapsed 取得成本資格。

最終 retained 41592301789 bytes（38.74／48 GiB）、candidate 5176971633 bytes
（4.82／6 GiB）、C 槽 free 239420968960 bytes，minimum free 20 GiB 通過。
外部 action 原位置仍剩餘 0、main 是唯一 worktree。封存入口為
[冷驗證 JSON](../../research/live-readiness-20261007/MAIN_COLD_RESULTS_20261008.json)，
完整逐 receipt 核對為 `out/consolidation-20261008-01/main07-cold-results.json`。
後續本頁狀態提交只更新文件；已執行的 build／test source SHA 保持 `5513458`，
不能將新文件 HEAD 或搬移前的 receipt 當成新 binary 資格。

一般鏈、phase 診斷版、bounded phase summary 與 auditor 已編譯；新 phase 量測、
prefix、R2 cost 及 literal production 仍為 NOT_RUN。冷回歸通過不授予實機 admission。

整合前 a2 的 33／45 affected groups PASS 是歷史結果；其餘 12 groups 因容量未建置。
新 main 的 affected cold、3063 prefix、R2 AA／noise／ABBA、literal production fault／
stress／slow-writer／RPC、admission／profile／physical 與完整 IN Miss=0 均分開記錄。
實機入口仍 **UNQUALIFIED**，完整 IN 結算 Miss=0 證據數為 0，章節分母與逐曲解鎖 unknown。
產品目標、P/G/B 報告與 HD 解鎖／回歸用途維持原契約；不新增 AP 前置。
