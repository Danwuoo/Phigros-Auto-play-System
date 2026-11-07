# 桌面短斜向 Flick 調整至48px（未實測）

使用者回報 Phigros 仍有 Flick 漏判，要求稍微加大短斜向幅度。本輪將距離從32調為48 reference pixels，增加50%；保留 `judgment-anchored-diagonal-snap-v1` 路徑邏輯，以及提前8ms、20ms滑動、末端6ms釋放的時間。這是候選參數調整，沒有新遊戲判定結果或漏判改善宣稱。

既有 native012 已支援此參數範圍，因此不改原生程式或重新編譯；先前等待／演奏分開計時、HUD聯合遮擋分派與面板大型manifest修復均沿用。因距離包含在完整編譯 fingerprint 中，即使沒有Flick的曲目也需要新的配套profile／fingerprint；本輪透過原生 `prepare-adaptive` 製作完整374份曲目資料，不直接修改舊profile中的距離或hash。

保留所有原譜、來源ID、音符／事件數、非Flick接點、全曲時序、資源限制、十指容量及−40ms相位。較大的Flick仍由同一有界矩形／64px內縮規則選擇完整路徑，因此允許其anchor、方向、bend／end／UP座標及相應證書重新選取。15份含blockAreaList的原項目不變。

新曲庫另存 `local/library-desktop-flick48-002`，完整寫入後才切換桌面設定；舊32px曲庫與設定備份保留。首份001輸出因舊報告缺少三個新版診斷欄位而停止，部分輸出及失敗紀錄原樣保留，沒有安裝。002允許這三個由原譜推導的額外欄位，保留原生開場model SHA、原有欄位與earliest IDs的嚴格一致性。人工選曲、READY後自行按遊戲Play、正常遊玩只存結算圖等流程維持。

依既有使用者要求，本輪不執行測試套件、`check-adaptive`、GUI、ADB、模擬器或實際觸控。`prepare-adaptive` 僅為安裝資料製作，不是遊戲測試。效果由使用者實測；先前五首隨機驗證仍未完成。
