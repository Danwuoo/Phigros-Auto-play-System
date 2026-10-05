# X1 R1／R2：離線比較來源驗證修補交接

日期：2026-10-02（Asia/Taipei）。**開發自驗通過，待總控獨立驗收；X1未由本chat宣告結案。** 依[現況J2／J3](../01-project/PROJECT_STATUS_NEXT_STEPS_20261001.md#j2-必須補修的兩個可重現缺口)修補比較入口。C36h tint1仍是behavioural／experimental baseline，main50仍是比較候選／mechanism donor。沒有啟動X2、模型訓練、emulator、真觸控、manual-session、goal或自動化，也沒有提交／push。

## 變更與來源契約

接手時為main、HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`、唯一registered worktree；策略50／27／11。既有X1未提交成果保留，[workspace-before.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/repair-r1-r2-20261002/workspace-before.json)記下status、branch、worktree、strategy與14個既有修改檔案SHA。此次只改原contact比較入口、offline CMake測試連結、contact測試，新增小型validation header與維護腳本，並新增本交接及向status J節追加J4。`src/`、`include/`、根CMake、原main.cpp、replay clock／cadence／owner／association不變。

- [contact_replay.cpp](../../../apps/frame_review/contact_replay.cpp)在讀trace／events前呼叫[comparison_input.hpp](../../../apps/frame_review/comparison_input.hpp)共用驗證。CLI仍為 `contact-compare manifest c36h-run main50-run new-report`，有效comparison JSON格式不變。
- **R1**：supplied manifest的實際file SHA必須同時等於兩份summary的 `input_manifest_sha256`。連只追加換行也拒絕；只改index SHA或window亦拒絕。驗integer schema=1、五個已知case ID各一次及對應anchor、integer ordinal在0–35999、first≤anchor≤last、每窗≤120 frames，並要求index/profile SHA與batch root必要欄位。不同查詢窗不能沿此CLI冒充原run input；沒有新增query-manifest平台。
- **R2**：第一summary必須明列 `lineage=c36h`，第二必須為 `main50`。交換、兩份同lineage、缺失／null／unknown lineage均拒絕。兩份run必須成功且原cadence／tie／recognition／receipt政策一致。
- 每個summary須有合法binary／source provenance SHA，兩角色的source／binary SHA須不同。source provenance SHA須實際匹配manifest `batch_root`下既有的 `source-v2/<role>-source-provenance.json`或早期root同名檔，且該檔lineage必須支持宣告角色；每檔≤2MiB，最多查兩條既有路徑。沒有新registry或遞迴搜尋平台。
- `binary_sha256`仍指**產生replay的歷史工具**；compare不hash自身argv[0]來限制run來源。CLI維護自驗另外重算兩個verified replay binaries及provenances，和run summary核對。入口沒有重新執行replay、重新hash全部PNG／export，也不把summary中的binary SHA聲稱成新工具產生的trace。
- 所有入口負例非零退出、明列reason且不新建comparison report；既有output先拒絕，保存原SHA。[scoped-tool-diff.patch](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/repair-r1-r2-20261002/scoped-tool-diff.patch)供比較既有frozen source的窄範圍變更；新增檔原文另在source snapshot。

## 新工具與既有replay的區別

以下 `B`＝`measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1`，`P`＝`B/repair-r1-r2-20261002`。本輪重用既有instrumented exports `out/x1/c36h-v3`／`out/x1/main50-v2`，未修改它們，沒有新增export或full replay。新建置為 `out/x1/repair-build36`／`repair-build50`，其工具另複製凍結到 `repair-tools36`／`repair-tools50`，沒有覆寫verified-tools。

| 識別 | SHA256 |
|---|---|
| 新C36h比較工具 `out/x1/repair-tools36/pas_frame_review.exe` | `5919a4d1316b4e7a8a6bc4d44a416f606e5108a1e97a95f7255dfb1fff5902b0` |
| 新main50比較工具 `out/x1/repair-tools50/pas_frame_review.exe` | `e19a618269d61cad0e99fb93c9012df20bef3c3de646b3d3c7dd5d651b3853b6` |
| 既有C36h trace的replay工具 `verified-tools36` | `438508987ec885873549582637b0521bc55c0e5ea0cf3c3cfa7d1675879ede95` |
| 既有main50 trace的replay工具 `verified-tools50` | `f402388a3316a251cc8d2b4fce2c05e30a9ccb64c229aefc1137d329a4838dfd` |
| 不變input manifest | `99e8a364ea9f8575d67fbe39b1b76cccd6820816bc4d999d1b1bdfcfec2dd955` |
| 新 `contact_replay.cpp` | `eb9aafdbfc54ca3c2708ce853d30f11edf7de0da8e171c285f9b29f784a99c62` |
| 新 `comparison_input.hpp` | `fa74ce05459b2100bddaf20ef4f106838a74a72865fb5b5826149abf9ef8bc18` |
| 新tests source | `1aa692ec456deb1a854f1d995a19738b75cc84bd52af54f72a682abec55e881d` |
| 新offline CMake | `b506a2aff0d5c040acea989b08cc489f8106d3597503438aebf70a98f4ba487b` |
| 新CLI維護腳本 | `57e63596f940994e50ba02069c6471803d3d087c001331fe9454c4e57f647916` |

[tool-freeze.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/repair-r1-r2-20261002/tool-freeze.json)（SHA `74856111618b5018efb5f734220c27c4163a2c2597e054a90441359d00d163b4`）列14個tool sources及其 `P/tool-source/`實體快照、12個新binary／DLL、兩個重用export的149個檔案SHA、兩個舊run的summary／provenance／binary來源，以及實際執行的新test binary SHA。新工具仍含原contact入口，但本輪只用compare／prefix及離線tests，**没有用它產生新全錄traces或gameplay**。

## 驗證結果與可重現命令

Windows x64／MSVC19.51.36256.0 v145 Release，兩套各自連結原lineage core；用既有 `PAS_RGB_CLIP_ROOT`，新test binary執行原offline套件及**10項新增ContactComparison測試**：C36h **207/207**，main50 **234/234**，fail0／skip0。兩套共用cases不當作441個獨立場景。保存[tests36 XML](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/repair-r1-r2-20261002/tests36.xml)／[tests50 XML](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/repair-r1-r2-20261002/tests50.xml)及原logs。兩個configure／build首輪即exit0；第三方／原export既有compiler warnings留在log，沒有新的patch編譯錯誤。未重跑Debug／ASan或全repository測試；本次没有策略或replay物件生命週期變更。

新增測試連結正式contact_replay.cpp並直接呼叫CLI，不複製validation實作。正例只有五行空scene＋空events；負例沒有trace檔，驗其reason即可確認在trace讀取前被拒絕，不複製數十MiB traces。涵蓋正确配對／不同compare binary、manifest bytes／index／windows及任一summary SHA不符、bound但非法schema／windows、交換／同lineage／缺失／unknown、provenance role／SHA、缺失source／binary SHA、同binary、policy mismatch、failed run及existing output保存。

[實際CLI命令／exit／reason／output存在性](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/repair-r1-r2-20261002/cli/result.json)（SHA `50b349aa22a9589e5483070caa7f768b361ba0feffcf611b85363afd61fd8fb9`）保存1個正例及**16個預期拒絕**，沒有意外成功。其中原總控wrong manifest直接重用原檔：exit1、`comparison_manifest_SHA_mismatch`、`P/cli/r1-original.json`不存在；原交換順序：exit1、`comparison_lineage_expected_c36h`、`P/cli/r2-original-swapped.json`不存在。另驗只改bytes、只改window、兩種same-lineage、兩側missing／unknown／failed／spoofed-source、mixed policy及existing output。所有summary-only負例留在P內；原兩份accepted錯誤輸出及原reproduction不改寫、不當研究結論。

下列只重算小報告；`cli-recheck-new`必須不存在，先核容量，不重跑full contact：

```powershell
$x1Batch = 'measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1'
$x1Repair = "$x1Batch/repair-r1-r2-20261002"
./tools/check-contact-comparison.ps1 -Batch $x1Batch `
  -Tool out/x1/repair-tools36/pas_frame_review.exe `
  -NewEvidenceRoot "$x1Repair/cli-recheck-new"

# 原R1／R2的單獨重現；兩者預期exit1，輸出檔必須不存在。
./out/x1/repair-tools36/pas_frame_review.exe contact-compare `
  "$x1Batch/acceptance-20261002/wrong-input-manifest.json" `
  "$x1Batch/c36h-verified-on-1" "$x1Batch/main50-verified-on-1" "$x1Repair/r1-recheck-new.json"
./out/x1/repair-tools36/pas_frame_review.exe contact-compare "$x1Batch/input-manifest.json" `
  "$x1Batch/main50-verified-on-1" "$x1Batch/c36h-verified-on-1" "$x1Repair/r2-recheck-new.json"

$env:PAS_RGB_CLIP_ROOT = "$pwd/measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
./out/x1/repair-tools36/x1_tests.exe --gtest_brief=1
./out/x1/repair-tools50/x1_tests.exe --gtest_brief=1
```

由工作區source重建前核對tool-freeze的14檔SHA、重用export及DLL；使用新build目錄。main50把下面的c36h-v3改main50-v2，各自建置，不混ABI：

```powershell
cmake -S apps/frame_review/offline -B out/x1/repair-rebuild36-new -G 'Visual Studio 18 2026' -A x64 -T v145 `
  '-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275' `
  "-DX1_SOURCE=$pwd/out/x1/c36h-v3" "-DX1_REPO=$pwd" "-DCMAKE_PREFIX_PATH=$pwd/out/vcpkg_installed/x64-windows"
cmake --build out/x1/repair-rebuild36-new --config Release --parallel 2
Copy-Item -LiteralPath out/vcpkg_installed/x64-windows/bin/z.dll,out/vcpkg_installed/x64-windows/bin/gtest.dll,out/vcpkg_installed/x64-windows/bin/gtest_main.dll -Destination out/x1/repair-rebuild36-new/Release
./out/x1/repair-rebuild36-new/Release/x1_tests.exe --gtest_brief=1
```

## 正確案例與報告兼容

新工具讀原 `c36h-verified-on-1`／`main50-verified-on-1`，輸出[五案例](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/repair-r1-r2-20261002/cli/five-cases.json) **537,176B，byte-identical**，SHA `37d469caa0017be00fb78736fd69fb5cc87585c40b9fd671bb7e8397c420e7f7`，與frozen報告一致；沒有新增comparison metadata。

新建prefix工具也讀此報告及舊events成功產[新prefix](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/repair-r1-r2-20261002/prefix.json)，SHA `5fd5a2e00aad3855b6dff8b5ac17ef4f6ddb2ba0d37695729bfac43b6f95ffdc`。對照原prefix，只有 `comparison_report`絕對路徑及 `binary_sha256`改成新prefix工具；cases、comparison report SHA、兩份events SHA、scope及physical gold等欄位皆相同。[semantic-check.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/repair-r1-r2-20261002/semantic-check.json)保存逐項核對。

| case | frames | state first ordinal | 成功Down C36h／main50 | 窗口commands C36h／main50 |
|---|---:|---|---|---|
| A3498：3493–3504 | 12 | 3493 | 1／1 | 9／9 |
| B4986：4979–4990 | 12 | 無 | 1／1 | 1／1 |
| C5520：5516–5524 | 9 | 無 | 1／1 | 6／6 |
| D6214：6158–6220 | 63 | 6160 | **1／0** | 6／0 |
| E5287：5281–5293，使用者正常control | 13 | 無 | 1／1 | 2／1 |

這是原有效比較語義重算，不更新原案例因果結論。A的無root續contact、B的absence及unknown tail、D的初始relation與confirmed preserve鏈、E的正常control定位沿用[原交接](C36H_CONTACT_REPLAY_HANDOFF_20261001.md)。offline root／action數不換算gameplay命中率或Miss改善。

## 保護、容量與未解項

[preservation-check.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/repair-r1-r2-20261002/preservation-check.json)核對原278個batch artifacts、7個acceptance artifacts、11個frozen binaries、149個export檔案，以及11個非本次code修補範圍的既有工作區檔案，mismatches=0。status追加前完整原文另外保留 `P/status-before-append.md`，SHA `59f73b5e40e55b9ced27f6a2d931d3ec9ec6d66bdf2fff6a50266e6ced49e696`；A–I及J1–J3原文保留。沒有reset／clean／刪除frozen或失敗evidence。

[capacity-ledger.json](../../../measurements/game-assist/2026-09-30-m0-manual-continue/contact-replay-x1/repair-r1-r2-20261002/capacity-ledger.json)（SHA `18d6efc533b89cfa095d63327bd5511e8e61daf54afa37074f53cd3629e1d6c3`）列所有新artifact的bytes／SHA，ledger自身11,253B計入容量、略自SHA避免循環。

| logical容量（bytes） | 實測 |
|---|---:|
| B開始／上限 | 128,626,102／134,217,728 |
| 本修補P新增，含source快照／tests／logs／reports／ledger | **983,240** |
| B結束／剩餘 | **129,609,342／4,608,386** |
| campaign＋prior research結束／8GiB上限 | **7,806,646,147／8,589,934,592** |
| campaign＋prior剩餘 | 783,288,445 |
| 新build36／build50 | 158,050,609／163,186,013 |
| 新tools36／tools50 | 3,725,312／3,978,240 |
| 新build＋tools另列合計 | **328,940,174** |
| 原重用exports36／50（已存在，未另複製） | 8,198,930／7,107,369 |

P全數留在同一B／campaign，沒有另root規避128MiB。建置／binary產品放新的out名稱且另列容量；舊out與exports保留。新增資料只有小報告與validation fixture，**new full replay=0、new live round=0**。

仍待總控獨立核對R1／R2、新source／binary freeze與正例語義，開發自驗不替代其驗收。完整原perception cadence、早期state、source render age、real RPC／recognition delay、物理identity gold及遊戲採納仍unknown。沒有修改runtime機制或恢復舊流程；X2尚未開始。
