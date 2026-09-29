# Legacy 章節 HD 實戰紀錄（2026-09-28）

> 2026-09-28 清理後註記：本文保留當時版本與證據界線，舊授權／待辦不作現行指令。後續方向見[跨曲學習研究](MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)；原始資料與 binary 的現存／已刪範圍見[清理紀錄](CLEANUP_AUDIT_20260928.md)。歷史 JSON 的 hash／pass 不代表被刪的 raw 仍可重算。

使用者完成 Legacy 章節的手動選曲。當天兩次有實戰輪次的 `manual-session` 合計保存 **20 張正面確認結算圖、19 個不同曲名**；另有一輪擷取中斷、一次只有待命的啟動。此處的「19」是從可核對的結算圖去重，並非從遊戲內部或譜面讀出的章節清單。歷史 Glaciaxion HD6 是另一個較早的 run，列在文末，不混入當天20張。

兩次實戰均使用相同Release binary SHA-256 `483cb83887d6daad9f01569aaca8f06c40e4e91df98f393b16a9c713c2534b76`、observer33／planner13、五接觸點、lead35ms、uncertainty30ms。來源固定為 gRPC payload fast／RGB888 top-down／256KiB、1280×720 rotation1，時間域為host QPC。每輪得分與判定由保存的**同一擷取結算圖**離線人工讀取；執行時沒有OCR成績、讀譜、回放按鍵或用本表決定觸控。[結構化證據](HD9_LEGACY_HD_RESULTS_EVIDENCE_20260928.json)包含各輪圖檔／summary的路徑和SHA-256、判定比例、樣本數與延遲分布；本機更完整的原始索引是 `measurements/hd9-cross-song-20260928/legacy-consolidated.json`。所有輪次manifest、events分段、summary和結算圖的hash已逐項核對。

表中 A 是首個實戰 session `manual-session-54180603650300`，B 是結束時正常STOPPED的 `manual-session-99700620346600`。P/G/B/M依次是Perfect／Good／Bad／Miss；P率以四種判定總數計算，ACC是遊戲畫面顯示，兩者定義不同。Max為Max Combo。原圖在各session的 `round-<輪次>/result.png`。

| 輪次 | 曲目／難度 | 分數 | P/G/B/M | P率 | ACC | Max |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| A1 | Eradication Catastrophe／HD7 | 828500 | 180/0/0/20 | 90.00% | 90.00% | 37 |
| A2 | Credits／HD10 | 567070 | 216/6/0/133 | 60.85% | 61.94% | 34 |
| A3 | Dlyrotz／HD9 | 903024 | 446/1/0/11 | 97.38% | 97.52% | 116 |
| A4 | Engine x Start!! (melody mix)／HD8 | 892474 | 181/3/2/6 | 94.27% | 95.29% | 67 |
| A5 | 光／HD7 | 911381 | 309/1/0/5 | 98.10% | 98.30% | 84 |
| A6 | Winter ↑ cube ↓／HD8 | 848129 | 412/6/0/31 | 91.76% | 92.63% | 65 |
| B1 | 混乱-Confusion／HD10 | 816297 | 417/14/0/47 | 87.24% | 89.14% | 67 |
| B2 | Cipher／HD10 | 823573 | 403/30/0/40 | 85.20% | 89.32% | 93 |
| B3 | FULL AUTO SHOOTER／HD9 | 765681 | 318/10/0/61 | 81.75% | 83.42% | 58 |
| B4 | HumaN／HD8 | 883761 | 202/7/1/12 | 90.99% | 93.04% | 103 |
| B5 | [PRAW]／HD10 | 866130 | 541/5/0/29 | 94.09% | 94.65% | 82 |
| B6 | Cereris／HD10 | 822104 | 624/7/0/70 | 89.02% | 89.66% | 106 |
| B7 | Pixel Rebelz／HD9 | 784574 | 425/6/0/74 | 84.16% | 84.93% | 102 |
| B8 | Non-Melodic Ragez (MUG Edit)／HD11 | 877874 | 631/9/5/30 | 93.48% | 94.35% | 194 |
| B9 | Sultan Rage／HD7 | 763069 | 271/9/0/54 | 81.14% | 82.89% | 57 |
| B10 | Class Memories／HD10 | 872847 | 726/9/0/36 | 94.16% | 94.92% | 143 |
| B11 | Bonus Time／HD9 | 882864 | 390/4/0/18 | 94.66% | 95.29% | 104 |
| B12 | ENERGY SYNERGY MATRIX／HD等級未辨 | 857750 | 536/7/0/37 | 92.41% | 93.20% | 110 |
| B13 | -SURREALISM-／HD9 | 762496 | 466/1/0/104 | 81.61% | 81.73% | 154 |
| B14 | Credits／HD10 | 576972 | 220/5/0/130 | 61.97% | 62.89% | 39 |

Cipher的結算畫面標題有花體後綴，表中只抄有把握的`Cipher`；B12等級最後一位被結果面板遮住，保留未知。沒有以外部曲庫補值。20張結算都仍有Good、Bad或Miss，**沒有All Perfect**。

## 重跑與中斷

- **Credits**：使用者確認重跑一次，結算圖可獨立證實A2和B14是相同HD10、均為355個判定。分數567070→576972，增加9902；Perfect216→220、Good6→5、Miss133→130、Max Combo34→39。B14圖上也顯示前一次best 0567070。這是兩次完整遊玩的比較，不能把差異全部歸因於策略，因為畫面到達間隔和操作時段不同。
- **-SURREALISM-**：使用者回報曾卡住而重來；B13是後來可核對的完整結算（762496分）。卡住的嘗試沒有可單獨歸屬的正面結算圖，重來次數、當時得分及具體bug原因均未知。不把它硬套到其他輪的中斷或尾端延遲。
- **A7**：首session另有一輪進入PLAYING約94.7秒，因`screenshot stream failed: 14`使程序FAULT，archive記`runtime_stop_aborted`、沒有結算圖；曲名與成績未知，不能算完整遊玩，也不能指認為-SURREALISM-。
- **待命啟動** `manual-session-99031291652700`：無遊玩輪次，正常STOPPED。當時使用者遇到「Touch to start」無法點擊；Android輸入診斷顯示`FocusedWindows: <none>`及Phigros視窗`NOT_VISIBLE`。Android guest重新啟動後，Phigros取得`FocusedWindows`、觸控能進入下一頁，之後才開始B session。這是啟動畫面輸入故障，不計入歌曲重跑；不能據此斷定其根因是輔助程式。

## 時序與範圍

各輪完整的`playing_capture_interval_ms`、`recognition_ms`、`host_residency_ms`均保留n／p50／p95／p99／max。完成輪次的遊玩decision到達間隔p95為31.43–43.84ms，合計156109筆**各輪樣本**；沒有把分位數錯當可直接合併的整體p95。尾端異常包括A4最大16.68秒、A6最大1.16秒、B11最大13.11秒，保留原值但原因未確認。這些間隔只涵蓋action owner實際消費的latest decision，含跳過frame，不代表全部capture callback；source絕對時間年齡未知。各輪最後release回報failed／unknown IDs均空；這只證明呼叫回報，未以畫面驗證每次接觸效果。

歷史比較基準Glaciaxion HD6為868880分、369 Perfect／2 Good／0 Bad／22 Miss、393判定；其原run是`measurements/game-assist/cpp-observe-17905107612608769`，使用同一音符策略來源的較早binary與不同生命周期。它不是A或B session的一輪，今日20張結算與19個不同曲名計數不含它。[策略及離線驗證紀錄](HD9_MANUAL_SESSION_20260927.md)說明差異。原始本機量測仍保留、未merge或push。
