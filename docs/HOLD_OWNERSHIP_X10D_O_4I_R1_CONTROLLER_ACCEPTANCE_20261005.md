# 4I-R1總控驗收：保全交付成立，執行gate否決；續派4I-R2D契約審查

2026-10-05，Asia/Taipei。**簽收4I-R1工程／流程partial的封存與來源保護；不通過runner controls、不通過BVI實作或候選採用。** 本次不是新增平台非quiescence證據：兩個實際root均自然退出；阻塞來自runner路徑錯誤及核驗失敗後仍執行下一控制。唯一runner修補額度已用完，不能用「configure尚未用」推導可繼續控制或第二次修補。

原開發chat `01a1072a-52d4-7433-bd2c-a9a1b8c360ee`「修補 BVI-1 冷契約與程序身分證據」已idle。原[結果](HOLD_OWNERSHIP_X10D_O_BVI_R1_RESULT_20261004.md)、[交接](HOLD_OWNERSHIP_X10D_O_BVI_R1_HANDOFF_20261004.md)、[4I-R1授權](HOLD_OWNERSHIP_X10D_O_4I_CONTROLLER_ACCEPTANCE_20261004.md)及所有source、oracle、收據均凍結不改。結果檔日期10/4是原交付日期，本頁10/5為總控驗收日期。

## 獨立查核

總控新 [check-r1.ps1](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/controller-review/check-r1.ps1) 僅做SHA／bytes、JSON映射、Git與容量維護查核，沒有執行任何開發runner／control／configure／build／candidate／PNG audit。未修改產品或開發source。

[integrity.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/controller-review/integrity.json)：**2112個不同檔案、2326项hash引用、0不符**，包含2039份原保護檔、R1來源／凍結／依賴引用。原479個Git路徑與dirty保留，R1新增23個共502；本總控頁另加1。HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、index SHA `260c49037641b2c8c44f40e828491778779223193d91cb7cd824c8534ef7e9e1`未變，src/include/root CMake無diff。未宣稱全部SDK或未列入ignored raw全面重驗。

8份錯放檔的retained SHA／bytes均吻合artifact-relocation；原錯放路徑目前不存在，亦不在開工保護清單。這支持原檔未覆寫、錯放新增資料已保全；不能倒推當時輸出邊界一直正確。

原69個case／oracle／typed名稱順序一致；typed-r1移除新增left/right/rails_provenance三欄後與原typed逐case完全一致。新增20 cases、4 schema/contact controls；1128列覆蓋表展開2392個case/step/field/layer引用，與原及新增expected欄位重建清單一致。原22 supplemental保留。這是結構核對，**不是356個layer-case、22 supplemental或4 controls已執行**。原oracle SHA `90f8680c4362d07a718ac2bb7142b37396871b7ffd81523a06ddd8a685ec5425`不變。

R1開發新增2681257B＝batch2567130B＋external114127B；累計development6826223B。交付時aggregate8283044410B、remaining306890182B，新out0B，原out374705B保留，均獨立重算相符。本輪總控新增review／本頁繼續占原controller8MiB，settled值見 [controller-receipt.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/controller-review/controller-receipt.json)。不回寫原開發final receipt。

## 執行時間序與停止違規

總控讀取原chat工具歷史並保存 [stop-sequence.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/controller-review/stop-sequence.json)，不是只接受自述：

1. `control-natural` 呼叫exit0；native receipt PID36100、creation134355963840706648、pwsh.exe完整image，記自然quiescence、streams完成、active_final0，約0.850秒。
2. 下一個獨立核驗命令讀R1/control-natural-exit.json，因檔不存在而exit1，明確拋出 `natural control failed; stop`。
3. 隨後仍呼叫 `control-nonzero`，native7、工具exit1；PID18652、creation134355964021246632、同pwsh.exe，記自然quiescence、streams完成、active_final0，約0.888秒。這個後續呼叫違反「任何控制未達即停」。

[run.ps1](../research/x10d_o_bvi_r1/run.ps1) 第4行仍指向舊4I batch，Save和stdout/stderr也共用該根。錯誤literal置換沒有讓資料落到R1；同時第9行capacity計數取的是錯誤batch，故執行前該欄亦不能作R1正確容量gate的證據。最後settle獨立重算仍吻合，兩者不能混稱。

原兩個control收據只證明兩個root的歷史身分／自然結束紀錄，第三owned-child負例未執行。沒有child留存／拒絕／清理的控制證據，也不能辨認原4I configure的2個未知member。總控未重現歷史process狀態；不把收據中的active0描述為本輪重新清理驗證。

runner一次repair已用，第三控制、configure、build、test、Debug／ASan、PNG audit皆未啟動。移檔是證據保全，不是修補runner、更不是重新放行。已執行的第二控制保留為程序違規，不刪掉來美化分母。

## source修補的審查結論

- **F1／F2有實際對應修改。** relate先將6份past減為5份才計current；有效窗口內RGB或descriptor任一signature已見就不新增獨立確認，span改為first/last independent之差。新R01/R02/R03/R04包含淘汰前後及without-oldest對照；R05/R06/R07分列ABA、duplicate延長span與30ms正例。讀source可確認改動位置，不能據此宣稱C++行為通過。
- **F3／F4有實際harness增補。** driver加入e2e，使用同一RGB提取結果通過link與fake constraints；expected先做handler／適用層檢查，實際checked欄再對coverage rows；輸出contact_id並驗逐幀傳遞及1→4改名。effect僅RGB/e2e適用，原V04 true未改。新增wrong-contact control目前是獨立比較兩個fixture數字，後續恢復設計須說明如何經真正的assertion／aggregate路徑拒絕，避免只測 `4 != 1` 就稱已驗整條拒絕鏈。
- **F5的範圍仍是conditional fake contact。** 已知一致receipt下，current body/rails/contact及unique line可供Move/refresh，不再要求新Down端部確認數；新Down保持3份／30ms。R08到120ms逐幀更新last-contact；R10失支持序列在121ms最後更新後到181ms達60ms，要求release。這是fixture算術與source閱讀；attachment_query為外部fake條件，未解決真實ownership或物件跳接，不能直接接正式owner。
- **F6 typed provenance保留限制。** left/right為獨立聲明fixture資料，未當作RGB識別成果；新增欄位不改原typed欄，effect由RGB/e2e承擔。尚無build、sizeof、probe實測、實際assertion分母或核心反例結果。

另有恢復前必審的source風險：run.ps1外層catch一律記`native_launch_possible=false`，但try包含native Run及之後Save exit；若退出收據寫入失敗，native可能早已執行。C# cleanup/finally裡也可能再throw，使外層收不到Result。這是靜態風險，並非本次兩個自然控制已發生此情況。新契約須依實際launch階段保留uncertain，而不是憑外層exception填「未啟動」。

因此六項修補可作下一次冷驗來源，**不能在本輪逐項簽pass**；目前也沒有證據否決整個BVI演算法族。執行管線與BVI候選的狀態必須分開。

## 授權下一包4I-R2D：只讀恢復契約審查

依使用者順序派送授權，使用 **GPT-6.1 Sol／xhigh**，一個研究chat。此包是局部執行管線的恢復設計審查，不是第二次runner修補或configure重試；原runner剩餘repair0、controls不通過仍有效。完成後交總控，總控另行驗收才可能授予任何新的實作／執行包。

輸入只讀原4I／R1授權、source、fixtures、manifest、controls、stop-sequence及新controller receipt。可用必要維護shell重核SHA／bytes／JSON，查官方文件作設計依據；不得執行既有runner、Add-Type runner、process controls、child probe、configure/build/test／產品binary。不得修改原source／oracle／receipt、重裝toolchain、調OS／registry、改quiescence或改BVI門檻。本包不接owner／live、不做PNG／full replay／成本／模型／goal／automation／commit/push，不自行續派。

交付僅新 `docs/HOLD_OWNERSHIP_X10D_O_BVI_R2D_REVIEW_20261005.md`、新handoff與小型machine-readable設計／保護／容量收據；證據根 `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2d/`。不新增可執行runner／candidate，不抄整批封存。總新增≤1MiB，含全部external docs、JSON、維護腳本及失敗，計入原development56MiB剩餘51894033B；不是新額度。out新增0B；總控仍共用原8MiB。保護集合以本頁controller receipt與ledger完結值為準。

最小可驗收輸出：

1. **根路徑先決条件。** 一個顯式canonical輸出根來源；在任何native launch、log／receipt建立前驗證repository範圍、精確允許的batch及舊封存不可寫，禁止靠leaf字串置換；列錯root、舊root、parent traversal、已存在output、無法建立收據的拒絕結果。來源根、證據根、out根與容量carry取同一解析結果。
2. **機械式停止。** 給出小型狀態／轉移表：preflight→natural已核→nonzero已核→owned-child已核→configure可執行；任一缺收據、非預期exit／SHA、path不符、identity不可信、cleanup不明或驗證失敗，都進本次attempt不可再前進的STOP。說明跨shell呼叫如何讀取唯一attempt狀態，不能只依agent記得上一條錯誤。此處只設計，不試跑或建立新工作流平台。
3. **誠實收據與失敗保全。** 區分未launch、可能launch、已launch、root退出、owned descendants未知及cleanup已核。說明exit receipt寫入失敗、cleanup/finally異常時如何保留已知資料與uncertain，不以exception冒稱native0次；盡量縮小現有runner的修補面。
4. **控制充分性及錯誤分類。** 自然0、預期非零7、owned-child拒絕三種控制如何各自機讀判定；非零7是預期值，不能和wrapper／verification失敗混淆。原先錯放收據與「失敗後第二控制」須在設計中有明確反例路徑。contact負控制須走共用assertion→aggregate失敗的設計，不能只比兩個數字。不得把設計案例列成已跑tests。
5. **恢復決策。** 對原R1 source及20新增cases說明可直接保留與尚需最小修補的清單，列明一次未來執行所需前置、hard stop與剩餘容量；提出單一推薦方案及可行性／未知，不授權自己實作。不要另建泛用測試平台或無限工具探索。

候選未驗、原repair額度0、下一包只讀設計，三者均須在交接與機讀receipt保留。後續若要新runner修補，必須由總控明確另立範圍，不可偷算在本R2D或重置R1計數。

產品目標與狀態不變：Chapter Legacy全曲解鎖IN、完整IN Miss=0，HD解鎖／回歸，P/G/B照報；章節分母及當前逐曲解鎖unknown。C36h tint1 baseline、main50 donor/live0、suppression OFF、P excluded、X12 not-ready。當前沒有新的遊戲改善或IN zero-miss證據。
