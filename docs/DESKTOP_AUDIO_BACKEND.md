# 桌面啟動的音訊與橫向畫面

工作區的新啟動器預設使用 `-audio none`。這會停用模擬器的電腦音訊輸出，避免已觀察到的 DirectSound playback buffer lost 路徑使音訊處理停止；不更動 Windows 音量、音訊裝置或 Android 媒體音量。曲庫面板的啟動提示與日誌會顯示「無聲模式」。音訊封包能繼續流動並不代表歌曲判定時鐘已校正，亦不代表演奏通過。

需要由電腦播放聲音時，可明確選用 DirectSound：

```powershell
pwsh -NoProfile -STA -File tools/desktop-start-runtime.ps1 -EmulatorAudioBackend dsound
```

`none` 與 `dsound` 是目前允許的兩個選項。這是啟動參數；請先關閉模擬器再用所選模式開啟。沿用已開啟的模擬器時，入口會保持目前音訊設定，狀態中的 `applied_backend` 與 `no_host_speaker_output` 為未知，不會誤報已切換成無聲模式。入口不會為了套用音訊選項而關閉正在遊玩的模擬器。

冷啟動完成後，入口先讀取 emulator 的加速度感測器，確認 skin 與實體方向。方向未朝橫向左轉時，最多順時針旋轉三次；每次等待 500 ms，最多讀回三次確認預期方向，避免讀到舊方向就連續旋轉。只有讀到 X≥8、|Y|<2，且重力總量在 8–12 範圍內，才執行 `wm user-rotation lock 1`，再讀回鎖定狀態及 display 0 的邏輯畫面，確認為 1280×720。這可處理部分 SDK 忽略 AVD `hw.initialOrientation`、只鎖 WM 仍留下直向視窗的情況。任何方向回應不明或驗證失敗都停止開啟遊戲；不修改私人校正檔或 PAS 座標轉換。沿用已開啟的模擬器時不查詢方向或旋轉，幾何驗證狀態保持未知，以免影響已開始的歌曲。

`-PlanOnly` 只列出預定的冷啟動音訊與旋轉參數，不連線、不旋轉、不啟動遊戲，亦不執行十指 fixture。人工選曲、READY 後由使用者按 Play、只保存結算圖，以及原有 no-snapshot-load/save 冷啟動政策保持不變。

以上是工作區來源更新。已安裝的版本快照、桌面入口、啟動設定和既有譜庫保持原件，待另行封裝與安裝後才成為桌面預設。新 bootstrap 可轉交明確的 `-EmulatorAudioBackend`；若仍指向不支援此選項的舊版本，它會提示先更新介面，不會修改舊版本或自動接受新 SHA。

來源變更限啟動 runtime、bootstrap 參數轉交、面板的一行固定音訊提示及離線測例。DirectSound／無聲後端機制與原始失敗日誌另有本機 source audit；新來源的實際冷啟動、橫向讀回與遊戲結果需分開驗證，不沿用先前實測作為本次通過證據。
