# 2026-10-09 一輪 IN 測試授權及冷修補

使用者明確要求：冷準備完成後交接新的 GPT-6.1 Sol／xhigh local main task，授權
啟動已安裝 emulator／Windows Computer Use，自行選曲及 Play。以遊戲現行畫面盤點
Chapter Legacy 全部 IN；每個可用譜面一次。IN 未解鎖／不可用記狀態，不補 HD 解鎖、
不加 AP／Miss=0／高分重試或其他練習輪。一輪完成後整理分數並停止休息。

只保存曲名、IN 等級、分數及 locked／unavailable／failed／interrupted 等簡短狀態。
不錄屏、不保存遊玩截圖、result PNG、pixels／clips、逐 frame／touch journal、trace
或其他遊玩 archives。CU 可用瞬時畫面導航及讀分數，不保存為證據。歷史及 cold raw
保持原樣；原完整 archive cost workload 不得因本次遊玩需求而減少。

直接在 local main 開發；沒有 push、訓練、worktree、automation 或新長期 goal 授權。
retained 48 GiB／minimum free 20 GiB 及原 stage/candidate/raw/metadata caps 保留。
不把分數表當成全曲 IN Miss=0 驗收，不由歷史 HD 結果推算現行解鎖或曲目分母。

原 `382d84513dde0e928e6c65d3486964f8290b8bec`（main22）完成三配置各 20 組冷回歸、
3063 prefix、4 項故障控制。Production normal Tap／Hold／Dense 與 stress Tap integrity
通過；stress Dense 41.725 秒因 archive mailbox overrun 執行失敗。Debug／ASan production
後四項、AA12／noise／ABBA36 未執行。normal Tap 有背景 prototype 小型編譯重疊，
不聲稱 quiet qualification。所有原始負結果及重疊索引保留。

原正式來源 892 檔另存並逐檔 SHA 核對於
`out/prelive-pre-emulator-planning/source382-formal-snapshot/snapshot.json`。
原三配置 compiler roots 已逐 entry 壓縮讀回 SHA、原檔再核對後封存並刪原 roots，見
`out/retained/consolidation-20261008/cold-source382-compiler-archives/*.verified.json`；
舊 binary 收據須先還原同 bytes roots 才能重新驗證。原 raw/source 未刪。

診斷 object-map 原型以 13 情境完整 bytes 核對原 core 通過，組件量測 p50
5.119 → 2.637 ms，尚不代表整體成本資格。score-only literal Tap／Hold／Dense／invalid
原型通過且無 gameplay 目錄。只讀 build reader 核對三配置原 build/source/dependencies/
環境/native process 收據通過，Debug／ASan 舊 report 的 scope 字串誤寫 Release；
實際 mode 驗證正確，正式 reporter 改為明列 mode/source，不重寫舊結果。

新整合需 fresh 正式 22 TUs、每配置 25 組冷回歸、prefix/fault/production9 及
原 R2 AA12/noise/ABBA36 成功才到 emulator 邊界。reader 不發 live admission，
新 task 取得當前 profile/capture/RPC/physical 收據並完成相應 validator 才能觸控；
安全故障立即撤銷釋放，未知 Down 不重試。當前尚未宣稱 ready 或啟動裝置。
