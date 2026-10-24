# 譜面輔助的局部視覺時間錨點：可行性與未通過界線

這輪僅做既有錄影的 **video-only 離線研究**，沒有讀取、解碼或處理音訊，沒有新錄製、裝置操作、輸入或 production importer/compiler/runner 修改。結論是：Dlyrotz 原生譜面在首個判定前已有可辨識的音符位置，可以產生局部歌曲時間候選；目前缺少可信的 capture→host 時鐘與原遊戲渲染誤差界，**尚不能產生具 live 資格的 SyncLease**。

本研究不沿用先前錄影的音訊零點、`+130 ms` 疊圖相位、目前 Offset 設定或任何 input lead。下列時間全部保留原始影片 PTS；它不等於歌曲時間或 host QPC。

## 最小模型與適用範圍

固定來源為已核對 SHA 的 Phigros 4.0.1 Dlyrotz IN 原生 v3 chart，首個判定時間是 `12.000 s`。本機實驗只使用 line 0 的 above Tap/Drag 中心，原生時間範圍為 `[1.028571429, 12.000] s`，畫面為 1280×720、無旋轉、滿版。這段線的數學位置固定在 `(640,576)`，切向為 `(1,0)`；不是整首或任意幾何的 renderer。

令 `F(t)` 為逐段積分原生 speed events 得到的 floor，`f_i` 為原件保留的 note floorPosition，`v_i` 為 note.speed，`a_i` 為 above/below 的符號。幾何候選為：

```text
F(t) = prefix_integral + event.value * (t - event.start_seconds)
P_i(t) = line_origin(t) + 72*positionX_i*tangent(t)
         + a_i*K*v_i*(f_i - F(t))*above_normal(t)
```

`K` 是須獨立驗證的 approach-distance 像素尺度，不能從 M2 的判定線 target 直接推得。原件 speed events 沒有 floorPosition，本實驗自行由零積分；note.floorPosition 仍使用來源值，沒有覆寫或重建它。實作使用 double，這不證明與遊戲 float 精度、首段 `9999` 速度累積、native floor rounding 完全相同。

參考來源是固定版本的社群實作，不是官方判定規格：

- [phisap `JudgeLine.floor` 與 `pos_of`](https://github.com/kvarenzn/phisap/blob/8ac9a67d584ed2c34faccebd9ad35194331ec9d5/judge_line.py#L118)：前者提供 floor 積分；後者只有線上落點。它沒有實作 approach renderer，note.speed/floor 並未被排程器消費，不能用來證明像素尺度。
- [Phira native parser](https://github.com/TeamFlos/phira/blob/191b56e8404e26ab4bdf6c7d38dc577a265510a7/prpr/src/parse/pgr.rs#L82)、[note renderer](https://github.com/TeamFlos/phira/blob/191b56e8404e26ab4bdf6c7d38dc577a265510a7/prpr/src/core/note.rs#L181) 與 [`HEIGHT_RATIO`](https://github.com/TeamFlos/phira/blob/191b56e8404e26ab4bdf6c7d38dc577a265510a7/prpr/src/core.rs#L15)：分別可交叉核對速度積分、height 差及尺度。實驗名義值為 `720/(2*0.83175) ≈ 432.82236 px/floor-unit`；Phira 會重建 note height 且對 Hold 特別處理，因此本實驗不把其 Hold 或可見性語義當成原遊戲保證。

首段 speed 含負值、零速與反向，不能直接將「越接近判定線」等同時間前進。line opacity 在約 `1.03–10.63 s` 為零，但實際音符依然可見；不可因線透明而刪除這些視覺錨點。

## 離線實驗

素材及工作程式保存在 repo 外 `../local-assets/phigros-4.0.1/m4-visual-timing-001/`。原件 chart SHA：

```text
aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90
```

第一份影片為 `dlyrotz-in13-20261023-02/capture.mkv`，SHA `9ef128e5072eae782d1c4aec373403926bc07af4c6497e9519856e5e2a90bcc2`。只選 video stream 解碼，從原始 PTS 抽 47 張開場畫面。色彩／水平 run detector 量測 cyan 音符中心，保留 note JSON pointer 配對；不是視覺模型訓練，也不識別所有 note 類型。

| 原始影片 PTS | 純像素／譜面結果 |
| --- | --- |
| 5.533 s | 4 個 cyan 中心，對到 line 0 的 notesAbove 40、44、42、43；在名義模型及 ±3 px 條件下，native time 候選約 `[1.388568,1.391730] s`。這些是因負速度提前進入畫面的未來音符，並非首四個判定。 |
| 6.040 s | 單幀有約 1.889 s 與 10.308 s 兩個相容候選；前後畫面的時間間隔才能排除後者。 |
| 7.004 s | 同一組三音符有 5 個候選，約 2.862、5.339、5.854、6.574、9.720 s；不能選最高分後直接 locked。 |
| 7.537 s | 零速度段保留整段 `[3.085714,3.771429] s`；位置本身沒有毫秒級時間資訊。 |

三個實際新畫面到 PTS `6.503 s` 時，已能保留同一開場 branch，所見 native 進度約 2.38 s，遠早於首個 12 s 判定。這支持開場辨識的可行性，並不證明從該畫面的抵達時間可準確預測 host dispatch。

22 張首判定前的樣本，以 **明示的實驗設定** `pixel residual ≤3 px`、假設影片 PTS 與 chart rate=1、每個渲染畫面相對此模型允許 `±30 ms`，共同 `video PTS - native time` 區間為 `[4.119857,4.142169] s`。這個窄的交集是同一份資料在假設下的結果，**不是已量測的 11 ms clock uncertainty**。不加 PTS 容差時共同集合為空；事後至少需要每畫面約 ±18.844 ms 才能容納這份資料。沒有證據可把這種不一致完全歸因於 capture lag、渲染 frame pacing 或尺度／floor 模型中的任一因素。

同 x 音符配對的 12 個間距尺度估計約為 430.36–433.33 px/floor-unit，中位數 432.13。這只是有限像素檢查，不能取代原遊戲 renderer 誤差界；尤其時間導數接近零時，小位置偏差會對應大時間範圍。

## 固定參數 heldout 與反例

第二份既有素材為 `dlyrotz-in13-20261023-wasapi-01/capture.mkv`，本輪仍只讀 video stream，沒有使用其對應的 WASAPI 資料。原始影片 SHA 及每個 sample 的 PTS/ordinal 保存在 `heldout-measurements.json`。

相同尺度、±3 px、±30 ms 設定套用到 PTS `42.527–53.017 s` 的開場樣本：前 17 張仍有相容時間 branch，至 PTS `51.040 s` 時共同集合變空，**結果為 rejected**。事後最小均勻時間擴張約為 ±41.922 ms；這僅描述失敗的大小，沒有將正式門檻改為該值。不能因第一份資料可配準就宣稱方法已跨錄影通過。

其他保存的離線案例：

- 首判定後 PTS `17.012–23.034 s` 的畫面串接到開場模型，被時間連續性拒絕。
- 同一個非停速畫面重複約 1 秒，被拒絕；實際畫面反向排序也被拒絕。
- **同一張畫面在 native 零速段內重複 0.2 秒仍留下寬候選。** 這是保留的反例，說明像素一致不能證明遊戲正在前進，也不能證明取得的是新 framebuffer。不能單靠這個候選自動續租。

這些是有限樣本與刻意構造負例，不是完整的錯曲、錯譜、遮擋、暫停、重開、不同 note skin 或不同縮放驗收。

## 建議的最小 production 契約（尚未實作）

先做獨立 `VisualTimingCandidateProducer`，輸出候選集合，不直接派送輸入。每筆實際 observation 應包含：

- 固定 chart/source/model/calibration identity、geometry version、epoch；圖像不能自行證明 chart hash。
- source frame id/sequence、原始 source timestamp 與 clock domain、實際 received_host、影像 hash；重讀舊圖不能建立新 sequence 或 observed time。
- **經驗證的 capture host-time interval**，或明確的 `timestamp_unknown/arrival_only`。來源只知道抵達時間時，不可把它當精確 capture time。
- 真實 playing 狀態 evidence，以及所有相容 song-time intervals、配對 note pointers、像素／模型參數區間、source 可見性／遮擋診斷。

候選以 set-membership 保留所有時間分支；低速度保留整個區間，速度／floor 跳變、line 移動與旋轉邊界必須分段。支援之前不能默默把 Hold、Flick、below 或不同幾何當成目前的 cyan Tap 模型。局部搜尋先驗必須來自同一 epoch 的實際新回合證據，不能在任意遊戲畫面上無條件宣稱「現在一定在首音前」。

取得至少數個具有足夠非零運動、不同 note／speed-event 資訊且時間一致的新觀測，才可能從 acquiring 轉入條件成立的候選。多分支、寬度過大、遮擋、未知 timestamp、錯 geometry、沒有可用速度、畫面過期或不一致，均回傳 ambiguous/unknown 並觸發現有 supervisor 撤銷。恢復須新 epoch、明確 re-arm 與已完成 release acknowledgement。

只有 capture→host 時間區間、原遊戲位置模型誤差與 rate bounds 都獨立量測通過後，才可把 song-time interval 與 host interval 的組合送入時鐘／租約 adapter。租約 uncertainty 需包含模型、像素、source/capture age、rate drift 與相位的最壞情形；更新只能來自新 observation，不是 status polling。視覺鎖定不會清除整首 contact plan 的 `requires_calibration`，也不授予 backend/input lead 資格。

目前最值得接續的是這個候選 producer 的離線區間求解與帶來源時鐘的 capture adapter；目前資料不足以核准 live 對時。scratch 程式只對固定開場模型枚舉種子並交集，尚未實作對一般曲線、所有遮擋或可見性邊界的完整 interval solver，不能直接提升為 production estimator。
