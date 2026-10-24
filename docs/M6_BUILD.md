# M6：Windows／patched gRPC 獨立重建紀錄

2026-10-23，**獨立 Windows Release build 與 6／6 CTest 已通過**。vcpkg 正常解析並完成 `x64-windows` Debug／Release 依賴；保留 PAS backend 已重新編譯，offline smoke 實際連結及呼叫自訂 patch symbol。這份紀錄的驗證沒有建立 emulator channel、讀取裝置或送觸控；裝置 runtime 與輸入資格另行驗證。

## 本次範圍與來源

- checkout：`feature/chart-driven-multigame`，基礎 HEAD `3be959e79ae14e6f43e64ec653b349830e217059`；工作樹另有里程碑開發變更。本次 M6 保留 retained PAS sources、proto、overlay；project CMake 另加入 offline patch-smoke target，實際驗證靜態 library 的 patch symbol 連結與 manifest。
- manifest baseline：`93c50752b23e350ca6b9063a167f0a4cf8a3b3eb`；features `legacy-grpc;legacy-tests`。
- gRPC：repository overlay `third_party/vcpkg-overlay/grpc`，版本 `1.81.1#2`；保留自訂 Windows read-chunk patch。`src/grpc_transport.cpp` 依賴 `pas_windows_read_config.h` 與 `grpc_pas_windows_read_patch_version()`，不能換成 stock gRPC 當作相容。
- `docs/EXTRACTION.json` 列出的 14 個 retained Git blobs 均與基線一致；`proto/` 與 `third_party/` 對 extraction baseline 的 Git diff 為空。工作區檔案 SHA256、overlay 每檔 SHA256 與版本指紋另存本機 `work/phigros-m6-build-provenance.json`，不含 token 或音樂資產。
- 本文件的 source build／CTest 範圍只建離線 library／FakeTouch 核心測試，測試不呼叫裝置、不送觸控。另行完成的 fixture 安裝準備列於下方，與 runtime 驗證分開記錄。

## 獨立環境

| 項目 | 本次設定 |
|---|---|
| generator | Visual Studio 18 2026，x64 |
| VS instance | `C:/Program Files/Microsoft Visual Studio/18/Community` |
| MSVC toolset | `14.50.35717`，v145；C／C++ compiler `19.50.35729.0` |
| CMake | 4.2.1 |
| vcpkg | `2026-02-21-82f3841f4f91e8389b7c7ddf61e6733683b2f67b` |
| target／host triplet | 均為 `x64-windows`，保留正常 Debug＋Release ABI |
| 並行度 | 已啟動的 vcpkg build 保持 `VCPKG_MAX_CONCURRENCY=2`；後續 project build 使用 4 jobs |
| CMake build | 工作區 `work/phigros-grpc-build` |
| vcpkg install | 工作區 `work/phigros-vcpkg-install` |
| vcpkg buildtrees | 工作區 `work/vb`，避免 Windows 260 字元來源路徑限制 |

`x64-windows` 使用動態 CRT；gRPC overlay 要求靜態 gRPC library，其他相依套件仍由 triplet／port 自行決定 linkage。測試執行檔所需 DLL 仍須透過正常 CMake／vcpkg app-local 機制提供。

## 重現命令

以下從 repository root 執行；`$taskRoot` 應設成這次工作區絕對路徑。命令只改 process-local 環境並於結束時還原。

```powershell
$taskRoot = 'C:/Users/wurre/Documents/Codex/2026-10-23/new-chat'
$taskVs = 'C:/Program Files/Microsoft Visual Studio/18/Community'
$taskPriorVs = $env:VCPKG_VISUAL_STUDIO_PATH
$taskPriorJobs = $env:VCPKG_MAX_CONCURRENCY
try {
    $env:VCPKG_VISUAL_STUDIO_PATH = $taskVs
    $env:VCPKG_MAX_CONCURRENCY = '2'
    cmake -S . -B "$taskRoot/work/phigros-grpc-build" `
        -G 'Visual Studio 18 2026' -A x64 `
        "-DCMAKE_GENERATOR_INSTANCE=$taskVs" `
        "-DCMAKE_TOOLCHAIN_FILE=$taskVs/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" `
        -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_HOST_TRIPLET=x64-windows `
        '-DVCPKG_MANIFEST_FEATURES=legacy-grpc;legacy-tests' `
        "-DVCPKG_INSTALLED_DIR=$taskRoot/work/phigros-vcpkg-install" `
        "-DVCPKG_INSTALL_OPTIONS=--no-downloads;--x-buildtrees-root=$taskRoot/work/vb" `
        -DRHYTHM_BUILD_PAS_GRPC=ON -DRHYTHM_BUILD_PAS_TESTS=ON
    if ($LASTEXITCODE -ne 0) { throw 'M6 configure failed' }
    cmake --build "$taskRoot/work/phigros-grpc-build" --config Release --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'M6 build failed' }
    ctest --test-dir "$taskRoot/work/phigros-grpc-build" -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'M6 CTest failed' }
} finally {
    $env:VCPKG_VISUAL_STUDIO_PATH = $taskPriorVs
    $env:VCPKG_MAX_CONCURRENCY = $taskPriorJobs
}
```

`--no-downloads` 使用此主機已有 source archives；乾淨主機須先取得 baseline registry 與對應來源。它不保證禁止 vcpkg 的 registry metadata 更新。本次起始曾取得 registry HEAD metadata，正常 ABI 解析後沒有手工解壓或混用舊 binary cache。既有不同 ABI 的 patched archive 不能當成本次建置成功的證據。

進行中曾評估把 gRPC 改為 4 jobs；本次使用的 vcpkg helper 每次 configure 都會刪除該 port 的 Debug／Release build dirs，重跑會損失已編譯的 gRPC objects。因此既有 Ninja 維持 `-j2` 到依賴完成，再以 4 jobs 建置 project。依賴與 binary-cache submission 均已成功；隨後同 flags 的 final configure 確認所有 13 套件均已安裝，沒有重建或混用另一 ABI。

## 完成結果與證據

- 已完成正常 vcpkg 安裝與 post-build validation：abseil `20260107.1#3`、c-ares `1.34.8`、grpc `[codegen,core] 1.81.1#2`、GTest `1.18.0`、nlohmann-json `3.12.0#2`、OpenSSL `3.6.4#1`、protobuf `[core,libprotoc] 6.33.4#2`、re2 `2025-11-05`、utf8-range `6.33.4`、zlib `1.3.2#2`，以及 3 個 vcpkg CMake helpers。套件版本與 ABI 均記入本機 provenance；所有依賴保持正常 `x64-windows` Debug／Release，project 驗證則使用 Release。
- 首次 gRPC build 在 Ninja `Stat(...)` 階段因原始檔完整路徑超過 260 字元失敗，未進入 C++ 編譯；失敗紀錄保留於 `work/phigros-vcpkg-install/vcpkg/blds/grpc/install-x64-windows-dbg-err.log`。改用 vcpkg `--x-buildtrees-root` 縮短中間路徑後重試，沒有修改 source／patch／triplet；gRPC ABI 仍為 `432e798d9ab5eef7f2d0f8a20ce5ff6414f7ac8104a8428c72ce6cdd9715c544`。vcpkg 正常辨識先前 10 個已安裝套件並沿用。
- 先前依賴的 configure／build／install logs 與 `*.vcpkg_abi_info.txt` 保留於 `work/phigros-vcpkg-install/vcpkg/blds/<port>/`；重試後新依賴則於 `work/vb/<port>/`。安裝狀態於 `work/phigros-vcpkg-install/vcpkg/status`，configure transcript 於 `work/phigros-m6-configure.log`。
- Release 已完成 `pas_core`、`pas_adb`、`pas_proto`、`pas_emulator`、`pas_core_tests`、`grpc_patch_smoke`、`touch_probe` 與預設離線 targets。第三方 gRPC TLS header 出現 C4996 deprecated `IdentityKeyCertPair` 警告；沒有編譯或連結錯誤，沒有修改保留來源以消除警告。
- 實際 6／6 CTest 通過：`rhythm_memory_contracts`（28 checks）、`phigros_synthetic_dry_run`、`sekai_synthetic_dry_run`、`touch_fixture_ledger_contracts`（19 checks）、`grpc_patch_runtime_contract`、`retained_pas_core`（12 個 GTest、6 個 suites）。本設定沒有開啟 chart import、supervisor、bridge、session runner 或音訊選項；其他整合 build 的測試數量另行記錄。
- `pas_emulator` 是 static library；另以 `grpc_patch_runtime_contract` 實際連結並呼叫 runtime，確認 package `1.81.1`、patch revision `2`、channel argument `pas.grpc.windows_read_chunk_bytes`，8／64／256 KiB 分別回傳 8192／65536／262144 bytes。直接呼叫 exported sizing helper 時，`0`、`-1`、32768、262145 bytes 均回退 8192；PAS wrapper 拒絕 32 KiB。manifest 的 `client_version` 實際為 `54.0.0`，此欄是 runtime 回報值，套件版號另由 `package_version:1.81.1` 記錄。smoke 沒有建立 channel。
- `touch_probe plan` 離線產生 15 份計畫：single／capacity／batch／cancel／transport-loss 各搭配 capacity 2／5／10、1280×720。只驗證計畫輸出，沒有執行 `run`、`--inspect-only`、觸控或容量資格測試；plan 的幾何也不等於已驗證裝置 profile。
- CTest transcript／JUnit 位於 `work/phigros-m6-ctest.log`／`work/phigros-m6-ctest.xml`；個別 retained GTest XML 為 `work/phigros-m6-core-tests.xml`。實際 runtime manifests 為 `work/phigros-m6-grpc-runtime-manifests.jsonl`，離線 plans 為 `work/phigros-m6-offline-touch-plans.json`。`work/phigros-m6-build-provenance.json` 已完成，包含 13 套件 ABI、14 個保留 blob 再核對、project source hashes、19 個 Release EXE／LIB／DLL hashes 與驗證 log hashes；final configure／build transcript 分別為 `work/phigros-m6-configure-final.log`／`work/phigros-m6-build.log`。這些 `work/` 路徑相對於工作區根目錄，而非 repository root。
- **這份 baseline build 驗證**沒有建立 emulator channel；fixture 實際輸入、觸控容量與延遲、校正及遊戲分數均為 **NOT RUN**。後續其他 build 的只讀 shadow 或裝置操作不歸入此測試結果。音訊工作已依使用者指示暫停。

## Fixture 裝置準備（獨立紀錄）

已將 evidence v3 APK 安裝至明確選定的 `phigros`／`emulator-5554`，package `org.pas.touchfixture.evidence.cpp`。安裝前沒有同 package；安裝後 device `base.apk` SHA256 再核對為 `d4ae58c7f9561b4262791a1711e2688db233ec054b10400e88b5e60fccfc34a2`，與建置產物一致。

該安裝紀錄建立當下僅完成安裝：**未 launch fixture、未 enable evidence property、未送 input**。實際操作與雜湊見 [安裝紀錄](../../local-assets/phigros-4.0.1/m6-touch-fixtures/install-record.json)；它不代表後續 runtime 工作的最新狀態。先前 fixture build manifest 的 `installed_or_launched:false` 保留原值，描述該建置紀錄建立當下的狀態；不能把它改成後來安裝步驟的歷史。
