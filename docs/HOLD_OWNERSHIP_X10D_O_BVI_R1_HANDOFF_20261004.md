# 4I-R1交接：停止於runner路徑控制，產品冷驗未執行

2026-10-04 Asia/Taipei。先讀[4I總控完整授權](HOLD_OWNERSHIP_X10D_O_4I_CONTROLLER_ACCEPTANCE_20261004.md)與[R1結果](HOLD_OWNERSHIP_X10D_O_BVI_R1_RESULT_20261004.md)。原4I／4R／O source、oracle、receipt、out及docs保持，不重跑原settle。HEAD/index／原479 Git paths/dirty以protection-before/after核對。

- 新source：`research/x10d_o_bvi_r1/`，candidate為bvi.hpp/cpp；renderer及typed adapter在main.cpp，嚴格coverage與e2e／fake序列driver為driver.inc。CONTRACT／PROTOCOL是修訂入口，source未編譯。
- 新batch：`measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1/`，開工保護2039檔；specification-freeze先於candidate，pre-execution-binding及source snapshot先於controls。
- 原denominator：直接引用原normalized-execution/oracle及原22 supplemental，69＋22不刪；typed-r1.json在保留原typed全部欄位下增補rails provenance，diff獨立保存。原oracle SHA90f8680c4362d07a718ac2bb7142b37396871b7ffd81523a06ddd8a685ec5425不改。
- 新oracle：r1-cases.json有20案例及4負控制，expected-coverage.json有1128行mapping。所有layer-case／assertions未執行；不能把清單核對当作測試通過。
- 新out：`out/x10d-o-bvi-r1/`未建立candidate輸出，configure/build均0。binary-manifest記candidate0；沒有probe EXE冒充candidate。
- 交付closure：dependency-manifest、input-manifest、artifact-ledger、delivery-status與final-receipt；完整compiler／SDK／runtime closure未驗，manifest只說明實際保存／核對範圍。

剩餘runner工程修補已用1次，原runner、diff及凍結缺陷版本都保留。新runner的batchPath仍指舊4I batch，原因是leaf-only Replace未匹配full literal。natural native0與nonzero native7的新增收據曾錯放舊root，第一個R1收據核驗失敗後第二控制被錯誤執行。8份新檔已在確認不屬原保護檔後移回R1，SHA／錯放path／責任記artifact-relocation。原檔未修改，全部失敗保留。

兩個root身份已記PID／creation／完整image與QPC時點、membership；兩個job自然active0、cleanup_zero、streams completed。第三owned-child負控制未執行，descendant identity及非自然cleanup能力尚未驗。**controls aggregate不通過，不准configure；不得第二次修runner或以兩個自然root控制放行。** 原4I兩個殘留member仍unknown，不猜vctip。

source六項修補的內容與限制見static-delivery-review及結果表；沒有可執行核心反例或family verdict。未做52PNG audit（原0/2）、owner/backend整合、runtime/cost/stress/full replay或live。本包到停止點即交，總控獨立驗收後才有可能另定新授權；本chat不續派。

額度不重置：原development4144966B、controller605130B、out374705B、aggregate8280363153B全carry，R1新source/docs/tools/failed artifacts另記；controller review已在campaign，不再加一次。final receipt是完整容量收尾依據，self hash排除；ledger／receipt自身SHA可由獨立驗收再計。PNG複本0、原資料刪除0。

C36h tint1 baseline、main50 donor/live0、suppression OFF、P excluded、X12 not-ready保持；Chapter Legacy分母／解鎖unknown，無新增IN Miss=0或遊戲改善證據。
