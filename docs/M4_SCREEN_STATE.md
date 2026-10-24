# 畫面狀態錨點：非音訊輔助線

依使用者要求，音訊研究已暫緩。這個元件只回答畫面是否符合已校正的播放、暫停或結算樣本；它不推估歌曲時間、不建立同步租約，也不自動啟動輸入。

`screen_state.hpp/.cpp` 接受有界 RGB888 畫面與 2–8 個不重疊的灰階區塊。每一區塊都須有足夠對比，且平均像素誤差均低於設定門檻。幾何不符、影像長度不符、找不到樣本或同時符合多種狀態時，回傳 unknown。只有相同狀態的多個樣本吻合才可合併。元件不從畫面推論 chart hash 或裝置身份。

目前工程預設：每一 anchor 平均誤差至多 8/255、對比至少 40/255、每個 template 至多 65,536 pixels、最多 8 個 templates。這些是有界的比對參數，沒有官方遊戲判定意義。背景遮擋或 HUD 特效可能使 playing 回傳 unknown；執行端應停止，而非沿用上一個 playing 標籤。

## 離線工具與資料格式

開啟 `RHYTHM_BUILD_SUPERVISOR=ON` 會建置元件與合成回歸。另開啟 `RHYTHM_BUILD_CHART_IMPORT=ON` 時提供：

```powershell
./build/Release/screen_state_probe.exe DATASET.json
```

資料 schema 為 `pas.screen-state-dataset.v1`，包含共同 width/height、templates、samples。每個 RGB 檔為緊密排列、top-down、三通道，長度必須精確等於 width × height × 3；工具會核對其 SHA256。templates 每筆提供 state、calibration_id、source_image_sha256、rgb `{path, sha256}` 與 regions `[[x,y,width,height], ...]`。samples 每筆提供唯一 id、rgb 與 expected_state。最多 256 個樣本；不連線或注入輸入。

PNG 的來源 hash 和人工標籤由 dataset producer 提供；C++ 工具只直接驗證 RGB bytes，不聲稱重新驗證 PNG 的解碼來源鏈。報告記錄 dataset hash、每個 template 的最差 anchor 誤差、結果與預期是否相同，固定 `live_qualified=false`。

## 本機證據（2026-10-23）

素材留在 repo 外 `../local-assets/phigros-4.0.1/state-calibration-001/`，不提交私人遊戲影像。

- 三個校正樣本：先前實際播放截圖、使用者暫停後的 gRPC 畫面、實際結算畫面。播放錨點採左上暫停圖示及右下難度文字；暫停採中央三個圖示；結算採靜態統計標籤，避免把零分或 584 Miss 當成必要條件。
- 六個未用於取樣的舊錄影畫面，涵蓋約 12、28、47、66、83、105 秒，全部辨識為 playing。最差 anchor 平均誤差約 1.55/255。
- 設定畫面回傳 unknown；三個另外取得的目前暫停畫面均回傳 paused。
- 合計 13 個樣本標籤吻合，其中 3 個是模板自身核對、10 個為獨立畫面。這是有限樣本回歸，沒有完整量測所有過場、遮擋或動畫的錯誤率。
- MSVC 19.50 / C++20 / `/W4 /WX` 編譯通過；15 個合成檢查涵蓋錯誤幾何、模糊狀態、區塊重疊、失去對比、資源上限與未匹配畫面。

gRPC metadata 的來源 timestamp 與主機 QPC 分開保存。主機 request/return bracket 並不自行證明 framebuffer 的 capture clock；這批畫面沒有被賦予 live timestamp qualification。未來餵給 `RunSupervisor` 的 `StateObservation` 必須來自實際新影格、帶其身份、序號、時鐘資格與期限；重讀舊 PNG 或 timer tick 不能刷新 evidence。
