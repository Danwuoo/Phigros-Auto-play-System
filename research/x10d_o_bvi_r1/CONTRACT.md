# 4I-R1 BVI-1 修訂契約

2026-10-04 Asia/Taipei。入口為 docs/HOLD_OWNERSHIP_X10D_O_4I_CONTROLLER_ACCEPTANCE_20261004.md。原 research/x10d_o_bvi/CONTRACT.md 的 renderer、RGB predicates、probe offsets、128 regions／16 lines／1MiB metadata／6291456 probes、matching 閾值及 deadline/grace 保持。只修同一 BVI-1 family；以下是執行前的可否證契約，尚非驗證結果。

1. 每次 relation 的有效因果窗口包含 current，最多 6 份且 capture span <=90ms（含等號）。先淘汰，再計算，不能以已淘汰樣本引入歧義或确认。第七份及 age90+1 都核實際 relation，對照 prefix 與移除 witness 的同 current 輸入。
2. 對每個 current ROI 的唯一有界幾何 correspondence，按時序遍歷有效窗口。可見 front endpoint 才參與獨立證據；RGB signature 或 descriptor signature 任一已出現就不新增 confirmation，記錄該觀測的兩個 signature。去重集合包含窗口内全部 eligible endpoints，非僅最後接受者。first/last 是被接受獨立觀測的 capture time，span=last-first；duplicate current 只更新 freshness，不延長 span。A/B/A=2，A0/B10/C20/C40=3、span20ms，新 Down 不合格。hash equality 仍是近似而非過去完整 RGB bytes 相等證明。
3. 分層 RGB measured、typed descriptor/link、fake lifecycle 保留，新增 e2e 層：實際 renderer RGB -> extractor descriptors -> causal link -> fake constraints；relation、independent、span、usable、Move、Down 全由同一 RGB 軌跡斷言。typed 不替 RGB 層取得通過。
4. expected 欄位先列適用層／handler 的完整覆蓋表；只有 permutation 是 metadata，值白名單 runtime-ids/anchor-ids/region-order/line-order/contact-id。未知、拼錯、適用層空或必要但沒實際核的欄位 aggregate fail closed。equal_to:null 明確核 null-reference 語義。輸出必記 Constraints.contact_id，逐幀等於 guard ID；renaming 比較按明確 1->4 映射，包括 Move／refresh／release 的責任 ID，不刪 ID 忽略錯誤。負控制核拼錯欄、未知 metadata、空適用層及錯 contact 傳遞。
5. 新 Down 仍需 >=3 獨立端部／>=30ms、當前唯一 line/contact 支持。已知執行 prefix>0 的 fake contact，當前 body 與雙側 rails/contact、唯一 line 且 relation 無 invalid／ambiguity 才可同 ID Move/refresh；不需重新取得新 Down 的 endpoint 數量。prefix/ID/attachment 是外部一致 fake receipt 條件，不能證明 physical owner。長靜止序列每幀更新 last_contact；支持消失或歧義不 Move、不 refresh，仍按原 60ms missing 到期；unknown/expired 釋放，completed 不復活。無增 grace、無永久 owner、無保留淘汰歷史。typed 增加獨立宣告 left/right rails 與 provenance，未供 provenance 不可偷偷默認支持。
6. 原 effect expected 不改；effect 是 RGB/e2e applicable，typed fixture 未供 effect measurement，typed/lifecycle 明列不適用，不能說 typed 已驗 effect。原 V04 必在 RGB/e2e 核 true。

原20頂層／69展開與22 supplemental保持在分母。新增案例與 schema 負控制分列，oracle先於 candidate 修補凍結；不以實作結果生成 expected。新 fixtures 的 synthetic geometry／fake receipts 不是 physical gold。任何同輸入核心觀測 claim 被可重現反例否決即停；普通編碼或 harness 問題保留失敗版本，可修，不改 oracle／閾值／family。
