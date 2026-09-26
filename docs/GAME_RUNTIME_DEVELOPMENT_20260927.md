# 遊戲 runtime 開發與實測紀錄（2026-09-27）

狀態：第一批 runtime／scheduler、遊戲 observer 與自動 PLAY 已接入；後續 G2 assist 有真實命中，四類完整試驗最佳結算 Perfect 302／Good 8／Bad 0／Miss 83。G1 全項、G3 所需能力與 HD／IN AP 尚未驗收。下列每批的版本、故障與結果分開保留，不以新修正回填舊 run。

## 規劃與工作區

本工作樹起點 `4419ae6`。依交接先核對來源 `C:/Users/wurre/Desktop/Phigros-Auto-play-System` 的七份規劃 hash，再同步 AGENTS、README、ARCHITECTURE、ROADMAP、新主程式計畫、機制研究與原計畫存檔；來源工作區未寫入。採 G0–G6 與 Glaciaxion HD → 首次 AP → IN，不恢復五擷取矩陣或大型 Fixture 前置階段。

新輸出位於本工作樹 `out/game-release/Release/`。使用 VS 附帶 CMake、MSVC 19.51／v145、Windows SDK 10.0.26100.0，唯讀沿用來源的 pinned `out/vcpkg_installed`，`VCPKG_MANIFEST_INSTALL=OFF`；未執行來源 binary 代替新建置。建置及測試日誌在本工作樹 ignored 的 `measurements/g0-preflight/`。依賴警告與原有分析器數值轉換警告仍存在。

## 實作與契約

- `src/runtime.cpp` 抽離 CLI runtime。capture、perception、action／input owner、journal 各有專用執行緒；主執行緒處理 supervisor／preview。frame 最新槽容量一、三個 capture buffers；decision 最新槽容量一。preview 使用配對的 pixels／scene 副本，不把不同 frame 的預測畫在一起，也不持有 capture lease。
- profile 新增 `capture.max_relative_lag_ms`，整數 1–1000，省略時 250。public config／manifest 記錄有效值；`runtime_capture_options()` 真正傳入 gRPC。主用仍是 payload fast／RGB888／top-down／256 KiB。
- source 超時、相對 lag drop、UI gate 丟失、epoch／generation／geometry 改變會撤銷。owner 不等待健康 probe／preview；dispatch guard 再查 supervisor 版本與來源期限。時間一律 QPC；Android 絕對 source age 為 unknown。
- scheduler 在 down 時才分配 contact。新提交 intent ID 與 Note ID／contact ID 分離，按接受順序配置；active revision 必須保留已執行 prefix，不能重播 move。單目標過期只釋放該 contact；全局 gate／input unknown 撤銷全部。相同期限先 up，再 move，再 down。拒絕事件有界保存並由 owner 排空。
- observer 最多 16 條候選線、128 個 track、每 track 8 個座標點、2048 個連通元件；不保存歷史 frame。全畫面幾何／色彩候選，有限近期一對一配對、相對線法向距離的短線性擬合，輸出殘差、表觀撞線區間與拒絕理由。350 ms 預測 horizon、100 ms target evidence、三張 HUD 確認都是開發参数，未當作遊戲判定窗。
- 首輪 HD 暴露一像素水平線與 Hold 邊框相連的漏線，以及黃鍵內外色帶分裂。新版本增加逐 row 的長細線候選，合併有明確內外包覆證據的不同寬色帶，再作全局一對一配對；相同寬度的重疊候選不因接近而合併。任意旋轉／隱藏線與重疊身分仍未完整驗證。
- `MENU` 由目前選曲頁的難度白底區與 PLAY 白底圖形確認；`PLAYING` 由獨立 pause／score HUD 確認。其他 UI 尚維持 `UNKNOWN`，不可宣稱完整辨認 PAUSED／RESULT。UNKNOWN 一律不具遊玩資格。
- `observe` 不建立真實 input。`auto-start` 是使用者新增授權的 UI-only 路徑：指紋匹配且即時 MENU／PLAY 證據成立後，只送一次 down/up；離開該頁、停止或失效仍保留釋放責任。它不向遊戲 Note 注入觸控；`assist` 仍拒絕。未知 input 不重試 down。
- `analyze game` 由 C++ 串流重算候選、拒絕、預測、處理耗時與 dry 同期限下壓偏差；各分布最多 100,000 樣本。候選出現次數不等於音符數，無法唯一配對的逐 Note 回饋為 unknown。batch 尚未接入共通 scheduler，沒有多押 batch 性能宣告。

## 當前環境與歷史能力核對

原本無執行中 Emulator，已按既有方式啟動 `phigros`，不改 AVD／遊戲設定。實際序號 `emulator-5554`；guest online processors 5，MemTotal 8,130,816 KiB；gRPC 1280×720、source rotation 1；Android 實體觸控基準 720×1280、90° 映射。遊戲啟動畫面顯示 3.20.0；使用者選定 Glaciaxion HD Lv.6。

`game-preflight` 實際取即時 gRPC pixels 並核對 serial、Android／ABI／型號、density、geometry、mapping、contact capacity 與已安裝 Native Touch Fixture APK hash。`measurements/g0-preflight/current-fingerprint.json` 與歷史 `touch-cpp-full-run3/summary.json` 匹配；能力報告 SHA-256 `315d738cf2917da84fdbf60afbc2e6afe7e69615c2055c9f01c56d74fb7246da`，APK SHA-256 `b3bfaf4e4e876e7833c9af1bd6149b40cdc9fa46b67646d52ea045dde6705dc6`。第一、二輪歷史報告分別有 Flick 可見路徑失敗與排程失敗，未改寫或刪除。

當前角落映射為 (0,0)→(719,0)、(1279,0)→(719,1279)、(0,719)→(0,0)、(1279,719)→(0,1279)。這是當前指紋／映射核對，並非本輪重跑六類能力，也不證明遊戲語義。

## 首輪實機觀察（改善前）

使用者先手動選曲，本輪曾以 computer-use 按 PLAY；使用者隨後明確要求停止 computer-use、改由程式自行 PLAY。此後不再使用 computer-use。這次 UI 操作與程式 Note 注入分開；程式真實遊玩觸控為零。

首 run：`cpp-observe-17904384543391553`，180 秒、9,615 個消費 frame、0 pool drops／consumer skips、104 個 dry 命令、0 真實 input、0 relative stale drops。實際 executable SHA-256 `35394835d04bbee47b62f66e4b62923e4669d7fbc3a36e90db65ba3fc2bcf4f7`。來源到達間隔 p50／p95／p99／max = 17.280／38.180／51.003／576.718 ms；處理完的 host residency = 2.315／3.816／7.883／26.452 ms；recognition = 1.146／1.930／3.977／18.538 ms。這是該 Release／5 核 8 GB／1280×720 環境的單批分布，並非 Android render-to-touch 延遲。

另有 30 秒尾段 `cpp-observe-17904386898974840`，1,470 frames，保留獨立摘要。兩 run 之間有觀察中斷，不宣稱一份不中斷的全曲 G1 驗收；沒有 AP 證據。預設沒有逐幀 PNG。首輪原始辨識保留，不因修正細線／配對就把舊 run 改標為新版本通過。

## 下一個實戰步驟

只在相應視覺、動作、停止／過期回歸與固定試驗參數成立後接 G2；仍須補暫停／恢復、Hold 頭尾、Drag／Flick 動作與重疊／線運動證據。G3 全曲、G4 HD AP、G5 IN／AP 不能由候選或 dry-run 代替。

## 程式自動 PLAY 實測（observer v2）

使用者回覆已就緒後，執行新工作樹 `pas run --config configs/phigros-hd-auto-start.json --mode auto-start --capability <歷史 run3 summary.json> --duration-s 300`，未使用 computer-use。run `cpp-observe-17904396418549672`，executable SHA-256 `ead592354e3181748485c68c49337d5560e87712ea13d67e990adb5037d37ac5`。能力指紋匹配後，frame 3 的 MENU／PLAY pixels 導出 (1204.201, 623.434)。只有兩次真實 UI 命令，沒有真實 Note 命令或手動遊玩觸控。

同一 QPC 時域的 PLAY down：scheduled 129521727624600、inject start 129521727816500、return 129521740540300 ns；up：scheduled 129521747624600、start 129521751310300、return 129521752555800 ns。RPC 返回成功不當作可見 effect；後續 PLAYING 畫面及本次結算圖另核對。兩個樣本不作 transport p99 性能宣告。停止時 release report 的 requested／failed／unknown ID 均空。

300 秒正常 STOPPED；16,097 消費 frame／16,222 發布 frame、125 latest overwrite／consumer skip、0 pool drop、0 relative stale drop、80 dry 命令。分布 n=16,096 的 capture interval p50／p95／p99／max = 17.169／39.468／52.691／382.445 ms；n=16,097 的 host residency = 6.498／10.562／13.779／34.329 ms；recognition = 5.093／7.910／10.068／30.124 ms。v2 增加全解析度細線 scan 後處理成本上升；此單批不能作跨版本正式性能結論，絕對來源 age 仍 unknown。

原始 JSONL SHA-256 `1bf976fac60beb9a28d5015e7d98c70f50e94cc4d2f5eddac429b00300646352`，C++ 重算摘要 `measurements/g0-preflight/auto-start-game-analysis.json`。MENU 23、PLAYING 9,034、UNKNOWN 7,040 frames；多線候選 1,799 frames，3,302 個表觀預測出現（均不是命中數）。未知場景仍可能產生圖像色塊候選，包括結算封面；總候選次數不能用來盤點真實譜面。三次 runtime revoke 為兩次 source evidence 過期、一次 UI gate 丟失；summary 第四次包含停止，沒有 scheduler rejection。

停止後僅保留一張獨立 gRPC 診斷圖 `measurements/g0-preflight/auto-start-result/diagnostic.png`。人工核對：Glaciaxion HD Lv.6、0 分、F、0 max combo、0.00% accuracy、Perfect 0／Good 0／Bad 0／Miss 393。符合曲中不注入觸控的行為，沒有 AP。程式收尾分類仍 UNKNOWN；這是人工結果核對，不是自動 RESULT 辨識通過。此 run 連續覆蓋開局至結算，但暫停／恢復與人工預測樣本仍未補齊，G1 不宣稱完成。

## 修正後回歸（observer v3）

v2 實戰暴露遮擋造成同一薄水平 band 分成 5 條候選。v3 以像素共線支持合併整個薄 band，不同高度的 band 保持分開；另修正晚到的舊 geometry 快照不可先撤銷當前有效意圖。新增兩個可重現負例，Release 建置完成、CTest 50／50 通過（`measurements/g0-preflight/build-observer-v3.log`、`ctest-observer-v3.log`）。此前 47／48 的 Hold 邊框誤線失敗、修正後 48／48 日誌均保留，未改寫。

本輪新碼未做 Debug／ASan 驗證，不借用歷史 T0–T5 sanitizer 通過結果。不以 v3 回歸或預期改善重標 v2 原始資料。

使用者再回到選曲頁後，v3 run `cpp-observe-17904400689620009` 由程式自動 PLAY，executable SHA-256 `5bae6e4748feb11de8a9c94d77a0011213cc8b6f901ab6ad92702ca549b7397e`。210 秒正常 STOPPED、11,742 消費／11,818 發布 frames、76 latest overwrite／consumer skips、0 pool drop、0 relative stale drop、114 dry 命令、兩個成功返回的真實 PLAY 命令、無真實曲中觸控。收尾 release report 無 requested／failed／unknown ID。runtime revoke 五次（三次 source 過期、兩次 UI gate 丟失），summary 第六次含停止；scheduler rejection 為零。

capture interval（n=11,741）p50／p95／p99／max = 16.731／36.395／49.616／316.038 ms；host residency（n=11,742）= 6.128／9.805／12.028／25.374 ms；recognition（n=11,742）= 4.825／7.296／9.168／23.974 ms。期間有約一秒的第二 gRPC 診斷 stream，可能擾動延遲；不作正式跨版本性能比較。

C++ 重算 `measurements/g0-preflight/auto-start-v3-game-analysis.json`：MENU 20／PLAYING 9,439／UNKNOWN 2,283 frames；多線候選 frames 186（v2 為 1,799），表觀預測出現 4,729（v2 為 3,302）。兩輪 duration 與採樣數不同；只支持遮擋造成候選重複的改善，沒有逐 Note 準確率或命中率。v3 raw SHA-256 `985c88ffd69d7dcbb8f2d5c1753bfc22ef82cb64995245fa40a3f3057b3bb183`。

限量診斷圖：曲中 `measurements/g0-preflight/observer-v3-live-sample/diagnostic.png`（SHA-256 `961e49366b75df8a2f8353fb5c45cbdc9bb62b5be6e08566fe87c8ace4d5de52`）及停止後 `auto-start-v3-result/diagnostic.png`。曲中圖片含水平細線 y≈576、垂直線 x≈640、Tap／Hold 的黃色外框；鄰近 QPC 130074020248900／130074040376400 ns 的 observer 決策將水平線輸出為 y=575.5，色帶仍被分成 Drag／Tap，垂直相交線的身分也未可靠處理。圖像與決策不是同一 frame，只能作近時刻幾何人工核對，不能當作精確撞線真值。

v3 結算人工核對仍為 Glaciaxion HD Lv.6、0 分、F、Perfect 0／Good 0／Bad 0／Miss 393。自動 PLAY 路徑已完成兩次實測；G0／G1 開發接線可 review，完整 G1 尚欠暫停／恢復、RESULT 分類、黃色外框／重疊類型與足夠人工預測樣本。此首批當時 G2 未開放；後續授權與實測見下節。

## G2 真實有限 Tap 閉環

使用者後續明確要求接 G2–G4 真實遊玩，不再停於 dry-run。先新增嚴格的類型／提前量／不確定度 profile、單一 action owner 的真實 GrpcTouch、逐 intent 計畫與 receipt 記錄；UI-only 與 Note scheduler 共用一個 transport，離開選曲頁只撤銷 UI 計畫一次。Tap 以局部像素線性擬合預測撞線，down 提前 8 ms，up 間隔 18 ms，最多 2 個獨立接觸；來源、UI、目標證據均須新鮮。短回歸含 contact 在 down 才配置、active prefix 不重播、過期／停止釋放、Hold 長度有界與 Flick 多階段門控。`ctest-assist-v1.log` 55／55 通過；先前 misplaced variable 的編譯失敗日誌保留。

首真實 run `cpp-observe-17904427334983916`，210 秒正常 STOPPED，11,762 個消費 frame，234 次真實 Note 命令。使用自有 Release executable SHA-256 `fc0c4e7a683158ca9dd47af75d71242df9af141dbb7f05db4e598df14052bef2`，當前能力指紋仍匹配。程式自動 PLAY，曲中沒有人工操作。人工核對 gRPC 結算 PNG 與 Emulator UI：Glaciaxion HD Lv.6，274,491 分，Perfect 109／Good 15／Bad 0／Miss 269、總數 393、max combo 10、accuracy 30.22%，Early 0／Late 15。這是有限 pixels→touch→pixels 的實際命中證據，不是 AP，也不能把 Note 命令數一對一當成判定數。

`assist-tap-v1-analysis.json` 由 C++ 從 raw 重算（raw SHA-256 `c6d399d231516aea409bed4989c51b6141eb3f6b8c6bb81f6de29f2a629b950d`）。真實排程 lateness n=234，p50／p95／p99／max = 5.276／13.829／15.337／16.080 ms；RPC duration = 0.807／1.433／1.891／2.446 ms。三次 runtime revoke，無 scheduler rejection。所有回饋逐 Note 身分仍 unknown；Good 全 Late 只支持固定提前量下一輪加大，不證明精確某段延遲。

限量圖為 `assist-tap-v1-live/diagnostic.png`（傾斜主線與相交垂直裝飾线）及 `assist-tap-v1-result/diagnostic.png`（SHA-256 `42722d0c039a1906db336609082e8ea83ed23ec88bda6d3a2289da1794a0d63b`）。這些圖只供離線診斷，執行時不回饋成腳本。

## 四類 assist 第二輪：保留失敗

使用者休息前允許 computer-use 做少量 Emulator 導覽；依此只點結算 Next 回到 HD，沒有 computer-use Note 操作。實戰圖暴露細線與 Hold 邊框相交、傾斜主線及黃色高亮外框；新增跨畫面支持點擬合線、Note 朝向配對、Hold 頭尾與短期尾端擬合、持續 Hold 的未執行步驟修訂／prefix 壓縮，Flick／Drag 暫定短動作。改用固定全類 profile：提前 20 ms、不確定度上限 30 ms、2 contacts。

action wait 改為 QPC 計算相對間隔的 Win32 高解析度 timer + wake event，沒有改用 UTC deadline；API 依 [CreateWaitableTimerExW](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw) 與 [SetWaitableTimerEx](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimerex) 契約。`ctest-assist-v2.log` 保留 55／56 的半像素位置容差失敗；支持點是整數 row，調整容差後 `ctest-assist-v3.log` 56／56 通過。

run `cpp-observe-17904437541363187`， observer version 5，實際 7,406 消費 frames、861 次真實 Note 命令後因 `gameplay input fault: evidence_expired` 提前 FAULT；緊急 release IDs 0／1 返回成功、failed／unknown 空，效果未以 RPC 成功冒充驗證。接近 100 ms 的 frame 間隔，加上辨識耗時，在 scheduler 的 gate deadline 先觸發黏性 fault；新鮮 pixels 稍後到達仍無法恢復，列為接線缺陷，沒有放寬 100 ms 證據期限。

遊戲自行結算：488,168 分，Perfect 205／Good 10／Bad 0／Miss 178，max combo 15、accuracy 53.82%、Early 0／Late 10。結算 PNG／manual JSON 與故障 raw 均保存；不能算完整 G3 run 或 AP。`assist-all-v3-analysis.json` raw SHA-256 `095c4ec566c381692765244c46fa7ba043d9e5c0595aa65148da1b7acdcffc23`。排程 lateness n=861，p50／p95／p99／max = 0.133／0.967／15.017／29.521 ms；RPC = 0.798／1.443／1.967／3.116 ms。capture interval n=7,405 = 17.202／38.051／49.867／175.883 ms；recognition n=7,406 = 4.872／7.087／8.922／32.559 ms。含中途一次 1 秒第二診斷 stream 與少量 CU 觀察，版本／命令組合不同；p95 改善不能當作受控跨版性能通過，p99 仍有晚到尾端。

## 完整四類試驗（build v5）

gate evidence 過期改為普通全局撤銷：立即取消全部計畫／釋放、記 `gate_evidence_expired`，不把短暫無新 pixels 當作 transport 永久 fault。後續必須以新鮮 gate 重新接手，舊計畫不恢復；真正 input unknown／release failure 仍為黏性 fault。短回歸包含期限邊界釋放與之後新計畫恢復，沒有放寬期限。另新增 `analyze game-image <PNG>`，只用單張人工保留的圖作離線幾何診斷，不建立輸入、不產生時序、沒有接入 runtime。

離線圖 `assist-all-v3-live/diagnostic.png` 暴露中央箭頭將 Flick 切成兩個 50 px 紅芯與黃色邊框；初版輸出兩個 Flick 加兩個 Drag。要求可見中央箭頭的縱向白／黃支持才合併紅芯，並從紅芯側邊核對 highlight；修正後只剩一個 Tap、一個 Flick，沒有假 Drag。獨立沒有箭頭的相鄰紅芯負例保留。`ctest-assist-v4.log` 56／56、`ctest-assist-v5.log` 57／57 通過。

run `cpp-observe-17904442971534535`，固定全類／20 ms／30 ms／2 contacts，210 秒正常 STOPPED，11,402 消費／11,459 發布 frames、57 latest overwrite／consumer skips、0 pool drop／relative lag drop、1,048 次真實 Note 命令、兩次真實 UI PLAY 命令。程式自行 PLAY；CU 只做重試 Next 與少量狀態觀察，沒有人工 Note 觸控。結算 683,550 分，Perfect 288／Good 11／Bad 0／Miss 94、總數 393、max combo 30、accuracy 75.10%、Early 0／Late 11；未 AP。正常 game release requested／failed／unknown 均空。

`assist-all-v5-analysis-old.json` 是建置 v5 的 C++ 重算，raw SHA-256 `f3f209c919a4b24f316a3ac7eb0e9073bef108230e6dbc1ffd0394a9971d81ba`。真實 lateness n=1,048，p50／p95／p99／max = 0.176／1.287／17.133／28.719 ms。此 lateness 混有提交時已過期的預測期限及全部命令階段，不能全歸因於 OS 計時器喚醒。9 個拒絕：6 down_too_late、2 contact_conflict、1 gate_evidence_expired；5 個 runtime revoke：3 source 過期、2 UI gate 丟失（含結算），停止計數另含第 6 次。少量外部診斷 stream 可能擾動，逐 Note 回饋仍 unknown，G3 所需類型能力不由結算總數單獨通過。

## Hold 幾何與待送 deadline 修正（build v6）

短而寬的 Hold body，其 PCA 長軸是橫向；依同幀有充分支持的線法向選長／短軸，保留正確頭尾方向。即使 head 相對線靜止，也輸出最新 projection，讓 active Hold 可跟隨線運動；尚未 down 的計畫可按新像素重估期限／位置，已 down 的 Tap／Flick prefix 不更動。新增短 Hold／靜止 head 與 pending deadline refinement／active down 不重播回歸，`ctest-assist-v6.log` 59／59 通過。

run `cpp-observe-17904446838608654`，observer version 6，固定同一全類／20 ms profile，210 秒 STOPPED、11,417 消費 frames、1,282 個真實命令（down 395／move 492／up 395），正常 game release requested／failed／unknown 空。結算 708,384 分、Perfect 301／Good 7／Bad 1／Miss 84、總数 393、max combo 34、accuracy 77.75%、Early 0／Late 7。保留 `assist-all-v6-result/diagnostic.png` 與該 run 的 manual JSON，沒有人工 Note 觸控，未 AP。

C++ 逐 intent 有界分析（最多保留 256 條尚可配對的 intent，eviction 明列，無逐 Note 判定歸因）重算 raw SHA-256 `3b4a61ae7ffa38aea59ddb6551e5de678d4205f163ffcf91b1186c3b03ce04cd`，檔名 `assist-all-v6-analysis-old.json` 實際由新建置中的分析器產生，名字不代表舊解析契約。真實 down 依 basis 為 Tap 167／Hold 100／Drag 122／Flick 6，與真實譜面／判定不能一對一等同；無法配對標 unknown。Hold 接觸時長 n=100，p50／p95／p99／max = 135.335／353.635／479.893／513.265 ms。6 次拒絕（3 down_too_late、3 contact_conflict）；這些分布用來定位早放／候選重複，不當作 Hold 正確率。

下一版針對實戰中的小命中特效與黃圈加入尺寸／薄帶形狀負例，build v7 的 60／60 回歸通過。分析亦顯示身體縮短的 Hold 被當成新 Tap，中止頭部 identity；後續改按法向的 leading edge 延續短 Hold，薄新 Tap 不能繼承舊 Hold。此改動需獨立實測，不回填 v6 成績。

## 退步批次與 Hold 身分診斷（build v8）

build v8 的 61／61 短回歸通過；run `cpp-observe-17904452127578843` 固定同一全類／20 ms profile，210 秒正常 STOPPED、11,244 消費 frames、959 真實命令（down／up 各 327、move 305）。可見結算 602,417 分、Perfect 253／Good 10／Bad 0／Miss 130、總數 393、max combo 32、accuracy 66.03%、Early 0／Late 10，較 v6 退步。保留結算圖、manual JSON、全部 raw，不能把候選減少当作命中改善。

`assist-all-v8-analysis.json` raw SHA-256 `d682bd204875096b4521b8e27d8336b938e428929f03d6c2b19b6190a3fbd3b3`；真實 down 依 basis 為 Tap 142／Hold 89／Drag 90／Flick 6。Hold 時長 n=89，p50／p95／p99／max = 90.969／137.309／315.964／362.489 ms，沒有證明尾端改善。3 runtime revoke、2 down_too_late，沒有 contact conflict；少量 CU 狀態觀察，預設無圖保存。

以首 Hold（intent 1／note 7）原始決策定位：frame 408 的 head y≈552、height 536，down 後 frame 411 仍見 body；frame 412／413 又出现 width 92／116、height 46／58 的厚藍色命中閃光，與 Hold 的近期頭部競爭，令其 association_ambiguous／samples 0，owner 隨即釋放。除了變色／消失，身分競爭亦是早放原因，沒有以結算倒推逐 Note 判定。

後續要求 Tap 為薄帶形狀；變淡的 Hold 只能依最近 100 ms 身分、同幀可觀測線及兩側平行輪廓續接。輪廓消失後不補寫歷史按鍵，畫面頂端裁切的 tail 保持 unknown。新 decision schema 2 明列 `note_anchor_semantics`（Tap／Flick core center、Hold leading edge）及 observation basis；C++ 分析器仍支援歷史 schema 1。新分析分開「接受時尚未到期」與「接受時已過期」down lateness，避免混為 timer jitter。build v9 的 63／63 通過；v10 保留 62／63 的測試失敗，其舊 expected head y=572 未區分 color anchor 與 outline 投影 y=576，改為各自核對其像素語義。

build v11（observer version 7）的 `ctest-assist-v11.log` 63／63 通過；色芯 head 與輪廓投影 head 各自驗證，未擴大容差掩蓋差異。輪廓由當幀兩側白／灰支持延續，至少 16 px 深度、兩邊末端差不超過 20 px；裁切到頂端可確認 body 但不推定完整 tail。停止／期限、前綴不可改、薄新 Tap 與閃光負例仍通過。

## 輪廓續接完整試驗與提前量（build v11）

run `cpp-observe-17904461554251836`，全類／20 ms／30 ms，210 秒正常 STOPPED，11,475 消費 frames、1,269 真實命令（down／up 各 318、move 633）。結算 712,926 分、Perfect 302／Good 8／Bad 0／Miss 83、max combo 37、accuracy 78.17%、Early 0／Late 8；未 AP。程式自行 PLAY，CU 只返回選曲與觀察，這輪沒有第二條中途診斷 stream。結算 PNG 與 `result-manual.json` 保存，executable SHA-256 `9af96c5411b508192108f9b1c155ff8c18523e8ef211a5ed9c72980e264536be`。

`assist-all-v11-analysis.json` raw SHA-256 `16bda7f9b12edde5f1b6e7b574b6786e0a250da973ea5e9f8dcbbd2c7128dac9`；實際 down 依 basis 為 Tap 141／Hold 81／Drag 91／Flick 5。Hold 時長 n=81，p50／p95／p99／max = 321.771／723.839／1073.589／1584.043 ms。接受時尚未到期 down n=252，lateness = 0.355／0.777／1.232／1.483 ms；接受時已過期 n=66，= 5.056／21.508／24.412／24.614 ms；未分類 0。RPC n=1,269 = 0.775／1.297／1.954／3.185 ms。3 次 runtime revoke（2 source、1 UI）、3 次拒絕（1 conflict、2 late）。改善 Hold 時長不等於逐 Hold 成功；不足的實際 down 與假候選仍待定位。

同一 executable 改 lead 35 ms、其他條件不變，run `cpp-observe-17904467711451736`，185 秒正常 STOPPED。結算 678,982 分、Perfect 291／Good 4／Bad 0／Miss 98、max combo 26、accuracy 74.71%、Early 0／Late 4。Good 減少但 Miss 增加，不能把提前量增加當作淨改善；下一輪回到 20 ms。此輪三次各 1 秒第二診斷 stream 與少量 CU 觀察，不能用兩輪分布宣稱受控性能差異。原始檔、結算 PNG／manual JSON 與獨立 `assist-lead35-v11-analysis.json` 均保留。

第三張有限診斷圖 `assist-lead35-v11-live3/diagnostic.png` 能辨出兩條可見 Drag，亦暴露一條命中特效的假 Drag；尚未完成特效抑制。機制研究已列明 FC／AP 指示可能改變線色；後續線候選加入藍／青色支持，仍須充分長細／跨畫面幾何，不以短蓝 Note 當成線。這是色彩不變性的合成修正，未宣稱本輪實際出現藍線。分析器另列 PLAYING-only 候選／理由／無線 frames，隔離 UNKNOWN 結算封面候選。

## 線候選合併與追蹤結果診斷（build v12–v15）

build v12 的色彩不變性回歸暴露斜線跨畫面 fit 與 PCA 重複候選（64／65）；舊去重只處理水平線。改為法向位置與切向夾角一起核對，build v13 的 65／65 通過。保留失敗紀錄，不改測試要求或放寬候選數。

分析器新增有界觀測身分結果（最多 512、超 200 ms 未見即結束；與意圖配對仍最多 256），分 actual down／accepted without down／near prediction not accepted／prediction never near／never predicted。near 的定義固定為相對 capture 的 crossing 在 −40 至 100 ms、uncertainty≤30 ms，僅供診斷，不宣稱符合所有 profile eligibility。這些是像素追蹤身分，不是譜面 Note；短期碎裂會重計。build v15 的 66／66 通過，observer version 8。

`assist-all-v11-playing-analysis.json`：PLAYING gate 9,112 frames，無候選線 227 frames；PLAYING target 的 line_unobservable 只有 86 次，全 UI 的 2,228 次多為結算候選。`assist-all-v11-tracks-analysis.json` 顯示 Drag accepted without down 27、near prediction not accepted 39、never predicted 468；追蹤 eviction 0。不能把這些數量當作真實 Drag 個數。

具體定位 note 110／intent 24：frame 1473–1474 的 Drag 有有效預測、計畫接受且 down 尚在未來；frame 1475 暫時缺候選，立即取消待送計畫；frame 1476 恢復同一身分時，submitted tombstone 阻止重送。後續修正 Tap／Drag 40 ms 的 bounded missing grace，保留原 100 ms 證據到期、UI／source 立即撤銷及不重播已執行 down；不以舊譜面補 Note。這項修正需下一版短回歸與獨立完整實測。

build v15 的全曲 run `cpp-observe-17904472910856189`，20 ms、185 秒正常 STOPPED，結算 695,763 分、Perfect 293／Good 11／Bad 0／Miss 89、max combo 33、accuracy 76.37%、Early 0／Late 11，未 AP，未超越 v11。只少量 CU 觀察、無中途第二擷取 stream。結果圖、manual JSON、`assist-all-v15-analysis.json` 與 raw 保留；真實 down 為 Tap 136／Hold 82／Drag 90／Flick 6，Drag accepted without down 32；6 個 scheduler 拒絕（1 conflict、5 late）。這輪尚未含 40 ms 漏辨容忍，不能歸因為其結果。

build v16 的 `ctest-assist-v16.log` 67／67 通過：單張 Drag 缺候選後恢復時，原待送 down 正常執行；到 40 ms 仍未恢復則取消、停止釋放且不重播。manifest 保留 observer version 8，新增 planner version 2，與更新前二進位 hash 分開記錄。此版的完整實戰驗證進行中，不能先算能力／AP 通過。
