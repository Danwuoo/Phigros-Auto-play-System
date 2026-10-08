# 序列化候選的成本量測前置核對

2026-10-07；current source `3f23faf8bb91a4a974c33463e7232d3507c1dcc1` 不修改。
原 `journal03/journal04` 維護 STOP 與 original session suite FAIL 均保留。

`sessbase01` 用 frozen `8e953c7` production core / 原 session tests，與 `3f23faf` 完整對照。
雙方都是 18 tests：16 PASS、1 舊本機 raw 缺失 SKIP、1 舊 main golden FAIL。
原生成器完整 108 幀 current targets/plans/receipts 完全相同；原 golden bytes 完全相同。
差異 286 筆均在 decision，沒有 plans/receipts 變化。證據在
`out/prelive-session-baseline-evaluation-sessbase01/evaluation.json`，所有 expected-native=1
均是事先聲明的保留歷史負控制，不把原 FAIL 收據改為 PASS。

追加 `research/windows_session_compatibility`：用未改的 `hd9_strategy_trace.hpp`、正式
current core 和原完整 golden，強制 108 幀、原/wrapped 完全相同；先套原測試已明列的
四欄 legacy projection，再要求 **恰好 286 個 add**，路徑只能是
`decision/targets/<id>/current_contact` 或 `tail_terminal_proof`，且型別符合。
只移除這兩個新診斷欄後，其他全部 decision 欄位、targets、plans、receipts 必須逐值相同。
任一其他差異即 fail；不更新 golden，不換場景、不刪 raw、不改正式策略。

新的有限 cold 資格：三配置 10 個完整功能控制、6 個新 encoding tests、原 session
16 PASS/1 SKIP/1 歷史 FAIL 的精確核對、與上述嚴格 schema compatibility。Debug/ASan
同樣使用完整新 source core。任一新增失敗或歷史負控制漂移仍停止。
Release 已完整重建 source3f core；Debug22 objects 已全建置，維護腳本的 library target
array 錯誤發生在 native launch 前，保留。新 recipe 接續未建的 libraries / tests，
不重編已通過的 core objects；ASan 使用全新 configure/core/build。

這個核對完成才可開始新 endpoint-free R2 **必要成本量測**；不是原歷史 main suite
已全部通過，也不授予 live 資格。所有 AA/noise/ABBA/cost/RSS 數值、scenes、cadence、
denominator 與停止條件不變；前一個 STOP 不恢復，產品／physical／完整 IN 仍需驗收。
原 storage 累積16GiB、候選4GiB、metadata64MiB、free20GiB及 reserve 均保留。
