# 選項 A：既有 R3 decision skips 離線 protocol

2026-10-03 Asia/Taipei，主要分類產生前凍結。僅讀五個既有 B0 AA run；不新增 runtime/cost/stress/OS/gameplay，不修改策略、門檻、原 meter 或封存。HEAD f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c，main/唯一 worktree，observer50/planner27/diagnostics11；C36h tint1 baseline、main50 donor/control、pending-only 候選及 suppression OFF 保持。

## 輸入、界限與 join

先記錄 workspace、實測容量、逐檔 SHA/檔長。R3 原 artifact-ledger 與 controller-review ledger 分別驗證，不將後加的獨立验收資料說成原 archive 修改。新 input-manifest 逐列標來源 ledger；原 frames/receipts/releases/event-timings/archive segments 只引用。實際 main/contract、R1 frozen core.cpp、action_wake.hpp、game_session.cpp、SessionArchive、編譯依賴及 binary 綁定一併核對。

每 run 固定 2560 attempts；frame attempt 必須連續 1–2560，owned⇒consumed⇒published；重複/倒序/缺列、truncated LF、超 count/row/bytes、非法時間、錯 SHA 都拒絕。receipt 以 source_frame join（允許舊 owned frame 的後續觸控）；release 以 call_index 及 offline_attempt；timing 以 event_index 串接完整 archive 順序，attempt 僅為最後 accepted anchor。archive event/receipt/release 全內容與 counts、manifest/segment SHA 必須一致。unknown 的 0 raw 欄位輸出 null，不作零成本。

容量：measurements/runtime-decision-skip-a ≤64MiB，其中自驗全部成果/失敗/fixtures≤48MiB，總控預留16MiB；out/decision-skip-a≤1GiB，free≥5GiB。舊 campaign+prior≤8GiB，R3≤2GiB，新分析+兩舊帳實際合計≤10GiB。檔長不冒稱 NTFS allocation。獨立 C++20 reader/analyzer 只連 JSON 與 Windows SHA，不連 runtime core；每 row 依舊限 1023/511/255/256KiB，archive events≤65536/run、run≤80MiB、metadata≤2MiB；新輸出≤16MiB/次、至多兩份 deterministic report 與有界合成 tests。

## 已有與缺失的時刻

capture_complete 是 synthetic capture host 時刻，非 render；publish_cost 是 duration。published_ns 只在 consumed lease 中被保存，未 consumed 為未量。recognition_start/complete 包住 process，不包後面的 lease reset、mutex publication、notify。owner_start 在 selection 及兩個 enqueue 後；owner_end−owner_start 僅 game.accept，非整個 action iteration。receipt/release calls 有 host start/end；event enqueue/writer serialize/write 只有 duration，非絕對時刻。writer 並行，不能加進 action critical path。

記 consumed 按序為 c_i，完成 R_i、下一份 start B_(i+1)。來源保證 decision publication P_i∈[R_i,B_(i+1)]（最後一份上界 unknown）；不是假定 P_i=R_i。owned p 的 selection S_p∈[max(R_p,owner_end_previous),owner_start_p]；沒有 previous 則只用 R_p。mutex critical section 的精確時間、scheduler due/wake原因、lock wait、OS scheduling 全部 Unknown。

## 固定分類與可證偽問題

consumer skip=published&&!consumed；decision skip=consumed&&!owned。每份 skip 列 run/window/block/typed phase/targets、前後 consumed/owned attempt、原時戳、相鄰 capture/recognition 間隔、P 上下界、S 界限；不強迫互斥根因。主要 hypotheses：

1. **純前一 game.accept 可解釋全部 skips？** 若 skipped R_i>前 owned owner_end，則 P_i 必在該 accept 之後，是直接反例。R_i 在 accept 內只是完成時刻 overlap，非 publication 已在 accept。若整個可能 publication opportunity [R_i,下一 consumed 的 publication 上界] 落在該 accept，才稱 accept 覆蓋該機會的強界限；其它保留 bounded/Unknown。
2. **到達群聚是否為 skips 特有？** 固定 cluster 間隔≤cadence/2（owner4ms/RGB8ms），分別計相鄰所有 capture attempts、相鄰 consumed recognition complete；gap 按後一 attempt 分窗。skip 分析使用它與下一 consumed 的 recognition-complete gap，對 owned 非末份也同算法。另報連續 skipped consumed 長度、attempt連續長度與 owned sequence gaps。cluster 不是 OS 原因，也不以不同 RGB/owner 負載隔離因果。
3. **action accept 外區間有多少可確認工作？** consecutive owned p→q 的 gap=[owner_end_p,owner_start_q]，其內沒有其它 accept。source 順序保證 p anchor 排除最前 decision/lifecycle 兩項後的 enqueue，及 q 最前兩項 enqueue，均在 gap 內；其 duration 和是 action elapsed service 的下界，含 archive mutex 等待，排除 decision_json 建構，不當成 CPU/JSON 純成本。receipt/release 有绝對時刻，僅完整落 gap 內者可另加（與 enqueue 不重疊）；gap 減上述和為未被這些 meter 包住的區間，不能拆 OS/wake/poll/其它診斷。writer duration 僅另報 anchor 描述。

所有全窗、warmup1–256、measurement257–2560、四576 blocks保留；phase=(attempt−1)%16 僅 typed stimulus 分層，非正式遊戲規則。對 skip edge 的非skip matched control：同 run、previous owned 同固定 warmup/block、同 phase、同 targets，最近 previous attempt（tie 小 attempt），可重用；無匹配明列。這不控制 capture gap、後一 phase、未觀測排程，不能作因果 A/B。全部 edges/匹配也輸出，不能只選 worst。quantiles nearest rank、n/p50/p95/p99/max/p95−p5；無樣本 null。

## 自驗、呈現與終點

合成驗證 missing/duplicate/reverse/zero unknown/timing-anchor/join/錯SHA/truncation/容量與分類等號邊界。相同已觀測欄位容許不同 publication/selection/wait 排程的反例，兩者都必須保留 causal Unknown。必要新工具 ASan；不重跑舊大 suite 或 runtime。兩份主要 report bytes 一致；原封存交付後重 hash。高資訊例子按固定規則選：最早 skip、最長 skip edge、最大 enqueue/gap 比率 skip edge及其 matches；重複去除，exploratory 補例獨立標記。小型 SVG timeline 以原量測 accept、publication bounds、gap 分開表示，不畫假 actor 時刻。

Verified / Strong inference / Hypothesis / Unknown 分級。至多提1–2個下一 hypothesis、支持/反例/否證條件/最小缺失觀測，只提案。回答選項 B 是否有價值；不自動實作方法變更、R4、X10d-O 或 X12。交付「選項A交付完成，待總控獨立驗收；成本資格仍not-ready」，完成後停止。

## Reader discovery 補記（主要分類仍未產生）

首次真實輸入被 `aa-rgb-1 event_index135 raw attempt35` 的 owned join 拒絕；來源顯示 action 的 `seen/current_attempt` 是 DecisionSnapshot.sequence，RGB GameObserver 另有單調 decision sequence，並非 source frame。owner_stimulus 將兩者设相同。此為原命名語義差異，非封存修改。新分析逐 archive game_decision `(sequence,frame_sequence)` 解析 current anchor；event timing 的 raw attempt 及 release 的 offline_attempt 先當 decision sequence，再映射 source frame，保留兩欄。原 R3 summary 的 event/release 固定窗按 raw anchor 只引用，不以新映射重評歷史；新報告 event/release 分窗明列 resolved source attempt。全窗數據和本題193 owner skips不受此差異影響。兩種序號、漏decision／錯anchor／非遞增mapping须拒絕。這是 source-directed reader 修正，未改 cluster、match、bounds 或主要 hypothesis 規則。
