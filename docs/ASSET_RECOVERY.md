# 接續既有畫面紀錄與本機 Phigros 環境

核對日期：2026-10-23。基準為 `2d479e17fa1566c089dce019cbb11eb883ba47fd`；初次抽取為 `3be959e`。本文件修正資產盤點的遺漏，沒有重寫舊結果或把歷史資料視為新的遊玩驗證。

## 已經做過的工作

| 資產 | 基準文件記載 | 新方向可使用的部分 |
| --- | --- | --- |
| Dlyrotz IN13 全輪錄製 | 7,722 張 1280×720 無損 PNG，131.8293931 秒，PNG＋index 共 2,385,518,593 bytes | 多幀 note／判定線軌跡、畫面幾何與時間錨點 |
| 使用者指定的 12 段 | 3,843 個影格引用、3,722 張不同 PNG、片段 MP4／mapping、使用者理由與原 journal | 優先核對已知瓶頸，保留 clip-09 正常對照 |
| 原逐幀分析 | 21,184 原 journal rows、7,715 次 replay、Note ID／cancel／receipt join | 對照舊觀測與操作，產生可修正的記憶候選 |
| 較早的視覺 dataset | 3 runs、54 clips、408 ROI | 局部視覺校正與特效／Hold／line 標註起點 |

原始文件已按基準版本補回：

- [全輪錄製與選片](catalog/06-engineering/FULL_ROUND_RECORDING_20261001.md)
- [逐幀分析與 C36h 結果](catalog/02-game-results/DLYROTZ_FRAME_ANALYSIS_C36H_20261001.md)
- [視覺 dataset](catalog/07-data-learning/VISION_DATASET_20260927.md)
- [歷史外部原件引用](research/zero-miss-20261006/windows/evidence/external-originals.json)

這些是歷史來源快照，裡面的舊授權、啟動步驟、狀態與相對連結不自動成為新分支的操作指令；未補回的連結目標仍可從基準 Git commit 讀取。

## Git 取回什麼，原檔還要找什麼

20 首分數批次確實沒有逐幀資料，但其他工作有錄製紀錄。`measurements/`、`out/` 在基準中被 Git 忽略；新分支的精簡也沒有遷移本機資料。基準 tree 的 9 張 PNG、合成控制證據 ZIP、測試結果 gzip 與原碼快照都不足以還原這批全錄原圖；目前未找到內嵌的影像 payload。

原全錄的專案相對位置：

```text
measurements/game-assist/2026-09-30-m0-manual-continue/
  full-recording36g-01/
    sessions/manual-session-22885039263800/
      manifest.json
      full-recording/index.jsonl
      full-recording/frames/frame-NNNNNN.png
    review/selected-clips-v1/
measurements/vision-dataset-20260927/manifest.json
```

歷史根目錄為 `$env:USERPROFILE\Desktop\Phigros-Auto-play-System`。2026-10-23 在本機核對時該根目錄不存在；使用者隨後確認舊本機資料已刪除，並要求 main 若無原件就重新製作。因此接續以新錄製為主，不等待已刪除原件；Git 中的文件、工具與事件證據仍保留。

已知 SHA256／bytes 與基準文件 blob 見 [LEGACY_ASSETS.json](LEGACY_ASSETS.json)。找回後先核對 session manifest、index、逐張 PNG SHA、來源時序與 journal 身份，再宣告資料可用；僅有同名檔或相同張數不夠。

## 本機 AVD 是另一個資產

Android AVD 預設在 `$env:USERPROFILE\.android\avd`，SDK 通常在 `$env:LOCALAPPDATA\Android\Sdk`。它們位於專案之外，分支抽取不會把 `phigros` AVD 搬進 Git，也不會因缺少專案 `configs/` 就刪掉 AVD。

唯讀盤點：

```powershell
& ./tools/inspect-local-assets.ps1
& ./tools/inspect-local-assets.ps1 -LegacyRepositoryRoot 'C:\path\to\old-checkout'
& ./tools/inspect-local-assets.ps1 -RecordingRoot 'C:\path\to\manual-session-22885039263800'
```

輸出僅描述檔案與設定的存在狀態，不能替代開機、安裝版本、音源、觸控接點能力或 gRPC 協定驗證。腳本不啟動 ADB／emulator、不讀 token、不注入觸控，也不修改 AVD。盤點輸出可能含本機路徑，應保存在忽略的 `local/` 或專案外。

## 接下來的實作順序

1. **先重建 Phigros 的觀測基準。** 舊原件已刪除，使用現存 AVD 錄製一首完整歌曲，帶上音訊、畫面時間戳、幾何與遊戲版本。新錄製使用新的 session 身份，與舊 7,722 張資料分開；擷取品質不足時明列缺口。
2. **建立可校正的歌曲記憶。** 新錄製已有離線解碼與逐幀 PTS／音訊 sample-offset 索引，接下來由全輪資料建立 note／line 路徑與人工修正入口。舊 12 段理由只作問題線索，不能直接對回新影格 ordinal。若日後取回歷史原件，再另讀舊 `source_frame`／QPC／journal；兩批身份不可混合。
3. **把歌曲時間與畫面錨點分開。** 原影片第 0 秒是第一張 pre-roll，與 `round_start` 差 0.9602991 秒；兩者均不能直接當歌曲第 0 秒。先離線核對時間與座標，再把視覺錨點交給同步器。
4. **做音訊 shadow 同步。** 舊錄製工具沒有歌曲音軌；新觀測基準已另錄 FLAC 並可解出 PCM／PTS 索引。下一步確認 song-time 錨點、建立音訊特徵，量對齊、漂移及失鎖，不輸入。
5. **最後接 rolling planner 與 ContactScheduler。** 讓記憶決定未來事件，以真實音訊／視覺觀測續租，保留 pause／restart／geometry 改變後的撤銷與 release-all。先做一首 Phigros，世界計畫 adapter 再按同一契約接入。

舊全錄包含 77 Miss，舊 runtime Note ID 和收據也不是遊戲逐 Note 真值，不能直接把歷史觸控當理想譜面。若有版本明確的精確譜面，可用它核對記憶；錄影仍保留作幾何、時序與失敗案例來源。原件尚未找回期間可以做 reader／schema 與合成測試，但不能宣稱已還原歌曲記憶。

## 可按基準取用的舊工具

`include/pas/session_recording.hpp`、`src/session_recording.cpp`、`tests/session_recording_tests.cpp`、`apps/recording_check/{main,select}.cpp`、`apps/frame_review/`、`apps/miss_review/main.cpp`、`src/session_archive.cpp`、`src/session_pixel_clips.cpp` 仍可從上述基準 commit 讀取。

這輪補回來源索引，沒有把舊 vision research graph 全部拉進 default build。後續 reader 應沿用錄製的資料契約，獨立於即時視覺策略；舊工具重新建置的依賴與功能仍須逐項驗證。
