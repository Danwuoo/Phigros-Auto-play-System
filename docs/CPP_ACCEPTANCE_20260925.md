# C++20 T0–T5 驗收紀錄（2026-09-25）

本頁只記錄 C++20 新執行檔與原生 Android Fixture 的證據。歷史 Python 結果沒有轉成新版本的 pass；原始日誌位於忽略提交的 `measurements/`。遊戲 assist 未啟用，也沒有向 Phigros 注入觸控。

合併前另經獨立審查，修正極小 frame 越界與合成閉環驗證缺口；修正後 Release／Debug／ASan 各 21/21 通過。下方 19 項為開發任務當時的紀錄，合併結論、原始資料複本與限制以 [合併驗收](CPP_MERGE_REVIEW_20260925.md) 為準。

## 環境與可重現性

- 主機：Windows，18 logical processors、34,037,383,168 bytes physical RAM；host monotonic clock 為 QPC，頻率 10 MHz。
- AVD：`emulator-5554`、Android 16 / SDK 36、x86_64，guest online processors 5、`MemTotal` 8,130,828 KiB；視窗／觸控 720×1280，gRPC 擷取 1280×720、`source_rotation=1`、RGB888 top-down。
- 原生 Capture Fixture：目標 `debug.pas.fixture_hz=48`，實際可見來源約 43.5–44.6 Hz；本機與 Android 已安裝 APK SHA-256 均為 `1059d59875c65840db07688ce091cc30a4c2053b7f393378f7905dbe5c375e6f`。
- 原生 Touch Fixture：本機與 Android 已安裝 APK SHA-256 均為 `b3bfaf4e4e876e7833c9af1bd6149b40cdc9fa46b67646d52ea045dde6705dc6`。
- CMake 4.2.1、MSVC 19.51.36256 / VC 14.51.36231、Windows SDK 10.0.26100、vcpkg baseline `93c50752b23e350ca6b9063a167f0a4cf8a3b3eb`。建置最多 4 個並行工作。Release／Debug 使用 VS BuildTools 18.9；ASan runtime 來自 Community 18.5 的 VC 14.50，詳見下方限制。

## Release 擷取正常基線

固定順序三批，各 2 秒暖機、60 秒正式窗口，gRPC payload、單程序 worker、無預覽與主機負載。每批原始 JSONL、manifest、summary 均在 `measurements/capture-cpp-baseline/normal-*`，`pas analyze campaign` 重算顯示 `all_valid=true`、`environment_consistent=true`、三筆原始 SHA-256 均吻合。以下各 p 值以批內樣本計算，不能把三個批次 p99 當成逐 frame 的合併 p99。

| 批次 | 來源 Hz | 不同可見 frame | 來源跟隨 | arrival p95 / p99 ms | host residency p99 ms | 最大無圖空窗 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| normal-1 | 44.57 | 2,673 | 99.94% | 47.70 / 62.93 | 1.91 | 84.37 |
| normal-2 | 44.04 | 2,640 | 99.86% | 46.77 / 62.51 | 2.29 | 79.59 |
| normal-3 | 43.52 | 2,609 | 99.92% | 47.74 / 60.93 | 2.52 | 82.62 |

40–57 Hz 是正常來源條件，並非性能 pass。p95／p99、最大無圖空窗、來源跟隨與失敗容許值仍待使用者依本基線決定；在門檻確認前，`performance_pass` 保持 null。來源絕對年齡未知，不能把 host residency 誤讀為相機至畫面的總延遲。

## 觸控與其他功能

- `measurements/touch-cpp-full-run3/`：Tap、Hold、Move、Flick、交錯雙指、同時雙指各 30 筆，共 180/180 可見逐指路徑通過；預定 up 前取消 30/30 通過。RPC call p95/p99 為 1.52/1.68 ms，排程開始誤差 p95/p99 為 14.91/15.74 ms。能力測試採 200 ms down 遲到窗口，原始誤差仍完整保存；此窗口不改變一般 runtime 的預設 30 ms。
- `measurements/touch-cpp-batch/`：同一 gRPC event 的雙指 30/30 通過，均觀察到同時兩指 active；RPC call p95/p99 為 0.83/1.79 ms。`touch-cpp-edge/` 四角各一次通過。`touch-cpp-disconnect/` 本機 channel 故障後 fresh-channel rescue 的畫面零接觸已通過；外部網路故障與真 RPC timeout 未驗證。
- Release、Debug、嚴格 ASan 各 19/19 CTest 通過；ASan 測試時未設定 `ASAN_OPTIONS=allow_user_poisoning=0`。合成 pixel→辨識→追蹤→預測→排程→假觸控→效果閉環 30/30 命中，0 誤觸。`pas.exe` 在移除 Python PATH 後仍可執行。`run observe` 的 fake 與 gRPC 模式、D3D11 預覽、capture-first Session、RGBA payload、ADB PNG、顯式 MMAP 診斷、schema 1→2 migration 均已實行。
- 本機 Emulator payload 的 row order 為 top-down；顯式 `bottom-up` 加原生 Fixture 驗證時 0 張可解碼並使命令失敗，沒有把倒置畫面當成有效擷取。`assist`、退休的 process capture，以及未標明診斷的 MMAP 均在 CLI 入口拒絕。
- 17 個可取得的歷史 `capture.jsonl` 以 C++ 重算，逐批核對舊 `stdout.json` 中共同的 capture/consumer 數、窗口起訖，以及 arrival／host residency／no-frame-gap 的 n、p95、p99、max，全部一致（浮點容差 0.000001）。其中 `normal-1-T` 的 capture/consumer 均 1,267、不同可見計數 1,264、來源 21.104888 Hz、arrival p95 97.277275 ms、host residency p99 27.007554 ms、SHA-256 與舊分析相同。歷史 `pause500-T` 首秒 25 筆 relative lag 亦與舊分析一致。
- 舊版 Capture Fixture APK `2f50b989c6ed48bfb804d3f5a9c6a289db49b6325a813608a373fd7358f25e23` 在同一 AVD 中依序由 Python、C++ 各量 30 秒；C++ 的 `--fixture-schema legacy` 同時驗證裝置 APK hash 與舊像素計數。Python 來源 53.21 Hz、arrival p95/p99 38.3/52.1 ms、host residency p99 7.5 ms；C++ 來源 56.47 Hz、arrival p95/p99 36.3/48.9 ms、host residency p99 2.1 ms。來源率不同且量測非同時，差異不能全部歸因於 host 語言。原始檔在 `measurements/capture-legacy-ab/`。

## 壓力與穩定性

`measurements/capture-cpp-stress-stability/` 依預寫 `campaign-plan.json` 執行 normal、slow50、recover100、pause500、pause500-guard100、host-load、stability，後者正式窗口 600.003 秒。`pas analyze campaign` 重新計算七批，`all_valid=true`、`environment_consistent=true`，七筆 SHA-256 均吻合。

| 情境 | 擷取 / 消費 frame | 來源 Hz | 跟隨 | host residency p99 ms | 最大無圖空窗 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| slow50 | 1,331 / 483 | 44.32 | 36% | 56.7 | 80.9 |
| recover100 | 1,310 / 788 | 43.70 | 60% | 37.8 | 83.9 |
| pause500 | 1,274 / 1,273 | 42.91 | 99% | 2.7 | 513.6 |
| pause500-guard100 | 1,266 / 1,265 | 42.99 | 98% | 2.8 | 528.0 |
| host-load | 1,267 / 1,266 | 42.17 | 100% | 4.4 | 74.1 |
| stability | 25,767 / 25,766 | 42.94 | 99.97% | 2.7 | 98.8 |

無 guard 的 500 ms 停頓後第一張相對 lag 為 502.7 ms；guard100 為 1.7 ms、恢復首秒最大 26.7 ms。這是來源與主機間的相對時差，不是來源絕對年齡。慢消費者條件下較低的跟隨率是預期丟舊 frame 行為，不與正常條件共用性能門檻。

## 尚待結案與限制

- 正常批次的性能數值門檻待使用者確認，現階段只宣稱量測與資料有效。
- BuildTools VC 14.51 未安裝 ASan runtime/header；正式 `windows-asan` preset 使用 Community VC 14.50 的 ASan 元件搭配 14.51 編譯。原先三個 gRPC loopback 的 `use-after-poison`，以 ASan `poison_history_size=1000` 追到 Abseil `Cord::InlineData::poison_this`：預編譯 vcpkg 依賴沒有 ASan 插樁，但專案翻譯單元看見 `__SANITIZE_ADDRESS__` 後啟用不同的 Cord poison／destructor 路徑。preset 現在透過 `/FIcmake/asan_unsanitized_dependencies.h` 讓 Abseil 標頭與二進位依賴一致，保留 `/fsanitize=address` 對專案程式碼的插樁；未關閉 ASan 的手動 poison 檢查，正式 preset 19/19 通過。同版 Community VC 14.50 編譯器／ASan runtime 的獨立建置亦 19/19 通過。vcpkg 二進位依賴本身仍未受 ASan 插樁；若要檢查其內部記憶體錯誤，需以 ASan 重建依賴。
- 原生 Fixture 證明觸控後端能力，不證明 Phigros 遊玩可用。MMAP 沒有 producer 同步證據，仍限診斷。WGC／DXGI／scrcpy 是 T6–T7 候選，未納入本次選型。
