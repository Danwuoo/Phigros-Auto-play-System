# X6：Current-pixel 中心可靠性（2026-10-03）

**工程自驗完成；異色遮斷 v1 不採用為 runtime 可靠性 gate。** 本輪總控實作及驗證，未冒稱其他 agent 獨立驗收。依[後續執行計畫](C36H_FORWARD_EXECUTION_PLAN_20261003.md)，沒有調閾值救結果；先轉 X9 的 Hold 因果定位。C36h tint1 仍 baseline，main50 僅 comparison/donor；正式策略未改，沒有新 gameplay 改善證據。

## 問題、方法與範圍

X5 補正發現 6173–75 yellow Drag 的中心及長度變動，原圖同時有紅 Flick 交疊。假說是量測形狀裁切可製造假轉向，而非物件實際轉向。本次先量 pixel cue，不修座標、不合併 identity、不改 relation 或 owner。

新 C++20 `x6_pixel.hpp` 在 observer ROI 的長軸兩端各加 24px，沿法向採樣半徑 clamp(ceil(thickness/2),3,12)；最長 513 bins、每 bin 最少 2 個目標色 sampling votes。色條件沿 frozen observer 的 cyan/yellow/red RGB predicates，逐幀只看 current RGB。輸出色核心 span、coverage、分段數、最大內缺口、異色缺口及相對 observer center 的 extent midpoint。旋轉格點會重複採到同一 pixel，votes 不宣稱 unique area。

預先規則：存在多段、最大 gap≥3 且至少 2 個 gap bins 有異色，才列 `foreign_overlap_fragmentation`；缺支持或 ROI clip 為 unknown。其餘 `current_core_supported_not_motion_gold` 只表示色支持，**不代表 motion reliable、單一 physical object 或正確線角色**。相同色塊加紅色缺口可以是一個遮擋物件，也可以是兩個獨立物件；測試保存這項非唯一性。

`x6_report.cpp` 只接受已獨立驗收 X5 report SHA `8bf5b5a5973bd028a254a8cc6b7179720aaed6468c0d00858e0ea0f8c6d20770`。重新核 114 張 PNG SHA，完整處理三 histories 的 337 role frames／1220 target occurrences。最多 6 frames／90ms、每幀 128 objects；只用 explicit runtime prior assignment 建測量 pair，identity 仍 proposed。Hold 明確排除；不是從 detector 未提出的物件量 recall。512MiB process commit、4MiB output 硬上限，PNG 逐張載入。

## 結果與反證

| history | targets | measured | Hold excluded | paired | 異色遮斷 witness | unknown |
|---|---:|---:|---:|---:|---:|---:|
| C36h |394|337|57|299|7|0|
| main50 |413|356|57|308|10|0|
| X4 factual history |413|356|57|314|10|0|

三者仍是同一 recording 的不同 replay history，非獨立歌曲。Human gold 增加 0；没有 precision/recall 或 gameplay hit rate。

**Verified measurements：** C36h local1622 在 6173→6174，observer 長度 152.007→124.015px（−27.992）；current 色核心 span 158→154px（−4），extent midpoint offset 0.5→10.5px，沿當前長軸的 observer center shift −11.510px。6174 coverage .92857、3 segments、最大 gap6，但 foreign gap bins 只有1，故 v1 **未觸發**。原 PNG 可見白色輪廓／箭頭參與交疊；不能改成放寬紅色閾值後宣稱預先成功。

**Strong inference：** 色核心的可見長度與 observer 中心變化不一致，支持 observation-shape bias 是 X5 約60°轉向 proposal 的重要混淆因素。尚未做 observation substitution causal experiment，不稱唯一原因；6140之外／其他歌曲亦未驗。

**Verified controls：** 6188–91 Tap local1626 的中心有明顯姿態變化，而 midpoint offset 僅 −0.5/−0.5/0/0.5px、coverage 全1。同期 Flick local1620 常有約 .758 coverage、2 segments，屬中央箭頭拓撲；不能以所有低 coverage／多段一律抑制。C36h 的7次異色 witness落在6170–74（含相鄰候選及Flick），不是7個物理失敗。E5287保留正常control；excluded Hold不被評判好壞。

**決策：** X6 特徵可用於離線拆解 observer center 與 RGB 支持，但單一異色缺口 witness 不足以保證中心可靠性，也未分辨 physical object 數。拒絕把 v1 帶入 X8，拒絕直接補 center／延長 evidence lease。X7 初配承諾仍缺新 role cue；暫不開 NDA-v2，先依計畫轉 X9，查原2479 Hold identity競爭在 C36h 是否仍可重現。

## 驗證與重現

入口：[pixel-reliability-x6.ps1](../tools/pixel-reliability-x6.ps1)。新 build `out/x6/{release,asan}`；舊 frozen binary/export 不重建。只重用 core 的 PNG/SHA utilities；report 不呼叫 observer／owner／touch。

- Release／Debug-ASan 各 **13/13**，fail/skip/disabled 0：完整色核心、遮擋、observer裁切、真缺像素、黑缺口、旋轉平移、軸符號、相鄰物件、錯色、同色二物件歧義、clip/NaN/容量／非法frame。
- 最後 Release reports `Report-release-3/4.json` 各 **1,240,209B**、byte-identical；ASan `Report-asan-2.json` 僅 binary SHA不同。完整 reader 使用明列較小 ASan quarantine；不宣稱 default-quarantine reader成功。
- 最後兩 binaries wrong-argc／wrong-parent 各2例，4/4符合非零及指定reason。不是全面輸入偽造或磁碟耗盡測試。
- 初build因同一行多個 GTest EXPECT_THROW 產生重複 label，已分行修正；早期3份報表的 midpoint offset差直接減 canonical標量，跨垂直時軸符號可能翻轉，已改投影到current axis並新增反例。舊報表保留但不作最後pair真值；色支持分類完全未改。

完整命令、測試XML、失敗、final source/binary SHA、來源保護、容量見 [final-summary.json](../measurements/game-assist/2026-09-30-m0-manual-continue/pixel-reliability-x6/final-summary.json)。16MiB開發／24MiB batch、8GiB campaign+prior、1GiB build上限。總控自驗不是新獨立验收；本文件結論不得冒稱正式候選已凍結或 Miss 已改善。
