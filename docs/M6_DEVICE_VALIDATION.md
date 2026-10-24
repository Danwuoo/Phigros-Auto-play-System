# M6 裝置輸入驗證：實測紀錄

2026-10-23。SINGLE002、capacity002、batch001、cancel001 與 transport-loss001 的 host／fixture／完整 Android callback stream 已通過各自範圍的離線核對；STOP002 的實際中止後釋放另行通過。SINGLE001、capacity001 與 STOP001 的未完成／失敗紀錄保留。以下只描述指定 fixture 的觀測，沒有 Phigros 輸入、判定窗口或 input lead 資格。

## 版本與證據範圍

裝置標籤為 `phigros`／`emulator-5554`，fixture package 為 `org.pas.touchfixture.evidence.cpp`。本輪 native evidence v3 APK SHA256 為 `d4ae58c7f9561b4262791a1711e2688db233ec054b10400e88b5e60fccfc34a2`；安裝後 `base.apk` 雜湊已核對一致。APK 建置當下的 `installed_or_launched:false` 與安裝當下的 `launched:false` 保留歷史原值，不改寫成後續 runtime 狀態。來源見 [fixture build／install 紀錄](../../local-assets/phigros-4.0.1/m6-touch-fixtures/install-record.json)。

三個 binary 範圍分開記錄：

| 範圍 | SHA256／狀態 | 可支持的結果 |
|---|---|---|
| 獨立 M6 baseline `touch_probe.exe` | `09af8024e022f846f659b789e7db38e684e170178fd4e341c5940195171489ac` | [M6_BUILD](M6_BUILD.md) 的 Release build／6 CTest；該 baseline 驗證沒有建立裝置 channel |
| adapter 前 live-run `touch_probe.exe` | `874892be3d555140e15140bf17a2f7742addc95e87cd26a69950baf8226b100c` | [capacity001 harness plan](../../local-assets/phigros-4.0.1/m6-capacity-run-001/plan.json) 明記的執行版本；不能把它當成 baseline binary |
| 新增 probe-local stationary MOVE adapter 的 `touch_probe.exe` | `4a1734a93c8d5ea388fa59d668465af1be13c45f476120a74b9303551c0125d0` | 完整非音訊 **15/15 CTest**；capacity002 與後續案例各自記錄相同 binary SHA，不把新結果套回舊案例 |

manual-mode 另有先前完整非音訊 build／link **14/14 CTest** 與同 binary 的 3 秒暫停畫面只讀觀測；`chart_manual.exe` SHA256 為 `498142e2174573ac963a0e0e1914a593c9d8b9daa62b20b2119a8ff21eab3454`。157 筆處理觀測皆為 paused、round 0，沒有誤開回合；這不是完整人工開始／結束 UI run，也沒有遊戲輸入。[既有 build／paused 證據](../../local-assets/phigros-4.0.1/m7-manual-build-001/manifest.json) 保留原樣。本輪新增 adapter 後完整 **15/15 CTest** 通過（含 `fixture_path_adapter_contracts`，既有 manual-mode 測試仍在內）；預設離線 **4/4 CTest** 亦通過。[本輪 build manifest／日誌與 SHA](../../local-assets/phigros-4.0.1/m6-probe-build-002/manifest.json) 與先前 14 項結果分開保存。整理本報告只讀取既有結果，沒有重跑測試。音訊工作維持暫緩。

## 已完成的裝置案例

| 案例 | 整體狀態 | 已觀測與限制 |
|---|---|---|
| SINGLE001 | **incomplete** | host／fixture visual path 通過，但 native JSONL 未取得，analyzer 明確為 `android:missing_file`；不能繼承 SINGLE002 的原件證據 |
| SINGLE002 | **passed_observed_fixture_path** | 三個正常 host commands 對上 native DOWN／MOVE／UP；一接點、目標位置、完整 lifecycle、最後 zero、無遺失／fault。只支持該 fixture/session 的觀測路徑 |
| capacity001 | **failed** | host 因 `held_contact_path_unverified` 中止；正常僅發四 DOWN 加一 MOVE，後續走 fallback release，沒有完成原定四 UP commands。native 局部事實見下段 |
| capacity002 | **passed_observed_fixture_path** | 九個原定正常 host commands 完成；native 九筆 motion／peak 4／primary 與 stationary 路徑／完整釋放／最後 zero 通過 |
| batch001 | **passed_observed_fixture_path** | 批次 DOWN／UP 加 primary MOVE 完成，native 九筆 motion／peak 4／最後 zero；不把批次 API 宣稱為 Android 原子同時觸控 |
| cancel001 | **passed_observed_fixture_path** | 四 DOWN 加 primary MOVE 後以 `release_all` 取消執行計畫，四個 UP 完整釋放；native 沒有 ACTION_CANCEL，不能把案例名稱當成該 action 的測試 |
| transport-loss001 | **passed_observed_fixture_path** | 正常 held 路徑先驗證；client channel 切至無效 target 後注入及原 channel release 失敗、四指維持 unknown；新 recovery backend 釋放至 native zero。不是實體網路斷線測試，recovery UP 座標未校正 |
| STOP001 | **failed_or_unexercised** | fixture 畫面／capture signature 準備未通過，沒有進入輸入；完整 native stream 為零 motion，沒有觸發 STOP，不能算停止成功 |
| STOP002 | **stop_cleanup_verified** | 實際 native DOWN 後寫 STOP；執行器捕捉 user stop，釋放已按下的兩指至 native zero。16 個停止／關聯／釋放檢查通過，正常 expected-path analyzer 仍保留 failed |

原始 analyzer：[SINGLE001](../../local-assets/phigros-4.0.1/m6-live-single-001/analysis.json)、[SINGLE002](../../local-assets/phigros-4.0.1/m6-live-single-002/analysis.json)、[capacity001](../../local-assets/phigros-4.0.1/m6-live-capacity-001/analysis.json)。原始 failed／incomplete 資料不改標。

SINGLE002 的 native 檔為 `touch-evidence-28731-7317172359167.jsonl`，SHA256 `a9d023cf5dae32ae3e57e7e5b6493abbefb13ee1cec49d69d181420e5ee88cb2`。錄前 header-only 與錄後 closed retrieval 的 boot、PID、檔名／start stamp 關聯一致，錄前 bytes 是錄後檔案的精確前綴。三筆 callback 與 host 命令以動作語義、座標及接點生命週期核對，不把各串流 sequence 數字相等當成跨域對應證明。

capacity001 的 native 檔 SHA256 為 `06af9e1c9c39e7336f6c5dc46aa3914c19b7267907b461afcc44000cd0e14dca`。獨立核對九筆 motion：DOWN、三次 POINTER_DOWN、一次 MOVE、三次 POINTER_UP、最後 UP。第 4／5 筆均保有四個 active pointers；primary id0 位移 59.985352 px、目標誤差 1.004180 px，其他三指相對各自首個 DOWN 的實際座標漂移為 0 px。第 6–9 筆完整釋放至空集合；footer `duration_limit`、`fault:false`、`lifecycle_errors:0`、沒有未記錄事件。這證明該次至少四接點與 abort 後最後觀測 zero，不證明最大容量或完整原定流程。

capacity001 的失敗來自 fixture visual stream 將同一 Android MOVE 展開成每指 snapshot：primary target 匹配後，三個位置未變的 stationary MOVE 仍被保留 verifier 判為 `extra move after expected path`。新 adapter 僅在 touch probe 處理這種 snapshot：stationary ID 須有 expected DOWN、沒有 requested MOVE，且已觀測合法 DOWN／尚未 UP，位置同時在 expected 與 observed DOWN 的 tolerance 內；primary、未知 ID、非 MOVE 都不省略。原始 evidence／counter／log 保留，retained `src/touch_evidence.cpp` SHA256 仍為 `cac6121faaca2d24665010701da7a4b907392a421445c86159adfd20be667422`。20 個離線 adapter 檢查由實作者完成，亦已納入本輪 15 項整合測試；本報告整理與唯讀覆核沒有重跑測試。

新 adapter 的 capacity002、batch001、cancel001、transport-loss001 均已完成，harness `pointer_evidence_passed`、`failure:null`、`clock_failure:null`；各 analyzer failures／incomplete 為空，native stream complete／lifecycle valid／final zero 均為 true。前兩者正常記錄九個命令，後兩者正常記錄五個命令再走明示的取消／故障釋放路徑。transport-loss 的接受範圍是 pre-fault 正常路徑加 recovery UP counts／zero；analyzer 明記 `release_coordinates_checked:false`，沒有把 emergency release 座標當成校正通過。

| 案例 | 原始 analyzer／harness | native JSONL SHA256 |
|---|---|---|
| capacity002 | [analysis](../../local-assets/phigros-4.0.1/m6-live-capacity-002/analysis.json)／[run](../../local-assets/phigros-4.0.1/m6-capacity-run-002/report.json) | `ffeba2ce9904dc785fd3c3a4372804eed2bbeee90e07dbf0e00683ac01012962` |
| batch001 | [analysis](../../local-assets/phigros-4.0.1/m6-live-batch-001/analysis.json)／[run](../../local-assets/phigros-4.0.1/m6-batch-run-001/report.json) | `4f9ed2a2478678b90e7d104b99b2bf1af4c5b834885487f5508d418ffb9a941d` |
| cancel001 | [analysis](../../local-assets/phigros-4.0.1/m6-live-cancel-001/analysis.json)／[run](../../local-assets/phigros-4.0.1/m6-cancel-run-001/report.json) | `1ded4dd5df9ac9c2ccd9eafc51b285c27aa31b5d8b66e7e898f60759ee1c009c` |
| transport-loss001 | [analysis](../../local-assets/phigros-4.0.1/m6-live-transport-loss-001/analysis.json)／[run](../../local-assets/phigros-4.0.1/m6-transport-loss-run-001/report.json) | `7c9e7b4be831165565f2e9e0c29190fbb289d85fb98d03a1bd1f42a5dbf2fdf7` |

STOP001 的 probe 在輸入前失敗、沒有 host manifest；後續保存的 [ADB 畫面](../../local-assets/phigros-4.0.1/m6-stop-run-001/adb-screen-after.png) 仍為黑色 fixture 內容，原 native closed snapshot 只有 header／footer、零 motion。[失敗報告](../../local-assets/phigros-4.0.1/m6-stop-run-001/report.json) 保留 `failed_or_unexercised`。這不是已證實只差 500 ms 的啟動競態修復；黑畫面根因仍未確定。下一次明確執行的 STOP002 同時加入 inspect-only readiness gate，該 gate 確認 signature／zero 且不輸入後，才進入實際 probe。

STOP002 在 native sequence 1／active `[0]` 已實際出現後寫入 STOP；執行中的第二個 DOWN 亦抵達，因此最後 peak 2。host 回報 `user_stop_requested:true`、`failure:user_stop_or_session_deadline`、return code 2，release requested `[0,1]`，failed／unknown 均空。native 四筆 DOWN／POINTER_DOWN／POINTER_UP／UP 完整且 lifecycle valid，最後 active 為空；其 SHA256 為 `e702bda36504a6c5a03b7afdf815778c07995ca59069155aef3e2c63548a6301`。錄前／watch／錄後 prefix、當次 PID／boot、實際 trigger 與 release evidence 的 16 項檢查全部通過。這只驗證本輪 STOP-file 路徑，不涵蓋 Ctrl+C、四指全部持有時的 STOP 或所有可能停止時機。原 [正常路徑分析](../../local-assets/phigros-4.0.1/m6-live-stop-002/analysis.json) 仍為 failed／`expected_path_verified:false`；[獨立停止接受報告](../../local-assets/phigros-4.0.1/m6-stop-run-002/report.json) 才記錄 `stop_cleanup_verified`，沒有把中止當成完整計畫通過。

## SINGLE002 callback clock join：僅診斷區間

使用 [order-only API](M6_CLOCK_INTERVALS.md) 與同時錄製的 256 個 causal pings，只將 Android `CLOCK_MONOTONIC` **callback arrival** 夾入 host QPC 區間，再減去該命令實際記錄的 host dispatch。沒有使用固定 offset 交集、同速假設、內插、外推或中點。

| 命令 | 支持 ping sequences | dispatch → callback 區間 |
|---|---|---|
| DOWN | 19／20 | [0.5841, 52.8123] ms |
| MOVE | 29／30 | [−1.1755, 33.9373] ms |
| UP | 36／37 | [−24.4695, 13.2152] ms |

負下界表示 causal 樣本間距不足以收緊排序，不能讀成負延遲。每個 sample 的 boot、兩次 retrieval 各自前後 boot／PID、跨 retrieval 一致性、filename／header 及來源 SHA 均已核對；六個離線拒絕負例與獨立區間算術檢查通過。Android `MotionEvent event_time` 另存，不當成 callback arrival；callback age 只在 Android 域內相減。

同一 Windows QPC session 是本輪執行者明示的關聯，沒有獨立保存的 Windows boot/session ID，也沒有身份認證。記錄的 QPC nominal resolution 為 100 ns，並不代表硬體或 instrumentation accuracy；C++ 整數轉換的數學截尾亦不是零誤差保證。額外 physical-time uncertainty padding 尚未量測，不能把表格當成完整實體時間精度界限。完整來源雜湊與範圍見 [join report](../../local-assets/phigros-4.0.1/m6-single-clock-join-002/report.json) 及 [precision note](../../local-assets/phigros-4.0.1/m6-single-clock-join-002/precision-note.json)。

這些資料沒有提供單一 input latency、遊戲 input lead、song zero、render/pixel age 或 Phigros 判定資格。[capture clock 的來源限制](M4_CAPTURE_CLOCK.md) 仍成立；後續案例只在其實際觀測範圍內另行驗證。
