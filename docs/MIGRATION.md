# 分支抽取與依賴邊界

## 2026-11-07 桌面原始碼基線

本機提交 `299570b` 收錄已部署 native025 封存內原先未提交的 73 個程式、工具與測試檔。收錄前逐檔核對工作樹的原始位元組。發行封存的 SHA-256 仍是檔案位元組的依據，Git blob 另受 Git 換行設定影響。

抽取後的兩個 PAS 檔案在此基線收錄既有 gRPC 連線預熱變更。新的 `GrpcTouch::prepare_connection` 由實驗入口在 READY 前呼叫，只允許閒置且無故障的 backend，採單調時鐘與最多兩秒的等待。原始抽取 blob 保留在 `EXTRACTION.json`，發行與 Git 的新身分如下。

| 檔案 | 桌面基線 Git blob | native025 原始檔 SHA-256 |
| --- | --- | --- |
| `include/pas/emulator.hpp` | `b909d6ddc68be9d5aec71f696f277662983cb66d` | `c4d5361d1990e883570aa02e117c54c54dc4358a48a182b2cc8eaba9b4a54437` |
| `src/emulator.cpp` | `c6dfe46536cafde4c5a4ffd0defa88eed92fdab2` | `6f6092823a1dbd12f13d85158381a4d151641c8175f1701f2aa03e2f9d4b84e2` |

## 2026-10-31 目前：全局phase−70已部署，第七批Random3待驗

在native025／frontend192104不變下，從6f曲庫只改全局phase−40→−70的一個raw字元，389 job／374 profile與CFP／所有source及15block列不變；Doppel／Manifold原job−70保持，372個其餘可前檢列新提前30ms。新library c84、設定c80b已原子安裝並正常重開，只變plan／library／deployment三欄，舊設定備份及六批失敗保留。指定Snow AP655與Manifold AP895均原生完整，但不計新第七批0/3；不宣稱全庫或qualified clock。未重建native或改PAS／compiler／前端，未加入私人素材；固定身份與實測見[目前修復紀錄](DESKTOP_BAD_MISS_FIX_2026-10-31.md)。

## 2026-10-31 native025＋Manifold單列已部署（新Random3待驗）

native025只新增3個實驗transport來源修改（streaming_touch／main／其測例），將raw壓力DOWN／MOVE1024、UP／release0；相對024另明列2個已授權歷史frontend源差異，其餘209來源保持、compiler及PAS來源不變。214source／39critical、Windows38/38與portable6/6已新建置核對；沒有合併main或加入私人素材。指定Manifold AP895原生完整後，以單列outlined v13＋Tap64＋ONLY Drag196提前29ms＋job phase−70安裝，global−40與其他388整列（含Doppel−70）exact；frontend192104不變。Current native025／library6f3a／configcbde已正常重開，327首389譜面374可前檢／15block。指定控制不計Random，五批失敗保留，新批0/3PENDING；不宣稱壓力單獨因果或全庫資格。[固定來源、部署與實測](DESKTOP_BAD_MISS_FIX_2026-10-31.md)。

以下為各歷史階段當時的遷移範圍。

2026-10-30 native013接續：相對native012，partial-score修正限實驗HUD helper、main的有界觀測metadata及對應合成測例；PAS傳輸、contact compiler、開場模型、原譜／48px曲庫與固定前端未再變更。20項關鍵來源建置前後SHA、新Windows build與最終33/33、portable4/4 receipt另存；封存222件／33,410,905bytes後原子切換runtime參照，原設定備份與舊回合保留。指定Xep+ion回歸及其後第二批三首Random完整完成，各有13＋6項稽核；第一批失敗仍保留，未改寫成成功、未合併main或加入私人原件。詳見 [三首實測與修復](DESKTOP_THREE_RANDOM_2026-10-30.md)。

2026-10-29 中途停止接續：改動限實驗main的等待／演奏deadline及prearm日誌取樣、learned HUD聯合分派、sweep面板manifest讀取。保留PAS sources與曲庫／compiler／開場模型未改；重新建置、封存與安裝native012，未執行測試。既有失敗原件不改寫，詳見[中途停止修復](DESKTOP_MID_SONG_STOP_FIX_2026-10-29.md)。

2026-10-29 後續桌面演奏更新：依使用者要求，安裝先前凍結的 b451 演奏核心與32px短斜向Flick，離線製作相容的278份開場profile並沿用96份與15份blockAreaList原列。只做部署資料製作與檔案封裝，不重編譯／測試／操作裝置；既有native來源及舊實測原件不修改。這次由使用者自行實測，沒有繼承先前未完成的五首驗證資格。見[桌面演奏核心更新](DESKTOP_PLAYBACK_MANUAL_UPDATE_2026-10-29.md)。

2026-10-29 桌面啟動修復：將可修改的開發腳本與已安裝介面分開，新增固定 bootstrap、啟動 runtime 與版本快照安裝器；設定整組切換並保留原件備份。只調整面板日誌位置與前端載入路徑，未修改保留 PAS sources、native binary、譜面／profile 或既有實測紀錄。使用者要求本輪不測試，詳見 [桌面介面安裝](DESKTOP_FRONTEND_INSTALL.md)。

2026-10-25 完整曲庫接續新增APK／catalog精確身份報告、native-FSB5身份manifest／profile、本機批量準備與v2人工面板；保留Legacy v1固定清單驗證。AT新增獨立難度policy與實際校正參考綁定，原IN門檻不改。移除獨立2000 notes拒絕，接點數改由既有有限step預算推導；時間／5指／停止／釋放與原譜完整性保留。改動限新實驗app／工具／測例，保留PAS／proto／third_party未改；私人校正／原譜／APK／音源／實測不入Git。見[完整面板紀錄](FULL_LIBRARY_PANEL_2026-10-25.md)。

2026-10-25 本機接續新增有界 runtime archive 工具及其邊界測試，並修正原生稽核測試的 Windows 路徑判斷；保留 PAS／proto／third_party sources 未改。新 binary、profiles、launch configs 與比較證據另存，不繼承 build006／AP 資格，見 [本機 native 與 Flick 紀錄](LOCAL_NATIVE_FLICK_2026-10-25.md)。

同日依使用者追加要求，修改實驗 app 的結算標籤判讀與影像保存、metadata-only HUD 及分析／面板提示；以新 v2 policy 明確啟用，舊 v1 與原件保留。沒有改保留 PAS sources／傳輸／接點 compiler 或輸入期限，詳見 [結算與保存政策](RESULT_RECOGNITION_AND_STORAGE.md)。

其後依使用者要求檢查Flick折返，另新增opt-in `judgment-anchored-straight-v1`到新contact compiler與實驗profile／CLI／manifest，並保留候選產生與分析工具的policy身份。缺欄位仍採舊dynamic-line-v1，不改共用預設、時機或非Flick路徑。保留PAS／proto／third_party不變；新幾何provenance與fingerprint獨立，不能繼承舊AP。見[Flick路徑契約](FLICK_TUNING.md)。

來源 commit：`2d479e17fa1566c089dce019cbb11eb883ba47fd`。
目的分支：`feature/chart-driven-multigame`。

這是以來源 commit 為 parent、建立精選新 tree 的普通 Git 分支。**不是 orphan branch，也沒有清除 Git 歷史**；目前 checkout 不帶入舊實驗，但祖先仍可存取那些檔案，因此不宣稱完整 clone 的歷史體積會縮小。

## 原樣保留（Git blob/tree 身份見 EXTRACTION.json）

| 檔案／群組 | 保留理由 |
| --- | --- |
| `include/pas/core.hpp`, `src/core.cpp` | Clock、LatestFrame、ContactPlan、ContactScheduler、FakeTouchBackend、PixelCoordinateMap |
| `include/pas/adb.hpp`, `src/adb.cpp` | 既有裝置管理／畫面輔助工具；不是逐 note 的 shell tap 主通道 |
| `include/pas/emulator.hpp`, `src/emulator.cpp` | GrpcCapture、GrpcTouch、release 與 transport failure 處理 |
| `include/pas/grpc_transport.hpp`, `src/grpc_transport.cpp` | 既有 transport 設定與 patch 契約 |
| `include/pas/touch_evidence.hpp`, `src/touch_evidence.cpp` | 既有輸入證據解碼與路徑檢查；舊 core tests 的必要依賴 |
| `include/pas/manual_score_budget.hpp` | 有界 session 的可複用 budget helper；新 runtime 尚未接通 |
| `tests/core_tests.cpp` | 舊核心回歸來源；本輪未在 Windows 執行 |
| `proto/`, `third_party/` | emulator proto 及 gRPC overlay 完整來源／原註記 |
| `.gitattributes`, `vcpkg-configuration.json` | 原有 Git 屬性與 overlay 路徑 |

共 14 個獨立保留檔案，加上 proto 的 1 檔及 third_party 的 14 檔：29 個原樣保留檔案。新建／重寫 13 個檔案；預期 tree 合計 42 個檔案。

以上為初次抽取 commit `3be959e` 的清單，不是後續 checkout 的固定檔案數。2026-10-23 的本機接續補回三份錄製／逐幀分析／dataset 原文件及 `external-originals.json`，其基準 blob 身份另列於 [LEGACY_ASSETS.json](LEGACY_ASSETS.json)。保留的 PAS sources 不變。

## 不帶入這個分支的現行 tree

`.idea/`、舊 `research/`、大型 `docs/research/`、開發歷史、舊 fixtures、舊 tools/scripts、舊 configs、一般 benchmark 與實驗 tests、舊 apps/launchers、vision-first game/motion/tracking/runtime/manual_session、native/scrcpy capture 替代路徑，均留在來源 commit/main。

這不是判定那些資料沒有價值，而是避免重建依賴再拉回整個 research graph。完整基準與分數可從 [原 README](https://github.com/Danwuoo/Phigros-Auto-play-System/blob/2d479e17fa1566c089dce019cbb11eb883ba47fd/README.md) 追溯。

抽取時遺漏了對其他完整畫面紀錄的接續說明；20 首 score-only 不能代表全部歷史。錄製文件與核對索引現已精選補回，舊錄製／回放程式仍可按基準 commit 取用，沒有加入 default build。大型原始 PNG、影片、journal、已編譯 binary 及本機 AVD 不會因 Git clone 而恢復；`measurements/` 亦維持忽略，避免把找回的本機資產混入版本控制。見 [ASSET_RECOVERY.md](ASSET_RECOVERY.md)。

## 建置與資格

新的 default build 只建置跨平台、無第三方依賴的 memory prototype。Windows PAS core、gRPC 與舊 GTest 明確 opt-in；新的 CMake 不參照移除的 research、舊 provenance generated header 或既存 main80 objects。

gRPC overlay 不會被默默移除或改成 stock。`vcpkg.json` 延用來源 baseline，但把依賴分成 optional features。初次抽取未測 Windows；本機接續已重建 core／ADB，並修正 `pas_adb` 遺漏的 JSON dependency（`legacy-core` feature、CMake package discovery 與 PUBLIC target link）。原 PAS source blobs 不變。gRPC fresh build、backend ABI/服務版本與裝置能力仍需驗證。

歷史 cost NOT_READY、live UNQUALIFIED、STOP 與分數只屬於舊工作；新分支不重寫、不繼承為通過，不自動開遊戲。沒有建立合併到 main 的 PR，也沒有改預設分支。

## 2026-10-24 共通匯入與首音候選來源紀錄

新增明確source-priority／constant-empty匯入政策、完整原始宣告／異常映射；嚴格預設不變。接點編譯新增可選解析投影速度上界，政策／來源自動綁入fingerprint；首音共通模型另綁來源與政策。chart_experiment新增不可修改的預驗證計畫移轉、共通IN HUD候選及Windows Unicode路徑處理。保留的14個PAS blobs、proto、third_party及main不變；原AP封存不重標。原譜與私有成果只在repo外，未寫回或提交。具體語義／限制見 [共通政策](CHAPTER_LEGACY_COMMON_POLICIES.md)。

後續新增terminal-only結果caption特徵，以及明確的每輪兩實際開場觀測標題冗餘HUD候選；只改實驗app／新header／測例與launcher，不改保留PAS sources。診斷採用既有immutable frame lease／原有capture與publication時戳，並把有界RGB／日誌配置移至READY前；沒有修改PAS pool實作或重新定義receipt時鐘。所有原始失敗與成功封存分別保留。

## 2026-10-31 native022 與單曲 Bad／Miss 修復（新 Random3 待驗）

新增明示 ABOVE Drag 原生來源閉集合、相容 fingerprint／runtime gate 與 desktop audio none／cold-start-only 橫向設定；native022 source freeze 分列相對 native021 的 14 修改／9 新增／191 既有 bytes 不變。Abs IN 的 29 個來源提前 29ms，保持原始 J／來源，實測 1595P／5G／0B／0M、Max Combo1600；這是一輪指定控制，不是 AP 或新 Random3。38/38 Windows native、6/6 portable 通過；native022＋29ms曲庫及前端170826已部署，coldskin-v2第三次實際冷開自動正向橫畫面通過（前兩次physical失敗保留）；新Random3仍0/3待驗。完整政策、失敗序列與固定身份見 [本輪紀錄](DESKTOP_BAD_MISS_FIX_2026-10-31.md)。

## 2026-10-31 native023結算期限修正（部署／回歸／新Random待驗）

native022新Random批R1 Credits AP、R2いざ、参ります FC完整通過；R3云女孩雖實際畫面669P AP，卻原生未確認結算／無result.png，故整批失敗、streak重置0，不沿用前兩首。main原把180s input budget當manual watchdog，早於既有240s session中止；新helper只讓manual settlement採input+60s固定總上限，input／500ms freshness／20ms／三CURRENT result條件不變。native02339critical、Windows38/38與portable6/6通過，已native-only部署（設定9fe864）；前端／曲庫不變；指定云女孩AP669且原生origin+184.195s確認結算、result.png／release通過，不計隨機。其後新Random首輪Triumph & Regret 1005P8G0B1M雖原生完整，仍嚴格失敗／重置0；後續V1／V2指定對照均997515／1006P8G0B0M，不能認定V2因果修復。Triumph單列V2已安裝（設定61daa／曲庫c5a，其他388列exact含Abs29），新批Random仍0/3待驗。見[失敗原件與凍結身份](DESKTOP_BAD_MISS_FIX_2026-10-31.md)。

## 2026-10-31 共通保留DOWN的V2已離線完成並安裝（新Random待驗）

Chariot新Random首輪原生完整但2Miss，嚴格streak再次重置0。2worker完成366prepare＋366check全PASS，366列為80／8／38／40且原DOWN exact；Abs／Triumph FAST2列30／60／40、無Flick6、blockAreaList15整列保留。324906非Flick／24844Flick來源DOWN exact；Chariot指定1007P2G0B0M完整回歸通過但不計Random。已安裝曲庫64bf／設定e070，native023／前端170826不變；新Random0/3，不由render FPS推定touch poll或失分因果，不把離線遷移當全曲遊戲資格。[目前紀錄](DESKTOP_BAD_MISS_FIX_2026-10-31.md)。

## 2026-10-31 native024已安裝；appearance witness資料／實測待驗

Doppelganger新Random被200ms首音門檻取消，0觸控／無score或PNG；未armed故無release10 RPC，kernelzero確認。四批失敗保留、strict0。新增預設false、pure solidDrag-only的v14 witness：all aliases＋真viewport transition certificate＋2／必要3個distinct實際supports共同origin；coverage／rendering／pixel age／clock仍unqualified。三source、39critical、Windows38/38／portable6/6通過，native024已native-only安裝（設定1adc），前端170826／global64bf不變，witness仍false。Doppel v14指定control首音lead498.731ms／age24.583ms開場接受，921/3115原生完整但915P1G4B1M品質失敗，0B0M安裝條件維持，候選未安裝；現庫仍witness=false，新Random0/3／task未完成，200/100/500/20不降。[原件與固定身份](DESKTOP_BAD_MISS_FIX_2026-10-31.md)。

## 2026-10-31 Doppel相同Drag5 body的phase−70指定AP（單列部署待完成）

相同profile／CFP／compiledbody的closed5Drag plan，phase−40曾912P3G5B1M，改−70指定control達921P0G0B0M／1000000AP，原生全921/3115、result.png／release通過。支持phase因素，不是qualifiedclock或個別Drag因果。strict per-job phase_ms前端正在實作，僅Doppel−70、global−40及其他388列保持；native不重build，sourcefreeze／tests／版本化bundle部署與freshRandom3待完成。Current仍024/global64bf/config1adc/front170826，APcontrol不計Random，四failed批次保留／streak0。[完整對照](DESKTOP_BAD_MISS_FIX_2026-10-31.md)。

## 2026-10-31 Doppel單列phase−70與strict前端已部署（新Random待驗）

Doppel witness＋closed5Drag29ms＋job phase−70已安裝，global仍−40、其他388整列exact；lib08d／config46da／frontend192104，native024fe08/ca2不變。Per-job strict整數phase有check/run拒錯及104PS SelfTest；安裝／獨立測試receipts分列，不重建native。正式面板已Computer重開、重用none模擬器；新batch024-rowphase70從0/3開始，指定AP不計，四批failed與全部controls保留，不宣稱clock或全庫資格。[部署身份與新批規則](DESKTOP_BAD_MISS_FIX_2026-10-31.md)。

## 2026-10-31 第五批Manifold首音預算失敗（streak0／診斷中）

正式部署後freshRandomR1 Manifold Hypothesis IN14在first_note_observation_budget取消，0觸控／未完整native結算／無PNG；root觀察0F與895Miss只作人類觀察，不造scoreartifact。第5批失敗保留、streak0／task未完成。Current024/front192104/lib08d/config46da不變，Manifold effective phase−40，並非Doppel−70外溢；開場原因診斷中，不先改clock／guard或歸因Flick。[原件與目前範圍](DESKTOP_BAD_MISS_FIX_2026-10-31.md)。
