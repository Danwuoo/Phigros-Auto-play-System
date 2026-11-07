# 桌面演奏核心更新：由使用者實測

使用者於 2026-10-29 要求將先前修改的演奏邏輯及短斜向 Flick 安裝到桌面程式，並明示本輪不執行測試，由使用者自行測試。這份更新與先前尚未完成的五首隨機驗證分開記錄，不宣稱已通過該驗證。

本輪採用既有凍結核心 `b45145af450aacdbf93bb672fd86f0603e69efb167182061e7cefb71732428a9`，包含演奏排程與批次 MOVE、HUD／畫面同步恢復、提早備妥的等待處理，以及旋轉 Hold 與混合開場辨識。沒有重新編譯或執行測試。32px 短斜向 Flick 使用 `judgment-anchored-diagonal-snap-v1`：判定前 8ms 按下、20ms 滑動、末端 6ms 後釋放；兩個 MOVE 保留短折點。

核心與其九個凍結 DLL 獨立封存到桌面 `Phigros Runtime/20261029-native-011`。既有桌面 `Phigros.exe` 與上一輪修好的獨立介面快照沿用；更新只切換啟動設定中的 native archive 與配套曲庫。

為配對新版開場模型，需產生 278 份新版曲目 profile；其餘 96 份保留原件，15 份含 `blockAreaList` 的項目維持標示不支援。資料製作僅執行離線 `prepare-adaptive`，不執行 `check-adaptive`、測試套件、GUI、ADB、模擬器或實際觸控。原譜／校正／完整接點 fingerprint、原有資源限制及每首政策保留，未以修改雜湊代替原生輸出。

仍為人工選曲、READY 後使用者按遊戲 Play、設定十指及正常遊玩僅保存結算圖。啟動設定更新前先保留備份，再以完整檔案切換。

已知限制：先前 Thrash force 的實際回合雖送完接點並辨識結算，最後仍有一個 kernel 觸點，該回合未通過釋放核對。這份安裝沒有新增實機證據，也沒有宣稱該異常已解決。停止／釋放與剩餘觸點檢查保留。所有本輪 CMake／CTest、Windows／PowerShell 自測、gRPC、GUI、裝置與遊玩測試均 **NOT RUN**。
