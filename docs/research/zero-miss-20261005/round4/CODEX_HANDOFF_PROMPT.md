# 給使用者自行移交 Codex task 的自足 prompt（最終留存版）

以下是接手內容；本雲端不建立task，也未執行Windows、裝置或實機。候選source已凍結並完成所列冷測；交付包身份依其DELIVERY.json及patch，沒有live-ready宣稱。

---

接手 Phigros Auto-play System（PAS）的持續開發。先在我指定的Windows checkout確認路徑與可用權限；保留所有既有dirty檔、原source、frozen inputs/oracle、失敗收據及STOP。雲端階段已按「開發到需要實機時停止並交接」完成它的範圍，現在由我自行移交。這份研究交付本身不表示裝置或遊戲已允許自動操作；若我另有明確裝置/Fixture/有限實機授權，按其範圍執行，否則到首個必要裝置操作前停並說明最小需求。

## 目標與不變限制

Goal是：在當前遊戲版本、已核實的完整Chapter Legacy分母N下，每曲IN已解鎖，且同一凍結通用候選逐曲完成完整IN結算Miss=0。記錄P/G/B/M及score；沒有AP、HD AP或已核准的每曲連續3次要求。HD只為實際必要解鎖／回歸，不用歷史HD分數代推IN狀態。現行N、各曲unlock仍unknown，產品未完成。

正式邏輯C++20、單程序多執行緒；決策只用current pixels與有界近期追蹤。不得讀譜、遊戲內部狀態、記憶體、存檔、音訊節拍、歌名/進度/舊按鍵或未來幀決定touch。不把人工ROI/proposed標註當runtime輸入。固定gRPC payload fast/RGB888 top-down/1280×720/rotation1/256KiB、latest-frame、QPC、單owner、五contact指紋與硬到期。unknown Down不重試，completed不復活，diagnostics不回餵策略。模型僅離線，無付費、push/PR/merge或擅改安全/裝置設定。

禁止無人自動連打、auto-start、自動選曲／重試。实機時由我選曲和按Play，每次一輪，故障/負結果先查原因，不挑最佳分數湊全曲成果。

## FROZEN：候選與交付身份

- 起始cloud baseline：a53a9b7021bcc900f498047f5cc017b42e4711b0
- 最終候選source commit：c2dda1db9fe95b46ff103300d43583857130cd8f。交付文件commit由包根DELIVERY.json的delivery_commit及format-patch最後commit識別，不用source版本號代替。
- 核心source/header七份SHA見docs/research/zero-miss-20261005/round4/evidence/final-core.sha256；bvi.cpp=866ad2440d088ec1caf360eb5d4a9c796fdfcbcf2e15aff92374baf2291ea824；relation_policy.cpp=37a23c696654b3935a51d422991f260213094a48d1488dc45e74334b29ac1efc。
- 包中incremental-from-a53a9b7.patch從a53增量；cumulative-from-74e5443.patch從74e5443累積。二選一，不重覆套；先核git HEAD/dirty及patch父commit，再於獨立乾淨branch用git am套用。不是已push。包內SHA256SUMS及DELIVERY.json列身份。
- 最終原356layer/3938assertions為36fail/native1；原54修18、新0。原339與79、新74/167/16、獨立101＋另5controls三配置通過。詳細證據和所有中間失敗在round4/VALIDATION.md。
- 雲端精確重跑命令在VALIDATION.md §5：run_regression.sh、run_new_contracts.sh、獨立run-independent.sh／run-supplemental.sh、run_scheduler_preservation.sh、run_cost_probe.sh均存在；Windows須對這四個v3 TU與相同fixture另做compiler wiring，不能把Linux scripts當Windows驗收。

用git rev-parse HEAD、git status --short及source SHA核對；若不符，先釐清本機差異再build。

## 已跑／未跑

- Round2 donor：原356 layer-cases／3938 assertions，54 fail，six inputs與原oracle已實跑；小證據包26份derived檔hash已核。
- Round3 isolated v2：同3938 assertions仍43 fail，原11修復/0新增；新48cases/339assertions全過；獨立35cases/79assertions仍2個duplicate ROI既存fail。Release/Debug/ASan+UBSan結果一致，LSan明示關閉。原紅字不被新契約洗掉。
- Round4：以FROZEN區塊及round4驗證為準，不把上述v2數字當v3結果。本readiness額外原封GCC語法檢查game/game_tracking/game_motion/game_session四個translation units exit0；core因windows.h缺失exit1，沒有link或執行owner。
- 未跑：v3正式main整合、真圖currentROI/all-lines橋接、真實完整contact-prefix候選重播、exact候選有效Windows成本、Windows候選完整build/三配置、當前preflight/五指、任何新IN實機結算。以前root349項（348pass/1skip）與CPU8項為歷史，不能填成本輪通過。
- C36h tint1是behavioural/experimental baseline，main50 50/27/11是comparison/donor、live0；相同observer/planner號不等於同binary。X10b suppression OFF，X11/R1/R2/R3成本not-ready、X12未過。

## 接手必讀與已有資料

Repo讀AGENTS.md、README.md、docs/goals/LEGACY_IN_ZERO_MISS_GOAL.md、docs/goals/legacy-in-zero-miss-state.json、docs/research/zero-miss-20261005/round4/READINESS_AND_HANDOFF.md及最終round4結果/驗證。再看round3契約/修補/獨立報告、round2/WINDOWS_RECEIPTS_UPDATE.md、catalog架構與全錄/接觸replay文件。這些最新Goal優先於舊文件「此包只整理、不開發」的歷史範圍。讀本機`.agents/skills`中相關指引；不要假定雲端工具/路徑能在本機直接用。

已有小包Phigros-minimal-evidence-26-deidentified-20261005.zip，ZIP SHA256 d0a2eb1bebd35ff90c27493fe4ed914970bb24d22137c45af424574d2563db59；manifest SHA256 ddc738c400ad29be11ffdcf9015b4acd31aed770b6bbcea60b55b08aab0178b3。雲端解包位置是/workspace/shared/phigros-evidence-round2/unpacked，這個路徑不保證存在Windows。先找我本機既有原件或已提供附件，正常材料化並核derived SHA，別要求重製/重傳相同包。包內歷史original SHA與匿名derived bytes不同時依manifest映射，不造tamper結論。

## 執行順序與最小命令／停止規則

### 1. Windows離線工具鏈與build

先記`git rev-parse HEAD`、`git status --short`、`cmake --version`、實際VS/v145/SDK/VCPKG_ROOT、dependency hashes和可用磁碟。依賴是vcpkg baseline 93c50752b23e350ca6b9063a167f0a4cf8a3b3eb、CLI11/json/protobuf/gtest/FFmpeg/zlib及overlay-patched gRPC1.81.1 EXACT。保持third_party/vcpkg-overlay，尤其256KiB patch。不要恢復已刪recovery的LibTorch；PAS_ENABLE_CPU_VISION=OFF。

CMakePresets.json的VS18 BuildTools 18.9.12105.275、ASan Community14.50.35717與C:/pas-bld-9408等是原機路徑，先核實，換機差異另記，不默換版本或覆寫舊build。

在確認不存在的新根，Release最小命令：

cmake --preset windows-release -B out/<fresh-id>-release -DPAS_ENABLE_CPU_VISION=OFF
cmake --build out/<fresh-id>-release --config Release --parallel 4
ctest --test-dir out/<fresh-id>-release -C Release --output-on-failure

Debug/ASan用對應preset、新根和Debug配置。記每命令native exit、stdout/stderr、實際test數、skip/disabled、exe/DLL/source SHA。空CTest不是pass，root pas_tests不含所有X1/P隔離owner tests。對最終BVI source另建隔離Windows harness，跑相同six inputs、contract/independent和wrong-contact正負控制；不能把Linux結果當Windows通過。若需改CMake adapter，改compiler/dependency wiring即可，不為測試重寫核心。

如遇已知wrapper阻塞：舊A9 cmd1/1/1只證明root syntax error，offending step仍unknown；D19/D20在mixed-separator transaction-root先失敗，CreateProcess0。新測試根先修canonical Join-Path及完整INIT欄位，注意FinishDiagnostic写tx根卻readback全域state的後續source問題尚未實修。沿用owned/identity/transaction安全核心；完整file-only交易通過後，再以有界argv0/7/version probe收CreateProcess字串、%cmdcmdline%、entry/vcvars start-end/exit/target-entry和cleanup。依首份trace只修一層，保留原STOP/失敗，不直接run舊封存腳本。

停止：build/測試/身份/程序收尾不明即停在本層；安全限制不放寬，沒有verified binary不進裝置。

### 2. Profile/five-contact核實

先離線核configs/phigros-hd-assist-five-lead35.json及實際capability原件。max_contacts=5不是max_contacts_verified=5；需serial、APK SHA、geometry、rotation、mapping、Android SDK/ABI/model/wm_size/density相符。

取得指定裝置讀取授權後，最小read-only命令：

<verified-pas.exe> game-preflight --config <verified-profile> --capability <verified-capability.json>

它會讀ADB/目前pixels但不建input。若fingerprint或five-contact證據不符，停；只有另有Fixture觸控授權才在新output執行bounded four/five、逐指Move/错開Up、cancel與final active0，按catalog/06-engineering/GAME_ACTION_SEMANTICS_20260927.md實際CLI。Fixture成功不等於Phigros採納。不要啟動遊戲來「順便驗」。

### 3. 真圖currentROI/all-lines＋完整contact-prefix冷整合

先找本機已存的選定原生RGB/PNG、frame/QPC/context/SHA index和必要events/full-recording，不先索整個out或全部7722 PNG。真像素內容要親自檢視。

最小讀取入口是GameObserver::process(Frame)後的candidate_batch()；寫有限shadow adapter連BVI extract/relate，按READINESS文件§3欄位契約。借同frame lease，all measured lines參與遮罩／競爭，association_invalid身分不能因轉型丟掉而變eligible。origin/head/body/current support要保存。Hold note.height可能只是screen-Y差；旋轉時用當前front/tail的normal投影，tail缺失不得造端點；held_body_patch不是已見front，不可轉新Down。BVI尚無Drag/Flick語義。容量超限invalid，禁止silent truncation與手填gold。

先測合法rails/front、斜交/旋轉、transparent/opaque effect、同column兩Hold、舊tail新front、neighbor、absence、duplicate/near-duplicate、all-lines歧義；保留安全和合法機會兩分母。只因合成expected不同，不准調色門檻追綠。

owner最小控制使用同一SessionGameOwner/ContactScheduler及bounded ReplayTouch，由候選自己的完整prefix产生實際fake receipt/absolute cursor/contact，再形成attachment關係。不能把BVI Guard.known_down/attachment_query手填成功當正式接入；不能回餵原版歷史按鍵。保留prefix_offset、unknown/completed、gate/expiry、same-contact Move、五指占用、錯contact和tail/release正反例。

candidate_id是每幀ROI序號，不能當長期note/intent/contact；query canonicalization後attachment也要跟明示mapping走。正式uint64 absolute cursor/prefix_offset不可截斷成研究Guard的int，null cursor不可當0；接手先建最小typed消費seam，不重寫owner核心。

若只有三幀或90ms圖，先限定observer診斷；長Hold需要從round reset或可驗checkpoint到窗口的完整候選接觸prefix。原events只供外部核對，不決策。missing pixels/receipt/prefix就是精確缺口，不seed原版Down補洞。

停止：真圖物件/可見性/attachment無法裁決就記unknown，指出一個最小片段或操作需求，不再靠無限synthetic宣稱通過。

### 4. Exact候選有效成本／安全gate

先freeze候選、adapter、source/linked closure、profile、input、meter與method。現有`pas analyze game-cold-pipeline`不連BVI；舊r3_meter連out/x11-p-r1 core，均不能直接量v3。新入口須驗對稱instrumentation及trace on/off/public行為，測完整currentRGB→adapter→BVI→owner＋archive，保留所有attempt/published/consumed/owner_seen、drop/skip/late/failed/unknown/release與n/p50/p95/p99/max/jitter。不以probe count/平均值或相加stage p99簽成本。

先A/A正常性/噪音資格，再預凍結A/B比較。舊R3 owner2282/2560<90%的not-ready與AA5/ABBA0原樣保留；不能降低90%、刪diagnostics、挑success subset、原樣重跑求過。成本或安全gate未過即不進live。

### 5. 有限實機與最終全N曲驗收

只有前述門檻全成立，且我對指定裝置有實機授權時才進行。先當前遊戲版本、Chapter Legacy全清單、逐曲IN畫面核N/解鎖；可遮帳號。舊20個曲名及兩首歷史IN不能當當前分母/全解鎖。

每次由我選曲/難度及按Play，使用exact已驗exe/profile/capability：

<verified-pas.exe> manual-session --config <profile> --capability <report> --one-round --no-preview --round-watchdog-s <事先約定上限>

需要完整接觸診斷才加--full-recording（自帶one-round），先預留5GiB PNG/index及archive空間。錄所有received frames不等於source沒漏幀；watchdog到期算aborted，不能算完整結算。Escape/Ctrl+C、unknown injection/release、geometry/source/gate/容量/錄影fault即停止並核釋放，不重試未知Down。

先一輪回答明示缺口，保留所有結果；Miss>0或fault後診斷，不自動連打找最好分數。局部修補要凍結新身份並重跑相應cold/成本gate，不把不同版的最低Miss拼成驗收。最終逐曲同candidate記session/round/原result圖SHA、exe/source/profile/capability/game version、完整P/G/B/M/score、completion/fault/release和event hashes。

只有當前全N曲全部IN已解鎖、同候選逐曲完整IN Miss=0且可追溯時才完成Goal。其餘只是進度。向我報已驗、失敗、未跑和下一個最小阻塞，附本機交付檔／commit；不聲稱雲端已做實機，不擅自push/PR/付費。
