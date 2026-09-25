# 統一擷取重測結論（2026-09-25）

**17／17 批完成，正式窗口共 18,395 張交付影格，全部尺寸／方向正確、Fixture counter 可解碼，沒有擷取錯誤；所有 process child 正常回收。效能驗收未通過：9 個正常批次的來源跨度率為 21.10–30.47 Hz，研究門檻通過 0／9。** 本輪建立的是使用者選定的「日常負載」基線，不能把不同來源率的數字當成後端速度排名，亦不能沿用先前約 59 Hz 批次的通過結論。

## 環境與固定條件

- Windows 11 10.0.26200、Python 3.14.7、Intel Core Ultra 5 125H（14 核／18 logical processors）、Balanced 電源方案。
- Android 16 API 36，`sdk_gphone64_x86_64`，`emulator-5554`，physical 720×1280、density 320；擷取固定橫向 1280×720、source rotation=1、RGB888、top-down。
- 原生 Capture Fixture，scale=1、offset=0；無預覽。每批暖機 10 秒；正常各 60 秒，恢復／停頓／GIL 各 30 秒。固定順序、事先門檻與完整參數見 `measurements/unified_capture_20260925/manifest.json` 及各批 `invocation.json`。
- 使用者明確選擇維持目前日常負載。1 Hz GetSystemTimes 是全機 busy%，包含測試程序與 AVD，不能單獨歸因於背景應用程式。正常批次 busy p50 約 55.91–65.56%，各批最大 69.80–89.34%。
- HEAD `b5da5c834d0b5b2343e83ef394697a8b235bd1be`，dirty=true；所有受測 Python／Fixture 原始碼與 runner 的 SHA-256 在批次前後一致。
- 實際安裝 APK 已另存，SHA-256：`2f50b989c6ed48bfb804d3f5a9c6a289db49b6325a813608a373fd7358f25e23`。本工作樹缺少舊 build APK，因此沒有以假定檔案代替實際安裝版本。

## 如何解讀

正常批次來源 max/min=1.4439，超過事先的 1.05 可比性門檻；也全部不在事先的 57–63 Hz 範圍。不同 counter/s 為 21.06–30.30；到達間隔 p99 為 75.96–126.45 ms。這些是本輪實測，不代表主機閒置時的極限。來源率同時可能受到 AVD、主機競爭與整條擷取管線影響，本輪未隔離各因素，不能推斷先前來源變快／變慢的原因。

慢 consumer 每次延遲 100 ms，15 秒後恢復：T/P 正式 skip 分別 226/201，恢復首張主機駐留 18.16/24.90 ms；後半段駐留 p99 為 17.94/29.28 ms。符合丟棄落後影格的觀察，沒有按舊序列逐張補讀的證據。主機駐留不是來源絕對年齡。

接收停頓 500 ms，未保護的 P/T 首張額外相對落後 452.33/467.70 ms。100 ms 相對保護的 T/P 各丟棄 10/8 張，首張額外相對落後 −0.62/75.59 ms，代價是到達最大空窗 618.78/638.96 ms。負值表示比選定錨點較不落後，不表示負的絕對延遲。保護僅測相對漂移，仍不能保證 source absolute age。

GIL 兩批明顯受不同整體條件影響：P 的來源 20.72 Hz、全機 busy p50 74.63%；T 的來源 43.83 Hz、busy p50 34.30%。P 有 26 次 IPC 覆蓋；T 有 831 次來源序號跳號。這能說明各自的丟棄位置，但不能用兩批的收到率宣稱 process 比 thread 快多少。

**MMAP 維持診斷用途。** 可見計數完整，不足以驗證整張 RGB 快照的一致性；不得據較低駐留數字切換主程式。process payload 可繼續作開發基線，效能仍屬未通過／有待閉環量測，不稱「已滿足遊戲需求」。

## 統一範圍與剩餘限制

本輪統一來源 APK、輸出格式／幾何、暖機／窗口、批次順序、場景與 raw timestamp 重算。`capture` 依 capture_complete、consumer 依 consume_ns 各自過濾 [MEASURING, STOPPING)，再用 frame_sequence 連回 timestamp；保留完整分布與原始資料，不將不同來源批次混合算一個百分位數。

兩個既有 CLI 的診斷成本尚不完全相同：thread 每次 consumer 取 RSS，process 使用資源快照。process 的 capture JSONL 是到達 parent 的影格，不包含所有被 IPC 覆蓋的 child frame；不同層的 drop 不可互相相加。CPU 與 counter 資源取樣窗口另見原始摘要，不當作相同正式窗口的精確值比較。

另發現既有故障工具的注入差異：thread 在 `collect()` 持有 `collection_lock` 時暫停，consumer 也會等待同一把鎖；process 在 child 暫停。因此 thread 停頓批次約 504 ms 的最大主機駐留含量測工具的人為阻塞，不能直接與 process 的駐留最大值比較。這不影響無暫停的正常批次。後續若要比較「僅擷取端停頓」的成本，應先移除量測鎖的連帶阻塞並記錄實際 resume 時間，再做新的配對故障批次；本輪保留數據及此限制，不宣稱該故障注入完全等價。

## 重現及保存

```powershell
$env:PYTHONPATH='src'
# 確認 org.pas.capturefixture 在前景後，輸出到尚不存在的目錄
python scripts/unified_capture_bench.py --serial emulator-5554 --include-diagnostic-mmap --output measurements/<new-run>
python scripts/summarize_unified_capture.py measurements/<new-run>
# 單一批次共用重算器
python scripts/unified_capture_bench.py --recompute measurements/<new-run>/normal-1-T/capture.jsonl
```

本輪原始目錄 `measurements/unified_capture_20260925/` 保存 manifest、已安裝 APK、每秒負載、17 組 invocation／stdout／stderr／JSONL／PNG、`results.json`、完整 `analysis.json` 和 `tables.md`。`measurements/` 被 Git 忽略，工作樹移除前需另行保存。以下列出每批 raw JSONL 雜湊；主程式驗收見 [M0–M2 驗收報告](ACCEPTANCE_20260925.md)。

## 詳細批次數據

完整批次：True；程式雜湊未變：True。
正常來源 max/min=1.44，近似同來源率（≤1.05）：False；所有批次接近 60 Hz：False。

T=thread payload；P=process payload；M=process MMAP，僅診斷。主機負載為全部 logical CPUs 正規化 busy%，每秒採樣，只選完整落在正式窗口的樣本。

| 批次 | 秒數 | 收到 / 消費 / 不同 counter | 來源跨度 Hz | 不同 counter/s | 到達間隔 n；p50 / p95 / p99 / max ms | 主機 busy n；p50 / p95 / p99 / max % |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| normal-1-T | 60.01 | 1267 / 1267 / 1264 | 21.10 | 21.06 | 1266; 47.37 / 97.28 / 120.56 / 226.29 | 58; 59.27 / 78.25 / 80.43 / 80.76 |
| normal-2-P | 60.01 | 1824 / 1824 / 1818 | 30.47 | 30.30 | 1823; 27.97 / 70.41 / 108.36 / 197.61 | 58; 62.17 / 84.43 / 87.84 / 89.34 |
| normal-3-M | 60.00 | 1442 / 1442 / 1440 | 24.08 | 24.00 | 1441; 41.68 / 72.02 / 126.45 / 194.16 | 59; 59.49 / 80.79 / 84.24 / 84.87 |
| normal-4-P | 60.02 | 1391 / 1390 / 1390 | 23.26 | 23.16 | 1390; 43.96 / 85.31 / 107.48 / 128.97 | 59; 63.87 / 77.39 / 82.92 / 83.09 |
| normal-5-M | 60.00 | 1482 / 1482 / 1482 | 24.76 | 24.70 | 1481; 43.88 / 63.95 / 75.96 / 118.31 | 58; 55.91 / 74.14 / 78.98 / 80.03 |
| normal-6-T | 60.01 | 1465 / 1465 / 1463 | 24.45 | 24.38 | 1464; 41.26 / 82.49 / 102.00 / 143.64 | 59; 65.56 / 81.35 / 81.85 / 82.16 |
| normal-7-M | 60.00 | 1485 / 1485 / 1482 | 24.77 | 24.70 | 1484; 44.29 / 63.02 / 79.09 / 111.27 | 59; 56.42 / 67.34 / 69.61 / 69.80 |
| normal-8-T | 60.01 | 1426 / 1426 / 1425 | 23.79 | 23.75 | 1425; 43.10 / 84.23 / 101.84 / 125.18 | 58; 60.41 / 74.56 / 78.65 / 79.95 |
| normal-9-P | 60.02 | 1419 / 1419 / 1417 | 23.65 | 23.61 | 1418; 43.53 / 83.97 / 103.52 / 121.19 | 59; 63.16 / 78.55 / 80.34 / 81.28 |
| recover100-T | 30.01 | 730 / 504 / 729 | 24.30 | 24.30 | 729; 41.46 / 77.19 / 104.42 / 113.02 | 29; 62.65 / 72.05 / 73.38 / 73.48 |
| recover100-P | 30.01 | 688 / 487 / 687 | 22.87 | 22.89 | 687; 44.66 / 82.49 / 106.62 / 209.32 | 28; 60.38 / 73.80 / 76.37 / 77.25 |
| pause500-P | 30.00 | 664 / 664 / 664 | 22.61 | 22.13 | 663; 46.94 / 90.86 / 111.56 / 509.78 | 28; 66.32 / 80.67 / 82.22 / 82.64 |
| pause500-T | 30.01 | 639 / 639 / 639 | 21.62 | 21.29 | 638; 47.21 / 92.26 / 117.98 / 509.53 | 29; 65.52 / 77.26 / 84.71 / 87.56 |
| pause500-guard100-T | 30.00 | 699 / 699 / 697 | 23.92 | 23.23 | 698; 40.83 / 90.82 / 119.38 / 618.78 | 29; 65.83 / 77.23 / 78.65 / 79.06 |
| pause500-guard100-P | 30.02 | 694 / 694 / 693 | 23.81 | 23.09 | 693; 42.71 / 90.58 / 117.31 / 638.96 | 29; 71.56 / 82.14 / 86.71 / 88.21 |
| parent-gil-P | 30.02 | 596 / 592 / 596 | 20.72 | 19.85 | 595; 46.67 / 107.57 / 155.36 / 245.42 | 28; 74.63 / 85.21 / 86.99 / 87.51 |
| parent-gil-T | 30.04 | 484 / 480 / 456 | 43.83 | 15.18 | 483; 61.69 / 103.13 / 115.07 / 125.13 | 29; 34.30 / 58.11 / 73.73 / 79.25 |

主機駐留 = consume_ns − capture_complete_ns，不是來源絕對年齡。process 的 capture 時間由 child 記錄，包含後續 IPC／parent 等待。

| 批次 | 駐留 n；p50 / p95 / p99 / max ms | consumer skip | IPC 覆蓋 | buffer 未讀覆蓋 | capture 錯誤 / child 回收 |
| --- | --- | ---: | ---: | ---: | --- |
| normal-1-T | 1267; 4.30 / 20.32 / 27.01 / 34.40 | 0 | N/A | 1 | None / N/A |
| normal-2-P | 1824; 10.37 / 24.54 / 35.49 / 57.37 | 0 | 0 | N/A | None / True |
| normal-3-M | 1442; 5.62 / 13.30 / 20.81 / 46.63 | 0 | 0 | N/A | None / True |
| normal-4-P | 1390; 8.59 / 23.36 / 29.42 / 35.00 | 0 | 0 | N/A | None / True |
| normal-5-M | 1482; 5.69 / 12.82 / 17.91 / 22.49 | 0 | 0 | N/A | None / True |
| normal-6-T | 1465; 2.97 / 14.01 / 18.25 / 29.32 | 0 | N/A | 1 | None / N/A |
| normal-7-M | 1485; 5.65 / 13.16 / 17.37 / 34.75 | 0 | 0 | N/A | None / True |
| normal-8-T | 1426; 3.00 / 14.60 / 19.01 / 25.28 | 0 | N/A | 1 | None / N/A |
| normal-9-P | 1419; 8.57 / 24.08 / 29.41 / 34.39 | 0 | 0 | N/A | None / True |
| recover100-T | 504; 4.21 / 41.04 / 70.15 / 104.88 | 226 | N/A | 227 | None / N/A |
| recover100-P | 487; 11.14 / 55.01 / 74.76 / 96.24 | 201 | 0 | N/A | None / True |
| pause500-P | 664; 8.89 / 26.73 / 31.96 / 35.95 | 0 | 0 | N/A | None / True |
| pause500-T | 639; 3.45 / 15.72 / 21.24 / 504.16 | 0 | N/A | 1 | None / N/A |
| pause500-guard100-T | 699; 3.03 / 14.93 / 20.26 / 504.96 | 0 | N/A | 1 | None / N/A |
| pause500-guard100-P | 694; 8.49 / 24.25 / 28.60 / 34.16 | 0 | 0 | N/A | None / True |
| parent-gil-P | 592; 42.48 / 74.98 / 89.38 / 122.57 | 29 | 26 | N/A | None / True |
| parent-gil-T | 480; 16.06 / 32.41 / 47.16 / 57.92 | 3 | N/A | 4 | None / N/A |

停頓後相對落後：以停頓前最後一張為錨點，主機時間差減來源時間差。不是兩種 clock 的絕對相減，也不是來源絕對年齡。thread 沒有實際恢復 timestamp，故只用請求停頓時間選擇後續樣本，時間欄統一從停頓開始算。

| 批次 | 停頓後首張：距開始 ms / 額外相對落後 ms | 後 1 秒內相對落後 n；p50 / p95 / p99 / max ms |
| --- | --- | --- |
| pause500-P | 504.09 / 452.33 | 25; -37.95 / 397.68 / 441.13 / 452.33 |
| pause500-T | 505.83 / 467.70 | 25; -21.87 / 405.80 / 454.98 / 467.70 |
| pause500-guard100-T | 614.46 / -0.62 | 25; -15.34 / 10.56 / 21.62 / 25.05 |
| pause500-guard100-P | 635.54 / 75.59 | 24; 25.31 / 51.02 / 69.96 / 75.59 |

### 原始 JSONL SHA-256

| 批次 | SHA-256 |
| --- | --- |
| normal-1-T | `e22f466f503c6c348fdaa1345f7cb75bcc94e5949d4d19340e012c6912cd7981` |
| normal-2-P | `613104dc5fc501bcd1f673c1b326f6dc52471f76de4eefefd59f89f69c742fb4` |
| normal-3-M | `c249376d4a26e7621b62702682862e8b99b65fdcd75adb5e24a97073b4561a36` |
| normal-4-P | `f7779e59a3ccb3dcb15bd7c8ae29b1452651ce81470b9e536ac75358fdedfa30` |
| normal-5-M | `bc278d5b225709a10a2edff5b984a658cae913fdf485cd0cbe0ee64049e60162` |
| normal-6-T | `d9463dcf22a4378e0a557bf0fba8655c14181d4ce408f3ab0aeff5c35ae3a70d` |
| normal-7-M | `d9645d25621fa058f7bbf0c8b2402eac34bd6c452701c729a1a2cf452086b84f` |
| normal-8-T | `7b9bc389a6983b2843d6686434732e5f5a64ece09263043d574e90336be036c4` |
| normal-9-P | `c001e34310c8b87ce75f9393cd4eebf12654b154edb261febaefac6a18a09e32` |
| recover100-T | `c1934f682654837cb7860cd25649ef9a92b1c08d0231cf63b00c8339c0fce692` |
| recover100-P | `d89d19a915f9ca97ea3f8bbb476dbee974937adde63b0d01f6883a472a8cf7f4` |
| pause500-P | `1d212ecaf5f88fc1bed750e4bcde51a44dc2ec6b6ab55adf4ccebf90c68f7c83` |
| pause500-T | `adc5c8733e2150526ac239beb921590ac1051e14b75aa0e2b69c9badb23017d8` |
| pause500-guard100-T | `ca85d3734f01e6db3e58afcc65668ee98801d10c53d67d0b3dd281d54feb8ff2` |
| pause500-guard100-P | `95a6fa83d5287861642a5867ffc884668e9126e091cd5ceed52c1199da0d0568` |
| parent-gil-P | `986f640761140a5d39fb38c4201ebe117b3642ab9080f7232dcad23285e40c75` |
| parent-gil-T | `85134e4caf96f59b1bbace183c4f78f07d74c0c77ff75f0a5f40c992e033b942` |

manifest SHA-256：`113037e020dba85d640520dd0c1b0948777f6f577356ce5e1c50b65997106eaa`。
host_load JSONL SHA-256：`bd20f1c6afbc07d1b3645cf41d8727d52b432a278493b1d325c2a47393bb038d`。
