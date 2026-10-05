# X10d-O：current body 支持不等於唯一 physical ownership

2026-10-04 Asia/Taipei。**唯一候選 BCC-v1 的 exclusive-owner 推論被合成反例否決，沒有可採用的 ownership 策略。** 本包停止於可信 Release 負結果；沒有接線到 observer/owner，沒有完整 recording replay、live、B插樁或P hook。C36h tint1仍 behavioural/experimental baseline；main50 comparison/donor/live0，Dlyrotz IN13的77 Miss未改善。產品 Chapter Legacy完整IN Miss=0、目前解鎖及章節分母仍未驗／unknown。

授權與先存契約：[protocol](HOLD_OWNERSHIP_X10D_O_PROTOCOL_20261004.md)。證據根 `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o`。本頁是開發自驗，**待總控獨立驗收**，沒有自簽策略採用。

## 實作與來源

從 frozen `out/x10c/baseline`（parent `out/x1/c36h-v3`）建立新 `out/x10d-o/reference`／`candidate` exports，兩份皆逐檔與parent相同；suppression OFF、pending hook未加入。正式src/include、root CMake及原apps/tests/docs未改。新C++20來源位於 `research/x10d_o`，包含契約、reference adapter、BCC-v1 resolver、独立render反例、FakeClock owner controls及packet reader；維護入口位於 `tools/x10d_o`。source/binary/DLL/input/compiled-source清單及固定snapshot見 `delivery-binding.json`，原dirty/frozen保護見 `protection-after.json`。

**Reference adapter不是原C36h的新函式或完整observer反例。** 它刻意暴露「可信任rails/held flag及geometry就把body extent對應首個anchor」的假設，只作先紅對照。真正C36h core及207項原tests完整保留。Candidate只在隔離resolver內實作：當前Note自身局部座標的雙側fill掃描，不跨可見gap；再以90ms內同context／切向／寬度及位置相容anchor提出歸屬。line association/root/owner action是分開的caller責任；沒有刪raw/direct front、thin Tap、改grace/alias/lead或復活X10b suppression。

這個helper可量當前支持區，但 `BCC_v1_exclusive_owner_hypothesis` 是被否決的輸出，不能供觸控使用。因冷反例已否決，沒有做完整策略接線／候選runtime patch；兩export runtime政策相同不構成新策略通過。`runtime-policy.patch`保存此零差異邊界，helper全文另snapshot保存。

## Grounding與軟體狀態

新packet保留七窗196張原PNG的ordinal/source_frame/capture_complete/pixels_ready/source domain與逐SHA綁定，沒有PNG副本。H2440–2495、K2865–2895，以及A3493–3504/B4979–4990/C5516–5524/E5281–5293/D6158–6220全部保留，新增窗0。直接目視原2474/2478/2479、2865/2881/2883/2884/2885、3498、6214十張圖；發現與缺口見 `grounding.json` 固定snapshot。

- H的高body／側rails連續且有yellow effects，較支持fragment假說；539與521的physical link仍proposed。讀原已驗full-prefix selected trace，原521/contact持續，539另於2478 Down、2479 identity ambiguity取消，沒有把它叫原Hold掉線。
- K2865同column兩段body有可見空隙；2881的incoming front仍約y533而線約y576。原witness的682 claim從576回延214px，可能涵蓋incoming body；local IDs不能把該推論升gold。K2884/2885新front到線，原tail／原body已不可辨；其ownership仍unknown。
- A3498給傾斜body及鄰線，D6214給不同方向thin notes；十張代表圖不足證明active旋轉、late alignment或absence/tail完整實景語義。這些缺口用分列合成controls處理，沒有自標human gold。

新C++ packet reader逐PNG SHA/decode及完整clock/trace join **196/196**；其中195幀有原consumed scene，K2882為原cadence skip，保留null。遍歷可用bank全部873個candidates及873個targets，其中263個Hold形狀，206份符合當前雙側支持scan、57份不符，共342300 probes。**這是重複逐幀候選的支持描述，不是physical notes、準確率或改善比例。** 全部physical_owner及action_eligibility為null；reader不讀runtime history來授予physical owner，也不啟動新的perception/owner。原full-prefix trace只讀引用，未造窗口初始contact。

## 先紅與停止反例

`red-declaration.json`在reference執行及candidate實作前保存完整8個red names；`red-verification.json`核actual fail清單相同。原期待、fail assertions與XML未重寫。

| 新執行 | 通過／失敗／skip | 範圍 |
|---|---:|---|
| Release原C36h suite | 207／0／0 | 原207 distinct cases，包括27張frozen realRGB |
| Release reference adapter契約 | 3／8／0 | 全部事先聲明red，並非C36h完整observer被證8種實景錯誤 |
| Release BCC-v1同套契約 | 10／1／0 | 唯一失敗 `TouchingTailIncomingIsUnknown`，阻止採用 |
| Release未改C36h owner controls | 8／0／0 | 完整owner/scheduler/FakeTouch，但沒有resolver接線 |
| Debug-ASan新契約／原回歸 | **未執行** | build command未過quiescence，不把產出EXE當驗收 |

反例用兩個獨立render世界：一個完整body；另一個同column的old body tail與incoming front貼合。兩段render的visible union與第一世界**逐byte相同**，可供resolver的近期anchor/context/clock/line/geometry亦相同。BCC-v1在兩者都給唯一owner1；第二世界oracle要求unknown，故fail。這證明此family從「連續支持＋唯一近期anchor」推唯一physical owner的資訊不足，**不證明真實H/K就是該合成世界，也不否決所有可能的ownership方法**。當前支持scan仍可作診斷，exclusive-owner採用否決；沒有調threshold或另一family。

其餘契約覆蓋可見gap不借extent、absence、一側缺失、第二anchor、不同方向／neighbor／thin Tap、當前body旋轉平移及Note切向不等於line切向。旋轉resolver case的anchor姿態相容是typed控制，沒有證新tracker可在真圖恢復它。late alignment case只驗不把Note axis強制等於line axis，**不是新完整遠→近association rollout**。

8項owner controls驗rootless body同contact按當前hit Move（非初Down、非盲跟line）、修訂後nonzero prefix/cursor、短缺不Move及longgap release、unknown Down不重試、completed不復活、gate/epoch/context撤銷、独立incoming/Thin Tap新Down正例，以及單份幾何tail pass不立即完成。pending缺席仍保持原C36h grace；没有混P。它們證明未改owner的軟體邊界，**不證明被否決resolver能提供可靠當前body**。舊tail已過線／new front接近的完整新ownership RGB rollout及真實旋轉／absence語義仍未完成；不能用這些controls代替端到端候選coverage。

## 執行失敗與未做比較

所有commands/logs/exits及失敗來源保留，不只保存成功輸出：

1. 初始prepare遇Git預設quoted中文路徑，逐檔inventory尚未寫出即exit1；修正 `core.quotepath=false`，只恢復空的新out／未初始化batch，不刪舊檔。這是初始化工程修正。
2. 初次configure native stdout顯示完成，但執行controller以integer-key Hashtable保存descendants失敗，沒有可信exit receipt。保留log，**一次執行工具修補**改為pid/creation array，另一次configure才exit0。後續沒有開A checker/guard/vctip猜switch支線。
3. Packet reader首build以std::string直接比較JSON，compile exit1；保存源碼及錯誤，改明確string extraction後build pass，未改策略／oracle。
4. Packet reader首run錯把所有selected frames當consumed，遇2882 null exit1；保留失敗後以原cadence skipped/null契約修正reader，196逐SHA/clock join通過。沒有補造scene或改輸入。
5. Debug-ASan build native log有三個EXE产出，但root退出後compiler/build descendants未在預定15s內quiesce，controller exit=-1／`descendants_nonquiescent`。受控PID/creation只限此命令後代，終止後只讀核對剩餘0；不改OS服務/registry或原A兩秒gate，不再修補/rebuild、不執行ASan tests。這些EXE封存為**failed-build/unverified**。

工具修補與一般source工程修正分列，沒有將failed configure/build稱為通過。Release tests與packet reader各命令已取得exit且quiescence核對；ASan缺口阻止宣稱完整冷驗收。

**新完整recording replay 0／上限4，failed replay 0，live0。** 在D已否決family，E門檻不成立；沒有湊滿replay額度。新reference/candidate全action分母、unmatched、first scene/history/owner/contact divergence、取消→返回、lost opportunities、release/contacts_at_exit及ON/OFF行為比較皆 `null/not_run`。不可填零或從source相同推為已跑全prefix。原C36h歷史2165 actions及ON/OFF bridge只作來源完整性支持，沒有重跑7722張；本包新PNG驗證限196張。

## 交付判定

已實作：隔離BCC-v1、正負合成契約、FakeClock controls、196-frame唯讀reader、source/容量/保護帳。已自驗：Release207/207、reference紅清單、candidate10/11負結果、owner8/8、packet196/196。待總控：本包研究／工程負结果及來源保護獨立驗收。否決：BCC-v1 exclusive-owner；保留：current-support scan診斷及反例。未完成：ASan執行、可靠ownership策略及端到端候選／全行為比較。Gameplay Unknown，未採baseline；X12成本not-ready仍不變。

容量以 `final-receipt.json`／`capacity.json` settled數字為準：batch加repo外部新source/docs/tool同檔全部計入224MiB開發額度，32MiB留總控；out export/build另≤1GiB，campaign+prior＋新外部檔不超8GiB。原dirty及index保持，原PNG／frozen未改；沒有stage/reset/clean/搬移/刪除/commit/push。下一步只交總控，沒有自行接下一包。
