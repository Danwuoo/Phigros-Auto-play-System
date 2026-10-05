# BVI frozen suite：已完整跑到斷言，冷契約仍失敗

2026-10-05；研究基準 `ebe955a6286ca1bb767b631948714a5545b7c174`。本輪使用新雲端身份 `cloud-bvi-frozen-20261005-round2`，沒有重開歷史 STOP，也没有修改正式程式、既有研究 source、原 oracle 或六個執行輸入。

## 1. 更新判定

**先前未跑 → 本次已實際跑完四層，但不是通過。**

- 首輪 [BUILD_TEST_AUDIT](../BUILD_TEST_AUDIT.md) 只有新 28-assertion core smoke；完整 JSON harness 因缺 header／frozen inputs 未跑。
- 本次先保留原始讀取邏輯，僅換新輸出 reservation：成功編譯，wrong-contact 負例正確返回 native 1；完整 suite 返回 **2／JSON type_error.305**，不是算法斷言失敗。原因是新 R1 typed fixture 的 `lines` singleton object 與原 reader 的 array 假設不相容。
- 在另一隔離 adapter 中，只讓已核對的 typed singleton object 被讀為一條 `Line`；**不更動 JSON bytes、解析後的 JSON、expected、core 或 driver**。Linux GCC14 O2、O0＋libstdc++ assertions、ASan＋UBSan 都完整跑到 **89×4＝356 layer-cases、22 supplemental、4 schema negatives**。
- 三配置均得到 **3,938 assertions、54 failed、native exit 1**。325/356 layer-cases 沒有失敗斷言，31/356 有失敗；所有新增20例的四層均無失敗，54項全在原69例。**這54項不是54個獨立 action bug**，分類見 §5。
- 22 supplemental、4 schema negatives、3 renderer equality controls、4 contact-adapter controls 全過；另行 wrong-contact 真執行保留兩條 expected=4／actual=1 的 failed rows，native 1，原 consumer 與新外部稽核均拒絕。
- 原 coverage 1,128 rows／2,392 layer references 全有記錄，`schema_errors=[]`、`unverified_oracle_fields=[]`。這只證明宣告的欄位被斷言，**不等於每個測試真的注入它命名的失效條件**。

新判定為：**完整 Linux 冷測已可達、原 reader／fixture 介面有阻塞、隔離 shape adapter 後冷契約實測失敗，candidate 不可採納。** 不再把它描述為「只缺建置、候選完全未跑」，也不能說「未修改的原 harness 通過」。Windows build、原程序控制、owner接入、真像素、成本與實機依然未驗。

主要結果： [完整 Release 報告](evidence/bvi-frozen-suite/typed-singleton-release-verified/suite.json)、[54項失敗及分層摘要](evidence/bvi-frozen-suite/typed-singleton-release-verified/summary.json)、[wrong-contact 真負例](evidence/bvi-frozen-suite/typed-singleton-release-verified/wrong-contact.json)。

## 2. Immutable inputs、來源與 JSON 依賴

上傳最小包 ZIP SHA-256 為 `d0a2eb1bebd35ff90c27493fe4ed914970bb24d22137c45af424574d2563db59`。README／manifest 清楚區分 historical original SHA 與 derived bytes。本 suite 使用的五個包內輸入未去識別改寫，與 original／derived digest 同時相符；第六個 supplemental 直接取既有 Git。

| CLI input | bytes／SHA-256 |
|---|---|
| normalized-execution.json | 326978／`96d536c5b5e9e01e359da2ead01596428b46f4265f28ba295ee9d9b4297650c5` |
| oracle.json | 16105／`90f8680c4362d07a718ac2bb7142b37396871b7ffd81523a06ddd8a685ec5425` |
| typed-r1.json | 419699／`ce69f19d5c488e780c7d1cb7785601d3e3f80750b134440c636dda20f19ff8c3` |
| supplemental.json | Git輸入／`45ab5f3d97c3513fb6836211801922a9d86863de273739e35b5e4263b7a6c0a1` |
| r1-cases.json | 469436／`8da656609b424eda46633145728dd485d12bfb25e83f1850235a527891e28f46` |
| expected-coverage.json | 224663／`4985ec8aac5d89f0863125daa6be483b26c1b200b00f5bbcb23fc691cc517f2a` |

每個 run 前驗固定 SHA，後再核 bytes 沒變。沒有複製原輸入進 Git。source 與 control freeze 的對照見 [source-provenance.json](evidence/bvi-frozen-suite/source-provenance.json)：

- `bvi.cpp`：`f45c079548c9e0e78472f31c1613f0b2aaddede0ca6e383ae663f31cfe91fdce`
- `bvi.hpp`：`6f6313ef23ae473404d07f22f3438abc25364d65bce9f08fa35eaad5872eff0f`
- `driver.inc`：`ba0328c622bcd44974e4d7e66414546d1e8de71cfd28f379d796096f2e67f0b3`

以上三個最新 build 檔均與包內 control freeze 相符。control `main.cpp` SHA `700912cf…cfa91` 與其 freeze 相符；最新 build `main.cpp` SHA `fc3199dc…a407` 只將歷史 reservation 從 attempt03 換為 build01。因此編譯最新 build core/driver，不誤用較舊 R1 driver。

**依賴採官方 nlohmann-json 3.12.0，沒有替換 JSON 實作。** repo 第三方 notice 及 vcpkg baseline 均指定 3.12.0；baseline `93c50752b23e350ca6b9063a167f0a4cf8a3b3eb` 的 port-version 2 包含 char8_t、optional 兩個 upstream patches。本輪從已連接 GitHub 只讀取得官方單 header，核 tag 與 pinned commit 內容相同，再以 `patch --fuzz=0` 套用兩個官方 patch 中未修改的 single-header hunks。

- [官方 pinned source](https://github.com/nlohmann/json/blob/55f93686c01528224f448c19128836e7df245f72/single_include/nlohmann/json.hpp)，MIT；upstream 953436 bytes，SHA `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63`。
- [vcpkg portfile](https://github.com/microsoft/vcpkg/blob/93c50752b23e350ca6b9063a167f0a4cf8a3b3eb/ports/nlohmann-json/portfile.cmake) 所列 patches 完整来源／摘要見 [dependency.json](evidence/bvi-frozen-suite/dependency.json)。有效 header 954094 bytes，SHA `f2e1f3cac4e48fa851d6986291e4bd5b384d87f92c175d8b126cbd91580289cc`。
- 歷史 Windows 是 multiple-header layout，本次是相同版本／port patches 的 amalgamated layout；**不聲稱與原 Windows 安裝樹逐檔相同**。全部只在 `/tmp` 隔離 include root，沒有系統安裝、CMake安裝或把整包header／binary放進repo。MIT原文沿用 repo 已有的第三方授權。

## 3. 原始 reader 失敗與最小 shape adapter

原始失敗：[O2 stdout／stderr／native exit](evidence/bvi-frozen-suite/release/run.log)；開啟官方 `JSON_DIAGNOSTICS=1` 的第二次冷診斷精確報：

`[json.exception.type_error.305] (/69/typed_frames/0/lines/angle) cannot use operator[] with a string argument with number`

原 `main.cpp:17` 對 `f["lines"]` 做 range-for。當 `lines` 是 object，nlohmann 迭代的是其 value；取到 `angle=0` 後又用 `l["center"]`，所以拋 exception。這是 reader／fixture schema 不相容，原 native suite report 仍只是新 reservation，不冒充完整結果。

已核：

- 原69例的217 typed frames，`lines` 全為 array。
- 新20例的159 typed frames，156處為一個具有 `center/id/angle/length` 的 object，另3處為 array。
- **156/156 object 字段、型別、全部值與對應RGB幀唯一一條line精確相同**；另3 array也一致。每一處 JSON pointer、case、對應 RGB pointer 保存於 [singleton-input-audit.json](evidence/bvi-frozen-suite/typed-singleton-release-verified/singleton-input-audit.json)。
- `author-fixtures.ps1:6,34` 的 `Clone` pipeline／`lines=(Clone $f.lines)` 是 singleton 序列被展開的 source-level 線索；未在這個 Linux 環境重跑 PowerShell，故不把它寫成已重現的 Windows authoring 根因。

adapter 原始碼差異：[identity-only diff](evidence/bvi-frozen-suite/release/adapter.diff)、[identity＋typed-singleton diff](evidence/bvi-frozen-suite/typed-singleton-release-verified/adapter.diff)。只有：

1. `save()` 接受本輪的新 attempt 字串，仍要求 pending=true、4MiB report bound；沒有寫入任何歷史 attempt 輸出。
2. `lines()` 對有 descriptors、四個正確鍵的 singleton object 讀入一条Line；原 array 路徑不變。全輸入SHA鎖定及先行 C++ audit 將此限定在上述新R1 typed frames。**沒有包裝／重写fixture、放寬core閾值、改期望、忽略failed row。**

identity-only main SHA `86d71616ddb1bfc74e0e00531337ce7feeb352f24205b46f0977d3a1cf11c79c`；shape adapter main SHA `5383ffb0d5a821df71f8a14c063a0ce4c29b2b67dc9eebbcf4128c79492b2e61`。兩個 main 都只生成在臨時目錄，core/driver從原repo直接編譯／include。

## 4. 實際結果與負控制

環境：Linux x86_64、GCC `14.2.0-19`、C++20。harness compile限120秒，C++外部audit compile限60秒，各 native test限60秒；未執行CMake、Windows shell、裝置、遊戲、RPC、owner或模型。

| 模式 | 編譯 | wrong-contact | 完整suite | report／限制 |
|---|---|---|---|---|
| identity-only O2 | exit0 | native1，兩條failed rows正確 | native2，type305 | 原失敗保留 |
| identity-only O0＋JSON diagnostics | exit0 | 同上 | native2，定位至typed lines/angle | 沒有修改語義來繼續 |
| shape adapter O2 | exit0 | native1，外部consumer核拒絕 | native1，3938／54failed | 最終runner另重跑，aggregate1 |
| shape adapter O0＋libstdc++ assertions | exit0 | 同上 | native1，3938／54failed | aggregate1 |
| shape adapter ASan＋UBSan | exit0 | 同上 | native1，3938／54failed | stderr0；LSan關閉，不是leak-free |

三配置完整 suite report 均 **3,006,856 bytes、SHA `8fbc37e8db4afe1d8736b2f89c17f25dc91d2213581d241279c6ab2dbda38611`**，逐byte相同。O2最終runner重跑也相同。sanitizer沒有發現新增診斷，不會把 frozen assertions 的54fail變成pass。

最終O2報告保留完整JSON；初次shape O2、Debug、sanitizer的相同全報告以各run的 `suite.json.gz` 無損保留，解壓後逐byte比對才移除冗餘明文，對應 `suite-decoded.sha256` 記原digest。全部failed rows、native exits、streams及原始reader失敗均保留。

| 層 | cases／有斷言 | assertions | failed assertions | failed layer-cases |
|---|---:|---:|---:|---:|
| rgb | 89／89 | 490 | 13 | 7 |
| typed | 89／89 | 589 | 9 | 9 |
| lifecycle | 89／89 | 1331 | 1 | 1 |
| e2e | 89／89 | 1495 | 31 | 14 |
| 四層合計 | 356／356 | 3905 | 54 | 31 |
| supplemental | 22 | 22 | 0 | 0 |
| schema negatives | 4 | 4 | 0 | 0 |
| renderer controls | 3 | 3 | 0 | 0 |
| contact-adapter controls | 4 | 4 | 0 | 0 |

wrong-contact另行執行，不塞進正 suite pass總數。原 driver 在 output-to-assertion 邊界將 contact 4改1，保留 `contact_id_transfer@0`、`renaming_output_contact` 兩條fail；原 `rejects_wrong_contact()` 及 [audit_io.cpp](evidence/bvi-frozen-suite/audit_io.cpp) 均核實際native exit與完整row，不接受只寫布林pass。schema typo／未知metadata／空coverage／wrong-contact 4/4正確拒絕。

注意最初 collector shell 以最後的 `date` 結束而回0，**從未把此0當測試通過**；當時各native exit已獨立保存。此收集層缺陷已修正，新runner在外部結果驗證後回aggregate1或2。最初三個run保留原log，追加清楚標記的 post-audit；完整沿革見 [collector-protocol-history.json](evidence/bvi-frozen-suite/collector-protocol-history.json)。

## 5. 54項失敗分類，不能簡化成54個算法bug

完整逐row expected／actual均在摘要與原suite中。本分類不刪fail、不調整expected，也不對未做的修補宣稱改善；獨立語義覆核應與此分開閱讀。

### 5.1 34項是harness／測試注入／診斷表示問題

| 類別 | 數量 | 可核機制與正確解讀 |
|---|---:|---|
| V16四種overflow在RGB/e2e只宣告usage，未注入真超限 | 24 | frozen frame仍只有一query／一line；`driver.inc:30` 只傳實際vectors，沒有把 `declared_usage` 傳入extract。故這24項不能證明真129ROI／17線仍被接受，是該層測試覆蓋缺口。supplemental的真API129query／17line負例另有通過。metadata/probes宣告也不能當實測達上限。 |
| V16 typed invalid只設flag，沒清body支持 | 8 | `main.cpp:39–42` 設 `o.invalid=true`，保留先前建的 supported body；`current_body_preserved` handler只看body。八例的qualification invalid、Move/refresh拒絕及release安全行為已符合，差在typed adapter輸出，不是八次危險續Hold。 |
| V18-V08 line-order在歧義時保留最後hit | 2 | typed reader遇兩條候選會反覆覆寫hit；line-order交換令y從499.5197438360538變500。normalized comparison移除line_id卻保留hit，typed/lifecycle各1fail。兩例仍line_unique=false、Move/refresh=false；屬診斷等價性契約問題。 |

### 5.2 20項是原RGB／端部／關聯／constraint鏈對frozen expected的實際不符

| 案例／機制群 | failed assertions | 本次實際觀察 |
|---|---:|---|
| V01-whole／touching＋V07-merge | 5 | 沒有宣告rails的RGB，extractor仍輸出left/right/front_end=true。`bvi.cpp:34–35` 將任何掃到的white計入rail，判定線可充當white rail；這使V01兩例Move=true、V07新front壓掉merge ambiguity（alternatives1、continued、Move=true）。這是冷反例，沒有真機因果結論。 |
| V02-continuation／stationary-current＋V17 gap/duplicate | 6 | front endpoint附近的判定線使blue/black採樣不符；continuation cap=false、stationary independent2而非3，V17三例endpoint從未eligible，不能將這些結果僅歸咎signature去重或40ms窗口。 |
| V04-contained-effect | 2 | RGB/e2e effect=false，frozen expected=true。透明黃色覆藍的內部RGB為(115,202,194)，不滿 `r>=180 && g>=120 && b<=180`；因此原effect predicate與凍結例不符。typed/effect本來不適用，不補稱已驗。 |
| V10-rotation | 7 | RGB/e2e contact=absent、hit/body/error不合，e2e Move=false。body支持仍在，rotated line的採樣支持未取得；屬旋轉接觸契約實際失敗，不是JSON singleton問題。 |

合計34＋20＝54。上表是按失敗row分組，同一機制可引發多層／多欄fail，不是獨立bug數。這20項也不能推論到C36h的77 Miss各自因果。

輔助 [case-diagnostics.json](evidence/bvi-frozen-suite/case-diagnostics.json) 由新 [diagnose_cases.cpp](evidence/bvi-frozen-suite/diagnose_cases.cpp) include同一shape adapter、原driver，呼叫未改的 `execute()` 取得原69例的完整trajectory／最後descriptor；沒有修改fixture或expected。它只說明上述實際失敗，不能取代原aggregate。

## 6. 可重跑方法與保護

[run_frozen_suite.sh](evidence/bvi-frozen-suite/run_frozen_suite.sh) 要求全新output子目錄，鎖六輸入、core/driver/main及dependency的SHA；typed-singleton模式先跑156處C++形狀／RGB對照核驗，再建立臨時adapter，保存完整diff、source/binary hash、reservation、native exit、streams、結果與aggregate exit。已有輸出拒絕覆寫。

```sh
E=docs/research/zero-miss-20261005/round2/evidence/bvi-frozen-suite
# 先從§2的官方pinned URL取得原header，保存於/tmp，不放進repo。
bash "$E/prepare_json_dependency.sh" /tmp/upstream-json.hpp /tmp/fresh-json-include
bash "$E/run_frozen_suite.sh" "$PWD" \
  /workspace/shared/phigros-evidence-round2/unpacked \
  /tmp/fresh-json-include "$PWD/$E/my-new-release" release typed-singleton
# 本次相同source/input的預期觀察為exit1及54failed，絕不是預期通過。
```

第五參數另支持 `debug`、`sanitizers`；第六省略則保留原reader，應重現type305而非完整報告。依賴重建與shell syntax已有 [核對紀錄](evidence/bvi-frozen-suite/dependency-preparation-tests.log)。依賴準備只套官方patch、核SHA，沒有安裝或網路副作用。

所有run的candidate metadata=185904 bytes、sizeof與probes記錄只屬此Linux ABI／合成輸入；probes_max=555648不是runtime成本或所有輸入最壞值。原 `bvi.cpp:97` 及 `driver.inc:81` 的misleading-indentation warnings保存，未順手改source。

## 7. 下一步與仍未驗

1. 保留原reader type305與shape adapter分層結論；若日後正式修fixture authoring或reader，另立版本和schema，不能回填原frozen輸入。
2. 優先隔離V01/V07 white-line誤rail、endpoint/line遮蔽、V04 alpha effect及V10旋轉採樣，維持frozen expected先做反例診斷；本輪沒有候選修补。
3. 修正測試注入與typed invalid／ambiguous diagnostics契約時，與候選改動分開；true capacity API負例與declared-usage測試不可互代。
4. Windows native launcher／wrapper、MSVC Release/Debug/ASan、source-age、physical ownership、真ROI／PNG、owner安全、runtime/cost、遊戲回饋、全曲解鎖及IN Miss=0仍另有gate。本輪不觸碰這些執行範圍。

**結論：BVI建置缺口已部分解除，完整冷測帶回的是有用的負結果。既有28/28小probe不能蓋掉這54項fail，這些合成結果也不能宣稱真機改善或zero miss。**
