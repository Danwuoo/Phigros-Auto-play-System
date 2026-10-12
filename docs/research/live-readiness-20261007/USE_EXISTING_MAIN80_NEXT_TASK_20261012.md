# 使用既有 main80：最新決定與新 task 交接

使用者最新指示：**「放棄實機 ready 門檻，使用既有版本，並 commit & push，派送新的 6.1 sol xhigh 告訴我下一步要做甚麼」**。

現行選擇是 main80／`3ee69edc85acb6b9b968424374a376f351d0e2dd` 的既有原碼。tracked runtime、建置、reader 與 admission 工具已 exact-byte 回復；main89 實驗改動撤回，所有歷史 commits、source、binary、raw、STOP、負結果與可恢復 archives 保留。見[回復與比較](MAIN80_ROLLBACK_AND_MAIN89_COMPARISON_20261012.md)。

使用者不再要求先修到全部 cold readiness gates 通過，才討論／安排後續實機使用。不要繼續以 Normal／AA／noise／ABBA 的成本門檻延長這輪開發，或追修33.9µs。這是使用者接受既有版本的決定；歷史 cost NOT_READY、live UNQUALIFIED、未知根因與正式 comparator REJECT 如實保留，沒有改成 PASS，也没有更動原 R2 數值或歷史收據。

## 新 task 本次工作

建立同一專案、local main、**GPT‑6.1 Sol／xhigh** task。其第一輪先讀本文件與回復比較，盤點既有可用 binary／啟動入口／profile，直接向使用者說明最短下一步與必要操作；不要自行重新開始冷成本修補／全盤測試或模型訓練。這次派送要求先告訴使用者下一步，不是在此派送訊息中新增一輪自動遊玩工作。

既有後續測試授權保持：新 task 可開 emulator／Computer Use，自行盤點 Chapter Legacy 所有當前可用 IN、各測一次；未解鎖／不可用註記，不加 HD 解鎖、額外重試或高分／AP／Miss0前置。跑完一輪整理後停止休息。只保存曲名、IN等級、分數與簡短狀態；不錄屏、不保存遊玩截图／PNG／pixel clips、逐frame／touch／trace或其他遊玩紀錄。CU 導航／讀分數可以使用瞬時畫面。

本次新增新 task 已明確授權，解除較早「先不要建立新 thread」的限制；使用者接受既有版本，亦覆蓋歷史「必須 cost ready 才可安排實測」的工作排序。首輪以盤點與下一步說明為任務，後續實測沿上述一輪範圍。仍直接在 main，不建立新 worktree；保留 SDK、emulator、AVD／遊戲資料與既有 profile。

## 身分與資料入口

- 舊版歷史 binary source：`3ee69ed`；回復提交與其後文件 HEAD 都不是原 binary 的編譯來源。本輪回復未重建或重新量測。
- main80 歷史結果：Normal3／3、AA11／12、noise15／18；ABBA36 full raw PASS但仍有時序負結果。main89 為2／3、6／12、11／18，ABBA36亦完整，詳細對照保留兩輪差異與限制。
- source／測量入口：[main80 完整結果](MAIN80_AA_ABBA_COMPLETION_REVIEW_20261012.md)、[main89 完整結果](MAIN89_ALL_COLD_COMPLETION_20261012.md)、[一輪只記分數授權](SINGLE_PASS_SCOPE_20261009.md)。
- 部分舊 compiler／source／journal 原位置已 lossless 封存；需要使用舊 binary 或核原 path 時按對應 restore index 恢復 exact bytes。不要因檔案封存就誤判遺失，也不要將 main89 bindings 套入已回復的 source。
- 遠端 commit／push 本輪已授權；新 task 以交接時實際 HEAD／origin/main 核對為準。歷史資料保存於目前主專案內，ignored 大型 payload 不作 Git push；Git 中保留程式、歷史提交與 SHA／restore 索引。
