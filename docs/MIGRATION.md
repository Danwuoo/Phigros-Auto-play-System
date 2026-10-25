# 分支抽取與依賴邊界

2026-10-25 本機接續新增有界 runtime archive 工具及其邊界測試，並修正原生稽核測試的 Windows 路徑判斷；保留 PAS／proto／third_party sources 未改。新 binary、profiles、launch configs 與比較證據另存，不繼承 build006／AP 資格，見 [本機 native 與 Flick 紀錄](LOCAL_NATIVE_FLICK_2026-10-25.md)。

同日依使用者追加要求，修改實驗 app 的結算標籤判讀與影像保存、metadata-only HUD 及分析／面板提示；以新 v2 policy 明確啟用，舊 v1 與原件保留。沒有改保留 PAS sources／傳輸／接點 compiler 或輸入期限，詳見 [結算與保存政策](RESULT_RECOGNITION_AND_STORAGE.md)。

來源 commit：`2d479e17fa1566c089dce019cbb11eb883ba47fd`。
目的分支：`feature/chart-driven-multigame`。

這是以來源 commit 為 parent、建立精選新 tree 的普通 Git 分支。**不是 orphan branch，也沒有清除 Git 歷史**；目前 checkout 不帶入舊實驗，但祖先仍可存取那些檔案，因此不宣稱完整 clone 的歷史體積會縮小。

## 原樣保留（Git blob/tree 身份見 EXTRACTION.json）

| 檔案／群組 | 保留理由 |
| --- | --- |
| `include/pas/core.hpp`, `src/core.cpp` | Clock、LatestFrame、ContactPlan、ContactScheduler、FakeTouchBackend、PixelCoordinateMap |
| `include/pas/adb.hpp`, `src/adb.cpp` | 既有裝置管理／畫面輔助工具；不是逐 note 的 shell tap 主通道 |
| `include/pas/emulator.hpp`, `src/emulator.cpp` | GrpcCapture、GrpcTouch、release 與 transport failure 處理 |
| `include/pas/grpc_transport.hpp`, `src/grpc_transport.cpp` | 既有 transport 設定與 patch 契約 |
| `include/pas/touch_evidence.hpp`, `src/touch_evidence.cpp` | 既有輸入證據解碼與路徑檢查；舊 core tests 的必要依賴 |
| `include/pas/manual_score_budget.hpp` | 有界 session 的可複用 budget helper；新 runtime 尚未接通 |
| `tests/core_tests.cpp` | 舊核心回歸來源；本輪未在 Windows 執行 |
| `proto/`, `third_party/` | emulator proto 及 gRPC overlay 完整來源／原註記 |
| `.gitattributes`, `vcpkg-configuration.json` | 原有 Git 屬性與 overlay 路徑 |

共 14 個獨立保留檔案，加上 proto 的 1 檔及 third_party 的 14 檔：29 個原樣保留檔案。新建／重寫 13 個檔案；預期 tree 合計 42 個檔案。

以上為初次抽取 commit `3be959e` 的清單，不是後續 checkout 的固定檔案數。2026-10-23 的本機接續補回三份錄製／逐幀分析／dataset 原文件及 `external-originals.json`，其基準 blob 身份另列於 [LEGACY_ASSETS.json](LEGACY_ASSETS.json)。保留的 PAS sources 不變。

## 不帶入這個分支的現行 tree

`.idea/`、舊 `research/`、大型 `docs/research/`、開發歷史、舊 fixtures、舊 tools/scripts、舊 configs、一般 benchmark 與實驗 tests、舊 apps/launchers、vision-first game/motion/tracking/runtime/manual_session、native/scrcpy capture 替代路徑，均留在來源 commit/main。

這不是判定那些資料沒有價值，而是避免重建依賴再拉回整個 research graph。完整基準與分數可從 [原 README](https://github.com/Danwuoo/Phigros-Auto-play-System/blob/2d479e17fa1566c089dce019cbb11eb883ba47fd/README.md) 追溯。

抽取時遺漏了對其他完整畫面紀錄的接續說明；20 首 score-only 不能代表全部歷史。錄製文件與核對索引現已精選補回，舊錄製／回放程式仍可按基準 commit 取用，沒有加入 default build。大型原始 PNG、影片、journal、已編譯 binary 及本機 AVD 不會因 Git clone 而恢復；`measurements/` 亦維持忽略，避免把找回的本機資產混入版本控制。見 [ASSET_RECOVERY.md](ASSET_RECOVERY.md)。

## 建置與資格

新的 default build 只建置跨平台、無第三方依賴的 memory prototype。Windows PAS core、gRPC 與舊 GTest 明確 opt-in；新的 CMake 不參照移除的 research、舊 provenance generated header 或既存 main80 objects。

gRPC overlay 不會被默默移除或改成 stock。`vcpkg.json` 延用來源 baseline，但把依賴分成 optional features。初次抽取未測 Windows；本機接續已重建 core／ADB，並修正 `pas_adb` 遺漏的 JSON dependency（`legacy-core` feature、CMake package discovery 與 PUBLIC target link）。原 PAS source blobs 不變。gRPC fresh build、backend ABI/服務版本與裝置能力仍需驗證。

歷史 cost NOT_READY、live UNQUALIFIED、STOP 與分數只屬於舊工作；新分支不重寫、不繼承為通過，不自動開遊戲。沒有建立合併到 main 的 PR，也沒有改預設分支。

## 2026-10-24 共通匯入與首音候選來源紀錄

新增明確source-priority／constant-empty匯入政策、完整原始宣告／異常映射；嚴格預設不變。接點編譯新增可選解析投影速度上界，政策／來源自動綁入fingerprint；首音共通模型另綁來源與政策。chart_experiment新增不可修改的預驗證計畫移轉、共通IN HUD候選及Windows Unicode路徑處理。保留的14個PAS blobs、proto、third_party及main不變；原AP封存不重標。原譜與私有成果只在repo外，未寫回或提交。具體語義／限制見 [共通政策](CHAPTER_LEGACY_COMMON_POLICIES.md)。

後續新增terminal-only結果caption特徵，以及明確的每輪兩實際開場觀測標題冗餘HUD候選；只改實驗app／新header／測例與launcher，不改保留PAS sources。診斷採用既有immutable frame lease／原有capture與publication時戳，並把有界RGB／日誌配置移至READY前；沒有修改PAS pool實作或重新定義receipt時鐘。所有原始失敗與成功封存分別保留。
