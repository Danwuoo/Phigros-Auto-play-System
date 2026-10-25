# Credits AT 無點擊與實機接續

2026-10-25，使用者明確授權修復 Credits 無點擊，並使用 computer-use 操作幾首歌曲驗證。一般測試仍離線；這些另列的裝置回合使用原生單輪入口、實際人工式選曲／READY／Play，不自動 retry 或提交分數。每輪保存計畫、events、receipts、HUD metadata、停止／釋放及文字結算，沒有保存回合圖片。

## 無點擊原因與修正

舊005兩輪 Credits AT 的 profile／難度身份正確，但完全沒有 DOWN／UP。主 HUD 暫停圖示的364個參考白像素，在穩定實際影格只有350個仍達白色門檻，因此被原有98%規則拒絕。捕捉仍持續；不能把未注入說成遊戲漏接。

第一次候選只選 RGB>=224 的350個像素，實機仍只有336／350通過，0 DOWN；該失敗及封存 `catalog-native-at-core-001` 完整保留。先前把失效像素直接等同202灰底邊的推論不充分，未把這個候選稱為改善。

只讀gRPC取樣將RGB留在RAM，僅寫座標／雜湊／統計。3幀跨80.1166ms證實失效14個像素位於頂邊 y26、x24..30及39..45，由參考255變成148／150／151。3×3強白鄰域侵蝕1px後留下230個內部像素，三幀白色比例均1.0。v4主判讀與learned輔助判讀共用此核心；原98%、均誤差8、負空間、60ms雙觀測、500ms freshness、epoch／geometry／capacity與釋放規則保持。舊IN v1／v2／v3參考mask不變。

診斷階段在任兩個HUD錨點已出現時顯示「HUD未通過」，避免AT字樣與分數都在卻仍顯示請按Play；此變更不提供輸入許可。

## 曲中失敗仍保留

`catalog-native-at-core-002` 已實際取得首音，並出現96 Combo；但完整回合於285 DOWN／284 UP、1097 receipts後以 `actual_UI_receipt_expired` 停止，最後kernel zero。結算文字觀察為446708分、285／0／0／353、Max Combo285、44.67%。這不是整曲完成或AP。

最後有效seq4684後，29幀捕捉持續正常，pause核心、七位score及負空間通過，但AT prefix先出現第六個白色元件，後續額外白墨約26%，原筆畫核心仍完整。Learned模型已在首音前以68.5812ms兩觀測建立，也曾成功補充199幀；尾端因原政策只接受score遮擋而早退。其未執行的auxiliary預設欄位不代表實際曲名消失。500ms停止符合原guard，不能藉此放寬watchdog。額外白墨來源未由該回合保存像素證明。

## AT 的獨立完整錨點備援

新的候選只在已學成且已armed、相同source／epoch／geometry與非否決畫面中，處理v4 primary prefix字形失配。primary pause、原字樣核心／負空間及七位clean score必須仍完整；接著以**當下實際像素**另驗learned完整AT難度（含等級）、曲名、pause及七位分數，全部嚴格通過才提供current candidate。auxiliary缺失仍拒絕且留下診斷，未放寬主字形、auxiliary或freshness門檻。IN模型不能借用此AT路徑，未ready／未armed／缺筆畫／偽造primary／暫停／結算／context變動均另有負例。metadata新增auxiliary valid／candidate／score欄位，區分未執行與實際判讀失敗。

## 本機原件

本輪根目錄：repo外 `local-assets/phigros-4.0.1/credits-hud-core-local-20261025-190156/`。早期候選、失敗、RAM統計、各封存與後續回合分開，舊005、build006、AP、私人譜面／音源及四APK不覆寫。327首／389譜公共曲庫與LFS原件未重抓或改寫。最終版本與逐輪驗證見後續本輪索引。
