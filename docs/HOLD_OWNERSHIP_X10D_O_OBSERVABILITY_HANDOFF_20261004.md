# X10d-O 工作包4R交接：研究／設計，待總控

2026-10-04，Asia/Taipei。**交一個 BVI-1 有界可見分界介面設計；沒有 ownership runtime 候選。** 先讀 [研究結果](HOLD_OWNERSHIP_X10D_O_OBSERVABILITY_20261004.md)、[設計JSON](HOLD_OWNERSHIP_X10D_O_OBSERVABILITY_DESIGN_20261004.json) 及 [總控4R契約](HOLD_OWNERSHIP_X10D_O_CONTROLLER_ACCEPTANCE_20261004.md)。原O負結果、11項tests、196PNG packet、frozen/raw/out保留，不prepare/build/finalize原封存。

新根：`measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-observability`。機讀入口為 `final-receipt.json`、`artifact-ledger.json`、`source-validation.json`、`protection-before.json`、`protection-after.json`、`review-selection.json`、`review-findings.json`。維護 `audit.ps1` 僅hash/容量／dirty核對，沒有產品或分析演算法；作者JSON呼叫一次JS語法失敗於任何nested tool之前，0檔写入，收據另留 `authoring-failure.json`，後續明文JSON可解析。初版容量使用Measure-Object加總OrderedDictionary而漏計外部三檔；原逐檔bytes正確，初版receipt/ledger/script以-v1完整保留、摘要不接受。`capacity-correction.json`記原因，最後receipt改用明確整數加總，全部外部檔及本交接新增文字均計額。沒有重做任何原O工具／binary命令。

## 判定與具體交付

- 原Solid與Touching相同完整input的owner期待矛盾成立；原11项已逐项列observable input／oracle，保持原測試。Gap原測試只有depth限制，不能偷加稱原owner-null oracle；原rotation/late軸測試不是完整時序rollout。
- 先列52張來源映射再直接目視H2468–2488、K2865–2889、A3496–3500及D6214。H有持續外rails／effect遮擋／小角度變化；K有雙端部與gap、front近線至重疊。total distinct196、額外PNG0、複本0、physical human gold0。觀察、proposed與unknown分列。
- 只審兩資訊：當前端部／分界／contact-zone支持，以及≤6實測／90ms分界延續。BVI-1分開current support、temporal link、physical假說及contact限制；禁止連續blue＋anchor ID→唯一physical owner。
- V00–V19是未執行的獨立oracle設計。包含不可辨pair、supported same-contact Move、獨立incoming及thin Tap機會、effect/缺側/多線、旋轉中Hold、遠→近late alignment、unknown/completed/返回/tail、context/capacity/期限與ID重編。實景physical期待null，不能讓全部unknown當成功。
- 原packet reader使用scene第一條line作context gate，且anchors空；206/263不是ownership準確率。原trace是rebased fake-clock，原capture與replay原點不能相減；2882可看圖但原consumed=false／scene null不補造。

## 總控獨立驗收及停止線

總控在本包4MiB預留內另建 `controller-review`，先核新ledger/receipt與三份外部doc/JSON，再核原450路徑及1762 protected檔不變、來源refs及selected PNG映射。驗收可直接目視H/K，核新契約是否真的需要不同可觀測input、same-input一致、正例保留incoming/Move，以及oracle沒有讀BVI自身輸出。**本包產品tests/build/ASan/full replay/runtime/cost/stress未執行**，不能以JSON與hash檢查簽成演算法驗證；不需重跑原O的Release或reader來驗收本次研究。

新根16MiB＝開發12MiB（含外部三檔及失敗收據）＋總控4MiB；精確charged/remaining及原8GiB carry在final receipt。campaign/prior45307809B/O外部72115B/O總控頁9546B都保留，加本包外部檔；不建out、不刪舊資料。

下一包最小提案是隔離C++20 descriptor/link契約＋獨立renderer/oracle與fake-clock；尚未授權實作或接owner。若摘要無法保留反證就停交缺口，不添模型／past RGB無界buffer。完整行為／成本／freeze/preflight/live另需原gate及總控續派。H physical fragment、K真正tail／採納、完整active旋轉／late alignment實景語義仍未驗；不從合成綠燈推遊戲zero miss。

C36h tint1 baseline、main50 donor/live0、suppression OFF、P不混入、X12 not-ready及Chapter Legacy全曲IN Miss=0未達保持。本chat交付後停止等待總控，不新goal/chat/automation、不自行續派、commit/push或emulator/ADB/觸控。
