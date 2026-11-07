# 桌面中途停止修復（未實測）

使用者回報數首歌曲中途停止，並要求本輪不測試。分析只讀取四個已存在的使用者回合，沒有啟動遊戲、擷取新圖片、送出觸控或重跑回合。

三個不同問題：

- iL IN 等待開場約125秒、演奏約115秒後撞到程序啟動起算的240秒 session timeout。現在保留有界的手動等待期限，成功取得開場 anchor 後，以固定的 song-origin／phase 設定獨立演奏期限；不因新影格或 planner tick 續期。未演奏時的 UI 診斷紀錄改為100ms取樣，實際 HUD／學習／solver 仍逐影格處理，避免長時間 READY 等候吃掉演奏紀錄預算。
- DESTRUCTION AT 的難度字樣形狀失配與分數合成單一邊界區塊同時出現，連續29張新影格在派送前被排除，未量測 learned anchors。現在補上這個聯合分派；仍要求同一 policy、當前畫面的完整 pause／難度等級／title 及既有分數紋理證據，500ms畫面有效期限與20ms輸入遲到限制未改。原回合沒有保存遊玩RGB，因此沒有宣稱新分派已通過該真實影格。
- 面板只接受1MiB manifest，但 iL／Igallta／DESTRUCTION 的紀錄分別約4.4／9.3／17.9MB。Igallta 原生實際已完成1114個音符、5809 steps、辨識結算及最後零觸點，面板卻誤報失敗。面板改為固定128MiB檔案上限，只把23個身份／完整計數／結算／釋放欄位轉成PowerShell物件，投影另限1MiB；整份原件的SHA256與原成功條件保留。

Rrharil AT 是使用者 STOP，沒有取得開場、沒有輸入。三個有輸入回合的實際 Write 最大遲到均小於6ms，並無20ms遲到或串流Finish失敗；四輪最後觸點均為零。上述只屬於既存回合的紀錄分析，舊原件與舊面板ledger不改寫。

只重新建置 `chart_experiment`，沒有執行任何測試。新核心 `286a73497bf926bd3676950957ec3f146b49a3c392443d1a039bbe7dc5e7d365` 與DLL封存為桌面 `Phigros Runtime/20261029-native-012`，面板另安裝新版本快照並更新完整設定。既有V10曲庫／profile／compiled fingerprint、短斜向Flick32px／8／20／6ms、十指、人工選曲與Play、只保存結算圖等設定保留。

本輪 portable CMake／CTest、native CTest、PowerShell自測、GUI、gRPC、裝置及遊玩測試均 **NOT RUN**，由使用者實測。既有五首隨機驗證仍未完成，不繼承任何新資格；較早Thrash釋放異常也沒有新增解決證據。
