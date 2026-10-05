# Phigros Auto-play System

Windows／C++20 的即時 pixels-to-touch 研究專案。正式程式以單程序多執行緒處理最新畫面、音符／判定線追蹤、撞線預測與可撤銷多指觸控；不讀譜或遊戲內部狀態。

目前開發暫停；最新盤點與分類文件由 [文件總目錄](docs/README.md) 進入。下列2026-10-04狀態保留為歷史快照。

2026-10-05 新增授權的[雲端多分支 zero-miss 研究](docs/research/zero-miss-20261005/README.md)已重新盤點程式、逐曲總帳與建置阻塞，並完成隔離 C++ 冷實驗。BVI 核心可在 Linux 編譯；完整 Windows harness／原 suite／真機仍未驗。這次研究不等於全面恢復功能開發或實機。需要補哪些 ignored 證據，見[最小檔案清單](docs/research/zero-miss-20261005/IGNORED_INPUTS.md)。

## 當前狀態（2026-10-04）

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
