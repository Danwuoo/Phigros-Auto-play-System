# 4I-R1結果：source修補已保存，runner路徑控制失敗，停止partial

2026-10-04 Asia/Taipei。依[新總控授權](HOLD_OWNERSHIP_X10D_O_4I_CONTROLLER_ACCEPTANCE_20261004.md)執行唯一 BVI-1 同族冷修補。**本包停止於 runner 工程控制；configure/build/test 均0，不能判演算法pass、core no-go或採用。** 原4I結果／交接／oracle／controller收據不覆寫。

開工重核總控2031個不同來源及新增總控保護檔，保護快照共2039檔、0不符。原479個Git路徑、HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、index SHA `260c49037641b2c8c44f40e828491778779223193d91cb7cd824c8534ef7e9e1`與dirty保存。完整收尾核對及容量以R1的[final-receipt.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/final-receipt.json)為準。

## 六項修補與凍結

[CONTRACT](../research/x10d_o_bvi_r1/CONTRACT.md)、[PROTOCOL](../research/x10d_o_bvi_r1/PROTOCOL.md)、新反例、獨立 expected／欄位覆蓋表及 typed provenance 增補先保存並SHA凍結，之後才修 candidate。原oracle SHA `90f8680c4362d07a718ac2bb7142b37396871b7ffd81523a06ddd8a685ec5425`保持；不改像素／matching閾值、不換family、不複製PNG。

| 發現 | 新source／harness處理 | 驗證狀態 |
|---|---|---|
| 第7份影響當前輸出 | relation先淘汰，最多5 past＋current；加入count及age淘汰前後歧義／確認輸出與移除witness對照 | source已保存，執行未驗 |
| A/B/A及duplicate延長span | 有效窗口內任一已見RGB／descriptor signature即去重；獨立first/last及current freshness分列 | source已保存，執行未驗 |
| RGB到fake完整路徑不足 | 保留RGB／typed／lifecycle，另e2e以实际RGB extract→link→fake核relation、independent、usable、Move／Down | harness已保存，執行未驗 |
| 未知expected、contact ID | 前置schema fail closed、全欄位覆蓋、permutation白名單；逐幀核contact傳遞與1→4映射，4負控制 | harness已保存，執行未驗 |
| 長靜止Hold | 已知fake receipt的current bilateral body/rails支持與新Down確認分開；跨6份及90ms逐幀更新last_contact／prefix／ID；失支持、歧義、unknown/completed/expired負例 | source及序列已保存，執行未驗 |
| V04 typed effect無來源 | effect僅RGB/e2e適用，typed不宣稱已核；原期待true不改。另增加author-declared typed rails provenance供current支持分層 | 欄位適用性已聲明，執行未驗 |

原20頂層／69展開及22 supplemental保留在分母。新增20個案例及4個schema/contact負控制，1128行expected覆蓋表；規劃4層×89 cases＝356 layer-case、另22 supplemental及4負控制，**實際全部未執行**。sizeof／metadata／probe實測、assertion執行分母、RGB／typed／fake結果均未驗。沒有以source閱讀推定原演算法已fail。

## 明確停止點及工程失敗

原runner與[runner-repair.diff](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/runner-repair.diff)保留，使用了剩餘的一次修補。增加owned job PID-list、PID／creation FILETIME／image、QPC／UTC觀測時點、前後membership核對及缺失原因；15秒quiescence不放寬。

**修補中的batch literal置換錯誤**：只置換被引號包住的leaf名稱，實際字串含完整relative path，未匹配。runner仍把兩個控制的新增command／exit／stdout／stderr寫到原4I batch。第一個控制之後讀R1收據找不到，驗證exit1；開發者錯誤地仍執行第二個控制。這是本輪可避免的工程／流程錯誤，非產品或核心觀測反例。

| 控制 | native結果 | 身份／收尾證據 | 包級判定 |
|---|---|---|---|
| natural | exit0 | root PID36100、creation134355963840706648，pwsh.exe完整路徑；自然active0、streams completed、identity trusted | 收據位置錯，控制gate未通過 |
| nonzero | exit7；工具呼叫回報exit1，保留native7 | root PID18652、creation134355964021246632；自然active0、streams completed、identity trusted | 在前項驗證失敗後被錯誤執行，不放行後續 |
| owned-child負例 | 未執行 | descendant識別／非自然拒絕／owned cleanup尚未建立 | 未驗 |

兩個已執行控制各native約0.9秒；不是candidate test。所記image皆 `C:/Users/wurre/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/pwsh.exe`。兩者只有root，不能證明會辨識殘留child，也不能補認原4I的兩個未知member。原4I configure仍是root exit0、15秒不歸零、wrapper125，不改原判定。

立即停止後，先以開工保護快照確認8份錯放檔均是本輪新增、不是保護原檔，再逐檔驗SHA並移至R1。[artifact-relocation.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/artifact-relocation.json)保存原錯放path／移後path／bytes／SHA。原檔不刪、不覆寫，R1失敗artifact完整保留；原4I新增誤放entry已移走，未使用第二次runner修補。frozen source／inputs／dependencies的109次只讀核對0不符。

新Release configure0／最多1；build0、candidate binary0、cold test0、Debug0、ASan0、52PNG audit0（原0/2保持）、full replay0、runtime/cost/stress0、live0。未掃殺同名程序、未重開A/vctip/OS/registry/toolchain支線，未連owner/backend、啟動emulator/ADB/觸控/模型/goal/automation，未commit/push或續派。

## 保留與容量

R1 source與pre-execution snapshot、原輸入引用、新JSON、所有失敗、兩個控制收據、source finding review、dependency/input/binary manifests、逐檔ledger及收尾receipt均在新R1路徑。未編譯，不能主張compiled dependency closure；只有已核installed tool／header fingerprints。MSVC/SDK/STL/CRT/system DLL、PowerShell Add-Type/Roslyn完整closure未驗；無candidate DLL/backend。

沿用development56MiB carry4144966B、controller8MiB carry605130B、out256MiB carry374705B、aggregate8GiB carry8280363153B。R1 batch及所有external source/docs/tools/snapshots/failed artifacts另計，controller review已在campaign，不重加。逐entry logical bytes與完整free/cap餘額由final receipt列出；本輪PNG複本0、原資料刪除0。

產品狀態保持：Chapter Legacy章節分母與逐曲解鎖unknown，全部解鎖IN及完整IN Miss=0尚未驗收；C36h tint1 baseline、main50 comparison/donor 50/27/11/live0、suppression OFF、P excluded、X12 not-ready。到此交partial，等待總控獨立驗收；不自行修runner、重試configure或派下一包。
