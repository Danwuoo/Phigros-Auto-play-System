# BVI build包：新增診斷預驗18/20，真native前STOP

2026-10-05 Asia/Taipei。唯一run `bvi-build-20261005-01`。本包是**未完成的工程封存**：既有helper25/25、transaction32/32、control39/39各一次通過；新增diagnostic20個case只有18通過。D19/D20的測試adapter使用混合斜線路徑，被既有精確scratch-root檢查拒絕。一次預驗額度已用完，不修測試後再跑，不進runner freeze或真控制。**Windows wrapper原始語法錯誤尚未定位或修好，BVI建置及冷suite全未執行。** 詳[交接](HOLD_OWNERSHIP_X10D_O_BVI_BUILD_HANDOFF_20261005.md)。

授權完整範圍保存在新batch `scope.md`。新source `research/x10d_o_bvi_build/`、batch `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-build/`；新out `out/x10d-o-bvi-build/`未建立。原control/resume/R2F/R2E的STOP、failed logs、source、receipts與out沒有續跑／覆寫／刪除。原control final SHA `28e653c844d44f0d80ef716fadbb7137a715a01be81de22bed809a953da8c2d8`、state SHA `9089d70d45978a13c83fe62d34b82fa39db08a861403aea2808160998be1f363`重核一致；舊三控制成功只作歷史證據，不填本包通過。

## 實作、預驗與失敗

- `owned.cs`、`identity.ps1`、`transaction.ps1`、`bvi.cpp`、`bvi.hpp`、`driver.inc`逐bytes保持control來源。`main.cpp`只改report reservation attempt字串，新綁定base/common及control腳本不改ownership/identity/transaction核心。原oracle、候選門檻、contact adapter及fixture未改。
- 既有三組各一次共96/96，假facts/native CreateProcess=0；cross-shell helper1＋transaction4拒絕成立。新增D01–D18涵蓋slot/revision/額度、禁止任意exe/argv/舊根、控制門檻、可信退出／cleanup、缺positive／pending／wrong-version拒絕。這18項是fake/helper契約檢查，不是程序平台驗收。
- 新增20-case oracle先落盤，一次執行18/20，沒有修改oracle或第二輪。`diagnostic-pretest.ps1:7`的 `$Evidence/scratch-diagnostic`及case子目錄以字串組合，實際傳給NewTransaction為 `C:\...\hold-ownership-x10d-o-bvi-build/scratch-diagnostic/D19`。不變的`transaction.ps1:12`要求字面prefix `Join-Path $Evidence 'scratch-'`＝`C:\...\hold-ownership-x10d-o-bvi-build\scratch-`；OrdinalIgnoreCase StartsWith=false。D19/D20都在NewTransaction丟`transaction-root`，尚未BeginTransaction、reserved command、假exited結果／完整拒絕或positive交易。兩份scratch state及失敗report均保留，不能將18項局部通過升格診斷state整體通過。
- 初次STOP維護保存另發生作者錯誤：INIT_PENDING state缺`running`屬性，PSCustomObject賦值失敗，DurableState未被呼叫。原stop source與exit1原因保存於`failures/stop-first.ps1`及`stop-save-failure.json`。唯一維護更正只補nullable欄位以保存STOP；不改runner/pretest／候選、不重跑任何case或native。其後durable STOP成功，revision1、known launch0、consumed0；新shell對3controls＋4probe＋10product全部17入口由共用CheckLaunchState拒絕。這是維護保存修復，不是診斷預驗repair／第二輪。

## 凍結、wrapper與產品分母

第一runner freeze、runner-contract、第二product freeze、product-contract均未生成。`run.ps1`仍有未seal的contract placeholder，沒有正式launch資格。新增diagnostic/wrapper/run源是未驗完整交易的草稿，不能直接續用為已驗執行核心。

wrapper-v0保留7份新根ASCII／UTF8-no-BOM、CRLF、NUL=0的marked batch及source/SHA metadata；是未執行草稿。`cmd.exe /c`的shell語法與native CRT argv quoting分層設計，逐步marker、`%cmdcmdline%`與純PowerShell argv-fixture已有草稿，但**未取得任何實際cmd命令列／offending step／vcvars或target入口證據**。沒有wrapper作者修正，probe0/4，既有configure語法錯誤仍unknown；不能因草稿把路徑斜線當已證根因。

| 分母 | 本包實際 | 結果 |
|---|---:|---|
| helper／transaction／control預驗 | 25／32／39，各一次 | 全通過；native0 |
| 新diagnostic預驗 | 20，一次 | 18pass／2failed，完整交易未通過 |
| 真natural／nonzero／owned-child | 0/3 | 未啟動；native/runner/verifier均null |
| 真wrapper probe／repair | 0/4、0/2 | 未啟動／未修正 |
| Release四命令 | 0/4 | configure/build/wrong-contact/full suite未執行 |
| Debug三命令／ASan三命令 | 0/3、0/3 | 未啟動，不倒填skip或可用性通過 |
| BVI layer-cases／supplemental／schema controls | 0/356、0/22、0/4 | 全未驗；sizeof/metadata/probes=null |

原fixture89×4、1128coverage rows/2392 refs及contact adapter按舊contract/input SHA引用。Wrong-contact真native1、兩failed rows、aggregate及正式consumer拒絕未執行。本包沒有C++ candidate binary或compile/link dependency closure。Add-Type僅在既有helper/control預驗編譯managed owned interop；不冒稱BVI build。PowerShell host及完整managed/Roslyn/system loaded DLL closure未全面採集；原依賴manifest只提供pin/existence參考，不當實際compiled/loaded證據。

## 保護、容量與交付

原Protection重建3100，再核control ledger全部shards/files/final及freeze/input/dependencies，去重為**3609舊檔／4010引用、0不符**；再加本次scope一項，final保護3610 unique／4011 refs。原594 Git paths及dirty、HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、index `260c49037641b2c8c44f40e828491778779223193d91cb7cd824c8534ef7e9e1`保持。沒有執行舊settle/check回寫。

carry aggregate8285901212、development8988255、controller1299900／remaining7088708保留、oldout374705；new development上限40249475、newout134217728/sharedout268435456、aggregate8589934592、free每phase至少5704253440。新source、scope/docs、全部scratch/失敗/維護保存更正/manifest/protection shards/ledger及self bytes均計入；newout0、PNG副本0、刪除0、reserve correction0。精確settled容量及所有SHA以新batch `final-receipt.json`、`artifact-ledger.json`、`protected-index.json`及其shards為準。每維護報告64KiB／logs總2MiB界限核對，未用分片漏算容量。

geometry-interface-review只讀引用SHA `e80ab11234bd8ed8d4d64f0e6354e4dca1dec6f3e4aa0e7d7e801d53207effb0`；PNG0/2。合法當前ROI/all-lines adapter、physical owner、成本/runtime/live仍缺，未開圖或跑provenance-gap driver。C36h tint1 baseline、main50 donor/live0、suppression OFF、P excluded、X12 not-ready及Chapter Legacy完整IN Miss=0目標保持，無emulator/ADB/touch/manual-session/full replay/models/goal/automation/commit/push或另派task。完成工程封存後交總控獨立驗收，不簽BVI cold或產品通過。
