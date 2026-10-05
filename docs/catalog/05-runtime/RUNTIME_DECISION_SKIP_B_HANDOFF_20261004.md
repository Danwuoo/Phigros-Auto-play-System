# 工作包3交接：B契約有識別性，尚未實作

2026-10-04 Asia/Taipei。**建議值得另案最小實作，只回答短residency是否被前selection→accept入口區間完整涵蓋。** [審查頁](RUNTIME_DECISION_SKIP_B_CONTRACT_REVIEW_20261004.md)為設計草案，待總控驗收；通過也不授權implementation、R4、新AA／cost／live。本chat交付後停止，不續派、傳訊、goal或automation。

新增入口：`measurements/runtime-decision-skip-a/controller-review-b-contract-20261004/`。`input-binding.json`是34個選定來源identity；`selected-binding-checks.json`只核19筆既有manifest／R1 binding期望。`raw-examples.json`保留8份小例列的來源SHA及line，沒有完整raw／193逐列重算。`fixture-draft.json`是手核整數oracle，包含真陽性、partial／late反例、兩個相容排程的Unknown、censor／同timestamp／membership／consumer gap及invalid；test_execution_count=0。

最小契約：每publication及每successful selection各兩次同mutex內相鄰QPC讀值，原owner_start/end保守夾accept入口；const snapshot与其epoch／generation／geometry／decision sequence／source frame／publication ordinal綁定。所有publication側錄包括未owned者，selected-but-fault與未selected分開。兩個新clock對不能拆JSON／OS／lock原因，owner_start也不是精確callee入口。

「主要」固定為短（≤4ms）skip份數嚴格>50%，原90%成本門檻不變。K=確定short且full、F=確定short且非full、U=可能short／cover Unknown／censor；保守比例上下界保留全部U。上界≤50%否證，下界>50%只支持當run區間命題，其餘Unknown；長skip、consumer skip與全部attempt另完整列帳。沒有以降低90%、挑owned成功subset或刪診斷來收斂。

預期工程選擇：只有完整區間支持H且observer effect可接受，才值得另研究accept前共同診斷服務；反例主導或識別性不足就停止量測線。未來冷C++20實作／獨立fake-clock／public-output bridge、D/T診斷ON/OFF與最多8個有限A/A screening runs只是提案，尚未執行或獲授權。新增熱路徑側錄陣列≤1MiB，post-join trace≤8MiB/run，原80MiB另計；未來最多768MiB campaign追加提案須重核舊8GiB／2GiB／10GiB帳与另決策。不能重開compiler／guard支線或把它變X10d-O前置。

本包維護核對及容量以[final-receipt](../../../measurements/runtime-decision-skip-a/controller-review-b-contract-20261004/final-receipt.json)與[final-capacity](../../../measurements/runtime-decision-skip-a/controller-review-b-contract-20261004/final-capacity.json)為準。接手232個dirty/untracked、206 tracked及285個舊A檔案，去重717份保護快照；另核選定frozen inputs。起始review2,198,701B／batch含外部文件36,952,818B／build65,366,106B獨立重核相符。所有新增文件／JSON／收據計入本包1MiB及既有review16MiB／batch64MiB，原檔不回寫、無raw／PNG整套複製。

HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、唯一worktree。A checker仍未完成；2037來源一致與原17/17不能補簽raw解析。原193因果Unknown、X12not-ready、noise未評估、B1成本Unknown、suppression OFF、14lost利弊Unknown、C36h77Miss、main50 donor/live0均保持。產品目標仍全Chapter Legacy解鎖IN及同版完整IN Miss=0，HD為解鎖／回歸，無AP前置。

本包configure／build／checker／probe／guard／runtime／cost／stress／full replay／emulator／ADB／觸控／UI／模型均0；沒有產品碼修改或stage／commit／push／reset／clean。PowerShell只有有界檔案維護與小例列讀取，失敗的查找／語法嘗試保留於receipt，未執行任何被禁止工具。下一步X10d-O由總控另派，不依賴193全歸因；本chat不派送。
