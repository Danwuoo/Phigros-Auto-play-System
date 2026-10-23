# 分支抽取與依賴邊界

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

## 不帶入這個分支的現行 tree

`.idea/`、舊 `research/`、大型 `docs/research/`、開發歷史、舊 fixtures、舊 tools/scripts、舊 configs、一般 benchmark 與實驗 tests、舊 apps/launchers、vision-first game/motion/tracking/runtime/manual_session、native/scrcpy capture 替代路徑，均留在來源 commit/main。

這不是判定那些資料沒有價值，而是避免重建依賴再拉回整個 research graph。完整基準與分數可從 [原 README](https://github.com/Danwuoo/Phigros-Auto-play-System/blob/2d479e17fa1566c089dce019cbb11eb883ba47fd/README.md) 追溯。

## 建置與資格

新的 default build 只建置跨平台、無第三方依賴的 memory prototype。Windows PAS core、gRPC 與舊 GTest 明確 opt-in；新的 CMake 不參照移除的 research、舊 provenance generated header 或既存 main80 objects。

gRPC overlay 不會被默默移除或改成 stock。`vcpkg.json` 延用來源 baseline，但把依賴分成 optional features。Windows fresh build、backend ABI/服務版本與裝置能力仍需驗證。

歷史 cost NOT_READY、live UNQUALIFIED、STOP 與分數只屬於舊工作；新分支不重寫、不繼承為通過，不自動開遊戲。沒有建立合併到 main 的 PR，也沒有改預設分支。
