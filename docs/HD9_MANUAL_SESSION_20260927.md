# HD9 音符策略＋多輪手動待命

本分支 `codex/hd9-manual-rounds` 從最高完整分数的 `1636519b9e3ddd472fad1d7494f5bc7525cc5fc1` 開始。main／origin/main `5ad759e` 與全部歷史分支、原始量測保留，未 reset main、merge 或 push。本次新授權暫停學習式模型方案；完成有限跨曲 HD 比較條件，不恢復舊的無限 Glaciaxion AP goal，不進 IN。

## 比較基準與界線

第九輪 Glaciaxion HD6：868880分，369 Perfect／2 Good／0 Bad／22 Miss，393判定，maxcombo82，observer33／planner13；五指、lead35ms、uncertainty30ms。原 run 為 `measurements/game-assist/cpp-observe-17905107612608769`，manifest 的原 executable SHA256 為 `f25d4774cc29c3c8fef029ed1f92535801f946879f800070c1e1fbebbbea0ca4`。

歷史第九輪 executable 沒有另存於現有 out／measurements 的 binary 清單，不能宣稱已找回完全相同 binary。另在未修改策略前，以該source重建到 `out/hd9-original-release/Release/pas.exe`，hash `e792272e98e63ba557f1c114f789f18437071f7778ec967e5937674cc99b79b8`；這是**重建副本**，不是歷史原binary。既有 main binary、重建副本與新 `out/hd9-session-release` 分開，未覆寫歷史資料。

`src/game.cpp`、`src/game_motion.cpp`、`src/game_tracking.cpp`、`include/pas/game.hpp`、`src/core.cpp` 等音符辨識、預測、planner／scheduler與原input檔案相對1636519無差異。沒有整包帶入observer34–36／planner14–18。已知HD9的pending Hold Down改早可能平移Up、未Down missing取消後submitted不重建、斜線／時間殘差限制均保留，屬比較版本限制，不能稱main最新策略或跨曲AP能力。

差異只在新生命周期與紀錄：手動開始、首個HUD後清空observer並重新確認（可能比原初始HUD多一幀）、來源／HUD失效撤銷保留本輪完成身分、新輪才完整reset、正面結算辨識、單次stop/cancel保留首份release report、有界分段writer。源時鐘仍unknown，host QPC一致。實戰比較必須同時呈現畫面間隔分布，不能只用分數歸因於策略。

## 使用

```powershell
out/hd9-session-release/Release/pas.exe manual-session `
  --config configs/phigros-hd-assist-five-lead35.json `
  --capability measurements/game-semantics-20260927/touch-five/summary.json --no-preview
```

啟動先核對實際裝置／映射／五指capability，再持續STANDBY；使用者自行選其他HD、按Play。從當前pause bars／score glyphs形成STARTING，再以新observer三幀HUD gate確認PLAYING；不讀譜面、記憶體、遊戲內部狀態、滑鼠hook、歌曲時間或已錄按鍵。不自動按Play、選曲、retry、continue或結算箭頭。Escape／Ctrl+C可隨時停止。

單程序可連續多輪。RESULT須同一幾何下三個不同、新鮮frame，跨度≥60ms，當前六個固定英文UI字形（Perfect／Good／Bad／Miss／Max Combo／Accuracy）全部相符，且沒有playing HUD／menu。字形模板只保留文字mask、不是分數／曲名／譜面；來自既有result3 PNG，±3px平移、Jaccard≥0.72，限1280×720英文布局。首個正面result候選即停止新Down；writer在owner釋放之後保存一張同capture結算圖供判定與比例人工核對。

黑屏、loading、pause、HUD消失或沒有音符均不能當RESULT。未知畫面停止注入但保留本輪身分；HUD恢復不能洗掉completed tombstone。真正結算後回STANDBY，需要正面選曲頁，或先離開結算到非HUD轉場後再確認新HUD，才能開新輪。若未確認結算便離開遊戲，尚不自動宣稱完整成績；可Escape停止並記aborted。

待命沒有60秒timeout，曲長不綁185秒。`--round-watchdog-s 0`預設關閉；有指定時異常單輪超時aborted／FAULT，絕非曲尾。尺寸／旋轉／generation變動、stream/input未知結果、release未知、journal故障均FAULT，保留既有emergency release；不能自動重建觸控backend續玩。

## 有界資源與證據

- 固定gRPC payload fast／RGB888 top-down／256KiB、1280×720 rotation1；capture容量1、三個物理buffer。單一perception／action latest packet；沒有第二capture或逐幀PNG。
- archive獨立writer；mailbox最多8192項且JSON serialized bytes≤16MiB，另最多一張待編碼結算圖。結算frame是有界copy，不持有capture pool lease；慢writer／overrun會FAULT，不阻塞owner等待磁碟。
- 每輪獨立round ID／epoch／owner、manifest、events、summary與hash。events分段16MiB、每輪最多32段，超額FAULT；四個統計vector各最多100000筆，完成後清空。percentiles是該輪**action owner收到的最新decisions**，包含跳過frame的真實間隔，原始事件可核對，不能冒稱全部callback分布。
- STANDBY不記逐幀decision；只記state及每60秒health，日誌1MiB×4輪替。round index及摘要逐輪落盤，不在RAM保留歷史列表。
- 禁止dataset採樣／tracking shadow；原20run訓練資料額度不變，沒有换root繞過額度。有限result PNG只作遊戲成績證據。manifest內含compiled source hashes、Git build commit、binary/config/capability hash、capture設定、clock domain、停止原因與release結果。

## 離線驗證

短測使用確定性pixels與FakeClock，包括100000次長待命、手動HUD開始、短曲及400秒長曲、RESULT latch、黑屏／loading／pause負例、fresh／duplicate／epoch／geometry、活動Hold停止釋放、HUD loss不重播、兩輪新owner、20輪archive與待命輪替。

`tests/data/hd9-strategy-golden.json`由獨立original build的 `pas_core.lib` 產生：六場景×18frame＝108個完整decision、plans與receipts。原音符檔案byte-identical，SessionGameOwner以相同pixels／FakeClock逐項相等。這验证策略沒有悄悄升級；不表示新生命周期與歷史原binary完全相同，也不是遊戲命中真值。

重現golden可在原commit獨立build後，以 `-DPAS_HD9_TRACE_BASELINE_LIBRARY=<original Release/pas_core.lib>` configure新build，執行 `pas_hd9_original_trace`。目前golden由原source static library實際產生，沒有以新策略自我生成oracle。

最後source `5b3375d` 的Release／Debug／嚴格ASan均 **179／179**，分別17.75／47.79／97.03秒；新增16項session測試。三配置各16份compiled source hash與目前檔案一致，差異0。既有result正例18張、menu負例4張通過。Release binary SHA256 `483cb83887d6daad9f01569aaca8f06c40e4e91df98f393b16a9c713c2534b76`，明確標為HD9音符策略＋新待命流程。完整log、hash與失敗迭代見[結構化證據](HD9_MANUAL_SESSION_EVIDENCE_20260927.json)。驗證完成本機日期2026-09-28；ASan使用halt／abort、無suppression，第三方DLL未插樁的限制保持。沒有重跑capture選型矩陣。

新build各自使用 `cmake --preset windows-release|windows-debug|windows-asan -B out/hd9-session-release|debug|asan`。ASan linker LIBPATH另指向新asan輸出目錄的 `asan_support/lib`；build `--config Release|Debug --parallel 4`，CTest `--test-dir <對應build> -C Release|Debug --output-on-failure`。三套binary在docs整理時build，build commit為5b3375d、dirty=true（文件尚未提交），以compiled hashes精確核對最後程式來源。

## 實戰流程與未驗收項

離線完成後才請使用者準備模擬器／其他HD。就緒前不啟動遊玩；遊玩期間不build/test、不操作CU、不另開capture。使用者手動選曲／Play，有限輪次，保持五指／35ms等參數；每曲記判定總數、Perfect／Good／Bad／Miss及比例、score、source／binary與畫面間隔。數字只從正面結算pixels讀取，未知保留unknown，不以RPC success或candidate數推斷。

2026-09-28 已完成真模擬器連續多輪實戰：同一策略Release binary兩個有遊玩的session共20張確認結算圖、19個不同曲名；另有一輪擷取故障中止、一個只有待命的session。Credits完整重跑一次，-SURREALISM-有使用者回報的卡住重來（未保存失敗嘗試的獨立結算）。數字、原圖索引、每輪時序與限制見[Legacy HD 實戰紀錄](HD9_LEGACY_HD_RESULTS_20260928.md)及[結構化證據](HD9_LEGACY_HD_RESULTS_EVIDENCE_20260928.json)。這證實同程序跨輪待命與跨曲結果蒐集，不表示音符策略All Perfect或未知中斷可歸因於模型／單一bug；不無限跑到AP。
