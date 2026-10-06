# Portable verification results — 2026-10-06

**Release、Debug、ASan+UBSan：每配置 617 checks 通過，另有 1 個 overdue-Flick case。**
這是 473 個 core/current checks 加 144 個 workload-diagnosis checks，三配置重跑
同一組契約，不是 1851 個不同案例。沒有 Windows、成本或 physical/live 升格。

| Configuration | Core/current checks | Workload diagnosis | Flick | Source integrity |
|---|---:|---:|---|---|
| GCC 14.2 Release `-O2 -DNDEBUG` | 473/473 | 144/144 | Pass | Before/after identical |
| GCC 14.2 Debug `-O0 -g1 -D_GLIBCXX_ASSERTIONS` | 473/473 | 144/144 | Pass | Before/after identical |
| GCC 14.2 `-O1 -g1 -fsanitize=address,undefined` | 473/473 | 144/144 | Pass | Before/after identical |

最終全部 stderr 為空；ASan/UBSan 無 findings。**LeakSanitizer 未驗證**：本容器
ptrace 使原 `detect_leaks=1` 啟動 fatal，原始失敗保存在 `sanitizers-01/`。
最終設定 `detect_leaks=0` 僅關閉獨立 leak scanner，不關閉 ASan 或 UBSan。

## 實際來源與環境

- Base commit：`e9e305e3aba13f05a786c74540419428610b2f76`
- Linux x86_64，GCC 14.2.0，C++20；沒有安裝 CMake、GTest 或 Windows SDK。
- Core clock shim：Linux `steady_clock` adapter，business clock 用 FakeClock。
- Core/current/owner/scheduler 實作直接編譯；沒有策略副本或額外成功 stub。
- `src/game.cpp` 包含父工作包的 diagnostic-only schema4/typed proof 變更，
  SHA256 `058c56606d0ce27937f763a57c74f5d1066a586abe2e074778b3b5910ba69709`。
  其他實際 closure 來源與 header 的完整 SHA 在每組 `source-before.sha256`。
- Patched nlohmann header SHA256
  `f2e1f3cac4e48fa851d6986291e4bd5b384d87f92c175d8b126cbd91580289cc`。

## 最終可用證據

- `evidence/final-release/`、`final-debug/`、`final-sanitizers/`：407 JSON checks，
  原 scheduler probe 13 cases/66 checks，另 1 Flick case；所有 test exit 0。
- `evidence/workload-release/`、`workload-debug/`、`workload-sanitizers/`：
  各 144 checks、所有 test exit 0；重用 archive 前驗證完整 closure，並核
  workload sources 與 archive before/after SHA。
- `evidence/summary.json`：機器可讀摘要；`MANIFEST.sha256`：本 lane 全部
  文字／JSON／source 證據檔清單。編譯執行檔僅留 ignored `out/`，不放 Git。

## 否定結果與限制

1. 原 Hold synthetic scene 在 A/B 完整 Runtime 都無動作；21 點 head phase
   sweep 只有 503 共同 entry，沒有 robust common interval。stationary-body
   控制亦保留 A 提早 Up、B 仍 active 的差異。不能拿單點陽性開啟 A/A 或 ABBA。
2. `normal_hold_tail_completion_proved=false`、`new_cost_scene_admitted=false`、
   `windows_cost_qualification=false`；舊 STOP 沒有重新開啟。
3. `release-01/` 保存初版 workload 的 2 個陽性 assertion 失敗與測試期間
   serializer 變更的 source mismatch。它不列入最終通過分母，也不刪除或覆寫。
4. `release-02/` 與 `debug-01/` 是新增 schema4 independent controls 之前的
   中間通過結果；最後以 `final-*` 為準。
5. 原 Windows GTest 186 項、native QPC、WIC/PSAPI chain executable、
   gRPC/capture/touch、真圖 full replay、physical judgment、完整曲目均未在
   此 lane 執行。未重跑原 3938 assertion frozen oracle；沒有把原 36 fail 改成 pass。
6. `CMakeLists.txt` 提供獨立 Linux 路線，但 CMake configure/CTest 未執行；
   實際通過證據來自保存的 Bash/GCC 命令及退出狀態。
