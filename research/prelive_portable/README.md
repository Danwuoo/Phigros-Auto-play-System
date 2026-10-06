# Linux portable cold-contract lane

這是 **Linux/GCC C++20 的離線契約驗證**，不是 Windows/MSVC、原生 QPC、裝置、
gRPC、遊戲採納或成本資格。`runtime_cost_gate` 及 physical gate 仍為 `NOT_READY`。
不覆寫 frozen oracle、歷史失敗、STOP、原圖或既有 Windows receipts。

## 建置邊界

- 正式 `src/core.cpp`、`game.cpp`、`game_motion.cpp`、`game_tracking.cpp`、
  `game_session.cpp` 直接編譯；owner、scheduler 與 current-rails 策略沒有複製或改寫。
- 原 `research/prelive_current/runtime.hpp` 直接由測試 include。完整
  `chain.cpp` 的 WIC/PSAPI/Windows 命令入口 **沒有在本 lane 建置**。
- 唯一平台 adapter 是 `shim/windows.h` 的兩個 QPC API 名稱：映到 Linux
  `steady_clock`，只供 HostClock 診斷。決策時間測試使用 `FakeClock`。
  這個時鐘不等同原生 QPC，也不提供 Windows 延遲／成本證據。
- 以 function/data sections 與 linker garbage collection 刪去未引用的離線分析
  函式；沒有替 `distribution`、SHA、capture、touch transport 等製作假的成功實作。
- 不連結 adb、emulator、grpc、manual-session、模型或 Windows device objects。
- 此工作樹的 `game.cpp` 只新增父工作包的 `decision_json` typed
  `current_contact` 輸出與 schema 4；不是 byte-identical 的完整 e9e305e。
  測試前後 SHA 列明實際編譯來源，既有 owner/scheduler 決策不變。

## 依賴

只使用環境已安裝的 GCC 14.2、binutils、Bash。未安裝或執行下載的 binary。
父工作包讀取官方 nlohmann-json 3.12.0 單一 header，再使用 repo **原有**
`prepare_json_dependency.sh` 與兩份 vcpkg patch 生成獨立 include root：

- 官方 upstream SHA256：`aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63`
- 本 lane 必須匹配的 patched header SHA256：
  `f2e1f3cac4e48fa851d6986291e4bd5b384d87f92c175d8b126cbd91580289cc`
- 本次 include root：`/workspace/shared/phigros-live-deps-20261006-patched`
- header 不複製進本次 Git deliverable；原版與 patch 許可沿 repo 原記錄。

## 重跑

從 repo 根目錄執行；每次輸出必須是不存在的新資料夾。腳本不下載依賴。

```bash
mkdir -p out
PAS_PORTABLE_SKIP_WORKLOAD=1 PAS_PORTABLE_DETECT_LEAKS=0 \
  bash research/prelive_portable/run.sh \
  /path/to/pinned/include release \
  research/prelive_portable/evidence/new-release
```

模式為 `release`、`debug`、`sanitizers`，後者使用 AddressSanitizer +
UndefinedBehaviorSanitizer。此雲端執行環境的 LeakSanitizer 因 ptrace 無法工作；
原 `sanitizers-01` fatal receipts 保留，故本次使用 `detect_leaks=0`。
**未驗證 memory leak freedom**，不可把此設定稱為 LSan 通過。

`PAS_PORTABLE_SKIP_WORKLOAD=1` 僅排除另個工作包正在修改的 workload tests。
定稿的工作負載陽性／陰性結果另行驗證，不把早期失敗隱藏成 pass。
取消該設定便會建置、執行 `cost_workload_tests.cpp`。

已定稿的 workload diagnosis 另以 `run_workload.sh` 重用三個已核 SHA 的 closure，
三配置各 144 checks 通過；結論仍是 `NOT_READY_no_robust_common_Hold_workload`。
這些檢查通過的是負結果與分母／release 契約，不是新成本情境取得資格。

附 `CMakeLists.txt` 提供相同 closure 與 `cost_workload_tests` target；本環境
沒有 CMake，故 **CMake configure/CTest 未執行**。本次實際建置均走 `run.sh`。
有 CMake 的 Linux 環境可設定 `-DPAS_JSON_INCLUDE=/path/to/pinned/include`；
sanitizer 可加 `-DPAS_PORTABLE_SANITIZERS=ON`。`run_case.sh` 保留每次新報告。

## 測試與驗收分層

原測試檔原樣編譯、沒有修 assertions：

| Suite | 每配置檢查數 |
|---|---:|
| Original owner prefix | 113 |
| Original front | 56 |
| Current rails | 68 |
| Own-receipt prefix | 113 |
| 本次 independent | 57 |
| 原 scheduler probe | 66（13 cases） |
| 原 overdue-Flick probe | 1 case（不虛構 assertion 數） |

每配置 JSON checks 407，加 scheduler 66 共 473 個報告內 checks；Flick
另列。三配置是同一組測試的三次執行，不是三倍的獨立行為覆蓋。
原測試的部分 JSON schema 名稱仍帶 `windows`，那是未改測試的歷史字串，
不能推翻本 lane 明確的 Linux 執行環境。

新 independent controls 涵蓋 1–5 接觸容量、128/129 pending plans、±1ns
expiry、fresh gate 不刷新舊 target、immutable Down prefix、same-contact Move、
terminal intent、未知 Down/Move/Up 不重試且保留釋放責任、七個 receipt command
欄位逐一竄改、無 accepted plan 的 receipt、missing/unknown final release、
64/65 backend event 容量、三個 frame lease 不被覆写、空白 pixels 的完整原
Runtime 診斷分母，以及 schema4 typed proof 與純 reason 字串可區分。

Body/tail、旋轉、五指及 terminal note retirement 由原 current/own-prefix
測試覆蓋。本 lane 不宣稱完成原本 Windows GTest 186 項、不重跑／修改
原 frozen 3938 assertion oracle，也不因此解鎖 live。

## 證據

`evidence/` 保存 stdout/stderr、compiler warnings、before/after source hashes、
exit codes、JSON assertions、依賴 source-only preparation 及實際編譯路徑。
`release-01` 同時保留最初 workload 陽性失敗與並行 serializer 修改造成的
source-hash mismatch；不是最終通過版本。最終結果見 `RESULTS.md`。
