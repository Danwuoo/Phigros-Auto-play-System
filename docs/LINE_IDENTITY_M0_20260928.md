# M0 線身分研究：歧義出生的可重現機制

2026-09-28，清理與文件更新後的首個離線研究。使用現有、未修改的 `GameLineTracker::update`，沒有模型訓練、Capture、Touch backend 或新實戰。這是 M0 的部分進展；30 clip 人工標註、完整根因及修正驗收尚未完成。

**已證明一個充分的合成失效機制：一幀多出的近鄰候選可使關聯歧義持續自我延續，之後即使只提供穩定單線，仍反覆出生新 ID。** 這支持先修關聯／出生生命週期的研究順序，不證明真實 Pixel Rebelz 的最初觸發就是重複偵測，也不表示學習式觀測不需要。

## 可重現方法與結果

原始來源是 [game_motion.cpp](../src/game_motion.cpp)，呼叫公開 tracker API，沒有寫入 private tracks 或改動 production 演算法。程式：[probe](../research/line_identity_probe.cpp)；[建置／重跑命令](../research/README.md)；[完整 JSON](../measurements/line-identity-research-20260928/synthetic.json)。

本機 Windows x64、MSVC 19.51.36256、VS2026 v145、Release `/O2`／C++20；只使用原有 nlohmann-json headers。每條合成線為 1280×720 context、中心 x=640／y=570+offset、水平 tangent、length1200／thickness2／confidence.85。所有時間是確定的整數 ns，**不是 host 延遲量測**。一般步長16ms；每個案例只有一次確定性序列，不把影格數當獨立實戰樣本。

| 序列 | 輸出觀測數 | 不同輸出 ID | association_valid=true／false |
|---|---:|---:|---:|
| 穩定單線，32 updates | 32 | 1 | 32／0 |
| 相距20px的雙線，32 updates | 64 | 2 | 64／0 |
| 相距2px的兩個獨立合成線，32 updates | 64 | 64 | 2／62 |
| 起初單線，第二幀兩候選（0、2px），之後30幀單線 | 33 | 33 | 1／32 |
| 上例再加入520／560ms空觀測，600ms重現單線 | 累計34 | 累計34 | 累計2／32；最後一筆恢復valid |
| 起初雙近鄰、16ms單線，之後100ms gap | 4 | 4 | 3／1；gap後恢復valid |
| 起初雙近鄰，之後120次1ms單線更新 | 122 | 122 | 2／120 |

480ms 指第二幀16ms至最後496ms的區間，遠超90ms track保存期限，但仍未自行恢復。空觀測控制刻意每次間隔<100ms，讓 track 自然90ms到期；另一控制單獨測100ms context reset。兩種都能恢復，表示「時間到期」本身存在，問題在持續輸入期間的新生狀態延续。

相距2px案例刻意保留兩個獨立對象語義，不能把「全部近線合併成一條」當作修復。1ms案例只是容量／生命週期壓力，不代表 emulator 真的交付1000Hz；輸出 ID數不等於內部同時存活 track數（內部上限仍16）。

## 程式中的因果鏈

1. 同方向近線通過 pair gate；成本是 across + 40×(1−orientation)。
2. 對某個 pair，只要同觀測或同 prior 有另一成本≤目前成本+3的 pair，就把當前觀測設為 association_invalid，跳過指派。
3. 迴圈後 assigned<0 的觀測仍進入新生分支，分配新 ID；容量未滿時連同 invalid 狀態加入 tracks。這沒有區分「沒有合理 predecessor」與「有 predecessor 但歧義」。
4. 下一幀仍有多個近鄰 prior；pair成本不排除 prior 的 association_invalid，因此歧義再次成立，再出生一個新 prior。
5. 原來的舊 prior 會到期，但新的 invalid prior 不斷补入，所以單靠90ms淘汰無法打斷此序列。容量滿時仍會發新輸出 ID，未存入的輸出明示 invalid。

另外，contested 判斷掃描完整 pairs，包含已經被其他指派占用的替代配對；這是後續全域可行指派消融要檢驗的問題，本輪沒有單獨隔離其效應。

## 真實 Pixel Rebelz 事件對照

從保留的第二輪 `manual-session-108176133899800/round-15/summary.json` 取得 event_segments 原順序，逐行讀取兩段原始 events，掃描到 **8,872 個 game_decision**。工具只摘錄發布後的線幾何／ID，不做 pixels replay。

- frame238007–238009 的已知案例仍為 ID7603→7604→7605、association_invalid。
- 延伸到此前31份發布 decision，共34份近案例紀錄（237976–238009），全部單線且 invalid；幾何近似，不能聲稱每張 pixels 一樣。
- 截至frame238007，連續「發布單條invalid線」的紀錄已有 **168份**，起始 frame237839、capture112245370437100ns，至案例 capture112248259654600ns，跨度 **2.8892175秒**。
- 這不是整個故障的起點：此前frame237838已發布invalid主線與另一條valid短線，因此只是「單線輸出」這個條件的連續區段。不能把2.889秒稱為完整故障時長或把168換算成168次Miss。
- 新增紀錄提供更長的發布狀態脈絡；沒有補回事前90ms的原始pixels、pre-assignment candidates、private track集合或逐note判定真值。不能由此唯一推定最初重複track如何產生。

歷史 source／probe hash 與 source segment 清單在 JSON；原始 event hashes 保留在 round summary。本輪也沒有更改原始跨版結算或JSON evidence。

## 對下一步架構的具體影響

優先把「当前可見幾何」「跨時身分可信度」「是否允許出生」「是否可供action」分成不同欄位。當前線可見而identity uncertain時，觀測仍可供診斷與後續關聯，不應每幀憑同一個歧義創造新confirmed lineage。

下一個有界比較分三組，每次只改一項，正式 runtime 仍凍結：

| 組別 | 要驗證的改動 | 必留反例／限制 |
|---|---|---|
| A | 現行 greedy＋現行出生 | 本報告的確定性基準 |
| B | 保留成本／greedy，只區分 unmatched 與 contested；contested不反覆出生confirmed track | 不把真實新線永遠拒絕；量化unknown時長、恢復與漏出生 |
| C | 有界全域可行assignment＋獨立birth／歧義狀態 | 真雙近線、交叉、遮擋、分裂／合併候選、順序交換、16線容量與90／100ms邊界 |

不能以IDSW下降單一數字選B/C：全部拒絕也能讓它下降。還要看有效觀測覆蓋、真實新線召回、錯線關聯、unknown時長、重複出生、恢復後是否錯接Hold，以及完整deadline成本。未知不開新Down，失效仍釋放；不延長source／target期限補救。

學習式觀測的比較仍保留：模型可改善可見物件漏辨與部位辨識，但若直接接到現行出生機制，近重複模型輸出可能再次觸發同類問題。因此先建立乾淨的 observation／identity／relation 介面與負例，再加入CNN／DETR觀測頭，比單獨替換detector更可解釋。這是本地實驗支持的工程推論，未比較新模型性能。
