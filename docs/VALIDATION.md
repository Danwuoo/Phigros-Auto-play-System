# 本輪驗證：2026-10-23

## 已執行

環境：Linux；GNU C++ 14.2.0；CMake 3.31.6。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror"
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/rhythm_memory_tests
```

結果：default targets 成功編譯；3/3 CTest entries 通過。`rhythm_memory_contracts` 中的 28 項具名檢查通過，另外兩項是 Phigros / Project Sekai ID 的合成 dry-run。

涵蓋：身份必填／hash 字串格式、接點生命週期、重複 ID、時間順序、NaN／越界座標、pixel 邊界、容量超限、同時 release/reuse、仿射映射、rate／overflow／input lead、取整坍縮、租約到期邊界、epoch／geometry 不符、不確定度與預設 disarmed。

注意：兩種 game ID 使用同一組合成接點，不是兩個原生 adapter 的測試；格式合法的 hash 不代表真的核對過資產 bytes。測試程式不使用會在 Release 被移除的 `assert`。

## 明確未執行／尚未具備

- Windows/MSVC、保留 PAS source 的 fresh build、optional gRPC overlay build、舊 core GTest：NOT RUN。
- 原生譜面 parsing、真實譜面 golden test、audio capture/實際同步、native game geometry、PAS bridge：NOT IMPLEMENTED。
- 手機／emulator 操作、live touch、長按／滑條／flick 判定、真實延遲或分數測試：NOT RUN。
- GitHub Actions 遠端 CI：本輪沒有設定或宣稱已執行。

本報告只對新的 portable contracts 有效。不得據此宣稱原系統已重建、兩款遊戲已可玩、AP、零 Miss、固定毫秒精度或 live qualification。
