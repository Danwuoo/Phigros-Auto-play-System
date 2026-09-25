# 重測工作區恢復與交接紀錄

日期：2026-09-25。本文件保存消失事件、重建来源與hash核對，**不是原始量測資料或重新執行的性能測試結果**。

盤點期間 `C:/Users/wurre/.codex/worktrees/8bd4/Phigros-Auto-play-System` 可讀；稍後建立快照時該目錄消失。本次規劃任務沒有刪除或移動它。使用者最初確認沒有備份，之後提供重建工作區：`C:/Users/wurre/Documents/Codex/2026-09-25/new-chat/outputs/recovered-8bd4`。

重建來源為目前桌面中的30個原任務未提交檔＋封存紀錄的15個檔案修補；另一個無實際變更的README修補略過。重建仍基於同一b5da5c8提交，沒有合併回main。使用者報告重建後diff check與75個unittest通過，本次規劃沒有再次執行測試。

本次用消失前已保存的70檔inventory核對重建檔，66檔hash相同。`src/pas/cli.py`、`tests/test_capture_bench.py`、`scripts/unified_capture_bench.py`、`scripts/summarize_unified_capture.py` 及 `docs/CAPTURE_IDLE_RETEST_20260925.md` 全部符合原hash。差異為README、ARCHITECTURE、ROADMAP、MAIN_PROGRAM_DEVELOPMENT_PLAN四份文件。不能由66檔吻合宣稱整個舊工作區完整恢復；完整清單見 [CPP_RECOVERY_VERIFICATION.json](CPP_RECOVERY_VERIFICATION.json)。

## 證據層級

- `CPP_MIGRATION_INVENTORY.json` 保存消失前兩工作區70個檔案聯集的hash。hash不能還原檔案。
- 桌面現存來源與上述已核對重建檔可凍結；每檔來源／hash由交接manifest保存。四份有差異文件以桌面最新決策為主。
- 原始 `measurements/capture_idle_retest_20260925T052732Z/` 不可存取，不能重算或重新驗證其164檔hash，不以摘要生成假的raw。
- 桌面日常負載／runtime／acceptance資料另行確認，不與消失的低負載資料混淆。

## 已讀歷史報告摘要

原報告 `docs/CAPTURE_IDLE_RETEST_20260925.md` 記錄：Windows11 10.0.26200、Python3.14.7、Balanced；Android16/API36、1280×720、rotation1、RGB888/top-down。已安裝Capture Fixture APK hash為 `2f50b989c6ed48bfb804d3f5a9c6a289db49b6325a813608a373fd7358f25e23`。

19/19批退出碼0、43,548張交付；正常9批28,989張，各10秒暖機＋60秒正式。payload六批來源44.66–55.89Hz，舊性能數值門檻0/6；MMAP三批數值3/3，但一致性未證仍diagnostic-only。正常全機busy p50各批20.12–37.22%。負載、時間與thread逐影格RSS診斷成本變動，不能將改善全部歸因於關閉程式。

另有process payload正式180秒：9,847張交付；到達間隔n=9,846，p50/p95/p99/max=15.97/39.04/52.02/77.19ms；主機駐留n=9,846，6.02/9.42/12.07/25.72ms。source absolute age未知。單組20秒擷取關／開／關的Fixture繪製率56.45／55.14／56.29Hz；不能由單組差異作一般化性能結論。

報告記錄75/75 unittest、observe10秒、Session5秒、MMAP Session拒絕、client取消／重連。真實0×0 inactive、完整AVD失聯未驗證。上述全部是已讀報告引用，本次未重跑；不以新40–57Hz正常範圍改寫歷史通關結果。

## 已成功讀取的四檔 diff 規格

### src/pas/cli.py

- `grpc_capture_bench` 新增 `sample_consumer_rss: bool = True`。
- `collect` 鎖前設 `pause_start_ns=None`；`collection_lock` 內只選定一次pause與開始時間，鎖外才sleep。完成後寫 `receiver_pause`，含 `start_ns`、實測 `ended_ns`、duration與frame sequence；consumer不被量測鎖連帶卡住。
- `sample_consumer_rss` 控制逐影格RSS，manifest保存設定，summary加入 `consumer_rss_sampling=per_frame|disabled`。
- CLI新增 `--no-consumer-rss`，傳入 `not args.no_consumer_rss`。

### tests/test_capture_bench.py

- 新增 `test_receiver_pause_does_not_hold_consumer_lock_and_logs_resume`：fake Source、warmup0.02秒、正式0.4秒、pause150ms、RSS關閉；一個pause、ended-start>=150ms、consumer駐留max<120ms、summary為disabled。
- C++移植優先用同步事件證明consumer不被鎖阻塞，實機分布另測，不盲目依賴負載敏感的120ms單次断言。

### scripts/unified_capture_bench.py

- `recompute` 用with context關閉檔案。
- 加 `slow50`，即consumer delay50ms；正常9批＋5種壓力各T/P，合計19批。
- manifest `fixed.thread_per_frame_rss=false`；thread命令加 `--no-consumer-rss`。
- limitations新增：child ipc_published是資源快照窗口估計，不是全部child frame的精確正式窗口事件。

### scripts/summarize_unified_capture.py

- `complete` 除批數齊全，還須各批returncode0、geometry_valid、fixture_decoded_fraction==1。
- payload normal與diagnostic MMAP獨立統計：`payload_normal_numeric_pass_count`、`diagnostic_mmap_numeric_pass_count`、`payload_normal_source_near60_count`。
- pause表從實際恢復時間起算；舊紀錄沒有ended_ns時標推算。保存end_basis與實際duration，`after_resume = after_pause_start - actual_duration`。

## 新任務執行方式

桌面來源疊入四個hash已吻合的修正檔，凍結為本次legacy參考；保留重建來源註記。75項通過是使用者提供的重建驗證結果，開發任務自行建立可重現驗證。上述修正於C++落實並補回歸；以仍可取得的舊JSONL驗證相容性，消失資料明列缺項。新環境5核／8GB重做C++測試；原始低負載資料遺失不阻止其他遷移，但不能補造或聲稱已重算。
