# 桌面介面版本安裝

2026-10-29 的啟動錯誤來自 `manual-launch.json` 仍引用舊版面板 SHA256，而開發目錄中的 `sweep-panel.ps1` 與 `catalog-panel-core.ps1` 已修改。只補其中一個雜湊，下一層相依核對仍會失敗。

桌面入口現在透過穩定的 `local/start-phigros.ps1` 載入完整版本快照。安裝器將啟動 runtime、完整曲庫面板、面板 core 及兩個曲庫入口一起複製到 `local/desktop-app/releases/<version>/tools/`，建立帶 SHA256／大小的 manifest，然後更新設定中的整組參照。開發目錄的後續修改不影響這份已安裝介面。

`tools/desktop-bootstrap.ps1` 是穩定入口的來源；`tools/desktop-start-runtime.ps1` 是原本啟動邏輯的來源。bootstrap 在任何 ADB／模擬器／遊戲啟動前，核對版本位置、五個指定成員及設定參照。它不在啟動時自動接受新雜湊。介面日誌保存在設定旁的 `panel-startup/`，不寫進版本快照。

工作區後續加入明確的 `none`／`dsound` 音訊選項（預設無聲）及冷啟動的橫向畫面核對，見[音訊與橫向畫面](DESKTOP_AUDIO_BACKEND.md)。已安裝快照在另行封裝前保持不變。

安裝或更新介面：

```powershell
python -X utf8 tools/install_desktop_frontend.py
```

安裝器只處理介面檔案與啟動設定，不執行應用程式、測試、裝置探索或觸控。設定與舊入口先逐位元組備份到 `local/manual-config-history/`；新檔案先完整寫入，再以同目錄原子取代切換。寫入前若原設定已被其他程序修改，安裝會停止。

既有 native archive、譜面／profile、相位、校正及曲庫身份保持原參照。仍為十指人工模式、使用者選曲和按 Play、正常遊玩只保存結算圖。這次安裝不代表先前未完成的五首隨機實機驗證已通過；該工作仍暫停。

依使用者明示要求，本輪不執行 CMake／CTest、PowerShell 自測、GUI、gRPC 或裝置測試，由使用者開啟桌面 `Phigros.exe` 測試。安裝時的檔案複製核對不宣稱為實際啟動成功。
