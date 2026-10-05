# X4：未 confirmed 初配的 proposed-role oracle 離線交接

日期：2026-10-02（Asia/Taipei）。**X4 已通過總控獨立驗收，離線因果實驗與工具結案；驗收證據與下一步見 §7。** 下列 §1–6 保留開發交付時的原文及自驗分母。C36h tint1 為 behavioural／experimental baseline；main50 原控制策略是 mechanism donor／可能 regression，50／27／11、live0。X2 關閉 winner override 的 variant 不作本次 baseline，也沒有與 X4 組合。沒有 production 候選或 gameplay 改善宣告。

## 1. 問題、介入及不可越過的界線

問題：只更改 D6158–6220 所選 Drag core 未確認時的 provisional line winner，能否改變後續水平錯配鏈，讓受當前支持的 vertical relation → fresh root 或原 current-overlap 資格 → plan → Down 成立？若不成立，第一個剩餘原 guard 在哪一層？

[experiment-spec-before.md](../../../measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4/experiment-spec-before.md) 在實作前保存 hypothesis、eligibility、packet、controls、成功／反證標準、兩 run 政策及 24MiB 預算。oracle 是 **proposed-role 研究介入，human／expert gold=0**；Down 或恢復鏈不能反證角色是真的，更不能推出 gameplay 命中率。

插入點位於 main50 原 score loop 之後、原 confirmed-preserve block 之前。新 export 只新增 `x4::choose(..., selected, best_score, second_score, ...)`；hook 的唯一策略 mutation 為 `selected=oracle`。原 `selected=&*established; preserve_confirmed=true;` 保持無條件啟用；best／second 原分數不改。identity ambiguity、relation ambiguity、conflict／continuation、history／confirmation、motion／root、owner／lease、scheduler／clock 都沿原程式；不清歷史、不強設 confirmed、不修 alias、不放寬 grace，不重試 Down。

eligibility：

- packet 只列 6158–6220，綁 ordinal、source_frame、原 PNG SHA 及當幀 core／line 的中心、尺寸／長度、切向。沒有 runtime note／line ID。
- 只匹配當幀唯一 D Drag core（沿原 X1 selector `250<y<325`）；必須 `confirmed_line_id==0`。
- proposal 是同幀原 RGB／已驗收 main50 control 的 current bank 幾何中的唯一垂直候選。重現時還要 current `observed_ns==capture_ns`、association_valid、原 length／extent 資格；幾何 correspondence 必須唯一。
- 缺 core／候選、無效、過時、非唯一或不同幾何為 unknown/no intervention；SHA／source-frame 錯配直接拒絕。不能製造線或把缺失視為像素不存在。
- 6174 的重疊身分仍交原 identity guard；packet 不提供跨幀 identity/history。介入後 ID／history／actions 自然演化，沒有餵回 control state。

packet 由 C++ `x4_tool packet` 只讀當幀 current candidates／line geometry 建立，不讀 recorded touch、未來判定、root、後續 owner/state 作 selection 輸入。原 6160、6174、6175、6214 PNG 已目視，黃色 core／垂直線與重疊可見；AI 目視及其 role／continuity 都只是 proposed。

## 2. 工作區、工具與來源

開始：main、HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、唯一 registered worktree。原 X1–X3 dirty 工作保留，沒有 reset／clean、commit／push、新 worktree、goal、自動化或下一 task。正式 `src/`、`include/`、根 CMake 不改。沒有 emulator、真觸控、manual-session、模型訓練或 live。

新 [x4_oracle.hpp](../../../apps/frame_review/x4_oracle.hpp)、[x4_input.hpp](../../../apps/frame_review/x4_input.hpp)、[x4_report.cpp](../../../apps/frame_review/x4_report.cpp)、[x4_oracle_tests.cpp](../../../tests/x4_oracle_tests.cpp)，沿既有 contact reader、SessionPerception、round reset、GameObserver、owner、between-frame scheduler 與 ReplayTouch；沒有新 replay 平台。原 contact／X2／X3 入口的契約保留。`X4_OFFLINE` 只在 offline CMake 加 targets，拒絕和 `X2_OFFLINE` 同時開；root runtime 不讀 packet、不含 hook。hook 預設 off。

新 export `out/x4/main50-provisional` 從已保護 `out/x1/main50-v2` 複製，共同檔案只有 `src/game_tracking.cpp` 改變，patch 見 [provisional-only.patch](../../../measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4/source/provisional-only.patch)。原 X2 control 的策略等價經 J7/X1 control equivalence 驗收，再以本次介入前全 prefix digest/actions 核對；沒有以同版本號代替 binary/source SHA。

| 綁定 | SHA256 |
|---|---|
| 原 X2 input manifest | `e1704c0c6ab0cc6bb9def4aad31a8e49416556947ba319d9487dbbd451577cbc` |
| X2 總控驗收 | `5436b5f6d981df40554dcce9bbbcb41e5aa15b7a2650ffb5a075883de8349dc8` |
| X3 總控驗收 | `75ae973b9003432493c0096b5c2d281669a27953b2a4e708f5e5b054d4c88fd6` |
| 新 X4 input manifest | `7c266f7b1163e5bba920fe901d48b48b3248b9b9e8c17647e165a93406261266` |
| X4 oracle packet | `f8e303c06b03648c874ec565e14c6fd0e4ec26d531d5400a41aa9b0e034d8f8f` |
| X4 replay binary | `da0cadc4f74ff1cd7d6de74266b78d9d7f840e7f651f9ee23e03cf1d5c43b318` |
| X4 tracking export | `e45547aea7932c9304e3a30d3da46e29d3d11217317038c96cbe8d1fa152cb86` |
| X4 source provenance | `a0da781cae0d9f59db1a02bbbb5fe5119c62fbe9efea1970fde6a0df6ec94f66` |

[input-manifest.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4/input-manifest.json) 將 C36h reference、main50 control 舊 run 的根目錄、summary／trace／events／state digests／first-context 實際 SHA、舊 source/binary binding 分別列出。舊 run **仍由原 X2 manifest 產生**；新 experiment manifest 僅橋接同 input/policy，不假稱重產。入口重算 parent／兩份 acceptance、角色／variant、source／binary／各 file SHA，拒絕錯角色、provenance、input/policy 或既有 output。新 query manifest 再綁兩次 X4 output file SHA；原 X1 R1/R2 與 X2 三角色 validation 沒有放寬。

原圖仍是 C36g full-recording，非 C36h 新 live：index `0bd8ec74b6bc19d1f8080c064453a052095e328e31d2759f12d6d04d78500f8d`、profile `d2b5cfdc6be023c715fe402a7124ccd1977f5956b8d9fa74466643a575dbd0e0`。完整 earliest available prefix → EOF、recorded owner consumed set＋最多 32 preroll、frame-first、fake origin 1s、capture/pixels_ready QPC 平移、zero recognition/RPC、五 contact success receipt 都沿 X1。original recorded actions 只在獨立 stream 展示，從未回饋動作。

## 3. 有界實驗結果

**Verified：在固定 C36g pixels 上，僅更改未 confirmed 的初配 winner，足以讓 D 恢復垂直 relation → fresh root → 原 owner plan → fake Down。** 成功限此反事實鏈；role proposal 仍未被證成可部署 rule。

完整結果：[causal-report.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4/causal-report.json)，235,965B，SHA256 `cab91f05748691f81849eda8b5861986ae3e2afb8323321e1511509302299e47`；query manifest SHA `69aec7dc4894dc3788904481960b465e24b6e7ab2484fc1cbd3cf40dc9a8ab3a`。各 run 的 summary／events／trace／oracle-attempts／first-intervention／G context／逐幀 digests 均由 query 實際 SHA 綁定；report 含全部109幀窗分母、D63幀及完整 receipt stream 的第一 action 差。

兩次新 variant run，ON 後 OFF，皆完整 earliest prefix→EOF：**7722 PNG 驗證、7715 perception calls／7 skip、5983 events、EOF contacts=0、success=true**，沒有第三次或失敗 full run。兩輪 canonical digest 同為 `18b3fe4d19a71097dbc83826eec0acdc3e2aa2abd1e1f05ebaedb591874a3832`；state-digests、events、oracle-attempts **byte-identical**，各 SHA 分別 `532b7747c725b038acd2727e7920856f444383a0cd398b6d153ca1ed353e969f`／`a0a5275a0ae33157353439424b6254853cd1f094504eb68e062db47c743c7248`／`a13772a8ec93641b5d243e56b501232e301869638a632506627c4d230b350da6`。trace 只增加診斷輸出。

packet63幀中59幀有唯一 current vertical proposal、4幀 unknown（6158／6159／6220缺唯一core；6210缺current vertical）。實際 hook rows：**5次 applied／different winner、55次 already-confirmed不介入、3次無唯一core不介入**。6210雖proposal未知，原confirmed資格已先禁止改winner；沒有把它升為有效proposal。

| ordinal | 原控制／X4差異及保留的guard |
|---|---|
| 6160 | 首個全prefix state 差，同 note1643、相同current core／bank；control水平316、hit≈(20.995,273)，X4垂直315、hit≈(938,287)。這有實際幾何差，並非僅ID hash差。原winner score26.596113／second289.121193保持；confirmed0、samples1、rootnull，首先仍是insufficient_history。 |
| 6160–6164 | 只有這5幀未confirmed，原best316→proposal315。6164沿原history門檻自然confirmed315／samples3，之後hook不再改winner。早期root未立即成立，outside_short_horizon等原限制保留。 |
| 6170–6172 | 原identity ambiguity仍導致line0／samples0／rootnull，沒有alias或history oracle；局部note1643未在這次反事實中換號。6173–6174自然重建history，6174現在ambiguity=false／samples2；這是早期選線引起的後續自然差，不能強制重播control的ambiguous flag或把物理continuity升格gold。 |
| 6199 | 首次exported vertical root，`106669173576ns`，samples5；距當幀約307.736876ms，owner尚無plan。geometry root出現不等於排程資格。 |
| 6210 | vertical315從當前bank缺失，best水平317；原conflict拒絕，old-visible=false、abs tangent dot0、prior-hit gap143.007465px，line0。bank缺失不代表RGB無線。 |
| 6211 | 當前新vertical318；dot1／gap6.5px通過same-local-line幾何，但原conflict仍拒。由原source及相鄰狀態可重建：首份replacement／normal distance約115.499px>40，尚未達兩份且≥12ms條件；replacement count未直接export，這項為Strong inference。 |
| 6212–6213 | confirmed315→318、history自然清除後samples1→2／rootnull，未借用舊root。6212續接成立符合兩次current幾何、間隔28.5819ms；physical same-line仍proposed。 |
| 6214 | vertical318／samples3／history33.2308ms，fresh root `106725530098ns`、uncertainty2.128659ms、reason prediction_observe_only、projection=false；原owner接受intent519。basis=`live_pixels_short_linear_fit_drag`，Down在fake QPC `106689291400ns`、(988.5,287.997100621)、contact0、success。不是current-overlap fallback。 |
| 6215–6219／6222 | 同contact0五次Move修訂當前支持；6222因current_object_missing_or_region_lost撤銷並成功Up，沒有再次Down或復活completed。D共7份receipt＝1Down＋5Move＋1Up。 |

這些note／line／intent號是**各run局部ID**。本次原main50 control為1643→1644、X4保留1643；不能把X2 disabled-override variant的其他號或C36h1622直接當同一physical identity。6174跨frame／跨角色continuity仍proposed。

**C36h reference** 在同RGB的6214已有vertical360／root `106745505268ns`，其Down在`106695505268ns`（delivery ordinal6215）、同(988.5,287.997100621)。X4 Down比它早 **6.213868ms**；D之後五Move及6222Up的phase／time／position相同，ID與全局action stream並非因此等價。原main50 control的此D cohort沒有Down，6214仍line305／relative_velocity_small／rootnull。這是恢復所選垂直動作鏈，沒有證明更接近遊戲判定。

完整receipt/release stream去除note／intent ID後，首差仍為ordered index1636：X4新增D6214 Down；control下一命令是另一物件6216 Down，時間與位置均不同。7722逐幀raw state digests中6925相同、797不同，首差6160；**797不是物理物件或錯誤數**，後續local ID／owner/contact分配亦會自然受新動作影響。介入前6160之前的全prefix state與actions完全一致。

**Controls**：A12/12、B12/12、C9/9、E13/13，合計46幀，scene／lifecycle／owner／contacts、scheduler及prefix summary逐項相同，保留B absence release与E正常control。G1533–1537的scene／lifecycle／owner／contacts **5/5**相同，沒有X2全prefix1535介入；G scheduler不在這個5/5指標內。D窗4/63原樣相同，59幀不同。原crossing／旋轉Hold／late alignment等另外由原gold回歸覆蓋，不把少數窗相同擴稱全局正確。

**安全分母**：全run成功Down505，local-note重複Down0、unknownDown0、EOFcontact0；D的1643只一次Down。unknown receipt分母0，因此這兩次成功receipt replay沒有驗未知注入情境；新direct-owner測試另有一次真正fake-unknown Down及no retry。physical duplicate、unsupported Move／premature Up的全局語義及遊戲接受均unknown。

## 4. 測試、失敗與判讀範圍

Release 新 export 的原 tracking／owner／session／core／contact 套件加 15 個 hook tests：最終 **249/249，fail0／skip0**。新測試直接呼叫 `track_legacy_batch` 與原 `GamePlanOwner`，涵蓋 default-off、scope、confirmed、候選 current/basic/unique、SHA/geometry、identity/relation ambiguity、late alignment、線追 Note、同方向 neighbor／新 fragment ID、真正 unknown Down、completed 不復活、absence 不造支持。原 crossing gold 及按住期間旋轉 Hold、head/body/tail、missing/lease/fake-clock 回歸保留；不修改 X2 243/244 的歷史 crossing 失敗。

已保存工程失敗：首次編譯 C7692（MSVC string/JSON 比較）修正型別；最初 log drain 逐行掃 campaign 過慢，native 已失敗後中止 wrapper，partial log 與明示 completeness 保留。之後把 console 保持在最多1MiB記憶體、child 結束後取得同 mutex 寫入，沒有改 replay clock。首次 Release **248/249**：新 completed test 沒有滿足原 overlap 的 samples≥2／span≥10ms；補兩份原 tracking 量測，另讓 unknown test 明確 assert 一次 unknown Down，避免空分母假通過。沒有改策略 guard 或原 gold。

Debug／ASan顺序確認build exit0，實跑 **249/249、fail0／skip0**；另用既有frozen C36h reference及main50 control Release各實跑 **213/213、244/244、fail0／skip0**，27張opt-in RGB具體讀取，不冒稱重建舊baseline／control ASan。XML／log／command及binary SHA保存在batch；兩角色共有測試不作獨立場景相加。X2 variant原crossing退化未重跑、未改gold。

ASan首次build wrapper在replay持有shared mutex時拒寫log，native exit未知、console不可取回；失敗record明列限制。随后顺序build確認與ASan成功獨立記錄。一次ASan test launcher誤指不存在的舊資料路徑，native test未啟動；修正為保護的C36h pixel-clips，保存launcher失敗。

**CLI**：最後20個實際預期負例全部符合reason／非零exit，無新有效output，existing report SHA不變；含parent/input/index/window/source/binary/tracking/oracle/file SHA、缺binding、role/swap/trace、float schema與原X1 R1/R2。首次harness18/20保留：R1誤給valid manifest，產生的report雖檔名含invalid，實為合法正例，與protected X1正例byte-identical，SHA仍`37d469caa0017be00fb78736fd69fb5cc87585c40b9fd671bb7e8397c420e7f7`；R2正確拒絕lineage但期待錯誤reason。更正兩個harness呼叫而非放寬契約，original R1wrong-input SHA／R2swapped lineage拒絕都重現。原始失敗、前版script、actual args與report保留，見[cli-negative-final.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4/cli-negative-final.json)。

第二輪後只加強**唯讀report query的role／trace label驗證**，沒有重建或覆寫replay凍結工具。原report tool `b8b141eb…`保留，新`out/x4/query-final/x4_tool.exe` SHA `086ef4928f8eff89eda57ed02892d63d0e5c2b687c6873ea0feb539344a8b5de`；新source／snapshot與ASan binaries另列final-tool-freeze。原source freeze及失敗不覆寫。

Windows／Intel Core Ultra5 125H／MSVC v145 Release，input1280×720。host cost為perception＋accept/poll/drain/digest/event writer，**排除PNG decode/hash、不推fake clock**；兩run各n7715、fail0，ON p50/p95/p99/max＝4.5761/10.52314/14.130918/35.5628ms、jitter(p95−p5)8.0946ms；OFF＝3.2991/7.22584/9.859206/21.8979ms、jitter4.92402ms。ON与ASan build有並行負載，這不是受控performance A/B、capture→touch latency或速度改善。

## 5. 容量、保護與重現

新 batch 固定 [preconfirmation-role-x4](../../../measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4)，24MiB＝25,165,824B；campaign＋prior 固定8GiB。開始 campaign 7,828,340,426B＋prior45,307,809B＝7,873,648,235B，剩716,286,357B。原 X1 130,356,292B／X2 52,804,100B／X3 13,451,038B 全保護。source／packet／trace／events／logs／失敗／帳本與自身長度全計額，PNG／舊 trace 只引用。out/x4 export/build/tools 是編譯產品，另列磁碟帳；沒有轉移實驗 evidence 規避額度。

每份 evidence 寫入核上限，使用 `Local\PAS_X1ContactReplayBudget`。replay 保留外部 logs/summary 2MiB，單 run stream 上限 ON 12MiB／OFF 6MiB，硬拒超額且停止釋放；不 silent truncate、不删失败／扩额。首次 run 前保留兩份輸出 headroom。精確最終bytes／remaining、所有檔案及ledger自身長度見[capacity-ledger.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4/capacity-ledger.json)；[preservation-check.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4/preservation-check.json)核舊檔、frozen export/binary/snapshot、formal source與J1–J9原文。

最終新batch **16,571,837／25,165,824B**，剩 **8,593,987B**；campaign＋prior **7,890,220,072／8,589,934,592B**，剩 **699,714,520B**。out/x4 export/build/tools另計 **572,866,067B**。保護核對 **1888次、0 mismatch**，HEAD／branch／worktree不變、formal diff空、J1–J9原文相同，X1／X2／X3精確bytes不變；檢查數為檔案次數，並非獨立場景。第二份自驗report亦byte-identical，沒有增加full replay或宣稱總控獨立驗收。

重現使用新 output，先核剩餘額度；本次最多兩次 full variant 的配额已经由保存的 attempts 計數，獨立驗收優先只重算 report／負例／測試：

```powershell
$x4='measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4'
./out/x4/query-final/x4_tool.exe compare "$x4/query-manifest.json" `
  "$x4/variant-on-1" "$x4/variant-off-1" "$x4/independent-report-new.json"
$env:PAS_RGB_CLIP_ROOT="$pwd/measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
./out/x4/tools/x1_tests.exe --gtest_brief=1
```

保存的 configure/build/run CLI／args／exit/log SHA 在本 batch `commands/`；source、export、binary/DLL 的实际SHA在 `tool-freeze.json`。不可向舊 X1/X2/X3 寫驗收資料或覆寫它們的 artifacts。新 replay 是固定 C36g pixels 的策略反事實；觸控改變後的遊戲 feedback 不在此輸入中。

## 6. 結論分級及停止

**Verified**：winner-only介入範圍、5次變更、原confirmed／ambiguity／conflict守門、兩rundeterminism、D fresh vertical root→plan→Down及controls／tests／CLI由source與實體evidence支持。**Strong inference**：6211缺第二份replacement的具體子因由原source與逐幀狀態重建，沒有新增內部counter實測。**Hypothesis**：線局部幾何／相對運動可形成泛化的early-role資格，尚未有能取代這個proposed oracle的runtime rule；closing、同法向或overlap單獨仍不足。**Unknown**：physical identity／judgment-role gold、遊戲採納、原perception race／早期state、source render age、真recognition/RPC、77 Miss逐Note原因及跨曲效果。

唯一下一步建議：**總控先獨立驗收X4**；驗收後再以6160–6164／6170–6174、6210–6214與crossing／late-alignment／旋轉／同向neighbor反例，研究能替代proposed packet的early-role資格與abstain邊界，再決定下一個單機制冷實驗。不得直接部署逐幀oracle或先刪preserve／conflict。此成功支持早期選線對固定pixels鏈的因果足夠性，未凍結production candidate。本chat停止於交付，不啟動第二機制、下一chat、emulator／真觸控／manual-session／live／訓練／goal／自動化；未commit／push。

## 7. 總控獨立驗收（2026-10-02）：通過

**Verified：沒有阻擋驗收項；X4 離線因果實驗與工具結案。** 不代表 production candidate、遊戲採納或 77 Miss 改善。完整驗收紀錄與 X5 提案見 [status J11](../01-project/PROJECT_STATUS_NEXT_STEPS_20261001.md#j11-x4總控獨立驗收通過離線因果實驗與工具結案2026-10-02)。§1–6 及原開發失敗／自驗保留為歷史；以下另計獨立驗收。

驗收包：[acceptance-summary.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/preconfirmation-role-x4/acceptance-20261002/acceptance-summary.json)，SHA256 `7f685429c59de8cf4bd0ab6b65f5f14da1c0bb8ea245344fc8d8fef67cd01fb8`。

- 凍結 X4 Release 與 Debug／ASan 各 **249/249、fail0／skip0／disabled0**，原27張 opt-in RGB 實跑；沒有重新 build 或重跑舊 C36h／main50 套件。
- 一份新 packet、兩份新 report 與交付 byte-identical。packet SHA 仍 `f8e303c06b03648c874ec565e14c6fd0e4ec26d531d5400a41aa9b0e034d8f8f`；report 各235,965B，SHA仍 `cab91f05748691f81849eda8b5861986ae3e2afb8323321e1511509302299e47`。new full replay=0，沒有超出兩次實驗額度。
- **20/20 CLI 負例**重跑皆符合預期 reason／非零 exit，無新有效輸出，existing report 不變；原 X1 R1/R2 保留，沒有 mutex 假覆蓋。
- 前後核對先前保護1646項、final source31／snapshot31／binary14、initial snapshot28／export208、frozen artifact204，0 mismatch。export 共同檔案唯一差異是 tracking hook；formal `src/`、`include/`、根 CMake 無 diff。舊 X1/X2/X3 bytes／SHA 及 status J1–J10 原文保持。

逐行 source 比對確認唯一策略改動是未 confirmed provisional winner；原 preserve／ambiguity／conflict／history／owner／clock 不變。**ON/OFF 指診斷 trace 開關，兩輪 oracle 都開啟**；真正策略對照為原 main50 control，C36h 為 reference。獨立核 D63/63 raw trace 與 attempts、原 events 的7份 receipt，五次介入範圍為6160–6164。6214 fresh root→原 plan→fake Down 及後續五 Move／Up相符，Down 比 C36h早6.213868ms；這是固定 pixels 鏈的因果足夠性，不是判定角色 gold 或更佳命中時間。

本次另目視6160／6174／6210／6214原PNG並核SHA。**6210的原圖仍可見垂直白線，current bank缺線不能當pixel absence**；提取子因未證，不額外修 detector。6211續接仍等待第二份≥12ms支持的子因維持 Strong inference，replacement counter未直接export。其餘 A/B/C/E46幀與G5幀的等價範圍維持§3；它們多在介入前，不能擴稱全部 downstream 語義安全。physical identity／role gold、未知注入的live分母、全局Move/Up語義、真時序、跨曲與Miss原因仍Unknown。沒有增加human gold。

容量：本次新增 **990,558B**，X4合計 **17,562,395／25,165,824B**、剩 **7,603,429B**；campaign＋prior **7,891,210,630／8,589,934,592B**、剩 **698,723,962B**。包含驗收summary自身；new build/export=0，既有out/x4編譯產品572,866,067B另計。原ledger及失敗attempts未覆寫。

下一步是 **X5：early-role資格與abstain反例研究**：重用X3/C36h幾何規則，提出最多一個不依賴packet／ordinal／ID／歌名／特定方向／未來動作的bounded離線shadow rule，在crossing、late alignment、旋轉、線追Note及neighbor等反例驗證，資訊不足即abstain。先決定是否有可泛化訊號，再決定單機制replay與回移C36h；目前未派送、未凍結正式策略。沒有emulator／live／真觸控／manual-session／訓練／goal／自動化、commit或push。