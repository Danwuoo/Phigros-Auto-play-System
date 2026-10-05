# X10c 預先 protocol：X10b 獨立核對與全前綴因果稽核

日期：2026-10-03（Asia/Taipei）。本文件在新實作／replay前保存；C36h tint1仍baseline、77 Miss未改善，main50僅donor/control。X10b尚未有正式candidate資格。

問題：2477內部前緣抑制是否只移除539額外contact，或改變後續物件的觀測／history／root？2883的684提前Down從哪個boundary開始？全首還有多少未解動作差異？

1. 獨立核對原X10b freeze逐檔source/export/binary SHA、四份XML、三份run summaries/events、baseline bridge及比較程式。核對結果存新batch，不修改原結果。既有XML只證明歷史執行，不冒稱本輪重跑。
2. 新offline reader/export最多八窗。本次七窗：H2440–2495、K2865–2895、A3493–3504、B4979–4990、C5516–5524、E5281–5293、D6158–6220。原raw PNG只引用並核SHA，不複製。全部物件納入；從原32 preroll至EOF，不用片段冷啟動。
3. 兩lineage：原frozen C36h-v3與frozen X10b。只添加單向diagnostics；predicate及所有tracking/owner/scheduler政策保持。保存每幀scene/candidate bank/owner/contacts/combined semantic digest，輸出有界fallback witness（would-suppress、history、current claims及raw candidates），selected-window identity/relation/root/owner trace與時間映射。source render age、早期state、原perception race、physical identity皆unknown。
4. 同pixels／owner consumed set／frame-first／zero fake recognition+RPC／success receipts，最多四次完整replay：兩版ON、兩版OFF。工程failure也計入；新輸出名稱，不覆寫。每版必須與原frozen全events及semantic digest相同，ON/OFF亦同。診斷false/true不能回饋策略。
5. 新C++全action audit以保留時間／座標／source frame／receipt狀態的normalized action作有界alignment。全分母、未對齊、尾部、release failure保留；排除local IDs不代表physical等價。先列全部分布，再查2883，不假設其後一致。若採LCS，固定最多10000 actions、25 million cells，超額拒絕，不靜默截斷。
6. 反證：新額外Down／提前Up、破壞支持／identity／owner guards，或無法解釋重要全首差異，均阻止採用。若明確反例否決當前family，停止，不調threshold。只有有可證偽的新機制，最多一個獨立、先有protocol與正負synthetic+real-pixel regression的候選；不得為交付硬採用。

容量：新batch `hold-cascade-x10c` ≤128MiB；campaign+prior總額≤8GiB；新 `out/x10c` export/build ≤768MiB。先記disk free及既有bytes，logs/failures/source/summaries全部計額。原 `out/x10b` frozen、禁止build。OFF可少diagnostics，但仍有全prefix digest及相同動作。

停止線：可重現negative或重要語義仍Unknown就交付研究結論及下一個獨立問題；不自行擴額度或追加無限replay。未cold接受不進X11；未X11成本／freeze不進X12。使用者已授權必要Computer/emulator且最多六輪，但此protocol new-live=0，因當前問題可由既有raw冷驗證。
