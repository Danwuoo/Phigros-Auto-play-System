# Phigros Auto-play System

Windows／C++20 的即時 pixels-to-touch 研究專案。正式程式以單程序多執行緒處理最新畫面、音符／判定線追蹤、撞線預測與可撤銷多指觸控；不讀譜或遊戲內部狀態。

## 當前狀態（2026-10-01）

main 已整合兩個 worktree 的 tracked 工作，commit `9fea67e248411f7c2309f220daf989f652b27d15`。候選為 **observer50／planner27／diagnostics11，live rounds=0**。完整來源、能力清單、未解問題與下一步比較見[現況與下一步研究](docs/PROJECT_STATUS_NEXT_STEPS_20261001.md)；來源整合細節見[整合交接](docs/MAIN_INTEGRATION_HANDOFF_20261001.md)。

- 整合 Release 回歸 349 項：348 通過、1 opt-in 跳過、0 失敗；離線 CPU 工具 8/8。這是軟體驗證，不是遊戲效果驗收。
- 恢復分支 C36h tint1 的 Dlyrotz IN13 為 795950／P496-G11-B0-M77，前次全錄亦 M77；使用者回報主要問題未改善。C36h 同號 37/19 與歷史 main 37/19 不是同一 binary。
- 保存全錄 7722 張 PNG、12 段選取與已填理由；pixel human gold=0。現有暖機 observer replay、原 journal join、合成 FakeTouch 冷鏈各自存在，完整真實全錄 owner 反事實重播尚未實作。
- 模型僅供離線輔助標記及主程式優化。C++ LibTorch CPU 小試已有權重與 18 張原生 ROI proposal；不進即時迴圈，不以合成 IoU 宣稱真實準確率。
- 本輪完成研究與整理；下一步建議先補最小完整接觸重播，再定位 Hold 的當前支持／身分／關聯失效。沒有啟動 emulator、真觸控、AP goal 或追加模型訓練。

歷史跨曲兩版各 19 首 HD 的 Miss 為 815→818，非 Perfect 957→860；沒有 AP 或未知曲泛化驗收。9/29 的 D1–D4／C0–C6 是已保存的冷工程成果，其後實戰與整合結果依日期分開閱讀。

[本次整理帳本](docs/CLEANUP_AUDIT_20261001.md)列出實際刪除、hash 核對、容量及保留範圍。恢復 worktree 的 ignored LibTorch／DLL references 仍在使用，不能因 Git 合併而刪除它。較早不可重算資料见[9/28 清理紀錄](docs/CLEANUP_AUDIT_20260928.md)。

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

以下使用本機已核對的整合候選；上面的 preset 重新建置會輸出至 `out/release-v145`，不能把該目錄殘留的舊 binary 當作目前候選。

```powershell
out/main-integration-v145/Release/pas.exe manual-session `
  --config configs/phigros-hd-assist-five-lead35.json `
  --capability measurements/game-semantics-20260927/touch-five/summary.json --no-preview
```

啟動後由使用者選曲並按 Play；程式僅由即時 HUD 接手。結算確認後釋放觸控、保存同源結算圖並回待命，不自動選曲／重試。Escape 或 Ctrl+C 停止。未知畫面不取得觸控資格；結算辨識限已核對的 1280×720 英文布局。

--pixel-clips 需使用從當前 source 重建的執行檔，或保留的第二輪 main-legacy-v145 binary；它啟用既有有限三幀診斷：最多 20 輪、每輪 30 張，writer mailbox 4 張；不是新研究的兩秒採樣器。--round-watchdog-s 預設 0，超時記 FAULT／aborted，不當成曲尾。這些命令是操作說明，不是自動啟動遊戲的授權。

熱調參分支另用 `configs/phigros-hd-assist-five-lead40.json`：manual-session 接受 30–45 ms lead，owner 依 profile 設定並在 manifest 記有效值。40 ms 四首已重測，分數皆小幅上升但 Miss 仍多。observer48／planner25 的後續手動試驗由使用者停止、無結算，不作效果證據。observer49／planner26 針對原線短暫消失補有界線位移投影；2026-09-30 的手動實測完成 Glaciaxion HD6、Dlyrotz HD9 兩首後依使用者要求停止，結果見[熱調試紀錄](docs/FOUR_SONG_HOT_TUNING_20260929.md)。

歷史比較版本請保留原 binary，不在當前 checkout 重建覆寫：
- 舊版：out/hd9-session-release/Release/pas.exe。
- 第二輪：out/main-legacy-v145/Release/pas.exe。
- 目前 out/release-v145 與第二輪 binary hash 不同，不能互當完全相同版本。

## 文件入口

| 文件 | 用途 |
|---|---|
| [現況與下一步研究](docs/PROJECT_STATUS_NEXT_STEPS_20261001.md) | 可核對清單、Hold 邊界、方案比較與最小交付 |
| [整合交接](docs/MAIN_INTEGRATION_HANDOFF_20261001.md) | 合併來源、frozen 配套、依賴與整合測試 |
| [架構](docs/ARCHITECTURE.md)／[路線圖](docs/ROADMAP.md) | 現行契約與後续優先順序 |
| [逐幀與 C36h 結果](docs/DLYROTZ_FRAME_ANALYSIS_C36H_20261001.md) | 使用者 12 段理由、原圖／事件及未改善結果 |
| [全錄契約](docs/FULL_ROUND_RECORDING_20261001.md)／[CPU 小試](docs/OFFLINE_CPU_VISION_PILOT_20261001.md) | 已有離線工具與限制 |
| [跨曲研究](docs/MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md) | 原始 M0–M6 設計；即時模型部分已退出現行範圍 |
| [冷開發結果](docs/COLD_DEVELOPMENT_RESULT_20260929.md)／[合併審查](docs/COLD_DEVELOPMENT_MERGE_REVIEW_20260929.md) | 已有組合庫、fake-clock 與性能證據 |
| [退步審查](docs/HOT_REGRESSION_REVIEW_20260930.md)／[恢復計畫](docs/BASELINE_RECOVERY_HOT_DEVELOPMENT_PLAN_20260930.md) | 歷史實戰、基線選擇與未證假說 |
| [舊版結果](docs/HD9_LEGACY_HD_RESULTS_20260928.md)／[新版結果](docs/MAIN_LEGACY_HD_COMPARISON_20260928.md) | 兩輪跨曲原始證據與限制 |
| [第三方授權](docs/THIRD_PARTY_NOTICES.md) | 固定依賴及授權原文 |
| [本次清理](docs/CLEANUP_AUDIT_20261001.md)／[9/28 清理](docs/CLEANUP_AUDIT_20260928.md) | 容量、保護與已刪範圍 |

src／include 為正式核心；apps 為 CLI；tests 與 fixtures 為回歸及 Android C++ Fixture；configs 為 profile。measurements 與 out 不在 Git 追蹤內，Git 提交不等於備份原始證據。历史實戰操作授權不自動延續到本輪。
