# 2026-10-31 桌面 Bad／Miss 修復：全局−70已部署，新Random3待驗

**目前：全局phase−70已部署並正常重開；指定Snow Desert AP655、Manifold AP895通過，第七批新Random3仍0/3、PENDING，task未完成。** native025／frontend192104維持，現用global-phase70-001 library c84／設定c80b。六批已失敗原件與全部指定控制均保留，不沿用為新批成績；不宣稱全374譜面穩定、動態遊戲clock合格或phase單獨因果。

## 全局phase−70目前部署：新第七批Random3從0/3開始

Snow Desert IN13指定控制 `computer-snow-desert-phase70-20261031-045040-0fd586/round-01`，UTC Play `2026-10-30T20:51:18.184Z`，實際 **1,000,000 AP100%、655P／0G／0B／0M、Max655／E0L0**；655接觸／3799步、原生exit0、確認結算、release10／kernel zero、只有result.png。W `snow-desert-phase-controls-001/snow-desert-phase70-AP-observed-score.json` SHA `3bee14868c6777317638e08b580566f6c55021788f840944221c4de19fc43a16`，完整native／quality audit SHA `b4ac10fc2bbc15c4213ecd0ccc84c85a2346d2f46e226ed009a01059d72ec90e`。先前第六批Random的同曲phase−40結果為650P／2G／1B／2M，完整原生仍不滿足品質要求；本次指定控制開啟numeric metadata而GUI基線關閉，保留此差異，不宣稱完全受控的單因素因果。

目前 `local/library-global-phase70-001/test-library.json` SHA `c84c40107d1b727d92bf55f2bc728bdf27d4f5adc5fd7cf9d448f55d1b53b642`，與實際Snow控制plan raw bytes相同。相對已安裝6f前版，**只將全局−40→−70的一個ASCII字元改動；389整個job物件與其他bytes完全相同**。兩個既有Doppel／Manifold job−70維持，其餘372個可前檢曲目的schedule相對提前30ms；所有389列effective phase為−70。Flick／Tap／Drag、source／profile／opening model／CFP、時間及步數預算完全不變；374可離線前檢、15份blockAreaList不支援列均保持。

已逐份讀取並核對374個profile SHA及source／CFP／完整計數，約1.29MB。原firstDOWN最小550.847458ms，套用−70後靜態最小480.847458ms、最大25.875945946s；這些是譜面排程座標，**不是新鮮觀測或實際取得首音後的200ms lead資格**。清單為 `local/library-global-phase70-001/static-first-down-summary.json` SHA `57e51d71c98d773cc822d443f620fc9f29e4b678313899315eacdd400d67bd94`，global tuning summary SHA `473ccebd1cd5a3111e2bc2dad564001dae8665e9c9a0625115f3c671c49ac8f1`；獨立靜態review W `global-phase70-independent-static-review-001.json` SHA `47922a7fbc968a87da93ef46531538169a5a19744369bc2f9fc84c9117d2302f`。本次沒有native prepare或全庫重新native check，沒有重建native025。

Guarded installer先實際validate-only通過，W `global-phase70-install-validation-001.json` SHA `e28465f83ab83f223b30de4eecb5ca96c1959907b0ceeec937b48ead632b5ca0`。root關閉desktop/native owners後明確執行，僅原子更新設定的plan／library／deployment三欄，保留exclusive舊設定備份。安裝report `local/manual-config-history/20261030-205830-374579-66e26cf2-global-phase70-install-report.json` SHA `8b9ef0f0b22920c2b0cf19990516f727061f17190f3aed50e1be1645f7c7ffe1`；**目前設定SHA `c80b55f9cc36dcac3c93f830aaa608615eade293030ba87e54e037564895b0e3`**。Native025 exe efb08／archive850e、frontend `20261030-192104-725465-9d25c589`保持；root已實際重開桌面面板，沒有自動Play。

第七批W `newrandom3-native025-globalphase70-001/`初始化 **0/3、PENDING**。指定Snow AP655與Manifold AP895不計；前六批失敗及全部controls保留。新批必須重新All Songs Random、manual READY／Play，三首各0Bad／0Miss且完整原生結算圖／release／kernel zero，任何失敗重置。本次只是同emulator固定phase假說的部署，不是動態同步／qualified clock或全374遊戲保證；200／100／500／20、epoch／geometry、十指與Stop保持，正常回合只存結算圖。

以下保留此前每階段當時的部署與失敗；目前版本以上述global−70為準。

## native025目前部署：Manifold指定AP895，重新隨機三首待驗

指定回合 `computer-manifold-native025-drag196-29-20261031-043416-c667b7/round-01`，UTC Play `2026-10-30T20:35:04.438Z`，實際Computer結算 **1,000,000 AP100%、895P／0G／0B／0M、Max895、E0L0**。全895接觸／6657步、原生exit0、結算確認、release10／最後kernel zero；正常回合只有result.png。W `manifold-outlined-controls-001/manifold-native025-drag196-AP-observed-score.json` SHA `afedc2151ab404f12bfceb83a5656dfa05813fee651dc95c0c049c2de9ab0dfe`，完整native／quality audit SHA `09c71ccf94d8458bb83276e608cba4639314210211e79ec64cc18326f713f866`。這是指定控制，不計Random。

Manifold使用既有opt-in outlined v13開場、548個Tap的64ms停留（DOWN不變，僅UP相對Tap32延後32ms），只將同SHA來源 `/judgeLineList/0/notesAbove/196` 的ABOVE Drag提前29ms；原生source ID／judgment_ns保留，dispatch與提前端點幾何分列。其他894個接觸保持該Tap64父候選，46個Flick仍為80px／8／38／40，沒有改成FAST30／60。只有Manifold job phase為−70；global維持−40，其餘388整列與原08d相同，包含Doppel原有−70。Profile SHA `ed696815a248b41c1e3b4dfbd32f465dca0d2fd2d9da5acfda9549ab5ab20c38`，CFP `204dd83020d5a5358af7f7c831a57abaa808be77a113712956efcde2f98a0364`，開場model為既有v13 `cef868390d26095e5b0d13fdc634e0deb74b5c5cd37fe011b9bd230b3f6c99f1`。

native025改動實驗串流transport的raw `Touch.pressure`：DOWN／MOVE為1024、UP／release為0；這是emulator原始ABS_MT_PRESSURE整數，並非Android正規化float。它沒有改compiler、譜面、排程時間、座標、pointer容量或release／Stop機制。此前同Tap64／−70僅換壓力的native025控制仍為884P／10G／0B／1M；加入唯一Drag19629後本輪AP，**不能把結果歸因為壓力單獨修復或已取得逐note遊戲callback**。原phase−40、−10、−70／Tap64與combined失敗控制保留；source／kernel配對中初始X未觀測及多解也未補造成完整唯一綁定。

相對immutable native024，來源快照為3個新transport檔：`apps/chart_experiment/streaming_touch.hpp`、`apps/chart_experiment/main.cpp`、`tests/streaming_touch_tests.cpp`；另明示兩個先前已授權的frontend來源差異 `tools/sweep-panel.ps1`、`tools/install_desktop_frontend.py`。其餘209份來源不變，214份source／10份binary、39個critical pins、236件共34,032,469bytes；Windows38/38與portable6/6新離線檢查通過，未把測試當遊戲資格。前端release仍 `20261030-192104-725465-9d25c589`，未隨native025重裝。

| 目前固定產物 | SHA256 |
| --- | --- |
| native025 exe | `efb08c976e68dab64cf188c0f3614784986efe608e29388b35f21a415c3df5cb` |
| native025 archive manifest | `850e2c5419599c6fb9d47f344d1e334b3bc9d26d636e53c9186c034863895f11` |
| W `20261031-native-025-package.json` | `8eaab9d578928663330461f9c3217e14378485b115721451a12994779530c378` |
| W `streaming-pressure-build-001-receipt.json` | `acf6142ab8074ede2865634d09a208317a0c9938963f69a6939247ae6b4f352c` |
| 最終單列library `local/library-manifold-tap64-drag196-29-rowphase70-001/test-library.json` | `6f3ac067ea5530d967da50c8a9e3adb6551cb27d0bcc22ed937c75c7ef0f5568` |
| 設定 `local/manual-launch.json` | `cbde0ccee3c40798cc5b2e7dcb207fffdb7725cc752b6859b6b99958462a94d9` |
| 安裝report `local/manual-config-history/20261030-204025-627129-eb750063-native025-manifold-install-report.json` | `9fe774c8bcf471cdaa2fd6b662a7a38199f7291e9efaf86fe728bed4f9d6e976` |

先以W `native025-manifold-drag196-install-validate-001.json`實際validate-only（SHA `19a7902f4ce8824c9ba7abe48c8b3c7be858fc45a54f6a0c1c8b69c045ef537f`），再於desktop/native owners關閉後由root明確執行原子安裝，保留exclusive舊設定備份。root已重新開啟桌面程式，實際顯示327首／389譜面／374可前檢、manual選曲與READY→人工Play，沒有自動開始；這不是新冷啟動或全庫遊戲資格。15份blockAreaList列仍明示不支援。正常演奏只保存結算圖，首次校正參考檔授權例外保留。

新批次必須真正All Songs Random重新抽三首，各自0Bad／0Miss且完整輸入、原生確認結算／result.png／release／kernel zero才計入；指定AP895、AP921及全部歷史回合不計。當前 **0/3、PENDING**。200ms first-dispatch lead、100ms anchor age、500ms actualUI及20ms step lateness、epoch／geometry失效、十指上限與Stop均維持；沒有把numeric HUD／kernel receipt變成遊戲clock，也沒有新增音訊PLL。

以下各節保留各階段當時狀態與失敗；目前部署以上述native025身份為準。

本輪 AbsoluTe disoRdeR IN16 控制測試得到 **999,016 分、1595 Perfect／5 Good／0 Bad／0 Miss、Max Combo 1600**，原生程式 exit 0、完整輸入後釋放並確認末觸控為零。這是一輪指定曲目的 FC，仍有 5 Good，不能稱為 AP。native022、29ms曲庫與前端已部署，第三次真正冷啟動已自動正向橫畫面通過；native022新批因第三首原生結算未確認而重置0/3。native023已建置、封存並部署，指定云女孩回歸已完整確認結算；新Random第一首有1Miss而失敗，streak重置0/3待修；這次控制測試不計入Random3。

## 主機音訊與同步範圍

先前 DirectSound 環境出現音訊錯誤、稀疏 PCM 與跨輪可變的視覺進度差。媒體音量 0 的既有觀測只有約 6.6% PCM 覆蓋率；新的同 AVD 冷啟動改用 `-audio none`，媒體音量恢復後，獨立 audio-only shadow 約 99.96% 覆蓋，兩段內容前進量與約 110 秒 host 間隔相容。這支持排除先前大幅時序異常，未證明每項改動的單獨因果。

`none` 使用虛擬音訊消耗端，**主機喇叭不播放遊戲聲音**；啟動工具保留 `dsound` 選項。本次選擇的是明示設定，不是自動偵測或靜默切換。原先兩次冷開僅鎖邏輯WM1，physical skin仍直向。後續 `cold-start-only-sensor-skin-landscape-lock1-v2` 在開遊戲前用最多3次clockwise旋轉取得+X正向，再確認WM1；重用已開模擬器不改方向。第三次實際冷開已自動正向橫畫面通過，未再人工旋轉。

音訊只作獨立內容／進度對照，沒有接成輸入 PLL。gRPC AudioPacket timestamp 是 server 讀出環形緩衝後的時間，不能當成首個／最後樣本的精確時間。數字 HUD、進度亮點及譜面 head shadow 都是有界 metadata 診斷，不能續租輸入權限或證明像素 age。實驗 static suffix clock probe 預設關閉；本輪成功沒有使用它。既有 CURRENT UI 500ms、步驟遲到 20ms、epoch／geometry 失效、release-all 與人工 Stop 路徑保留。

## 選定來源與觸控順序

新增 opt-in `above_drag_source_ids`：只接受同一原譜 SHA／JSON pointer 的 ABOVE Drag，拒絕未知、重複、錯譜面來源、錯類型或錯側；最多 2048 個 ID，每個最多 512 bytes。排序後的集合與政策寫入 fingerprint／provenance。空集合保留舊預設與既有 default CFP；原始 `judgment_ns`、來源身份與譜面 bytes 不變，提前量另記於 `dispatch_judgment_ns`。

Abs 來源選擇完整掃描 347 個 ABOVE Drag 對 955 個 Tap，共 135 組原生 J 相隔 150–250ms 的配對。在 Drag 原始 DOWN 時間重算未來 Tap 的判定線 pose，按切向距離絕對值 ≤151.2px 選出 **29 個 Drag**；不以法向距離排除，所有配對及投影診斷均保留。151.2px 是社群參照候選，並非已量測的 Phigros 判定界，也不是只挑已失分的音符。

30ms 提前會把密集段七組 Drag 放到前一 Tap 之前 **0.703125ms**；實際 Write metadata 確認 Drag 先、Tap 後約 0.386–0.531ms。這形成「Drag 可能先命中目前 Tap，Tap 自己的 DOWN 再碰下一顆同 x Tap」的可檢驗候選，並無遊戲逐 note callback 可證明實際配對。20ms 讓 Tap 先 9.296875ms，清掉密集段，但約 56 秒仍有一個 Bad；29ms 則保留 Tap 先 **0.296875ms**，同一首實測達到 0 Bad／0 Miss。串流仍依序送 DOWN；排程順序不等於 app callback 或取樣時間保證。

成功設定只套用本候選的 Abs IN：29 個指定 Drag 整接觸提前 29ms；其中 27 個落點依法在提前端點重算。**1118 個 Tap／Hold 接觸與 ownership 完全相同，130 個 Flick 的實際步驟／幾何保持 FAST 父候選一致**；其餘 Drag／Flick 只有新增原生 dispatch metadata，沒有額外接觸／時間／幾何改動。原生音符 1600、事件 4790、峰值 4 指、完整來源均保留。

Flick 為 `judgment-anchored-diagonal-sampled-v2`，80px／lead 30ms／travel 60ms／release 40ms，三個 MOVE 的來源時間為 J+4.285714／17.142857／30ms，DOWN J−30ms、UP J+70ms；Tap32、Drag32、phase −40ms 不變。此 Flick 設定不推廣成全庫預設。候選庫其餘 388 列與 FAST 父候選完全相同，仍保留 389 譜面、374 可準備、15 份 blockAreaList 不支援列。

## 實際控制測試：完整保留失敗

以下均為同一首 Abs IN 的指定控制，沒有隨機資格；跨輪環境、開場與派生幾何不全是單一變量，不由分數差聲稱因果已定。

| 控制 | 分數 | Perfect／Good／Bad／Miss | Max Combo | 回合尾碼 |
| --- | ---: | --- | ---: | --- |
| NULL、原 V1 48px／8／20／6、Tap32 | 931344 | 1583／10／7／0 | 596 | `234832-6f5d66` |
| NULL、FAST V2 80px／30／60／40 | 939966 | 1578／17／5／0 | 738 | `000025-d24444` |
| 同 FAST、29 個來源提前 30ms | 957497 | 1589／7／4／0 | 978 | `001938-4f0ff2` |
| 同 FAST、同來源提前 20ms | 975713 | 1595／4／1／0 | 1233 | `003509-a446f6` |
| 同 FAST、同來源提前 29ms | **999016** | **1595／5／0／0** | **1600** | `004532-423d0a` |

20ms 唯一重置約在 56 秒，候選為 Drag305 `/judgeLineList/0/notesAbove/304` 與未來 Tap1422 `/judgeLineList/7/notesAbove/1`；首個低 Combo 影格送達比 Tap 自身 Write 早 3.4558ms。158px 的界外 Drag304 替代也保留，未把候選當成已取得逐 note 判定證據。29ms 成功回合為 `computer-abs-above29-29-fast-20261031-004532-423d0a/round-01`。

## 建置、來源與固定身份

native022 封存了實際建置來源／binary，Windows native CTest **38/38**、獨立 portable CTest **6/6** 通過；測試本身不進行裝置探索或遊戲輸入。相對 native021，來源快照保留 191 份既有檔案 bytes，另列 14 份修改、9 份新增、無刪除；合計 214 份 source、37 個 critical source pins。這些是建置與相容性範圍，不轉移 native021 的遊戲資格。

功能來源：

- [接點選項](../include/rhythm/contact_compiler.hpp)、[closed source lookup／prejudge／fingerprint](../src/rhythm/contact_compiler.cpp)、[compiler 測例](../tests/contact_compiler_tests.cpp)。
- [profile／CLI／metadata](../apps/chart_experiment/main.cpp)、[runtime 明示權限與來源檢查](../apps/chart_experiment/experimental_runtime.hpp)、[runtime 測例](../tests/experimental_runtime_tests.cpp)。
- [串流 Write／釋放](../apps/chart_experiment/streaming_touch.hpp)、[桌面 cold-start／audio 選項](../tools/desktop-start-runtime.ps1)、[桌面 bootstrap](../tools/desktop-bootstrap.ps1)、[面板](../tools/sweep-panel.ps1)。

本機 W 代表 `C:/Users/wurre/Documents/Codex/2026-10-28/new-chat/work`。私有譜面、音源、參考 RGB 與執行證據不由 clone 取得，也不提交本頁。

| 固定產物 | SHA256 |
| --- | --- |
| native022 `chart_experiment.exe` | `fa8d71e25b67308eb5daa509652afd2dfeb79acd187d6984f143066897c64ec0` |
| native022 archive manifest | `88fab9bca5a755a118725eee97969fce517b974e9c67f5c065c548f36502a498` |
| W `above-drag-filter-build-001-receipt.json` | `c791ea649a97e0d4cc647b8fecece6a02bf70d4e2814843a980ab1c2e454061d` |
| W `abs-null-v1-above-drag-filter-002/above-source-ids.json` | `a9814b8d9f016939d825458880b5f8c1dc8ea6caf104eaa0b51433dc76f63fbe` |
| 29ms 候選 `test-library.json` | `4ec1487fec1fc9cce6580b10673bd786da1458e7350915230a7f3f62a1c5be2a` |
| 29ms Abs `song-profile.json` | `4600f05e93c758ac20b0c42d22af9abe05ba7ee176e9d7493738e8d9bfe1a4e2` |
| 29ms Abs compiled CFP | `74e33fe509de4c37f36b7a09f12d9c0823433ba9aedcc868c68c788c82fce80c` |
| W `abs-above29-29-fast-score-001.json` | `736d13400bcecac08494eca830579476e9506d7e2580bd9b7f40f5bcd65cb67d` |

補充診斷索引：W `emulator-audio-timestamp-contract-review-001.json`、`abs-null-audio-control-content-timing-review-001.json`、`abs-above29-30ms-order-reversal-proof-001.json`、`abs-above29-20ms-single-Bad-localization-001.json`。前兩者保留音訊／畫面時鐘界線，後兩者保留實際 Write、原始 OCR 與候選配對的不確定性。

## 部署／Cold verify／新 Random3

**native022已部署且第三次實際冷開通過；該版新Random批次失敗並重置0/3，後續native023已部署且指定回歸通過、新Random3待驗。** native022與29ms曲庫已安裝，當前前端為 `20261030-170826-150838-e47d07c0`，設定SHA256 `0b30fae7d374e6115d34f2d574f00bdc4f5b0c81f331153fa4da2b6415dd0632`。runtime／library安裝receipt為W `20261030-165547-849006-ac717b7a-native022-runtime-library-install.json`，SHA256 `5a8d43eb1f131b6b631bdb0719f1a7e8a8fbb0689d9fe125471bb4d69ec2ebb8`；前端切換receipt為 `local/manual-config-history/20261030-170826-150838-e47d07c0-frontend-install-report.json`，SHA256 `65b05785a0724cff72fad7f775b001c0e4196f5bd53350ffb4bda53f5858d7ed`。前端manifest為 `6705a8335d6dc7271436f4510d413be984773e799dc0258b63ad91e252ea1ac2`。先前165551前端設定 `2a60a6abeea40d6a50d27527a367dad527d115e390a4e0c0bc08d3468840d052` 保留於history。安裝時active graph既有紀錄1389件／1,639,965,581 bytes、明示2GiB上限；本文件更新未重新雜湊整庫。

前兩次冷開雖有邏輯WM1／1280×720，physical skin仍直向，須人工Rotate LEFT才正確，均不計冷開通過。後續helper的3檔來源修改及獨立12／49／71項離線檢查，另存W `desktop-cold-skin-normalization-source-receipt-001.json`、`desktop-cold-skin-normalization-test-receipt-001.json`（後者SHA256 `cbf380560c0ccd12a0c5b261f6ff74c33291208961e22901df5cad1ad8e04e51`），不是native022 binary重建或資格移轉。

第三次session `20261031-010832-260` 從桌面冷開：audio none套用，helper自行3次clockwise後+X9.81、WM1／1280×720，observer active0、不要求root或容量fixture；Computer確認遊戲title與面板正向橫置，沒有人工旋轉。小型status原樣封存W `desktop-native022-cold3-actual-status-001.json`，SHA256 `200017303496aa1af166d5b8e271d9af9d770f3c1acaa9fadf69adddf37fd594`；包含實際Computer觀測範圍的receipt為W `desktop-native022-cold3-actual-receipt-001.json`，SHA256 `c7fa2734ba931af6792dc282612b36df771a3c4ee8f89a59f7e762cb6df98bf2`。這只通過一次冷開，不能代替演奏品質驗證。

後續由正式面板manual READY→人工Play進行Random3；Stop／release及只存結算圖維持。首次校正參考檔的既有授權例外保留，正常演奏只留結算圖；數字／音訊shadow不保存額外RGB／PCM／WAV。

重抽新Random3進度 **0/3，PENDING**；native022批次的前兩次通過只保留為歷史，第三次未完整完成。每首必須由真正 All Songs Random 抽選、人工 READY／Play，完整演奏且結算 **0 Bad／0 Miss** 才計入連續成功；Good 不等於 AP，任何失敗保留並重置本批次。本文五個控制回合不計入，歷史 native013／native021 的 operational Random3 亦不沿用。

## native022 新Random批次：前兩首通過，第三首失敗，重置0

這批是真正All Songs Random抽選，draw／Play時間與run分列如下（時間為UTC；判定欄為P/G/B/M）。三張遊戲結算雖均0B/0M，第三首不滿足原生結算與保存條件，**本批不算3/3**。

| 次序 | 曲目 | 遊戲結果 | draw UTC | Play UTC | run | 原生完整性 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | Credits IN13 | 1000000；535/0/0/0 | `2026-10-30T17:15:05.788Z` | `2026-10-30T17:17:14.849Z` | `catalog-manual-panel-20261031-011615-103-3fa0bc` | 原生完整／結算圖／release通過 |
| 2 | いざ、参ります IN15 | 999272；863/2/0/0 | `2026-10-30T17:19:52.638Z` | `2026-10-30T17:21:43.280Z` | `catalog-manual-panel-20261031-012058-381-3b58fe` | 原生完整／結算圖／release通過 |
| 3 | 云女孩 IN12 | 1000000；669/0/0/0 | `2026-10-30T17:24:31.496Z` | `2026-10-30T17:26:02.959Z` | `catalog-manual-panel-20261031-012519-665-50c77f` | 遊戲AP觀察；原生未確認／無result.png，失敗 |

R1、R2各有獨立score與native-audit PASS，都是同一native022／設定／曲庫身份。R3的669 notes／2053 steps與全部Write、release／kernel zero通過；但failure為 `manual_round_ended_without_result_confirmation`、`manual_result_confirmed=false`，沒有 `result.png`。669P AP只依root的實際Computer結算觀察記錄，沒有補造圖、改native `AP_verified`，也不覆蓋本輪失敗。

所有原件位於W `newrandom3-native022-001/`：R1 `random1-credits-score.json`／`random1-credits-native-audit.json`；R2 `random2-iza-mairimasu-score.json`／`random2-iza-mairimasu-native-audit.json`；R3 `random3-cloud-girl-observed-score.json`／`random3-cloud-girl-failed-native-audit.json`。R3 audit SHA256 `4617ab8c733fe1a348de064b5e6d42269f8ad7224f55ea261a002fa6d73b3ae3`；整批 `batch-final-failed-report.json` SHA256 `c2419dd330b3c42622e1e39a43de32ac0a435e46719036bb0053439e730948e7`；重置後 `checkpoint.json` SHA256 `976e4668f43f0d043391e882163e1d0e70f78d7ec727862fa01e5b98949e56af`。前兩首不能沿用至修復後新批次。

## native023 長曲結算期限修正：指定回歸通過，新Random待驗

R3 profile的input duration為180s；原main把同一180s也傳給ManualLifecycle，從第一個fresh HUD起算。開場HUD約在origin前0.92–1.05s出現，故watchdog在origin+178.955–179.079s就終止；輸入已於origin+174.962s完成，而anchor-relative session上限仍是origin+239.96s。這證明當時不是整體session240s用完：較短manual watchdog先終止，結果動畫／辨識來不及形成三個CURRENT證據。

修正只在[desktop runtime limits](../apps/chart_experiment/desktop_runtime_policy.hpp)、[main接線](../apps/chart_experiment/main.cpp)及[ManualLifecycle測例](../tests/manual_lifecycle_tests.cpp)：input budget仍1..600s，manual settlement與session總上限為input+60s（最多660s），政策 `input-budget-plus60s-bounded-manual-settlement-v1`。R3對應180／240／240s；這是first-HUD及anchor各自固定的硬期限，不承諾最後UP之後完整60s，亦不讓新觀測或tick延長期限。3個distinct CURRENT結算影格／至少60ms、500ms UI freshness、20ms遲到與所有release條件不變；未知畫面與到期依然失敗，不當成結算。

native023實際凍結39個critical sources，Windows CTest38/38、portable6/6通過；三組新synthetic tests涵蓋legacy180s提前中止、181s synthetic三觀測確認及240s不可續期，但不等於遊戲回歸通過。main SHA `e033da29fb81a48378e67f13fe2a527dd7e32cfe69eac3fba948fe7f2bf45530`、helper `96448ad761c92079d39b0af80e818acd306fc62682879f126d090581d3d87ca4`、test `b9448117dc3bec89b3780b44e4551cfec8c6ae1c43c698d15c6f7597c5c46f6b`。建置context SHA `14008d402296469b4b023e0e9d5425551ae2b1877f3ebb461af20fd4fc8f0873`；exe SHA `1d0336722344e853feccb125e0fd33cf1f5dc3725b7c09c22b9b10e2251fe442`；W `manual-settlement-build-001-receipt.json` SHA `850f9ffde761dd55c72c3f175f3bb79e5e28a597046aef78f670ab7b82b6cb68`。

native023 native-only安裝於UTC 2026-10-30T17:38:48，設定SHA256 `9fe864dc95fd6e2f39ccf5571564a96efc4de9ebc93c60d7ef1e8214e4180d78`；安裝receipt `local/manual-config-history/20261030-173848-875148-86594927-native023-install-report.json` SHA `9bf73f483208e7cf071c2a17215f9aec1ccf4fdcbba1a2886b297db7b2c24ca8`。Archive manifest SHA `260aeac9b6083c1c47402f5606e818f5c75614e8d32090694e54c395565a803a`，W `20261031-native-023-package.json` SHA `012a68e38c93b8c955af4a927ba7eb283f95beffb042aabe805069b2e3d9a5c7`；source快照相對022共5檔修改（3個settlement來源＋已部署的cold-skin啟動來源／測例）、209份原件不變，無新增／移除，compiler及PAS不變。前端170826、library4ec保持原bytes。root以Computer重開正式面板並在相同none模擬器取得云女孩指定READY；此時只是就緒，沒有把舊022成績當成023遊玩通過。

指定云女孩回歸run `catalog-manual-panel-20261031-013934-842-68abea/round-01` 已實際完成：**1,000,000 AP、669P／0G／0B／0M、Max Combo669**；669 contacts／2053 steps全原序，原生確認結算、failure=null、release／kernel zero，只有 `result.png`。結算影格receipt為origin+**184.195221109s**，超過舊180s input/manual上限，在新manual240s範圍內；compiled-plan SHA仍為失敗022回合相同的 `89738bae7903a9b629fecb28c10858a0401bf410b26c559a2dd9efd59c5b51c6`，沒有靠更換譜面或接點重寫失敗。W `native023-cloud-regression-001/cloud-girl-native-audit.json` SHA `c5cfb3deed37439670c4ba4f5e2c7bde4801d178fcb274f28c2d467bcd842db4`；`cloud-girl-observed-score.json` SHA `ba47af25849973991a77cdef57ce7566c562cd5f2d1e1440ad363256011caefe`。這是指定回歸，**不計Random3**；前一批022失敗原件維持。

新批次W `newrandom3-native023-001/` 從0初始化，使用native023／設定9fe864／曲庫4ec／前端170826。R1真正All Songs Random於UTC17:44:24.019抽到Triumph & Regret IN15，已完整演奏，實際結算974438 V99.63、1005P／8G／0B／1M、Max Combo789、E8／L0；原生完整／結算／kernel zero成功，但判定有1Miss，故嚴格失敗、重置0，不計成功。

**native023指定云女孩回歸通過；其後新Random首輪1Miss失敗，重置0，待修復後重抽全新三首。** 指定回歸不計隨機進度；本頁不把指定回歸當成新Random3，也不宣稱全374份穩定／AP。

## native023 新Random首輪：原生完整，但1 Miss，嚴格失敗

真正Random的Triumph & Regret IN15（run `catalog-manual-panel-20261031-014525-819-a7c470/round-01`）實際Computer結算 **974438 V99.63、1005P／8G／0B／1M、Max Combo789、E8／L0**。root確認完整輸入、原生結算、result.png與release／kernel zero均通過；依本次規則，1Miss仍是失敗，**task未完成、streak重置0**，不能以正常結算替代品質通過。正式原件已保存W `newrandom3-native023-001/`：`random1-triumph-regret-observed-score.json` SHA `579384b8e8f8e0512a2abbdece0769e348773ce240cbe7f6b3657eb95c4063f9`、`random1-triumph-regret-failed-audit.json` SHA `bf2361195c684a168d21616414cbd3b2c80daf855442b91eaee09b46682e9f9b`、reset0的 `checkpoint.json` SHA `6318602de1dfd914359a3b44e9853964273297a4caff7efdf0cc9e5e79fa3c5d`。

名義rank225或790僅為候選（Hold結束延遲會影響判定順序，沒有逐note callback）；不把原Miss直接歸因Flick。後續指定V1／V2對照與單列安裝見下節；舊失敗使用的設定9fe864／曲庫4ec維持原件。

## Triumph單列V2已安裝；兩個指定對照同成績，沒有因果修復結論

指定V1及V2各一輪均實際得到 **997515 V、1006P／8G／0B／0M、Max Combo1014、E8／L0**，原生全輸入／結算圖／release與kernel zero通過；兩首均不是Random。V1的profile與compiled-plan與前一1Miss回合bytes完全相同，表示原政策本身也在下一輪達到FC。因此，接受V2作本機單列候選，**不等於證明它消除了原Miss**；數字OCR只供metadata診斷，沒有逐note判定callback。

W `triumph-numeric-controls-001/` 的V1 `triumph-v1-numeric-observed-score.json` SHA `115c100390225af5f20a295353f197691a0faf8f647a596c8214f357f7007570`、review SHA `bba0e268da206d5e9cfcfde006f9dd355f446dada436222a7f278546c3602eb1`；V2 score SHA `1e33c1f790fe686e99078ad1a34eddbd4566230581f9ae81735ace8da5998ab0`、`triumph-v2-numeric-control-review.json` SHA `7e0fe72d4b218e1f2d3a093b9cd56763ad5241d1d1011684b8301e0fd46f4b8a`，均明示不計Random。

當時只替換Triumph & Regret IN一列為sampled-v2 80px／lead30／travel60／release40ms：904個非Flick contacts／owners exact，110個Flick原生來源／J／anchor／投影exact，7個方向為80px合法screen-fit重選；events2589→2699，峰值2→3，無丟note。其餘388列含Abs29保持exact。Library為 `local/library-triumph-v2-fast-probe-001/test-library.json` SHA `c5a3787f644fa7eda2ffe5037dccb2a0dbf45eb65edc1b97d175dbe3afce793c`，summary SHA `43e2d7e1291883e89593a7bb7a04d32a357e673b9add2f45723e31dfa2bc52fd`。安裝receipt `local/manual-config-history/20261030-175837-539708-d725f3c4-triumph-v2-library-install-report.json` SHA `334ac81175d20d104dfa3eecc882213abaa04a5da2585c375b7e645c6ea910f7`；**該次設定SHA `61daa11f2de7fd2a94438486de3a206e83b04772d3989e6067f62f040f80ccb0`**，native023與前端170826不變。

新批W `newrandom3-native023-v2-001/checkpoint.json` 從 **0/3、task未完成** 初始化，兩批既有失敗及所有指定control只保留歷史，不移入本次streak。待真正All Songs Random重新連續三首全部0B／0M且完整原生結算／result.png／release通過；不把這次單列候選推廣成全庫預設或全374份保證。

## 共通保留DOWN的V2：離線366/366通過，已原子安裝

這次共通政策是 **80px／8／38／40ms**，不是366列一起使用30／60／40。post=30ms，原DOWN完整step維持J−8ms；MOVE為J+4.285714、17.142857、30ms，UP為J+70ms。324906個非Flick contacts／owners exact，24844個Flick原始來源／J／DOWN／anchor／投影exact；完整開場模型、原firstDOWN及特殊above／below來源filter、outlined Drag opt-in等由逐列比較保留。2份Abs／Triumph FAST列（30／60／40）、6份無Flick、15份blockAreaList共23整列exact。沒有丟note或把15份blocked變成可玩。

全部366份native prepare及366份check exit0；獨立metadata review核對完整執行receipt／profile／CFP／來源及預算。374份ready合計357894 notes／1887020 steps，峰值max9、每列step預算仍有界。這些是離線資料完整性證據，不等於374份都實際遊玩通過。`local/library-global-v2-preserved-down-001/summary.json` SHA `b8b723b6850d03810125ccc7a5addd1e03cdcf1801541fcb9faa856f86033948`；W `global-v2-preserved-down-final-independent-review-001.json` SHA `6d1c7281085a7604f2cf7961d412b909f1aec2ce3045ef72bfb1f7ad414a5d80`。

Chariot指定control run `computer-chariot-global-v2-preserved-down-20261031-022234-288ca5/round-01`，UTC Play `2026-10-30T18:23:31.736Z`，實際 **999376 V99.93、1007P／2G／0B／0M、Max Combo1009、E2L0**；全1009 contacts／3836 steps、exit0、原生結算／result.png only／release與kernel zero通過。W `chariot-global-v2-control-001/chariot-global-v2-observed-score.json` SHA `e41cb22504c6d7b3282bbcfdd353cacdc4b405b525220b0429d82571b64c2818`、`chariot-global-v2-native-audit.json` SHA `249c6a48ef496965ac03ec281da92f45acf3387620ed3ff930cd1e9439862d9e`。原2Miss回合與這次指定FC保留為兩份證據；無逐note callback，不能聲稱已證明Flick造成／消除那兩Miss。

正式曲庫 `local/library-global-v2-preserved-down-001/test-library.json` SHA `64bfeb6eb89f4f50ed96ed5a99147432aebd862141a3ae5fbb2935a5f84e0c76` 已原子安裝，**目前設定SHA `e070d0f98f37deefa1afa32ebb011554a9fb4faa7a36f5e754555193e21ab70b`**；native023／前端170826不變。安裝receipt `local/manual-config-history/20261030-182706-359795-3a4fc33b-global-v2-preserved-down-install-report.json` SHA `5236c2718b9f7dda980ed4ffd6763eb3f561f4bc1557d253ebf9b6375d2a4a4f`，只驗metadata／profile pins，沒有重雜湊原譜音訊payload。

新批W `newrandom3-native023-global-v2-001/checkpoint.json` 從 **0/3、task未完成** 開始。Abs／云女孩／Triumph／Chariot指定controls不計入；三個既有失敗批次全部保留。下一步仍須真正All Songs Random連續三首各0B／0M、完整原生結算、result.png及release通過。render cadence不等於touch poll，遊戲判定因果與全庫資格均不由離線或單曲回歸擴張。

## Doppelganger首音0觸控失敗；native024已安裝、見證選項尚未啟用

Global批R1真正Random抽選Doppelganger IN，draw UTC `2026-10-30T18:28:41.630Z`、Play `2026-10-30T18:30:22.973Z`，run `catalog-manual-panel-20261031-022942-622-627595/round-01`。failure為 `first_note_trigger_cancelled: first_note_dispatch_lead_below_200ms`；實際DOWN0／UP0、無分數或result.png。未armed，release requested為空，**沒有release10 RPC**；獨立final kernel zero確認。之後人工no-autotouch觀察不是候選測試，也沒有保存原件，不將其遊戲結果連到新候選。W `newrandom3-native023-global-v2-001/random1-doppelganger-preinput-failed-audit-002.json` SHA `df1cba24ee0af7e01ea7b39af2220a8e9b91f2f0157074fe3337bb55129726a1`，reset0 checkpoint SHA `07c64215f8edfa35c3c65689c1dd58759e746e025469fc9342d84710b390e3fa`；001保留，002明確更正release範圍。

原生首Drag `/judgeLineList/5/notesAbove/0` 的J=3.910714286s；tick232→233以speed10000進畫面後，speed0平台55ticks約736.6ms，至tick288才恢復speed2。solid detector已見首頭；舊路徑要求兩實際影格且位移>6px，要到J前約53.57ms內才足夠，不能滿足200ms lead。早期aliases不能任取；最後unique區間的receipt本身已比phase-adjusted target晚26.490175ms，solver CPU為0也無法通過。這是開場證據形狀不足，不是已證outline錯誤或Flick演奏失分。

[adaptive first-note](../apps/chart_experiment/adaptive_first_note.hpp)新增explicit opt-in `native_head_appearance_witness`，**預設false，僅pure solidDrag，不能與outlined混用**。啟用後model v14保留所有source／temporal aliases，取全部實際support的共同origin交集；只接受真viewport offscreen→full-head visible、unique source／speed-event、連續正speed且transition≤20ms的native certificate。ROI邊界不能代替viewport邊界；selected pair及必要第三幀的全部distinct實際evidence IDs保留。完整head／coverage／rendering仍是有條件假說，source pixel age與clock仍unqualified，沒有用receipt／歷史影格替代新證據。

三source凍結：main `ba29bf87bfa87feb2ec45575d1d4554fe1dd3a8b8be1aff543f93af656698617`、header `7579767fd7c96469d3d2bc273b4221472b6d7ea657210d8025b0a76709a7bc9e`、test `81dcc1269b2dade999a7658e2464b00e033d1bdd743b1c62cbb4e41f2bd0464e`。六組synthetic契約覆蓋default legacy golden hash、viewport／ROI、all aliases、兩跳、19／201ms、duplicate SHA、stale100、lead200及epoch stop；不是遊戲回歸。Windows38/38、portable6/6與39critical通過；W `native-head-appearance-build-001-development/manifest.json` SHA `a55fd26b3cdae7737bfbf8ffd7439ee341578be02c117e1f8bbe63891d7b5dba`，build receipt SHA `c51ef05c411cc800b4805773a2ac26de1d34bfd2aec93fb5b0adc032f2f37377`，exe SHA `fe0842e11e86501085788d071d98cbc87f6f50d3105203f2b645136a117d5ce7`。

native024已封存至 `Desktop/Phigros Runtime/20261031-native-024` 並native-only原子安裝。Archive manifest SHA `ca2a103c9dfdfd612456de76bcec7a526fdbaef65c2a9a1a5a85c866acdc2ff4`，W `20261031-native-024-package.json` SHA `29448d644df7310494514120c7c45a69fd684b202f71f1e86f4dc729f3e0696d`；安裝receipt `local/manual-config-history/20261030-184922-409560-1e47140e-native024-install-report.json` SHA `9f971268a5ae56d2a5bc38ca8f609c3dc78741e7c4ae9f92d21e4872fa3c8db8`。**目前設定SHA `1adc9f889f54fa4331a3d82c60061a65fe7aa31c45f41ede7363d79770763146`**，前端170826及global64bf曲庫不變，既有profile的witness仍false。

**Doppel單列資料與一次實測已完成：開場接受，但4B1M品質失敗；候選未安裝，freshRandom3仍0/3。** 200ms first dispatch lead／100ms anchor／500ms actualUI／20ms lateness不降。之後指定回歸必須取得首音並完整送譜、原生結算／PNG／release；不計Random，再重抽三首。四批既有失敗及controls均保留，不由synthetic或新binary移轉資格。

## v14指定control：開場證據通過，完整演奏但品質不通過

run `computer-doppelganger-appearance-v14-20261031-025026-e37cf1/round-01`、UTC Play `2026-10-30T18:51:11.016Z`，actual first lead **498.730709ms**、current receipt age **24.5831ms**，support IDs完整保留 `whole-song/frame-2495`、`2496`、`2498`。沒有降低200ms lead或100ms anchor gate，亦沒有任選早期aliases。全921 notes／3115 steps與原compiled-plan body bytes exact；原生exit0、結算確認、release10／kernelzero，只有result.png。

實際Computer結算 **920179 S99.42、915P／1G／4B／1M、Max Combo234、E1L0**。因此opening acceptance與native operational pass為true，整體quality pass／installer acceptance為false。這是指定control，不是Random，不能把4B1M記為修復完成，也沒有放寬安裝的0B0M要求。W `doppelganger-appearance-controls-001/doppelganger-v14-observed-score.json` SHA `f771040388c379359b0972e995e2aac33e242dd8fb52e7e9a8738b7110ae5d36`；`doppelganger-v14-native-failed-quality-audit.json` SHA `eb58920a3e816a09ffd2a73ad4209c966cb3699fd45b1fa57ad9608f40cf897c`。

候選只改Doppel一列的explicit witness／opening model，profile SHA `38540671289455d77cc8b02e276aa8eba41988b146dca64d5cb8d6fd941570b6`、opening model SHA `731a7496d73bf760d7f1972d4f43e41f5a11b6cfc689c9fb239de8edaae3c0ee`、compiled CFP仍 `c0086d838fe70c238be14782497cd2ef73206d4aa0f7d6086e08bdc1fc622f5f`。候選library SHA `8992cf8d9e7f34eb70a73bd94a9bd0b9d4570df5b7a9d12d2c8746cdf9c91663` **沒有安裝**。現用設定1adc／global64bf繼續default witness=false；人工待機／先前no-autotouch觀察不作候選資格。

下一步只讀source相容Drag→Tap閉集合與numeric counter時窗分析；目前沒有逐note callback，尚未將4Bad／1Miss對應至確定source IDs，不改clock或放寬guard。開場見證仍是有條件rendering／coverage假說，source pixel age／clock未qualified；freshRandom3維持0/3，須先取得完整0B0M回歸，才可接受新單列安裝並重新抽三首。

## 同closed Drag5計畫phase −70指定AP；後續per-job部署見下節

前一個closed-source Drag5候選將5個明示ABOVE Drag提前29ms、開啟v14 witness；phase−40指定control為 **918518 A99.23、912P／3G／5B／1M、Max234／E0L3**，native完整但品質失敗，原件保留。其後使用**相同profile／chart／audio／CFP／完整compiled-plan body與同5個source IDs**，只改phase−40→−70：run `computer-doppelganger-drag5-phase70-20261031-031250-a2c6c2/round-01`，UTC Play `2026-10-30T19:14:14.722Z`，實際 **1000000 AP100%、921P／0G／0B／0M、Max921／E0L0**；921 contacts／3115 steps、原生exit0／結算確認、release10／kernelzero、只存result.png通過。

phase70實際first lead471.454909ms、anchor age18.3509ms，保留3個actual support IDs3931／3934／3936，200／100／500／20未降。這個相同body對照支持offset／phase因素；跨輪開場與像素age未獨立量測，**不稱為qualified遊戲clock，也不證實每個Drag的個別判定因果**，不由單次AP推論全庫AP。

W `doppelganger-appearance-controls-001/doppelganger-drag5-phase70-observed-score.json` SHA `57cd78bbc7a342c7c49e65609caf864e00c13c56af29876ad25e518e8435b478`，`doppelganger-drag5-phase70-native-audit.json` SHA `026947799cc27b24828e208a9bdbaf5b5c64d326c34058b5aef6766e80378695`；先前−40 score SHA `46d2b66d3ef7179d1533f240ccf6db686b4e652e1a30efd2d7c5f59f8b7df430`、failed audit SHA `649bef20a8e2c01f36ea22a7db57fe38b5634299eab8ec6a80a0a1b3e40386cd`。同profile SHA `aca1217bc1c0f5fd6f7d1a5120fa826fb8bec249aace5220eb60f4905a7ce35d`、CFP `f1835deeed41dbd8bae2dc9536dd2e38d624ebdaf750b331116c48c3990bf62b`；phase70測試library SHA `cbd6e9b19cdb1a2ec585c4f42630d8390598681be7b0e39370e1aa50bcc84e2c` **未安裝**，不是可直接套全庫−70的設定。

接下來只做最小strict per-job `phase_ms` 前端支援：僅Doppel一列覆寫−70，global plan繼續−40，其餘388列保持；搭配該列witness／closed5Drag的部署bundle當時待來源freeze、review、離線測試與版本化安裝，現已完成如下節。**沒有重建native024；此段測試當時為1adc／global64bf／前端170826，最新部署身份見下節。** 指定AP只完成單曲控制，不計Random；新批須在正式單列部署後重新All Songs Random三首各0B0M及完整原生結算／PNG／release。freshRandom3仍0/3，四批失敗與每個失敗control完整保留。

## 最終單列與strict per-job前端已部署；freshRandom3仍0/3

正式library為 `local/library-doppelganger-drag5-rowphase70-001/test-library.json` SHA `08d957810fe72c175640fa4d41d2d69227029c5cb92aa874b119888711fab098`，profile／compiled body／CFP與指定AP控制相同；Doppel一列合併v14 witness、明示closed5個Drag提前29ms與 `phase_ms=-70`，global plan保持−40，其餘388整列exact。它是單列phase override，沒有把全庫改成−70。Library install receipt `local/manual-config-history/20261030-192059-171857-aa5b18ef-doppel-rowphase70-install-report.json` SHA `9a187ccace7cb6129ee503c61df6845141617a1f3d9598eb39ebda72ec1cbff4`；該步中間設定45f7與隨後前端切換後設定分別保留。

前端release `20261030-192104-725465-9d25c589`，manifest SHA `03ed09966f5991bafc013048539e62a6795d5754062c7420373677bb15df3d3a`；[sweep-panel](../tools/sweep-panel.ps1) SHA `d201bbaf03272d286978105525df616cf7cd5dd1683209d895fc5e08f236f8f9`。strict整數job phase優先於plan，未給job時才fallback；null／字串／bool／float／超界值在check與run參數建立均拒絕。獨立W `desktop-row-phase-source-validation-001.json` SHA `7e009b591d1be65d0490a7f509030fcd3b135ec2b3538f9bb313bd230698f907` 記104項PS SelfTest exit0，沒有啟動native或視窗、沒有裝置測試；安裝器不自行執行測試，報告只引用各自獨立receipt，不將舊「user禁測」文案當成目前驗證範圍。

Frontend install receipt `local/manual-config-history/20261030-192104-725465-9d25c589-frontend-install-report.json` SHA `6456e4f22ef94a7cb29a33644f029a91fe88413ecb7b2b4d048ea7b8e48b9e72`；**目前設定SHA `46da6bd9fe0c48dfa54ed666e611952e8cf3b534fc7af377a79b5a01318ca71a`**。Native024 exe `fe0842e11e86501085788d071d98cbc87f6f50d3105203f2b645136a117d5ce7`／archive manifest `ca2a103c9dfdfd612456de76bcec7a526fdbaef65c2a9a1a5a85c866acdc2ff4`完全不變，沒有新native rebuild。root以Computer重開正式面板，重用原none backend模擬器；這不是另一次冷開資格，也不是Random結果。

新批預定W `newrandom3-native024-rowphase70-001/`，**0/3、task未完成**；只從這次正式部署後真正All Songs Random抽選、manual READY／Play，連續三首各0B0M、完整原生結算／result.png／release及kernel zero才完成。指定AP921及全部舊controls不計，四批既有失敗原件保留；任何新失敗仍需保留／修復後重抽三首。開場／渲染／時鐘資格邊界不因單次AP或新前端改變。

## 第五批Random失敗：Manifold Hypothesis 0觸控／觀測預算用盡

native024-rowphase70新批R1真正Random抽選Manifold Hypothesis IN14，draw UTC `2026-10-30T19:22:45.401Z`、Play `2026-10-30T19:24:46.744Z`；run `catalog-manual-panel-20261031-032354-577-91808f/round-01`。小型manifest原件SHA `779f38c4c6c09e91955a88a6cc37fa8a7ed888bae2104eba1ae7f0d03aaefdfa`，failure `first_note_trigger_cancelled: first_note_observation_budget`，DOWN0／UP0、whole-song sequence未完成、manual result未確認、沒有PNG，final kernel zero確認。不把zero-input的release狀態誤稱為十指release RPC或完整遊玩通過。

root實際Computer觀察自然結算 **0分 F00%、0P／0G／0B／895M、Max0／E0L0**；這個人類觀察與「原生沒有確認結果／未保存result.png」分列，未補造score／image檔。正式failed audit與checkpoint由audit agent在W `newrandom3-native024-rowphase70-001/`保存，本筆先明示直接原件／root觀察範圍，不造尚未落地的audit pin。**此為第五批失敗，嚴格streak再次重置0，task未完成**；四批先前失敗、每份指定control與Doppel AP全部保留，不沿用為新三首。

現用native024／frontend192104／library08d／設定46da不變。Manifold實際effective phase−40、舊default witness=false，非Doppel的−70 override；首音觀測／model／budget原因仍診斷中，未宣稱Flick失分、來源clock或其他原因已證實。沒有新候選部署，也沒有降低200／100／500／20。後續需具體開場修復與指定回歸，再重抽全新三首；新Random仍0/3。

本文件更新沒有執行新建置、測試、裝置或 UI 操作；以上結果引用各自固定原件。
