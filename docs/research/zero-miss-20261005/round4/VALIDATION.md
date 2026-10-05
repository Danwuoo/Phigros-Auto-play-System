# 最終驗證與重跑契約

## 1. 身份與範圍

- 原repo基準：`74e54437d4a3ad2b2bd1a3b09312211a92f2359e`
- 本階段起點：v2 `a53a9b7021bcc900f498047f5cc017b42e4711b0`；Goal紀錄commit `637611b92cc8e2ab00d034bb30405a8870f26da1`
- **最終候選source commit：`c2dda1db9fe95b46ff103300d43583857130cd8f`**。後续交付commit只記文件/證據/停點，candidate identity以此與SHA為準。
- Final core：[evidence/final-core.sha256](evidence/final-core.sha256)。`bvi.cpp` SHA866ad2440d088ec1caf360eb5d4a9c796fdfcbcf2e15aff92374baf2291ea824；`relation_policy.cpp` SHA37a23c696654b3935a51d422991f260213094a48d1488dc45e74334b29ac1efc。
- 源文件與測試在`research/bvi_cold_v3/`，四個translation units一起編譯，切勿混v2/v3 ABI。Descriptor256B、metadata236216B是Linux GCC ABI，不是Windows ABI宣稱。
- 正式`src/include/configs/tests/fixtures/tools/root CMake`及所有v2、歷史frozen source與inputs未改。舊三輪research SHA manifests核對全過。四個main translation units只做原封語法檢查；沒有正式owner連結/執行。

## 2. 依賴與输入

雲端GCC14.2.0/C++20，JSON include `/tmp/phigros-bvi-round2-deps`。`nlohmann/json.hpp`必核SHA256 `f2e1f3cac4e48fa851d6986291e4bd5b384d87f92c175d8b126cbd91580289cc`；為repo vcpkg baseline固定的3.12.0 upstream＋相符patch，取得方式沿round2 `evidence/bvi-frozen-suite`依賴說明。未把第三方header或binary包進commit。

既有去識別證據Library ID `libfile_2cf1e7ff23f48191bf13869a367a191b`，version0；ZIP SHA `d0a2eb1bebd35ff90c27493fe4ed914970bb24d22137c45af424574d2563db59`。解包manifest SHA `ddc738c400ad29be11ffdcf9015b4acd31aed770b6bbcea60b55b08aab0178b3`。26份derived hashes已核，原件historical hashes不與衍生bytes混用。既有readable unpack root `/workspace/shared/phigros-evidence-round2/unpacked`。

原suite共六份inputs，runner逐份鎖SHA：包內normalized/oracle/typed-r1/r1-cases/expected-coverage五份＋tracked `research/x10d_o_bvi/supplemental.json`。只套round2已審的singleton-lines形狀I/O adapter和fresh output reservation attempt；不改歷史main/driver/oracle或期望，不把`declared_usage`偷偷變真容量溢出。

## 3. 最終三配置結果

Release `-O2`；Debug `-O0 -g -D_GLIBCXX_ASSERTIONS`；Sanitizers `-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined`。每組fresh output。ASAN_OPTIONS=detect_leaks=0，UBSAN halt_on_error=1；原LSan受ptrace限制，沒有leak-free宣稱。

| 結果 | Release / Debug / Sanitizers | 證據 |
|---|---|---|
| 原3938 / 356 layer-cases | 各36 fail，native1／aggregate1 | `evidence/final-{release,debug,sanitizers}/suite.json` |
| 新R1 20×4 layer-cases | 原預期無失敗 | 同層legacy-summary `new_case_failed_assertions=0` |
| wrong-contact-only負控制 | 各預期2 fail、consumer_rejects=true、native1 | 同層wrong-contact.json；這個負例不算新產品缺陷，也不混入3938 |
| 原v2 contract48/339 | 各0fail/native0 | 同層contract.json，literal期望未改 |
| 原v2独立35/79 | 各0fail/native0 | 同層independent.json，原duplicate2fail已解 |
| 新relation37/74 | 各0fail/native0 | `evidence/contracts-final-*/relation.json` |
| 新contact52/167 | 各0fail/native0 | 同層contact.json，direct139是其子集不能相加 |
| V08新4/16 | 各0fail/native0 | 同層line_ambiguity.json |
| 独立44/101 | 各0fail/native0 | `evidence/independent/final-*/results.json` |
| 独立後追加5control | 各0failed cases/native0 | supplemental-final-release-rerun、supplemental-final-debug、supplemental-final-sanitizers |
| 既有scheduler13/66及Flick | 各native0 | `evidence/scheduler/final/`；舊Linux clock shim，正式source未改 |

完整suite每層89，合計356；通過336、失敗20 layer-cases，所有失敗與不利結果保留。4 schema負控制、22supplemental、renderer/contact-adapter控制由原driver執行並核對，未跳過。所有最終sanitizer native stderr空；無ASan/UBSan報錯不是實機或Windows驗收。

三配置完整suite逐byteSHA：`33172758b42d8c3e360777e914d3ea0ec21877d275cab842e2d91c658a1d240d`；原339輸出SHA353dc81cf031e29630d0b9aa36587590d13f9459e1ebbf28a38dbcd38a48712a；原79輸出SHAb977fae54611ab8d385acb42e90bdf68aa10ef07b083c3ec047c15207a09c52a。新三組與獨立三配置的byte比較亦核對；單次source前後SHA一致。

### 54原失敗逐項守恒

`evidence/final-original54-map.json`與`final-original54-classified.json`有每個assertion pointer、old/actual/expected/status。原54修18、仍36、新0；從v2則43修7、仍36、新0。C1/C2/C4各5/6/7已修；C3混色2、C5容量fixture24、C6raw-body8、C7raw-hit-order2仍失敗。動態equal_to/equivalent_to的reference-case輸出materialization有24筆變動，原定義及literal expected完全不變；這不是把候選輸出抄成新gold。

最終36不是36個已證正式產品bug；但也不能說全部fixture、全部無害或legacy全過。effect實際語義及真圖支持未裁定。新contract可驗真正溢出、invalid eligible-action fail-closed、ambiguous無可用action，而舊raw診斷差異照留。

## 4. 中間失敗如何保留

- `evidence/legacy-v3-*`：第一次整合3938，42fail＝原仍36＋V08新增6；三配置保留，不能誤當最後候選。
- `evidence/line-ambiguity/bvi-before.cpp`與凍結4/16：before4fail，after0；最終再次三配置。
- `evidence/relation/novelty-before.json`：same exact ROI的measured_depth遮擋變化曾繞過去重；先凍結再修。37/74包含該2例，不能與早35/72相加。
- `evidence/independent/query-noise-*`：在看source後才追加的x/width/angle微抖反例，不冒稱原盲測已涵蓋。三個pixels相同但背景變動的case都曾錯Down，修後拒絕；1px與0.5px累積正例仍保留。
- contact fresh-output首試source-race被完整性檢查拒絕，後續source-freeze後重跑；relation最初一次CLI參數數目錯誤原log保留並更正。
- independent supplemental首份summary使用過期JSON欄位，native結果已過但摘要錯誤；原runner/錯誤留存，更正摘要工具後用fresh根重跑，未改候選或oracle。
- 第一次git commit因repo未配置作者返回128；以命令限定的自動研究身份重試成功，未改global config。這是交付工具小錯，不是測試結果。

## 5. 可直接重跑的雲端命令

從repo root執行，先設現地路徑；下列只是冷測，不啟動任何裝置：

```bash
REPO=$(pwd)
INPUTS=/workspace/shared/phigros-evidence-round2/unpacked
JSON_INCLUDE=/tmp/phigros-bvi-round2-deps
E=docs/research/zero-miss-20261005/round4/evidence
# <fresh-name>每次換成不存在的路徑，不覆寫本輪結果。
bash research/bvi_cold_v3/run_regression.sh "$REPO" "$INPUTS" "$JSON_INCLUDE" "$E/<fresh-name>-legacy" release candidate all
# 正確觀察是exit1及36fail，不能因exit1直接跳過讀report。
bash research/bvi_cold_v3/run_new_contracts.sh "$REPO" "$JSON_INCLUDE" release "$E/<fresh-name>-contracts"
bash "$E/independent/run-independent.sh" "$REPO" "$JSON_INCLUDE" candidate release "$E/<fresh-name>-independent"
bash "$E/independent/run-supplemental.sh" "$REPO" "$JSON_INCLUDE" "$REPO/research/bvi_cold_v3" release "$E/<fresh-name>-supplemental"
bash research/bvi_cold_v3/run_scheduler_preservation.sh "$REPO" "$E/<fresh-name>-scheduler"
bash research/bvi_cold_v3/run_cost_probe.sh "$REPO" "$JSON_INCLUDE" "$E/<fresh-name>-cost"
```

legacy/new/independent/supplemental重跑時把release換debug、sanitizers即可。scheduler自身跑三配置，cost只Release。若依賴SHA、輸入hash、source或原oracle不同，不直接套用本報告結果。Windows需新build wiring與相同source/tests，不直接執行本Linux shell當作Windows認證。

## 6. 不在本輪pass清單

正式BVI pixels→touch／owner adapter、原Windows process-control/D19/D20重跑、root CMake/CTest、真圖人工gold、完整contact-prefix候選replay、Windows有效成本、profile／五指／裝置、任何新遊戲結果均未跑。root歷史348pass+1skip、舊CPU8pass、X1通過是歷史證據，不歸入此次分母。

停止理由：沒有真圖與當前合法ROI/all-lines可裁定adapter front/depth/角度／歧義旗標；fake Guard不能替代真owner identity/cursor。下一步按READINESS_AND_HANDOFF W0→真圖cold/owner→成本安全gate→有限實機。當前Chapter Legacy N、IN解鎖及全曲Miss=0仍須實機確認；本次已完成的是雲端開發和交接準備。
