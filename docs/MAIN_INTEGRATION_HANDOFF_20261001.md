# Main 整合與 Astra 後續研究交接（2026-10-01）

使用者要求將所有 worktree 合併至 main，再建立 GPT-6 Astra／xhigh 新 task，閱讀理解、研究下一步開發方向並整理清洗專案。本文件是目前工作的入口；較早文件的操作授權、版本與待辦需按日期及原始證據理解，不自動恢復 AP goal、實戰或訓練。

## 合併範圍與版本

Git 登記的兩個 worktree 已先各自保存未提交變更：主 checkout 的 `codex/four-song-hot-tuning` 提交 `39366fd`（此前 HEAD `25d4645`）；隔離恢復 checkout 的 `codex/baseline36-recovery` 提交 `98169a5`（此前 HEAD `e23b3f9`）。main 先 fast-forward 至前者，再以 merge commit 整合後者。合併前 patch、heads 及驗證 log 保存在 ignored `measurements/integration-20261001/`。本次沒有向 remote push，也沒有移除 worktree 或清除 ignored 證據。

main 新整合候選為 **observer50／planner27／diagnostics11**，來源版本只有 `include/pas/strategy_version.hpp` 一處。它是兩支來源的整合候選，**不是恢復版 A36／C36h，也沒有新實戰驗收**。歷史 main observer37/planner19 與 A36 派生 C36h 的同號版本不是同一程式，必須依 variant、binary SHA、source hash 區分。合併不會使歷史資料變成整合版的驗收資料。

保留 main 的全域線 assignment、有界三幀近鄰出生、線局部相對運動、確認關聯／短缺線預測、排程與單一 owner 安全邊界、冷組合庫、完整 QPC 階段計時。整合 C36h 的當前正交／染色 ridge、Note 自身局部幾何、Flick 中央箭頭合併、同 Hold 的 body patch/front 接續、較嚴格的當前 Tap 核心資格，以及 full-recording／one-round、逐幀離線工具與 CPU 模型工具。

衝突中的線 ID 契約統一為：有歧義的已配對觀測可保留舊 ID 作診斷 trace，但 `association_valid=false`、清除當前 motion 支持，且不更新內部 measured lifetime／motion fit。持續歧義後舊 ID 仍應到期；單次重複不能污染後續真單線。保持 main 的可分離近鄰三幀出生規則。恢復支線兩項測試原先要求所有診斷 ID 必為零，改為可為既有 ID，同時保留拒動作、無 motion、舊 ID 到期與新 ID 不橋接的斷言。這不是用測試刪除掩蓋衝突。

GameObserver 保留 main 原有四個參數的語義，將 split-joined-lines 作第五個可選參數；恢復支線的消融測試呼叫已按新 API 明確映射。LibTorch 預設不建置、不連結 `pas`，仍是獨立 offline CPU 工具。

首輪整合回歸 349 項找出兩個交互問題，未刪原斷言：新增正交 ridge 可使內部白裝飾比真線更近，搶走當前成對 rails 的 Hold 關聯；以當前 direct rails 的局部朝向增加軟成本（非遠處硬角度排除）保留真線關聯。主線同線 80px 的位移容忍也曾繞過 patch/front 切換的同 ID／body 範圍資格；切換時沿用恢復版原有 48px 小位移，超過它必須通過 patch_transition，穩定 anchor／旋轉位移仍沿用主線契約。再測發現原小位移案例被過度收緊，已恢復原 48px 範圍；原測試及所有反例均保留。

## 已知開發成果與未驗收項

1. main 冷開發 C0–C6 已有合成 RGB／oracle、fake-clock、混合五指、旋轉 Hold 與有界性能框架；歷史 309／310 項等驗證屬當時版本。observer49/planner26 的短缺線預測已進過兩首手動測試，四首比較未完成，詳見原熱測／退步審查文件。這些結果不能證明當前整合版實戰改善。
2. 恢復支線從凍結 A36 起步，完成原 12 輪與後續指定單輪。C36g-rec1 全錄 Dlyrotz IN13：778185、P485/G16/B6/M77；C36h tint1：795950、P496/G11/B0/M77。使用者回報主要問題未改善。較高分、冷 root 恢復與較少 Bad 不足以宣稱 Miss 根因修復。
3. 已保存原全輪 7722 張原生無損 1280×720 PNG；使用者選出的 12 段含 3722 張 unique／3843 次引用及已填理由。逐幀工具核對 index／SHA、journal 與 observer／owner 事件。cancel、combo 消失、runtime ID 不等於逐 Note 遊戲 Miss 或物理 ID 真值。
4. 已辨認 body patch/front 語義切換、旋轉 Flick 中央箭頭拆分、交叉 component 漏垂直染色 ridge 等觀測失效；C36h 在冷重播恢復若干路徑，但主要問題實戰未通過。最新 C36h 單輪只有稀疏 27 張 RGB／9 triples，不能稱該輪全錄，不能逐一回推 77 Miss。
5. C++20 CPU 學習工具已完成有界合成小試與原生 ROI proposal。4377 參數、合成不同 renderer seed macro IoU .89267；真實人工 pixel gold=0、真實準確率 unknown。18 張 Dlyrotz 家族 development ROI 尚未人工逐物件覆核，不能算跨曲測試。Flick／特效存在明顯 domain gap。使用者最新限制：**模型只辅助離線分析與主程式優化，不進即時迴圈**。
6. 完整暖機 observer→planner/owner→FakeTouch 的真實全錄反事實重播仍是缺口；現有 observation replay、原 journal join 與合成 cold pipeline 不等於這項能力已完成。下一步需研究是否以它先定位 Hold 失支持邊界，再轉成有界非學習式修正。

## 必讀索引

先讀 AGENTS.md、README.md、ARCHITECTURE.md、ROADMAP.md、MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md，以及判定線形式、note形式。再按證據閱讀：

- `BASELINE_RECOVERY_HOT_DEVELOPMENT_PLAN_20260930.md`、`HOT_REGRESSION_REVIEW_20260930.md`、`FOUR_SONG_HOT_TUNING_20260929.md`：歷史基線選擇、退步與熱測範圍。
- `FULL_ROUND_RECORDING_20261001.md`、`MISS_FRAME_ANNOTATION_20260930.md`、`DLYROTZ_FRAME_ANALYSIS_C36H_20261001.md`：來源時鐘、選段理由、連續影格、join 資格及未解項。
- `OFFLINE_CPU_VISION_PILOT_20261001.md`：模型工具、容量、freeze、原生裁切與 proposed/human gate；不再用同一合成 eval 無界選模。
- `NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md`、`COLD_DEVELOPMENT_RESULT_20260929.md`、`COLD_DEVELOPMENT_MERGE_REVIEW_20260929.md`：已存在的 C0–C6 能力與限制。
- `CLEANUP_AUDIT_20260928.md`：以前已刪範圍。不存在的 ignored raw 不可假稱可重算。

## 原始資料、凍結工具及保留清單

主要 campaign：`measurements/game-assist/2026-09-30-m0-manual-continue/`。既有 8GiB 上限與 logical entry 計帳保持；本次整理前最後帳本為 7,631,712,403 bytes，後续新增須重算並预留，不將工作區搬移當作消失的容量。原 12 輪 closed，不增補改寫。

必保留原全錄 `full-recording36g-01/sessions/manual-session-22885039263800/`、選段／REASONS 原文及 snapshot、frame-analysis36g／recorded／36h 的來源映射與報告、`acceptance36h-01/` 的原 event／結果／frozen runtime。C36h tint1 pas SHA：`61ecfacd2cc48ed270506719a1089be646d915c64e9026706ac1654e0d4bbce9`。還要保留兩輪跨曲原始證據、能力報告、observer33／36／38 等独立基線 binary／library／headers／來源／設定與必要回歸輸入。以磁碟實際 manifest 核對，不能只憑名稱決定重複。

`learning-cpu-01/` 保留 packet-v1（superseded）／v2、proposals、來源 QA、freeze、experiment-audit、capacity、所有 synthetic bootstrap 記錄與 final checkpoint。模型 SHA：`151e47c730e11aa3c06a75a6801650c0832faab6a8d02a80495ce8256d17e8cc`；原 CPU tool SHA：`cf6c5dac20979224cf5b9cfd8c203cec47045ac0c38f3064b1811d6a92fb00bb`。proposed 不是人工 gold；Reasons 是人工文字觀察，也不是像素 mask gold。

恢復 worktree 絕對路徑：`C:/Users/wurre/.codex/worktrees/baseline36-recovery/Phigros-Auto-play-System`。其 ignored `out/dependencies/libtorch-cpu-2.7.0/libtorch`、`out/vision-cpu-v145/` 及 provenance/DLL references 仍被 frozen 工具使用。第三方 CPU 依賴及原建置另計 3GiB，之前總量 1,778,892,397 bytes。**merge tracked files 不會搬移 ignored 依賴，不能直接刪除此 worktree 或沿用舊路徑假稱全部已整合到磁碟同一處。** 如需搬移，先列 references、複製核 hash、測 freeze 工具、更新新 manifest，保留歷史引用，再考慮移除舊目錄。

## 新 task 的執行範圍

以 main 為閱讀及整理入口，先建立「已實作／冷驗／實戰結果／未驗／提案」清單，研究下一步的優先順序、可證偽假說、最小可交付、證據／標註需求、停止條件。使用完整既有影片和影格；模型僅辅助標記、定位觀測缺口，不替 owner 判定身分、續租或 Down。

整理清洗已获授權：先盤點 tracked、ignored、frozen、active outputs、dependency references 及 hardlinks 的 logical／physical 容量，建立保留與可清理清單。可直接修正導覽／過時當前狀態、重複說明、無使用的生成暫存與可重建工作檔；保存可核對的清理帳本及刪前來源。不得因名稱相同刪原證據、手寫理由、獨立基線、凍結 runtime／models 或原失敗試驗；不 reset/clean、遺失未提交修改或盲目遞迴刪除。清理 Windows 目錄前必核對 absolute target 在預期範圍內。

本輪先做研究、閱讀及整理。不要自動開 emulator、真觸控、恢復 AP／無限實戰、追加模型訓練、推送或建立大型新開發 goal。研究可以檢查或运行已有離線測試；新實作方向先提交具體證據與里程碑。沒有 CUDA；不要求雲端 GPU。自有正式逻辑依舊全 C++20，必要维护 shell 可用。

## 本次整合驗證

隔離建置 `out/main-integration-v145`（Windows x64 Release、MSVC v145），全套 349 項中 **348 通過、1 個 opt-in 跳過、0 失敗**，72.37 秒；跳過項需額外指定 recorded clip 對照根目錄。optional CPU 整合另建 `out/main-integration-cpu-v145`，工具與 **8／8** 測試通過，10.84 秒；只驗工具、序列化及資料 gate，不追加模型訓練實驗。既有 Debug／ASan 成績屬歷史版，本次不冒稱已跑。

本次預設 build 的 `pas.exe` dumpbin dependencies 沒有 Torch／c10，Release 目錄也沒有 Torch DLL。C36h binary、原 CPU tool 與 final checkpoint SHA 逐一核對符合 freeze。新 main／CPU 建置、舊 CPU 依賴及建置的 logical 容量另存 `capacity.json`，不刪原 raw。本次新 live rounds=0。

合併前 dirty patch、source SHA、首輪／修正後失敗 log、最後成功 `ctest-accepted.log/xml`、`cpu-ctest.log/xml`、build logs、保留 SHA、容量及最終 commit／ancestry 核對保存在 `measurements/integration-20261001/`。來源依本次 Git merge snapshot，歷史基線及原失敗試驗仍各自可追溯。
