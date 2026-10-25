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

## 最終封存與全部回合

程式提交 `40660a0667dbe3bc0c79751d311cdd9a1e8d8916`；最終獨立封存 `catalog-native-at-core-003/`，196份固定產物，binary SHA256 `18284d1df201ce1386d88602198abefa71723e6df3b128e1c5c435d1fa13a263`。沿用既有patched gRPC，不重建或替換第三方來源。Windows native29/29、portable configure/build/CTest4/4、learned HUD264 checks、glyph172 checks與面板SelfTest52 checks通過。首音、phase、Flick及既有profile／compiled fingerprint保持。

以下 Perfect／Good／Bad／Miss 由 computer-use 暫存結算視圖獨立讀取，另寫文字sidecar；一般回合圖片0張。raw manifest的unreviewed／AP false不改寫。

| 本輪序號／封存 | 曲目 | 分數與判定 | 原生結果 |
| --- | --- | --- | --- |
| 01／core001 | Credits AT | 0，0／0／0／638；停止後僅只讀續播診斷 | 0 DOWN，user_stop，未完成 |
| 02／core002 | Credits AT | 446708，285／0／0／353 | 285 DOWN／284 UP，HUD逾期停止 |
| 03／core003 | Credits AT | **1000000，638／0／0／0，本輪AP** | 638／638，2530 receipts，完整結算 |
| 04／core003 | Credits IN | **1000000，535／0／0／0，本輪AP** | 535／535，1780 receipts，完整結算 |
| 05／core003 | Glaciaxion IN／直線72px | 999568，728／1／0／0，Early1 | 729／729，6037 receipts，完整結算；不是AP |
| 06／core003 | Dlyrotz IN | 無結算，暫停離開；不補造數字 | 0 DOWN，first_note_observation_budget |
| 07／core003 | Eradication Catastrophe IN | **1000000，593／0／0／0，本輪AP** | 593／593，3455 receipts，完整結算 |
| 08／core003 | Credits AT重跑 | 999506，637／1／0／0，Early1 | 638／638，2530 receipts，完整結算；AP未重現 |

最終binary的6輪中5輪完整，涵蓋3首／4份IN或AT；同設定Credits AT兩輪均全曲完成、無Miss，其中一次AP、一次Good Early。不能由此宣稱全庫穩定、穩定AP或Flick因果改善，也未改時機。Dlyrotz的513候選全在PLAYING後產生，70幀有首音positive，卻因原譜高速／負速source／time別名保留多分支而保守拒絕。沒有增加預算強行起跑，既有AP資格不帶入這輪。

`final-live-independent-audit.json`獨立核對五輪 **16332／16332** 個完整steps的scheduled時間、phase、pixel、pointer ownership均吻合，沒有漏步／多步；全部8輪release known、kernel zero、實際0圖片。兩AT新prefix fallback分別141／152個CURRENT實際觀測，其中30／32連續幀跨過舊44秒失效段，並未延長500ms。journal各小於64MiB／16000 rows，HUD容量無drop。首次launch helper參數shadow與Python UTF-8準備錯誤均在native啟動前發生，另存setup紀錄，沒有漏列裝置回合。

## 最終人工入口與保留

最終設定是本輪根下 `panel-final/launch-config.json`，SHA256 `0a4866754d29973d6a8b174a6477e5959496fe748421b77f8f82722de63c09be`。完整389份仍可見；262份可進入逐首前檢，126份原跳過加Dlyrotz這份實測開場待修，共127份列明原因。預選Credits AT。1158個pins全部核對；新視窗實際有非零handle／正確標題，桌面Test Panel／Song Library均改指向新003封存與設定，舊快捷方式另存備份。操作仍為選曲、啟動本首、READY後Play；本次明示授權的computer-use驗證已結束，沒有native owner等待或自動開始下一輪。

舊8798份私人原件的存在／大小／mtime及369個原身份SHA、舊005manifest與兩輪ledger額外3個SHA全部不變；13個保留PAS source Git blobs及proto／third_party tree保持抽取身份。origin/main仍為 `2d479e17fa1566c089dce019cbb11eb883ba47fd`。本輪不動main、不force push、不提交私人RGB、譜面、音源或校正。
