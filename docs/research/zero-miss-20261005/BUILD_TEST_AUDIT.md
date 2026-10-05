# BVI 建置與測試稽核：已能冷編核心，正式候選驗收仍未解除

日期：2026-10-05。研究基準：`74e54437d4a3ad2b2bd1a3b09312211a92f2359e`。本輪只新增本報告與同目錄下 `evidence/build-audit/` 的獨立研究證據；未改正式 source、歷史研究 source、原測試、oracle、分支或 commit。

## 1. 結論

1. **「BVI 完全無法編譯」不再是本輪全部證據的正確描述。** 未改動的最新 `bvi.cpp/bvi.hpp` 已以 Linux GCC 14.2、C++20 編譯；新小型 probe 的 **28/28 assertions 通過**。同一 probe 的 ASan＋UBSan 在關閉 LeakSanitizer 後也是 28/28，沒有 sanitizer 診斷。這是新 Linux 核心 smoke evidence，並未改寫歷史 Windows attempt。
2. **完整 BVI 冷驗收仍未通過。** 原 JSON harness 本輪編譯在缺 `nlohmann/json.hpp` 處停止；現存 CMake 缺 portable 設定，雲端亦無 CMake。原 frozen case inputs 不在 Git。89×4＝356 layer-cases、22 supplemental、4 schema controls、wrong-contact 真 consumer、Windows Debug／ASan 均未跑。
3. **D19/D20 的確切 source-level 阻塞仍存在**：adapter 用字串 `"$Evidence/scratch-diagnostic"`／`"$scratch/$id"`，guard 比對 Windows `Join-Path` 產生的字面前綴。它與舊 Windows configure syntax failure 是兩個不同問題。後者仍缺 offending step 的實際 trace，不能先宣稱斜線就是原因。
4. **保留成功的 process-control 核心即可，不需重寫。** `owned.cs`、`identity.ps1`、`transaction.ps1` 與已成功 control-03 版本在此 checkout SHA 完全一致。本輪未執行 Windows control；歷史三真控制的成功與新 wrapper 未驗必須分列。
5. **最短路徑是取得小型 frozen inputs＋修正有界建置入口，先到達既有候選 assertions。** 不需要為此上傳整包 `out/`、vcpkg、SDK、模型或所有錄影，也不需啟動模擬器。Linux smoke 通過不提供 Windows、ownership、runtime、真機或 zero-miss 資格。

## 2. 範圍、讀取與實測環境

已讀 `AGENTS.md`、README、架構、路線圖、跨曲研究與最新狀態入口；checkout 沒有 `.agents/skills/` 可讀。採用新研究授權，只執行安全、非裝置的 C++ 編譯及合成 API probe。歷史文件的暫停／一次額度仍保留，不執行或重啟任何旧 attempt runner。

- 實測平台：Linux 6.18.44 x86_64，`g++ (Debian 14.2.0-19) 14.2.0`。
- `cmake`、`ninja`、`clang++`、`pwsh` 不在此次 PATH；未安裝、下載或升級。
- `/usr/include/nlohmann/json.hpp`、checkout 的 `out/`、`measurements/` 不存在。
- 編譯每個命令上限 60 秒；單次 probe 上限 30 秒；無網路、遊戲、裝置或觸控工作。
- 編譯產物置於 `/tmp/phigros-bvi-*`，研究文字／source／log 留在 `evidence/build-audit/`，沒有寫入既有 out 或 measurements。
- 後核 `git diff --exit-code -- src include tests fixtures research CMakeLists.txt CMakePresets.json` 為 0；此報告及其他平行研究為新未追蹤文件，不能把工作樹整體誤稱 clean。

環境、hash 與執行原文：[`environment-and-hashes.txt`](evidence/build-audit/environment-and-hashes.txt)、[`core-compile.log`](evidence/build-audit/core-compile.log)、[`original-harness-blockers.log`](evidence/build-audit/original-harness-blockers.log)。

## 3. Current evidence matrix

「歷史報告」意指本輪讀到 Git 中的一手作者結果文件；原始 ignored receipts 未取得，因此沒有聲稱本輪重算其全部 hashes。

| 層次 | 本輪可核證據 | 通過／失敗／未跑 | 不能推論 |
|---|---|---|---|
| 成功程序控制 | control-03 文件記 natural 0/0/0、nonzero 7/7/0、owned-child 0/125/0；本輪核共享核心 SHA 相等 | 歷史三真控制通過；本輪 Windows 未跑 | 新 wrapper、路徑綁定或 Windows 候選通過 |
| 最新三組前測 | BUILD 結果記 helper25、transaction32、control39，共96/96 | 歷史 fake/helper 通過；本輪未跑 | native CreateProcess／候選成功 |
| 最新 diagnostic | D19/D20 字串 mixed-slash 與 guard 原文仍在 | 歷史18/20，source-level 原因重核；PowerShell 未跑 | 20項整體或完整診斷交易通過 |
| Windows configure | control-03 記 cmd stderr41B、stdout0、exit1；無 offending line trace | 歷史失敗；本輪未重現 | CMake 或算法本體已失敗，或已找出 syntax 根因 |
| BVI core translation unit | GCC 原封編譯 `research/x10d_o_bvi_build/bvi.cpp` | **本輪通過**；1項 misleading-indentation warning | 完整 harness 可編譯、Windows ABI／效能通過 |
| 新 C++20 小 probe | 直接連結上述原封 core；28個小 assertions | **本輪28/28** | 原356 layer-cases、原oracle或真圖測試通過 |
| GCC ASan＋UBSan＋LSan | 編譯成功，首次執行回 `LeakSanitizer ... fatal error ... does not work under ptrace` | **執行失敗，環境限制**；失敗保留 | leak-free 或 sanitizer 全部通過 |
| GCC ASan＋UBSan，LSan關閉 | 相同 binary、相同 probe，僅 `detect_leaks=0` | **本輪28/28、exit0** | LSan、MSVC ASan、未覆蓋輸入無問題 |
| 原 JSON harness | `main.cpp:2` include header 不存在；fixtures未提供 | **編譯阻塞**；full suite未跑 | 原測試通過或失敗 |
| 原 CMake | 命令返回127，CMake不在環境；source另有 Windows-specific flags | **configure未啟動** | Linux CMake configure pass／候選 compile fail |
| root正式程式／349回歸／CPU8項 | Windows dependencies、原回歸仍是歷史文件證據 | **本輪未跑** | main50 live-ready、現在仍349／8項通過 |
| 真圖ROI／physical ownership／owner接入／成本／IN | 本次probe沒有這些輸入或真後端 | **未跑** | Miss改善、X12 ready、完整IN zero miss |

歷史引用：`docs/catalog/03-ownership-bvi/HOLD_OWNERSHIP_X10D_O_BVI_R2F_CONTROL_RESULT_20261005.md:14–26,36–38`；`.../HOLD_OWNERSHIP_X10D_O_BVI_BUILD_RESULT_20261005.md:3,9–18,20–30`。較舊 README 中 `348 pass／1 skip` 屬歷史整合，不能和本輪 28 assertions 合計。

## 4. Source-level 阻塞與最小解除方案

### 4.1 D19/D20：修 adapter 的 canonical 路徑，不放寬 guard

- `research/x10d_o_bvi_build/diagnostic-pretest.ps1:7,25–29` 使用混合分隔符組成 scratch/case 目錄。
- `research/x10d_o_bvi_build/transaction.ps1:11–14` 在 `NoAlias` 前，以 `StartsWith(Join-Path $Evidence 'scratch-', OrdinalIgnoreCase)` 檢查字面前綴；它沒有先 canonicalize `$dir`。
- 最新作者報告確認 D19/D20 停在 `NewTransaction`，未到 `BeginTransaction`；這是 adapter 輸入錯誤被 guard 拒絕，不是已完成的 transaction 失敗。

**建議的最小修補（本輪未套用）：** 新授權的測試入口使用 `Join-Path` 組合 scratch 與 case path，並按既有 exact-root 規則核其絕對路徑；不把 guard 改成「任何同名子字串都允許」。保留 sibling、`..`、reparse、舊根拒絕。先驗完整 D19 拒絕交易及 D20 positive 交易，再跑20項必要回歸；原 failed inputs、logs、STOP 原樣保留。此處仍是 Windows PowerShell 語義，Linux 字串 smoke 不能替它簽收。

### 4.2 INIT state：初始化 schema 本身仍不完整

`research/x10d_o_bvi_build/initialize.ps1:7` 沒有 `running`、`reason`、`diagnostic_positive` 等後續會使用的欄位；前測 `diagnostic-pretest.ps1:9` 的 fake state 則有。`run.ps1:39` 與交易程式會更新 state。歷史首次 STOP 的 `running` 缺欄錯誤已見 BUILD 結果第12行，當時只修維護保存而沒有回補 initializer。

最小下一步是新入口共享完整初始 state schema，冷測 INIT／RUNNING／STOP 的保存與讀回。不要以大量獨立 boolean pass 取代真實檔案交易。是否所有欄都必要，須依新入口走訪過的狀態明確列出，不宣稱本輪已驗。

### 4.3 Windows syntax：根因仍 unknown，取逐步 trace 即可

原 wrapper 是 `research/x10d_o_bvi_r2f_control/configure-release.cmd:5–8`：先 `call vcvars64.bat`，再呼叫 CMake。原 runner `owned.cs:99,106–108` 用 CRT quoting 建立 `CreateProcess` 字串；`cmd /d /s /c` 還有另一層 shell 語義。

新草稿已提供 `wrappers-v0/configure-release.cmd:2,6–14` 的 entry／vcvars／CMake marker，以及 `run.ps1:26–30` 的 `commandline_to_CreateProcess`。`wrappers.ps1:19` 把 executable 路徑正斜線轉成反斜線是**待驗假說**，不是確定修復。

最小恢復實驗：沿成功的 owned/identity/transaction 核心，先用新的有界允許清單執行純 wrapper argv fixture，取得實際 CreateProcess 字串、`%cmdcmdline%`、階段 marker、完整 exit／cleanup／stdout／stderr；看到具體失敗步驟後只修該步。若連 entry marker 都沒有，先查 shell invocation；若 vcvars結束而CMake未進，查下一命令。不要重新寫 process owner、猜測解除 quoting，或用編譯成功替代程序收尾。

### 4.4 現有「standalone」不等於 portable／CTest-ready

最新 `research/x10d_o_bvi_build/CMakeLists.txt:7–14`：

- JSON headers 硬綁 `../../out/vcpkg_installed/x64-windows/include`；沒有 `find_package(nlohmann_json)` 或可配置的 portable include。
- `/W4 /permissive- /utf-8 /Zi /showIncludes`、`/DEBUG /INCREMENTAL:NO` 無 `if(MSVC)`，非MSVC也會收到。
- ASan會 copy固定 Windows MSVC14.50 DLL，Linux不能照搬。
- 最新檔沒有 `enable_testing()`／`add_test()`。R1只有 `enable_testing` 並註明要 runner供 frozen arguments（`research/x10d_o_bvi_r1/CMakeLists.txt:16–17`）。未註冊案例時不能把 `ctest` 無測試視為測試全綠。

**最小工程方案：** 若要正式支持 portable冷測，新增隔離研究 target，或後續經授權把 compiler flags分支化、JSON dependency可配置化；仍直接編譯同一 `bvi.cpp`，不要 fork算法。Linux與Windows各自產生 binary/hash/result。CTest若接入，必須明確註冊 exact frozen inputs與fresh output reservation；不能用一個空CTest補齊原356案例。

根專案本來是 Windows產品：`CMakeLists.txt:21–27` 的固定依賴及 `:48–51` 的Windows libraries，`CMakePresets.json:8–20` 的VS/v145/local paths，均不是此雲端研究應順手移植的範圍。

### 4.5 歷史runner的綁定不適用於新checkout

`research/x10d_o_bvi_build/base.ps1:2–7` 固定本機絕對根與旧 attempt；`:85–98` 檢查歷史 HEAD/index/git-path counts，`common.ps1:18–24` 另固定594 paths。`seal.ps1:29` 還引用文件搬移前的 `docs/HOLD_...`。新 checkout已是74e54437及分類後 docs，不能直接執行這套封存腳本。

這不是理由去修改舊收據、繞過來源保護或reopen STOP。新入口只重新綁當前明確範圍及必要 immutable inputs，引用既有成功核心與歷史保留記錄。不要為適配新Git樹重抄整個恢復平台或要求全部舊out。

## 5. 本輪小 probe 的意義與限制

完整source：[`bvi_core_probe.cpp`](evidence/build-audit/bvi_core_probe.cpp)。28項分別涵蓋：

- capacity含邊界／超出、129 queries失效、frame byte-count／source／nonfinite拒絕。
- 64×64 RGB合成 Hold 的body、雙rail、front/rear endpoint、同線contact；兩條支持線不唯一。
- 三個獨立樣本跨度30ms確認、相同signature不湊confirmation、最多六份樣本先淘汰、40ms與40ms+1 gap。
- source age100ms−1／100ms、context改變、非遞增key。
- qualified新Down、無free contact拒絕、活動body同contact Move/refresh、不要求重新three-sample確認。
- unknown Down、completed、gate／plan deadline等號、矛盾receipt、missing60ms−1／60ms。

實測 `sizeof(Candidate)=154112`、`sizeof(Observation)=31792`、`sizeof(Descriptor)=200`、metadata總185904 bytes，這些只屬此GCC ABI。單一合成RGB輸入probe count為4307，不是全場景最壞值或runtime成本。核心硬界限見 `bvi.cpp:21,25–29,38,55,60–85,87–99`。

此probe是在閱讀現有source／契約後新撰寫的研究smoke，並非獨立人類gold或既有凍結oracle。typed measurements明確手建，只隔離關聯與fake lifecycle。`bvi.hpp:30–33` 明列 constraints沒有scheduler／owner／actuator；`Guard.attachment_query` 是fake接入條件，通過不代表觀測可以判定物理owner。

完整logs：[`probe-release.log`](evidence/build-audit/probe-release.log)、[`probe-sanitizers.log`](evidence/build-audit/probe-sanitizers.log)、[`probe-asan-ubsan-no-lsan.log`](evidence/build-audit/probe-asan-ubsan-no-lsan.log)。結構化28項結果及預期含義見 [`probe-results.json`](evidence/build-audit/probe-results.json)。首次LSan失敗與之後有明示限制的重跑分開，沒有覆寫失敗。

## 6. 可重現命令

所有命令從repo root執行；每份log已帶實際命令與exit。正式source保持原封。

```sh
timeout 60s g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic \
  -Iresearch/x10d_o_bvi_build -c research/x10d_o_bvi_build/bvi.cpp \
  -o /tmp/phigros-bvi-core-audit.o

timeout 60s g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic \
  -Iresearch/x10d_o_bvi_build research/x10d_o_bvi_build/bvi.cpp \
  docs/research/zero-miss-20261005/evidence/build-audit/bvi_core_probe.cpp \
  -o /tmp/phigros-bvi-core-probe
timeout 30s /tmp/phigros-bvi-core-probe

timeout 60s g++ -std=c++20 -O1 -g -fno-omit-frame-pointer \
  -fsanitize=address,undefined -Iresearch/x10d_o_bvi_build \
  research/x10d_o_bvi_build/bvi.cpp \
  docs/research/zero-miss-20261005/evidence/build-audit/bvi_core_probe.cpp \
  -o /tmp/phigros-bvi-core-probe-san
# 首次執行：因LSan／ptrace環境限制 exit1。
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  timeout 30s /tmp/phigros-bvi-core-probe-san
# 同binary的有界環境診斷重跑：exit0，無leak檢查。
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  timeout 30s /tmp/phigros-bvi-core-probe-san

# 兩個保留的阻塞檢查：各為exit1、exit127。
timeout 30s g++ -std=c++20 -fsyntax-only -Iresearch/x10d_o_bvi_build \
  research/x10d_o_bvi_build/main.cpp
cmake -S research/x10d_o_bvi_build -B /tmp/phigros-bvi-original-cmake
```

Hash：

| 項目 | SHA-256 |
|---|---|
| 未改 `bvi.cpp` | `f45c079548c9e0e78472f31c1613f0b2aaddede0ca6e383ae663f31cfe91fdce` |
| 未改 `bvi.hpp` | `6f6313ef23ae473404d07f22f3438abc25364d65bce9f08fa35eaad5872eff0f` |
| 未改 `driver.inc` | `ba0328c622bcd44974e4d7e66414546d1e8de71cfd28f379d796096f2e67f0b3` |
| GCC O2 probe binary | `c00113ec632b02568b90adbc1f653d13934e7abb2b3ffa02547c19021795cb83` |
| GCC ASan＋UBSan probe binary | `8bb6d9929e7934017a4a6accc24a4d1f43efb25c4b70157e29d4c91108f29699` |

Binary hash是本次編譯成品識別，不保證其他環境bitwise重現；尤其 `-g` 可能帶workspace路徑。研究probe及證據另列 `evidence/build-audit/evidence-sha256.txt`。

## 7. ignored 檔案的最小補件清單

`.gitignore:5–6` 忽略 `measurements/`、`out/`。本輪source盤點與小probe **不需等補件即可完成**。若要前進到原fixture重現／Windows根因，按目的小量提供即可。下列共同根稱為 `M = measurements/game-assist/2026-09-30-m0-manual-continue/`。

| 用途／優先 | 具體路徑 | 原因 |
|---|---|---|
| 原suite重現必需 | `M/hold-ownership-x10d-o-bvi/normalized-execution.json`、`oracle.json` | 原69案例及凍結expected |
| 原suite重現必需 | `M/hold-ownership-x10d-o-bvi-r1/typed-r1.json`、`r1-cases.json`、`expected-coverage.json` | typed69、新增20、handler／1128 coverage rows |
| fixture provenance | 同R1根 `specification-freeze.json`；最新build根 `input-manifest.json`，或control根 `contract.json` 的inputs entries | 按bytes/SHA核檔案；不重生假稱原frozen fixtures |
| Windows wrapper根因有助 | `M/hold-ownership-x10d-o-bvi-r2f-control/configure-release-{command,result,verification}.json`、`configure-release.stdout.log`、`configure-release.stderr.log`、`argv-readback.json`、`contract.json`、`freeze.json`、`state.json` | 保存已失敗原argv／41B stderr／退出和來源綁定；尚無step trace時仍不能證明具體行 |
| 最新D19/D20獨立重核有助 | `M/hold-ownership-x10d-o-bvi-build/diagnostic-{oracle,pretest}.json`、`failure-classification.json`、`state.json`、`final-receipt.json` | 重核18/20、native0、STOP而非只依作者報告 |
| compiler依賴版本識別 | control根或R1根 `dependency-manifest.json` 中nlohmann entries；非整包headers／DLL | 確認原JSON版本與hash，後續只準備必要依賴 |
| 不需再提供 | `research/x10d_o_bvi/supplemental.json`、C++／PowerShell／cmd source | 已在Git |
| 本分支不需要 | 整包`out/`、vcpkg／SDK／compiler安裝樹、cache、APK、模型、全部7722張PNG、帳密或token | 對建置根因與這28項probe不是最小輸入 |

原六個suite CLI inputs由 `research/x10d_o_bvi_r2f_control/prepare.ps1:19` 明列；`driver.inc:72–76` 驗證69／22／20分母。`main.cpp:47` 的report reservation綁舊attempt，因此新研究不能直接冒用舊reservation並改寫歷史輸出。

## 8. 建議下一個有界工作包與風險依賴

1. **補齊小型manifest與fixture。** 先SHA核對，列出缺／不符；不要重生成後稱原oracle。接著才準備必要JSON dependency及可用的CMake。這一階段不需Windows遊戲環境。
2. **最小入口修補。** 完整INIT schema、canonical scratch path、当前source/output/attempt綁定；保留已驗owned/identity/transaction核心與舊STOP。將「未launch的作者錯誤」與「已launch程序／來源／cleanup故障」分開訂有界處理規則。規則應先凍結，不能看到失敗再放寬oracle。
3. **Windows argv／wrapper診斷。** 只執行有界fixture/version probe，取得失敗步驟；有可信陽性後，再configure→build→wrong-contact負例→完整Release suite→可用Debug／ASan。每一層記精確source/binary/dependency／exit，不能沿用本Linux binary或舊Windows數值。
4. **候選本體判定。** 89案例四層、22 supplemental、4 schema controls完整通過才可以說原冷契約通過；任何fail保留根因及coverage，不刪測試、不改gold追pass。本輪28個smoke只提早揭露基本編譯及局部契約狀態。
5. **之後再接其他研究分支的gate。** 合法current ROI/all-lines真圖、physical ownership未知、完整owner安全與機會損失、exact候選runtime成本仍各自成立；全部不是本輪小probe覆蓋。完成build恢復也不授權live，更不代表全曲IN解鎖或Miss=0。

主要風險：原fixture缺失使完整oracle不可重現；Linux／MSVC ABI和浮點差異；新wrapper綁定未驗；fixture層與真圖current inputs不同；fake attachment不等於physical owner；成本仍not-ready。最不應新增的依賴是「先重寫整個process-control平台」或「先大量訓練／全量標註才能編譯BVI」。
