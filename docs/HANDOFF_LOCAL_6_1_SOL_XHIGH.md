# 本機 6.1 sol xhigh 交接 prompt

2026-10-25 本機接續已完成新 Windows native 29/29 CTest、21 份 baseline 重建、23 次離線前檢與新 boot 五指 fixture；歌曲比較依使用者要求暫緩。新 binary／設定與本機證據見 [本機接續紀錄](LOCAL_NATIVE_FLICK_2026-10-25.md)，勿將下列歷史待辦或 NOT RUN 當作最新狀態。

使用者追加的結算 v2 與一般回合不存圖片已建成新 003 封存；新設定使用 003，002 及 build006／AP 原件保留。v2 完成 100/100 原圖回放、29/29 native、4/4 portable，實機歌曲仍暫停。詳見 [結算與保存政策](RESULT_RECOGNITION_AND_STORAGE.md)。

以下區塊可直接貼給本機模型。先讀本文件及連結的驗證紀錄，再沿用現有工作目錄；不要重新設計已完成的曲庫或重抓同一份來源。

---

你要接續 `Danwuoo/Phigros-Auto-play-System` 的 `feature/chart-driven-multigame`，完成 Windows 建置、既有本機環境串接與 Flick 單變量試驗準備。使用者已授權完成公開 IN／AT 曲庫抓取、Git LFS 入庫與使用端改善；不要再次只提出方案。main 不動，私人 APK、校正、AVD、原始錄影與實測證據留本機。

## 1. 先了解已完成的工作

起始 commit 是 `3e66ab7a26d64a40289573fe0c5ab3f247b1c1c2`。後續 branch HEAD 含本輪程式碼與 LFS 資產提交；先 `git log -5 --oneline` 核對實際 HEAD。使用者舊目錄是：

```text
C:\Users\wurre\Documents\Codex\2026-10-23\new-chat\outputs\Phigros-Auto-play-System
```

這是使用者提供的位置，必須確認目前是否仍存在。不要把 Linux 雲端暫存路徑帶入 Windows launch config。先檢查 `git status`，保留任何使用者變更，正常 fast-forward 更新工作分支。

必要閱讀：

- `AGENTS.md`、`docs/TECHNICAL_DIRECTION.md`。
- `docs/SONG_CATALOG.md`、`docs/SONG_CATALOG_CLIENT.md`。
- `docs/FLICK_TUNING.md`、`docs/VALIDATION_CATALOG_2026-10-25.md`。
- `docs/MODEL_HANDOFF.md`、`docs/SESSION_RECORDS_2026-10-24.md`。
- 需要原生建置時讀 `docs/M6_BUILD.md`；需要原 Legacy 面板時讀 `docs/SONG_TEST_PANEL.md`。

本輪使用者明確指定公開來源原譜／音源用 Git LFS 入庫，優先於舊文件「所有原始素材不進 Git」的概括規則；範圍僅限新 `assets/phigros/4.0.1.2/`。不要因此將私人素材加入 Git，也不要改寫歷史封存敘述。

已完成：327 首、333 IN（含 6 變體）、56 AT、327 OGG、3 metadata，共 719 份來源資產完整性核對。固定上游三個 commit 在 `configs/song-catalog-sources.json`，不是 latest 分支。新 schema `pas.catalog-library.v1` 不等於原本 21 首的本機實測 library schema。

嚴格 native importer 實際結果：356 passed、5 v1 unsupported、28 rejected；28 中 13 為倒置事件區間，15 為未支援欄位。全部來源原件均保留。公共來源版本 **4.0.1.2**，原生實測環境 **4.0.1／157**，身份核對未完成前不能自動套用。

## 2. 先讓曲庫可用

從 repository root，用 PowerShell 7.2 以上執行：

```powershell
git switch feature/chart-driven-multigame
git pull --ff-only origin feature/chart-driven-multigame
git lfs install --local
git lfs pull --include="assets/phigros/4.0.1.2/charts/**,assets/phigros/4.0.1.2/music/**"
python tools/sync_song_catalog.py verify --output assets/phigros/4.0.1.2
pwsh -NoProfile -File tools/start-song-library.ps1 -SelfTest
pwsh -NoProfile -STA -File tools/start-song-library.ps1
```

若只要先看索引，可先不 hydrate LFS；介面會標示原件待下載。不要在部分下載狀態執行 verify 後把缺項 library 提交到遠端。LFS 發布已完成於資產 commit `9ad6700678cc8ddbb259a5611a9be74cd9f6c5a2`；716 份遠端 pointer 的 OID／長度均核對相符，716 個 LFS 物件可讀，另有 4 份完整下載 SHA256 回驗。詳見本輪 validation 與 `manifests/remote-lfs-verification.json`，不需要重抓或重推同一份來源。

新使用端是 `tools/song-library-panel.ps1`，快捷入口是 `tools/start-song-library.ps1`。支援搜尋、IN／AT／變體／狀態篩選、來源與完整性、匯入報告、開啟原譜音源與連接舊本機面板。曲庫載入與 hash 核對使用背景 runspace，搜尋採 160 ms debounce；啟動不接觸裝置。

先核對實際舊 `launch-config.json`，再以 `-LegacyConfiguration <path>` 連結；使用者按「開啟既有實測面板」後才會啟動原 Legacy 面板，不會把正在瀏覽的公共來源譜面直接送到 native runner。

## 3. 快速建置

確認 CMake、C++20 compiler、Python、PowerShell 路徑。預設只做 portable 離線建置：

```powershell
pwsh -NoProfile -File tools/build-local.ps1 -Mode Portable
$env:PAS_TEST_PWSH = (Get-Command pwsh).Source
python -m unittest discover -s tests -p 'test_*.py'
```

若 LZ4 未安裝，先使用本機專案的隔離 Python 環境與既有 assets requirements；報告測試 skips，不可把跳過計入通過。不要為這一輪無故重建整套 vcpkg。

原生建置需要既有 patched gRPC 的 x64-windows prefix，應含 `include/grpc/pas_windows_read_config.h`。先找 `docs/M6_BUILD.md` 的現有 `work/phigros-vcpkg-install/x64-windows` 或搬移後對應目錄，唯讀核對，不換成 stock gRPC。

```powershell
$dependencyPrefix = 'C:\實際路徑\work\phigros-vcpkg-install\x64-windows'
$vcpkgToolchain = 'C:\實際路徑\vcpkg\scripts\buildsystems\vcpkg.cmake'
pwsh -NoProfile -File tools/build-local.ps1 `
  -Mode Native -DependencyPrefix $dependencyPrefix `
  -VcpkgToolchain $vcpkgToolchain -PrintOnly

pwsh -NoProfile -File tools/build-local.ps1 `
  -Mode Native -DependencyPrefix $dependencyPrefix `
  -VcpkgToolchain $vcpkgToolchain -Jobs 4
```

`-CMakePath`／`-CTestPath` 可指定不在 PATH 的完整路徑，`-BuildDirectory` 可指到新的短路徑，`-Generator` 可明確指定已安裝的 Visual Studio 或 Ninja。上述 `C:\實際路徑` 必須換成已確認存在的目錄。Native mode 開啟 chart importer、visual candidates、supervisor、calibration trial、PAS core／gRPC／tests／bridge／session runner；音訊線保持暫緩。

只有需要 importer 時可用 `-Mode Importer -DependencyPrefix <nlohmann_json 或既有完整 prefix>`，不用 gRPC。本輪 Windows MSVC portable 4／4、importer／Flick 13／13、12 項使用端測試及 14 checks SelfTest 均通過；1440×900／1080×720 實際 WinForms 圖片也完成版面檢視。完整 Python 為 223／223、0 skips。這些結果不涵蓋 Windows native 主程式與 patched gRPC 全套；本機完成後記錄實際 CTest 名稱、數量與結果。

## 4. 新 binary 的封存與面板串接

新建置不能覆蓋舊 build006 或 Dlyrotz AP 的 binary／manifest，也不能在原 launch config 中只改 hash 假裝沿用舊實測資格。

依 `tools/run-adaptive-song.ps1` 實際讀取的 `pas.adaptive-build-archive.v1` schema，建立新 archive 目錄：收集新 `chart_experiment.exe`、所需 DLL、來源／build／測試證據與每檔 SHA256。目前 repo 沒有名為 `archive-runtime.ps1` 的工具，不要假設它存在；可在本機補一個明確的新封存工具並測試它的檔案界線。

重新產生新 profile／plan 與新 launch config，保留各舊檔。`run-adaptive-song.ps1 -CheckOnly` 核對封存與 profile；有 5 點需求時使用該輪 boot 有效的 capacity evidence。新二進位可能使冷開機 bootstrap 的來源／binary hash pin 需要建立新 config，不能手動解除舊檢查。

本輪雲端沒有 Windows AVD、校正原件與已封存 native binary。實際 native 主程式編譯、check-adaptive、裝置與遊戲判定要在本機補驗。結算漏記及 20 ms lateness 停止仍是原本未解項目，不能標為這輪修好。

## 5. Flick：先幅度，再按證據調時機

使用者認為照譜面打的方向正確，優先調觸控幅度與時機。已把以下四個欄位接進真正 `prepare-adaptive`／profile／`check-adaptive`／runtime：

| 欄位 | 舊預設 | Native flag |
| --- | ---: | --- |
| `flick_lead_ms` | 30 | `--flick-lead-ms` |
| `flick_travel_ms` | 60 | `--flick-travel-ms` |
| `flick_distance_reference_pixels` | 60 | `--flick-distance-pixels` |
| `flick_release_ms` | 24 | `--flick-release-ms` |

保持 `lead < travel`。DOWN 在 judgment−lead，滑動持續 travel，末端停 release 後 UP。改距離會改名義速度；實際螢幕路徑還受判定線運動與 projection 影響。舊 profile 沒欄位仍採 30／60／60／24；Tap／Drag 不受 Flick 參數改變。

native prepare 先 compile 再把真正第一個 DOWN 綁定 opening，避免調 lead 後仍沿用硬編碼的首觸時序。參數改動必須重新準備 profile 與 fingerprints，不能直接在舊 JSON 改數字後沿用 expected plan SHA。

對一首可跑完但仍漏 Flick 的歌曲，先診斷既有回合：

```powershell
python tools/analyze_flick_misses.py `
  --run-dir 'C:\實際私人目錄\song-live-001' `
  --output 'C:\實際私人目錄\analysis\song-live-001-flick.json'
```

輸出為 run 目錄外的新檔。查看逐 Flick lateness、RPC duration、螢幕距離／速度、重疊、pointer 重用與原譜 note pointer；HUD 只提供經核對的時間索引，不會把 combo 下降自動當成某個 note Miss。

從既有正確來源 profile 產生 baseline 加單一幅度候選：

```powershell
python tools/prepare_flick_trials.py `
  --baseline-profile 'C:\實際私人目錄\ready\song-profile.json' `
  --chart 'C:\實際私人目錄\assets\Chart_IN.json' `
  --source-manifest 'C:\實際私人目錄\assets\source-manifest.json' `
  --executable 'C:\新封存\bin\chart_experiment.exe' `
  --output 'C:\實際私人目錄\flick-distance-001' `
  --vary flick_distance_reference_pixels --value 48 --value 72
```

48／72 是比較用候選，不是已證明更好。產出的 `prepare-commands.ps1` 只執行 offline preparation，需原 source manifest 旁的匹配 WAV 等既有條件。查看各 `report.json` 並完成 `check-adaptive` 後，再保留原操作員選曲／READY／Play 流程。

固定 phase、APK／chart、解析度、Tap／Drag、inset、projection、HUD 與 capacity；A 為 baseline，B 為一個候選，交錯做 B–A–B 或 A–B–A，保留全部完成與中止回合。先比較距離，證據需要時再獨立比較 lead、travel、release。沒有多輪改善證據前，不改所有歌曲共用預設。

## 6. 全庫後續順序

1. 先把 Windows native build 與現有 Legacy 曲目串接好，驗舊 profile 預設相容，再挑一首有 Flick Miss 的曲目做單變量比較。
2. 比對公開來源與安裝版本的譜面 SHA／音源／song ID，建立明確 identity mapping；保留所有原件。
3. 對 13 份倒置事件用既有明確 compatible policy 另外稽核；先讓 `chart_inspect` 可明確選政策並保持 strict 預設。不要把這些結果混進既有 strict 報告。
4. 研究 15 份新欄位與 5 份 v1 的語義並補實作／測試，不能忽略欄位、刪 note 或改來源 SHA。
5. AT 共用 HUD 尚未具備；逐曲 calibrated-title 路徑也要真實校正證據。Random 變體要確認實際選曲與遊戲對應。
6. 對超過 notes／steps／contacts／180 秒限制的譜面另做容量與完整時間線設計，不任意提高上限後宣稱可用。
7. 結算漏記、20 ms lateness 與實測覆蓋要繼續保留為待處理；RPC success／離線通過不等於 AP。

完成回報請列：實際 commit、建置位置與 binary SHA、測試結果／skips、曲庫完整性、Windows 畫面驗證、Flick 每輪參數與獨立結算、仍未完成項目。程式碼可繼續提交到同 branch；不要 force push，不要動 main。

---

本文件的命令含需由本機實際盤點替換的路徑，沒有假設本機私人原件已在雲端可用。最新雲端與遠端發布驗證以 [本輪驗證紀錄](VALIDATION_CATALOG_2026-10-25.md) 為準。
