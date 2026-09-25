# M0–M2 獨立驗收（2026-09-25）

結論：**M0 核心 smoke 可保留，M1 不通過，M2 部分證據不足；暫不宣告 M0–M2 全數通過。** 本次是驗收與統一擷取重測，未修改待驗的主程式，也未操作 Phigros 遊玩。

驗收工作樹 HEAD：`b5da5c834d0b5b2343e83ef394697a8b235bd1be`，dirty=true。原有 70 個 unittest 加上本次 4 個離線重算測試，共 **74 個全部通過**（18.145 秒），但下面的額外契約重現 3/3 失敗；測試數不能代替契約驗收。既有 M0–M2 完成敘述應以本文件的修正結論為準。

## 阻擋與待修正項目

### 1. P1：證據逾期不會在下一個動作前釋放 Hold

位置：`src/pas/contact_scheduler.py`，`next_due_ns()`、`run_due()`，尤其第 154–159 行。

重現：證據時間 t=0、期限 150 ms；10 ms down，1000 ms up。時鐘推到 300 ms 呼叫 `run_due()`，仍有 active contact 0，armed=true，fault=null。原因是 next step 尚未到期便 break，證據檢查只有動作到期才執行；若下一步為 up，更會直接跳過 freshness 檢查。

要求：將證據到期列為 owner 喚醒期限，且在「尚無到期命令」時也檢查維持資格；失效即撤銷後續命令及 release。不能只等待下一個 move/up。驗證 last evidence + deadline 前後、無新 frame、長 Hold、無 pending 命令與停止競態。

### 2. P1：dispatch 僅核對全域 gate，舊目標證據可繼續注入

位置：`src/pas/contact_scheduler.py`，第 158–160 行；submit 的第 88–92 行只有入列時檢查。

重現：t=0 提交 t=300 ms 的 Tap，證據期限 150 ms；t=299 ms 更新 gate，但不更新該 ContactPlan。t=300 ms 仍送出 down，此時 gate age=1 ms、plan evidence age=300 ms。

要求：dispatch／Hold 維持分別驗證 gate 證據與該計畫的來源證據／維持期限。新 UI frame 不等於該目標已重新辨識；必須有新 revision 或明確且有界的延續證據才能更新資格。這直接對應開發計畫第 5 節「只做 submit 時檢查不足」。

### 3. P1：同一已完成意圖的新 revision 會重複 down

位置：`src/pas/contact_scheduler.py`，第 94–96 行。

重現：同一 epoch/key 的 revision 1 Tap 完成；提交 revision 2，又產生第二個 down。`_completed` 只阻擋較小或相同 revision，沒有阻擋已消耗的意圖。

要求：定義並落實 completed intent 的去重生命週期，同一個已完成意圖不能因晚到的追蹤更新重觸；真正新意圖使用新 key。記錄有界，但淘汰後也不能讓仍可能到達的舊計畫復活；驗證遲到 revision、同 epoch gate 切換與 tombstone 淘汰。

### 4. P2：多指獨立移動／Flick 反向能力宣稱超過驗證範圍

位置：`src/pas/touch_bench.py`，第 174–185、370–374 行；`fixtures/touch_android/src/org/pas/touchfixture/MainActivity.java` 的事件計數。

180 組觸控與 30 組 batch 的原始 cases 確實存在，符合現有 checker 的零失敗敘述。然而 `moves_with_other` 只代表有兩指時任一指改變；沒有核對 A 的逐段位置、B 不變、各 ID 的交錯釋放。Flick 僅檢查 move 數量，座標誤差只取整組最後事件；即使中途方向錯誤或兩指一起漂移，仍可能通過。少數截圖也不能證明每組全程路徑。

要求：Fixture 將有界且可持續讀取的逐 pointer 路徑／階段證據呈現在 pixels，由 checker 驗證保持指漂移上限、移動指每段位置／方向、反向與交錯 up。加入錯誤路徑的負例，確認它們不能得到 `fixture_verified=true`。完成之前，把這些細分能力標為未驗證；保留目前 down/up、同時兩指、終點、取消等已有證據，不丟棄原始數據。

## 可重現驗收

在倉庫根目錄執行：

```powershell
$env:PYTHONPATH='src'
python -m unittest discover -s tests -v
python scripts/verify_scheduler_acceptance.py --output measurements/acceptance_20260925/scheduler_regressions.json
```

第二個命令目前 exit 1，三個 named cases 均 `passed=false`。它只使用假時鐘與假 backend，不接觸模擬器。修正後應變成全綠，並將邊界行為納入正式 regression suite。

已核對的既有實機證據：

- `measurements/runtime/touch-20260925-005509/`：180 cases，RPC／現有 visible checker 失敗各 0、cancel 30 組；報告 `gameplay_enabled=false`。
- `measurements/runtime/batch-20260925-010738-1114b5be/`：30 cases，RPC／現有 visible checker 失敗各 0。
- `measurements/runtime/disconnect-20260925-010049/`：一組 channel 斷線／新 channel 救援 smoke。不是整台 AVD 失聯或實機 RPC deadline 逾時的驗收。

本次獨立實機 smoke：

- `measurements/acceptance_20260925/touch-20260925-131651-1224e270/`：六類各 1 組，共 6 組、取消 1 組，RPC／現有 visible checker 失敗 0。樣本不足，正確保留 `fixture_verified=false`；不是重新完成每類 30 次的能力驗收。最終 PNG 顯示 down=62、up=62、active=0。
- `measurements/runtime/20260925T051711Z-799fba21/`：實機 observe＋診斷預覽運行 3 秒後退出。靜態 Fixture 收到 1 張 1280×720、rotation=1，有效維持 UNKNOWN、input_created=false、journal fault=null、child 正常回收。預覽初始化包含在第一張主機駐留中，不把這個 smoke 當效能數據。
- 完整 unittest 輸出保存於 `measurements/acceptance_20260925/unittest.txt`。重測後已返回開始時的 Touch Fixture 前景，確認無殘留接觸。

## 統一擷取重測

工具：`scripts/unified_capture_bench.py`。每次輸出到新目錄；先寫 manifest 再執行預定批次，不自動重跑挑好數字。正常批次各暖機 10 秒、正式 60 秒，順序 T/P/M、P/M/T、M/T/P；T=thread payload，P=process payload，M=process MMAP 診斷。壓力批次只比較 T/P，暖機 10 秒、正式 30 秒，包含 100 ms consumer 在第 15 秒恢復、接收停頓 500 ms、停頓加相對落後 100 ms 門檻、parent Python GIL 負載。

固定 1280×720、來源方向 1、RGB888、top-down、原生 Capture Fixture、scale=1、offset=0、無預覽。使用者選擇「照目前日常負載測試」，因此不關閉使用者的其他程序；每秒以 Windows GetSystemTimes 保存全機 busy%，不宣稱背景負載受控。原生 Fixture APK 從已安裝套件拉回保存及計算雜湊，來源與 runner 亦保存 SHA-256。本工作樹缺少 Capture Fixture 的舊 build APK，故不宣稱已完成舊 build 與安裝檔的二進位比對；本輪統一使用並保存同一份已安裝 APK。

兩條路徑共用 raw JSONL 重算：[MEASURING, STOPPING) 的 monotonic 窗口；capture 依 capture_complete、consumer 依 consume_ns 各自過濾，thread consumer 以 frame_sequence 回連 capture 時間。保留來源可見 counter 跨度率、不同 counter/s、到達間隔與主機駐留 p50/p95/p99/max，避免只比較 summary 名稱相似但定義不同的欄位。

事先門檻：正常批次來源 rate max/min ≤1.05 才可視為近似相同來源率；接近 60 Hz 的範圍先定為 57–63 Hz。研究性能門檻維持不同 counter ≥55/s、到達 p95≤33.4 ms、p99≤50 ms。來源不足時保留結果，標示 60 Hz 前提不成立，不以結果反過來調整門檻。

限制：process JSONL 是「交付到 parent 的 frame 的 child capture 時戳」，不是所有 child 收到的 frame；process CPU 有各自資源取樣窗口，兩後端 RSS／日誌取樣成本也不同，不將 CPU 直接排序或合併。比較對象是現有含診斷的擷取管線，不能把差異全部歸因於 transport。MMAP 即使計數可讀，也沒有證明整張畫面無撕裂；不升格主程式。source absolute age 與真正畫面到觸控延遲仍未知。

正式資料：`measurements/unified_capture_20260925/`。**17／17 批完成，共 18,395 張交付影格；9 個正常批次的來源 21.10–30.47 Hz，效能門檻通過 0／9。** 程式雜湊在測試前後一致。完整分布、來源可比性、停頓工具的剩餘差異與 raw 雜湊見 [統一擷取重測報告](CAPTURE_RETEST_20260925.md)。
