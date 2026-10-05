# X10：main50 fallback rail-claim 單機制消融（2026-10-03）

**局部因果作用成立，原 donor 不採用。** 在 C36h 新隔離 export 中，只加入 main50 fallback 重建末端的「width差<20、line切向中心差≤20且已有current claim就skip」；沒有移植 recovery排序、tracking、relation、owner或lease。正式src/include不變。helper在`apps/frame_review/x10_claim.hpp`，export在`out/x10/c36h-claim`，證據在`measurements/game-assist/2026-09-30-m0-manual-continue/hold-claim-x10`。

## Verified

- 1次完整固定pixels replay：7722張PNG SHA、7715次perception、32preroll、136選窗frames、結尾contacts0、success=true。沿X1 owner cadence／frame-first／zero recognition/RPC；不是新實機。
- **H2478：** C36h539保留原色核心中心約(277.427,508.364)、root_past；不再重建到y572.648產生新Down。原521 contact持續。到2479仍是539 root_past，沒有原539額外Down／取消配對。這證明此單機制足以改變該局部路徑；不證明physical identity gold或少一Miss。
- **A/B/E/D選窗：** 所有target欄位去除local note ID／revision並排序後相同，動作去除local intent/contact ID後相同；各22／4／2／23個receipt/release，D仍保有垂直線root及6215 Down。此處包括窗口內全部objects，不只是主cohort。不能宣稱contact allocation IDs、全首策略或gameplay相同。
- **首次全prefix動作差異在2447**，早於H窗：Down scheduled 42747689599→42749957058ns（晚2.267459ms）、x423.5011→423.1987。H窗2460已有target差；H選窗動作27→25。全prefix兩版仍各2165個receipt/release（1721 receipts＋444 releases），所以不能用總數宣稱改善；首個全state差異Unknown，generic X1沒有逐幀全prefix digest。
- C++ comparison reader `x10_compare.cpp`核summary、bridge、policy、PNG join，保存[comparison.json](../measurements/game-assist/2026-09-30-m0-manual-continue/hold-claim-x10/comparison.json)。C5520未放入此次五窗，但原X9 control及原Flick合成測試保存；不假稱此次新C observation trace已驗。

## 反例與測試

原207項suite先206 pass／1個opt-in skip；另以frozen C36h clips補跑該27張RGB case pass，207個distinct tests皆已執行、無failure。新donor contract五項 **3 pass／2 fail**，故拒絕採用；沒有把語義反例改成pass的期待。

1. 同column同寬、old current body位於normal375..575，新body80..180，中间195px可見空間；donor仍suppress。
2. incoming note與其horizontal line相容，claimed body來自vertical方向；donor不看claimed tangent，仍suppress。

這兩項是pure typed-geometry contract反例，**尚未證明完整observer在相同RGB會進入該call path**；不冒稱兩個已觀測到的gameplay regression。第二例初測把incoming tangent設成不相容其line，已修正為incoming與line相容、claimed body不同方向；原失敗輸出保留，最後讀`donor-tests-context.xml`。原suite通過也不能替缺少的分離body／不同線call-path regression背書。

反例機制來自donor沒有current body normal extent／同方向rail pair／獨立front檢查，不是某個距離閾值差一點。ASan／trace-off新run未執行，本原donor已被拒絕，不具正式candidate資格。初CMake GTest imported target scope錯誤修復並留log；沒有改 frozen export/binary。

## 下一個 bounded hypothesis（X10b，獨立proposed）

要測的不是把20px換一個值，而是補**current evidence的類型**：只有當fallback擬前緣確實在同向current held body的normal範圍內，而且當前藍色側帶從擬前緣兩側連續穿過，才能拒絕把它重新估成新前緣。真獨立front、normal分離／正交body、缺一側／缺pixels、strong direct front均保持原路徑；不合併ID、不改已Down／unknown／completed／alias。

優先重用C36h既有`fill_crosses_hold_front`同型的有限像素證據及current `claimed_outlines`，先寫合成RGB正負例，再單獨C36h variant。下一次trace擴到2440–2495以覆蓋已知最早action差，仍需要明列exact全state首差未知。若此新pixel witness也失敗，停止該family，不開閾值搜尋。只有實際合成／real replay／owner守門／determinism及成本通過，才進X11／有限live。

本輪只有總控實作／自驗，非獨立agent驗收。C36h仍baseline／77 Miss未改善，main50仍donor，沒有emulator、模型訓練、正式策略修改、commit或push。
