# Phigros Auto-play System

2026-10-10 冷驗證 main30／`f6548e5`：三配置75組／3063 prefix／fault4及production normal
三項、stress兩項與Debug Tap通過。Stress Dense完整archive正常完成；Debug Hold auditor
錯把中途已release後的contact重用當搶占。修補按原journal／QPC核對verified backend release，
93 controls及七份原full raw組件通過；原STOP保留，ASan後兩項與AA/noise/ABBA未跑。
新整合完整驗證尚待完成，cost NOT_READY／live UNQUALIFIED，未啟動emulator。

2026-10-09 後續測試範圍已限定：[Chapter Legacy IN 一輪、只記分數、完成後停止](docs/research/live-readiness-20261007/SINGLE_PASS_SCOPE_20261009.md)。
冷修補整合直接建置三配置的新正式來源；舊 `382d845` stress dense mailbox overrun
與未執行的 AA／ABBA 保留。新的分數模式停用遊玩持久化，仍須通過當前資格，尚未啟動 emulator。

2026-10-08 使用者已續派：先完成 AA producer／production dense 冷修補與原門檻驗證，
實機測試準備完成後交接新的 GPT-6.1 Sol／xhigh local main task，授權其 emulator／
Computer Use 有限測試。最新 recipe 見[啟動前工作包](research/pre_emulator/PLAN.md)。
修補正在驗證，尚未取得新成本／live 資格；下列停止盤點為前階段快照。

2026-10-08 最新入口為[開發盤點與本階段停止](docs/research/live-readiness-20261007/PRE_EMULATOR_STAGE_20261008.md)
及[逐項收據索引](docs/research/live-readiness-20261007/PRE_EMULATOR_STAGE_20261008.json)。
依使用者「完成當前階段後停止，盤點開發情況」，本輪已封存並停止後續開發／測試。
實際 source `23a2634` 完成 Release／Debug／ASan 各自 22 正式 TUs 新建置、**60/60 組**
affected 冷回歸、3063/3063 像素前綴與四項 literal production 故障控制。
Production normal Tap／Hold 的 raw integrity／數值 gate 通過；normal dense 因 archive
mailbox overrun 執行失敗，後六項未跑。R2 AA Tap1/2 PASS、Tap3 producer／publication
各一次 deadline overrun，後九項／noise／ABBA 未跑。**實機 NOT_READY，入口 UNQUALIFIED**；
仍有可冷修補項，完整 live validator、當前裝置／physical 與完整 IN Miss=0 未完成。

外部專案副本、證據與封包清算見[main 整合與清理帳本](docs/catalog/06-engineering/MAIN_CONSOLIDATION_20261008.md)；
main 是唯一 worktree，retained prelive 上限依[容量授權](docs/research/live-readiness-20261007/CAPACITY_APPROVAL_20261008.json)
為 **48 GiB**，收尾帳本 44.916 GiB。未啟動 emulator／ADB／真觸控，沒有自動續跑。
下列較早來源與「最新」入口保留為歷史快照；後續文件 HEAD 不代替已編譯 source SHA。

2026-10-07 最新續作見 [owner cooperative 階段交接](docs/research/live-readiness-20261007/OWNER_COOPERATIVE_STAGE_HANDOFF_20261007.md)。
候選 `45f8a5f` 三配置39組 affected 冷回歸、3063 prefix、成本工具42＋22控制通過。
新 Windows AA 前兩筆 Tap PASS；第三筆安全停止，owner seen 2181/2560、measurement 1925/2304，
producer/publication overruns 12/13，必要 gate NOT_READY；後九筆／noise／ABBA未跑。
實機入口仍 UNQUALIFIED，全曲完整 IN Miss=0 未完成。使用者最新授權下一個
GPT-6.1 Sol／xhigh task 以 Computer Use 與 emulator 自行選曲／Play／驗收，仍須通過既有 gate。
下列先前階段結果保留為歷史，不代表此次候選。

2026-10-07 Windows 實際結果見 [Windows readiness：NOT_READY](docs/research/live-readiness-20261007/README.md)。
三配置與 83 個冷 stage、3063 WIC prefix、cold CLI 15 控制通過。
[Job 身分修補 v3](docs/research/live-readiness-20261007/JOB_IDENTITY_REPAIR_20261007.md) 已通過
39＋9 個 file-only 控制、4 個 native 資格控制與27次實際追加控制，包含 error 5 的真實分支。
新 AA04 的直接 producer／auditor／analyzer 全部通過程序驗證，但首筆 Tap full phase
lateness p99=18.832ms 超過原15ms門檻，批次停止；後11筆／noise／ABBA未跑。
入口保持 UNQUALIFIED，零 emulator／觸控；完整 live validator、physical 與全曲 IN Miss=0 尚未完成。

最新雲端 readiness 候選由 [10/6 續作入口](docs/research/live-readiness-20261006/README.md) 進入。
起點為 main `e9e305e3aba13f05a786c74540419428610b2f76`，本分支只有 local commits，未 push。
本輪補強成本原始證據／campaign 身分、活動 Hold 的當前 body 與不確定釋放證據；
共享 renderer 與 tail 缺口分層揭露。軟體冷驗證不能授予 Windows 成本或實機資格，
目前仍 **NOT_READY**；產品 Chapter Legacy 全曲完整 IN Miss=0 未完成。

Windows／C++20 的即時 pixels-to-touch 研究專案。正式程式以單程序多執行緒處理最新畫面、音符／判定線追蹤、撞線預測與可撤銷多指觸控；不讀譜或遊戲內部狀態。

2026-10-06 current-rails-v1 已整合至 main 的正式離線 observer／owner／scheduler；
三配置回歸及3063幀自身接觸前綴通過。成本／physical gate仍NOT_READY，未接live CLI，
Chapter Legacy完整IN Miss=0目標未完成。最新結果由
[Windows冷準備入口](docs/research/zero-miss-20261006/windows/README.md)進入，
近期 ignored 資料與精簡 ZIP 見 [封包說明](docs/research/zero-miss-20261006/windows/PRELIVE_IGNORED_PACKAGE.md)。
分類文件見[文件總目錄](docs/README.md)。下列較早驗收及2026-10-04狀態保留為歷史快照。

前一包派送撤回後的[檔案驗收與診斷修補](docs/research/zero-miss-20261006/windows/WITHDRAWAL_ACCEPTANCE.md)保留原件與 STOP。使用者已確認一筆 Hold head前緣語義，當時三配置獨立核對256幀決策及原圖取樣；該標註不等於合法觸點或遊戲採納。

**最新 [10/6 冷準備結果](docs/research/zero-miss-20261006/windows/PRELIVE_RESULTS.md)：三配置113／56／113／68／186項回歸通過，完整3063幀自身receipt有效；原v3的36fail原件保留，本包未重跑／改原oracle。A/A在第5筆Hold無動作時停止，ABBA未跑；有限負載／fault／10000次stress有完整負結果，成本未取得資格。沒有裝置操作或實機採納，產品Goal未完成。**

2026-10-05 [首輪雲端多分支 zero-miss 研究](docs/research/zero-miss-20261005/README.md)完成程式、逐曲總帳與建置盤點，以及隔離 C++ 冷實驗；當時原 frozen suite 尚缺輸入。

[補件後第二輪](docs/research/zero-miss-20261005/round2/README.md)首次完整執行356 layer-cases／3938 assertions，發現54fail並分類；C36h M77圖及Windows失敗收據已覆核。

**10/5 [第四階段冷候選v3與交接](docs/research/zero-miss-20261005/round4/README.md)：原3938斷言43→36fail、無新增失敗；stationary／exact alias／斜交contact及正反例完成三配置。當時雲端委託停在真圖／Windows必要門檻；本機接手另見10/6入口，產品Goal未完成。**

**[第三階段冷測修補](docs/research/zero-miss-20261005/round3/README.md)：新隔離候選修正rails／端部與merge資格，原同oracle54→43fail、無新增原suite失敗；新48-case契約339斷言全過，獨立35-case仍保留duplicate ROI既存2fail。** 程式在[research/bvi_cold_v2](research/bvi_cold_v2/README.md)，尚未整合正式鏈，未進live。Windows、成本、真圖及全曲分母／解鎖仍有門檻。

## 歷史狀態（2026-10-04）

產品目標為 **Chapter Legacy 所有曲目解鎖 IN，完整 IN 結算 Miss=0**；P/G/B與分數照樣報告，HD為解鎖及回歸階段，沒有AP前置。最新逐曲證據、研究狀態與順序工作包見[10/4基準總帳](docs/catalog/01-project/LEGACY_IN_ZERO_MISS_BASELINE_20261004.md)及[配套JSON](docs/catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json)。現行章節分母未核，`chapter_listing_verified=false`；目前各曲解鎖狀態unknown，不能由HD分數代推。

main HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`；整合沿革見[整合交接](docs/catalog/06-engineering/MAIN_INTEGRATION_HANDOFF_20261001.md)。**C36h tint1為behavioural／experimental baseline；main50 observer50／planner27／diagnostics11是comparison／donor、live0**。同號歷史版本依binary/source SHA分讀。[10/1狀態](docs/catalog/01-project/PROJECT_STATUS_NEXT_STEPS_20261001.md) A–I及早期「最新」是歷史，J5–J32銜接後續。

- 整合 Release 回歸 349 項：348 通過、1 opt-in 跳過、0 失敗；離線 CPU 工具 8/8。這是軟體驗證，不是遊戲效果驗收。
- 恢復分支 C36h tint1 的 Dlyrotz IN13 為 795950／P496-G11-B0-M77，前次全錄亦 M77；使用者回報主要問題未改善。C36h 同號 37/19 與歷史 main 37/19 不是同一 binary。
- 保存全錄7722張PNG、12段選取與理由；pixel human gold=0。X1完整fixed-pixels owner／scheduler／FakeTouch接觸重播及R1/R2於10/2已獨立驗收；固定pixels不包含新觸控的遊戲feedback。
- 模型僅供離線輔助標記及主程式優化。C++ LibTorch CPU 小試已有權重與 18 張原生 ROI proposal；不進即時迴圈，不以合成 IoU 宣稱真實準確率。
- X10b standalone suppression已否決，suppression OFF；X10d-P單一pending-missing hook冷契約已獨立通過，14組機會損失利弊unknown。X11-P/R1/R2/R3工程與負結果已簽收、成本仍not-ready，X12仍conditional；X10d-O尚未開始。
- 本包只整理證據與入口；接續順序為A獨立驗收收尾→B量測契約審查→X10d-O→conditional候選live／全曲IN。未啟動emulator／觸控／模型／goal，未追加runtime、cost、stress或full replay。

歷史跨曲兩版各 19 首 HD 的 Miss 為 815→818，非 Perfect 957→860；沒有 AP 或未知曲泛化驗收。9/29 的 D1–D4／C0–C6 是已保存的冷工程成果，其後實戰與整合結果依日期分開閱讀。

[10/1整理帳本](docs/catalog/06-engineering/CLEANUP_AUDIT_20261001.md)保留當時範圍；[9/28清理紀錄](docs/catalog/06-engineering/CLEANUP_AUDIT_20260928.md)列不可重算資料。10/4現查：舊recovery目錄未registered，其LibTorch及CPU runtime路徑不存在，main CPU cache仍指舊路徑；不宣稱當前CPU可build，也未重裝。原frozen引用與raw不回寫。

## 建置與回歸

需要 Windows x64、VS2026 MSVC v145、Windows SDK、CMake 3.28+ 及 vcpkg。依賴由 vcpkg.json／vcpkg-configuration.json 與 third_party overlay 固定；不要替換已修補的 gRPC 1.81.1。

```powershell
$env:VCPKG_ROOT='C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg'
$env:VCPKG_MAX_CONCURRENCY='4'
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

Debug／ASan 對應 windows-debug／windows-asan。presets 含本機 VS instance 與 sanitizer 路徑，換機須核對。第三方 DLL 未全面 ASan 插樁；效能只用 Release。

初次安裝的 gRPC 深路徑可能超過 Windows 限制；本機 C:/pas-bld-9408 與 C:/pas-pkg-9408 為 out/vcpkg_installed/vcpkg 下 blds／pkgs 的短路徑 junction。清理後可重建空 staging 目錄；換機須設定自己的短路徑。已安裝 x64-windows 依賴保留。

## 手動實戰操作參考（本輪不啟動）

以下保留main50歷史操作參考，它目前是comparison／donor、沒有live-ready資格。preset建置輸出至`out/release-v145`，不能把殘留binary或此命令當作候選升格／本輪啟動授權。

```powershell
out/main-integration-v145/Release/pas.exe manual-session `
  --config configs/phigros-hd-assist-five-lead35.json `
  --capability measurements/game-semantics-20260927/touch-five/summary.json --no-preview
```

啟動後由使用者選曲並按 Play；程式僅由即時 HUD 接手。結算確認後釋放觸控、保存同源結算圖並回待命，不自動選曲／重試。Escape 或 Ctrl+C 停止。未知畫面不取得觸控資格；結算辨識限已核對的 1280×720 英文布局。

--pixel-clips 需使用從當前 source 重建的執行檔，或保留的第二輪 main-legacy-v145 binary；它啟用既有有限三幀診斷：最多 20 輪、每輪 30 張，writer mailbox 4 張；不是新研究的兩秒採樣器。--round-watchdog-s 預設 0，超時記 FAULT／aborted，不當成曲尾。這些命令是操作說明，不是自動啟動遊戲的授權。

熱調參分支另用 `configs/phigros-hd-assist-five-lead40.json`：manual-session 接受 30–45 ms lead，owner 依 profile 設定並在 manifest 記有效值。40 ms 四首已重測，分數皆小幅上升但 Miss 仍多。observer48／planner25 的後續手動試驗由使用者停止、無結算，不作效果證據。observer49／planner26 針對原線短暫消失補有界線位移投影；2026-09-30 的手動實測完成 Glaciaxion HD6、Dlyrotz HD9 兩首後依使用者要求停止，結果見[熱調試紀錄](docs/catalog/02-game-results/FOUR_SONG_HOT_TUNING_20260929.md)。

歷史比較版本請保留原 binary，不在當前 checkout 重建覆寫：
- 舊版：out/hd9-session-release/Release/pas.exe。
- 第二輪：out/main-legacy-v145/Release/pas.exe。
- 目前 out/release-v145 與第二輪 binary hash 不同，不能互當完全相同版本。

## 文件入口

| 文件 | 用途 |
|---|---|
| [10/4 IN zero miss總帳](docs/catalog/01-project/LEGACY_IN_ZERO_MISS_BASELINE_20261004.md)／[JSON](docs/catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json) | 最新入口、逐曲證據、基準角色及順序工作包 |
| [10/1狀態](docs/catalog/01-project/PROJECT_STATUS_NEXT_STEPS_20261001.md) | 歷史因果鏈；J5–J32接續最新狀態 |
| [整合交接](docs/catalog/06-engineering/MAIN_INTEGRATION_HANDOFF_20261001.md) | 合併來源、frozen 配套、依賴與整合測試 |
| [架構](docs/catalog/01-project/ARCHITECTURE.md)／[路線圖](docs/catalog/01-project/ROADMAP.md) | 現行契約與後续優先順序 |
| [逐幀與 C36h 結果](docs/catalog/02-game-results/DLYROTZ_FRAME_ANALYSIS_C36H_20261001.md) | 使用者 12 段理由、原圖／事件及未改善結果 |
| [全錄契約](docs/catalog/06-engineering/FULL_ROUND_RECORDING_20261001.md)／[CPU 小試](docs/catalog/07-data-learning/OFFLINE_CPU_VISION_PILOT_20261001.md) | 已有離線工具與限制 |
| [跨曲研究](docs/catalog/01-project/MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md) | 原始 M0–M6 設計；即時模型部分已退出現行範圍 |
| [冷開發結果](docs/catalog/04-offline-research/COLD_DEVELOPMENT_RESULT_20260929.md)／[合併審查](docs/catalog/04-offline-research/COLD_DEVELOPMENT_MERGE_REVIEW_20260929.md) | 已有組合庫、fake-clock 與性能證據 |
| [退步審查](docs/catalog/02-game-results/HOT_REGRESSION_REVIEW_20260930.md)／[恢復計畫](docs/catalog/02-game-results/BASELINE_RECOVERY_HOT_DEVELOPMENT_PLAN_20260930.md) | 歷史實戰、基線選擇與未證假說 |
| [舊版結果](docs/catalog/02-game-results/HD9_LEGACY_HD_RESULTS_20260928.md)／[新版結果](docs/catalog/02-game-results/MAIN_LEGACY_HD_COMPARISON_20260928.md) | 兩輪跨曲原始證據與限制 |
| [第三方授權](docs/catalog/08-licenses/THIRD_PARTY_NOTICES.md) | 固定依賴及授權原文 |
| [本次清理](docs/catalog/06-engineering/CLEANUP_AUDIT_20261001.md)／[9/28 清理](docs/catalog/06-engineering/CLEANUP_AUDIT_20260928.md) | 容量、保護與已刪範圍 |

src／include為正式核心；apps為CLI；tests／fixtures為回歸及Android C++ Fixture；configs為profile。measurements／out不在Git追蹤內，Git提交不等於備份原始證據。後續有限live依既有授權、gate與總控續派；本整理包不啟動。
