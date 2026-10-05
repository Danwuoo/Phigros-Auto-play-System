# X10d-O 預先契約：current-body ownership

2026-10-04 Asia/Taipei。工作包4，實作及執行前保存。授權以 `RUNTIME_DECISION_SKIP_B_CONTROLLER_ACCEPTANCE_20261004.md` 及其 controller receipt 為準；本包只冷開發，不接B、P、live或成本。C36h tint1仍 behavioural/experimental baseline，main50 comparison/donor/live0，Dlyrotz IN13 M77未改善；全章節分母及目前解鎖 unknown。

## 來源、歸屬與修改邊界

HEAD `f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c`，原 dirty 全保留。先逐檔hash Git tracked/untracked（不含ignored原raw）及X10c frozen source/binary binding、baseline ON/OFF bridge。從未有suppression/P hook的 `out/x10c/baseline` 建立 `out/x10d-o/reference` 及 `candidate`，不在原out rebuild。只有新 `research/x10d_o`、新 `tools/x10d_o`、三份新docs及新batch可寫；src/include/root CMake、原apps/tests/docs、frozen來源/raw/ledger只讀。完整新source、patch、input、DLL/binary與compile閉包另存batch固定snapshot。

像素部位、tracking ID、line association、body ownership、action eligibility分開。head接入需當前front與有效association；body續接需當前支持且與原body歸屬相容；tail通過需独立當前tail支持，不由幾何過線宣告completed。predicted或舊anchor不是當前證據，local blue連續不證physical owner。ID只local join，unknown/proposed不升human gold。

## 唯一預聲明family：bilateral current corridor / BCC-v1

研究假說：以候選自身當前Note切向（不是運動方向或line法向）掃描兩側body fill，取得包含當前接觸區的有限連續區；兩側共同支持、context一致、anchor≤90ms、當前measured line／合法geometry，且候選與anchor local切向/寬度相容時，將該區對應近期anchor。遇到可見gap或第二個相容anchor輸出unknown，不跨gap、不放寬alias/grace/lead/color、不抑制任何raw/direct front或thin Tap。

此family先在隔離C++ resolver測試，不直接修改owner或fallback。reference resolver只投影C36h既有 `rails_geometry/held_body` claim的信任邊界，**不是原C36h的新行為**。reference正式core、原tests保持不變。resolver的support / owner結論是否能供策略使用是待否證命題。若反例否決exclusive-owner推論，保留pixel-support工具與negative，停止family；不為交付強接hook。

預先red清單：`GapCannotBeBorrowed`、`AbsentBodyIsUnknown`、`OneSideAbsentIsUnknown`、`SecondAnchorIsUnknown`、`TouchingTailIncomingIsUnknown`。最後一例是獨立render的兩個物理世界：原body持續 vs 舊tail與新front貼合，visible union、近期anchor輸入相同，synthetic oracle要求physical歸屬unknown；不得用runtime ID指定答案。它同時要求報正例coverage，不准全拒絕。不得刪除此負例或以golden修改掩蓋。

正負例：同column法向分離兩Hold；old tail/new front（gap及無可見gap）；fragment/effect；neighbor、不同line/方向、thin Tap；active期間line旋轉/平移；遠處未對齊近線才對齊；Note朝向/運動/line法向不相等；無root但當前body有效；短缺/absence/長gap；epoch/context/gate；pending/active/unknown Down/completed/nonzero cursor/prefix；返回過線。新fake-clock controls連未改C36h完整owner/scheduler，記其既有pending missing語義，**不期待P hook**。resolver與完整owner未接線時不宣稱end-to-end candidate通過。

## Grounding與執行門檻

先讀並逐SHA綁 H2440–2495、K2865–2895 原index/PNG/source_frame/QPC及X10c selected trace、fallback witness；控制窗 A3493–3504/B4979–4990/C5516–5524/E5281–5293/D6158–6220，一共196幀，新增窗0。直接目視代表原PNG，proposed link、不可判部位及rotation/absence缺口分列。只讀原已驗全prefix狀態，不從窗口冷啟動造contact。不得把synthetic world升格為真圖gold。

依序：source/protection/capacity → packet/grounding → reference core與resolver red XML → 唯一candidate resolver → Release原207 tests含27 RGB及新契約 → Debug-ASan新契約及相關原回歸。保留所有actual fail/skip/assertions；沒有預先授權修改舊期待。若family被否決，只作必要自驗/封存，不執行完整recording replay。

只有必要冷契約、input及來源binding均通過且family未被反例否決，才另freeze首run protocol/source/binary/DLL/manifest，最多4次full replay（失敗亦計）。預定reference ON、candidate ON/OFF，reference OFF可用exact baseline bridge；首run後比較binary不可重build。原7722 PNG逐SHA、32preroll/7715perception、原owner consumed cadence/capture-ready/frame-first，五指FakeTouch、零fake recognition/RPC分列；recorded touch只外部comparison。完整首action→EOF分母、全部unmatched、首次scene/bank/history/owner/contact差、取消/返回/重建/無返回、lost opportunity、release/exit contacts都須稽核；LCS≤10000 actions/25M cells，超界拒絕。新Down、獨立incoming漏拒/誤接、unsupported Move、過早Up、unknown重試/completed復活或重要全prefix差不可解均拒採。少Down/539消失不算成功。

## 硬上限與停止收尾

batch `measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o` ≤256MiB，其中開發224MiB、總控32MiB；所有新source/docs/commands/logs/failures/snapshots計入（repo外部同檔副本另加）。export/build `out/x10d-o` ≤1GiB。campaign+prior≤8GiB，不另root重置。開工實量8196523814+45307809=8241831623B，留本batch後仍餘79667513B；每重步前核全部額度及disk free≥剩餘全部預留+5GiB。無PNG複本、不刪/搬舊檔。

每configure/build命令≤300s、tests≤180s、packet reader≤180s；至多2 compile workers，單命令stdout/stderr各≤8MiB、≤100000行，命令總attempt≤24。同步PowerShell controller每250ms監視，超時/超額僅終止該Start-Process建立的root與可證descendants，保留失敗。O採一般冷build契約：root exit後15s內compiler/build descendants須退出；不沿A兩秒gate追switch/vctip，不全域服務/registry修改。一次有據工具修補上限，策略counterexample不算工程修補。run record/line≤2MiB；input index≤36000行/32MiB；196帧trace≤16MiB/256行；resolver候選/anchors≤128、每candidate≤4096 normal positions×12 probes（兩側6條bands）；一個原生RGB frame≤4096²×3、同時一張解碼；新reader output≤8MiB。每command輸入/exit/source snapshot保存，無background queue/writer，full replay streams沿已驗reader硬限且本batch額度先約束。

超限、工具無可信執行或策略反例時停止依賴步驟，完成已做/未做/unknown清單及容量/保護封存。禁止pas/live、emulator/ADB/UI/真觸控、模型、runtime/cost/stress/ABBA/B插樁、新chat/agent/傳訊/goal/automation/commit/push。新RESULT/HANDOFF與machine receipt交總控獨立驗收，不能自行採baseline或宣稱gameplay改善。
