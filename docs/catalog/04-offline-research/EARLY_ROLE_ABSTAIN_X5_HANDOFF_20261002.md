# X5：有界 early-role 資格與 abstain 研究交付（2026-10-02）

**總控已通過 X5 固定來源的負面研究結果驗收，詳見 §9；`X5-NDA-v1` 不通過策略採用，不進下一次策略 replay 或 runtime。** 下列 §1–8 保留開發交付原文，C36h confirmation 語義與工具重用限制以 §9 為準。幾何推薦在 main50 control 的 D6162–6164 與 X4 proposed 垂直候選一致，但6160–6161仍 abstain。這是3/5早期幀的研究一致性，沒有新 state／root／Down，也不能宣告 role 正確或 Miss 改善。

C36h tint1仍為主要 behavioural／experimental baseline，仍有77 Miss；main50仍為 control／donor／可能 regression。HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、main／唯一 registered worktree；observer50／planner27／diagnostics11、live0。沒有把本次工具或 X4 oracle variant 升格 baseline。

## 1. 授權範圍、來源與實驗先決

已讀專案 README、架構、路線圖、跨曲研究、最新 status J7–J11、X1修復／X2／X3交接及 [X4交接 §7](PRECONFIRMATION_ROLE_ORACLE_X4_HANDOFF_20261002.md#7-總控獨立驗收2026-10-02通過)，並核對 Note／線形式。開始時原 X1–X4 dirty 工作保留，正式 `src/`、`include/`、根 CMake沒有修改；不從舊文件恢復熱測流程。

只讀已驗收的三個 factual history，沒有新 observer／contact replay。它們來自同一份 C36g Dlyrotz IN13 RGB recording，**不是三首歌、三個獨立樣本或 human role gold**。C36h／main50的橋接來自原 X2 manifest；X4只讀 trace ON history。X4 ON/OFF 都啟用 oracle，差別是診斷 trace；策略對照仍是原 main50 control。X2 關閉 confirmed-winner override 的 variant不列入新規則樣本，原243/244 crossing失敗不改 gold、不重跑。

| 角色 | 原 run／來源 | 本次讀取 |
|---|---|---|
| C36h reference | `confirmed-preserve-x2/c36h-reference-on-1`，frozen C36h-v3 tracking | A/B/C/E/D五窗109幀；G沒有相容 trace，維持Unknown |
| main50 control | `confirmed-preserve-x2/main50-control-on-1`，原preserve開啟 | 同五窗109幀＋原G1533–1537 context5幀 |
| X4 factual history | `preconfirmation-role-x4/variant-on-1`，已驗收 winner-only oracle | 同五窗109幀＋G5幀；只是已產生的 history，不當 oracle 正例標籤 |

每角色五窗為A12、B12、C9、E13、D63幀。reader逐行綁 source_frame、PNG SHA、index與原fake-clock offset；實際核114份 unique PNG、讀7722行index。所有當幀 target及所有當幀 line都納入，沒有只挑D成功cohort。缺的跨曲相容 trace及C36h G未補造。

來源契約見 [input-manifest.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/input-manifest.json)：角色／root／逐檔file SHA與source-provenance、實體binary、tracking source binding分開核對。X4 input SHA `7c266f7b1163e5bba920fe901d48b48b3248b9b9e8c17647e165a93406261266`，acceptance SHA `7f685429c59de8cf4bd0ab6b65f5f14da1c0bb8ea245344fc8d8fef67cd01fb8`；X3 acceptance SHA `75ae973b9003432493c0096b5c2d281669a27953b2a4e708f5e5b054d4c88fd6`。實體run binary及各source SHA完整保留在manifest；不能拿版本号代替binary身份。

## 2. 原機制比較與可重用範圍

| 機制 | 實體source支持的行為 | X5判讀 |
|---|---|---|
| C36h role class | prior revision≥2、10–40ms，travel≥3px／speed≤4000px/s、normal fraction≥.85且接近；有有效line model才扣平移／旋轉，同近期已選線可用成對local distance。normal-approach class先於appearance class，再比較distance＋confidence；非normal時alignment≥.95；另有margin8、單線fallback與同線preserve | normal／tangent分層、有限量測可重用；兩pose、appearance硬門檻、已選線history及fallback不是一般early-role證明。其baseline仍有Miss，不能整段照搬 |
| main50 score／preserve | distance＋extent＋弱appearance cost＋confidence＋最多±6 trend、same recent −120；rails／near-line orientation權重分別40／12，遠處3。confirmed override在當前線有支持且distance未超容差時保留，之後另有ambiguity／conflict／replacement guard | D初配近水平316在6160得分26.5961，垂直315為917.6049；近期折扣會繼續偏向既選線。X4已證winner-only初配介入的固定pixels因果足夠性；這不授權移除preserve／conflict |
| X3 pose secant | 兩份當前pose可量位置／法向／相對distance變化；`motion_valid=false`只使擬合model無效，不抹除pose量測 | 使用note-only與line-induced distance變化分解，保留model validity與secant validity各自的Unknown；不把secant當瞬時速度或root |
| Hold／owner | 接入、body持續支持／Move修訂、tail結束是不同判斷；同contact、unknown Down不重試、completed不復活 | 本規則排除全部Hold head/body/tail，不接owner。原旋轉Hold及真正unknown／completed回歸另實跑；不能把「排除」寫成改善Hold |

D6194水平317的兩pose absolute-distance rate約−1052.735px/s，note對它的normal速度0；垂直315約−526.368px/s。因此closing-only會偏向line-driven closing。D6210缺current垂直bank也不能稱pixel absence：X4獨立驗收已目視原圖仍有垂直像素支持，detector子因維持Unknown。本次没有放寬white detector或提供missing line。

## 3. 一個版本的規則、輸入邊界與停止條件

先保存 [experiment-spec-before.md](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/experiment-spec-before.md) 及原X3/raw trace的探索摘錄，再凍結 [rule-freeze-v1.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/rule-freeze-v1.json)，SHA `2984c75da22d080a4fd31202c5e42648f0e87ecc89880efa62228fe670b4fb74`。最多一個threshold版本；判讀反例後沒有調門檻救結果。

[x5_rule.hpp](../../../apps/frame_review/x5_rule.hpp) 是固定容量pure C++20 geometry function；沒有JSON／檔案、ordinal、歌名、run root、local ID值、proposed角色、未來動作、owner／receipt／scheduler輸入或觸控API。candidate位置按線局部座標算，允許candidate順序置換、平移／合理旋轉／tangent sign翻轉。現時刻只有明確unconfirmed、強current非Hold、無identity ambiguity才可進early scope；缺confirmed export不能從samples／root／reason反推。

history最多6個已處理current frames／90ms，由adapter只沿已輸出的unique prior correspondence連到本reader已存pose。note ID／line ID僅用於同run索引，數值不進score；prior ambiguous、missing、超出窗口不補歷史，physical identity仍Unknown。這不是C36h原10ms bucket history的完整等價移植。

候選需current有效、confidence≥.5、length≥frame width×.32、目前垂足在half-span內。至少3份paired pose且span≥12ms；note整段位移≥3px、整段平均速度≤4000px/s，**沒有宣告每個短pair都≤4000**。每interval須同側持續closing、整段closing≥3px、note motion沿candidate normal占比≥.85。line-induced contribution以`local-distance delta − note-only normal delta`量測，ratio≤.5；pair線角變化≤.10rad。它包含line平移及旋轉效果，不借model補造current支持。appearance保留為量測欄，不作遠處配對必要條件。

overlap／過線不當role證明；distance≤height/2＋2px abstain。有支持但paired history缺失、current stale／invalid、line-driven／旋轉closing的競爭線視為unresolved，否決選擇。eligible候選以最小note-normal fraction排序；兩個以上候選margin須≥.15，tie／near-tie abstain。只有一個eligible時margin為null，不虚報競爭強度。

成功門檻原先是非空early推薦、已聲明abstain边界／不變性成立，且能通過有意義role反例，才考慮下一個隔離replay。**同Input late-turn反例已讓最後一條不成立；因此停止規則搜尋與replay提案。**

## 4. 完整分母、D時序與推薦限制

兩份Release report各 **3,953,654B**，byte-identical，SHA `1b6154aa6ce16bbb901ed63a7692f0fc3a31a9795f4fcff473da75f2db6f3fcf`。見 [shadow-report-1-compact1.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/shadow-report-1-compact1.json) 及 [determinism-compact1.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/determinism-compact1.json)。採共享`line_columns`34欄及`latest_secant_columns`9欄的array表示，保留全列，不silent truncate。original score／gate／winner只在comparison欄；X4 packet只在全部decision完成後加入D annex，不回饋rule。

| factual history | frame | target出現 | target×line出現 | 幾何eligible line | early-scope target | 推薦／abstain | valid／unknown latest secant |
|---|---:|---:|---:|---:|---:|---:|---:|
| C36h | 109 | 394 | 1172 | 210 | 0 | 0／394 | 926／246 |
| main50 control | 114 | 413 | 1208 | 230 | 68 | 12／401 | 952／256 |
| X4 history | 114 | 413 | 1208 | 234 | 67 | 12／401 | 964／244 |

合計1220次target、3588次candidate pair；同一物件／窗口／pixels的重複出現高度相關。**24次推薦不是24個命中、獨立樣本或cross-song coverage。** 每角色完整`decision_reasons`／`candidate_reasons`皆在report：main50 early scope的68次為52 no-eligible／4 unresolved／12推薦；X4的67次為46／9／12。其餘多為confirmed relation排除、Hold排除或identity ambiguity。C36h因沒有explicit confirmed export，333次confirmation unknown、57 Hold、4 identity ambiguity；全abstain是資訊缺口，不是成功保守策略。

| D所選Drag時段 | main50 control shadow | 意義／限制 |
|---|---|---|
| 6160 | 1 pose、no eligible | 不能借6161運動或proposed packet回填第一幀 |
| 6161 | 2 pose、no eligible；4.2067ms pair仍列secant | burst pair不等於穩定角色／motion model；此pair note speed約5941px/s |
| 6162–6164 | 3／4／5 pose，history32.2206／38.2170／62.2244ms；推荐315 | 幾何與X4 proposed垂直一致，control factual winner仍316；首推薦差在6162。覆蓋原五次early intervention中的3/5，比X4開始晚兩幀 |
| 6170–6172 | identity ambiguous，abstain | 不用local ID alias跳過；X4 history亦維持這三幀abstain |
| 6173／6174 | control已confirmed／identity ambiguous，abstain | X4 history這兩幀已confirmed而排除；自然state分歧沒有被抹成同一physical history |
| 6194、6200、6210–6214 | confirmed relation排除 | 本規則不修conflict／replacement、不解除completed、不借舊root；6210 missing bank仍保留 |

每個main50 history的12次推薦為E5286–5288三次、D6162–6164三次、D6177–6179三次、D6180三個target。後者有推薦315而factual winner305的物件，role仍Unknown；不能用D主cohort的一致性替其他推薦背書。A/B/C/G在本rule scope沒有推薦，仍保留全部物件及abstain原因。

## 5. 反例與證據分級

[x5_scenarios.hpp](../../../apps/frame_review/x5_scenarios.hpp) 的typed合成prefix：note在0／20／40ms為(400,400)→(400,420)→(400,440)，有水平y576及垂直x900候選，note appearance仍垂直。world A在prefix後繼續向下並於接近時對齊水平；world B在prefix後轉右並對齊垂直，水平是decoration。兩者給rule的**Input完全相同**，未來／獨立scenario role labels不在Input。v1選水平index0，符合A而違反B的指定role，沒有abstain。

**Verified**是這個可重現的typed geometry rule反例及原檔／hash／分母／測試結果；**沒有生成相同PNG、重播遊戲或驗證這兩個合成world的遊戲合法性**。code／report中的`legal future`指研究允許的late-turn／late-alignment情境假設，不能升格為Phigros實戰真值。這足以否定「v1目前條件可保證unique judgment role」的研究主張，不是證明一切bounded pixel rule或學習都不可能。

27個新tests涵蓋first observation、2-pose、遠normal近tangential、late appearance、轉向、line chasing／moving closing、旋轉、同向neighbor／tie／margin、短fragment、missing／stale／invalid、identity／confirmation unknown、Hold排除、overlap／過線返回、非finite／數量／時間界限、順序／ID重命名／rigid transform／normal sign與bounded adapter。falsifier test刻意assert錯選B的語義失敗可重現；**測試pass不代表rule通過角色反例**。原crossing gold由既有frozen control套件驗；新confirmed-crossing測試只驗scope exclusion，沒有替代原gold。

**Strong inference**：在D已量到的運動幾何下，normal/tangent與line contribution較closing／最近線提供更多可分辨訊號；main50初配及same-recent折扣與錯誤早期history相容。X4 winner-only介入的因果足夠性是先前Verified，本次沒有做新因果介入。**Hypothesis**：還有當前pixel cue能分辨接近decoration與將來late-turn role；目前沒有獨立標註支持，human／expert role gold增加0。**Unknown**：physical note／line identity、D及所有新推薦的判定角色、遊戲採納、原perception race／早期state、source render age、真recognition／RPC時序、77 Miss逐Note原因與跨曲效果。

## 6. 自驗、實際失敗與工具上限

Windows NT10.0.26200、Intel Core Ultra5 125H／18 logical processors、PowerShell7.6.5、CMake4.2.1、VS18 BuildTools v145 C++20；精確compiler版本見 [attempt-notes.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/attempt-notes.json)。沒有做runtime latency benchmark或效能改善宣告。

- 新Release及Debug-ASan各 **27/27，fail0／skip0／disabled0**；新reader实际在ASan下处理337 role frames。其report也是3,953,654B，SHA `f59a6227f126352969b1c8cacfd6435ada917f6dec5fa6ba7c9dcf1ed9a879f0`；只替換analysis binary SHA後與Release全文相同，見 [asan-reader-equivalence.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/asan-reader-equivalence.json)。
- 原frozen C36h **213/213**、原main50 control **244/244**、X3 **13/13**，以及X4真正unknown Down／completed owner兩個guard tests **2/2**，皆實際pass。兩舊control套件使用原27張opt-in RGB fixture；共有case不相加為新場景。未重建舊baseline ASan、未重跑X2失敗variant。
- CLI **20/20預期拒絕**：18個X5案例與原X1 R1 wrong-input／R2 swapped lineage。包含float schema／integer、parent／acceptance／query／rule／index、role／root、file／source／binary／tracking SHA、budget、missing／existing／outside output。皆有實際argv、reason、非零exit；不是mutex busy假覆蓋，無新有效report，existing SHA不變。合法舊X1 comparison新輸出與原SHA `37d469caa0017be00fb78736fd69fb5cc87585c40b9fd671bb7e8397c420e7f7` byte-identical。

收尾發現原reader的16MiB開發writer cap亦限制總控，會使預留8MiB無法用於兩份新report。因此只修改輸出CLI：明確`--acceptance`使用原24MiB batch／8GiB campaign上限，default仍16MiB；`--verify-existing`只重算完整report與既有檔比較，不寫新report。旗標不進pure Input／features／decision。原report／ASan成功的實體binary與當時`x5_report.cpp`已另存 [report-tool-before-acceptance.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/report-tool-before-acceptance.json)，沒有覆寫原報表或改rule。

最後reader Release／ASan重新build，各27/27再過。Release兩次、ASan一次從全部bound原trace重新計算337 role frames，完整JSON與原兩report一致，僅正規化analysis binary SHA；不是只核檔案hash。見 [final-reader-verification.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/final-reader-verification.json)。原20個CLI拒絕亦由最後binary另跑20/20，並增加6/6輸出模式負例：未知flag、acceptance existing／wrong parent／outside、default實際16MiB超額及verify missing，皆不產output。沒有在開發時消耗驗收預留額度實寫acceptance report；24MiB正例writer admission仍由source review，不能稱所有quota邊界都已實测。

保留的失敗包括首次configure的GTest imported scope、首次編譯的GTest同列label、首次26/27的MSVC error-text assertion；reader首次target沒有tangent欄，改只讀matching current candidate的appearance、無匹配仍Unknown；第一份完整object-key report超過6MiB硬拒，改共享欄array且不删行。這些是schema／表示／harness修復，沒有新增rule／threshold版本。

ASan reader先與negative writer撞shared mutex，wrapper無法保存console、native exit Unknown，另存launcher失敗說明且不算pass。順序重跑預設ASan quarantine因512MiB process commit上限而`Failed to mmap`／exit1，完整log保留、沒有report。**同一binary、原512MiB上限**，設`ASAN_OPTIONS=quarantine_size_mb=16:thread_local_quarantine_size_kb=64`後exit0且結果等價；此設定縮短freed-block留存偵測窗口，是明列instrumentation限制，不能聲稱預設ASan reader成功。

reader綁定的JSON≤2MiB、row≤2MiB、trace≤16MiB／109行、G≤2MiB／5行、index≤36000行；每幀line≤16、note／target≤128、diagnostic≤8192；report≤6MiB／337 role frames。純rule／adapter的6-frame／90ms／固定array上限與Windows JobObject process commit512MiB獨立。上限拒絕不silent truncate；已實測的物件／非finite／時間與report超額另有證據，沒有宣稱每一種磁碟／深度極端都已測。

## 7. 保存、容量與重現／独立驗收入口

新source僅 `apps/frame_review/x5_*`、獨立 `x5_offline/CMakeLists.txt`、`tests/x5_rule_tests.cpp`及必要maintenance shell `tools/early-role-x5.ps1`。standalone CMake以subdirectory重用原offline工具及frozen main50-v2核心的既有SHA utilities；report不呼叫observer／owner，不改原shared offline CMake或runtime entry。沒有新export；所有compiled products只在`out/x5`。

source／binary／DLL／shared reader與重用export實際SHA見 [tool-freeze.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/tool-freeze.json)。**產生compact1 report的原reader** Release SHA `1f33ee6ef0b8a6039d106891086ab1500cbe78b7740ab2b2e717588d8b8a1a73`，Debug-ASan SHA `923b9acfc42b5eeab3c1ea396c2a1c8ad96f40c8e4f001ecec5a6bc88de0bc73`，保存於`out/x5/report-tool-compact1`；最後驗收CLI的兩binary身份見final-reader-verification及tool-freeze，不能拿最後binary宣稱原report byte-identical。每次configure／build／test／report actual command、exit及log SHA另存，失敗没有覆盖或删除。

最後Release reader SHA `3680e1ac2d55c689c17847bccdab9d4e1419409176e4df19e49d6a8af02aaece`，Debug-ASan SHA `c2a71ced1111bad4d0acb9d7870f223b616733cc1ae0ba586d313b942353b8fd`；純rule／adapter等其餘分析source與產生原report時一致，完整重算另證輸出CLI修訂未改features／decisions。

新 evidence只在 [early-role-x5](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5)，batch硬限24MiB＝25,165,824B、開發限16MiB＝16,777,216B，為總控驗收保留至少8MiB。campaign＋prior硬限8GiB；起點 **7,891,210,630B**。原 X1 **130,356,292B**／X2 **52,804,100B**／X3 **13,451,038B**／X4 **17,562,395B**保持。PNG／舊trace只引用，source snapshot／reports／failed logs／command／quota ledger及summary自身全部計額，out/x5編譯產品另計。

精確最終用量／remaining及另列out/x5見 [final-summary.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/final-summary.json)；全artifact ledger見 [capacity-ledger.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/capacity-ledger.json)，ledger及summary自身長度計入最終總量。每次CREATE_NEW寫入使用`Local\PAS_X1ContactReplayBudget`核額度；沒有把evidence移到out規避額度。

最終X5 **13,742,992／25,165,824B**，開發16MiB內還有 **3,034,224B**，總控可用remaining **11,422,832B**（高於8MiB預留）。campaign＋prior **7,904,953,622／8,589,934,592B**，剩 **684,980,970B**；`out/x5` compiled products另計 **460,087,722B**。final-summary自身525B已納入，無新增full replay。

[preservation-check.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/preservation-check.json)核開始時保護檔、正式source、舊batch bytes、HEAD／branch／worktree、X1–X4 frozen binary／export／snapshot、X4最新workspace source及新source freeze；歷史共享workspace source以最新X4／接手snapshot為準，不把X2／X3較早freeze拿來覆寫目前工作。status只更新最上方latest及追加J12，A–I／J1–J11原文逐字保留。

接手保護檔 **1872次**、繼承freeze及原report-tool **683次**核對，mismatch0；新9份source／snapshot及binary核對亦0 mismatch，formal diff空、舊四batch bytes不變、campaign除X5外bytes與起點相同。數字是檔案檢查次數，存在重複路徑，不是獨立場景數。

獨立驗收先核freeze／parent／quota，**不重跑full replay、不覆寫本次evidence**。可用最後Release reader的`--acceptance`重算兩份report（兩份約7.91MB，須先預留log／summary空間），及凍結Release／ASan binaries實跑tests／CLI負例。新report兩份應互相byte-identical，與compact1 report比較時只略過analysis binary SHA；若要不新增大檔，可用`--verify-existing`完整重算核對。`-Mode Reports/Negatives/Finalize`含開發固定檔名，已用過，不能直接重跑這些mode覆寫。

```powershell
$x5="$pwd/measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5"
# reader report輸出必須直接位於batch root，使用全新檔名；先核總控剩餘額度。
./out/x5/release/Release/x5_report.exe "$x5/input-manifest.json" "$x5/acceptance-report-new-1.json" --acceptance
./out/x5/release/Release/x5_report.exe "$x5/input-manifest.json" "$x5/shadow-report-1-compact1.json" --verify-existing
./out/x5/release/Release/x5_tests.exe --gtest_brief=1
./out/x5/asan/Debug/x5_tests.exe --gtest_brief=1
# 若重測ASan完整reader，保留上述明列quarantine設定及實際env／exit／log。
```

report只讀指定原history並產生新shadow欄；新summary/log等可寫總控新子目錄，native report filename受batch-root契約限制。總控source review應特別核pure Input隔離、model false不抹secant、三history confirmation缺口、synthetic failure不是gold、CREATE_NEW及fail-closed paths。

## 8. 單一工程決策與停止

**否決NDA-v1進入下一個隔離replay；停止這一版本及門檻搜尋，保留negative result供總控獨立驗收。** 目前geometry能推薦D晚兩幀的垂直候選，也能排除一部分tangential／line-driven／競爭情境，但無法辨別合成prefix中「接近decoration後转向」與「目前normal approach所指角色」。C36h又缺explicit confirmation，現有資料不能把全abstain當跨baseline改善。

未提出移除preserve／conflict、拉長history、補未來oracle或訓練來救此結果。若後續另行研究，先決證據是**可在當前pixels辨別上述角色的cue及獨立標註**，並先覆蓋late turn／neighbor／crossing；不是因形式複雜就預設需要學習。這個資訊缺口未解前沒有可審查的runtime候選。

本chat停止於開發交付。new full contact replay=0、new live=0；沒有emulator、真觸控、manual-session、模型訓練、AP goal、無限實戰、chat訊息／新chat／goal／自動化、commit或push。独立驗收由總控後續另行完成，本文件不宣告通過或結案。

## 9. 總控獨立驗收（2026-10-02）：接受固定來源的負面研究結果

**X5研究交付驗收通過，NDA-v1不通過策略採用。** 維持停止此版本、不進下一次策略replay／runtime的決定。完整紀錄見 [status J13](../01-project/PROJECT_STATUS_NEXT_STEPS_20261001.md#j13-x5總控獨立驗收接受固定來源的負面研究結果2026-10-02)。§1–8為開發交付歷史，下列限定及補正優先。

獨立驗收包：[acceptance-summary.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/early-role-x5/acceptance-20261002/acceptance-summary.json)，SHA256 `a0d2281a19dc07f39797fceae62fc531dc56f24e1754c918a5bef083cb1e93d0`。

- 最後frozen Release／Debug-ASan新tests各27/27，fail0／skip0／disabled0；本輪未build，未重跑舊baseline套件。
- `--acceptance`實寫兩份各3,953,654B report，byte-identical，SHA `8bf5b5a5973bd028a254a8cc6b7179720aaed6468c0d00858e0ea0f8c6d20770`；只正規化analysis binary SHA後與compact1原report全文一致。ASan另以原512MiB cap及已明列縮小quarantine設定重算337幀，與新report相符；new full replay=0。
- 26/26 CLI負例依預期reason拒絕，無新有效output／既有report改寫／mutex假覆蓋。來源、snapshot、binary、原artifact及四個舊batch均核對保持，正式src/include／根CMake無diff。
- 原trace的337幀／1220次target／3588次candidate pair、選擇0／12／12、全部abstain／eligibility／secant分母及D時序另核一致。E5287仍正常control；24推薦不是24個獨立樣本或hit。兩張原PNG目視不增加human role gold。

**C36h語義補正（Verified）：** C36h `GameTrackHistory`本身沒有main50的`confirmed_line_id`／replacement state，只有history與current geometry的preserve predicate。原報表`confirmation_not_exported`是adapter保守unknown，不能只當log漏欄位；C36h全abstain不是規則的有效跨baseline評估。後續需先定義C36h的early-scope映射，不可填confirmed=0或把兩lineage語義混用。

**工具擴充限制（source review）：** `feature_json`以`a.ns`重建previous observed time，沒有保留stale prior的原時間／檢查`old.current`，將來擴大輸入時可能錯標latest secant valid；pure rule的history資格仍另檢查current。本次863次raw bank line全為current，固定來源report未受影響。接入新trace前須修時間保真並加stale-prior adapter負例；本輪沒有改frozen工具。

**合成反例範圍：** 驗到的是相同typed Input／不同指定role的不可區分，不是已證相同RGB或合法遊戲world。它否定NDA-v1的unique-role充分性，不能推成所有幾何／學習方法都不可能。spec／code／report的`identical-pixels`、`legal future`字樣只能按此限定閱讀。normal/tangent／line contribution的量測仍可重用；沒有證據支持立刻deploy或追加閾值搜尋。

本次新增 **8,162,616B**；X5合計 **21,905,608／25,165,824B**、剩 **3,260,216B**；campaign＋prior **7,913,116,238／8,589,934,592B**、剩 **676,818,354B**，summary自身計入。new build/export=0，out/x5原460,087,722B另計。HEAD仍f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c、main／唯一worktree，C36h baseline／77 Miss、main50 live0均不變。

下一個提案先處理lineage scope、stale-prior保真與少量真實late-turn／crossing／neighbor視覺反例；不假定當前pixels一定有足以唯一辨識role的cue，不自動訓練模型。取得可測訊號或延後決策／abstain方案才評估下一個variant。尚未派送新task，無emulator／真觸控／manual-session／live／訓練／goal／自動化、commit或push。

## 10. 總控直接補正完成（2026-10-02）

依使用者授權直接完成，見 [X5_SCOPE_TIME_REPAIR_20261002.md](X5_SCOPE_TIME_REPAIR_20261002.md) 與 status J14。原§1–9、frozen source／binary／report保留；工作目錄adapter是新修補版本，不能拿舊tool-freeze SHA對它宣稱相同。

stale-prior latest secant已拒絕；Release／Debug-ASan各34/34，新reader重算337幀與原已驗report全文相同（僅排除analysis binary SHA）。新C36h scope以pre-selection relation samples<3證明preserve尚不可達，獨立輸出85/394次early comparison；不補main50 confirmation、不送回NDA-v1、不產生新推薦。原negative result與否決NDA-v1維持。

新附錄保存27張原PNG的81份三history packet及35次bounded轉向proposal；另目視8張原圖。6173–75交疊造成center／span偏移是觀測可靠性假說，6188–91姿態變化與6180鄰近cores作對照；沒有新增人工role gold。下一步規格改為小範圍離線中心量測可靠性消融，先驗feature能否區分現象，不做NDA-v2或直接live。來源、測試、determinism、容量及保護清單見新batch `early-role-x5-repair/repair-summary.json`。沒有新contact replay或production改動。
