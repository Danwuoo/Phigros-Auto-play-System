# 2026-09-23 基線量測與限制

## 環境與重現

- 主機：Windows 11 build 26200、AMD64、Python 3.14.7；時間來源為同一程序的 `time.monotonic_ns()`。CPU 型號、主機背景負載與使用率未控制或記錄，故主機排程數值僅是這台機器的初步基線。
- 本機 Android SDK 的 `platform-tools/adb.exe` 和 `emulator.exe` 已安裝，但 `adb devices -l` 為空，`emulator -list-avds` 為空，沒有 system image。模擬器／Android／遊戲版本、AVD 型號、解析度、方向、縮放、目標幀率和真實觸控後端均無資料；**所有模擬器整合、觸控能力與遊戲判定效果尚未驗證**。
- 指令見 [README](../README.md#執行與重現)。暖機：主機排程每批先執行 10 次，不納入 100 次統計；合成與緩衝區測試沒有暖機。原始 JSONL 位於本工作樹的 `measurements/`（不納入 Git）；每次重跑同名檔會覆寫。統計採線性插值百分位數；本文的 jitter 為預定觸控與實際注入開始之誤差的 p95−p5，並另記 p95 絕對中位偏差。呼叫返回不代表畫面已反映觸控。

## 容量 1 的最新 frame 緩衝區

`buffer-bench` 以主機 clock 執行 1 秒，合成 producer 預定每 2 ms 發布 1×1 RGB24 frame，consumer 每讀 1 張後延遲 20 ms。結果：發布 357 張、讀到 48 張、未消費即被覆蓋 308 張；consumer 序號總共跳過 301。兩個計數因量測起訖與讀取邊界不同，不應相加。已讀 frame 的 age（讀取時間減擷取完成時間，n=48）為 p50 1.180、p95 2.899、p99 3.218、最大 3.232 ms；0 次錯誤。這驗證了下游落後時讀到最新 frame，不建立持續成長的 frame 佇列。原始檔：`measurements/buffer_slow_consumer.jsonl`。

## 合成 pixels → touch → pixels 閉環

合成畫面 64×64 RGB24、目標 1 秒出現一次、500 ms 後到達 y=48 判定線、60 fps、30 個目標；真值只在測試 world 與離線摘要使用。目標到達時間與偵測座標的量化誤差可重現；時間由虛擬 monotonic clock 推進，假觸控無 OS／模擬器注入延遲。

| 額外辨識耗時 | 處理 frame | 跳過擷取時限 | 命中／目標 | 預測誤差 p50 / p95 / p99 / 最大 (ms) | 預定到注入誤差 | 觸控到效果被看見 |
| --- | ---: | ---: | ---: | --- | --- | --- |
| 0 ms | 1,800 | 0 | 30/30 | −4.386 / −4.385 / −4.385 / −4.385 | 0 ms（虛擬 clock） | p50/p95/p99/max 4.386 ms |
| 5 ms | 1,800 | 0 | 30/30 | −4.386 / −4.385 / −4.385 / −4.385 | 0 ms（虛擬 clock） | p50/p95/p99/max 4.386 ms |
| 25 ms | 900 | 900 | 0/30 | 無可用注入樣本 | 0 次觸控 | 無效果 |

前兩批各 30 次觸控、30 次效果觀察、0 次誤點／漏點；擷取間隔 n=1,799，p50/p95/p99/max 均 16.667 ms。預測誤差為最後送出的計畫時間減測試真值；前兩批 p95−p5 約 0.0005 ms，幾乎完全由固定影格週期的小數捨入造成。辨識耗時 0／5 ms 是注入的虛擬延遲，不是 Python 影像處理的實際耗時。25 ms 的同步處理超過每影格 16.667 ms，跳過一半擷取時限；預測到期後排程器拒絕過晚動作，故 30 次全漏，沒有盲目補按。這暴露了目前同步 runner 的負載瓶頸；真實擷取及排程須獨立執行並重新量測。原始檔：`measurements/synthetic_60fps.jsonl`、`measurements/synthetic_delay5ms.jsonl`、`measurements/synthetic_delay25ms.jsonl`。

## 主機 monotonic 排程與假觸控

每批 100 個預定 down 事件，相隔 10 ms，前 10 次暖機排除。`--load` 在另一個 Python thread 持續執行 CPU 運算，只是受控擾動，不等於可比的 CPU 使用率。下表單位 ms；誤差 = `injection_start_ns − scheduled_ns`。每批 0 次假後端失敗。

| 條件／批次 | n | p50 | p95 | p99 | 最大 | jitter p95−p5 | 原始 JSONL |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| idle 1 | 100 | 0.688 | 4.352 | 24.814 | 34.652 | 4.100 | `measurements/scheduler_idle.jsonl` |
| idle 2 | 100 | 0.620 | 2.917 | 5.494 | 12.014 | 2.698 | `measurements/scheduler_idle2.jsonl` |
| load 1 | 100 | 7.716 | 14.884 | 15.554 | 16.140 | 13.695 | `measurements/scheduler_load.jsonl` |
| load 2 | 100 | 7.808 | 14.439 | 15.368 | 15.536 | 13.754 | `measurements/scheduler_load2.jsonl` |

假後端注入呼叫耗時：idle 1 的 n=100、p50 0.006、p95 0.009、p99 0.012、最大 0.117 ms；load 1 的 p50 0.002、p95 0.005、p99 0.009、最大 0.009 ms。排程尾端遠大於假注入呼叫，且 idle 批次間差異大；目前無法推斷遊戲觸控時效或判定窗是否可達標。需在 AVD 準備後量測擷取、真正觸控、畫面效果及整體誤差，再比較候選後端。

## AVD 驗收進度與後續需求

1. 使用者已建立並啟動 AVD，自行安裝遊戲並進入選曲畫面；`probe` 與首批 `capture-bench` 已完成，結果如下。CPU 使用與掉幀尚未量測；沒有可靠畫面產生時間時，不宣稱知道真實 frame age。
2. 建立可見回饋的簡單 Android 測試畫面及可維持獨立接觸點的觸控候選。逐項測 Tap、Hold、Move、Flick、同時按下、交錯放開與取消，量測座標誤差及觸控效果延遲。`adb shell input tap` 不足以完成此驗收。
3. 在同一實機配置重跑完整簡單目標閉環和正常／受控負載多批次量測。只有完成第 1–4 階段的實機驗收，才開始 Phigros 介面交接與專用辨識。

## 2026-09-23 `phigros` AVD 首次擷取量測

- AVD 設定：Small Phone、Android 16／API 36.1、Google Play x86_64 標準 4 KB 映像；4 vCPU、6 GB RAM、20 GB 內部儲存、host GPU、60 Hz 顯示。使用者已自行安裝遊戲並進入選曲畫面。ADB 序號在本次量測為 `emulator-5554`；`getconf PAGE_SIZE=4096`。實際擷取像素尺寸為橫向 1280×720。AVD 設定由使用者提供，連線、頁面大小、遊戲安裝與擷取尺寸由 ADB 驗證。
- 測試命令：`$env:PYTHONPATH='src'; python -m pas.cli capture-bench --serial emulator-5554 --samples 30 --warmup 3 --log measurements/adb_capture_phigros_20260923.jsonl`。主機為 Windows 11 build 26200、AMD64、Python 3.14.7；未控制背景負載。計時使用同一主機 `time.monotonic_ns()`。3 張暖機影格不納入統計，正式樣本 30 張，全部成功。
- `adb exec-out screencap -p` 呼叫耗時，n=30：p50 681、p95 901、p99 951、最大 960 ms。純 Python PNG 解碼耗時，n=30：p50 2,499、p95 2,799、p99 2,902、最大 2,909 ms。相鄰擷取完成間隔，n=29：p50 3,156、p95 3,556、p99 3,688、最大 3,733 ms；p95−p5 為 808 ms。原始 JSONL 在上述 `measurements/` 路徑，不納入 Git。
- 另以 5 次未壓縮 `adb exec-out screencap` 呼叫做初步診斷，呼叫耗時 665、1,017、596、720、588 ms。這不是正式候選後端基準，未解碼或驗證像素；它僅顯示避開 PNG 解碼仍不足以接近 60 Hz 影格週期。`screencap` 不提供可靠的畫面產生時間，因此無法量測真實 source frame age。
- 結論：目前 ADB PNG + Python 解碼路徑不能支撐遊戲即時接手。下一步須實作並比較更快的持續擷取路徑，記錄像素正確性、擷取間隔、frame age 可觀測性、CPU 負載和 p50／p95／p99／最大值；之後才進入觸控後端與簡單目標閉環驗收。此結果不代表遊戲判定窗或未來後端的能力。
