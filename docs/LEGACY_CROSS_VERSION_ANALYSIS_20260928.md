# Legacy 兩版本實戰結果分析

> 2026-09-28 清理後註記：本文保留當時版本與證據界線，舊授權／待辦不作現行指令。後續方向見[跨曲學習研究](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)；原始資料與 binary 的現存／已刪範圍見[清理紀錄](CLEANUP_AUDIT_20260928.md)。歷史 JSON 的 hash／pass 不代表被刪的 raw 仍可重算。

分析日期：2026-09-28。這是離線分析，沒有修改策略、開啟遊戲、訓練模型或合併分支。

結論：最新 main 策略在多數歌曲的結算表現改善，Good 顯著減少，但總漏鍵未降低；少數歌曲的線關聯與動作類別出現退步。現有證據支持優先處理「判定線身分／音符關聯＋學習式當前觀測」，不足以宣稱純規則方法已達理論極限，也不能只換音符模型便假設追蹤問題會消失。

## 比較範圍與方法

舊版為 observer33／planner13，使用[HD9跨曲證據](HD9_LEGACY_HD_RESULTS_EVIDENCE_20260928.json)；新版為 observer36／planner18，source `cd0ec437f495ee73e92a1adaccd097a86eeb91ac`，沿用 main 遊戲策略並新增有界像素採样，見[第二輪證據](MAIN_LEGACY_HD_RESULTS_EVIDENCE_20260928.json)。比對19首同曲HD、各8655個結算判定；Credits舊版取最後完整重跑B14。新版另外兩首IN分開列出，不與HD相減。第一輪Glaciaxion歷史基準不在本次19首配對內。

本次重新核對新版21份摘要與21張結算圖SHA-256，42份均與索引相符。對 Credits、混乱-Confusion、FULL AUTO SHOOTER、Pixel Rebelz、-SURREALISM-、光、Cipher 共7首、兩版本14輪，按各輪summary的event_segments順序串接原始事件，使用既有C++ `pas.exe analyze game <events.jsonl>` 重算。分析器來自合併後 main Release，其分析邏輯未在本次修改。分析輸出在 `measurements/cross-version-analysis-20260928/`，每份 `.analysis.json` 帶原輸入SHA-256。2026-09-28 清理已在核對 14/14 串接 hash 後刪除串接副本；原始 round event_segments 全數保留，可按 summary 陣列順序作二進位串接重建；`diagnostics-summary.json` 摘錄各輪診斷及完整排程/RPC分布。

這是每版本大多一次的描述性比較。時間、環境、採樣與策略一起變動，沒有控制變因或逐音符真值，不能建立單一改動的因果效果。沒有把每首分位數合併成整體分位數。

## 整體與集中失敗

| 指標 | 舊HD9 | 最新main策略 |
| --- | ---: | ---: |
| 每曲分數算術平均 | 828394.7 | 840325.4 |
| Perfect總數 | 7698 | 7795 |
| Good總數 | 134 | 39 |
| Bad總數 | 8 | 3 |
| Miss總數 | 815 | 818 |
| Miss／總判定 | 9.4165% | 9.4512% |
| 非Perfect總數（G+B+M） | 957 | 860 |

19首有15首分數提高，中位分數差+19392；14首Miss減少、4首增加、1首不變。改善歌曲合計減少103個Miss，退步歌曲增加106個，淨增3個。Good少95、Bad少5、Perfect多97是結算分布變化，不能逐鍵聲稱某個Good被轉成Perfect。新版剩餘860個非Perfect中，818個（95.12%）是Miss，後續主要改善空間是漏鍵／接觸失效。

| 曲目 | 舊→新Miss | 分數變化 | 判讀 |
| --- | ---: | ---: | --- |
| 混乱-Confusion | 47→101 | -72605 | 最明顯退步 |
| FULL AUTO SHOOTER | 61→93 | -60835 | 明顯退步 |
| Pixel Rebelz | 74→92 | -33317 | 明顯退步 |
| Eradication Catastrophe | 20→22 | -10500 | 小幅退步，單次差異不能判定穩定退化 |
| Credits | 130→128 | +5718 | 仍漏36.06%的判定，舊初次為133 Miss |
| -SURREALISM- | 104→104 | +1900 | 漏鍵數未改變 |
| Cereris | 70→43 | +33060 | Miss改善最多 |
| Cipher | 40→27 | +56311 | Good亦由30降至1 |
| 光 | 5→2 | +32381 | 最接近完整命中，仍有1 Good、2 Miss |
| Dlyrotz | 11→6 | +58548 | 分數改善最多，仍有6 Miss |

新版Credits、-SURREALISM-、混乱、FULL AUTO SHOOTER、Pixel Rebelz合計518 Miss，占全部818的63.33%。可優先用這些曲目的困難場景查漏，同時保留光、Dlyrotz、Cipher等改善曲目作回歸對照；不要按歌名切策略或逐曲調參。

## 時序與追蹤線索

環境共同為Windows x64／Release、gRPC payload fast RGB888 1280×720 rotation1、256 KiB、既有五指lead35ms配置。下表是各曲action owner實際消費的playing decision畫面間隔，包含跳幀，不等於全部capture callback或觸控延遲。

| 曲目 | 舊→新樣本n | p50 ms | p95 ms | p99 ms | max ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 混乱 | 6078→7593 | 20.39→16.65 | 42.30→31.90 | 53.47→43.54 | 135.99→100.66 |
| FULL AUTO SHOOTER | 6202→7505 | 19.63→16.58 | 41.37→31.88 | 54.68→43.45 | 979.42→963.76 |
| Pixel Rebelz | 8529→8560 | 16.57→16.57 | 31.45→31.65 | 43.43→44.10 | 140.24→140.39 |

混乱及FULL AUTO SHOOTER在處理間隔分布改善下仍明顯退步；Pixel Rebelz間隔近似卻增加18 Miss。因此單以「新一輪比較卡」不足以解釋三首退步。FULL AUTO SHOOTER兩版都有近1秒最大間隔，原因仍未確認，不能把全部時序問題排除。

7首新版之已接受且當時預定在未來的Down，逐曲樣本n=141–353、host排程遲到p95=0.553–0.757ms；RPC各曲p95=0.919–1.008ms。每曲n/p50/p95/p99/max在上述diagnostics-summary保留。這只涵蓋成功形成排程／發出呼叫的子集，不代表撞線預測正確或遊戲內已命中，也看不到未排程音符的延遲。

從「有音符候選但無可用線」診斷看，三首退步存在一致線索：

| 曲目 | `line_unobservable`觀測次數 舊→新 | 占該曲playing目標觀測比例 舊→新 |
| --- | ---: | ---: |
| 混乱 | 77→2149 | 0.42%→9.28% |
| FULL AUTO SHOOTER | 264→1901 | 1.61%→9.39% |
| Pixel Rebelz | 480→2892 | 2.26%→13.95% |
| Credits | 2169→2765 | 11.96%→15.04% |
| -SURREALISM- | 3821→4318 | 13.01%→14.83% |
| 光 | 78→79 | 0.50%→0.49% |
| Cipher | 3→2 | 0.02%→0.01% |

這些是重複影格中的目標觀測次數，不是獨立音符或Miss數；跨版候選數也會變。`line_unobservable`可能是完全沒有線，也可能有線但線身分無效。`multiple_line_association_unvalidated`另有計數，並未混入上表。

動作層亦有值得追查的差異：Pixel Rebelz Hold Down為43→15，Tap維持161；混乱 Drag Down為220→183，Tap135→125；FULL AUTO SHOOTER Flick25→15、Tap137→120。Down數包含錯認、重建及連續Drag覆蓋語義，不是譜面音符數，不能直接與Miss一對一抵扣。三首新版pending prediction取消僅1／3／1次，不能直接把大量漏鍵說成新取消事件造成；首次排程門控是否拒絕仍需另看候選。

### 具體片段：已偵測線，身分仍連續重建

Pixel Rebelz新版round15、`pixel-clips/round-15/clip-6-uniform/` 對應source frame 238007／238008／238009，跨度32.023ms。三筆decision中的線center=(640,570.7798)、tangent=(0.965868,0.259033)、length=1325.2324、confidence=0.85完全相同；line_id卻為7603→7604→7605，三幀均association_valid=false、motion_samples=1、motion_valid=false。四個Hold候選18171、18172、18166、18167持續存在並移動，但都標為line_unobservable、line_id=0。

事件摘錄為 `measurements/cross-version-analysis-20260928/pixel-rebelz-line-identity-case.jsonl`，SHA-256 `61e83193a1fb55d7c63458353a2c6a06ee9b48e2a84e870d2eac78931d187d0e`。此例證明至少存在「線被偵測到，但身份關聯失效」的情形，不證明這四個Hold最後都Miss。

程式核對：`GameLineTracker::update` 中競爭配對會將association_valid設為false；未配對觀測會建立新ID並保留新track。應檢查歧義後新增track是否讓後續候選持續互相競爭，以及motion預測是否放大此情況。這是根因候選，尚未以隔離重播／修正對照證明，不在本輪更改程式。

## IN與資料能做什麼

Dlyrotz IN13為634461分、395/5/6/178，Miss率30.48%；光IN12為799603分、443/7/0/67，Miss率12.96%。兩者都無同譜面舊版基準，僅建立IN起點，不把HD→IN落差當版本退步。

558張RGB確實是186個三幀片段：最短跨度7.015ms、中位34.188ms、最長66.164ms，全部片段首尾跨度相加僅6.439秒（不是連續覆蓋整曲）。由index逐組計算，明細 `measurements/cross-version-analysis-20260928/clip-spans.json`。包括固定窗438張、事件窗120張；前20輪實際涵蓋18首HD加2首IN，第21輪ENERGY SYNERGY MATRIX沒有片段。140張的舊偵測器target count為0，但不能當成沒有音符的負樣本真值。

每輪兩個事件窗都在開局約1–57秒內用完；每組只保留觸發後三幀，沒有事前上下文。這些資料適合開始標註當前音符、線實例、關聯與硬負例，亦能顯示短暫身分異常；不足以驗證遮擋後重接、完整旋轉、長Hold或跨秒的身份一致性。第一輪沒有對應像素片段，所以目前是兩組結果／事件資料，加一組稀疏像素資料。

## 建議的後續順序（待開發）

1. 先把已找到的線ID反覆重建片段整理成有人工像素核對的回歸情境，確認線有效性與音符關聯在哪一步中斷；保留目前較好的命中時序與非重播規則。不要單純放寬freshness來掩蓋問題。
2. 使用現有全畫面樣本建立跨曲音符head/body/tail、判定線實例及note→line關聯的標註規格，並把模型提議的當前觀測與tracker的時間假設分開。模型推論／追蹤最終共同改善可用觀測；這輪不選型、不訓練。
3. 下一次採樣改為較少、較長且分散全曲的有界片段，例如約1–2秒的研究目標，包含失效前後而非只取三幀；具體容量與可承受負載需先量測。按每曲分配額度，保留最後歌曲覆蓋，避免開局耗盡所有事件窗。
4. 使用統一參數，按整首歌曲分開訓練／驗證／保留測試。這19首已用來選研究方向，屬開發比較集；要宣稱未知曲泛化，應保留未用於選型與調參的新曲作最終測試。研究優先關注上述五首困難場景，但驗收須包含所有曲目與容易曲回歸。

下一版關鍵指標是每曲Miss／非Perfect、最差曲退化幅度、線與音符ID切換及關聯正確率；後兩者須有人工真值才能計算。模型辨識率與總分單獨提升，都不能替代跨曲漏鍵及追蹤驗收。
