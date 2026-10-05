# Chapter Legacy IN zero miss：基準、逐曲證據與接續入口

2026-10-04，Asia/Taipei。**產品目標是 Chapter Legacy 所有曲目解鎖 IN，並以完整 IN 結算 Miss=0 驗收；HD 用於解鎖與回歸。** P/G/B、分數、完整／中止及版本照樣報告，不增加 AP 或 HD AP 前置。本包只整理既有證據及文件；未修改策略、建置 runtime、追加 replay／cost／stress／遊戲輪次或訓練，也未 commit／push。

最新入口以本頁及[機讀總帳](LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json)為準。[10/1 狀態](PROJECT_STATUS_NEXT_STEPS_20261001.md) A–I、研究頂部及早期「最新」字樣是歷史；X1 已於 J5 結案，J5–J31 記錄後續進展，新追加 J32 銜接本包。[10/3 計畫](C36H_FORWARD_EXECUTION_PLAN_20261003.md)保留歷史期限、授權及負結果，最新追加段接續下列工作包。本包完成後停止，等待總控獨立驗收；本頁不替總控簽收本包。

## 基準與產品驗收界線

- main HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，唯一 registered worktree。main50 metadata 為 observer50／planner27／diagnostics11、live0，是 comparison／donor；名稱 main 不使它成為 behavioural baseline 或 live-ready。
- **C36h tint1 是 behavioural／experimental baseline，仍未解決主要 Miss。** Frozen [pas.exe](../measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/candidate36h-tint1-runtime/pas.exe) SHA256 `61ecfacd2cc48ed270506719a1089be646d915c64e9026706ac1654e0d4bbce9`；dirty source 基底 `e23b3f9edfc2df5c6e1ce1aa4c8168af8aeea726`，完整來源映射見[原 freeze](../measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/candidate36h-tint1-freeze.json)。Profile SHA `4201679acc8ded39a394852d5f8d7f11fd62e736c32f10aa77657fc8fdbef18b`。Dlyrotz IN13：795950、P496/G11/B0/M77。
- main50 [pas.exe](../out/main-integration-v145/Release/pas.exe) SHA256 `1440cba9b176f23f9c95513e56b3e8826df1f638b8b17c055955ee170a88addb`。本包重 hash 兩個 binary；同號版本不作相同 binary 證據。
- 歷史七輪的 manifest 數字欄為 36/18，strategy 文字及報告稱 main37/19，binary 為 `d9fa50034d1935b87ee4a1ab1f73ec762fa63b0e0f3835ffe4778f3e0fd8e780`。JSON 同時保留這些欄位；C36h 37/19 與它分開，未修寫歷史 manifest。Dirty build 不能只靠 commit 重建為原 binary。
- X10d-P 是已驗冷契約的 pending-only 候選；suppression **OFF**。成本資格 not-ready，候選尚未產品採用。沒有已验部署策略可由研究工具結果直接升格。

正式策略仍自有 C++20、單程序多執行緒、即時 pixels／有界近期狀態；不讀譜、遊戲內部、曲名／進度、音訊或舊按鍵。模型只離線。RPC／FakeTouch 成功、少 Down、取消數、root 或 ID 穩定都不等於遊戲 Miss 改善。

## 逐曲 inventory

**`chapter_listing_verified=false`，現行完整章節分母=null。** 已知 20 個曲名來自歷史十九首及使用者另選 Glaciaxion，章節身分只具有歷史選曲／provisional roster 支持，當前成員清單尚未核。不能稱完整現行章節已有 20 首。

本包選入 **89 份有完整保存結算的 run＝70 HD＋19 IN，33 個 session**：舊 HD20（含 Credits 完整重跑）、A36 的 HD19＋IN2、後續七輪、四首 lead35／lead40、main49 兩首、恢復16輪、M0十二輪及 C36g／rec1／C36h 各一輪。這是已選資料的 inventory，不是所有歷史嘗試的無遺漏普查。原始數字來源、每輪 session／round、binary SHA、build commit／dirty／compiled source SHA、完整 profile／SHA、result／summary／manifest 路徑與 SHA 均在 JSON，版本及輪次分開保存。

下表僅展示兩個歷史十九首 HD 批次及所選 run 數。舊 Credits 採最後完整 B14 作十九首比較；A2 仍留在 JSON，不取最佳值。分數欄依次為 `score；P/G/B/M`，不拼接不同版本作一套通過結果。Glaciaxion 不混入歷史十九首。

| 曲名 | 舊33/13 HD（Credits B14） | A36 36/18 HD | 保存HD／IN run數 | IN曾可玩證據 |
|---|---|---|---:|---|
| Eradication Catastrophe | 828500；180/0/0/20 | 818000；178/0/0/22 | 7／0 | unknown |
| Credits | 576972；220/5/0/130 | 582690；224/3/0/128 | 5／0 | unknown |
| Dlyrotz | 903024；446/1/0/11 | 961572；452/0/0/6 | 9／11 | 有保存IN結算 |
| Engine x Start!! (melody mix) | 892474；181/3/2/6 | 919792；187/0/0/5 | 2／0 | unknown |
| 光 | 911381；309/1/0/5 | 943762；312/1/0/2 | 3／8 | 有保存IN結算 |
| Winter ↑ cube ↓ | 848129；412/6/0/31 | 865601；419/3/0/27 | 2／0 | unknown |
| 混乱-Confusion | 816297；417/14/0/47 | 743692；376/1/0/101 | 3／0 | unknown |
| Cipher | 823573；403/30/0/40 | 879884；445/1/0/27 | 2／0 | unknown |
| FULL AUTO SHOOTER | 765681；318/10/0/61 | 704846；295/1/0/93 | 3／0 | unknown |
| HumaN | 883761；202/7/1/12 | 903153；212/0/0/10 | 2／0 | unknown |
| [PRAW] | 866130；541/5/0/29 | 895722；548/4/0/23 | 2／0 | unknown |
| Cereris | 822104；624/7/0/70 | 855164；656/2/0/43 | 2／0 | unknown |
| Pixel Rebelz | 784574；425/6/0/74 | 751257；412/1/0/92 | 3／0 | unknown |
| Non-Melodic Ragez (MUG Edit) | 877874；631/9/5/30 | 887993；642/7/3/23 | 2／0 | unknown |
| Sultan Rage | 763069；271/9/0/54 | 817320；292/1/0/41 | 2／0 | unknown |
| Class Memories | 872847；726/9/0/36 | 897218；743/3/0/25 | 2／0 | unknown |
| -SURREALISM- | 762496；466/1/0/104 | 764396；465/2/0/104 | 3／0 | unknown |
| Bonus Time | 882864；390/4/0/18 | 909078；392/4/0/16 | 2／0 | unknown |
| ENERGY SYNERGY MATRIX | 857750；536/7/0/37 | 865043；545/5/0/30 | 2／0 | unknown |
| Glaciaxion | 另批，不納十九首 | 另批，不納十九首 | 12／0 | unknown |

所有 20 曲的**目前 IN 解鎖狀態均為 unknown，zero-miss 產品驗收均未通過**。Dlyrotz 有11份、光有8份保存 IN 結算，證明那些版本／時段曾可玩 IN；它們不證明目前只解鎖兩首。其餘曲目沒有選入 IN 結算，狀態仍 unknown，不記為未解鎖。HD 分數再高也不自動代推解鎖。本包選入89份均有 Miss，完整 IN Miss=0 證據為0；現行分母未核，因此不報章節完成百分比。

原[roster](../measurements/game-assist/2026-09-29-chapter-legacy-full/roster.json)及[HD接續](LEGACY_HD_VS_CONTINUATION_20260930.md)明列清單未核。本表不使用外部曲庫補歌名、難度或解鎖資訊。ENERGY SYNERGY MATRIX 的歷史 HD 等級末位未辨仍為 null；Cipher 未辨花體後綴不補猜。

## 研究狀態：六種判定分開

「独立驗收」欄引用既有總控／接手驗收；本包只核對其文件與引用，不重跑那些實驗。「否決」指該規則或候選採用，不等於否決整份研究證據。

| 項目 | 已實作 | 開發自驗 | 獨立驗收 | 否決／限制 | Gameplay | 下一決策 |
|---|---|---|---|---|---|---|
| X1 fixed-pixels contact replay | 完整工具與 R1/R2 已完成 | 有 | J5 已通過 | 固定 pixels 無新觸控 feedback | 未新增 | 使用既有入口，不再列待實作 |
| X2 winner-only preserve | 離線消融 | 有，crossing fail 保留 | J7 接受研究 | 不足恢復 D，非部署候選 | Unknown | 保留因果與負例 |
| X3 role／切向診斷 | 唯讀工具 | 有 | J9 接受 | closing 不足證 role | Unknown | 不直接改 association |
| X4 early proposed-role oracle | 離線單機制 | 有 | J11 接受 | packet 非 gold／可部署資格 | Unknown | 因果恢復不當 hit |
| X5 NDA-v1／scope修補 | shadow rule、後續 scope/time 工具 | 有 | J13 接受負研究；J14 是總控直接自驗，未另獨立簽收 | NDA-v1 否決，停止閾值搜尋 | Unknown | 不自動開 NDA-v2 |
| X6 pixel-center reliability | 離線量測 | 總控直接自驗 | 無另獨立 agent 簽收 | 異色缺口 v1 否決，非 runtime cue 保證 | Unknown | 保留工具與不可識別反例 |
| X9 Hold causal window | 既有工具兩次 full-prefix | 總控完成／核對 | J16／後續鏈引用，未冒稱另有獨立 X9 簽收 | 原 contact 保持；另有 fragment Down | Unknown | 為 O 保留真圖反例 |
| X10／X10b／X10c | donor、suppression及 composition 研究 | 有 | J19–20 接受工程／negative | standalone suppression family 否決 | Unknown | suppression OFF，不調 threshold 救結果 |
| X10d-P pending-missing hook | 單一取消 hook／隔離候選 | 新13/13；原suite206 pass/1事先聲明語義fail | J22／[驗收頁](PENDING_CANCEL_X10D_P_CONTROLLER_ACCEPTANCE_20261003.md)通過冷契約 | 14組少Down機會利弊 Unknown | 未測候選閉環 | 保留冷契約；成本資格仍不成立 |
| X11-P／R1／R2 | runtime／量測契約及 active coverage | 有 | [X11](RUNTIME_X11_P_CONTROLLER_ACCEPTANCE_20261003.md)、[R1](RUNTIME_X11_P_R1_CONTROLLER_ACCEPTANCE_20261003.md)、[R2](RUNTIME_X11_P_R2_CONTROLLER_ACCEPTANCE_20261003.md)簽收工程與負結果 | not-ready；R2 admission 非B1退步 | 未新增 | 不重跑追 pass |
| X11-P-R3 | 長窗容量與完整 collector | 有 | [R3](RUNTIME_X11_P_R3_CONTROLLER_ACCEPTANCE_20261003.md)簽收 | 五normal全B0；RGB4＋owner1，AA5/ABBA0；noise未評估 | 未新增 | 選項A驗收後只審查最小B契約 |
| 選項A reader／bound診斷 | 獨立 C++20 reader 已完成 | Release／ASan17/17及報表重現 | 10/4原reader self-test17/17、verify-input原tree相同；新checker尚未建置驗證 | reader reproducibility ≠ checker獨立核對；193因果仍Unknown | 未新增 | 第二包收斂独立驗收 |
| X10d-O body ownership | **尚未開始** | 無 | 無 | 不混 pending-only、不復活 suppression | 未測 | 第四包有界產品開發 |
| 候選live／全曲 IN | conditional | 無 | 無 | X12成本gate仍未過 | 未執行 | 第五包依gate及總控決策 |

R3 決定性分母是 **2560 published／2475 consumed／2282 owned＝85 consumer skips＋193 decision skips；89.140625%＜原90%**。長窗樣本／容量已成立；不追加 owner2–4、不改門檻，B1成本差 Unknown。選項A的136／51／6 publication界限、群聚及 enqueue 是可核對量測／界限，193組 JSON／lock／wake／OS 因果分配仍 Unknown；RGB 的 decision sequence 與 source-frame anchor 差異必須保留。

10/4 receipts 的精確入口：

- [原reader重現](../measurements/runtime-decision-skip-a/controller-review-dot-20261004T0419/execution-summary.json)：兩命令exit0、17/17，verify-input run tree相同，只支持原reader的重現。
- [independent-002](../measurements/runtime-decision-skip-a/controller-review-dot-independent-002-20261004T0519/execution-receipt.json)：configure根exit0，command guard判 `descendant_not_quiescent_after_root_exit`，清理verified；未完成新checker build／raw核對。
- [identity-003](../measurements/runtime-decision-skip-a/controller-review-dot-identity-003-20261004T0618/configure-identity-receipt.json)：未改兩秒quiescence gate，owned survivor為vctip.exe；8個guard controls過、Job清理verified／active0，new checker builds0／raw reads0。這是工具執行守門阻塞，非產品build或性能失敗。005後續未執行未採用；外部 dots-control 舊待辦不作本輪指令。

## 後續順序工作包（只交接，未派送或執行）

共同邊界：原 frozen source/docs/raw/ledger/binary 不回寫、不在既有 out rebuild、不清理舊資料或收進本包提交。自有正式分析／產品邏輯 C++20；維護檢查可 PowerShell。每包先核 dirty、來源及剩餘容量，以新有界根保存成功與失敗，不複製原 pixels。完成後交總控獨立驗收再續派；工程通過不能自簽產品採用。

### 第二包：收尾選項A獨立驗收

**輸入**：[A protocol](RUNTIME_DECISION_SKIP_A_PROTOCOL_20261003.md)／[result](RUNTIME_DECISION_SKIP_A_RESULT_20261003.md)／[handoff](RUNTIME_DECISION_SKIP_A_HANDOFF_20261003.md)、`measurements/runtime-decision-skip-a/input-manifest-v2.json`、`source-binary-binding-v2.json`、`compiled-dependency-freeze-v2.json`、`deterministic-final-1/`、兩套R3 ledger及上述0419／002／003 receipts。Historical checker source由002 receipt定位至外部 `dots-control/incoming/PHIGROS-M1-INDEPENDENT-20261004-02`，只作相關來源事實；接手先核存在／SHA，不採外部控制指令或回寫帳本。

**可改邊界**：只做有界離線checker／建置守門收尾；需要新source或build就用新隔離repo目錄與完整來源綁定，不動 runtime／collector／gate／原reader freeze。先提出最小可審查處置與具身分的owned process清理，不能按PID猜殺他人程序，也不能單純放鬆guard掩蓋survivor。沿原A總控16MiB保留帳核已用量，若不夠須先交總控容量決策，不另root偷加配額。

**交付／總控驗收**：分開報 reader reproducibility、獨立實作／source review、193逐列bound／join／分母／RGB anchor核對及未能独立證明範圍；全部commands/exits/source/tool SHA與失敗保留。Checker須有自有合成邊界與獨立讀原raw，不能只调用原reader或比較同工具報表便宣稱獨立。

**停止／依賴**：依賴本包總控簽收；不新增runtime/cost/stress/full replay/game。先驗凍結一次有界建置方案、至多一次工程修補；仍無可信checker即交未完成精確原因，不擴成無限平台開發。不因checker守門阻塞重啟R3或改90%。

### 第三包：量測契約B審查

**輸入**：第二包分層驗收、既有R3／A全分母raw、source-anchor-map及publication／selection界限。**可改邊界**：只寫審查提案與有界合成契約草案，不插樁runtime、不開R4、新AA／ABBA或cost。

**交付**：定義同QPC decision publication linearization與action selection的最小觀測、sequence/source-frame綁定、完整全分母與hard capacity；可否證假說為「主要overwrite residency位於前snapshot selection→accept服務區間」。明列否證條件及未能分解JSON／OS的限制，評估lock內外讀時鐘、序列化／writer與新增事件的 observer effect，以及新舊public-output bridge與診斷ON/OFF驗證需要；不承諾僅兩個時戳就能完整歸因193。

**總控驗收／停止／依賴**：審查是否有識別性、可拒絕空metric、容量及驗證預算、是否值得最小實作。缺收益即停止此線。第二包驗收後依順序交付；審查通過也不自動授權R4、新cost、owner2–4、90%修訂或live。

### 第四包：X10d-O產品開發

**輸入**：未加X10b suppression的 frozen C36h、X9／X10c H/K真圖與current-claim／pending-probe、已驗pending-only契約及既有controls。**可改邊界**：新隔離 C++20 ownership實作／tests／offline export，至多一個事先聲明的通用候選；先保留失敗再改，不混移植pending-only或復活suppression。正式採用另需總控決策。

**交付**：同column分離Hold、舊tail過線／新front接近、rotation／neighbor／absence真圖反例＋有獨立控制的合成／fake-clock契約。分開原held body、獨立incoming front、fragment、pending／active／unknown Down／completed及head/body/tail。當前歸屬不足輸出unknown；local blue連續不當physical ownership。旋轉期間同contact依當前支持Move，遠處未對齊不能硬拒；不靠grace／alias續命或舊root。必要完整行為比較另先凍結新protocol／來源／binary／容量與全action分母，反例失敗保留。

**總控驗收／停止／依賴**：真圖grounding與proposed/gold分層、先紅後綠、跨形式負例、完整行為差異／first divergence、unknown／completed守門與機會損失；沒有可用candidate也可交科學負結果。一次family被否決即停門檻搜尋。順序接在第三包，但**研究不依賴193skips全部歸因**；量測線停止不阻塞獨立ownership冷研究。遊戲收益／成本不能由冷結果代推。

### 第五包：候選live及全曲解鎖／IN驗收（conditional）

**輸入**：總控接受的通用候選、完整行為證據、有效成本gate、exact source/binary/profile/DLL與preflight指紋、最新容量預留及逐曲總帳。**可改邊界**：凍結候選有限live計畫與新結果／選曲清單；只在先決條件成立、總控依既有授權範圍續派時開emulator／觸控。現有X12 gate未過，當前不執行。

**交付**：先核当前Chapter Legacy可見清單與逐曲IN解鎖；HD解鎖／回歸及IN正式結果分層、完整／中止全列。逐attempt記score/P/G/B/M、same-stream result及環境／source／binary/profile SHA；同一凍結通用策略的完整IN Miss=0才記该曲驗收，不拼不同版本最佳。已有X12最多6attempt是pilot上限，不自動擴張為全章節批次；若全曲需要更多輪，由總控另凍結有界輪次與容量，不在本包偷開無限實戰。

**總控驗收／停止／依賴**：成本有效且完整行為／安全契約過、baseline A/A與候選比較完整報告、清單分母可核、逐曲IN结果可追溯。未知注入／釋放、指紋／source變化或容量超限立即安全收尾，退化保留。X12保守6attempt磁碟預留 `6,530,531,328B`（journal每attempt 1GiB physical，native512MiB是logical，另standby/metadata/result）；future根12GiB另free5GiB，並非已建立live根。成本／遊戲證據缺失就維持conditional，不以工具完成叫產品完成。

## 本包自驗、保存與Unknown

[前置清單](../measurements/controller-baseline-20261004/prestate.json)記實際接手2個tracked修改及220個untracked。`apps/frame_review/main.cpp`與其餘原untracked不改；原untracked forward及tracked dirty status保存為preimage，只有末尾追加。README／ARCHITECTURE／ROADMAP／AGENTS只同步入口；正式src/include/root CMake對HEAD仍無diff。

本包親自核對89輪result／summary／session與round manifest可達及可解析、311項既存期望SHA（含兩個現行binary），另核C36h profile與freeze來源映射，並保存引用的實際hash；數字沿原pixel-review引用，未逐圖重新讀P/G/B/M，也未把它們升格逐Note gold。原events僅核路徑存在及保留summary內hash；未重新hash所有events、DLL closure或7722張PNG。完整自驗、修改清單、保護／容量與第二包入口見[本包handoff](../measurements/controller-baseline-20261004/handoff.md)及`validation-final.json`；這是開發自驗，尚待總控驗收。

歷史868880分Glaciaxion的原 `cpp-observe-17905107612608769` raw已刪，原binary未找到，只能引用[歷史審查](HOT_REGRESSION_REVIEW_20260930.md)，不列保存run或可再算資料。M0使用者指定重設刪除的兩輪不復原；中止／待命／開局前斷線另列JSON，不算完整結果。

**CPU依賴現查**：舊recovery目錄仍存在但不在registered worktree清單；其中 `out/dependencies/libtorch-cpu-2.7.0/libtorch` 與 `out/vision-cpu-v145/Release` 不存在。現有 main CPU CMakeCache的Torch_DIR／TORCH_LIBRARY仍指舊路徑。10/1「依賴仍在使用／可build」按歷史閱讀，本包不宣稱當前CPU可build，也未重裝。正式pas不連LibTorch的契約不變。

仍Unknown：目前章節分母及各曲解鎖、physical identity／body歸屬／judgment role gold、逐Note遊戲採納／77Miss根因、14組機會損失利弊、193skip因果、有效AA noise／B1成本差、真capture/RPC/source age及跨曲改善。新維護資料僅`measurements/controller-baseline-20261004/`，上限16MiB，原pixels只引用、不複製、不清理舊資料。
