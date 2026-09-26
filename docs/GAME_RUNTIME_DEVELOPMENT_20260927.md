# 遊戲 runtime 開發與實測紀錄（2026-09-27）

狀態：第一批 runtime／scheduler、遊戲 observer 與自動 PLAY 已接入；後續 G2 assist 有真實命中，四類完整試驗最高分結算 Perfect 350／Good 9／Bad 0／Miss 34。G1 全項、G3 所需能力與 HD／IN AP 尚未驗收。下列每批的版本、故障與結果分開保留，不以新修正回填舊 run。

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

build v16 的 `ctest-assist-v16.log` 67／67 通過：單張 Drag 缺候選後恢復時，原待送 down 正常執行；到 40 ms 仍未恢復則取消、停止釋放且不重播。manifest 保留 observer version 8，新增 planner version 2，與更新前二進位 hash 分開記錄。

run `cpp-observe-17904476712326667`，20 ms、185 秒正常 STOPPED，9,859 消費 frames、1,268 真實命令。結算 727,863 分、Perfect 307／Good 10／Bad 1／Miss 75、max combo 39、accuracy 79.77%、Early 1／Late 9，仍未 AP；無中途第二擷取 stream。`assist-all-v16-confirmed-analysis.json` raw SHA-256 `690c1a825176269dff9078a0eb322143451193a007eaca1741a7494f363bc358`：實際 down Tap 134／Hold 80／Drag 108／Flick 5，Drag accepted without down 10，6 contact conflict。相較 v15 的 32 個未下壓 Drag 接受計畫減少，但逐 Note 判定仍 unknown，增加觸控不等於全部成功。

v16 shell 重導向的 `live-assist-all-v16.json` 為空，原因未確定；程式自身 `summary.json`、manifest、events 原樣完整保留，正式狀態以該 summary 核對。以空檔取得 run path 的後續分析呼叫失敗（path is required），錯誤 `assist-all-v16-analysis.json` 保留；改用已核對 run 路徑產生上述 confirmed 分析。結果 PNG／manual JSON 均已保存，沒有用推定填補 raw。

build v17 將 Drag 計畫接觸從 50 ms 擴至 90 ms（仍受新鮮證據、40 ms 漏辨及即時全局撤銷約束），檢查捕捉時間的 jitter／表觀 crossing 誤差是否使短接觸漏接；這是待實測假設，不宣稱 Drag 判定窗或可承受 90 ms 擷取延遲。新增晚 crossing 仍有接觸、沒有重播 down 的合成回歸，68／68 通過；planner version 3，manifest 明列 90 ms。

run `cpp-observe-17904480876064329`，20 ms、185 秒 STOPPED，10,069 消費 frames、1,311 真實命令。結算 741,997 分、Perfect 310／Good 13／Bad 0／Miss 70、max combo 50、accuracy 81.03%、Early 0／Late 13；未 AP。無中途第二 stream，結果 PNG／manual JSON 保存。`assist-all-v17-analysis.json` raw SHA-256 `a2c2687d11a9defc6d5d91ed33b7db96fa4f6f05d76eb3410bd094e4c7626235`：down Tap 136／Hold 86／Drag 101／Flick 6，Drag accepted without down 4、near prediction not accepted 28，contact conflict 1。v16 的有限近線原始決策亦顯示一段成對 Drag 間隔約 74 px／1453 px/s≈51 ms；但 v17 實際衝突只有 1，不能只憑理論重疊將 70 個 Miss 歸因於 contact capacity。

build v18 保留 observer 8／planner 3 行為，只增加 first near prediction 相對 recognition end 的 available lead 診斷，68／68 通過。重算 v17：未接受的近線 Hold 候選 n=8，第一次可靠近線預測 available lead p50／max = −26.405／−0.850 ms；Tap n=3 = −19.788／−1.993 ms。這些是追蹤候選，不能單獨視為真实漏掉的 Note。

同版 35 ms profile 的 run `cpp-observe-17904484620758492`，185 秒正常 STOPPED，結算 744,809 分、Perfect 317／Good 6／Bad 0／Miss 70、max combo 39、accuracy 81.65%、Early 1／Late 5。無中途第二 stream，結果 PNG／manual JSON、raw 與 `assist-lead35-v18-analysis.json` 保存；Good 改善而 Miss 不變，仍未 AP。新 planned deadline 恢復將另開版本，不回填本輪。

## 有界晚預測補接（build v19）

planner version 4 將接受下限從校準後 due 已過 20 ms 改為不超過 60 ms，超過仍拒絕；可靠的 crossing／uncertainty、新鮮 100 ms pixels、獨立 PLAYING gate 與安全座標仍必要。尚未 down 時可把過去期限改為立即執行，四類未執行步驟維持順序與名義接觸長度；已 down 不重播，停止／失效照舊釋放。這是待實測恢復政策，60 ms 不是遊戲 Perfect 時窗、來源絕對延遲或 scheduler 可容忍的 jitter。

`ContactPlan` 附加可選 `predicted_down_ns`（host QPC），只供診斷，不替代 scheduler 的步驟期限。初始／真正使用的新 pending 預測保存原下壓期限；active 已下壓時保留。計畫 journal 同步輸出，C++ 分開 real schedule lateness、predicted deadline lateness、intentionally clamped downs 與缺歷史欄位的 unknown。補接當下約 1 ms 的 schedule lateness 不能掩蓋原預測晚到 31 ms；短回歸明確核對兩者。

build v19 的 `ctest-assist-v19.log` 70／70 通過，包含四類有界晚預測立即 down、原預測期限保存、超限／來源過期拒絕、active down 不重播及兩組晚到量分開核對。

run `cpp-observe-17904488972403215`，20 ms、185 秒 STOPPED，9,535 消費 frames、1,383 真實命令。結算 759,695 分、Perfect 315／Good 16／Bad 0／Miss 62、max combo 57、accuracy 82.80%、Early 0／Late 16，未 AP。一次 1 秒額外 live 診斷 stream，不能宣稱受控性能比較。raw SHA-256 `00ee8b8bc8dbde05099afc0258ebfe47d4e596e7742814a78e8ae0388e1c03b8`；down Tap 138／Hold 96／Drag 123／Flick 6，刻意 clamp 103 次、預測期限 unknown 0；5 contact conflict、1 gate expiry。`assist-all-v19-analysis.json`、result PNG／manual JSON 均保存。

## 補接時限與校準量分離（build v20）

planner version 5 改以 estimated crossing 已過不超過 40 ms 為下限，校準後 future due 仍不超過 60 ms；不讓增加 lead 隱含縮短補接窗口。原始 predicted down 仍保存，clamp 後立即觸控不重播。新增 35 ms lead／crossing 已過 30 ms 的回歸：原預測 down = −65 ms，實际排程 0，active／停止責任維持。`ctest-assist-v20.log` 70／70 通過。

run `cpp-observe-17904493232314195`，35 ms、185 秒 STOPPED、10,052 消費 frames、1,464 真實命令。結算 789,084 分、Perfect 335／Good 6／Bad 0／Miss 52、max combo 51、accuracy 86.23%、Early 2／Late 4；未 AP。沒有中途第二 stream。`assist-lead35-v20-analysis.json` raw SHA-256 `0f015158705cd140469e882ed15c73420c458bac00f661e2f23e943f8e651cef`，down Tap 139／Hold 89／Drag 124／Flick 6、刻意 clamp 127／unknown 0，2 contact conflict、1 gate expiry。結果 PNG／manual JSON 保存。

後續可選 `--keep-diagnostic-anomalies` 在同一即時 frame 的近線窄 Hold 對或近線 history／association／fit 失敗時，各只保留第一張，最多兩張。以固定兩槽副本保存，輸入停止／釋放後才編碼；預設 none，不讀檔案回饋 Note 操作。共用既有 WIC PNG writer 到 core（與讀取 helper 相同模組），不從 runtime 反向依賴 bench。這是為精確對上 raw 決策而保留的有限診斷，不擴大為長期錄影或 frame 佇列。

build v21 保留 Windows `near` 巨集名稱衝突的編譯失敗；改用 `near_line` 後 build v22 成功，70／70 短回歸通過。`fake-observe-v22.json` 的 fake run 正常 STOPPED、input_created=false、diagnostic_images_saved=0，目錄只有 manifest／events／summary；非 assist 使用診斷旗標明確拒絕（`diagnostic-invalid-mode-v22.log`）。observer 8／planner 5 的實際識別／遊玩政策保留，另開 35 ms 有限診斷 run，PNG 留待輸入停止後核對。

## 同幀異常證據與 Hold 核心碎裂（build v22–v23）

v22 run `cpp-observe-17904498546490119`，35 ms、185 秒正常 STOPPED，9,958 消費 frames／1,400 真實命令；結算 771,743 分、Perfect 329／Good 7／Bad 2／Miss 55、max combo 31、accuracy 84.87%、Early 3／Late 4，未 AP。原始 raw SHA-256 `746a11f692c5febd9c274117f5a395f2643fc6f44e8e5f32173676b6d824cc3c`，結果 PNG／manual JSON 與 `assist-lead35-v22-analysis.json` 保存。沒有中途第二擷取 stream，輸入停止後實際保存兩張診斷 PNG。

`diagnostic-hold-fragments.png` 對上 source frame 506／capture QPC 139743125272100 ns：一條 Hold 的當前頭部停在 y≈576 主線，白色 rails x≈779／931 延續至 y≈146；命中特效截斷藍色核心。舊 observer 將上部核心當作 note 15 的新 head（y≈473），另將下部碎片建立 note 18／19，並因方向與多線關聯未驗證而失去可靠樣本。這是同一畫面的幾何錯誤證據，不能由 score=0 推成 Hold 未命中（長按結算尚未出現）。第二張 frame 584 顯示左侧 Hold 即將結束的短藍色核心與 hit effect，旁邊另有正常接近的 Hold。

v23 observer version 9 保留 planner 5：最近 Hold head 靠近充分可見主線時，同幀兩側 rails 先驗證連續支持，再以完整近期身分續接。rail 頭部 12 px 內沒有支持即拒絕，不能跨空白接到遠方 body；rails 已證實時，包在當前 body 範圍的厚藍色碎片不再各建新身分。其他位置的 Note 及獨立薄 Tap 保留。新的短合成回歸涵蓋八張碎片／rails 連續畫面、同時另一 Hold／薄 Tap，以及 rails 與主線有空白時不得延續。此修正不讀取診斷 PNG 作執行時輸入。

v23 build／71 項 Release 測試通過。run `cpp-observe-17904505170905854`，35 ms、185 秒 STOPPED，9,882 消費 frames／1,233 真實命令。結算 764,135 分、Perfect 321／Good 13／Bad 0／Miss 59、max combo 38、accuracy 83.83%、Early 2／Late 11；未改善、未 AP。`assist-lead35-v23-analysis.json` raw SHA-256 `5f431fdd7fcedd4351ffccd647b3bd0362e9989886b9b44ffd2546087d3c0128`，結果與兩張同幀異常圖保留。沒有中途第二 stream。

frame 395–402 的第一條 Hold 確曾以完整 rails 續接（原下壓 intent 1 已執行）；frame 403 因命中特效遮住 rail 而退回偏移核心，frame 432 又分成三個 Hold。v24 observer 10 只對前一張已確認 rails 的 Hold，且當前 head 上方 4／8／12 px 的多點取樣仍有藍／灰長條填色時，允許最多 32 px 的短 rail 遮擋。其他情況仍為 12 px，沒有當前填色的 24 px 真空白不能續接。八張合成序列增加兩側不同位置的 24 px 遮擋，並保留反例；不能將對特效的容忍套成放寬來源期限。build v24／71 項 Release 測試通過。

v24 run `cpp-observe-17904509835434632`，35 ms、185 秒 STOPPED，9,742 消費 frames／1,295 真實命令；結算 797,952 分、Perfect 340／Good 7／Bad 0／Miss 46、max combo 35、accuracy 87.67%、Early 0／Late 7，為目前最高分、未 AP。結果 PNG／manual JSON、`assist-lead35-v24-analysis.json` 與兩張診斷圖保留，無中途第二 stream。實際 down Tap 136／Hold 88／Drag 133／Flick 6，Hold 時長 n=88 的 p50／p95／p99／max = 484.634／951.624／1588.338／2825.303 ms。來源／gate 中斷亦出現，不能把 Miss 全歸因於辨識或宣稱受控性能改善。

frame 519 的 Hold 17 仍正確續接，frame 520 退回核心，head 突然移到 y≈496（當時 PCA 僅列出一側 rail）；單側輪廓支持失效是待核對的解釋；身分的 appearance 被覆寫，使後續即使 rails 重新清楚也因 last-head 距線大於 40 px 而無法恢复。frame 529 的同幀 PNG／C++ WIC 像素讀取顯示雙側輪廓與 head 填色實際仍可見。有限離線 pixel helper 原始碼、失敗／修正建置及輸出保存在 ignored `measurements/g0-preflight/png-pixels*`，不接入正式輸入。

v25 observer 11 為每個既有 Hold 另保留一份已確認 rail 幾何／QPC，最多 90 ms；不保存 frame。暫時破碎核心不能覆寫這份 anchor；恢復時仍先核對當前雙側 rails、主線與必要填色。驗證成功的 outline 候選明確沿用原身分，避免其他短期碎片搶走配對；沒有當前雙側支持時不造 outline，不延長來源／owner 證據期限。新增單側 rail 缺一幀後恢復原身分及過期不得恢復的合成測試。

v25 build／72 項 Release 測試通過。run `cpp-observe-17904516371318531`，35 ms、185 秒 STOPPED，9,865 消費 frames／1,288 真實命令。結算 830,700 分、Perfect 350／Good 9／Bad 0／Miss 34、max combo 62、accuracy 90.55%、Early 1／Late 8，為目前最高分、未 AP。`assist-lead35-v25-analysis.json` raw SHA-256 `cd7b875f1abe15be7e9908a46edbd528f5e2a33a6ae7dea0bd4a80c3b817b0da`，結果 PNG／manual JSON 與診斷圖保留，無中途第二 stream。

v25 第一個窄 Hold 對移至 frame 6064／QPC 141627271546600 ns。原圖含一條接近中 Hold（完整外輪廓約 x=454–618、head y≈548）及旁邊獨立 Tap；穿過 body 的垂直裝飾線把藍色核心切開，observer 輸出兩個窄 Hold 且多線配對沒有樣本。此處尚未在判定線上，原先只對已到線 Hold 的修正無法涵蓋。

build v26–v27 observer 12 將當前完整彩色 Hold 的雙側 rails 支持也存入同一份 90 ms anchor。接近中僅用最近最多六個、90 ms 內 rail-validated 頭部坐標擬合預期位置，在其法向 ±32 px 內尋找當前完整前緣填色與雙側 body rails；搜尋不是未見畫面的觸控依據，驗證失敗就不續接。前緣保留 color detector 的 2 px 內側語義；接近主線至 8 px 才轉已有到線維持路徑。完整身分配對排除同 body 的碎片，其他薄 Tap 保留。新增移動 Hold 被垂直線切開的合成序列，核對身分、完整寬度、頭部位置與 500 px/s 時間預測。v26 完成編譯後檢查短尾部的分支條件，v27 補上 8 px 轉換；v26 未進實戰。

v27 build／73 項 Release 測試通過。run `cpp-observe-17904522950572565`，35 ms、185 秒 STOPPED，9,926 消費 frames／1,312 真實命令。結算 807,405 分、Perfect 344／Good 6／Bad 0／Miss 43、max combo 42、accuracy 88.52%、Early 0／Late 6；較 v25 退步、未 AP。`assist-lead35-v27-analysis.json` raw SHA-256 `7b713398dd184cd94a0e4c56eecfd91dba779d5324297590ba640d1c56de7523`，結果與兩張同幀診斷圖保留，無中途第二 stream。down Tap 134／Hold 94／Drag 129／Flick 6，4 contact conflict；這些像素身分數量仍非譜面個數。

frame 5995 與 v25 frame 6064 是相同幾何類型的有限診斷，仍輸出兩個窄 Hold（其中寬度約 34 px）而非完整可見 body。只靠舊 anchor 的接近恢復未解決此例，不能因合成測試通過而宣稱已修正；下一步需要從當前外輪廓直接取得完整幾何，保留 v25 最高分基線與全部退步紀錄。

v27 同一份原始碼另經 Debug 與嚴格預設 AddressSanitizer 回歸，各 73／73 通過，CTest 總時間分別 9.50／21.97 秒；日誌 `test-game-debug-v27.log`、`test-game-asan-v27.log` 及 configure／build 紀錄均保留。ASan 使用 BuildTools 14.51 編譯器與 Community 14.50 sanitizer header/runtime；第三方 pinned vcpkg 二進位本身未插樁，不能將通過結果擴稱為其內部驗證，也不代表遊戲辨識或 AP 通過。正式遊玩只使用 Release。

## 當幀外輪廓重建（build v28–v33）

observer 13 從當幀藍色 body 碎片、跨畫面主線及兩側窄白輪廓重建完整幾何，不要求之前曾有完整核心。容量與像素支持規則見架構契約；新的觀測依據 `hold_current_parallel_rails_and_fill` 不冒充近期身分續接。對傾斜的獨立薄 Tap，按相對主線的正常方向厚度保護；已有完整色芯且雙側 rails 驗證成功時保留原候選。新的合成序列涵蓋第一幀即為斜向、灰／藍漸層、被垂直裝飾切開的 Hold，並核對頭／尾、身分與 500 px/s 表觀預測；短 50 px Hold、12 px 邊框閃光、空輪廓及相鄰兩條短 Hold 分開驗證。

v28 的 70／73、v29 的 74／75、v31 的 74／75、v32 的 73／75 失敗紀錄均保留；v30 的診斷 assertion 呼叫錯函式名造成编譯失敗亦保留，未進實戰。修正包含不讓新幾何遮掉已到線身分、不沿與裝飾線相連的輪廓越過 body 填色尾端、完整當前前緣不另做歷史搜尋、逐像素前緣定位以避免 2 px 二次量化的速度偏差，以及保留完整色芯的已知 tail。原幾何／預測數值要求未放寬；舊裝飾線測試的觀測依據容許新增當幀證據，仍驗證完整身分、位置及速度。

v33 Release build／75 項回歸全部通過（CTest 5.42 秒）。有限離線重算 v25 frame 6064 與 v27 frame 5995，均得到一條完整 Hold 與另一獨立 Tap；早先失敗重算檔案保留。v27 重算約為 width 153 px、head y=546、tail y=240，而非兩個窄 Hold。`analyze game-image` 只有單幀、無 input、無撞線預測，不把歷史 PNG 餵給 runtime。忽略的 `hold-rails-diagnostic.cpp` 為自有 C++ 單張 WIC 診斷，包含現行幾何 helper 的離線快照及 probe 輸出；初次 NOMINMAX 建置失敗與修正紀錄亦保留。這些只證實該畫面的幾何，不能替代完整實測或 AP。

v33 run `cpp-observe-17904542683786152`，35 ms、185 秒正常 STOPPED，9,693 消費 frames／1,355 真實命令；結算 798,321 分、Perfect 343／Good 4／Bad 0／Miss 46、max combo 27、accuracy 87.94%、Early 0／Late 4，較 v25 最佳退步，未 AP。沒有中途第二 gRPC stream。結果 PNG／manual JSON 與兩張診斷圖保存；`assist-lead35-v33-analysis.json` raw SHA-256 `a3ae90c5bffec3592469d106e2795136bdea8c78b693456c1d4498c322ac62ef`，down Tap 136／Hold 109／Drag 125／Flick 6、5 contact conflict、1 UI gate lost／2 source evidence expired。Hold 時長 n=109，p50／p95／p99／max = 312.537／1129.015／1385.551／3491.707 ms；識別 n=9693，5.302／8.135／10.541／24.731 ms。環境由該 run manifest 核對，非受控性能比較，不能把增加 Hold down 當成命中改善。

第一張窄 Hold 對移至 frame 6024／QPC 144259486930300 ns。原圖約 x=355–525 的另一條接近中 Hold，左側 rail 被短粒子特效遮擋；C++ 單張 probe 中左側支持到 depth 30 就停，右側持續約 308 px。暫時離線 helper 用 32 px 遮擋限制後，兩側均為 308 px、width 約153 px／head y≈548，附近裝飾線造成的較寬錯配仍因末端差異被拒絕。probe／build 與前後失敗重算全部保留，不能把離線重算当作 live 驗收。

observer 14 只有在當前前緣兩排填色支持成立時，對新幾何的 rails 容許最多 32 px 短遮擋，保留 body 填色尾端、兩側末端差與 4 px 側向搜尋限制。正例增加接近中單側 24 px 遮擋；新的 44 px 斷裂負例讓 v34 回歸 75／76，暴露搜尋在 body 內部重新起算 head。v35 要求前緣前方 4 px 的一排不再有完整填色，不能把內部截面当成新的 leading edge。v35 Release build／76／76 全部通過（CTest 5.63 秒）；實際 v33 frame 6024 的無 input 單張重算仍保留一條完整 Hold（153 px／head≈548／tail≈241）與獨立 Tap。尚待下一輪完整實測，未更改 planner 5／35 ms 提前量。

v35 run `cpp-observe-17904549524972216`，35 ms、185 秒 STOPPED，9,873 消費 frames／1,357 真實命令；結算 823,982 分、Perfect 351／Good 5／Bad 0／Miss 37、max combo 50、accuracy 90.14%、Early 2／Late 3，未 AP，總分與 Miss 仍未超過 v25。結果 PNG／manual JSON、兩張診斷圖與 `assist-lead35-v35-analysis.json` 保存，無中途第二 gRPC stream。raw SHA-256 `89ce0f94d9382447d0b2986d1ff611016d6dfd7067ca460479cc0e754d81de96`，down Tap 138／Hold 98／Drag 127／Flick 6；7 contact conflict、2 gate evidence expiry，runtime 5 UI／6 source revoke。Hold 時長 n=98，p50／p95／p99／max=490.132／1174.950／1483.617／3522.823 ms。部分撤銷明確在曲中，不能推成只有載入／結束；實際對逐 Note 的影響仍 unknown。

frame 6034／QPC 144943787955000 ns 的已到線 Hold 在黃色特效覆蓋下，當幀重建又輸出上部 note747（head≈515、depth62）與下部 note738（head≈570、depth46），而完整外輪廓仍約在 y=440–600。這次是 body 的內部黃色覆蓋被當成填色結束，同時讓前方「沒有藍色」誤充 leading edge，並非已知裝飾線錯配本身。後續 observer15 保留新前緣兩排藍／灰支持，在 body 結束／前方不延續的判斷中額外識別當前黃色覆蓋；黃色本身不能建立新 Hold。合成完整漸層／斜向／裝飾／短 rail 遮擋序列新增跨 body 的黃色帶，原完整 head／tail／速度要求保留。尚待 build v36 回歸及實測。

v36 的黃色帶合成回歸 76／76 通過（5.35 秒），但原始 v35 frame6034 的單張重算仍出現多個假 head／短 tail；保留 `assist-v35-anomaly-observer15-v36.json`，不宣稱離線單幀已解決。v37–v38 增加 bounded held-region 排他：只有 90 ms 內、已驗證到線的 rail anchor，才抑制其寬度與近期 body 範圍內重新建立完整頭部；原既有路徑仍必須重新驗證當前雙側輪廓才能續接。已到線 anchor 不被未到線的破碎 rail 候選覆寫。8 張已按住 Hold 的合成序列加入內部黃色覆蓋，仍核對完整身分／到線位置、獨立另一 Hold／薄 Tap，以及輪廓消失時停止形成候選。v38 Release build／76／76 通過（5.23 秒）；v37 只完成編譯，未進實战。保持 observer15／planner5，下一輪全曲仍須驗證，最高分仍為 v25。

v38 run `cpp-observe-17904557528045827`，35 ms、185 秒 STOPPED，9,632 消費 frames／1,345 真實命令；結算 828,880 分、Perfect349／Good10／Bad1／Miss33、max combo58、accuracy90.46%、Early1／Late9，未 AP。Miss 為目前最低，但總分與 accuracy 仍未超過 v25，不能視為受控淨改善。原始 raw SHA-256 `b69604edd32c94a33c2e19c72425ac1bbafd86a4bd41a5d6652523cd91906e6d`、result PNG／manual JSON、兩張異常圖與 `assist-lead35-v38-analysis.json` 保存，無中途第二 gRPC stream。down Tap135／Hold98／Drag132／Flick6，4 contact conflict／1 gate evidence expiry；runtime5 UI／6 source revoke。Hold 時長 n98，p50／p95／p99／max=490.853／1352.047／1811.434／2290.860 ms。frame5846仍有已到線 Hold 的黃色輪廓／覆蓋下窄碎片；純白轮廓支持不足仍未解決。

檢查另外發現近期身分續接的薄 Tap 保護仍以畫面包圍盒高度<12判斷，與當幀重建的線法向厚度保護不一致；傾斜的8px薄 Tap會有高於12px的包圍盒，可能被刪掉。v39新增合成已到線斜Hold、其body內獨立薄Tap的五幀運動，要求保留兩身分及500px/s預測；既有8張輪廓續接正例另增加下部48px黃色rail。等待回歸重現後才修正，無新增實機完成宣告。

v39僅新增/加強兩個合成測試，75／77失敗，確實重現已到線黃色rail拆裂及傾斜薄Tap被刪除。v40 observer16同步使用法向薄帶保護；對具有當前兩排藍／灰前緣的完整重建，或近期rail anchor加當前head填色證據成立的到線續接，才允許黃色覆蓋參與雙側rail支持。無填色、單rail、過長gap、空輪廓等負例保留。v40 Release build／77／77通過，CTest5.09秒；planner5／35ms不變，尚待完整實測，不把合成通過等同AP。

v41的兩側碎片負例單項1／1通過；v41b改以單一側邊碎片隔離邊界，1／1失敗，輸出明確讓原note1的觀測head從到線位置跳到x≈904.5／y≈505、width64，可能污染既有接觸更新。v42 observer17把held-region排他從中心±0.3width改為整個候選寬度落在近期body半寬+16px之內，沿用90ms期限与當前完整輪廓另驗證原則；排除不能刷新原Contact。這同時保留v40黄色rail與傾斜Tap修正。v42 Release build／78／78通過（5.27秒）；v40／v41／v41b未進實戰，失敗日誌及單幀異常重算保留。下一輪35ms全曲驗證，未宣稱AP。

v42 run `cpp-observe-17904569085009563`，35 ms、185 秒正常 STOPPED，9,807 消費 frames／1,304 真實命令。結算 817,939 分、Perfect 347／Good 10／Bad 0／Miss 36、max combo 33、accuracy 89.95%、Early 1／Late 9，較 v38 退步、未 AP。raw SHA-256 `33161957d214d85aa6d4fd27e5131cb263045d6c3a373060660ac34f73204ccb`；結果 PNG／manual JSON、兩張異常圖及 `assist-lead35-v42-analysis.json` 保存，無中途第二擷取 stream。down Tap 136／Hold 97／Drag 127／Flick 5，8 contact conflict、1 gate expiry；runtime 1 UI／2 source revoke。frame 3410 的保留圖實為 Drag 命中特效附近的窄藍色碎片，不能當作兩條真實 Hold。

## 接觸衝突診斷與完整黃色 highlight（build v43–v46）

v43 的 C++ 分析器依 journal 順序維持最多 16 個已成功 RPC down／up 的本機接觸摘要；owner revoke 清空本機歷史，失敗或缺必要欄位的 receipt 列 unknown。每份分析最多保留 64 個 conflict，含被拒絕計畫、當時最新目標及既有接觸計畫，其餘只計 omitted；計畫／targets 另限制 16／128。這不證明遊戲實際接觸效果或真實三押，舊紀錄缺欄位也不能推成成功。短回歸核對失敗 down 不佔摘要、成功 up 移除及 owner reset；Release 79／79 通過（5.09 秒）。

同一份 v42 raw 的 `assist-lead35-v42-conflicts-v43.json` 顯示重複 Hold 及黃色候選的例子，尚無足夠證據增加已驗證的兩指容量。intent 303 被拒時兩指由 Tap／Flick 使用；其 Drag 候選約 (411,542)、148×26，而 Tap 約 (410,541)、126×8。原 highlight 過濾在高度超過 24 px 時跳過，會讓完整黃色端帽建立多餘身分。v44 新增 148×26 的連通外框與獨立 Drag、Tap、Flick 的運動回歸，確實失敗（4 候選而非 3），原失敗紀錄保留。

observer18 以黄色帶相對自身方向的法向厚度，核對可見色芯寬度的 25%（最低仍 24 px），再沿用位置、寬度及當前藍／紅芯像素支持過濾 highlight；不是放寬全部 Drag 形狀。v45 Release 80／80 通過（5.57 秒）。v46 同步 manifest 版本，decision schema2 以可選診斷欄位輸出 `rails_geometry`／`head_on_line`，便於下一輪分辨色芯與已驗證輪廓。planner5／35ms、兩指能力門控與來源期限不變，尚待完整實測。

v46 Release 80／80 通過（5.70 秒）；run `cpp-observe-17904581124806726`，35 ms、185 秒 STOPPED、9,596 消費 frames／1,299 真實命令。結算 812,926 分、Perfect344／Good8／Bad0／Miss41、max combo52、accuracy88.85%、Early1／Late7，未 AP，較 v42 退步。raw SHA-256 `c3e5523ccaff632c56c83d6d115fbee69d1269c25378d1a939a5430221dc0a5e`；manual結果、PNG、兩張異常圖及 `assist-lead35-v46-analysis.json` 保存，沒有中途第二 gRPC stream。down Tap138／Hold95／Drag134／Flick6，4 contact conflict／1 gate expiry；runtime2 UI／2 source revoke。四個衝突仍含重複 Hold、highlight碎片或同列早到的多個Drag，不能據此宣告真實三押。

## 短突發的時間跨度與歷史保留（build v47–v48）

v46 frame3169／QPC148051508178600 的 note334，位置約(423,256)，離線約320px，只有3個樣本卻預測58.5ms後撞線；速度5466.8px/s、殘差0.00084px，已下壓的同列兩個Drag亦在遠離線的位置。其前三次觀察的host時間跨度不足30ms，低殘差不能證明可可靠外推。v47新增短突發合成回歸，3張1.8ms間隔、每張前進10px重現5555.6px/s／零殘差與錯誤短期crossing。原失敗保存；穩定段末尾尚在350ms horizon之外，該正例的crossing assertion另須按實際距離校正，不放寬速度要求。

observer19 對相對運動擬合、追蹤速度外推及接近中rail anchor搜尋要求至少30ms時間跨度。近期點以各身分10ms host-QPC bucket保留最新坐標，bucket起點不隨替換推進，避免連續快幀永遠只剩一點；最多六點並嚴格移除90ms以前的點。每幀仍辨識／更新當前evidence，不以降頻代替來源期限，active Hold 的當前輪廓核對不依賴新crossing。schema2追加可選 `history_span_ns`，記錄实际預測跨度；絕對source age仍unknown，30ms為待實測的估計政策，不是遊戲判定時窗。等待回歸與實機驗證。

v48 Release 80／81，只有上述穩定段誤設crossing的assertion失敗；短突發拒絕已成立，其餘既有回歸皆通過。v49依該段實際距離核對 `outside_short_horizon` 與500px/s，另新增25張／4ms連續快幀，要求最多六點、跨度≤90ms、至少30ms的實際crossing與500px/s（±20）預測。Release 82／82通過（5.68秒）；只修正正例的錯誤horizon期待，未放寬原負例或速度要求。v47／v48未進實戰，下一輪仍35ms／兩指。

v49 run `cpp-observe-17904587762096256`，35ms、185秒STOPPED、9,836消費frames／1,212真實命令。結算811,807分，Perfect347／Good4／Bad0／Miss42、max combo44、accuracy88.96%、Early0／Late4，未AP；Good減少但Miss未改善，不能宣告受控性能改善。raw SHA-256 `f91d865868590f197a0d4f4eab6b855cd8bd0a5dc8a962694e1e855ab26ada4e`、manual結果／PNG、两張異常圖及 `assist-lead35-v49-analysis.json` 保存，沒有中途第二gRPC stream。down Tap134／Hold91／Drag114／Flick6，5 conflict／1 gate expiry；runtime5 UI／5 source revoke。80ms跨度下同列Drag仍可能被擬合成約2460px/s，不能把所有錯配歸因於短突發。

## 裝飾線擴大 Hold 的反例（build v50–v51）

v46 的衝突226包含已到線的約135px Hold與同位置附近的新200.5px短Hold。v50新增一個144px藍色body、雙側真rails加右側獨立白裝飾線的合成反例；單項確實失敗，重建將body擴成200px、中心偏移25.5px。原先兩排7／9填色允許缺少同一側的兩點，裝飾線又能滿足長度／末端條件，造成較寬候選優先。

observer20 保留兩排7／9要求，另要求兩側外部取樣群各至少一點有當前填色；前方續接檢查亦保持同一幾何規則。只靠一側body加白裝飾線不能擴寬，內部裝飾切開、漸層、黄色覆蓋、短rail gap与短Hold等既有正／負例不放寬。v51 Release83／83通過（5.62秒）。純單幀重算v46 frame5949仍含碎片／部分head，只有當前畫面且無近期identity，不能宣告其持續觸控已解決。

opt-in第二個診斷槽不再由預期的stationary Hold初始insufficient history觸發；只記近線association ambiguous，或至少30ms跨度、尚未到線的nonlinear mismatch。兩槽容量、停止後編碼、預設不存圖與不回饋input原則不變。此變更只改善诊斷選取，不代表所有被保留畫面都是Miss；v50未進實戰，v51待全曲。

v51 run `cpp-observe-17904594247906811`，35ms、185秒STOPPED、9,663消費frames／1,179真實命令。結算810,674分、Perfect344／Good7／Bad0／Miss42、max combo49、accuracy88.69%、Early2／Late5，未AP。raw SHA-256 `ca016c28a1f9f714727260f58a31407568c1382620e3bed771e5f06b2cc9110a`、manual結果／PNG、两張診斷圖及 `assist-lead35-v51-analysis.json` 保存，無中途第二gRPC stream。down Tap137／Hold96／Drag112／Flick6，沒有contact conflict，3 gate expiry；runtime5 UI／5 source revoke。零容量衝突仍有42 Miss，不能把剩餘失敗歸於容量。新第二診斷 frame825 是已轉暗的Hold；frame806先前因101.7ms擷取間隔清空history及HUD counter，雖當前pause_bars2／score_glyphs8，仍觸發UI gate lost。這是待研究的時間／重辨識事件，不在此輪放寬100ms輸入期限。

## 當前線投影與有限 Drag 共用（build v52–v54）

v52新增三項短回歸，投影及密集Drag兩項失敗、成員消失一項通過。±4px位置誤差的5點擬合雖有有效crossing，舊hit點偏離當前線3.2px；時間估計使用擬合distance，不應把殘差變成觸控法向位移。observer21保留時間／uncertainty計算，hit使用當前獨立可見線與實際頭部的正交投影；移動線未來姿態仍未驗證。

planner6只對已active、未到release的Drag，且新Drag具新鮮pixels、可靠短期crossing、同位置≤2px、預定覆蓋區間重疊，延長既有up而不送第二次down。只共用已成功維持的本機plan，不合併pending down、Tap、Hold、Flick或不同位置。primary與alias仍在128個identity內；每幀以目前可見成員重新核對同一位置與期限，失去全部支持／gate時釋放，單一alias消失不解除仍有支持的接觸。已完成或unknown input不得重播。未增加實體手指容量，也未啟用batch。

`game_drag_coverage` 記member／leader Note、原intent、source frame、QPC evidence／accept／release與unknown遊戲效果，最多128筆待取診斷；與新physical down分開。C++分析只在先前successful RPC down及尚未up的本機歷史成立時列 `covered_by_active_drag`，不稱Perfect或真實譜面數量。v53 Release86／86通過（5.69秒），v54同步planner版本及real_input診斷，增加不同位置／gate失效／新epoch與分析unknown歷史回歸，Release88／88通過（5.95秒）。v52／v53未進實戰，v54仍需完整曲驗證。

v54 run `cpp-observe-17904606480434858`，35ms、185秒STOPPED、9,662消費frames／995真實命令。結算818,537分、Perfect351／Good1／Bad0／Miss41、max combo52、accuracy89.48%、Early1／Late0，未AP。raw SHA-256 `f4ee0fd191988470f17accc5bceb53a234a066afbe90e53747e052b1816316b3`、manual結果／PNG、兩張診斷圖及 `assist-lead35-v54-analysis.json` 保存，無中途第二gRPC stream。down Tap137／Hold98／Drag109／Flick6；3次coverage均有journal順序的既有successful RPC contact支持，遊戲判定仍unknown。1 contact conflict／1 gate expiry，runtime8次revoke。擷取間隔n9661，p50／p95／p99／max=17.6791／39.6279／52.9433／684.5945ms；絕對source age仍unknown。漏音未有明顯改善，接下來針對motion reset與當前HUD確認的耦合做短回歸，保留100ms輸入期限。

## Motion reset 與當前 HUD 確認（build v55）

v51的101.7ms間隔曾同時清空 motion 與三幀 HUD 確認，在當前HUD仍完整時先撤銷，再因epoch改變清空第二次。observer22將分類確認與motion reset分離：物理幾何不變、sequence/QPC递增、间隔≤250ms且source有效時保留確認計數；新frame仍須當前完整HUD及容量有效。>100ms間隔或新epoch清空全部tracks／rail anchor，不保留旧触控計畫，不改变input100ms硬期限。缺HUD、重播、長間隔、幾何變化及無效source仍冷啟動。兩項新回歸核對101.7ms後立即分類但新Note只有一點／無crossing，新epoch再次清空motion；缺HUD、251ms、幾何與無效source均重新三幀確認。等待Release回歸，尚未實測。

v55 Release90／90通過（6.26秒），既有scheduler來源期限／取消回歸亦通過。保持planner6／35ms／兩指，開始下一輪完整HD；未把分類快取期限當作輸入期限或延遲性能改善。

v55 run `cpp-observe-17904611692364172`，35ms、185秒STOPPED、9,394消費frames／928真實命令。結算785,445分、Perfect332／Good8／Bad0／Miss53、max combo52、accuracy85.80%、Early0／Late8，較v54退步、未AP。raw SHA-256 `737ed9e5a7df25eac92646a397f61b2447f09bfae1a70e5f5510880a00891fa3`、manual結果／PNG、兩張診斷圖及分析保存。down Tap136／Hold85／Drag102／Flick6，2次本機Drag coverage、3 conflict／2 gate expiry；journal7 source expiry／1 UI revoke。擷取間隔n9393，p50／p95／p99／max=18.0313／39.9216／56.5348／329.6893ms，來源絕對年齡unknown。UI重複撤銷减少不代表實戰總體改善。

## Combo 消失診斷（build v56）

既有近線history failure常在命中特效附近觸發，缺少失敗關聯。observer23只增加中央HUD白色數字形狀計數（上限16），排除上方進度／長rail／較低COMBO字樣；不做數字OCR，不作觸控決策或逐Note判定。獨立read-only latch以兩張當前存在的字形arm，至少兩張且跨12ms的消失觸發；250ms內同物理幾何／遞增QPC及frame的觀察才連續，UI未知／容量失效／長間隔／重播／幾何變化清空。第一個消失畫面取代第二診斷slot，最多兩張、decision發布後copy、input停止後PNG編碼、預設不保留。每次消失的journal事件只列unknown per-note feedback，不反饋排程。三項短回歸核對字形區域、閃爍／突發／重新arm及不連續負例，等待Release驗證。

v56 Release93／93通過（6.49秒）。observer23／planner6／35ms、兩指與輸入期限不變，開始一輪完整HD診斷；合成字形符合不代表所有真實字體／遮擋已驗證，combo消失仍不唯一對應Miss或某顆Note。

v56 run `cpp-observe-17904615370888702`，35ms、185秒STOPPED，9,399消費frames／987真實命令。結算797,265分、Perfect341／Good5／Bad0／Miss47、max combo35、accuracy87.60%、Early0／Late5，未AP。raw SHA-256 `6db4467bd338d34f5590945366ea2c5b01d74553df5d9c4cd52d1cc643864edd`、manual結果／PNG、兩張診斷圖與分析保存。down Tap137／Hold89／Drag110／Flick6，1 coverage／3 conflict，runtime4 source／1 UI revoke。首個消失事件frame878／QPC151433493800200的PNG仍清楚顯示combo3：一條當前白色垂直裝飾線連上數字，full-frame連通區的高度超過字形限制，被誤列0；因此不能拿這張圖推定Miss。下一步只將診斷白色連通區的分割限制在中央HUD區域，並加連接裝飾線的短回歸，遊玩邏輯不改。

v57 observer24只修正診斷分割：中央HUD ROI內先裁切、再建立半解析度白色連通區，mask／queue最多65,536格、計數仍≤16；窄裝飾線本身不符合寬度，與字形相連則保留可見字形。新增六幀先独立／後連接完整垂直線的三筆畫，要求不誤報消失；随后刪除字形、只留直線，仍須兩幀實際觸發。等待Release及當前Debug／嚴格ASan回歸；v27的73項舊驗證不能代替新source。

v57 Release四項GameDiagnostics短回歸通過（101ms），原v56 frame878 PNG單幀重算 `assist-v56-combo-observer24-v57.json` 正確輸出1字形；PNG SHA-256 `dde3f198dbf710f044810ba43208453311f0e4545144263d50fba4e5d3d9a5a8`。這只核對該張圖，不把離線影像回饋input，也不宣告其他所有消失事件已消除。Debug與ASan正在建置，期間沒有遊玩／capture stream。

v57同一份source的Release／Debug／嚴格預設ASan各94／94通過，CTest總時間12.69／12.07／30.99秒。ASAN_OPTIONS未設定，沿用v27的BuildTools14.51與Community14.50 sanitizer支援；pinned第三方vcpkg二進位仍未插樁。build／test日誌均保存，沒有與遊玩／capture stream併行。開始observer24／planner6／35ms的下一輪完整HD診斷，AP仍未達標。

v57 run `cpp-observe-17904621971078348`，35ms／185秒STOPPED，9,518消費frames／993真實命令。結算807,277分、Perfect332／Good16／Bad0／Miss45、max combo91、accuracy87.12%、Early0／Late16，未AP；combo提高不代表總體改善。raw SHA-256 `b791ef0fe210f5b97822ff395e388dcd597cbbbfd01f59dc0d1cb7c5b49d6eae`、manual結果／PNG、兩張診斷圖與分析保存。down Tap134／Hold90／Drag106／Flick6，2 coverage／4 conflict／1 gate expiry，runtime6 source／1 UI revoke。first combo disappearance frame796確實無中央數字，長Hold仍在畫面：Note51在frame782–784連續無候選，intent6成功down QPC152090557282800後，在152090592690600由missing evidence取消並成功up；frame785又恢復同Note，但已完成intent不重播。此段無source／UI revoke。下一步核對頂端裁切藍色body跨過75%畫面高度時的辨識缺口，不放寬60ms Hold grace／100ms來源期限。

v58新增14張裁切長Hold由藍色接近、跨540px高度、到線後轉灰的觀測＋owner回歸，原observer24即通過（82ms）。已有anchor與完整當前輪廓足夠時，近期路徑可以補回，因此不能把v57失敗完全歸於高度限制。另補v58b無任何先前anchor、第一次就由550px裁切body開始的當幀雙側輪廓＋前緣回歸；此例針對direct route目前排除頂端seed的限制。v58與v58b尚未進實戰。

v58b單項確實失敗：首次550px裁切body、兩條完整白rails及清楚leading edge仍沒有候選，日誌保留。v59 observer25僅讓觸及頂端的藍色seed走當前雙側輪廓重建，高度上限95%／底端至少20%畫面，普通seed保持75%／頂部10%排除；前緣雙側填色、paired rails、tail未知、來源與grace期限均不變。另加入無rail／單rail／無fill的裁切負例。此改動補足無歷史幾何路徑，不宣稱v57那三張未保留的missing frame已被唯一定位；等待Release完整回歸及實機驗證。

v59 Release96／97通過（6.97秒）。首次裁切幾何已出現、所有原回歸與新負例通過；唯一失敗是新正例末張讓head從到線跨到590px，卻仍期待既有on-line Hold路徑繼續以500px/s離開線。原路徑在到線後使用stationary leading anchor，並非本例要核對的「首次已裁切接近」階段。v60将五張正例改為550→566px／200px/s，保持全部初始高度>540px、相同輪廓、同ID、leading位置／未知tail／有效預測要求；不放寬幾何容差或原負例。到線轉灰與owner續接仍由v58的14幀回歸核對。v59未進實戰，失敗保留，等待v60完整回歸。

v60 Release97／97通過（6.83秒），observer25／planner6／35ms／兩指不變，開始完整HD驗證。Debug／ASan的94項通過僅屬v57 source，不能代替本次新增裁切seed路徑的完整驗證。

v60 run `cpp-observe-17904630843405836`，35ms／185秒STOPPED，9,676消費frames／1,014真實命令。結算828,053分、Perfect351／Good5／Bad0／Miss37、max combo66、accuracy90.14%、Early0／Late5，未AP，仍未超過歷史最高分或最低Miss。raw SHA-256 `a8334c0fe71660871ae17fa5c2dbd4de372b8fcdc5fa4e2822bc2dd387158dd3`、manual結果／PNG、兩張診斷圖與分析保存，無中途第二擷取。down Tap138／Hold89／Drag105／Flick6，3 coverage／1 conflict／1 target expiry，runtime3 source／1 UI revoke。first combo事件frame1107仍清楚有combo11：黃色ribbon的白邊連接兩字形，寬度超過64px使整個cluster被刪除，不能將事件延後解釋為first Miss延後。

v61只補diagnostics version2，遊玩仍observer25／planner6／35ms。第一slot改存首次近線長Hold幾何消失，latch只保存上一張最多16份坐標／幾何摘要，當前主線／PLAYING有效且同epoch／幾何、≤100ms遞增來源才比较；已近尾端／短body不觸發。兼容新pixel identity仍視為可見，窄碎片不能充完整body，未知UI／長間隔／replay等負例均不延續。附prior frame／Note／QPC，未讀input內部狀態，故不稱active接觸或Miss。第二combo診斷接受與細白邊相連的寬cluster，但額外要求有局部高白ink columns；純橫邊框加窄直線為負例。兩個slot仍write once／總共≤2，先發布decision、後copy、input停止後編碼，預設不運行latch／不存圖；等待100+短回歸。

v61 Release101／101通過（6.70秒），原v60 frame1107 PNG單張重算為1個可見字形cluster；SHA-256 `52e7fabbe4c68cea111eee2b05d9c58104aaaeea1ba7925f0eee80713c3d7db9` 與 `assist-v60-combo-diagnostics2-v61.json` 保留，不代表OCR11或即時逐Note判定。開始相同35ms的完整HD，無同時build／第二capture；AP未達標。

v61 run `cpp-observe-17904636008676799`，35ms／185秒STOPPED exit0，8,866消費frames／1,012真實命令。结算784,351分、Perfect328／Good10／Bad0／Miss55、max combo72、accuracy85.11%、Early1／Late9，未AP；raw SHA-256 `ff8d3aa0e5ee57ff61ff6b95c2fa5e4b9dae550aae7f25581fed7241d30fae1f`，manual结果、结算与两张诊断PNG保存。down Tap137／Hold92／Drag109／Flick6，2 coverage／0 conflict／3 gate expiry，runtime8 source／1 UI revoke；capture interval n8,865，p50／p95／p99／max=19.2107／43.68354／59.205652／558.1463ms。首次Hold诊断frame376：同Note8从前张head(423.99,576)、width141.93／depth504变成当张color_core head(427.78,495.76)、width134.39／depth416，非完全没有候选。PNG显示亮蓝body仍到判定线，白rails从y500左右到576被淡金色特效覆盖；左x348样本(210,197,146)既非中性白、也未满足b<g−55黄类。当前front fill与近期anchor仍有，32px rail gap不足跨越约76px的染色；原完整on-line geometry路径未成功，内部分片继续匹配同ID并刷新input。此图没有单独证明Miss成因或逐Note判定；v62先加入合成连续染色轮廓＋owner测试，针对保留当前paired rails的近期续接验证，来源100ms／anchor90ms／Hold grace60ms不放宽。

v62合成18幀測試在observer25確實於第8幀缺失Hold，舊source短測失敗日誌保留（61ms）。observer26只為近期held fill有效的續接加入原rail±4px、頭後≤96px的淡金染色，仍须每側至少三個當前中性白樣本。新增無anchor、101ms過期、無當前fill、左側全染色無白段、單側輪廓五個負例；正例同ID／幾何／owner全程單一down持續接觸，最後stop釋放。等待完整Release驗證，不能以修正測試當作實戰AP。

v62 Release102／103通過（7.09秒）；染色正例仍失敗於i8，因上一張頭部尚在566px、距線10px，走approaching leading-edge路徑，尚沒有已到線的anchor。這次live frame376的prior head明確是576px，因此v63補一張未染色的到線畫面，從i9再開始相同80px染色；期望同ID／幾何／單Down接觸、所有五負例與實作期限均未放寬。v62未進實戰，失敗日誌保留，等待完整重跑。

v63 Release103／103（7.27秒）、Debug103／103（18.31秒）通過。檢查負例fixture後發現其前置亦尚未到線，因此v64替六個負例補足到線anchor（無anchor例除外），過期例改為最後160ms→261ms仍101ms間隔；另補200px染色超出96px辨識範圍加32px缺口，不能連接更遠白rail的負例。正例与production observer26不改；等待Release／Debug／嚴格ASan當前fixture驗證，v63未進實戰。

v63嚴格ASan103／103亦通過（38.88秒）。v64最終fixture與observer26／planner6／diagnostics2，Release／Debug／嚴格ASan各103／103通過，CTest總時間8.29／16.81／44.52秒；ASAN_OPTIONS未設定、pinned第三方DLL未插樁，build warnings保留。全部build／test停止後才開始35ms／185秒HD；v62／v63沒有實戰。當前六負例確實先建立到線anchor再破壞必要當前證據，正例全程只一Down；不以合成成功宣稱AP。
