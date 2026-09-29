# Phigros Auto-play System

Windows／C++20 的即時 pixels-to-touch 研究專案。正式程式以單程序多執行緒處理最新畫面、音符／判定線追蹤、撞線預測與可撤銷多指觸控；不讀譜或遊戲內部狀態。

**後續主線：[主程式架構與跨曲學習研究](docs/MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)。** 先隔離線身分／音符關聯問題，依[判定線形式](docs/判定線形式.md)驗證幾何與動作策略，建立人工標註與基準；確認觀測／關聯瓶頸後才比較學習式方法。新模型尚未訓練或接入；目前没有 AP 或未知曲泛化驗收。

2026-09-29 [非學習式冷開發](docs/COLD_DEVELOPMENT_RESULT_20260929.md)完成分段journal追溯、既有RGB冷重播、FakeTouch反例修正及計畫§8的C0–C6可冷驗收。分析範圍與驗收門檻見[計畫](docs/NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md)。本輪沒有啟動emulator；熱測另階段。

同日續作：沿用原開發task，依[計畫§8的C0–C6](docs/NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md#8-持續冷開發工作包與資料回饋2026-09-29追加)完成既有像素輔助標記／有限通用調參、代表性RGB與fake-clock組合、Hold／排程及完整冷鏈優化。冷驗收通過只適用於離線合成與既有短RGB；新版尚無遊戲效果驗收，本輪未啟動emulator或訓練模型。

## 現況與證據

2026-09-29 合併審查與冷測量器修補見[驗收紀錄](docs/COLD_DEVELOPMENT_MERGE_REVIEW_20260929.md)；本機main整合不代表新版實戰驗收。

- 目前 checkout 為 observer47／planner23／diagnostics11；observer38／planner21／diagnostics7 配套已獨立凍結。現行合成RGB／oracle／fake-clock代表矩陣32／32列通過；線ID確認與局部重接、旋轉Hold同contact、Drag／Flick混合和容量退化均有正反例。C1的3／4／5px共用線缺口參數實驗保留4px，未把未覆核舊RGB當gold。六區塊逐列預檢在原始RGB微基準及25ms固定Tap的正式FakeCapture→LatestFrame→observer→owner→FakeTouch冷鏈均超過預先凍結噪聲容忍；密集128 Note場景的逐批加速實驗未過，但延遲與安全未超原計畫非退步容忍。Tap／密集各10,000幀長跑及受控慢writer／fake RPC已完成，完整結果和限制見[冷開發結果](docs/COLD_DEVELOPMENT_RESULT_20260929.md)。最近七輪實戰仍是observer37／planner19（六HD＋光IN）；新版沒有實戰遊戲結果。
- 兩版各比較同 19 首 HD：舊版 815 Miss，新版 818 Miss；新版 15 首分數提高，但少數曲明顯退步。另有新版兩首 IN，各一輪，不能當泛化驗收。
- 第二輪保存 186 組三幀／558 張全畫面 RGB，尚未具人工線／音符／關係真值。第 21 輪依採樣上限没有片段。
- 判定線 M1 的 D1–D4 已有正式 C++ 修正與合成／fake-clock 回歸；[實作與驗證紀錄](docs/JUDGMENT_LINE_M1_IMPLEMENTATION_20260928.md)列出實際 pixels 核對及仍未知的遊戲語義。兩秒採樣 ring 與模型未接入。
- 擷取固定 gRPC payload fast／256 KiB；觸控上限為已驗指紋下的五指。絕對來源年齡仍未知。
- 2026-09-28 已依使用者要求清除舊文件、舊量測與多餘建置。保留清單、執行結果及不可重算的歷史範圍見 [清理紀錄](docs/CLEANUP_AUDIT_20260928.md)。

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

## 使用者手動開始、多輪待命

```powershell
out/release-v145/Release/pas.exe manual-session `
  --config configs/phigros-hd-assist-five-lead35.json `
  --capability measurements/game-semantics-20260927/touch-five/summary.json --no-preview
```

啟動後由使用者選曲並按 Play；程式僅由即時 HUD 接手。結算確認後釋放觸控、保存同源結算圖並回待命，不自動選曲／重試。Escape 或 Ctrl+C 停止。未知畫面不取得觸控資格；結算辨識限已核對的 1280×720 英文布局。

--pixel-clips 需使用從當前 source 重建的執行檔，或保留的第二輪 main-legacy-v145 binary；它啟用既有有限三幀診斷：最多 20 輪、每輪 30 張，writer mailbox 4 張；不是新研究的兩秒採樣器。--round-watchdog-s 預設 0，超時記 FAULT／aborted，不當成曲尾。這些命令是操作說明，不是自動啟動遊戲的授權。

歷史比較版本請保留原 binary，不在當前 checkout 重建覆寫：
- 舊版：out/hd9-session-release/Release/pas.exe。
- 第二輪：out/main-legacy-v145/Release/pas.exe。
- 目前 out/release-v145 與第二輪 binary hash 不同，不能互當完全相同版本。

## 文件入口

| 文件 | 用途 |
|---|---|
| [架構](docs/ARCHITECTURE.md) | 現行執行流程、時鐘、容量與失效契約 |
| [路線圖](docs/ROADMAP.md) | M0–M6 階段及下一步 |
| [跨曲學習研究](docs/MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md) | 證據、候選、標註、模型部署與驗收設計 |
| [舊版結果](docs/HD9_LEGACY_HD_RESULTS_20260928.md)／[新版結果](docs/MAIN_LEGACY_HD_COMPARISON_20260928.md) | 原始兩輪比較與限制 |
| [跨版分析](docs/LEGACY_CROSS_VERSION_ANALYSIS_20260928.md) | 退步曲、線身分與時序線索 |
| [資料工具](docs/VISION_DATASET_20260927.md)／[追蹤對照](docs/TRACKING_COMPARISON_20260927.md) | 現有離線能力與真值缺口 |
| [第三方授權](docs/THIRD_PARTY_NOTICES.md) | 固定依賴及授權原文 |
| [清理紀錄](docs/CLEANUP_AUDIT_20260928.md) | 保留資料、刪除範圍與驗證 |

src／include 為正式核心；apps 為 CLI；tests 與 fixtures 為回歸及 Android C++ Fixture；configs 為明確 profile。measurements 與 out 不在 Git 追蹤內，Git 提交不等於備份原始證據。
