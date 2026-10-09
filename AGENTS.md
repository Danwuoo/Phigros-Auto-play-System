# 專案工作準則

開始工作先讀 [README.md](README.md)、[架構](docs/catalog/01-project/ARCHITECTURE.md)、[路線圖](docs/catalog/01-project/ROADMAP.md) 與 [主程式架構與跨曲學習研究](docs/catalog/01-project/MAIN_ARCHITECTURE_CROSS_SONG_LEARNING_RESEARCH_20260928.md)。

## 現行方向

2026-10-10 main29／`208ad7b` 三配置各25、完整圖三份及3063 prefix通過；RPC audit
程序收據因未驗image的PID9008 error5而STOP（native0、runner125、identity false）。
原source／raw／STOP保存，slow／production9／AA12／noise／ABBA36 NOT_RUN。
新guard首50ms sleep1ms後原25ms，立即resume取樣；原image／64 held／8 snapshots／
50ms shared drain／deadline gates不改。39＋9＋12 controls、native4及short RPC24
組件診斷通過，新gate main06；新整合須fresh全包，未到實測ready。其餘一輪IN分數
授權沿用，不因修補另加門檻、不啟動emulator、不以組件PASS當整體資格。以下main28為歷史。

2026-10-10 冷續作：先讀[一輪分數授權與最新修補](docs/research/live-readiness-20261007/SINGLE_PASS_SCOPE_20261009.md)。
main28 source `62a8000` 三配置各25、3063 prefix、fault4、production normal三項通過；
stress Tap producer0而auditor2（原Hold-only取消契約遇六次Tap），後五项與AA/noise/ABBA未跑。
原905 source／raw／STOP及graph collector／ASan reader watchdog負結果保留；新Tap取消
契約的67控制及原full raw組件核對通過，但不授整體資格。新整合須fresh三配置及其後原gates。
graph以同source sealed Release reader核對各配置完整收據圖、明列executor配置；不發admission。
本task仍只冷開發，完成後交新的6.1 Sol/xhigh local main task。該task獲授emulator/CU/
選曲Play，以當前遊戲畫面盤點Chapter Legacy所有可用IN，各一次、只保存分數表，未解鎖記狀態；
不錄屏或持久化其他遊玩證據。一輪整理後停止休息，不補HD/解鎖/AP/Miss0/高分或額外重試。

2026-10-09 使用者明確指定後續新 GPT-6.1 Sol／xhigh local main task：授權開啟
emulator／Computer Use，自行盤點 Chapter Legacy 現行曲目，所有可用 IN 各測一次。
完成一輪後整理分數並停止休息；IN 未解鎖／不可用就註記，不加 HD 解鎖或重試。
只保存曲名、IN 等級、分數與簡短狀態；不錄屏、不保存遊玩截圖、PNG、pixel clips、
逐 frame／touch／trace 或其他遊玩紀錄。CU 導航／讀分數可用瞬時畫面，不另存證據。
歷史及冷驗證 raw 保留，cold archive workload／原 gate 不變。詳見
[本輪授權與修補](docs/research/live-readiness-20261007/SINGLE_PASS_SCOPE_20261009.md)。

2026-10-08 使用者已續派：完成冷修補與實機啟動前準備後，建立新的 GPT-6.1 Sol／xhigh
local main task，並交接其開啟 emulator／Computer Use、自行選曲／Play 進行有限測試的授權。
先處理 AA producer deadline 與 production dense archive overrun；不把授權當資格或提高門檻。
本 task 先冷開發；實際 emulator／device／physical 驗證交新 task，只有取得相應收據才能觸控。
以下停止指示保留為前階段歷史，最新續派已解除停止。

2026-10-08 最新使用者指示：「完成當前階段後停止，盤點開發情況」。本輪已封存並
停止開發／測試，未啟動 emulator；先讀[階段盤點](docs/research/live-readiness-20261007/PRE_EMULATOR_STAGE_20261008.md)
及[收據 JSON](docs/research/live-readiness-20261007/PRE_EMULATOR_STAGE_20261008.json)。
實際編譯 source `23a2634`，三配置各 22 正式 TUs／60 組冷回歸、3063 prefix、四項故障控制通過。
Production normal Tap／Hold 通過，dense archive mailbox overrun 執行失敗；AA Tap3
producer／publication 各一次 deadline overrun，後九筆／noise／ABBA 及 production 後六項未跑。
成本 NOT_READY、live UNQUALIFIED、完整 validator 未實作；不把較晚文件 HEAD 當作 binary
來源。尚有可冷修補工作，未到只差 emulator 的階段；本次不建立自動續跑。後續續作依新指示。

2026-10-08 使用者最新授權：將 Desktop、Downloads 與可讀 C 槽內的外部專案副本、
證據及封包集中回 main，核對保存後刪除外部位置；後續直接在 main 開發，不再新建
worktree。retained prelive 上限提高為 48 GiB，minimum free 20 GiB 及既有 stage、
candidate、raw、metadata 界限保留。見[整合帳本](docs/catalog/06-engineering/MAIN_CONSOLIDATION_20261008.md)。
保留歷史 source／binary SHA、原始負結果、STOP 與搬移索引；搬移不授予新 main 資格。
既有 emulator／Computer Use 自行選曲及 Play 授權沿用，但須先通過現行 build／cost／
admission／profile／physical gates。模型訓練、遠端 push 與新長期 goal 未授權。
模擬器、SDK 與已安裝 AVD／遊戲資料是後續開發依賴，保留原位置與 profile 指紋。
以下較早 evidence-only／cloud-only 範圍為當時階段記錄，不能覆蓋最新使用者授權。

2026-10-06 雲端續作入口：[readiness 工作](docs/research/live-readiness-20261006/README.md)。
指定起點 e9e305e，ignored compact 與直接父提交 f07dc40 的來源映射已核；不要回套舊 v3 patch。
本輪只雲端實作／必要離線驗證／local commits，未 push、未操作裝置；Windows 成本、physical
與全曲 IN Goal 分開保持 NOT_READY。先讀最新 RESULTS／READINESS／WINDOWS_HANDOFF，再讀
下列歷史方向；保留原 oracle、所有負結果與 STOP。新 candidate 必須以完整 source/build/profile/
evidence 身分銜接，不能用原已簽收 binary 或 donor CLI 冒稱新候選 live-ready。

2026-10-04 最新入口為[Chapter Legacy IN zero miss總帳](docs/catalog/01-project/LEGACY_IN_ZERO_MISS_BASELINE_20261004.md)與[逐曲JSON](docs/catalog/01-project/LEGACY_IN_ZERO_MISS_EVIDENCE_20261004.json)。產品目標是所有曲目解鎖IN並以完整IN結算Miss=0驗收，HD為解鎖／回歸，P/G/B照報、不增AP前置。現行章節分母及各曲目前解鎖unknown；不能由HD分數代推。main HEAD f83c7ea、50/27/11/live0是comparison／donor，C36h tint1仍behavioural／experimental baseline。X1已獨立验收；X10d-P冷契約通過但X11/R1/R2/R3成本not-ready，suppression OFF、X10d-O未開始。順序工作包見總帳，當前包只整理證據與入口，不啟動emulator／觸控／manual-session／模型、runtime/cost/stress/full replay或長期goal。較早授權依當時範圍閱讀，後續live仍需既有gate及總控續派。

2026-10-01歷史整合commit `9fea67e248411f7c2309f220daf989f652b27d15`。C36h同號observer37/planner19與歷史main不同binary，主要Miss未改善。閱讀[狀態J5–J32](docs/catalog/01-project/PROJECT_STATUS_NEXT_STEPS_20261001.md)、[歷史整理帳本](docs/catalog/06-engineering/CLEANUP_AUDIT_20261001.md)及[整合交接](docs/catalog/06-engineering/MAIN_INTEGRATION_HANDOFF_20261001.md)；A–I及早期「最新」字樣為歷史。模型只離線。10/4舊recovery LibTorch／CPU runtime路徑已不存在，cache引用是歷史，不默認可build或重裝。

2026-09-28 使用者指定跨曲學習研究為後續主線。先做 M0 證據／標註小試與線 ID 異常研究，再依 M1–M6 逐階段推進。舊 Glaciaxion HD AP → IN、獨立大型 Fixture 前置階段與五擷取重選均已退出現行順序。研究提案不等於已實作或已驗收；選定方向不代表自動恢復舊 AP goal、無限實戰或模型訓練。

同日使用者補充[判定線形式](docs/catalog/01-project/判定線形式.md)：先以線局部座標、相對運動與明確策略研究多線、斜向、旋轉、線追 Note、突現及過線往返；不得因形式複雜就預設需要學習。學習只對已確認的觀測／關聯缺口比較，M3／M4可不做；詳見研究 §2.7。幾何過線不等於 note 已完成，返回不代表可重播已執行／未知 Down。

2026-09-29持續冷開發以[原計畫§8–9](docs/catalog/01-project/NON_LEARNING_COLD_DEVELOPMENT_PLAN_20260929.md)的C0–C6為準，沿用原GPT-6 Sol／xhigh task設goal。使用既有pixels、非學習式輔助標記、合成與fake-clock補組合／Hold／排程與效能；本輪不啟動emulator、真觸控或模型訓練。部分未達是里程碑，不是goal完成；proposed不能作人工gold，缺實戰語義不阻塞其他可冷完成項。

旋轉情境另須覆蓋「Hold按住期間線仍旋轉」與「Note接近時才與線對齊」。區分Note朝向、運動方向、線法向；不以遠處當前同法向作必要配對條件。Hold的接入、持續body接觸與tail結束分開驗證，保留同一contact並依當前支持修訂Move；不固定初次Down位置，也不盲目跟著線旋轉。

## 不變原則

1. 自有正式邏輯全 C++20、單程序多執行緒，包括正式資料、分析及訓練邏輯；第三方原版工具可保留原語言。CMake、JSON、XML 與必要維護 shell 可使用。
2. 遊戲決策只能來自即時 pixels 與有界近期追蹤。不得讀譜、遊戲內部狀態、記憶體、音訊節拍、歌曲身分／進度或歷史按鍵序列來決定觸控。
3. 擷取只公開最新 frame；慢消費者跳過舊圖。物理 buffer、mailbox、軌跡、推論與診斷皆有硬上限。診斷資料單向流出，不回饋動作策略。
4. 當前觀測、線／音符身分、note→line 關聯、撞線預測、動作規劃與注入有清楚邊界。預測或插補不是當前像素證據。
5. 主機時間統一 QPC monotonic；明列 capture_complete、pixels_ready、recognition_complete、預定觸控、injection_start／return。來源時間另列 domain；未知來源年齡不冒稱已量測。
6. 單一 touch owner；門控與每個計畫獨立到期。未知注入結果不重試 Down，失效撤銷並釋放，完成意圖不復活。
7. 效能報環境、n、p50／p95／p99／max、失敗與 jitter；不能只報平均、只報已排程成功子集或把各階段 p99 相加。

## 固定基礎與驗證

- 主用擷取固定 gRPC payload fast／RGB888 top-down／256 KiB；不重開選型。WGC／DXGI／scrcpy 僅 bench，MMAP diagnostic-only，無已驗正式備用。
- 模擬器、解析度、方向、縮放與五指 mapping 由 profile／preflight 指紋核對。Fixture 能力不等於 Phigros 動作語義驗收。
- main50比較／donor版本observer50／planner27／diagnostics11、live0，C36h tint1為behavioural／experimental baseline；observer38／planner21／diagnostics7是9/29冷起點，歷史main37/19七輪的manifest數字欄另為36/18，依binary/source SHA辨識。保留observer33／36／37／38及A36派生frozen配套，不以版本號代替SHA。manual-session由使用者選曲及按Play；本整理包不啟動、不新建長期goal。
- 時間預測／排程變更須有可重現的合成軌跡／fake-clock 回歸；實戰結果附版本、環境與原始證據。
- 模型資料按歌曲／譜面家族切分；標註的 unknown 與 proposed 不升格為人工真值。不按歌名調參，不將舊動作當專家策略。
- 清理後保留範圍與資料缺口見 [清理紀錄](docs/catalog/06-engineering/CLEANUP_AUDIT_20260928.md)。旧文件／legacy 程式可從 Git 歷史查閱；刪除的 ignored raw 不可假稱仍可重算。不要恢復舊流程只為滿足歷史文件。
- 修改契約時同步更新文件。保留目前兩輪跨曲原始證據、能力報告、獨立 baseline 與必要測試輸入；新增資料另有容量帳本。
