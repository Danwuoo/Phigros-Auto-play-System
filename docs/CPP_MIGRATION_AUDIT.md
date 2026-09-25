# C++ 遷移盤點（2026-09-25）

本次為文件、程式與既有證據盤點，沒有重新執行模擬器測試。遷移決策與開發順序以 [遷移計畫](CPP_MIGRATION_PLAN.md) 為準；逐檔 SHA-256 見 [盤點清單](CPP_MIGRATION_INVENTORY.json)。

> 交接時狀態更新：原 `8bd4` 消失後，使用者從封存任務修補重建於 `C:/Users/wurre/Documents/Codex/2026-09-25/new-chat/outputs/recovered-8bd4`。本次比對原盤點hash，66／70檔相同，包含四個修正版程式及低負載報告；其餘四個是文件差異。可將已核對程式與報告納入來源快照，但原始量測164檔仍未恢復。詳見 [復原交接紀錄](CPP_RETEST_RECOVERY_NOTES.md) 與 [核對清單](CPP_RECOVERY_VERIFICATION.json)。

## 1. 來源與版本

- 桌面主目錄：`C:/Users/wurre/Desktop/Phigros-Auto-play-System`。
- 較低負載重測工作區：`C:/Users/wurre/.codex/worktrees/8bd4/Phigros-Auto-play-System`。
- 兩者 HEAD 都是 `b5da5c834d0b5b2343e83ef394697a8b235bd1be`，有未提交／未追蹤內容。不能僅 checkout HEAD 就開始移植。
- 盤點聯集共 70 個檔案：45 個 Python（26 個 src、11 個 scripts、8 個 tests），2 個 Java、1 個 HTML、3 個 JSON profile、2 個 Android Manifest、1 個 proto、13 個 Markdown（含根目錄 README／AGENTS）、1 個雜湊 txt、1 個 pyproject、1 個 gitignore。排除 measurements、IDE 設定及本次新增的遷移文件。
- 重測工作區相對桌面的必要程式增量：`src/pas/cli.py`、`tests/test_capture_bench.py`、`scripts/unified_capture_bench.py`、`scripts/summarize_unified_capture.py`。這四個檔案採重測版為移植參考。
- 增量包括：receiver pause 移到 collection lock 外、實際 ended_ns、可停用逐影格 RSS、slow50 場景、payload/MMAP 分開計數、批次完整性包含退出碼／像素驗證、舊日誌推算恢復時間明示。
- `docs/CAPTURE_IDLE_RETEST_20260925.md` 僅存在重測工作區。其他兩邊的文件有差異，需以桌面文件為主整合新增報告，不能直接用整個重測工作區覆蓋主目錄。AGENTS 原始 hash 差異的文字 diff 為空，屬行尾差異。

## 2. 程式對應

| 現有檔案／元件 | 現有責任 | C++ 遷移處置 |
| --- | --- | --- |
| `contracts.py`, `clock.py` | Frame、Observation、Track、HitIntent、TouchCommand／Receipt、主機／假時計 | 核心值型別與 Clock interface；QPC 時計；保留 produced unknown |
| `latest.py`, `capture.py` | 容量一緩衝、擷取 worker、ADB PNG 路徑、取消／錯誤 | 有界 buffer pool＋最新發布槽；原生 PNG 解碼；callback 不執行分析／同步寫檔 |
| `capture_grpc.py` | 裝置 discovery、認證、streamScreenshot、像素正規化、relative lag、MMAP | C++ gRPC client；payload 正式、MMAP diagnostic；逐項保留 fault 語義 |
| `capture_process.py` | Windows spawn、IPC、CRC、child 監督、資源快照 | 新正式 runtime 已選單程序，因此不照搬 spawn／IPC；取消、生命週期、buffer ownership、計數契約轉成 thread 版本；舊 process 留 legacy 對照 |
| `adb.py`, `session.py` | 盤點、capture-first 啟動、靜態／inactive 健康判斷 | C++ process launcher、Session state machine；新圖到達後重新核對健康 probe 結果 |
| `runtime.py`, `config.py` | observe、嚴格 profile、manifest、有界 journal、方向檢查 | 單程序 supervisor；schema migration；observe 不建立 input；assist 保持禁用 |
| `vision.py`, `synthetic.py`, `fixture.py` | 綠色目標、短期線性追蹤、撞線、合成世界、可見計數解碼 | C++ 確定性測試與簡單 detector；不是 Phigros 辨識；真值留在測試端 |
| `scheduler.py` | 舊 Tap 排程 | 保留語義測試與 adapter，避免另維護兩套 live owner |
| `action_planner.py`, `contact_scheduler.py` | 一般 down/move/up 計畫、revision／epoch、single owner | 移植並修正三個 P1，不能以 Python 輸出為錯誤行為 oracle |
| `input.py`, `input_grpc.py` | 座標映射、fake backend、sendTouch、unknown state、緊急釋放 | 單 touch owner，獨立 capture/input channels；失敗 down 不重試 |
| `capability.py`, `touch_bench.py` | Fixture 門控、能力報告、可見事件 checker | C++；新版 pointer 路徑驗證，舊能力不自動升格 |
| `preview.py` | Tk/Pillow 降頻預覽 | 依使用者介面決策移植；只讀診斷，不作決策輸入 |
| `telemetry.py` | JSONL、分布統計 | C++ 有界 writer 與可串流重算器；int64 時間戳不可經浮點 JSON 遺失精度 |
| `cli.py` | 11 個子命令及大量 bench 實作 | 拆分 apps／bench／analysis，不移植成巨型單檔 |
| `emulator_proto/emulator_controller_pb2.py` | 產生的 Python binding | 不手工翻譯；由既有 proto 產生 C++／gRPC binding |
| `__init__.py`（兩處）、`pyproject.toml` | Python 封裝與入口 | 留在 legacy；正式根目錄改 CMake／vcpkg |

## 3. 命令與工具涵蓋範圍

目前 CLI 11 個唯一子命令：`run`, `touch-bench`, `touch-batch-bench`, `touch-disconnect-smoke`, `probe`, `synthetic`, `schedule-bench`, `capture-bench`, `start-session`, `offline-capture-bench`, `buffer-bench`。

移植前建立逐命令／逐旗標相容表：可直接保留、版本化轉換、已由單程序決策取代。舊 `--capture-execution process` 不得默默執行 thread；明確拒絕或輸出遷移指引。舊 process 日誌仍須可分析。

| scripts | C++ 目標 |
| --- | --- |
| `unified_capture_bench.py`, `summarize_unified_capture.py` | `pas campaign`, `pas analyze campaign` |
| `profile_grpc_stages.py` | `pas profile-capture`，統一時間點與來源條件 |
| `offline_capture_comparison.py`, `recompute_offline_capture.py` | 離線比較／重算命令；明列 legacy process schema |
| `analyze_capture_pause.py` | 停頓／恢復分析；實測 ended_ns 與推算分開 |
| `verify_capture_faults.py`, `verify_probe_race.py` | C++ 故障／probe 競態測試或對應診斷命令 |
| `verify_scheduler_acceptance.py` | 三個具名 C++ 回歸與邊界擴充 |
| `build_capture_fixture.py`, `build_touch_fixture.py` | CMake targets／原生工具呼叫與 C++ 必要輔助程式；無 Python 建置依賴 |

上列新命令拼法為計畫設計，尚未實作。

## 4. Fixture 與測試

- Capture Java Fixture：Canvas、非對稱角落、24-bit 可見 counter、移動矩形；改 NDK C++ 後渲染管線改變，必須視為新測試來源，不與舊 APK 當同一條件。
- Touch Java Fixture：active pointer、96 點 trail、down/up/move/cancel 等 RGB 編碼；目前只有統計與最後位置，不足以驗證每指完整路徑。
- HTML capture Fixture：歷史瀏覽器比較來源。凍結進 legacy，由新版 Native Fixture 覆蓋正式能力，不為純 C++ 目標再維護 JS。
- 8 個 test 檔涵蓋：pipeline、capture bench、process bench boundary、capture process、gRPC capture、runtime core、input gRPC server、unified analysis。
- 桌面報告記錄 74 tests 通過；重測記錄 75 tests 通過。這是歷史測試紀錄，本次沒有重跑。
- 3 個額外排程重現失敗：證據到期 Hold 不釋放；dispatch 只用新 gate 放行舊 plan；completed intent 新 revision 再 down。
- 必須移植真正 loopback gRPC server、fake clock、取消阻塞、warmup 窗口、probe 競態、invalid frame、故障注入與重算等效測試。process-specific 測試須有替代或退役理由，不湊測試數。

## 5. 文件處置

| 文件 | 處置 |
| --- | --- |
| `AGENTS.md` | 保留 pixels-only／容量／monotonic／分布／分階段原則；加新遷移入口 |
| `README.md` | 前置新決策與遷移狀態；完成後改 C++ 安裝／執行；舊命令移歷史說明 |
| `ARCHITECTURE.md` | 明確以單程序 C++ 覆蓋舊 Python process 開發選擇；歷史測試敘述保留日期 |
| `ROADMAP.md` | 遷移 → 五候選評估 → 主程式新閉環／遊戲開發；不再強求來源達 59–60 Hz |
| `MAIN_PROGRAM_DEVELOPMENT_PLAN.md` | 標示舊語言／程序／先後順序已被新決策取代；保留未實作 M3–M7 功能規格 |
| `CAPTURE_AND_VISION_RESEARCH.md` | 舊 Python／混合建議失效，新增 C++ 五路徑與證據來源 |
| `CAPTURE_IMPLEMENTATION_PLAN.md`, `CAPTURE_LOW_LATENCY_PLAN.md` | 歷史修正／MMAP 一致性要求保留；process 不再是正式架構要求 |
| `ACCEPTANCE_20260925.md` | 保留失敗證據；新修正用獨立報告，不改寫歷史結果 |
| `CAPTURE_RETEST_20260925.md`, `CAPTURE_IDLE_RETEST_20260925.md`, `MEASUREMENTS.md`, `LIVE_CAPTURE_SHA256.txt` | 不回溯套用新門檻或新環境；新結果另列 |
| `PHIGROS_MECHANICS_RESEARCH.md` | 保留來源與未驗證事項；後續自有演算法均改 C++，本次不提前實作遊戲辨識 |

## 6. 量測資料保管

歷史資料被 Git 忽略，不能只保留分支就刪除工作區：

- 桌面 `measurements/unified_capture_20260925/`：日常負載 17 批。
- 重測工作區 `measurements/capture_idle_retest_20260925T052732Z/`：19 批、長跑、分段成本、A/B/A、observe／Session、164 檔 artifact hash。
- 桌面 `measurements/acceptance_20260925/`、`measurements/runtime/`：排程與觸控證據（實際存在項需在凍結時逐項核對）。

新開發任務先建立可驗證來源快照與 evidence index。複製保管必要證據時以顯式路徑、容量檢查、hash 驗證執行，不能搬走或刪除原始檔；不要複製 token／keystore 等憑證到 Git。

## 7. 本機工具鏈盤點

- 已發現 Visual Studio Community 2026 `18.5.11716.220`，具 x86/x64 C++ tools component；MSVC 目錄 `14.50.35717`。未進行 C++ configure／compile，不能寫成已驗證可建置。
- PATH 上 cmake／ninja 位於 Python310 Scripts 目錄；正式 C++ 建置須使用獨立安裝或 VS 隨附可執行檔，避免殘留 Python bootstrap 依賴。
- PATH 未找到 vcpkg；不等於全機未安裝，開發任務再查 VS bundle 並 pin baseline。
- Android SDK 已有 build-tools、platforms、emulator 等；本次未見 SDK `ndk/` 目錄。NDK／SDK 命令列工具缺件可依使用者授權安裝。
- 使用者指定下輪 AVD 5 vCPU／8 GB RAM，本次未確認已套用；preflight 必須核對執行中設定與 profile，不能沿用歷史 4 核／6 GB 紀錄。
