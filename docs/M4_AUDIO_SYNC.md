# M4 音訊同步：離線核心與未通過的實錄門檻

目前具備無第三方的 feature-window 同步核心、串流 feature adapter 與合成回歸。**尚未通過整首即時輸入門檻**：1D 能量包絡在實錄結算後曾錯誤獲鎖；FFT 八 band 改善該負例，但首次獲鎖仍晚於首音；實際 C++ IIR 八 band 的一秒窗口更出現錯誤樂段獲鎖。這不是音訊／遊戲判定相位校正，也不是 live qualification。

## 實作與時間契約

`include/rhythm/audio_sync.hpp`、`src/rhythm/audio_sync.cpp` 接受 100／200 Hz、1..8 band 的 frame-interleaved 特徵，不擷取音訊、不操作裝置、不呼叫觸控。reference 的 WAV SHA／feature ID 由呼叫端驗證後提供；核心只檢查身份字串格式與數值／資源限制。

`AudioObservation.window_end_host_ns` 是音訊窗口結束位置映射到 host monotonic 的時間；`received_host_ns` 是交付時間。兩者分開保存，收到 callback 不能冒充 sample time。`timestamp_uncertainty_ns` 必須由 adapter 提供；未知值不能填零來宣稱準確。

`observe()` 只接受新的、遞增 observation ID 與 window-end 時間。成功觀測產生 `ClockMap`／`SyncLease`；lease 的 `observed_host_ns` 用窗口時間。`status()`／`snapshot()` 只檢查 clock、age、expiry 及增長的不確定度，從不刷新觀測時間或 ID。`reset()` 增加 epoch，暫停／恢復及外部 seek／restart／geometry 變更均需撤銷舊 lease。paused 狀態遇到 clock rollback 仍保持 paused，不會自行恢復獲鎖。

相關比對先對每個 band 各自去除時間均值，再合併 centered dot products／norms。全域比對要求 peak、次峰差、最低 RMS／feature variance，以及三個時間相容錨點；目前 acquisition 每錨點至少有半個窗口的新 feature，避免把 90% 重疊資料視為三次獨立確認。追蹤只搜尋已有 clock 附近，檢查 phase residual／rate／fit residual。

局部追蹤的 `continuity_assumed=true` 代表以未中斷播放為條件。**跳到聲音完全相同的重複樂段，在此觀測下不可辨識。** 因此 playing／paused／result supervisor 是必要條件，結算後不能自動重新 armed；即使 score 很高也不構成完整的歌曲身份或 seek 證明。

## 可攜 feature adapter

`audio_features.hpp/.cpp` 提供 mono float PCM → 100 Hz features 的離線與串流相同實作。`log_rms_v1` 是診斷用單 band；`iir8_log_energy_v1` 是七組 sample-rate-normalized、各兩級一階 low-pass 的相鄰差分，再加最高頻 residual，形成八 band。每 10 ms 計算 band RMS、取 natural log、floor 1e-5；獨立保留未取 log 的原始 PCM RMS。IIR 串流實作可用於診斷，尚未通過本曲開場的聲學定位門檻。

`fft8_log_energy_v1` 使用 20 ms symmetric Hann、10 ms hop、未補零的 DFT 頻率格；以 double-precision Bluestein／radix-2 FFT 實作，無第三方依賴。單邊頻譜的 DC／Nyquist 權重為 1，其餘為 2，以 `N * sum(Hann²)` 正規化 band power，再取 RMS／natural log／floor 1e-5。分界同為 100／250／500／1k／2k／4k／8k Hz。feature ID 記錄 kernel、窗口、hop、精度與 floor，不能把 IIR 與 FFT 混用。

FFT 第一個 feature 的真實 sample window 為 [0,20 ms)，之後每 10 ms 產生一個。`AudioFeatureSeries.feature_origin_ns` 為 10 ms，reference 必須將它設為 `AudioFeatureReference.song_start_ns`；feature k 的 exclusive end 為 `origin + (k+1)*10 ms`。RMS／IIR 的 origin 為零。live assembler 同樣使用最後 sample 的實際時間，所以 20 個 FFT features 首次觀測在 210 ms，沒有把 warmup 隱藏為 phase 偏移。

採樣率需可被 100 整除；八 band 接受 32–192 kHz，單 band 接受 8–192 kHz。每包最多兩秒，窗口 ring 最多 500 feature frames，預設窗口 500、每五個新 frames 交付一次觀測。FFT 另保留最多 20 ms PCM overlap，最大 convolution buffer 為 8,192 complex doubles。`first_host_ns` 必須是首個 sample 的 device/QPC 映射，`received_host_ns` 是實際收包時間；整包 exclusive end 不得晚於收包。未知 uncertainty 預設為 −1 並拒絕，輸出 uncertainty 包含窗口內最大 mapping uncertainty 及預設 2 ms continuity tolerance，FFT 包含兩個重疊 hop 的 uncertainty，沒有把 callback 時間當成 sample 時間。

device position 不連續、sample time 偏離連續預期超過 tolerance、採樣率變更、explicit discontinuity 會清除 partial-bin／filter／ring 並開始新 segment；timestamp error、未知 uncertainty、NaN、超量或 future packet 拒絕。`segment_reset` 即使同包被接受，也要求呼叫端立即撤銷舊同步。`reset()` 保留 observation ID 單調遞增，未收到足夠新樣本不會產生觀測。

## 明確設定與依據

| 設定 | 預設 |
| --- | --- |
| 窗口 | 500 frames；100 Hz 時為 5 秒 |
| 全域 peak／次峰差 | 0.8／0.08；次峰排除最佳點前後 100 frames |
| 局部搜尋／次峰差 | ±150 ms／0.02；排除前後 3 frames |
| acquisition | 3 anchors、至少 1 秒跨度；每個至少半窗口新資料 |
| tracking fit anchor 間隔／容量 | 500 ms／16 |
| phase residual／rate deviation | 30 ms／3,000 ppm |
| RMS／feature variance 下限 | 1e-5／1e-8 |
| observation age／lease duration | 200 ms／250 ms |
| mapping uncertainty／總 model uncertainty 上限 | 20 ms／30 ms |
| 資源 | reference ≤120,000 frames；window ≤2,000；bands ≤8；每觀測 ≤100,000,000 correlation terms |

這些是明示的工程與合成測試預算，**不是** Phigros 判定窗或實測延遲保證。model uncertainty 包含半個 feature interval、anchor fit residual、adapter 提供的 mapping uncertainty，以及窗口內最大允許 rate warp；age 增加時另按 rate budget 增長。這是保守模型預算，沒有統計校準。100 Hz 的半格為 5 ms，不能把輸出整數納秒當成納秒測量精度。

## 合成驗證

15 項具名 C++ checks 已於 MSVC 19.50／C++20／Release／`/W4` 通過。範圍包括已知 lag、多 band／200 Hz、三錨點與新資料守門、silence／mute／flat features、錯源、global 重複歧義、局部連續性限制、pause／resume／seek／restart／geometry、delayed／future／backward timestamps、NaN、重複 observation ID、CPU／記憶體預算及 lease 不被 tick 續租。

確定性 100 Hz／2-band／1 秒窗口、真實 rate +500 ppm 的合成測例：phase error p50 0.067472 ms、p95 0.085880 ms、p99 0.092803 ms、max 0.099131 ms；最後 rate error 0.497798 ppm。這只對該已知訊號／時鐘有效。壞觀測立即撤鎖；未交付新觀測時，預設 age 超過 200 ms 撤鎖。另有 5 秒預設窗口的 acquisition 新資料守門測例。

另有 15 項 portable feature checks，於同一 MSVC／C++20／Release 環境以 `/W4 /WX` 通過：physical RMS／log floor、IIR partial-bin chunk invariance、44.1／48 kHz 穩態 sine 比較、44.1 kHz fragmented packet 觀測時間（與整包差 ≤1 ns）、500-frame ring／50 ms hop／真實 ID、device/time/discontinuity reset、future samples／未知 uncertainty／timestamp error／NaN／兩秒資源限制、離線 tail 及 reset 後 ID。FFT 另外與獨立 O(N²) direct DFT 在 44.1／48 kHz 比對（log bands 差 <2e-6），驗證任意 packet splitting 的 features 完全相同、20 ms warmup／10 ms overlap、origin／真實 exclusive end、gap 清除 overlap，以及上一個 hop 的 uncertainty 只在仍被窗口包含時保留。這些驗證串流與時間契約，不代表聲學定位已合格。

## 原生音源對實錄：保留負結果

本機報告：[FFT／1D report.json](../../local-assets/phigros-4.0.1/m4-audio-sync/report.json)、[實際 C++ IIR report-iir8.json](../../local-assets/phigros-4.0.1/m4-audio-sync/report-iir8.json)、[實際 C++ FFT report-fft8-cpp.json](../../local-assets/phigros-4.0.1/m4-audio-sync/report-fft8-cpp.json)、[逐 band 研究 report-perband-research.json](../../local-assets/phigros-4.0.1/m4-audio-sync/report-perband-research.json)。同目錄保存完整 CSV timeline、SHA 與 feature manifests；不提交音源或逐時特徵素材。這些本機檔案不保證其他 clone 存在。

Reference 為 M0 native WAV，44.1 kHz stereo、SHA `aa9423b0f86ca4c1250f460bfbbc23d78a6405c2de739a2ba5ffef0b48261a44`；觀測為 scrcpy 錄製解出的 48 kHz WAV、SHA `da282d7e0c3fa361cf0cd4c677e0376e756c650e5d8069d06c62c9bf42bd18c7`。不是 WASAPI 錄音。

Replay 以 sample count 建立虛擬 host clock（100 秒原點、虛擬 10 ms 交付延遲），沒有從 scrcpy PTS 推造原始 QPC。實際歌曲／畫面／判定相位誤差均為 unknown。所有門檻保持相同；2／3 秒實驗另使用目前的半窗口新資料 acquisition 守門。

| 實驗 | 首次 lock（capture 秒） | 主要結果 | 固定 130 秒後負例中的 lock |
| --- | --- | --- | --- |
| 1D log RMS、5 秒、舊重疊 anchors | 30.20 | 主段 30.20–122.70；結算後 133.80 秒誤對 native 90.16 秒，score 0.932、margin 0.440 | **1 次** |
| 8-band FFT、5 秒、舊重疊 anchors | 29.21 | 29.21–127.91 連續 conditional tracking | 0 |
| 8-band FFT、2 秒、≥50% 新資料 anchors | 17.21 | 比較接近開場，仍晚於首音約 16.22 秒的 capture 估計 | 0 |
| 8-band FFT、3 秒、≥50% 新資料 anchors | 83.61 | acquisition 太晚，不能供整首使用 | 0 |
| C++ IIR 八 band、1 秒、≥50% 新資料 anchors | 20.50 | **錯誤樂段**：第一次 offset 約 −94.52 秒，另有約 −1.25／9.70 秒 aliases | 0 |
| C++ IIR 八 band、2 秒、≥50% 新資料 anchors | 85.50 | 85.50–125.90 conditional tracking，太晚供整首使用 | 0 |
| C++ IIR 八 band、3 秒、≥50% 新資料 anchors | 83.70 | 83.70–126.00 conditional tracking，太晚供整首使用 | 0 |
| C++ FFT 八 band、2 秒、≥50% 新資料 anchors | 17.21 | 17.21–126.71 conditional tracking，仍晚於首音 | 0 |
| C++ FFT 八 band、3 秒、≥50% 新資料 anchors | 83.61 | 83.61–127.71 conditional tracking，太晚供整首使用 | 0 |
| C++ FFT 八 band、5 秒、≥50% 新資料 anchors | 69.01 | 69.01–127.91 conditional tracking，太晚供整首使用 | 0 |

1D feature 為 channel 算術平均後的 10 ms RMS，再取 natural log、floor 1e-5。研究 FFT feature 為 20 ms Hann／10 ms hop，分界 100／250／500／1k／2k／4k／8k Hz 的八 band log RMS；各 band 各自做時間中心化。使用 bundled NumPy 2.3.5。這些 FFT 結果不能直接驗證另一種 IIR feature extractor。

IIR 實驗使用 portable C++ `extract_audio_features()`，reference／capture 分別 12,369／17,945 frames。1 秒窗口首次錯誤 lock 的 score 0.9122、margin 0.1039；之後錯誤重複段的 score 甚至達 0.98。其 offset 範圍 −94.5289 至 9.7139 秒，不能因 offset median 約 4.2249 秒或 130 秒後零 lock 而算通過。2 秒／3 秒 offset 範圍分別 4.2004–4.2550／4.2017–4.2449 秒，只是 fitted clock hypotheses 的變化，沒有實測 phase gold。所有 confidence 門檻不變，1 秒僅把次峰排除區設為半窗口 50 frames；2／3 秒維持 100 frames。這份結果不支持把 shadow 窗口下限降至一秒。

C++ FFT 對同一對 WAV 產生 12,368／17,944 frames；序列化為 float32 後，242,496 個 native／capture feature values 與 17,944 個 capture RMS values 對 NumPy 研究版最大差均為 0。這確認 kernel 的實作一致性，同時保留它無法在首音前獲鎖的負結果。

開場逐 band 診斷存於同目錄 `fft8-early-band-diagnostic.json`：2 秒窗口在 capture 8.01 秒，0–100 Hz 佔 query 時間中心化 variance 的 63.9%；>8 kHz 佔 13.0%，但有 73% frames 在 log floor。錯誤重複段（offset −1.26 秒）總 score 約 0.967，主要 alignment（約 4.22 秒）約 0.960。500–1000 Hz 的個別 correlation 其實偏好主要 alignment（0.796 對錯段 0.158），但只佔總 variance 的 1.1%。因此只去 band 均值仍會讓重複低頻／floor crossing 支配 aggregate NCC。下一項獨立 scratch 假設是逐 band variance normalization，加上明示 weak／floor 排除與最低有效 band 數；尚未修改 production sync，也不把同錄製研究當 held-out。

該 scratch scorer 已跑完整舊錄音：各 band 的 Pearson correlation 等權平均，query／candidate 均要求 log-feature variance ≥0.01、band RMS ≥1e-4、floor frames ≤50%，至少四個有效 band。NCC／margin／anchor 門檻保持原值；2／3／5 秒首次 lock 分別為 83.11／18.41／19.61 秒，皆無首音前 lock，130 秒後 lock 亦均為零。這未解決主要瓶頸，不建議直接加入 production。候選依各自有效 band 子集平均，也會影響 score 可比性，須另驗證。

更明確的結構限制是：開場可唯一定位的短窗口不足以湊齊三個各有半窗口新資料、且每個均單獨 globally unique 的錨點；後續重複段讓 top-1 峰跳到其他樂段。下一項研究是維持 bounded timing paths，讓早期唯一證據與後續所有相容候選共同確認 path-level uniqueness；不能只降低單窗 margin，也不能硬寫已知約 4.22 秒 offset。

使用者後續指示先不做音訊線，音訊研究已停止。既有 path prototype 結果已凍結於 [provisional note](../../local-assets/phigros-4.0.1/m4-audio-sync/PATH_RESEARCH_PROVISIONAL.md)：2／3／5 秒窗口首次 lock 為 8.31／27.81／15.01 秒，原錄音 130 秒後皆無 lock。但其 greedy predecessor 選擇可能錯誤排除可行競爭路徑，**path uniqueness 尚未成立**；尚未實作原訂的 bounded predecessor alternatives。production 同步策略未升級，也未以新 WASAPI 錄音調整門檻。另有 0.4–1.1 秒的 synthetic seek 撤鎖延遲及 identical-repeat seek 不可辨識限制，仍須獨立 supervisor。

130 秒後是固定的同錄製負例區段，沒有為 v2 降低 NCC 門檻；但整體仍是同一份錄製上的開發實驗，**不是獨立 held-out 錄製**。native 首音在 12 秒；由約 4.22 秒的音訊對齊推得 capture 首音約 16.22 秒，該估計也不是遊戲判定 phase gold。

目前需要可驗證的初始歌曲位置先驗、更有資訊的開場觀測、獨立錯源／重複／結算錄製，以及 playing 狀態 gate。不能因多 band 修復其中一個負例，就把 M4 宣告為可直接接 live 輸入。
