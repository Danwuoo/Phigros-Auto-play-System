# 重新建立 Phigros 畫面與音訊基準

舊 7,722 張原圖沒有提交至 Git，使用者已確認本機副本刪除。新錄製使用新的 session 身份，不冒稱恢復原件。舊文件／核對指紋仍見 [ASSET_RECOVERY.md](ASSET_RECOVERY.md)。

## 本機已確認的前提

現存 `phigros` AVD 可啟動，內有 `com.PigeonGames.Phigros` 4.0.1（versionCode 157）。本輪以 `-read-only -no-snapshot-save` 執行，另外以命令列覆寫 RAM 為 4 GiB；沒有重建或清除 AVD。只使用選單操作開始歌曲，沒有自動點擊音符。

原本媒體音量為 0/15。此時 gRPC 回傳的 PCM 長度遠低於經過時間；臨時改為 5/15 後，3 秒測試取得約 3.010 秒 PCM。錄製前要區分「靜音」、「沒有音訊來源」和「樣本缺失」，不能只看 RPC 沒報錯。

## 整首觀測錄製

`tools/record_scrcpy.py` 使用明確指定的完整 scrcpy **5.0.1** release。client/server 必須配對；這條觀測工具與保留的 C++ patched-gRPC backend 分開，沒有更換後者的 transport。

官方來源：[5.0.1 release](https://github.com/Genymobile/scrcpy/releases/tag/v5.0.1)、[錄製文件](https://github.com/Genymobile/scrcpy/blob/v5.0.1/doc/recording.md)、[音訊文件](https://github.com/Genymobile/scrcpy/blob/v5.0.1/doc/audio.md)。本輪 Windows x64 release ZIP 的 SHA256：

```text
b12a2c4ee8be317422451fc7dcf8ee20a71b5ea7ef9ad73ddd825a25316227a5
```

示例（輸出父目錄須已存在；session 目錄必須全新）：

```powershell
python ./tools/record_scrcpy.py ./recordings/new-session `
  --scrcpy 'C:\path\to\scrcpy-win64-v5.0.1\scrcpy.exe' `
  --duration 180 --max-mib 1024 --min-free-mib 2048
```

工具固定 `--no-control --no-playback --no-window --require-audio`，擷取 Android `output`，不使用主機或裝置麥克風。錄製為 **H264 20 Mbps＋FLAC 的 MKV**，限制最高 60 fps；H264 是有損來源，解出的 PNG 也不能當成原始 framebuffer 無損圖。影片實際 fps 與來源缺口必須另量。

輸出包括 `capture.mkv`、`capture.log`、`summary.json`，後者保存 command、runtime／server SHA、主機 wall/perf 時鐘 anchors、程序狀態與容量限制。native `--time-limit` 自行停止；wrapper 另監控空間、總 bytes 與外層 timeout。成功僅稱 `recorded`，不表示遊戲成功、逐幀無缺口或已校正 A/V phase。

工具不選曲或按 Play。應先確認歌曲與難度，再開始錄製，看到 log 的 `Recording started` 後由操作者開始歌曲；保留開始選單、載入、整首歌曲與結算。UI 操作時間只是命令時間，不能當成 song-time zero。

## 完整解碼與時間索引

`tools/analyze_capture.py` 以 PyAV 解碼影像和音訊，保存逐幀 PTS／time-base、音訊樣本數與非零比例、時間缺口、原檔 SHA 及預覽 PNG。影音 PTS 屬於容器／裝置錄製時間線，不是歌曲判定時間；仍需建立歌曲時間錨點。

```powershell
# 建議在獨立 venv 安裝；不影響 default C++ build。
python -m pip install -r ./tools/requirements-media.txt
python ./tools/analyze_capture.py ./recordings/new-session/capture.mkv `
  --output ./recordings/new-session-analysis
```

只接受全新分析目錄。分析成功稱 `decoded`；寂靜與缺少音軌分開回報。用實際音訊 sample duration、PTS 覆蓋與畫面可見起訖核對整首覆蓋，不能用檔案存在或影片秒數取代核對。

## gRPC 原圖與 PCM 診斷

`tools/record_emulator.py` 是另一個明確 opt-in 的短錄工具，輸出 PNG、每張來源 seq／timestamp／arrival／SHA、PCM WAV 與 packet 索引。預設 server PNG 保留其 bytes；RGB888 必須明確提供 `--rgb-row-order`。本機 RGB888 已與 server PNG／ADB 截圖比較，實際為 `top-down`，與 proto 的歷史 bottom-up 註解不同。

先依 `tools/requirements-recorder.txt` 在獨立環境安裝依賴，用該環境的 `grpc_tools.protoc` 對 `proto/emulator_controller.proto` 產生 Python bindings（不要提交生成物）：

```powershell
python -m grpc_tools.protoc -I ./proto --python_out=./local/bindings `
  --grpc_python_out=./local/bindings ./proto/emulator_controller.proto
python ./tools/record_emulator.py ./recordings/new-probe --duration 5 `
  --bindings-dir ./local/bindings --discovery-file 'C:\path\to\pid_123.ini'
```

需先建立 `local/bindings`；若工具鏈沒有內建 well-known proto 搜尋路徑，另外以 `-I` 指向 `grpc_tools/_proto`。工具預設只連 `127.0.0.1:8554`，由指定 discovery/token 檔讀取認證，不把 token 寫進產物，也不自動降級成無認證模式。

本輪 server PNG 在選單僅約 9–12 fps，故不作完整歌曲的主要來源。這個模式適合原圖抽樣、幾何或 transport 診斷；若要全面無損高 fps，還需獨立驗證 bounded 接收／編碼 pipeline。

官方 [`AudioStream.cpp`](https://android.googlesource.com/platform/external/qemu/+/refs/heads/emu-master-dev/android/android-grpc/services/emulator-controller/server/src/android/emulation/control/audio/AudioStream.cpp) 的時間戳是在讀取 ring buffer 後設定，沒有 sample-position 契約；參考原碼分支也不等於本機 binary 的逐位元證明。因此 gRPC 音訊 timestamp 僅作粗時間觀測，不能當作第一個 sample 的精確播放時間。新的音訊同步器仍須量測相位、延遲與漂移。

## 本機資料與測試

錄影、音訊、標註與校正放在忽略的 `recordings/`、`measurements/`、`local/` 或專案之外。原生歌曲譜面與素材身份尚未取得，這些觀測資料只能建立可人工校正的記憶候選。

離線 recorder 測試不連 emulator、不執行 ADB、不注入輸入：

```powershell
python -m unittest discover -s tests -p 'test_record*.py' -v
```
