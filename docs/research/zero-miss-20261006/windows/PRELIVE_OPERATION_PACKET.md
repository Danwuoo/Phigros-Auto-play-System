# current-rails-v1 有限實機前操作包

本文件只準備審查；device read、Fixture、emulator、遊戲及 touch 都沒有執行。
實際交付 `current_chain.exe` 是 offline-only entry，只有 WIC／memory pixels 與
ReplayBackend；完整 typed hook 在正式 observer、lifecycle、SessionGameOwner、scheduler
之前作用。它不接受 endpoint，也没有接入原 `pas manual-session` CLI。
不能將舊 donor pas.exe 的觸控視為此候選結果。成本或 physical gate 未過時保持 NOT_READY；
啟用任何真裝置入口必須另有明確授權、相應 source/binary freeze 及獨立資格驗證。

## 冷準備與歷史指紋

exact source／binary／lib／DLL／flags／輸入／receipt 見同目錄
`prelive-evidence-01/closure.json` 與 `candidate-manifest.json`。正式變更是 typed
current contact 與自身 execution guard，原 v3 七份 core／oracle 沒有修改。
全分母結果見 PRELIVE_RESULTS；沒有使用歌曲名稱、ordinal、舊 touches 或本表來決定動作。

|項目|離線已核值|當前資格|
|---|---|---|
|config|`configs/phigros-hd-assist-five-lead35.json` SHA d2b5cfdc6be023c715fe402a7124ccd1977f5956b8d9fa74466643a575dbd0e0|檔案指紋；不是當前裝置 preflight|
|capture|gRPC payload fast、RGB888 top-down、1280×720、rotation1、256KiB|本包 memory／PNG stub；真capture待驗|
|touch|720×1280、rotation90、max contacts5、100ms timeout|本包五slot ReplayBackend；真 mapping 待驗|
|scheduler／policy|plans128、steps16、horizon350ms、evidence100ms；types15、lead35ms、uncertainty30ms|各計畫獨立 expiry、QPC、單 owner|
|歷史 serial|`emulator-5554`|historical only，當前未再驗；執行前由使用者指定／確認|
|歷史 capability|SHA 73dc5c94f1418a1e75bf4488871d27c849e0d6105adc79ec03c167ff672dbcd3|原檔只讀已核；當前 app／geometry／mapping unknown|
|歷史 Fixture APK|SHA b3bfaf4e4e876e7833c9af1bd6149b40cdc9fa46b67646d52ea045dde6705dc6|沒有重裝、啟動或 Fixture touch|

capability 原路徑：原根 `measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p/capability-reference.json`。
四 corner mapping、來源 domain、config／geometry／APK／五 contact 要比對同一份新 preflight；
任何不符均先停止。歷史來源不證明當前 emulator 存在、遊戲版本或解鎖狀態。

## 待授權指令及 gate

以下是原 donor CLI 中已查到的語法模板，尚未綁定可執行的裝置候選；佔位符不得直接執行。
先需要指定 serial、目前版本及獨立簽收 source/binary/environment，不能直接選取 PATH 中的 pas.exe。

|指令模板|目的／風險|必要前提與停止條件|有限額度|
|---|---|---|---|
|`<qualified-donor-pas> probe --serial <confirmed-serial>`|讀裝置 inventory，可能連 ADB|明確 device-read 授權；serial不符、未知回應、程序 guard拒絕即停|一次read、30s、metadata1MiB|
|`<qualified-donor-pas> game-preflight --config <confirmed-profile> --capability <verified-capability>`|讀目前capture geometry並比歷史mapping；不是候選實戰|先核binary/profile/capability SHA，真capture fingerprint不符即停|一次preflight、60s、metadata8MiB|
|Fixture touch|驗五contact、orientation、corner、unknown／release路徑|另行Fixture與touch授權；先核exact CLI/source/APK及目前geometry；不能以冷fixture代替Phigros語義|一個有限case/session；budget先凍結，不自動追加|
|current-rails manual-session|觀察目前合法接入／body／tail及結果|**目前無此 live binary／CLI 命令**。需成本、physical語義、driver資格及另行runtime endpoint接線／freeze；舊donor `manual-session`不滿足|未准執行，零輪|

原 CLI 的 `manual-session --config … --capability … --one-round --round-watchdog-s …`
語法已核，但只屬 donor。不得加上不存在的 candidate flag，或用不同版本最佳分數拼成驗收。
沒有 auto-start／自選曲／連打／失敗 Down重試。遊戲一定由使用者選曲及按 Play，一次一輪。

開始前核新資料容量及20GiB最低free，PNG／raw另訂單輪硬額；延用本包≤12GiB新OUT、
≤64MiB metadata只是冷準備額度，不能自動借給新live campaign。
停止：Escape／Ctrl+C／watchdog、gate過期、geometry/context/revision不符、capacity超限、
critical writer滿、RPC failed/unknown、線／contact證據不足或結果到達。
停止後核 requested／failed／unknown release及 backend剩餘contact；unknown結果保存並停，
不重試Down、不用第二次release成功覆蓋第一次unknown。未核實釋放時不開始下一輪。

五 contact checklist：只由本owner成功receipt建立；不借歷史Down；每次Move保持同contact並修訂
當前body交區；head、body、tail各自證據；completed/cancelled/unknown/retired不復活；第六contact拒絕。

## Chapter Legacy 結果格式

同目錄 `PRELIVE_RESULT_TEMPLATE.json` 是外部採證模板；目前章節分母、曲目與IN解鎖均unknown，
不能把歷史20曲名單當目前roster，也不能由HD分數推IN。
第一次有授權時逐屏取得目前 Chapter Legacy 完整名單／IN鎖定狀態，保留UI來源、app版本與SHA。
逐曲只接受同一凍結候選完整IN結算的Miss=0，P/G/B／score照報，HD只供解鎖／回歸，不增AP條件。
沒有每曲三連勝或其他新增驗收前置。任何缺結算、版本不符、人工觸控混入或raw不完整都保持unknown／not accepted。

剩餘最小外部證據：1519 candidate4/5物理identity；目前合法Hold接入點／body續接／tail與遊戲採納；
當前profile/serial/app fingerprint、真capture/RPC延遲、完整IN解鎖分母與結算。這些不能由合成或舊按鍵裁決。
