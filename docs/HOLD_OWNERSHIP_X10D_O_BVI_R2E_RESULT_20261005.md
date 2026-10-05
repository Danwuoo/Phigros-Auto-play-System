# 4I-R2E結果：natural收據SHA讀取失敗，durable STOP

2026-10-05 Asia/Taipei。唯一attempt `bvi-r2e-20261005-01`。依[完整派送](HOLD_OWNERSHIP_X10D_O_BVI_R2E_DISPATCH_20261005.md)完成新局部恢復實作、封閉mock及source freeze，開始第一個真natural控制後遇明列hard stop。**工程partial；process controls aggregate未通過，BVI尚未編譯／冷驗，不能判candidate pass、family no-go或採用。** 原R1 repair1/1仍用完；沒有第二attempt或修改凍結runner追pass。

## 實際結果與停止

新[run.ps1](../research/x10d_o_bvi_r2e/run.ps1)單一入口、精確RootBinding、固定stage/argv、CreateNew預開輸出、durable RUNNING、共享nullable Result、checkpoint、有界nonthrow cleanup及同入口verifier已保存。R1 bvi.cpp/hpp逐bytes保持；main/driver只增預開report reservation及共用AssertionAccumulator→aggregate→return→consumer的contact負控制adapter，尚未C++編譯。最小WIC PNG target在首次configure前已納入source；未執行。

[mock-v1.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2e/mock-v1.json)25筆helpers/mock核驗0失敗：P01–P09、S01–S07、E01–E05及必要變體；CreateProcess0、child0、旧根寫入0。另一次工具shell以同CheckState讀scratch durable STOP拒絕續跑（S06）。這些是path/state/fact/cleanup-isolation helper核驗，不是所有Windows API故障注入或真child識別／清理驗收。C01–C04已預聲明並接C++共用機制，實際C++執行0。Add-Type只編譯owned-job interop，不能算BVI build。

source/contract/fixture引用在三控制前凍結。允許14個ordered stage＝3 controls＋11 product commands，至多一次Release configure、一次wrong-contact-only、各配置完整suite及一個PNG模式；本輪只有natural真控制被consume/launch。Release/Debug/ASan既有工具依賴只讀查存在與SHA，未以存在代驗證。

| 維度 | 保存事實 | 判定 |
|---|---|---|
| natural native | PID23864、creation FILETIME134356476193584800；完整pwsh.exe image，QPC frequency10000000，native0／runner0 | 確定launched/resumed/root exited，不冒稱未launch |
| 自然收尾 | assigned-suspended會員身分前後可信；root-exit、quiescence-end、final active/list0；held root handle退出，streams completed；無forced cleanup、無次錯誤，native elapsed1.28827s | 原15s內自然zero的單root事實；不能替owned-child負控制背書 |
| verification | exit1，讀natural-command SHA發生sharing violation | gate未通過，包級STOP |
| durable state | STOP revision3、stage consumed1、known launches1、receipts0 | 不授予下一stage；原next欄仍natural只保留停止前位置 |
| 後續 | nonzero0／owned-child0／configure0／build0／wrong-contact-only0／cold0／Debug0／ASan0／PNG0 | 全部未驗；原PNG額度已用0/2、剩2，不重置已用紀錄 |

失敗位於自然程序事實與VerifyResult已回傳之後的receipt Entry/Sha階段。預開command/result等handle為FileAccess.ReadWrite、FileShare.Read；`Get-FileHash`建立新讀handle的分享模式不容許已有write handle，發生「being used by another process」。此錯誤可由source與[失敗verification](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2e/natural-verification.json)定位。這是本包可避免的作者錯誤，不能歸給遊戲、BVI核心或未辨child平台問題。共享Result與8份checkpoint保住launch、native0、identity、自然active0及stream事實；catch保存verification1及STOP，未執行第二控制。收據handle於finally釋放後可只讀核SHA；後續維護讀取不補簽原verification pass。

沒有更改sharing flags或runner再試。未呼叫STOP後下一stage；另shell僅讀production STOP並以共用CheckState拒絕，無launch。所有已知與未驗欄見[stage-summary.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2e/stage-summary.json)。

## 來源、分母與保全

先重建R2D controller source_anchors的2134來源（0 SHA/bytes不符），再核delivery receipt、後續controller review全部檔、盤點頁、派送頁與dispatch receipt。去重後2145檔；原507 Git paths/dirty、HEAD f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c、index260c49037641b2c8c44f40e828491778779223193d91cb7cd824c8534ef7e9e1保持。完整來源清單仍引用原585722B manifest，未複製；post rehash見protection-after。src/include/root CMake及原apps/tests未改，舊runner/settle未跑，原source/oracle/out/收據不回寫。

首次compact摘要把兩個post anchors直接加2，其中一個已在dictionary，unique欄多計1。原protection-before保留，before-corrected按實際dictionary去重2145；原2134重建及SHA核驗均有效。另兩個rg wildcard-directory查詢遭Windows拒絕exit1，已改固定根搜尋；全部列入preformal維護finding，不偽稱產品反例。mock重要失敗0，真attempt失敗1保留。沒有刪原data／失敗或複製PNG。

保留原69＋20＝89cases、四層規劃356layer-case、22 supplemental、原4 schema/contact controls、1128coverage rows／2392引用。實際candidate layer-case0／assertion0、supplemental0、schema0。sizeof(Candidate/Observation/Descriptor)、metadata與probe實測均null／未驗；C++source static_assert與規格上限不是實測。contract/input/source/dependency/binary manifests列實際引用SHA；沒有BVI binary或新out。

只量得pwsh root及interop工程控制。MSVC/SDK/STL/CRT、WIC、BVI include/link與DLL完整compiled closure未驗；PowerShell Add-Type/Roslyn/.NET/system DLL closure未完整採集。已安裝工具fingerprints只作來源，不以預期或DLL存在當loaded closure。

52圖packet只保存原PNG path/SHA、host QPC capture/pixels_ready與獨立source-domain metadata；不複製像素。選取包未提供BVI front/depth/width/angle ROI及bounded-current全線幾何adapter。歴史full-prefix bank、owner/contacts、4R理由／部位、oracle／physical labels沒有供candidate；geometry-sources逐列來源SHA並記缺口。PNG driver凍結為provenance-gap模式，未执行，不能把其設計unknown52當實際審核結果或觀測支持。第二模式未聲明、未用；不建新辨識平台。

## 容量與交付

共用dispatch carry：aggregate8283855884B/8GiB、development6974627B/56MiB、controller1268200B/8MiB、out374705B。R2E新增全entry≤40MiB，newout≤128MiB並另納aggregate，保留controller剩7120408B。公式carry＋新batch＋新source/docs/tools＋新out；不重複加campaign內review。維護report≤64KiB、維護logs總≤2MiB，free每phase≥5704253440B。實際交付bytes、remaining、全部SHA与self exclusions以[final-receipt.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2e/final-receipt.json)與artifact-ledger為準，out新0。

交總控獨立驗收即止。下一包若再授權，最小工程問題是預開handle之hash/readback分享相容性，應先在封閉scratch測「writer仍開啟時的完整SHA鏈」；不能在本attempt內改runner、改root、reset state或第二次natural。BVI正負冷契約及三控制仍缺，故不提出owner採用建議或接正式owner。

Chapter Legacy全曲解鎖IN／完整IN Miss=0未新增證據。C36h tint1 behavioural baseline、main50 donor50/27/11 live0、suppression OFF、P excluded、X12 not-ready保持；无emulator/ADB/觸控/manual-session、模型、goal/automation、full replay、runtime/cost/stress、commit/push或自行續派。
