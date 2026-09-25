# C++20 遷移與驗收矩陣

2026-09-25。本表區分「已有 C++ 程式碼」與「已用新原始證據驗收」。本輪 Release／Debug 各 19/19、原生 Touch Fixture 180+30、雙指 batch 30、三批正常擷取與七批壓力／穩定性都已取得新證據；ASan 完整嚴格驗收及正常性能門檻仍待結案。完整數字與限制見 [C++ 驗收紀錄](CPP_ACCEPTANCE_20260925.md)。來源凍結與雜湊見 [來源清單](CPP_SOURCE_SNAPSHOT_MANIFEST.json)，可取得的歷史 raw 與缺失項見 [證據索引](CPP_EVIDENCE_INDEX.json)。

## 正式元件

| 舊功能 | C++20 位置 | 驗收依據／待確認 |
| --- | --- | --- |
| Clock、Frame、latest、型別契約 | `include/pas/core.hpp`、`src/core.cpp` | QPC；三個固定物理 slot、一個邏輯最新 frame；單元測試與慢消費者測試 |
| 設定與 schema | `include/pas/config.hpp`、`src/config.cpp` | schema 2 `execution=thread`；`config migrate` 輸出新檔；舊 process 不默默轉換 |
| ADB 與 PNG 診斷 | `src/adb.cpp` | Win32 有界子程序、WIC RGB；AVD PNG 2 秒短測通過，約 3 Hz，只供診斷 |
| gRPC payload／MMAP | `src/emulator.cpp` | 官方 proto C++ codegen；loopback、實機 RGB/RGBA、MMAP 短測與 3×60 秒 payload 基線；MMAP 僅診斷，未證明 producer 同步 |
| 單程序 observe／Session | `apps/pas/main.cpp` | fake／實機 observe、capture-first Session 與 12 ms 健康 probe race 實測；assist 拒絕 |
| 診斷預覽 | `src/preview.cpp` | Win32／D3D11，實機 2 秒約 10 次 draw；需人工視覺外觀 QA |
| 簡單目標辨識／追蹤／預測 | `src/core.cpp` | 只讀 pixels；合成閉環與預測時序測試 |
| 排程與假觸控 | `src/core.cpp` | gate／plan 到期、單調 birth ID、stop 線性化三項 P1 回歸 |
| gRPC 多指觸控 | `src/emulator.cpp` | RPC 成功只代表呼叫返回；接觸效果由 native Fixture pixels 驗證 |
| Capture／Touch Fixture | `fixtures/android/` | NDK x86_64 NativeActivity；APK hash 一致；六類逐指 180、取消 30、batch 30 通過 |
| JSONL／pause 分析 | `src/analysis.cpp` | 有界 streaming、半開窗口；舊 normal-1-T 與 pause500-T differential 一致；新 campaign 七批 raw hash 一致 |
| 量測與正式日誌 | `src/bench.cpp`、`src/journal.cpp` | bounded writer；3×60 秒 Release 正常基線與 600 秒穩定性完成；性能門檻待使用者確認 |

## 11 個既有 CLI 用途

| 舊子命令 | C++ 子命令 | 決策 |
| --- | --- | --- |
| `run` | `run` | schema 2 profile；observe only；assist 明確拒絕 |
| `touch-bench` | `touch-bench` | native Fixture v2 每指路徑及預定 up 前取消；六類與取消各至少 30 組才宣稱完整能力 |
| `touch-batch-bench` | `touch-batch-bench` | 同一 gRPC event 兩指，至少 30 組才宣稱能力 |
| `touch-disconnect-smoke` | `touch-disconnect-smoke` | 本機通道故障與 fresh-channel rescue，單次 smoke；真實外部網路故障另列限制 |
| `probe` | `probe` | ADB 裝置盤點 |
| `synthetic` | `synthetic` | virtual monotonic pixels → fake touch → pixels 閉環 |
| `schedule-bench` | `schedule-bench` | QPC 主機喚醒分布 |
| `capture-bench` | `capture-bench` | gRPC payload、診斷 ADB PNG、顯式診斷 MMAP；新 `--output-dir` 保存 raw/manifest |
| `start-session` | `start-session` | 先擷取後啟動 package；不建立 input backend |
| `offline-capture-bench` | `offline-capture-bench` | native fake source 單程序測試，與舊 process 數據分開 |
| `buffer-bench` | `buffer-bench` | 固定 slot 最新 frame 慢消費者測試 |

`config migrate`、`capture-campaign` 與 `analyze capture|pause|campaign` 是新增的 C++ 工具命令。`capture-bench` 保留明確 gRPC endpoint/token 檔、RGB/RGBA、row order、RGB 容量與 `--load`；token 不寫入 manifest。`--capture-execution process` 與 `--child-load` 因單程序決策退役，必須明確拒絕。舊 `--no-log-cost`／`--no-consumer-rss` 不再適用：新擷取 callback 只進有界 Journal，消費者日誌在另一個 worker 寫入，資源報告改為帶取樣時間括號的單一程序 CPU／RSS。舊 `--samples`、`--log` 的固定筆數／任意檔路徑改由正式窗口與必填 `--output-dir` 取代。舊 browser Fixture 的 `--fixture-x/y/scale` 不適用於原生 Fixture；若要比較舊 browser raw，仍用離線 `analyze`。未列出的舊 CLI flag 應視為待逐項審核，不宣稱完全兼容。

## 11 個歷史 scripts 的用途

| 凍結腳本 | C++ 對應或處置 |
| --- | --- |
| `analyze_capture_pause.py` | `pas analyze pause` |
| `build_capture_fixture.py` | `fixtures/android/CMakeLists.txt` + `cmake/PackageFixture.cmake` |
| `build_touch_fixture.py` | 同上，touch target |
| `offline_capture_comparison.py` | `pas offline-capture-bench`；process/thread 比較因正式架構變更退役 |
| `profile_grpc_stages.py` | `capture.jsonl` 分段時戳與 `pas analyze capture`；更細的 protobuf-only profile 待驗收 |
| `recompute_offline_capture.py` | `pas analyze capture`；舊 process raw 的欄位相容需 differential |
| `summarize_unified_capture.py` | `pas analyze capture` 單 run，`pas analyze campaign` 跨 run 重算與 hash 驗證；待實測驗收 |
| `unified_capture_bench.py` | `pas capture-campaign` 預先寫入固定順序、逐 run manifest/raw/summary；待實測驗收 |
| `verify_capture_faults.py` | C++ loopback／fault 測試；真實 invalid／inactive 待驗收 |
| `verify_probe_race.py` | C++ 健康 probe 返回後重檢；回歸測試待補 |
| `verify_scheduler_acceptance.py` | `tests/core_tests.cpp` 三項 P1 回歸 |

## 8 個歷史測試檔的責任

| 凍結測試 | C++ 對應／處置 | 新證據狀態 |
| --- | --- | --- |
| `test_capture_bench.py` | `pas offline-capture-bench`、`pas buffer-bench`、`pas capture-bench`；新 C++ JSONL 窗口驗證 | Release 實測通過 |
| `test_capture_grpc.py` | `tests/grpc_loopback_tests.cpp`：真 RPC、截斷 payload、凍結 timestamp、取消；實機 payload／MMAP 診斷 | Release／Debug 通過；嚴格 ASan 第三方 poison 報告待解 |
| `test_capture_process.py` | process IPC 按單程序決策退役；有界 `LatestFrame`、worker 取消與 ownership 改由 C++ 測試／observe 驗證 | CTest 與 observe 通過 |
| `test_input_grpc_server.py` | `pas touch-bench`、`pas touch-batch-bench`、`pas touch-disconnect-smoke` 加 native pixels 逐指驗證 | 正式樣本通過 |
| `test_pipeline.py` | `tests/core_tests.cpp` 的 pixels→追蹤→預測與 `pas synthetic` 端到端假觸控 | 30/30 合成命中 |
| `test_process_bench_boundaries.py` | process CPU／RSS 取樣退役；半開時間窗口、跨界 frame 排除與新 JSONL 重算 | CTest 與正式窗口重算通過 |
| `test_runtime_core.py` | schema 2 設定、gate／stop、`run observe`、Session／probe race 實機與 fake profile | 實機與 fake 短測通過 |
| `test_unified_capture_analysis.py` | `pas analyze capture|pause|campaign`，歷史 JSONL golden 差分與新 campaign hash 驗證 | 舊 normal/pause 差分與新十批 hash 通過 |

## 尚未授權的性能數值門檻

40–57 Hz 是正常來源區間，並非自動通過。待 5 vCPU／8 GB AVD 的第一批 C++ Release 基線完成後，將 p95／p99、最大無圖空窗、來源跟隨比例及失敗容許值連同測試環境、樣本數與原始分布交由使用者決定。不能沿用舊 57–63 Hz pass 欄位。
