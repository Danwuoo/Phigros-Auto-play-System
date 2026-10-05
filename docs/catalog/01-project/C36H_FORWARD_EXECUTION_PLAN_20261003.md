# C36h 後續開發執行計畫（2026-10-02→10-03）

## 授權、期限與完成定義

使用者本輪要求總控規劃更後續內容並直接開發，自行判斷停止線；盡量於 **2026-10-03 01:00 Asia/Taipei** 完成，未完成交新 **gpt-6.1-sol／xhigh** chat。已建立一次thread heartbeat「Phigros 凌晨一點開發交接」，automation ID `phigros`，01:00提醒，不輪詢時間。若提醒到達，保存當下進度、停新實驗、釋放觸控並交接，不讓兩chat同時寫檔。不得將尚待驗證的長期研究標完成。

本輪使用者明確授權 Computer／emulator 在需要實機驗證時使用，優先於AGENTS／旧文件的研究輪禁令；不必再要求重複許可。使用已驗profile、裝置與五指mapping預檢、有限單輪入口與安全釋放。正式策略仍只用即時pixels及有界近期state，模型只離線；不讀譜、不按曲名調參、不讀遊戲記憶體、不用原touch作oracle。新授權不等於現在必須跑live或恢復AP長期goal。

接手：main／HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、唯一worktree，既有X1–X5及修補未提交。C36h tint1仍baseline／77 Miss；main50 50／27／11是donor／control、live0。保留所有dirty與frozen evidence，不reset／clean／push。下一階段採新目錄，不重build frozen out/x5-repair。

**本輪工程完成**＝至少完成X6的可重現pixel量測、正負控制、判定與交接；若支持下一機制，沿依賴鏈繼續，不在「工具寫完」即自動停止。較遠路線不是要求一小時內證明跨曲泛化。只有達到明確科學停止線，或期限到交接，才停；未有結果不得借停止線省略驗證。

## 路線與分支

| 工作包 | 具體問題／改動 | 入口與輸入 | 驗收／下一決策 |
|---|---|---|---|
| **X6 Current-pixel中心可靠性**（現在做） | 6173–75的轉向是遮擋改變量測中心，還是真實位移？量core coverage、端點、最大缺口、跨色交疊、中心與span變化。先量測，不填回座標 | 新C++ offline reader，重用`load_diagnostic_png`、X5 fixed report／SHA／correspondence；D6170–6180、6188–6191，E5286–88及A/B/C全物件作control；所有337 role frames均可核，不單挑成功物件 | 合成遮擋／真实平移／旋轉／neighbor／absence可區分；真圖只proposed。兩次deterministic、ASan／容量／錯source拒絕。若無區分能力，交付負結果並转X7的延後承諾，不繼續調遮擋閾值 |
| **X7 Relation承諾與可觀測性** | 確定何時資訊足夠初配；分開appearance與motion。C36h scope只作研究分層，不能借main50 confirmed；檢驗延後承諾的lost opportunity | 固定prefix逐幀comparisons；先以原C36h role gate與unknown分支為對照。相同prefix／不同future反例保留 | 至多一個新bounded規則，預先spec再eval；若只能靠future／packet判role則拒絕。報所有abstain與first-divergence，不以推薦數量判成功 |
| **X8 C36h單機制variant** | 將已支持的X6可靠性訊號或X7規則擇一帶回C36h lineage，不能兩項混改 | 新isolated source export／binary，既有X1 full contact replay＋同input／clock；C36h未改reference、main50只對照 | 原synthetic／real controls過，source hook唯一、trace-on/off一致，完整prefix events／first divergence可追。重Down、unknown retry、completed復活或無支持Move任一新增即拒絕；保留失敗 |
| **X9 Hold獨立分支** | 不讓D案例吃掉全部研究；3498已持續contact，不再錯修no-root。尋找真實body支持與identity／association失支持的第一因果點 | 3493–3504正支持、4979–4990absence、既有miss-review高資訊clips；owner cancellation理由對齊current bank與prior Down | 先建分類：missing pixels／missing extraction／identity／relation／root-only／owner。本輪最多一項被證實子機制，不能靠放寬grace／lease消失cancel。無可重現failure則保留control，不硬改 |
| **X10 Donor與interaction驗證** | main50 global line assignment／短缺線projection等是否值得移植？避免新版整包替換 | 只選與已定位問題相關的一个donor；C36h、C36h+fix、C36h+donor、合併variant形成2×2比較；不用全機制排列 | C36h優點、crossing／neighbor／旋轉Hold／absence／unknown guards保持。交互退化則拆開或拒絕，不以總root數掩蓋 |
| **X11 Runtime成本與候選凍結** | 改動是否破壞latest-frame／owner時序與界限？ | 現有cold-pipeline／fake-clock；獨立正式candidate build，不連模型；版本、source/binary/profile/SHA一併freeze | 全分母n、p50/p95/p99/max、jitter／fail／drop，不能加各階段p99。證據支持的單一候選才有live資格；無性能預算空間則拒絕或簡化 |
| **X12 有限實機驗證**（本輪已授權） | fixed pixels無法回答觸控feedback、遊戲採納與真延遲；測候選是否改善或回退 | 預檢裝置／geometry／mapping；C36h A/A先兩輪同譜估波動，再候選與baseline交錯A/B各最多兩輪，合計至多6輪。若無候選但核心缺口只能靠新錄影回答，可做最多2輪baseline targeted capture，算入6輪 | 收全部完成／中止／score/P/G/B/M/combo、latency和失敗分布；unknown注入／釋放失敗／mapping變更立即停止且保留。一次改善只能稱pilot，不宣稱AP／統計顯著。遊戲選曲由UI操作，歌名不得進策略 |
| **X13 跨曲保留驗收與整合** | 修正是否只適合Dlyrotz？是否值得正式採用？ | 從既有不同歌曲／譜面家族按已知形式選development controls與未調參holdout，區分HD／IN；不足資料明列pilot | 凍結後才解封holdout；退化分布逐曲列。通過才建議正式整合、同步ARCHITECTURE/ROADMAP契約。失敗保留candidate並回退新改動，不刪歷史證據 |

順序並非要求每條都做：X6→X7/X8是主線；X9可在role訊號不足時提供獨立高資訊問題；X10僅在需要donor時啟動。X11是任何live候選的共同gate。X12新baseline資料可作資訊不足的有界分支，但不能以多跑幾輪取代分析。X13跨曲驗收可能需另一輪授權的額外採集，但本輪6輪內可涵蓋已有候選的保留曲。

## 視覺工具、模型與蒸餾

X6先使用可解释C++ color-core probe，並用原圖AI目視提出反例。當candidate ROI偏差使量測不可用，才比較一個離線模型proposal與人工／既有幾何的review成本，最多一個小包，不先train `pas_vision_cpu`。模型不能決定role、lease、contact或補不可見current evidence。所有model/AI proposals與人工gold分欄；本輪沒有人工gold就不報precision／recall。

可採用鏈：pixel cue → 可重算量測 → 預先定義hypothesis → 正負控制／消融 → 單一bounded C++ gate／state transition → full replay → 成本 → frozen live。例：coverage收縮且center沿主軸偏移可輸出measurement unknown；不能直接把舊center當新觀測。遮擋判斷若只是色閾值自我證成，必須以原圖及旋轉／highlight／相鄰物件counterexample否證。

## Gate、停止線與資料預算

- **科學停止**：一次預先聲明的候選family經反例否決，最多一次修工程錯誤後再驗，不另開無限門檻搜尋。尚有獨立可測問題則轉分支，不宣稱研究總目標完成。
- **策略停止**：source-only修正沒有完整行為驗證、沒有當前像素支持、需要future／runtime ID gold／曲名條件，或增加未知Down重試／完成復活，不能進正式候選。
- **live停止**：最多6輪、不無限選最佳；任何注入unknown或無法確認釋放先安全收尾。baseline A/A不穩定則不作因果改善宣稱，先整理時序／環境證據。
- **期限停止**：01:00提醒後不新開長操作；先保存狀態，若有未完成工程，建立6.1 sol xhigh task，提供本文件與未完成精確指令；原chat不監看。若已得到足夠negative result且無有價值的bounded下一步，記理由而不是創建空任務。
- **容量**：先保留旧campaign＋prior≤8GiB；X6新batch≤24MiB（16MiB開發／8MiB驗收），PNG只引用，source/logs/failures計額，新build另列≤1GiB。舊campaign目前約7.918GB，餘約672MB，不能塞新full recording。若需live，另立`measurements/game-assist/forward-live-20261003`≤12GiB總帳、每輪journal≤256MiB、最多2輪full-recording且每輪≤5GiB；其他輪只必要bounded clip/result。開錄前驗disk free≥保留額度＋5GiB，不刪任何舊raw。

## 進度與交接（持續更新）

- P0：本計畫已完成。01:00 timer曾設定，但父chat未收到提醒；01:12確認逾時後停止新增實驗並安全封存，timer已停用，按使用者授權交新6.1 sol／xhigh接續。尚未啟動emulator，沒有正式候選策略。
- X6：已完成[量測、13/13雙build回歸及封存](../04-offline-research/PIXEL_CENTER_RELIABILITY_X6_20261003.md)。中心偏移與色核心span不一致可量化，但異色缺口v1漏掉6174且有同色二物件歧義，拒絕進runtime；不調閾值救結果。
- X7／X8：缺新role cue／可採用規則，暫不開NDA-v2或策略variant。沒有把X6工具完成當作Miss改善。
- X9：已[完成兩個完整replay及封存](../04-offline-research/HOLD_CAUSAL_X9_20261003.md)，batch10,744,999B。發現原contact持续，另個fragment新增Down再取消；全prefix與原凍結行為相同，92個retained control rows byte-identical。
- X10：[單一main50 fallback-claim消融已完成](../04-offline-research/HOLD_RAIL_CLAIM_X10_20261003.md)，batch6,897,075B／out160,904,698B。2478額外Down消失，A/B/D/E normalized target/action保持；但2個typed geometry反例失敗，拒採原donor。全prefix首次action差2447，不只局部2478；尚未定位全state首差。
- X10b：[冷驗證已完成](../04-offline-research/CURRENT_INTERIOR_CLAIM_X10B_20261003.md)，Release/ASan各原207/207＋新13/13；baseline／ON／OFF共3個完整replay，ON/OFF全semantic/events一致。H局部額外Down/Up移除，但全序列2883還有未解差異，**不採用、不live**。新batch≤128MiB、export/build≤512MiB，原3次replay額度已用完。下一問題是2881–2885像素與contact因果，不是調閾值。見[01:00接手頁](C36H_0100_HANDOFF_20261003.md)。
- X10c：接手者完成[X10b獨立工程驗收與四次fullprefix因果稽核](../04-offline-research/HOLD_CASCADE_X10C_RESULT_20261003.md)。全action exact alignment2161 matched、baseline4／variant2 unmatched，未稽核0，差異只在H及K，2887之後亦核對。首observable state差2477；684至2880的local history相同，2881抑制前witness亦相同。standalone suppression把root_past矛盾變成absence，舊owner pending grace留下2883 Down／2884 Up；未修改owner的獨立FakeClock對照重現。**該family否決採用，不調threshold，達科學停止線。**原RGB另有同欄兩分離Hold及current claim歸屬缺口，physical gold0。新batch≤128MiB／out≤768MiB，四次replay已用完，原X10b不改。
- X10d：分為兩個可獨立否決的問題。**X10d-P 總控獨立工程／冷契約驗收通過**（新契約Release/ASan各13/13、原suite只保留預先聲明的一項契約失敗；14組機會損失仍Unknown），詳[總控驗收](../04-offline-research/PENDING_CANCEL_X10D_P_CONTROLLER_ACCEPTANCE_20261003.md)，見[PENDING_CANCEL_X10D_P_RESULT_20261003.md](../04-offline-research/PENDING_CANCEL_X10D_P_RESULT_20261003.md)；X10d-O 尚未啟動。以下保留原研究安排：**先X10d-P pending契約**：main50 `src/game.cpp`已有`pending_down_current_object_missing`的cursor==0分支及對應missing/fresh-return測試，先研究只把這一機制帶回未改C36h的隔離export，X10b suppression維持OFF；不重造現有能力或整包搬owner。必驗pending／active／unknown Down／completed、fresh-return資格、due在新snapshot之前／之後／同時、gate/reset、旋轉Hold當前body仍支持；保留C36h原pending容忍tests為baseline契約，candidate的刻意語義變更另列，不能改期待掩蓋regression。完整H/K/A/B/C/D/E及全首action差異需逐一解釋，少Down不作成功指標。**再X10d-O current body歸屬**：以同column分離Hold、舊tail過線／新front接近、旋轉/neighbor/absence建pixel反例，區分原held body、獨立incoming front與內部fragment；body歸屬不足就unknown，不能以局部藍色連續升格ownership。先補契約與證據，至多一個事先聲明的ownership候選；不以pending cancellation通過替X10b suppression背書。兩項各有獨立停止線；只有各自有證據才另開交互消融，不能同批混改後宣稱因果。所有新replay另立protocol/source/binary/容量帳，原X10b/X10c不追加、不重build。
- **下一優先X11-P**：pending-only候選通過冷契約，可準備C36h baseline／單hook完整runtime的成本驗證及source/binary/profile凍結，尚未啟動。原replay EXE不是live binary、零fake recognition/RPC不是實際成本；明列計時邊界、latest-frame、jitter／失敗與容量。X11通過才進X12已授權最多6輪有限emulator A/A及A/B，判定14組機會損失與閉環採納的實際利弊；X13跨曲仍conditional。X10d-O另案，不是pending-only候選X11的前置，不能同批混改。C36h tint1仍baseline，X10b仍否決，本輪live0。較早各段及01:00頁保留歷史狀態。
- **X11-P交付完成，待總控獨立驗收：not-ready**。見[完整runtime／冷成本結果](../05-runtime/RUNTIME_X11_P_RESULT_20261003.md)及[交接](../05-runtime/RUNTIME_X11_P_HANDOFF_20261003.md)。B0/B1完整source/pas.exe/DLL/provenance已freeze，唯一策略差仍pending hook、suppression OFF；source inverse/public bridge與新B1 13/13契約過，原suite保留唯一已聲明fail。24cost＋4stress額度全部用完；A/A p99 noise不足，dense owner兩批p99差+.46655/+.45685ms越T=.298ms，另baseline一筆lateness15.0906ms越15ms，維持failed不放寬。正式archive/wake、原live未列compiled SHA及真裝置/觸控仍有缺口。X12只預備最多6輪exact binary/profile/停止方案，當前阻擋，不執行；C36h journal512MiB硬限與本計畫256MiB要求另缺可驗guard。新batch≤128MiB、out≤3GiB、8GiB總帳保留，無emulator/ADB/live/模型/commit/push/新goal/chat。下一步由總控獨立簽收negative結果再定有界工程，不順帶開X10d-O/X12。

- **X11-P總控獨立驗收完成：簽收工程與負成本，仍not-ready。** 四suite及兩版512-input bridge實際重跑符合；274/1490/1923 source/dependency/artifact條目重核，noise/evaluate重算bytes一致。沒有新cost/stress/full replay/live。下一優先 **X11-P-R1正式Wake/SessionArchive離線量測契約修補**，先處理release分母、testcase skip/空metric、live lead對應與256/512MiB容量契約，再另立有界成本protocol，AA不穩即停止，不追樣本。詳[總控驗收及可派送工作包](../05-runtime/RUNTIME_X11_P_CONTROLLER_ACCEPTANCE_20261003.md)，尚未派送。X12仍阻擋；原live未列compiled SHA、14lost機會與77Miss改善仍Unknown，不混X10d-O或復活X10b。總控初估16MiB不足已記錄、完整保留於32MiB retention，8GiB總帳未超。

- **X11-P-R1交付完成，待總控獨立驗收；not-ready。** [R1結果](../05-runtime/RUNTIME_X11_P_R1_RESULT_20261003.md)保存shared正式Wake/完整SessionArchive/lead35、逐release分母、skip/空metric/missing row/寫檔/容量負例、新Release及ASan冷驗。B0/B1唯一策略差仍pending8行，source/compiled dependencies/closure/profile及新SHA另freeze。4functional stress＋8A/A後noise四項fail，baseline owner coverage114/128亦fail；ABBA0，不重跑救結果。完整raw/失敗/容量入口R1 final-summary，無live或新goal/chat。
- **X12 pending-only容量計畫修訂（總控R1驗收後，最新）：** native32×16MiB為512MiB logical journal限，不是Windows實體byte硬限。CRLF反例32×256 logical正常完成而實體8224B>8192B已確認。按文字轉換最多2倍保守保留journal每attempt 1GiB physical、standby8MiB、metadata2MiB、result PNG4MiB，最多6attempt共6,530,531,328B；future live根仍12GiB、另5GiB free reserve。這是磁碟保留推導，不是新增runtime cutoff；取代R1初估3,277,848,576B，歷史freeze不回寫。本預案無full recording/pixel clips，不建立live根，成本not-ready仍阻擋X12。詳R1總控驗收。


- **X11-P-R1總控驗收完成：工程與負結果簽收，維持not-ready。** 八套suite（含ASan）、兩版streaming bridge及release probe重跑符合；noise重算一致，12run raw獨立分母/quantile/archive內容核對通過。512MiB實體硬限說法被CRLF反例否證，上述容量計畫已修；正式runtime未改。下一優先 **X11-P-R2量測有效性與active stress覆蓋契約**：先分析既有raw與樣本量/啟動/負載，再決定有無值得執行的新bounded A/A，不原樣重跑追pass。X12仍blocked，未派送新task。詳[總控驗收](../05-runtime/RUNTIME_X11_P_R1_CONTROLLER_ACCEPTANCE_20261003.md)。

- **X11-P-R2交付完成，待總控獨立驗收；not-ready。** 見[R2結果](../05-runtime/RUNTIME_X11_P_R2_RESULT_20261003.md)、[protocol](../05-runtime/RUNTIME_X11_P_R2_PROTOCOL_20261003.md)、[原raw分析](../05-runtime/RUNTIME_X11_P_R2_RAW_ANALYSIS_20261003.md)及[交接](../05-runtime/RUNTIME_X11_P_R2_HANDOFF_20261003.md)。先用原12run量化order statistics／固定窗口／負載／全skip及archive；原noise fail不重開。≥2000 measurement samples的完整JSON AA8筆容量投影228,019,240B>128MiB，未有已驗bounded壓縮collector，所以新AA0/ABBA0，成本admission拒絕，不縮樣本或放寬gate。
- **R2 active coverage已自驗、待獨立驗收：** 未改R1 core/shared Wake/archive/lead35，固定head0–800ms、body800–1800ms、gate-off1800ms一target stimulus，不按收據延長支持。三筆OS concurrency B0 Release→B1 Release→B1 ASan，各1Down/24Move、same contact、32/33/33 body witnesses及requested1 release/contacts_exit0；所有768attempt/562skip/75receipts/40release calls/708events保存並核raw，ASan無報告。新三套7/7、原八套契約結果與512-input streaming bridge保持；零contact不能pass的新負例已驗。這不授予正常成本或128-target active聯合覆蓋資格。
- **R2終點與下一決策：** 新工具source/dependency/profile／未改runtime SHA另freeze，正式src/include/root CMake、舊frozen/PNG/dirty保留；完整commands/XML/raw/exact容量在新R2 final-summary/ledger。下一包若仍求成本資格，先決定可保留≥2000樣本＋全warmup/fail/late/full diagnostics的collector容量方案；不得用剩餘第四stress或相同replay追pass。X12仍blocked，最新6,530,531,328B reserve不改，本task不建立live根、不新chat/goal、不傳訊、不啟動emulator/ADB/真觸控/模型、不commit/push、不自簽總控。

- **X11-P-R2總控簽收，仍not-ready。** 新三suite各7/7、兩版bridge及原12/新3run raw重算符合，原三筆並行已補active same-contact/release（含ASan），不作性能/128target聯合覆蓋宣稱。成本AA0/ABBA0因128MiB長窗admission不成立，不是candidate性能退步。下一 **X11-P-R3長窗成本資格**：新獨立measurement根≤2GiB、舊campaign+prior≤8GiB不變（兩帳≤10GiB），先驗長窗collector有限上限及完整JSON保留，再固定方法一次8AA、通過才16ABBA；不先造壓縮平台，不追安靜樣本，不自動無限後續修補。尚未派送、不啟動live。詳[總控驗收及R3規劃](../05-runtime/RUNTIME_X11_P_R2_CONTROLLER_ACCEPTANCE_20261003.md)。原R2 ledger的status/forward SHA已用總控pre-review快照保留，最新追加不回寫歷史freeze。

- **X11-P-R3交付完成，待總控獨立驗收；not-ready。** 見[R3結果](../05-runtime/RUNTIME_X11_P_R3_RESULT_20261003.md)、[protocol](../05-runtime/RUNTIME_X11_P_R3_PROTOCOL_20261003.md)、[交接/獨立驗收](../05-runtime/RUNTIME_X11_P_R3_HANDOFF_20261003.md)。bounded collector新三suite各10/10、兩版512input全byte/row/EOF bridge一致、五筆長窗raw重算14families及完整固定窗口。主measurement n≥2000、run80MiB/全JSON容量已成立；AA5後第一owner full owned2282/2560=89.140625%<原90%，立即停止，AA其餘3/ABBA全部未跑，新stress0。R3預先加measurement90%亦fail，原full門檻已足以獨立否決；方法差異明列且freeze不回改。noise未評估、B1成本差異Unknown，不把負結果說成candidate退步。old core/正式86檔/dirty/oldraw保留、唯一pending8line/suppression OFF、新archive對稱sample限三處16384→65536；51bindings/575deps/另2input/six maps/inverse及全ledger保存。normal raw98,156,255B、out278,012,952B、old8,241,831,623B不變，new2GiB/工程96MiB/controller32MiB/build3GiB/old8GiB/兩帳10GiB/free5GiB遵守。下一只待總控簽收negative，若另定有界工程先用已有raw研究owner完整分母缺口，不原樣追pass、不自動R4/R5/X12。X12仍blocked，14lost機會/77Miss/真閉環/跨曲Unknown，無emulator/ADB/live/model/新goal/chat/automation/commit/push；歷史各段不改。

## X11-P-R3總控決策更新（2026-10-03）

以[R3總控驗收](../05-runtime/RUNTIME_X11_P_R3_CONTROLLER_ACCEPTANCE_20261003.md)及status J30為最新。R3工程與negative evidence簽收，成本仍not-ready。長窗樣本與容量已足夠；第五筆B0 owner full owned2282/2560未過原90%，依先驗停止，AA5/ABBA0/stress0。四RGB僅個別normal通過，完整AA noise未評估；不可把本次稱候選B1性能退步。

**停止目前成本資格的追加測試與候選升格，不自動R4/R5或X12。** X10d-P冷驗證仍保留、C36h tint1仍baseline、suppression OFF；90%完整分母不能放寬，也不減診斷或追樣本。本次獨立重驗809artifact、來源依賴及舊freeze、8/8/10新tests、五raw audits、另分母/主要quantiles、兩bridge，無新成本或live。

若另決定繼續，先對既有owner raw的193decision skips提出可證偽原因，量測到達群聚/已知action服務空窗並明列缺失actor時戳，才判斷最小插樁或新有界實驗是否有價值。未取得識別性不直接再開方法修補輪；本段是決策條件，不是已派送的新工作包。真正capture/RPC、14lost機會利弊、77Miss與跨曲仍Unknown；X12不因本次工程簽收自動取得資格。

## 選項A交付更新（2026-10-03，J31，待總控獨立驗收）

**選項A交付完成，待總控獨立驗收；成本資格仍not-ready。** [result](../05-runtime/RUNTIME_DECISION_SKIP_A_RESULT_20261003.md)／[handoff](../05-runtime/RUNTIME_DECISION_SKIP_A_HANDOFF_20261003.md)。只讀R3五B0 run，193owner decision／85consumer skips全逐列，原AA5/ABBA0/90%fail/noise未評估不變。142份recognition完成在前accept前、51在後；publication界限136前/51後/6未知，exploratory6份整個residency在前selection→accept間。191/193下一recognition≤4ms，但owned亦966/2281群聚；185edges按先驗全匹配，skip enqueue p50不高於control，193因果分配仍Unknown。

新reader辨明RGB event/release raw attempt是decision sequence而非frame attempt；source-anchor-map由完整archive解析，owner兩者相同。沒有修改原R3 raw/ledger/window/gate。新獨立C++20工具Release/ASan各17/17、ASan五run raw驗證exit0、兩份16檔byte-identical、兩ledger/實際source/dependencies/SHA/全denominators/容量及所有失敗保存；data≤48MiB+controller16MiB/build≤1GiB/free5GiB。精確帳本runtime-decision-skip-a/final-summary，status/forward只追加。

建議總控選項B僅做有界量測契約審查：至少補publication/selection觀測才有actor識別性；沒有授予方法實作、成本R4、owner2–4或X12。若不補最小觀測，停止此線、由總控考慮另案X10d-O；本包不執行。C36h baseline/main50 donor、pending-only/suppression OFF與14lost/77Miss/真閉環/跨曲Unknown保持；本chat完成後停止、不開task或傳訊。較早總控決策與凍結歷史保留。


## 2026-10-04 基準／逐曲證據與順序工作包入口（開發自驗交付）

以[IN zero miss總帳](LEGACY_IN_ZERO_MISS_BASELINE_20261004.md)／[JSON](LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json)及status J32接續，前文期限／「下一優先」按各段日期作歷史，不覆寫原byte prefix。本包開發自驗完成後待總控獨立驗收，未代簽。Chapter Legacy全部解鎖IN＋完整IN Miss=0為產品目標，HD用於解鎖／回歸，P/G/B照報、無AP前置；chapter_listing_verified=false、目前分母／各曲解鎖unknown，89份保存結果按版本分開，不拼最佳。

現況：C36h tint1 behavioural/experimental baseline、main50 comparison/donor 50/27/11/live0；X1已獨立驗收；X10b suppression family否決/OFF；X10d-P冷契約通過、14lost取捨Unknown；X10d-O未開始。X11-P/R1/R2/R3工程與負結果已有驗收、not-ready維持。R3全五run為B0，owner2282/2560=89.140625%＜原90%、85consumer＋193decision skips，長窗容量/樣本已成立，AA5/ABBA0/noise未評估，不是B1成本退步。

選項A仍須分層收尾：0419原reader17/17、verify-input原tree一致是reproducibility；002/003新checker未build驗證，configure根exit0但兩秒quiescence守門因owned vctip.exe survivor拒絕，Job cleanup verified，005未執行未採用。193因果Unknown、publication/selection缺失及RGB sequence/source anchor差異保留；不以工具阻塞代稱產品失敗。

只準備、不執行後續包：**包2 A獨立驗收收尾 → 包3 B有界量測契約審查 → 包4 X10d-O產品開發 → 包5 conditional候選live／全曲IN**。精確輸入／可改邊界／交付／總控驗收／停止與依賴見10/4總帳。B不自動授權插樁/R4/新cost；O不依賴193全部歸因，先同column分離Hold、舊tail/新front及rotation/neighbor/absence真圖反例＋合成控制，至多一事先声明通用候選，不混pending-only／復活suppression。未過X12成本gate不自行live，最終採用仍需完整行為／成本／遊戲結果證據。舊X12最多6attempt只是pilot額度，全曲額外輪次須由總控另凍結有界計畫。

本包僅新增≤16MiB維護根及同步README/ARCHITECTURE/ROADMAP/AGENTS入口；原dirty／frozen source/docs/raw/ledger/binary保留、無formal策略diff、無emulator/ADB/觸控/manual-session/model/runtime/cost/stress/full replay/goal/chat/automation/傳訊或Git提交／清理。原recovery LibTorch/CPU runtime目前不存在，cache引用為歷史，未重裝。完成後停止，待總控獨立驗收。
