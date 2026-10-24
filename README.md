# Rhythm Chart Memory — Phigros / 世界計畫研究分支

**方向：譜面／操作記憶驅動，音訊同步，視覺只做定位、校正與異常停止。**

這是 `feature/chart-driven-multigame` 的精簡起點，不是已完成的雙遊戲自動遊玩器。來源為 `main` 的 `2d479e17fa1566c089dce019cbb11eb883ba47fd`；原純視覺程式、實驗、分數與歷史仍保留在 main，沒有覆寫或合併。

**Dlyrotz IN13 AP 已於 live011 達成一次。** [獨立結算證據](../local-assets/phigros-4.0.1/m7-whole-song-live-011/result-observed-001.json)確認 **1,000,000 分、584 Perfect／0 Good／0 Bad／0 Miss、Max Combo 584、100.00%**。[全曲實驗](docs/M7_WHOLE_SONG_EXPERIMENT.md)由首音 gRPC 視覺觀測觸發原生譜面，完整送出 **584 contacts／5,669 events／峰值 4 點**，manual-mode 確認結算、failure null、最後 kernel zero。這是指定 Phigros 4.0.1／AVD／chart／binary／設定的一輪 AP；跨輪重現率、其他曲目／遊戲、來源像素 age 與獨立 input lead 尚未驗證，音訊仍暫緩。原 manifest 的 `unreviewed`／`AP_verified=false` 保留，後續獨立 sidecar 才記錄本輪 `AP_verified=true`。

本輪成功設定與失敗歷史已依 [實作計畫](docs/CHART_DRIVEN_IMPLEMENTATION_PLAN.md)及[驗證紀錄](docs/VALIDATION.md)封存。下一步可在固定身份下驗跨輪重現率；M0–M7 各項未取得的時間／音訊／生產資格仍保留原狀，不由單次 AP 一併宣告完成。

換模型或接續工作請先讀 [模型交接與 AP 重跑設定](docs/MODEL_HANDOFF.md)。本機可用 `tools/run-dlyrotz-ap.ps1 -CheckOnly` 核對成功封存，確認後再以人工開始的一輪模式執行。

**M0 已完成**：從既有 AVD 的 Phigros 4.0.1 分包取得 Dlyrotz IN 原生 v3 譜面（24 lines／584 notes）及匹配音源；重跑抽取的 10 份輸出 SHA 完全一致。來源鏈、工具與本機素材位置見 [M0 資產報告](docs/M0_ASSETS.md)。

**M1 已完成**：[嚴格 v3 importer／chart_inspect](docs/M1_IMPORT.md) 保留原生語義與來源 SHA；真實 584 notes 的 1,168 個 start/end 時間已逐項核對。幾何／接點與全曲實驗進度另列於下；音訊同步資格仍暫緩。

**後續進度**：[M2 幾何](docs/M2_GEOMETRY.md)、[M3 接點編譯](docs/M3_CONTACTS.md) 與 [M5 bridge](docs/M5_PAS_BRIDGE.md) 已有離線驗證；完整 Dlyrotz 候選的 **584 notes／4,911 actions** 通過 FakeTouch 與 M7 supervisor 整合回放，峰值 4 點、pending 峰值 8，逐筆回執吻合且全部釋放。畫面外 6 個 notes 由有界法向交集策略保留，沒有丟棄，但實際判定接受度尚待校正。[M4 音訊](docs/M4_AUDIO_SYNC.md) 未通過全首同步門檻；[M6 裝置報告](docs/M6_DEVICE_VALIDATION.md) 已核對單指、四指、批次、release-all、模擬 client transport loss 與實際 STOP 後釋放。加入非同步 foreground watcher 的 manual build **16/16 CTest** 通過，兩次只讀一輪起訖均完成；遊戲 input lead／jitter 與整首輸入資格仍待驗；後續單 Tap 的實際判定另列於下。FakeTouch 與 CTest 不是遊戲成績。

**目前優先順序**：依使用者要求暫緩音訊線，繼續全曲接點候選、[非音訊 supervisor](docs/M4_SUPERVISOR.md)、[畫面狀態錨點](docs/M4_SCREEN_STATE.md)、M6 觸控證據與 [M7 session runner](docs/M7_RUNNER.md)。畫面狀態已有 13 個實際樣本核對；生產 M7 SessionRunner 保留 FakeTouch／qualification gate，另有明示未取得資格的全曲實驗入口，已完成四輪整曲輸入，live011 取得一次 AP。遊戲設定目前 Offset 為 0 ms；歌曲相位和 backend 輸入提前量仍各自待校正。

**開始與結束沿用 manual-mode**：由使用者選曲並按 Play，程式以不同送達且未過期的 UI 觀測確認播放／結算；暫停或黑畫面只暫停處理，結算頁保持鎖定直到人工下一輪。新的 [manual-mode 接續](docs/M7_MANUAL_MODE.md) 保留這套生命週期及 Esc／Ctrl+C 停止方式，先提供只讀 `chart_manual shadow ... --observe-only` 入口。送達序號不證明影格由不同次渲染產生，也未界定來源像素有多舊，見 [capture-clock 界線](docs/M4_CAPTURE_CLOCK.md)。UI 起訖與曲中精確時鐘分開驗證。

兩次實際只讀回合皆一開始、一確認結算；002 開場舊結算頁保持 latch，人工 retry 後才開新回合。非同步 foreground 檢查的 002 觀測到 consumer skips 0、直接 PLAYING→STARTING 到期回退 1 次（001 為 718 skips／97 次），但仍有 unknown 與 594 ms 最大間隔；這不是受控 benchmark 或連續輸入資格。另發現的 watchdog 邊界修正獨立驗證，不套用這輪結果。詳見 [兩輪證據與限制](docs/M7_MANUAL_MODE.md)。

**歷史首 Tap 校正進度**：[實驗 CLI](docs/M7_FIRST_TAP_CALIBRATION.md) 的 live004（phase 0 ms）實際結算為 **0 Perfect／1 Good Late／583 Miss**；live005 與 live007（−80 ms）各為 **1 Perfect／0 Good／0 Bad／583 Miss**，同設定 live006 則為 **0 Perfect／1 Good Late／583 Miss**。各次只有一組 DOWN／UP，其餘音符不在試驗預算內，最後指定 kernel route 為零。兩次 Perfect 與中間的 Good Late 都保留；這些歷史單 Tap 結果不證明穩定相位、input lead 或 AP。先前 live002／003 在 DOWN 前失效停止的原件也保留；沒有放寬 freshness gate。

**歷史三 Tap 與全曲中止紀錄**：三 Tap live001 有 3 DOWN／3 UP 與 kernel zero，缺最終結算。全曲 live005／006 的 53／150 Perfect 與停止紀錄保留於[全曲報告](docs/M7_WHOLE_SONG_EXPERIMENT.md)。[HUD Build005](../local-assets/phigros-4.0.1/m7-whole-song-build-005/manifest.json) 通過 **24/24 CTest、default 4/4、8 組／81 HUD checks**；[live007](../local-assets/phigros-4.0.1/m7-whole-song-live-007/manifest.json) 使用 226 幀 HUD fallback 越過先前停止區域，送出 **196 DOWN／1,514 MOVE／195 UP、1,905 筆成功 RPC**，於 `compiled_trajectory_coverage_expired` 停止並確認 kernel zero。[結果 sidecar](../local-assets/phigros-4.0.1/m7-whole-song-live-007/result-observed-001.json)確認 **196 Perfect、388 Miss、Max Combo 196、score 335,616**。

**成功設定（live011／Build009）**：[Build009](../local-assets/phigros-4.0.1/m7-whole-song-build-009/manifest.json) 通過 **25/25 CTest、default 4/4**。Flick 使用 **30 ms lead／60 ms travel／60 reference pixels／24 ms 末端停留**，Tap／Drag 32 ms、phase −80 ms；480 個非 Flick contacts 與 live010 完全相同，104 個 Flick 重編譯後全譜為5,669 events。[來源與時序稽核](../local-assets/phigros-4.0.1/m7-whole-song-live-011/analysis/live011-timing-audit.md)逐筆核對來源／phase／pixel／deadline／完整步數，全部吻合；DOWN 最大發送遲到 **3.896696 ms**，全 events 最大 **5.955067 ms**。遊戲 AP 由結果圖獨立確認，不從 RPC success 推導，也未由一次成功證明每個參數的因果。

**歷史 live010** 使用24 ms Flick末端停留，完整584／4911後為580 Perfect／4 Miss；lead16 ms／travel32 ms／位移48保持舊值。其原件與使用者Flick漏判觀察保留於全曲報告。

**前一輪實驗（live009）**：[Build007](../local-assets/phigros-4.0.1/m7-whole-song-build-007/manifest.json) 通過 **25/25 CTest、default 4/4**，保留 Build006 完整 contact tail／高精度 owner wait，將實驗 Tap／Drag dwell 改為 **32 ms** 並加入釋放後寫檔的 HUD timeline。Live009 完成 **584 DOWN／3,743 MOVE／584 UP**，failure null、manual-mode 確認結算、最後 kernel zero；[獨立結果](../local-assets/phigros-4.0.1/m7-whole-song-live-009/result-observed-001.json)為 **579 Perfect／5 Miss**。[逐筆稽核](../local-assets/phigros-4.0.1/m7-whole-song-live-009/analysis/live009-timing-audit.md)確認 104 Flick／78 Hold 全部 steps 與 live008 相同，只有 402 Tap／Drag 的 UP 延後 16 ms，所有 DOWN 不變。使用者觀察 Miss 全是 Flick，接續以保存的 **1,904 個 HUD samples** 定位；不能從兩輪 3→5 Miss 推論 32 ms 的因果。Phase 維持 −80 ms，時間／停止／生產 gate 未放寬。

[Build006](../local-assets/phigros-4.0.1/m7-whole-song-build-006/manifest.json) 的 **23 組／12,582 runtime checks、24/24 CTest、default 4/4**、[主機等待量測](../local-assets/phigros-4.0.1/m7-host-wait-001/REPORT.md)與[非零 RPC 全譜離線封存](../local-assets/phigros-4.0.1/m7-whole-song-latency-replay-001/README.md)各自保留原範圍，不能作為遊戲時間資格或 AP。

## 接續既有畫面紀錄與本機環境

專案曾完成 Dlyrotz 的 **7,722 張連續 PNG、約 132 秒全錄、12 段人工選片與相關日誌**。20 首 score-only 資料沒有逐幀，不代表其他歷史工作也沒有。這次補回三份原始錄製／分析文件與外部原件索引；大型原圖在被 Git 忽略的 `measurements/`，clone 不會自動取回。

先讀 [資產接續與實作順序](docs/ASSET_RECOVERY.md) 和 [來源／核對指紋](docs/LEGACY_ASSETS.json)。本機 AVD 另存於 Android 使用者目錄，不屬於 Git 分支；可用唯讀入口盤點 SDK、已註冊或遺留的 AVD 與已知錄影位置：

```powershell
& ./tools/inspect-local-assets.ps1
# 若已找到搬移後的舊專案，指定該根目錄；不啟動裝置或注入輸入。
& ./tools/inspect-local-assets.ps1 -LegacyRepositoryRoot 'C:\path\to\old-checkout'
```

後續先以 Phigros 的既有資料完成離線記憶與視覺錨點，再做音訊 shadow 同步；兩款遊戲各自保留 adapter 邊界。

使用者已確認舊本機原件刪除；本機接續改為用現存 AVD **重新錄製 Dlyrotz IN13 的影音觀測基準**。新的 [錄製與解碼工具](docs/RECORDING.md) 保留來源時間線及品質檢查，供接下來的記憶 importer／同步器使用。這是觀測資料，不是已完成的自動點擊整合。

## 已有與尚未完成

| 範圍 | 狀態 |
| --- | --- |
| 舊 PAS clock、LatestFrame、ContactScheduler、觸控抽象、座標映射 | 原檔保留 |
| 舊 ADB、gRPC 畫面／觸控、proto、自訂 gRPC patch | 原檔保留；Windows source build／runtime 與 fixture 單指／四指、所測釋放路徑通過；遊戲時序資格待驗 |
| 跨遊戲 resolved-contact memory、身份欄位、接點生命週期／容量檢查 | 已有可執行的離線骨架 |
| 仿射時鐘映射、輸入提前量、同步租約有效性檢查 | 已有模型與離線 audio estimator；真實音訊門檻仍待通過 |
| Phigros / Project Sekai dry-run | 同一組合成接點，以兩種 game ID 驗證；不是兩套遊戲 adapter |
| 本機 AVD 盤點、gRPC 短錄、scrcpy 觀測錄製與離線解碼索引 | 已有明確 opt-in 工具，不連入 default tests |
| Phigros v3 原生譜面 parser／時間正規化 | M1 離線驗證通過；世界計畫 importer 待做 |
| 幾何 adapter、接點編譯、planner→PAS bridge | 核心與部分真實譜面離線通過；整首候選／實際判定待校正 |
| 音訊對齊、WASAPI packet adapter | 已完成兩輪有界 live shadow；未通過全曲同步資格，目前依使用者要求暫緩 |
| 畫面 playing／paused／result／unknown、同步期限與停止監督 | 有界像素模板與 supervisor 已有離線／樣本核對；capture clock 資格仍待量測 |
| M7 compiler→supervisor→PAS→FakeTouch | 生產入口保留 fingerprint／qualification gate；另有實驗全曲入口；live011 完成 584／5669 並取得一次 AP，時間／生產資格仍另驗 |
| Manual-mode 開始／結束 | 非音訊整合 16/16 CTest；兩個獨立只讀回合各完成一開始／一結算，002 另驗證初始 result latch→人工 retry；同程序多輪、連續 playing 及遊戲時序資格待驗 |
| 開場 Tap 視覺校正實驗 | −80 ms 的 live005／007 各一個 Perfect，006 為 Good Late；三 Tap live001 有 3 DOWN／UP 和 kernel zero，缺最終結算，判定未驗證 |
| 全曲 chart_experiment | Live011：584／5669、584 Perfect／零 Good／Bad／Miss、1,000,000分、最後 kernel zero；Build009 25/25 CTest、default 4/4。一次 AP 已確認 |
| M6 fixture 容量／停止／釋放 | 單指與四指路徑、batch、release-all、client 故障 recovery 與 STOP002 cleanup 通過；失敗／缺證據的舊案例保留，未量最大容量 |
| 全曲自動點擊、AP、遊戲 input lead／jitter、端到端延遲 | 指定 Dlyrotz IN13 一輪 AP 已達成；跨輪重現率、獨立 input lead／jitter與端到端延遲資格仍未驗證 |

## 離線建置（不需要遊戲、vcpkg 或第三方套件）

需要 C++20 compiler 與 CMake 3.20+。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Linux / 單組態 generator：

```sh
./build/rhythm_dry_run phigros
./build/rhythm_dry_run project-sekai
```

Visual Studio 多組態 generator：

```powershell
.\build\Release\rhythm_dry_run.exe phigros
.\build\Release\rhythm_dry_run.exe project-sekai
```

dry-run 只印出合成事件，不連線、不注入輸入。範例的全零 hash 是明示的假資料；validator 只檢查 hash 字串格式，並未驗證真實資產內容。

## 結構與閱讀順序

```text
include/rhythm/chart_memory.hpp   新的離線資料／時間契約
apps/chart_memory/main.cpp        無裝置的合成 dry-run
tests/chart_memory_tests.cpp      新契約測試
include/pas/ + src/               精選保留的舊 C++ 核心
proto/ + third_party/             舊 emulator protocol 與必要 gRPC overlay
cmake/PasGrpc.cmake               可選的 Windows backend 建置
```

先讀 [技術方向](docs/TECHNICAL_DIRECTION.md)，再看 [保留與排除清單](docs/MIGRATION.md)、[來源指紋](docs/EXTRACTION.json) 與 [本輪驗證範圍](docs/VALIDATION.md)。

## 可選的舊 Windows 核心

`RHYTHM_BUILD_PAS_CORE=ON` 建置保留的 scheduler / evidence / ADB libraries，需 `nlohmann_json`（ADB 公開 header 使用 JSON）。使用 vcpkg 時選 `legacy-core`；或透過 `CMAKE_PREFIX_PATH` 提供已安裝的 JSON package。default offline build 不受這項依賴影響。

```powershell
cmake -S . -B build-core -A x64 "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_MANIFEST_FEATURES=legacy-core -DRHYTHM_BUILD_PAS_CORE=ON
cmake --build build-core --config Release
```

gRPC backend 與舊測試需要對應 vcpkg features，例如：

```powershell
cmake -S . -B build-win -A x64 "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" "-DVCPKG_MANIFEST_FEATURES=legacy-grpc;legacy-tests" -DRHYTHM_BUILD_PAS_GRPC=ON -DRHYTHM_BUILD_PAS_TESTS=ON
cmake --build build-win --config Release
ctest --test-dir build-win -C Release --output-on-failure
```

本機 MSVC 19.50 已成功重建 core／ADB／patched gRPC，runtime patch contract 與保留 PAS GTest 均通過，見 [M6 build](docs/M6_BUILD.md)。不能以標準 gRPC 靜默取代 `grpc/pas_windows_read_config.h` 與 patch API。`touch_probe` 提供明確 opt-in 的 fixture 輸入入口，`chart_manual shadow` 提供只讀 UI 觀測；`chart_experiment --experimental-whole-song` 已完整實測指定曲目，live011 取得一次 AP；原中止紀錄保留。[裝置報告](docs/M6_DEVICE_VALIDATION.md) 分開列出 binary 身份、實測範圍與未取得的時間資格。
