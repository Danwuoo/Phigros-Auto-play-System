# 工作包2：選項A獨立 checker，有限處置後未完成

2026-10-04 Asia/Taipei。**本包已交來源／完整性核對與守門失敗證據；獨立 checker 未建置、未驗收，待總控審查。** 唯一一次工程修補後，configure 根程序 exit0，仍有 private Job 所屬 vctip.exe 超過原兩秒 quiescence gate；守門正確拒絕，Job cleanup verified、active0。依凍結停止點沒有繼續 build，也沒有第二工具鏈、追加 runtime/cost/stress/full replay/game。

HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，唯一 worktree。工作包1八檔、總控驗收頁、全部原 dirty、原 reader／raw／freeze／ledger 均保持。正式 src/include/root CMake、index、策略與資格未改；未 stage/commit/push/reset/clean。新成果只在 [review root](../../../measurements/runtime-decision-skip-a/controller-review-independent-package2-20261004) 與同名新隔離 build 根；本頁及交接為新增文件，不回寫總帳或工作包1入口。

## 已完成的來源及完整性核對

[source-integrity.json](../../../measurements/runtime-decision-skip-a/controller-review-independent-package2-20261004/source-integrity.json) 重新 hash／核 bytes，以下各組有重疊，不能相加當 unique 檔案數；unique=1710、mismatch0。

| 分組 | references | mismatch |
|---|---:|---:|
| 原 input-manifest-v2 | 94 | 0 |
| 原 A 最終 source/binary/tool bindings | 12 | 0 |
| 原 A 實際 CL/link/compiler dependency freeze | 396 | 0 |
| R3 原 artifact-ledger | 809 | 0 |
| R3 controller ledger | 92 | 0 |
| R3 compiled dependencies | 575 | 0 |
| 接手既有 dirty/untracked（含工作包1及驗收頁） | 229 | 0 |

舊 `C:/Users/wurre/Desktop/dots-control` 整根不存在。以有界檔名搜尋找到桌面 `independent-checker-002-20261004T0519.zip`；九檔全部符合 archive 內 manifest，其中 owner checker 36320 bytes、SHA256 `d95bd77c051acbe9aa24c0d5340022d495997ee60041d62c49193b9a823de805`，也符合歷史 runner 內寫死的期望 SHA。這是 **002 archive 副本復原**，不是恢復原外部目錄；[recovery.json](../../../measurements/runtime-decision-skip-a/controller-review-independent-package2-20261004/recovery.json) 記 zip SHA、來源及每檔核對。舊 work-order／runner 僅作資料，未採其外部指令。

原始 archive 固定保存於 `recovered-002/`。`source/` 保存首方案；`source-repair/` 保存唯一修補方案與實際執行的來源。復原 owner 演算法使用自有嚴格 flat parser、ordered-vector neighbors、integer ns 與 nearest rank，不共享原 reader。它原範圍僅 owner frames；另新增 C++20 `independent_raw_review.cpp` 草稿，直接處理五 run frames／receipts／releases／timings／archive、decision-sequence→source-frame mapping、逐列 bounds、分母、enqueue/call bounds，再第三層比較原輸出。它不連原 reader／runtime core；新功能 **未編譯、未測試，不能作可信 checker**。

## 實際執行與停止

[protocol](../../../measurements/runtime-decision-skip-a/controller-review-independent-package2-20261004/protocol.md) 在首次 build 前凍結。保留原兩秒 gate，root suspended→private unnamed Job→resume，CREATE_NO_WINDOW；最多64 active Job processes。維護 guard 增加 creation-time／image-path／membership 觀測與 free-space 檢查，清理用 Job handle，不按 PID 或程序名殺其他程序。所有時間／log／process／output limits及精確命令見 protocol 和兩份 receipts。

| 執行 | root exit／guard | 實際證據 |
|---|---|---|
| guard controls | 7/7 通過，0失敗 | normal、fast/active log overflow、build/review growth、timeout+foreign sentinel、root exit with descendant；各負例 verified cleanup。此次7項，未冒稱重跑歷史003的8項。 |
| 首次 configure | exit1／拒絕 | `/d2:-notip` 導致 compiler C1007 `unrecognized flag '-:-notip'`，且 owned vctip survivor；elapsed6.2598684s，cleanup verified／active0。 |
| 唯一修補 configure | exit0／拒絕 | 移除無效開關，仅新子程序 `TF_BUILD=True`；MSVC19.51.36256.0、SDK10.0.26100.0、VS18/v145 configure成功；elapsed8.0061141s，仍有 owned vctip，cleanup verified／active0。 |
| checker build／ASan／synthetic／raw | **全部0次** | pipeline在configure guard停止；歷史24 controls與新增12 controls均未執行，沒有 checker binary 或可信結果。CompilerIdCXX／ABI probe binary不能冒稱 checker binary。 |

首方案選用未有公開支持的 compiler 開關，是本包工具選擇錯誤；已保存全部失敗，不把它說成 MSVC 不可用。Microsoft [CL/_CL_ 文件](https://learn.microsoft.com/en-us/cpp/build/reference/cl-environment-variables?view=msvc-170)只支持參數傳遞行為，不支持該內部開關。唯一修補依本機 cl/link binary 中可見的 TF_BUILD／CI 字串及 [Microsoft TF_BUILD 定義](https://learn.microsoft.com/en-us/azure/devops/pipelines/build/variables?view=azure-devops)提出子程序處置；官方並未保證能抑制 vctip，本次實測確實未清空 Job。沒有全域環境／registry／服務／VS 檔案變更。

修補 survivor：PID27460、creation FILETIME134355799590020176，image為既有 `.../MSVC/14.51.36231/bin/Hostx64/x64/vctip.exe`；Job membership verified，root exit後及清理前皆可讀身分。PID僅為觀測，終止依 private Job。見 [configure-repair-command.json](../../../measurements/runtime-decision-skip-a/controller-review-independent-package2-20261004/configure-repair-command.json)、[首次 receipt](../../../measurements/runtime-decision-skip-a/controller-review-independent-package2-20261004/execution-receipt.json)、[修補 receipt](../../../measurements/runtime-decision-skip-a/controller-review-independent-package2-20261004/execution-repair-receipt.json)。後續沒有嘗試放寬 gate、忽略 survivor、再猜 switch 或換工具鏈。

## 結論分層與未完成範圍

| 層級 | 本包支持範圍 |
|---|---|
| 原 reader reproducibility | 引用0419原17/17及verify-input相同tree；此包重新核其封存SHA，沒有重跑，也不把它充作獨立checker。 |
| 獨立來源審查 | 復原002 exact-SHA；已讀 R3 的 recognition_complete→lease reset→mutex publication 與 selection→兩項enqueue→owner_start/game.accept 順序。這支持 publication／selection **bound 契約**，不提供精確actor時刻。新補checker為未測草稿。 |
| 獨立合成／逐列raw | **未完成**。無可簽收24/12 tests、五run12800 attempts、owner193／85、全194／139、193逐列bounds/joins、136/51/6、6exploratory、RGB mapping、群聚／matches／quantiles的新checker實測。SHA完整性不是解析／數學重算。 |
| 因果／成本／產品 | 193因果Unknown；X12not-ready、B1成本Unknown、noise未評估、suppression OFF、14lost機會利弊Unknown、C36h77Miss未改善、main50donor/live0不變。 |

原 reader 報告的 `2560published/2475consumed/2282owned=89.140625%<90%`、publication136/51/6與6exploratory、全194/139及RGB差異均仍是已保存的 **歷史來源結論**；本包不新增獨立驗收背書，不從少skip/Down推稱Miss改善。不能用193因果不可識別作checker必須全解的前置；本次未完成原因是有限工具方案的quiescence拒絕。

## 容量、保護與交接

接手 old0419/002/003合計901551B，A build累計64662477B，A batch35655668B。新review4MiB／新build128MiB採較嚴上限，並计入總review16MiB、A build1GiB、A batch64MiB；free至少5GiB，未複製raw／PNG／約12MiB整套report。最終實測及所有新檔SHA見 [final-delivery](../../../measurements/runtime-decision-skip-a/controller-review-independent-package2-20261004/final-delivery.json)，包含新source/log/report與本頁／handoff的證據快照；檔長不冒稱NTFS allocation。

[protection-after](../../../measurements/runtime-decision-skip-a/controller-review-independent-package2-20261004/protection-after.json)重核原229檔與全部舊A檔／來源bindings。下一包B最小輸入與停止界線見 [handoff](RUNTIME_DECISION_SKIP_A_INDEPENDENT_HANDOFF_20261004.md)。此包只交總控審查；沒有自簽第二包完成、續派B或啟動O／live。總控若要另定工具處置，需新決策；本chat已到預定停止點。
