# X10b：current interior-front witness（2026-10-03）

**2026-10-03接手者最新結論：封存工程證據獨立核對通過，standalone suppression family否決採用。** [X10c](HOLD_CASCADE_X10C_RESULT_20261003.md)以四次有界重播定位2881抑制→target absence→舊pending Hold missing grace→2883 Down／2884 Up；不進X11／live。下文保留父chat封存時的自驗與未解狀態。H2478局部正例成立，但不能把總action少2換算為全首非退步或Miss改善。

## 改動與來源

只有新隔離`out/x10b/c36h-interior/src/game.cpp` fallback重建末端加一個predicate；source起點是frozen C36h-v3 export。正式src/include未改，main50並未升格baseline。`x10b_claim.hpp`要求fallback candidate為history-derived、非direct front／非active held observation，與current held outline方向一致、body normal extent容納±8px採樣，且當前藍側帶在兩側穿過擬前緣。保留原width差20／along20資格；沒有調閾值搜尋。caller的claimed_outlines必須來自本幀像素；不讀模型／曲名，不合併ID、不重試Down、不改owner或lease。

最多128 claims、每candidate60個RGB probes；frame/finite geometry／current line檢查拒絕unsupported輸入。這不是所有物理重疊Hold的唯一識別證明，也不是新public arbitrary-candidate API。

## 實際驗證

- **Release及Debug-ASan各原207/207、新13/13**，fail/skip/disabled0；原suite含27張frozen RGB。ASan只跑測試，沒有ASan全recording replay。兩build suite共享case，不能相加為440個獨立場景。新13個為local RGB／typed geometry契約，並非13段完整gameplay。
- 新full replay **3次**：原C36h baseline、X10b trace-on、X10b trace-off。各7722 PNG核SHA、7715 perception、32preroll、156 traceframes、結尾contacts0。H擴為2440–2495，其他A/B/E/D；C未列新trace。baseline全semantic/events與原freeze bridge一致。ON/OFF全semantic SHA及events bytes相同。
- X10b的Release runtime-reader binary與source另凍結；不以相同observer37/planner19字樣冒稱等於C36h live binary。所有來源／commands／XML／失敗／capacity見新batch`hold-claim-x10b`。
- Host cost當時與ASan／建置重疊，不作性能或live latency比較；X11尚未驗收。

## 局部正例、controls與全局限制

**Verified：** H2477 first target差（所選156frames内、去local note ID/revision），539留下原current color位置／root_past，沒有被reconstruct成新的approaching front；2478額外Down與2479相應Up移除，原521 contact保持。2440–2459的20幀targets全部相同；原donor在2447的早期action差消失。A/B/E/D選窗all-target及normalized actions保持（22/4/2/23）。首次全prefix action差2478。

**Verified negative audit：** full events baseline2165→variant2163，但新C++ `x10b_audit` 的bounded deletion alignment在2883失敗：不能表示「只刪掉那兩個action、其餘全相同」。前兩個deleted-reference actions確是539的Down/Up；剩餘1453個variant actions不在該成功匹配prefix內，不能隱藏此分母。

| local role / ordinal | baseline | X10b |
|---|---|---|
| 684／2881 | pending prediction取消，root_past | 未見同事件 |
| 684／2883 | 此時未Down；682有Move | 684在50310736400 Down，(782.505,576)，早於同幀682 Move |
| 684／2884 | 682有Move | 684 Up＋`current_object_missing_or_region_lost`取消；682仍Move |
| 684／2885 | 684在50351301500 Down，(780.784,576.533) | 此窗口沒有對應新Down |

ID只作各run内join；本段尚未逐物件RGB證明相同physical body。這可能是重建、identity／history、root或owner action差，**Unknown**，不能直接稱early Down更好，也不能把新cancel當新Miss。2883缺新的per-object trace，下一task應優先補這個窗口。全prefix first state divergence仍Unknown；只有全digest及選窗細節，不能捏造因果首幀。

## 交接與停止線

X10b暫停在研究候選，不build正式live runtime、不啟動emulator。下一步獨立核對source/binary/tests及同policy，再為2881–2885補有界暖機trace／原RGB packet，找第一個boundary。已有batch的3次replay上限已用完；新問題必須新experiment manifest、預先容量與停止線，不能覆寫或加跑掩蓋舊失敗。若新增窗口證實regression，拒採本family，不調門檻救例；若可解釋且對照通過，再做X11成本／候選freeze，之後才在已授權最多6輪內有限live。

完整較遠計畫：[C36H_FORWARD_EXECUTION_PLAN_20261003.md](C36H_FORWARD_EXECUTION_PLAN_20261003.md)。指定接手：[C36H_0100_HANDOFF_20261003.md](C36H_0100_HANDOFF_20261003.md)。本輪是總控自驗，不冒稱另經獨立agent驗收。C36h仍主要baseline、77 Miss未改善；human gold增加0、game adoption／跨曲效果未知。
