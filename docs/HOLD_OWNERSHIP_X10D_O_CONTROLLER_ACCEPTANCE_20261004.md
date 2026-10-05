# X10d-O 總控獨立驗收與接續決策

2026-10-04，Asia/Taipei。**簽收 BCC-v1 被否決的研究結果及已完成的 Release 工程交付；不簽為 ownership 候選通過。** ASan 測試、候選接線、完整行為比較均未完成。C36h tint1 仍為 behavioural/experimental baseline，main50 為 donor/comparison、live0；suppression OFF，X10d-P 不混入。X12 成本資格仍 not-ready，Chapter Legacy 全曲解鎖 IN／完整 IN Miss=0 目標未達。

開發 chat：`01a10693-fbb4-7c00-b2e3-36082e83f12d`「開發 X10d-O Hold 歸屬契約」。原 [protocol](HOLD_OWNERSHIP_X10D_O_PROTOCOL_20261004.md)、[結果](HOLD_OWNERSHIP_X10D_O_RESULT_20261004.md)、[交接](HOLD_OWNERSHIP_X10D_O_HANDOFF_20261004.md)及 batch/out 封存不回寫。本頁接續 [B 驗收及工作包4授權](RUNTIME_DECISION_SKIP_B_CONTROLLER_ACCEPTANCE_20261004.md)；總帳內 O「尚未開始」按當時盤點閱讀，O 最新判定以本頁為準。

## 獨立核對與重現

總控只新增本頁及原 O batch 下 `controller-review/` 維護驗收資料。先讀實作及測試，再執行 frozen Release binaries，沒有 build、ASan、full replay、runtime/cost、emulator、ADB、觸控、模型或 goal。所有輸出使用新路徑。

- [完整性核對](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o/controller-review/integrity-retry.json)：**1322 個不同檔案的 SHA 及有宣告的 bytes，0 不符**。包括原435份 Git 檔、14份新增 repo 檔、source/snapshots、export、binary/DLL/cache/工具、選定 inputs、196 PNG 及 artifact ledger；組別有重疊，不相加。這不是整個 SDK/STL/system DLL closure，也不冒稱重 hash 所有 ignored raw。
- [補充核對](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o/controller-review/supplemental.json)：Git 路徑449＝原435＋開發14，非預期新增0。pre-reference 的 contract/reference/tests 與交付相同，candidate-before-build 的 candidate/owner/tests 也相同。兩份各72檔 export 與 frozen X10c parent 相符，runtime-policy.patch 為0B。
- HEAD 仍 `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，index SHA 不變，正式 src/include/root CMake 對 HEAD 無 diff；執行後重新 hash 上述1322檔，全部未變。

[獨立執行結果](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o/controller-review/reproduction-summary.json)：

| 項目 | 總控重跑結果 | 支持的結論 |
|---|---:|---|
| 原 C36h Release suite | 207通過／0失敗 | 原回歸保持，包括既有27張RGB |
| Reference adapter | 3通過／8失敗、exit1 | 失敗名稱與預聲明8項一致；不是C36h完整observer的8種實景錯誤 |
| BCC-v1 | 10通過／1失敗、exit1 | 同一 `TouchingTailIncomingIsUnknown` 反例重現 |
| 未改 C36h owner controls | 8通過／0失敗 | 原owner軟體守門；未接resolver，不是候選端到端通過 |
| Packet reader | 196/196、exit0 | 與開發報表逐byte相同；195 consumed／1原skip，873 bank candidates、263 Hold形狀、206 supported |

新 pixel-support report SHA256 `d3621ff8e883a436bbc0dd21dc2e28c0aa709a2a8eed489a1872ced23e787c30`。原始全action分母及候選ON/OFF仍 not_run/null，不用原封存trace或shape reader代替完整新replay。

總控維護 script 首次在產生命令receipt時將 PowerShell `$false` 寫成 `false`，於首次 binary 啟動前退出；當時已完成完整性檢查。保留原 script 與 failure receipt，修正後用 v2 及新 integrity receipt 重跑。實際 binary 執行恰上述5次，沒有隱去失敗或重跑產品建置。

## 判定及適用範圍

**1. 負結果成立，並暴露原正負 oracle 的可識別性矛盾。** `SolidSupportPositive` 以完整藍色矩形要求 owner1；touching 反例把它拆成175與125px兩段，render union逐byte相同，Note、line、anchor/context/clock也相同，卻要求 unknown。任何只依相同輸入的確定性 resolver 都不能同時滿足這兩個 owner 期待。因此這組11項不能原封不動當成下一候選的「全綠」目標。BCC-v1 把當前連續支持和單一相容近期anchor推成唯一 physical owner，已有足夠反例否決；改門檻或換family名稱不會增加資訊。

原測試及失敗必須保留。下一契約須分開「可見支持」「有界時序關聯」「唯一physical identity」「可允許的contact action」，並讓觀測完全相同的輸入得到一致的輸出限制。保守 unknown 也不能作產品成功：獨立incoming的機會、同contact持續Move與未知Down／completed不復活都須各有可辨識正負例。不得移除反例、讓所有案例都unknown或把軟體anchor當physical gold來取得全綠。

**2. 合成世界不足以否決所有即時pixels方法。** 總控直接查看原2478、2865、2881、2884：H可見長body與遮擋效果；K可見同column分離body與gap、front逐步接近線。這與開發grounding的可見描述一致，但不能確證ID521/539/682/684的physical關係或真tail完成。合成fill矩形不保證真實遊戲具有無縫貼合、相同有界近期影像的兩世界；未提供近期影像的helper亦不等於所有合法tracker。這是下一步研究可觀測差異的理由，不是已找到新候選。

**3. Packet數字只支持該reader的描述。** Reader對每個Hold使用scene第一條line作validity/context gate，再以空anchors掃描；因此206/263包含這個line選擇條件，不是獨立於line的純藍色pixel支持率，更不是正確note→line配對、physical ownership準確率或改善比例。沒有anchors及action接線，全physical_owner/action_eligibility為null是正確邊界。下一研究不得用這個數字當ownership品質基準，也不得回寫封存reader「修好」負结果。

ASan native產物存在不能代替建置守門通過；本次未重跑其建置或tests。開發保存的非quiescent失敗與PID/creation清理收據支持停止交付，總控沒有重演當時OS程序狀態。完整source/build依賴證明與端到端coverage未因Release重現而補齊。

## 接續工作包4R：可觀測契約與時序證據審查

依使用者既有「驗收確認後依順序派送」授權，下一個 chat 使用 **GPT-6.1 Sol／xhigh**，先做有界冷研究與可執行契約設計。原第五包live依賴仍不成立；本決策不採行B八輪量測、不重啟A建置支線，也不實作BCC-v2。

交付必須回答：在合法的即時pixels＋有界近期狀態下，有哪個實際可觀測差異能區分原held body延續、獨立incoming及fragment？若現有證據不足，具體缺何種輸入／標註，並停止該推論，而非泛稱需要更多資料或直接要求模型。

範圍與驗收：

1. 先核本頁機讀receipt、O的固定source/反例及X9/X10c原資料；保存原dirty與O封存。整理原11項的observable inputs和oracle，列出不可同時滿足的期待，提出新、另存的contract vectors；原文件／tests不改。
2. 以O原196張／七窗為起點，必須實際目視H/K連續短序列，而非只引報告。必要時另選相鄰原PNG至多64張，先列ordinal/source_frame/clock/SHA及理由，總distinct輸入上限260，不複製PNG。至少覆蓋可見gap前後、front近線、effect遮擋，並明列旋轉／late alignment證據缺口。目視事實、proposed關聯和unknown分層，human gold新增0。
3. 對最多兩種新增可觀測資訊作source／真圖／反例審查，例如短期內實際可見的分界持續或端部差異；這只是設計比較，不実作family、不跑策略。區分current pixel evidence與prediction、Note朝向／運動方向／line法向；給定有限history/window/context/capacity及失效條件，禁止曲名／進度／按鍵歷史決定策略。
4. 交付至多一個值得下一包冷實作的通用設計，或有證據的no-go。新可區分正例不能只是空泛的「唯一anchor」；identical-input pair應輸出相同限制。列具體輸入與獨立oracle、必要反例、同contact Move與新incoming Down機會、unknown/completed守門、旋轉中Hold及近線才對齊的未完成範圍。未執行tests明標未執行；不把觀測契約當完整遊戲驗收。

本包只新增研究docs、設計JSON及必要維護hash/容量資料。**不新增產品／分析演算法或新C++binary，不build、不跑ASan、full replay、runtime/cost/stress、B插樁、emulator/ADB/觸控、模型、goal、automation、commit/push或自行續派。** 如需新工具才能回答，交具體缺口與最小後續方案，不擴張本包。

容量新根 `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-observability`，上限16MiB（開發12MiB含外部新增docs/JSON/失敗資料，總控4MiB）；不建out、不複製pixels、不刪舊資料。接手重核原8GiB aggregate：carry既有prior45307809B、O外部72115B、本總控外部頁與以後新增外部檔，勿只算campaign或另root歸零。本輪O總控實際容量與carry值見[controller receipt](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o/controller-review/controller-receipt.json)。開工free至少新預留＋5GiB。完成即停止交總控；有新設計也須另行驗收後才授權實作。

產品判定不變：目前章節分母、各曲當前IN解鎖unknown；89份選入完整結算皆有Miss，完整IN Miss=0證據仍0。此次驗收新增的是可重現的否決證據，沒有新增遊戲改善。
