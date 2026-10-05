# X2：confirmed-preserve winner-only 離線消融交接

日期：2026-10-02（Asia/Taipei）。開發交付時為自驗完成、待總控驗收；**同日總控獨立驗收通過，X2作為離線因果實驗結案**，詳[status J7](PROJECT_STATUS_NEXT_STEPS_20261001.md#j7-x2總控獨立驗收通過離線因果實驗結案2026-10-02)。本文件§5、§6的三處來源／因果／指標範圍描述已依原trace校正；校正前原文保存在本批acceptance目錄。C36h tint1 仍是 behavioural／experimental baseline，main50 是 control／mechanism donor，50／27／11 live rounds=0。

**結果：只停用 confirmed-preserve 的 winner assignment 確實改變 D 的 relation、history 與接觸；成功 Down 前綴由 0 變 1，但新增的是 6200 的水平線 current-overlap Down。它未恢復 C36h 在 6214–6215 的垂直 relation→root→plan→Down 鏈。第一個剩餘 guard 是 6194 的 `confirmed_line_relation_conflict`；6214 再次出現同一 guard。** 不把新增 Down、root、events 或較早觸控換算成命中率、Miss 改善或 production 修正。原 crossing-line preserve 測試在 variant 失敗，亦反對直接整體移除此機制。

## 1. 交付與邊界

本批 `B2`＝`measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2`。先讀：

1. [input manifest](../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/input-manifest.json)、[與 X1 輸入等價核對](../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/input-equivalence.json)。
2. [三角色原比較](../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/three-role-comparison.json)、[逐幀因果鏈／repeat／X1 等價報告](../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/causal-chain-report.json)。
3. [唯一策略 statement 的 patch](../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/source/winner-only.patch)、`B2/source/` 三份 role provenance、`B2/tool-source/` 工具快照、[tool freeze](../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/tool-freeze.json)。
4. [CLI 結果](../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/cli-checks-final/result.json)、[歷次嘗試帳](../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/attempt-ledger.json)、[原資料保護核對](../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/preservation-check.json)、[容量與逐檔 SHA 帳](../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/capacity-ledger.json)。

HEAD 仍為 `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，main／唯一 registered worktree。既有 X1／R1–R2 修補及使用者未提交工作均保留。修改限 `apps/frame_review`、offline tests／維護 shell、新交接及 status J6 追加；正式 `src/`、`include/`、根 CMake、歷史 exports／frozen binary／X1 evidence 均不變。本輪 new full replay=5、new live round=0。沒有 emulator、真觸控、manual-session、模型訓練、goal、自動化、其他 chat 訊息、commit／push／reset／clean；交付後停止，不自動開始下一實驗。

## 2. 精確 intervention 與新入口

[prepare-confirmed-preserve-x2.ps1](../tools/prepare-confirmed-preserve-x2.ps1) 建立獨立 `out/x2/main50-winner-only`，保留 X1 main50 原 export。只將原 `selected=&*established;` 改為：

```cpp
if(x2::winner_override_enabled) {
    selected=&*established;
    x2_override_applied=true; // 只讀診斷
}
preserve_confirmed=true;     // 原 flag assignment 保留
```

`preserve_confirmed` 的原 eligibility 條件、flag assignment 及 `!preserve_confirmed && second_score-best_score<8` ambiguity bypass 全保留。initial score、candidate／line identity、history reset、projection、root、owner、lease、touch scheduler 與完成／unknown Down 規則沒有第二項介入。後續 history／分數／ID 因唯一介入而演化是實驗結果，不能把 control 的 history 重新餵回 variant。

[x2_ablation.hpp](../apps/frame_review/x2_ablation.hpp) 的 thread-local switch 預設 enabled；explicit `contact-x2` 依綁定 role 切換。只讀 `x2_winner_only` 記錄 eligibility、原 flag 語義、override enabled／applied、pre／post winner、confirmed ID、best／second score、ambiguity、identity ambiguity、conflict、最終有效 relation／line、projection、samples／reason、source frame／evidence time。每幀 selection bank 上限 128；它不提供策略輸入。

[contact_replay.cpp](../apps/frame_review/contact_replay.cpp) 以 `PAS_X2_OFFLINE` 隔離新功能；原 `contact`／`contact-compare` 的政策與 R1／R2 validation 保留。新入口為：

```text
contact-x2 manifest role-provenance new-run trace(on|off) owner frame-first
contact-x2-compare manifest c36h-reference main50-control main50-variant new-report
```

每份 summary 綁定 supplied manifest 的實際 SHA、role、variant、lineage、source provenance SHA、tracking source SHA、實體 replay binary SHA。比較前驗三角色與固定政策，重算實體 provenance／binary SHA，拒絕交換、同角色、錯 variant／source／binary、failed run 及 existing output。兩個 main50 role 刻意共用同一 ablation-capable binary，靠明確 role／variant 與不同 provenance 分別綁定；不是假稱有兩份不同 binary。C36h 使用原 `out/x1/c36h-v3` export，重新建 X2 工具，策略未改。

全 prefix 每幀保存 canonical state digest；五窗原 109 幀 trace 保留，另外自動保存第一個 different-winner 附近 2 幀前＋當幀＋2 幀後，共 5 幀 `first-intervention.jsonl`。context ring 最多兩幀／16MiB，機制資料與所有 streams 受批次額度；trace-off 仍保存 bounded digest／mechanism，無完整 scene trace。receipt／release 有完整有界 journal，不只挑成功排程片段。

## 3. 輸入、來源與重現綁定

三角色使用同一份新 manifest。與 X1 相同的 `session_root`、index SHA、profile／profile SHA、五窗及 assumptions 已逐欄核對；只有 batch root、experiment／64MiB 額度、角色與 source／binary metadata 不同，新 manifest SHA 必然不同。

- 原始 pixels 是 **C36g** `full-recording36g-01/sessions/manual-session-22885039263800`，不能標成 C36h live。index SHA `0bd8ec74b6bc19d1f8080c064453a052095e328e31d2759f12d6d04d78500f8d`；profile SHA `d2b5cfdc6be023c715fe402a7124ccd1977f5956b8d9fa74466643a575dbd0e0`。
- 每次從最早可得 prefix 到 EOF：核對全部 **7722 PNG**，**7715 perception calls／7 skipped-perception frames**，recorded owner set=7684，preroll=32。這些分母沿 X1；缺失的實際 perception race／早期 standby state 未重建。
- fake monotonic origin=1,000,000,000ns，recorded time origin=23,084,035,991,700ns；相對間隔、session gate、owner cadence、frame-before-due tie、recognition=zero fake time、RPC receipt=success／zero duration／five contacts 全相同。source epoch／generation／geometry 固定為 1，source render age unknown。
- 五窗固定：A3498 3493–3504；B4986 4979–4990；C5520 5516–5524；D6214 6158–6220；E5287 5281–5293。E 是使用者正常 control，沒有改列 failure。
- C36h 原 snapshot 的 saved source 與 dirty freeze provenance 沿 X1保留；freeze commit `e23b3f9edfc2df5c6e1ce1aa4c8168af8aeea726`、dirty=true。原 frozen runtime SHA `61ecfacd2cc48ed270506719a1089be646d915c64e9026706ac1654e0d4bbce9` 不等於本輪新 replay 工具，亦不按 observer37／planner19 號碼與歷史 main 互換。main50 donor 源自上述 main HEAD，production frozen runtime SHA仍 `1440cba9b176f23f9c95513e56b3e8826df1f638b8b17c055955ee170a88addb`。

| 識別 | SHA256 |
|---|---|
| X2 manifest | `e1704c0c6ab0cc6bb9def4aad31a8e49416556947ba319d9487dbbd451577cbc` |
| 新 C36h replay binary `out/x2/tools36/pas_frame_review.exe` | `bf78aed01795802ee0d0736101dc423d2fd0744e8144a5263bfde38c2c61f550` |
| 新 main50 control／variant replay binary `out/x2/tools50/pas_frame_review.exe` | `6981f88d91e404f8d5b2fe1c9ced34154af6d07dbcf8da6298bc94e12c02ecdb` |
| C36h source provenance | `fd4b42be2c1a80b051d75ecea2a5f6194a0b305acc5b701b4fce5a63076b707e` |
| main50 control source provenance | `20651cf7e4bf1c3544f5a34bea6625288742872050712e9a996bd3fa94fd442e` |
| main50 variant source provenance | `c5bcc44b3bd574fde7bd18aae624906cf2b211321051f6ff22f0ed90407ee3b3` |
| 新 main50 instrumented tracking source | `6050c8bcb778368f7d0ef1df05a4f1736d71572ec94024d66ee0dce7ed6c1aee` |
| winner-only.patch | `320ef5307fcbda0a1870c0446ece298f52adea3617e6ee26f108ec6a5ea29e6a` |
| 三角色 comparison | `51e91e440cd0b7fcd17c63fc5d55b935e5965753dfd9193f7a03ebde1f85b27d` |

完整工具 source／exports、test／analysis binaries／DLL、XML 與逐檔資料 SHA 在 tool-freeze／capacity-ledger；未把版本號當 binary 身分。原 X1 manifest `99e8a364ea9f8575d67fbe39b1b76cccd6820816bc4d999d1b1bdfcfec2dd955`、J5 acceptance `53338cb1b4f5910399d15366db2d661567e4fc834578f78145dc6eeb6bf7cb83` 與全批保護核對都保留。

## 4. 全 prefix 第一個真正介入

最早 preserve eligibility 是 ordinal **804**，note52、best／confirmed 都是 line2；enabled 有 assignment，但 winner 沒變，不能稱第一個因果 divergence。第一個 pre-winner≠confirmed 且 eligible 的 frame 是 **1535**，source13302，PNG SHA `67b08276825548d810b7fab9d30f82f902566a22475f8ed89e66045067c7703f`。全 7722 state digests 亦在這幀首次分歧，1533–1537 的 context 保存於兩個 main50 trace-on run。

此時 note184，best line3 score48.229426，confirmed line2 score178.459826：control override 3→2，原 preserve flag=true，最終 line2／samples5／`outside_short_horizon`；variant 保留 winner3，**原 flag 仍 true、ambiguity=false**，但原 conflict guard 拒絕，最終 line0／samples0／`confirmed_line_relation_conflict`。第一差異可歸因 winner assignment，不能歸因整個 preserve block 或 ambiguity gate 被關閉。

完整 ordered receipts／releases 的第一 action divergence：variant 在 **1547**，fake27,573,008,167ns／source13313 Down(408,468)；control 在 **1552**，fake27,650,719,811ns／source13318 Down(429.024997,590.403207)。相差77.711644ms，皆 local note184。判定忽略 note／intent ID，保留時間、位置、phase、receipt 與 contacts；這不是只因 ID 重新編號。1546／1551 沒有另存完整 scene，本輪不加跑擴窗；journal／plan 與首次機制 context 有界保存，物理正確性 unknown。

全 prefix control preserve eligible=15043、different winner=774；variant=14481／406。後續 eligibility 受 state 演化影響，不能把兩份 count 當相同 state 上的配對試驗，更不能當命中率。原始 events 分別為 C36h6115、control5964、variant6083，只作 audit。

## 5. D：initial relation、剩餘 guard 與實際 Down

案例 correspondence 按同幀 pixels、note 幾何／kind、line 幾何、候選追蹤與同 run cohort 推定；local IDs 只用於 run 內 join。D cohort：C36h1622，control1643→1644，variant1649→1650；**不是人工 physical identity gold**。相同數字不表示同一物體，variant 更不是 control1644 的直接延續。

| ordinal | main50 control | winner-only variant | 可支持的判斷 |
|---|---|---|---|
| 6160 | 無 confirmed，best水平316，score26.596113；second是水平305約289.121193，垂直315約917.604857；samples1 | 同 pixels／score／水平316，eligible=false；note ID 已因較早 prefix 分歧偏移 | initial selection 沒有被此次介入修正；C36h 本幀選垂直356 |
| 6194 | 水平305已 confirmed；best水平317 score157.6，305約171.985435；eligible=true，override317→305，samples5，root_past／root null | 同 best，flag=true／ambiguity=false；保持317後原 conflict guard 拒絕，line0／samples0／root null | 所選D目標的窗內state指標首差；**第一剩餘 guard=relation_conflict**，當幀舊305仍可見，不是 ambiguity |
| 6199 | 續305／samples5 | 原90ms history expiry：reset site229，prior_samples5；confirmed清空後選317／samples1 | 原 state machine 的自然結果，沒有人工清 history |
| 6200 | 續305，未 plan／Down | valid水平317，samples2；note≈(711.002476,287.000001)，line y295，signed distance≈−8px，**尚無 fitted root** | 原 owner 的 current drag overlap 路徑可直接 plan／Down，不能把 root 當所有 Down 的必要條件 |
| 6201–6205 | 無 cohort Down | 6201 samples3，有 root106,397,436,972ns；原 moving-horizontal 支持更新 Move，6205 Up | 先 Down 後出 root；觸控鏈與 C36h 垂直鏈不同 |
| 6214 | best垂直318 score53.699259，水平305約171.401630；eligible=true，override318→305，samples5，relative_velocity_small／root null | confirmed已自然變317；best垂直318 score50.099419、second約291.404116；**eligible=false／flag=false**。317自6212起已不在當前lines；保持318但不符合舊水平317的同線幾何續接，原conflict拒絕，line0／samples0／root null | 與6194「舊線仍可見」是不同conflict子路徑；未恢復垂直 relation／root／新 plan／Down |

**總控校正（Verified：原trace＋實際source）**：variant最後一次有效317量測在6211，至6214相隔61.8127ms，尚在90ms近期界限；其切向近水平，新318為垂直，絕對切向dot約0.0000582，未達local-continuation的0.97。6214的當前線集合只有318、305，不能寫成317仍可見。原6194 conflict則有當前305；同一reason字串不能合併成同一causal boundary。逐項原始摘錄與校正前原文見[驗收證據](../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/acceptance-20261002/causal-evidence-and-corrections.json)。

variant 的原 owner intent520 以 `live_pixels_current_drag_overlap` 建 plan，6200 fake **106,396,050,100ns**／source17967 成功 Down(711.002475874,295)，`predicted_down_ns=null`。後續 Move：6201(725.986473,276)、6202(740.988942,258.010929)、6203(754.102291,242)、6204(768.152623,227)；6205 fake106,506,085,100ns／source17971 Up(768.152623,227)。共6條命令，原同一 contact0，沒有第二次 Down。

C36h 6214 valid垂直360／root106,745,505,268ns，原 plan 於6215 fake**106,695,505,268ns**／source17981 Down(988.5,287.997101)，亦有6條命令。variant 的水平 Down 比該 C36h Down 早299.455168ms；時間差與位置／relation差保存，不能稱「恢復 C36h chain」。這些是 fixed-pixels counterfactual，原 C36g touch對pixels的影響不會因新FakeTouch改寫。

6214 variant owner 保有 `submitted=true`／intent520、`cursor=null`（6205 已完成）；原 owner 對 submitted且無cursor不再建新計畫的規則亦保留。**關聯層先被 conflict 擋住**；即使未來有新 relation，不能跳過 completed 不復活原則。沒有為得到垂直 Down放寬 guard、再消融或重送 Down。

## 6. A／B／C／E controls 與安全分母

下表是 **main50 control vs variant**；C36h vs main50原差异仍在三角色報告，不能說三者相同。表中窗內state指標只比較所選target的幾何／body flags、配線pose、reason／samples／root容差及lifecycle；按幾何配對而非跨run ID數值。**沒有首差不代表owner／contact／scheduler全部相等**，這些另由trace與action檢查。全prefix原始semantic digest及X1 control核心trace等價是另外兩個檢查，不能與此窗內指標混用。

| case | 窗內首 state 分歧 | 成功 Down prefix control／variant | 窗內命令 control／variant | 實際 control 結果 |
|---|---|---|---|---|
| A3498 | 無 | 1／1（3488，同時間／位置） | 9／9 | body／rails、held support保留；3498無root續持同contact；所選cohort全prefix13條命令相同 |
| B4986 | 無 | 1／1（4963） | 1／1（4986 Up） | absence release仍在；**action數值有差異，不能稱全control完全不變** |
| C5520 | 無 | 1／1 | 6／6 | Flick merge／root與所選prefix6條命令相同，localID1592／1594 |
| D6214 | 6194 | 0／1 | 0／6 | 新增6200水平overlap鏈，垂直鏈未恢復 |
| E5287 | 無 | 1／1 | 1／1 | 正常control；5293 Down時間／位置相同，localID1574／1576；沒有重標failure |

B 的前置 Down4963：control85,599,365,239ns／x757.359728545，variant85,599,904,786ns／x757.356089882，皆y576；variant晚**0.539547ms**、dx=−0.003638663px。4986 Up皆85,988,537,200ns／y576，x752.064838923 vs752.041281034，dx=−0.023557888px。cohort prefix各4條命令；窗內是1條Up，current Hold absence後沒有Move／新Down／retry。物理tail終點仍unknown，不靠absence倒推Miss。

三個 primary roles全prefix：同local note重複successfulDown=0、unknownDown=0、unknownDownretry=0、EOF contacts=0；FakeTouch拒絕duplicate active contact。**unknown receipt政策的本次實驗分母是0**，unknownDown安全由保留的fake-clock負例支持，不能把success-only replay說成有測到unknown receipts。local-note計数也不能證明全局physical重複Down為0。

A/C/E所選cohort動作及當前支持無新增差異；B保留absence release與上列微小動作差；D水平Move的current line支持見逐幀trace，但目標物理role／正確觸控線未有人工作gold。因此對 unsupported Move、premature Up、lost contact、invalid resurrection 的**全局物理正確性為unknown**；本輪沒有觀察到local重Down／completed重播，不把此說成gameplay全面安全驗收。rotation-during-held、late alignment、head/body/tail分離、body-without-root、completed return、unknownDown等原fake-clock／synthetic回歸保留。

## 7. 自验、反例與所有嘗試

Windows x64／Windows10.0.26200／Intel Core Ultra5 125H／RAM34,037,383,168B／1280×720 RGB888 top-down rotation1。新工具以MSVC19.51.36256.0 v145 Release建置；Debug／ASan使用離線CMake的`/fsanitize=address`與既有VS14.50 sanitizer headers／libs／DLL，不安裝新runtime。使用既有27張opt-in RGB fixture設定`PAS_RGB_CLIP_ROOT`。

| 新建 binary 實際執行 | Release | Debug＋ASan | skip |
|---|---|---|---|
| C36h reference | 213／213，fail0 | 213／213，fail0 | 0 |
| main50 control | 244／244，fail0 | 244／244，fail0 | 0 |
| main50 variant（全套開關停用） | **243／244，fail1** | **243／244，fail1** | 0 |

唯一 variant 失敗：`GameTracking.CrossingLineDoesNotStealAnApproachingConfirmedNote`，frames4、5原gold期待line1／非conflict，variant得到line0／`confirmed_line_relation_conflict`。此為**已量測的消融退化**，保留原gold／XML／非零exit，沒有排除測試或改期待掩蓋。這不是ASan crash，兩份log沒有sanitizer錯誤。控制組／variant共用cases不合算為更多獨立場景，未宣稱跑完整repository；沿X1明列排除兩個GameRuntime transport cases：`ActualCaptureOptionsUseEffectiveProfileGuard`、`HistoricalFingerprintRejectsMappingDeviceAndUnverifiedReports`，不列為pass／skip。

新增6項實際CLI契約測試（兩lineage均跑）與main50另4項直接original tracking synthetic測試：enabled保留winner；disabled只改winner且原flag／conflict保留；score margin4時原ambiguity bypass保留；best已是confirmed或無eligible不產生第二語義。沒有照抄另一份策略實作。

實際工具CLI額外**15個預期拒絕、0意外成功**：manifest只改bytes／index／window、swapmain50／swapreference／duplicatecontrol、wrongvariant／provenanceSHA／trackingSHA／binarySHA、policy、failedrun、existingoutput、原R1／R2。均非零、明列reason、無新有效report；existingoutput保留SHA。X1正例用本輪新binary重算仍byte-identical，SHA`37d469caa0017be00fb78736fd69fb5cc87585c40b9fd671bb7e8397c420e7f7`。

五次完整primary run全exit0，每次核對7722／calls7715／skip7／EOFcontact0。variant兩次trace-on與trace-off的semantic digest、events／state-digests／mechanism實際檔案byte-identical，兩次on的109幀trace亦byte-identical。C36h與main50 control的canonical digest／events與X1完全相同，109幀核心trace亦相同；正規化只排除readonly新增X2diagnostics、export行號偏移與metadata／hostcost，scene／lifecycle／owner／contacts／candidatebank／prefix／scheduler不排除。

canonical digest：C36h`54ce21932b25eabf5d552f49b3020fa31856ba9c0ffe1c3d981a0b7bb79ec4ae`；main50control`c58428a4d4ecfa72253fb38f15de90771907a28bf3101cf0cf384225b5b28119`；variant三run均`a4f79cde887df2689506cf1105cda3e5403f9984cada7c61cf7b646d6c16f042`。驗證範圍是已綁定source／binary與固定pixels的本次產物，不是任意被改造trace的全面鑑證。

失敗亦保留：main50首build有C++JSON型別及namespace結尾錯誤，原`build50.log`／exit保留，修正工具後新`build50-repair1`成功；silent compare已產有效report但wrapper未建emptylog即hash而失敗，保存[recovered command](../measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2/commands/three-role-comparison-recovered.json)，exit0明列為由完成report推定，不假裝wrapper當時成功，未重跑fullreplay／覆寫report；第一次CLI負例wrapper用`$input`參數撞PowerShellautomaticvariable，實際manifestargv為空，非契約結果，原輸入／log／command保存，改`inputManifest`後新`cli-checks-final`15項通過。ASan收尾以`verify -ResumeChecks`續跑，不重新寫已完成report。唯讀檢視時的PowerShellparser／錯誤檔名不算primaryrun或CLI契約分母。所有工程嘗試與預期variantfail見attempt-ledger；沒有刪失敗資料或只報成功子集。

hostCPU cost範圍是perception＋accept／poll／drain及digest／event writer，不含PNG decode／hash，**不是capture→inject latency或fake時間**；有診斷allocation，trace-off也保留機制／digest。每run n=7715，整次run failure0／skip-perception7：

| run | p50 ms | p95 ms | p99 ms | max ms | jitter p95−p5 ms |
|---|---:|---:|---:|---:|---:|
| C36h on1 | 3.021 | 6.446 | 8.964 | 19.122 | 4.324 |
| main50 control on1 | 3.353 | 6.724 | 9.027 | 14.176 | 4.399 |
| variant on1 | 3.325 | 6.743 | 8.836 | 14.386 | 4.413 |
| variant on2 | 3.369 | 6.829 | 8.834 | 18.157 | 4.541 |
| variant off | 3.070 | 6.170 | 8.209 | 16.090 | 3.917 |

## 8. 容量、保護與可重算範圍

本新batch獨立硬上限**64MiB＝67,108,864B**，campaign＋prior仍受**8GiB＝8,589,934,592B**。啟動前X1=130,356,292B，campaign=7,762,085,288B，prior=45,307,809B，合計7,807,393,097B。manifest內campaign-before是建立manifest當刻的中途容量，不能代替啟動前快照。metadata／logs／失敗輸入／patch／source快照均在B2計額；commandlog在append前限制1MiB、replay為外部log／metadata保留2MiB，共用namedwriterlease，不刪舊資料、不另root規避batch額度。沒有複製rawPNG。

最終batch **41,783,234／67,108,864B**，剩**25,325,630B**；campaign＋prior **7,849,176,331／8,589,934,592B**，剩**740,758,261B**。包含ledger自身32,448B的計額、177份其他artifact SHA及`out/x2`各build／tools／export磁碟bytes見capacity-ledger。out/x2是編譯產物／export，另列磁碟帳；沒有把replayevidence轉移到out逃避額度。ledger自身SHA留null避免selfhash循環，自身bytes仍計入；toolfreeze保存23份tool source、25個binary／DLL、159個export檔案與全部6份XML。最後逐檔重核均0 mismatch；檢查次數不當獨立場景數。

`workspace-before.json`保存既有dirtyworkspace／受保護檔案與正式source SHA；初始guard scripts建立後才做該快照，不能說它是零X2檔案狀態。`status-before-append.md`是J6前完整原文。finalize重算受保護檔案／formal SHA、X1總bytes，驗status仍以原文逐字起始、formalGitdiff空，且tool-source副本逐byteSHA吻合。J1–J5及其歷史X2未啟動敘述不改寫，只在J6補本次交付。

## 9. 重現與下一個唯一建議

已完成的五run、CLI與build命令／exit／logSHA在`B2/commands`及`B2/logs`。下列只重算小comparison，newoutput必須不存在，先核剩餘容量；不要重跑會拒絕既有batch的初始化／prepare／run腳本，也不要在舊run中覆寫：

```powershell
$x2Batch='measurements/game-assist/2026-09-30-m0-manual-continue/confirmed-preserve-x2'
./out/x2/tools50/pas_frame_review.exe contact-x2-compare "$x2Batch/input-manifest.json" `
  "$x2Batch/c36h-reference-on-1" "$x2Batch/main50-control-on-1" `
  "$x2Batch/main50-no-override-on-1" "$x2Batch/acceptance-new-comparison.json"
```

重建時用tool-freeze的source／export與新build目錄，沿`commands/configure36.json`／`configure50.json`；不要把新compile當歷史productionbinary。variant回歸須設定`PAS_X2_NO_WINNER_OVERRIDE=1`並保留上述單項expectedfailure／非零exit，其他gold不改。全replay重現另需新batch額度及同manifestbinding，不能把不同manifestSHA冒稱本次run。

**判斷：winner覆蓋是可證的因果因素，但「只停用它即可恢復D的C36h垂直鏈」被本次結果否定。** initial水平relation仍在；原conflictguard接著限制切線；90ms自然expiry後水平overlap先完成contact；crossing負例也失去原保護。保留ambiguitybypass的synthetic測試排除了「誤關整個flag造成新ambiguity」解釋。

**唯一後續建議（須先獨立驗收，不在本chat啟動）：以已保存D6194–6214 pixels／trace與crossing負例，離線研究 confirmed-line conflict 與水平current-overlap的幾何role判據，先建立線局部座標／相對運動及有效relation切換的證據，再決定是否值得另一個單機制實驗。** 本輪不直接蒸餾「刪preserve」回C36h，不放寬lease／historyexpiry／conflict，不新跑第二消融或live。

仍unknown：D人工physicalnote／line-rolegold及水平Down對遊戲是否有效；正確touch時機／root／gameplay採納；sourceage、真recognition／RPCdelay與未錄perception序列；fixedpixels下新觸控應造成的後續畫面；77Miss逐Note原因、跨曲淨效果。這些unknown不由此次離線events數量或測試pass解除。
