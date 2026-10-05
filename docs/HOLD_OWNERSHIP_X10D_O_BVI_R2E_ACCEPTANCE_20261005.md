# R2E總控獨立驗收：接受工程partial封存，執行與BVI驗收未通過

2026-10-05 Asia/Taipei。總控只讀來源、Git、SHA/bytes及封存程序證據；未執行開發runner、interop、configure、build、candidate、PNG或live。開發報告見[R2E結果](HOLD_OWNERSHIP_X10D_O_BVI_R2E_RESULT_20261005.md)，新獨立證據見[controller integrity](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r2e/controller-review/integrity.json)。

## 驗收結論

接受「工程partial且已STOP」的交付分類與封存；不接受execution recovery完成、三控制通過、BVI冷契約pass或產品採用。2214個唯一檔、2387個引用SHA/bytes核驗0不符，其中原保護集合2145檔。原507 Git paths／dirty、HEAD f83c7ea、index SHA保持，正式src/include/root CMake無diff。R2E新增320063B＝batch206810＋external113253；out新增0。開發交付aggregate8284175947B／8GiB與self bytes核對成立；本次總控資料另計controller receipt，不修改原收據。

natural-command/result、8 checkpoint、verification、state彼此一致：native0、runner0，launched/resumed/root exited，PID23864／creation134356476193584800，完整image、可信membership，root-exit／quiescence-end／final active0，streams完成、無強制清理，elapsed1.28827s。但verification1；STOP revision3、consumed1、known launches1、receipts空。唯一native命令收據為natural；其餘controls、configure/build/test、PNG均無執行證據，開發也明列0。這是封存事實核對，沒有重跑控制來回填pass。

## 已定位問題及證據邊界

run.ps1的Reserve保留ReadWrite／FileShare.Read writer；VerifyResult之後的Entry透過Get-FileHash開新reader，在writer仍開啟時發生sharing violation。result已寫入且共享Result仍在，catch保存verification1和STOP，finally才釋放handle。這是runner收據交易的作者錯誤，不是BVI反例、未知child平台故障或A/vctip問題。

25筆mock的結果列均pass；source顯示它們測path/state/helper與故障事實保存。沒有把實際預開writer、command/result/log/report/verification的hash與readback、receipt引用、durable下一stage串成完整交易。故25/25不能支持該交易正例。下一包必須先以真檔案I/O及fake native事實測完整共用交易，並驗SHA/JSON所有入口，不只替換Get-FileHash單點。仍開啟的report／identity writer與Get-Content/ReadAllText讀取亦需覆蓋。以相容reader或既有handle做SHA時，不得藉普遍放寬FileShare.Write失去保護，也不能hash尚未停止寫入的內容。

另發現R2E png.cpp只是decode原圖後固定記missing geometry/unknown；沒有呼叫候選提取、关联或更新。其誠實標示candidate_executed=false，因此沒有偽報pass，但即使日後執行成功也不能算BVI真圖審核。geometry-sources指出合法current ROI／全線adapter均缺。下一包保留52圖額度0/2，不消耗額度執行已知只輸出unknown的模式；用有限來源審查列出合法上游幾何的具體接法與缺口即可。

R1核心bvi.cpp/hpp、原69＋20 cases、22 supplemental、4 controls、1128 coverage rows／2392引用未取得任何新C++執行結果。contact adapter共用assertion→aggregate→return／consumer的source已存在，尚未build或真正負例驗證。不得把source static_assert、依賴檔存在或Add-Type當candidate驗收。

## 下一步與產品距離

使用者已授權驗收後續派且盡量合併多步。依[新R2F派送](HOLD_OWNERSHIP_X10D_O_BVI_R2F_DISPATCH_20261005.md)，另給一次明確局部恢復範圍；R2E STOP永久保留，R1 repair額度不重置。R2F先完整storage交易預驗，再按gate連續三控制、Release負控制與完整冷驗、可用Debug/ASan；不把缺合法幾何拖成全部冷開發停擺。

距首個候選實機pilot仍是原五個邏輯階段，第一階段尚未通過：可信執行恢復→BVI冷契約及合法真圖觀測→隔離owner整合回歸→同候選runtime/cost→freeze/preflight與有界live准入。這不是固定五個task或時間承諾。Chapter Legacy分母／當前各曲IN解鎖unknown，IN Miss=0未增加證據；C36h tint1、main50 donor/live0、suppression OFF、P excluded、X12 not-ready不變。

本次維護script的最後console摘要曾對OrderedDictionary直接Select-Object而顯示null；實際integrity.json先完整寫入，隨後重新讀取核得上述2214/2387/0。沒有執行失敗或修改交付數值；此顯示限制保留記錄。
