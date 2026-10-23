# Rhythm Chart Memory — Phigros / 世界計畫研究分支

**方向：譜面／操作記憶驅動，音訊同步，視覺只做定位、校正與異常停止。**

這是 `feature/chart-driven-multigame` 的精簡起點，不是已完成的雙遊戲自動遊玩器。來源為 `main` 的 `2d479e17fa1566c089dce019cbb11eb883ba47fd`；原純視覺程式、實驗、分數與歷史仍保留在 main，沒有覆寫或合併。

## 已有與尚未完成

| 範圍 | 狀態 |
| --- | --- |
| 舊 PAS clock、LatestFrame、ContactScheduler、觸控抽象、座標映射 | 原檔保留 |
| 舊 ADB、gRPC 畫面／觸控、proto、自訂 gRPC patch | 原檔保留；Windows optional build，本輪未建置 |
| 跨遊戲 resolved-contact memory、身份欄位、接點生命週期／容量檢查 | 已有可執行的離線骨架 |
| 仿射時鐘映射、輸入提前量、同步租約有效性檢查 | 已有純軟體模型；沒有自動估計同步或漂移 |
| Phigros / Project Sekai dry-run | 同一組合成接點，以兩種 game ID 驗證；不是兩套遊戲 adapter |
| 原生譜面 parser、音訊擷取／對齊、遊戲幾何 adapter、planner→PAS bridge | 尚未實作 |
| 實機遊玩、AP／分數、端到端延遲與 Windows backend 重建 | 未測試；不能宣稱支援 |

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

`RHYTHM_BUILD_PAS_CORE=ON` 建置保留的 scheduler / evidence / ADB libraries。gRPC backend 與舊測試需要對應 vcpkg features，例如：

```powershell
cmake -S . -B build-win -A x64 "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" "-DVCPKG_MANIFEST_FEATURES=legacy-grpc;legacy-tests" -DRHYTHM_BUILD_PAS_GRPC=ON -DRHYTHM_BUILD_PAS_TESTS=ON
cmake --build build-win --config Release
ctest --test-dir build-win -C Release --output-on-failure
```

此路徑是待驗證的重建設定，不代表本輪已建置成功。不能以標準 gRPC 靜默取代 `grpc/pas_windows_read_config.h` 與 patch API。這個分支沒有 live CLI；編出 library 也不等於已整合遊戲。
