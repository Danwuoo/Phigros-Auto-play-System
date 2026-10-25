# 全曲 IN／AT 固定來源曲庫

更新：2026-10-25。工作分支：`feature/chart-driven-multigame`。

本輪把固定版本的公開譜面與配對音源完整下載、核對原始 bytes，建立可重建的曲庫與 Windows 瀏覽入口。範圍是來源快照內的所有 IN／AT，包含原生變體；不包含 EZ／HD，也不宣稱包含快照之後才新增的歌曲。

## 入檔內容

| 項目 | 數量 |
| --- | ---: |
| 不重複歌曲 | 327 |
| IN 原譜 | 333（327 份基本譜＋6 份變體） |
| AT 原譜 | 56 |
| 譜面合計 | 389 |
| OGG 音源 | 327 |
| 上游 metadata | 3 |
| 完整性核對資產 | 719／719 |
| 原譜 bytes | 1,690,292,007 |
| 音源 bytes | 1,264,287,749 |

原譜與音源合計約 2.75 GiB；不同難度與變體共用同一首歌的音源，沒有重複存放。Git LFS 管理 389 份 JSON 原譜及 327 份 OGG。metadata、library 與 manifests 使用普通 Git，便於不下載音訊就瀏覽索引。

來源為 [7aGiven/Phigros_Resource](https://github.com/7aGiven/Phigros_Resource)，固定三個 commit，而非會變動的 branch HEAD：

| 分支內容 | Commit |
| --- | --- |
| info | `da34d1de07cd59b096ca0520b2815a2ea11cc13e` |
| chart | `5d40bfdf05bee53f5107a6ea5620ca0e953de815` |
| music | `5142ba2673ccbeb23173886f3bd851e9c881e6ec` |

來源版本標記為 **4.0.1.2**。目前已封存原生實測環境是 **4.0.1／157**；兩者不能只憑歌名相同就當作同一譜面／同一版本。完整鎖定設定見 [song-catalog-sources.json](../configs/song-catalog-sources.json)。上游公開可讀與本次資料來源記錄不另構成授權聲明。

本輪使用者已明確要求將抓取內容用 Git LFS 推到此工作分支，因此公開來源 IN／AT 與配對音訊採上述入庫方式。這項範圍不包含私人 APK、校正、AVD、原始實測影片或既有本機證據。

## 取得與啟動

已經有此 repo 時，先保留本機變更，再正常 fast-forward 更新工作分支：

```powershell
git switch feature/chart-driven-multigame
git pull --ff-only origin feature/chart-driven-multigame
git lfs install --local
git lfs pull --include="assets/phigros/4.0.1.2/charts/**,assets/phigros/4.0.1.2/music/**"
python tools/sync_song_catalog.py verify --output assets/phigros/4.0.1.2
pwsh -NoProfile -STA -File tools/start-song-library.ps1
```

新 clone 若要先看曲庫而稍後才下載約 2.75 GiB 原件，可使用：

```powershell
$env:GIT_LFS_SKIP_SMUDGE = '1'
git clone --branch feature/chart-driven-multigame --single-branch https://github.com/Danwuoo/Phigros-Auto-play-System.git
Remove-Item Env:GIT_LFS_SKIP_SMUDGE
Set-Location Phigros-Auto-play-System
git lfs install --local
pwsh -NoProfile -STA -File tools/start-song-library.ps1
```

瀏覽器會辨識 LFS pointer，明示原件尚未下載；只有核對成功的原譜／音源才會交給本機應用程式開啟。要下載單首，可把 `git lfs pull --include` 限定為該首的 `charts/<track_id>/**` 與 `music/<song_id>.ogg`。完整介面操作見 [曲庫使用端指南](SONG_CATALOG_CLIENT.md)。

`verify` 會重新核對全部檔案並重建 library／verification report；它不連網。若本機只有部分原件，report 與索引會如實標示缺項，返回非零，不應把這種本機狀態提交覆蓋完整封存。先瀏覽索引不需要執行 `verify`。

## 可重建的下載管線

```powershell
# 只建立固定來源鎖定檔與 metadata。
python tools/sync_song_catalog.py snapshot --output assets/phigros/4.0.1.2

# 下載缺少的原件；既有正確檔案直接重用。
python tools/sync_song_catalog.py sync --output assets/phigros/4.0.1.2 --jobs 4

# 離線重驗完整原件。
python tools/sync_song_catalog.py verify --output assets/phigros/4.0.1.2
```

只有 `snapshot`／`sync` 會存取網路。每個檔案先核對固定上游 Git blob SHA 與 bytes，再記錄 SHA256，驗證成功才原子改名為正式檔案。下載中斷的 `.part` 可續傳；伺服器不支援 Range 時會重新下載。既有錯誤檔案預設拒絕覆寫；明確使用 `--repair` 才先隔離為 `.corrupt-*` 再重抓。

可用 `--charts-only` 先取得譜面；該模式仍會明示未取得音源，不把它當作全庫完成。來源 revision 改變時使用新的版本目錄與設定，不覆寫既有固定快照。

資料結構：

```text
assets/phigros/4.0.1.2/
  charts/<native_track_id>/IN.json 或 AT.json
  music/<canonical_song_id>.ogg
  metadata/info.tsv、difficulty.tsv、version.txt
  manifests/source-lock.json
  manifests/source-tree-info.json、source-tree-chart.json、source-tree-music.json
  manifests/verification.json
  manifests/native-import-report.json
  library.json
```

`library_id` 包含曲目、變體與難度，避免 IN／AT 被舊 track-only 去重方式合併。基本曲目的 native ID 仍含 `.0`，Random 的額外變體保留 `.1` 至 `.6`。來源未提供完整章節成員證據，因此曲庫不虛構官方章節順序。

## 原生匯入稽核結果

實際以本輪 Linux Release `chart_inspect` 對全部 389 份原譜執行嚴格 v3 匯入：

| 結果 | 數量 | 意義 |
| --- | ---: | --- |
| passed | 356 | 嚴格 importer 接受，尚待幾何、接點、首音、HUD 與本機版本核對 |
| unsupported_format | 5 | formatVersion 1，現有 v3 importer 不支援 |
| rejected：reversed event interval | 13 | 原生事件區間倒置，嚴格政策拒絕 |
| rejected：unsupported field | 15 | 新欄位尚未支援，例如 `blockAreaList` |
| timeout／integrity_error／inspector_error | 0／0／0 | 稽核程序與檔案完整性沒有這些錯誤 |

五份 v1 為 Burn、Next Time、Rubbish Sorting、mechanted、もぺもぺ 的 IN。原始資料均保留，沒有轉換或刪去未支援的欄位。

13 份倒置區間可在後續另用既有明確的 compatible policy 稽核；目前 `chart_inspect` CLI 只提供 strict，這一輪報告不混入相容解讀。15 份新欄位需研究語義後才決定支援方式，不能直接刪除欄位以求通過。

重跑命令：

```powershell
python tools/inspect_catalog_native.py `
  --library assets/phigros/4.0.1.2/library.json `
  --inspector build-local-importer/Release/chart_inspect.exe `
  --output work/native-import-local.json --jobs 2
```

單組態 generator 的 inspector 通常直接位於 build 目錄下。輸出須為新檔；明確使用 `--replace` 才能更新既有報告。預設 exit 0 表示稽核完整完成，`all_passed` 另行記錄；若需任何譜面拒絕都回傳非零，加入 `--require-all-pass`。

瀏覽器只在報告綁定的 library SHA256、每份 chart ID／SHA／相對路徑一致時套用此報告。全部 356 份 passed 仍維持 `live_ready=false`。

## 實測接續邊界

現有全曲執行端還有 2,000 notes、16,000 steps、最多 5 contacts 與最後事件小於 180 秒等有界限制；全庫中已有 8 份譜面超過 2,000 notes、9 份的最後原生 note 不早於 180 秒。這是原譜結構分析，不是完整接點編譯。AT 的共用 HUD 政策、變體的遊戲選曲對應、4.0.1.2 與本機版本的來源身份，均需個別補齊。

新瀏覽器可以連接既有 `launch-config.json` 開啟原 Legacy 面板；原面板仍掌管已校正曲目的核對、READY、STOP 與紀錄。公共曲庫的瀏覽選項不會自動轉為可實測清單。Flick 幅度／時機調整已接進 native adaptive profile，使用方法見 [Flick 微調](FLICK_TUNING.md)。

## Git LFS 發布

[catalog-lfs.yml](../.github/workflows/catalog-lfs.yml) 只在工作分支且 commit message 明確包含 `[catalog-lfs-sync]`，或手動 workflow dispatch 時執行；普通程式碼變更不反覆下載全庫。

流程會驗證固定來源、建立 LFS pointer commit、執行 `git lfs fsck` 和 `git lfs push origin HEAD`，核對分支尚未移動，最後才推送 branch ref。分支若在傳輸中前進，流程保留已上傳物件並拒絕覆蓋分支，交由後續正常整合。`main` 不在任何寫入目標中。

傳輸報告與 Windows 曲庫預覽各有 Actions artifact。發布成功仍須另核對實際 LFS 物件可下載，不能只看 pointer 檔案存在。

本輪測試與尚待本機驗證的項目見 [驗證紀錄](VALIDATION_CATALOG_2026-10-25.md)；接續建置見 [本機 6.1 sol xhigh 交接](HANDOFF_LOCAL_6_1_SOL_XHIGH.md)。
