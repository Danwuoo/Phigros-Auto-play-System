# Glaciaxion IN：固定判定錨點的直線 Flick 候選

2026-10-25 使用者要求先改善 Flick 折返與抖動，時機保持不變，完成後開啟新面板供人工實測。此前[72→60→72三輪](FLICK_BAB_2026-10-25.md)的全部記錄保留；沒有從它們推導幅度優勝或已改善遊戲判定。

## 已確認的路徑問題與修改

舊 `dynamic-line-v1` 在 Flick 的整個滑動期間重新計算 line move／rotate，再做逐點 normal intersection。判定線運動可能抵消滑動或改變命令方向。72 px 計畫中，17 個 Flick 有方向折返、共28次，3個端點位移低於名義幅度的四分之一；主機 receipts 的座標與此計畫一致。這是命令幾何的問題線索，尚未確認它就是遊戲 Miss 的原因，也沒有 Android 實際運動量測。

新增明確啟用的 `judgment-anchored-straight-v1`：只在判定時刻計算一次投影錨點與 side normal，整段沿固定方向等速滑動，不再追隨中途 line pose，也不逐點投影。完整直線須落在既有 canvas／inset 矩形，否則拒絕整份計畫；不裁切、縮短或自動換回舊政策。參數、profile、compiled fingerprint、check/run及manifest均綁定policy，缺欄位仍用原dynamic政策。

實作保留原離散保持誤差預算、像素映射、容量與停止／釋放流程，另限制重複像素取樣的工作量。投影provenance只記判定錨點，不冒充整段動態line追蹤的證明。新政策仍為待實測的幾何候選，沒有取得遊戲或AP資格。數學契約與CLI見[Flick指南](FLICK_TUNING.md)。

## 真實原譜的離線逐項比對

使用原本授權的Phigros 4.0.1／157 Glaciaxion IN；chart SHA256仍為 `78a9227277d27de457d48e0e0802c23da7beaab0bbc1e0252274dd4c9f37ade9`。沒有重抓曲庫或用公共4.0.1.2取代本機原譜。

| 核對項目 | 舊72 px | 新直線72 px |
| --- | --- | --- |
| phase | −80 ms | 相同 |
| lead／travel／release | 30／60／24 ms | 相同 |
| Tap／Drag | 32／32 ms | 相同 |
| inset／projection／capacity | 64 px／D7000／5 | 相同 |
| contacts／Flick／非Flick | 729／51／678 | 相同 |
| events | 6277 | 6037 |
| Flick命令折返 | 28次 | **0次** |
| Flick短端點位移候選（<18 px） | 3個 | **0個** |
| 新端點位移範圍 | — | **71.253–72.250 input pixels** |

51個Flick的判定時刻必要MOVE座標逐一相同，DOWN／滑動末端／UP時間逐一相同；全部678個非Flick接點（含每個step的時間／座標／phase）完全相同。opening fingerprint與第一個DOWN也相同。內部MOVE取樣依新直線速度重新產生，沒有宣稱每個內部MOVE的deadline與舊路徑相同。

新binary重新編譯舊dynamic政策時，實際重現原fingerprint `8119a9dd305f8193b7b2c332af9c7036c2d10f7292eb61ad2f66b92b78910a17`；新直線fingerprint為 `6e16c82366fb113e22f49806f3b26ce6a6326ffb299cf2bb8500431b9c58c307`。沒有在舊profile上直接改policy並沿用舊hash。

## 新建置、封存與離線驗證

- 程式提交：`03bb467d7f22a98d23f6fe2e01bdee715c3b28e7`。新build為task外部 `work/catalog-native-build-002`，VS18／MSVC19.50、Release x64，使用既有patched gRPC prefix與toolchain，沒有重建第三方gRPC或改保留PAS sources。
- 新封存：`local-assets/phigros-4.0.1/catalog-native-build-004/`，196份固定產物，archive建立時HEAD與來源提交一致，工作樹乾淨。003、舊build006與AP封存不覆寫。
- 新 `chart_experiment.exe` SHA256：`9459933625dae69a5ccdebd11addf2b6c91bd789eb278317f9cd11f01b876063`。
- archive manifest SHA256：`7167f284a41d0dc7648b300372305b41242916fcd89b0d742eab15d78396d5f2`。
- 新native **29/29 CTest**；Flick policy **199 checks**，包含移動／旋轉線、above／below、四種input rotation、判定錨點與關鍵時間不變、非Flick不變、連續held-error抽樣、越界拒絕與重複像素取樣工作量上限。
- 完整Python **232項：231通過、1項Windows symlink權限skip**。初次完整執行有一個既有catalog測試的暫存檔WinError32清理錯誤；該組17項重跑及完整232項重跑均通過，初次失敗日誌保留，沒有改與此無關的程式來掩蓋失敗。
- portable configure／Release build／CTest **4/4**；面板離線SelfTest **52 checks**。新004封存下21首native offline preflight全部通過，包含新Glaciaxion profile；GUI完整 **324個pins**核對通過，皆無裝置存取或觸控。另行唯讀檢查已知AVD的boot未變，沒有自動重啟或建立五指新結果。

## 新人工面板與證據邊界

本機新準備根目錄為repository外 `local-assets/phigros-4.0.1/flick-straight-local-20261025-171452/`。保留舊準備與失敗稽核產物；正式使用 `straight-72-final/song-profile.json`、`offline-path-audit-final.json`、`panel-straight-72/launch-config.json`，快捷入口為 `open-straight-72.cmd`。新profile SHA256為 `fa39a2132eee766fb4d3d0e78aecd37c81d1129077353d424a5fed5e7d5869a8`，launch config SHA256為 `3841afeb19ccaf0d6b449ffb6321b8f6df7c74b107eebd7b7bd3213682632a16`。

新面板只預選 **Glaciaxion｜直線72 px**。原21首清單與分類保留，其餘profile仍採舊dynamic政策；新政策沒有成為所有歌曲的預設。使用者先在遊戲選IN，再操作面板啟動本首，等READY後自行按Play；不自動選曲、不自動重試。既有停止按鈕／STOP／release-all與boot／容量／來源／幾何／freshness guard保留。

一般回合仍 `image_retention=none`：每輪新目錄保留compiled plan、receipts、events、metadata-only HUD、manifest及結算frame sequence／receipt time／RGB SHA，不保存PNG／RGB／HUD灰階。結算數字由使用者文字提供，不作數字OCR或自動AP判定；即使不存圖片，文字記錄仍占容量。

**新004歌曲實測在交付時為NOT RUN**；折返歸零與端點位移是離線命令座標結果，沒有宣稱實機抖動、Flick Miss或AP已改善。開面板不自動開始native輸入；後續所有完整或中止回合都須保留，結果另存sidecar，不改原manifest。
