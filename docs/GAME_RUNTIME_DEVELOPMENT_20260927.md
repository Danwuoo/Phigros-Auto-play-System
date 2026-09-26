# G0／G1 開發紀錄（2026-09-27）

狀態：已實作第一批 runtime／scheduler 接線、保守遊戲 observer 與自動 PLAY；G1 的完整實戰驗收、G2 遊玩觸控、HD／IN AP 均未完成。以下實機資料不當作 AP 或完整遊戲能力證據。

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

v3 結算人工核對仍為 Glaciaxion HD Lv.6、0 分、F、Perfect 0／Good 0／Bad 0／Miss 393。自動 PLAY 路徑已完成兩次實測；G0／G1 開發接線可 review，完整 G1 尚欠暫停／恢復、RESULT 分類、黃色外框／重疊類型與足夠人工預測樣本，G2 不開放。
